#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace RS;

//==============================================================================
RedSunAudioProcessor::RedSunAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "RED_SUN_STATE", RS::createParameterLayout())
{
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pIn = raw ("inGain"); pOut = raw ("outGain"); pMasterDrive = raw ("masterDrive");
    pMasterMix = raw ("masterMix"); pOS = raw ("oversample");
    pX[0] = raw ("xover1"); pX[1] = raw ("xover2"); pX[2] = raw ("xover3");

    for (int b = 0; b < numBands; ++b)
    {
        auto& p = bandParams[b];
        p.drive = raw (bandId ("drive", b)); p.level = raw (bandId ("level", b)); p.mix  = raw (bandId ("mix", b));
        p.tone  = raw (bandId ("tone", b));  p.dyn   = raw (bandId ("dyn", b));   p.style = raw (bandId ("style", b));
        p.mute  = raw (bandId ("mute", b));  p.solo  = raw (bandId ("solo", b));
    }

    pScOn = raw ("scOn"); pScLo = raw ("scLo"); pScHi = raw ("scHi"); pScDepth = raw ("scDepth");
    pScThresh = raw ("scThresh"); pScAttack = raw ("scAttack"); pScRelease = raw ("scRelease");

    apvts.addParameterListener ("oversample", this);
}

RedSunAudioProcessor::~RedSunAudioProcessor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener ("oversample", this);
}

//==============================================================================
bool RedSunAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
        || l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto& sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

void RedSunAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (samplesPerBlock, 64);

    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };

    for (int b = 0; b < numBands; ++b)
    {
        bandBuf[b].setSize (2, maxBlock);
        bandDry[b].setSize (2, maxBlock);
        dynGain[b].assign ((size_t) maxBlock, 1.0f);
        bandState[b] = {};

        using OS = juce::dsp::Oversampling<float>;
        os2[b] = std::make_unique<OS> (2, 1, OS::filterHalfBandFIREquiripple, true, false);
        os4[b] = std::make_unique<OS> (2, 2, OS::filterHalfBandFIREquiripple, true, false);
        os2[b]->initProcessing ((size_t) maxBlock);
        os4[b]->initProcessing ((size_t) maxBlock);

        bandDelay[b].setMaximumDelayInSamples (4096);
        bandDelay[b].prepare (spec);
        bandDelay[b].reset();
    }
    globalDry.setSize (2, maxBlock);
    globalDelay.setMaximumDelayInSamples (4096);
    globalDelay.prepare (spec);
    globalDelay.reset();

    latencyForMode[0] = 0;
    latencyForMode[1] = (int) std::lround (os2[0]->getLatencyInSamples());
    latencyForMode[2] = (int) std::lround (os4[0]->getLatencyInSamples());

    osMode = juce::jlimit (0, 2, (int) std::lround (pOS->load()));
    latencyNow = latencyForMode[osMode];
    setLatencySamples (latencyNow);

    for (auto& f : xo) f.reset();
    apA.reset(); apB.reset(); apC.reset();
    for (auto& f : xo) f.freq = -1.0;
    apA.freq = apB.freq = apC.freq = -1.0;
    for (auto& f : scHp) f.reset();
    for (auto& f : scLp) f.reset();
    duckA.reset(); duckB.reset(); duckAP.reset();
    duckA.freq = duckB.freq = duckAP.freq = -1.0;
    scEnv = 0.0; scGain = 1.0f; prevScOn = false;
    prevIn = prevOut = 1.0f; prevMasterMix = 1.0f;
}

void RedSunAudioProcessor::parameterChanged (const juce::String& id, float)
{
    if (id == "oversample") triggerAsyncUpdate();
}

void RedSunAudioProcessor::handleAsyncUpdate() { updateLatency(); }

void RedSunAudioProcessor::updateLatency()
{
    setLatencySamples (latencyForMode[juce::jlimit (0, 2, (int) std::lround (pOS->load()))]);
}

void RedSunAudioProcessor::copyRing (const std::array<float, ringSize>& ring, float* dest, int n) const
{
    const int mask = ringSize - 1;
    const int start = (ringPos.load (std::memory_order_relaxed) - n) & mask;
    for (int i = 0; i < n; ++i) dest[i] = ring[(size_t) ((start + i) & mask)];
}

//==============================================================================
void RedSunAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, true, 0);
    if (mainBus.getNumChannels() < 2 || sr <= 0.0 || os2[0] == nullptr)
        return;

    // Sidechain input (optional bus)
    const float* sc[2] = { nullptr, nullptr };
    bool haveSc = false;
    if (getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled())
    {
        auto scBus = getBusBuffer (buffer, true, 1);
        if (scBus.getNumChannels() > 0)
        {
            sc[0] = scBus.getReadPointer (0);
            sc[1] = scBus.getNumChannels() > 1 ? scBus.getReadPointer (1) : sc[0];
            haveSc = true;
        }
    }
    scConnected.store (haveSc);

    // Oversampling mode changes: flush state and re-align delays
    const int mode = juce::jlimit (0, 2, (int) std::lround (pOS->load()));
    if (mode != osMode)
    {
        osMode = mode;
        latencyNow = latencyForMode[osMode];
        for (int b = 0; b < numBands; ++b) { os2[b]->reset(); os4[b]->reset(); bandDelay[b].reset(); }
        globalDelay.reset();
    }
    for (int b = 0; b < numBands; ++b) bandDelay[b].setDelay ((float) latencyNow);
    globalDelay.setDelay ((float) latencyNow);

    float* io[2] = { mainBus.getWritePointer (0), mainBus.getWritePointer (1) };
    const int n = buffer.getNumSamples();

    for (int start = 0; start < n; start += maxBlock)
        processChunk (io, sc, start, std::min (maxBlock, n - start));
}

//==============================================================================
void RedSunAudioProcessor::processChunk (float* const* io, const float* const* sc, int start, int len)
{
    const float invLen = 1.0f / (float) len;
    const int   mask = ringSize - 1;
    const int   ringBase = ringPos.load (std::memory_order_relaxed);

    // ------------------------------------------------------------ input gain
    const float inGain = dbToLin (pIn->load());
    float inPeak = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        float* d = io[c] + start;
        for (int i = 0; i < len; ++i)
        {
            d[i] *= prevIn + (inGain - prevIn) * (float) (i + 1) * invLen;
            inPeak = std::max (inPeak, std::abs (d[i]));
        }
    }
    prevIn = inGain;
    meterIn.store (std::max (meterIn.load(), inPeak));

    // global dry path (delay-compensated) + analyser feed
    for (int i = 0; i < len; ++i)
    {
        float l = io[0][start + i], r = io[1][start + i];
        inRing[(size_t) ((ringBase + i) & mask)] = 0.5f * (l + r);
        if (latencyNow > 0)
        {
            globalDelay.pushSample (0, l); globalDelay.pushSample (1, r);
            l = globalDelay.popSample (0);  r = globalDelay.popSample (1);
        }
        globalDry.setSample (0, i, l);
        globalDry.setSample (1, i, r);
    }

    // ------------------------------------------------------------ crossovers
    double x1 = pX[0]->load(), x2 = pX[1]->load(), x3 = pX[2]->load();
    orderCrossovers (x1, x2, x3, sr);
    xo[0].set (sr, x1); xo[1].set (sr, x2); xo[2].set (sr, x3);
    apA.set (sr, x2);   apB.set (sr, x3);   apC.set (sr, x3);
    bandFc[0] = std::sqrt (30.0 * x1);  bandFc[1] = std::sqrt (x1 * x2);
    bandFc[2] = std::sqrt (x2 * x3);    bandFc[3] = std::sqrt (x3 * 18000.0);

    float* bp[4][2];
    for (int b = 0; b < 4; ++b) for (int c = 0; c < 2; ++c) bp[b][c] = bandBuf[b].getWritePointer (c);

    for (int c = 0; c < 2; ++c)
    {
        const float* in = io[c] + start;
        for (int i = 0; i < len; ++i)
        {
            double l1, r1, l2, r2, l3, h3;
            xo[0].split (c, in[i], l1, r1);
            xo[1].split (c, r1, l2, r2);
            xo[2].split (c, r2, l3, h3);
            bp[0][c][i] = (float) apB.allpass (c, apA.allpass (c, l1));   // phase-matched so the sum is flat
            bp[1][c][i] = (float) apC.allpass (c, l2);
            bp[2][c][i] = (float) l3;
            bp[3][c][i] = (float) h3;
        }
    }

    // ------------------------------------------------------------ bands
    for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::clear (io[c] + start, len);

    bool anySolo = false;
    for (int b = 0; b < numBands; ++b) anySolo = anySolo || bandParams[b].solo->load() > 0.5f;

    const float masterDriveDb = pMasterDrive->load() * 0.18f;
    for (int b = 0; b < numBands; ++b)
        processBand (b, start, len, masterDriveDb, anySolo, io);

    // ------------------------------------------------------------ master mix
    const float mMix = pMasterMix->load() * 0.01f;
    for (int c = 0; c < 2; ++c)
    {
        float* o = io[c] + start;
        const float* d = globalDry.getReadPointer (c);
        for (int i = 0; i < len; ++i)
        {
            const float m = prevMasterMix + (mMix - prevMasterMix) * (float) (i + 1) * invLen;
            o[i] = o[i] * m + d[i] * (1.0f - m);
        }
    }
    prevMasterMix = mMix;

    // ------------------------------------------------------------ Sidechain Savior
    const bool scOn = pScOn->load() > 0.5f;
    if (scOn)
    {
        if (! prevScOn)
        {
            duckA.reset(); duckB.reset(); duckAP.reset();
            for (auto& f : scHp) f.reset();
            for (auto& f : scLp) f.reset();
            scEnv = 0.0; scGain = 1.0f;
        }

        double lo = juce::jlimit (20.0, 0.40 * sr, (double) pScLo->load());
        double hi = juce::jlimit (lo * 1.2, 0.47 * sr, (double) pScHi->load());
        duckA.set (sr, lo); duckB.set (sr, hi); duckAP.set (sr, hi);
        for (int k = 0; k < 2; ++k) { scHp[k].setHighpass (sr, lo); scLp[k].setLowpass (sr, hi); }

        const double depth  = pScDepth->load();
        const double thresh = pScThresh->load();
        const double aA = std::exp (-1.0 / (sr * 0.001 * std::max (0.05, (double) pScAttack->load())));
        const double aR = std::exp (-1.0 / (sr * 0.001 * std::max (1.0,  (double) pScRelease->load())));
        const float  gs = 1.0f - (float) std::exp (-1.0 / (sr * 0.002));

        for (int i = 0; i < len; ++i)
        {
            // band-limited detector on the sidechain signal
            double lvl = 0.0;
            for (int c = 0; c < 2; ++c)
            {
                const double s = sc[c] != nullptr ? (double) sc[c][start + i] : 0.0;
                const double d = scLp[1].process (c, scLp[0].process (c, scHp[1].process (c, scHp[0].process (c, s))));
                lvl = std::max (lvl, std::abs (d));
            }
            scEnv = lvl > scEnv ? aA * scEnv + (1.0 - aA) * lvl : aR * scEnv + (1.0 - aR) * lvl;

            const double over = 20.0 * std::log10 (scEnv + 1.0e-9) - thresh;
            double k = juce::jlimit (0.0, 1.0, over / 12.0);
            k = k * k * (3.0 - 2.0 * k);
            const float target = dbToLin ((float) (-depth * k));
            scGain += (target - scGain) * gs;

            // carve the selected range out of the main signal
            for (int c = 0; c < 2; ++c)
            {
                double l, rest, mid, high;
                duckA.split (c, io[c][start + i], l, rest);
                duckB.split (c, rest, mid, high);
                io[c][start + i] = (float) (duckAP.allpass (c, l) + mid * scGain + high);
            }
        }
        scReductionDb.store (-20.0f * std::log10 (std::max (scGain, 1.0e-4f)));
    }
    else
    {
        scGain = 1.0f;
        scReductionDb.store (0.0f);
    }
    prevScOn = scOn;

    // ------------------------------------------------------------ output
    const float outGain = dbToLin (pOut->load());
    float outPeak = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        float* o = io[c] + start;
        for (int i = 0; i < len; ++i)
        {
            o[i] *= prevOut + (outGain - prevOut) * (float) (i + 1) * invLen;
            outPeak = std::max (outPeak, std::abs (o[i]));
        }
    }
    prevOut = outGain;
    meterOut.store (std::max (meterOut.load(), outPeak));

    for (int i = 0; i < len; ++i)
        outRing[(size_t) ((ringBase + i) & mask)] = 0.5f * (io[0][start + i] + io[1][start + i]);
    ringPos.store ((ringBase + len) & mask, std::memory_order_release);
}

//==============================================================================
void RedSunAudioProcessor::processBand (int b, int start, int len, float masterDriveDb, bool anySolo, float* const* io)
{
    auto& st  = bandState[b];
    auto& buf = bandBuf[b];
    auto& bpm = bandParams[b];
    const float invLen = 1.0f / (float) len;

    const int   style     = juce::jlimit (0, numStyles - 1, (int) std::lround (bpm.style->load()));
    const float driveDb   = bpm.drive->load() * 0.36f + masterDriveDb;          // 0 .. 54 dB
    const float driveLin  = dbToLin (driveDb);
    const float levelLin  = dbToLin (bpm.level->load());
    const float mixTarget = bpm.mix->load() * 0.01f * std::min (1.0f, driveDb * 0.5f); // fully transparent at 0 drive
    const bool  muted     = bpm.mute->load() > 0.5f || (anySolo && bpm.solo->load() < 0.5f);
    const float muteG     = muted ? 0.0f : 1.0f;
    const float dyn       = bpm.dyn->load() * 0.01f;
    const float toneDb    = bpm.tone->load() * 0.12f;

    // -- dry path, delay-compensated for oversampling latency
    for (int c = 0; c < 2; ++c)
    {
        bandDry[b].copyFrom (c, 0, buf, c, 0, len);
        if (latencyNow > 0)
        {
            float* d = bandDry[b].getWritePointer (c);
            for (int i = 0; i < len; ++i)
            {
                bandDelay[b].pushSample (c, d[i]);
                d[i] = bandDelay[b].popSample (c);
            }
        }
    }

    // -- dynamics: envelope-controlled drive (negative = compress, positive = expand)
    float* dg = dynGain[b].data();
    {
        const float* l = buf.getReadPointer (0);
        const float* r = buf.getReadPointer (1);
        const double aA = std::exp (-1.0 / (sr * 0.004)), aR = std::exp (-1.0 / (sr * 0.060));
        for (int i = 0; i < len; ++i)
        {
            const double pk = std::max (std::abs ((double) l[i]), std::abs ((double) r[i]));
            st.env = pk > st.env ? aA * st.env + (1.0 - aA) * pk : aR * st.env + (1.0 - aR) * pk;
            if (std::abs (dyn) < 1.0e-4f) { dg[i] = 1.0f; continue; }
            const float envDb = 20.0f * std::log10 ((float) st.env + 1.0e-6f);
            dg[i] = dbToLin (juce::jlimit (-18.0f, 18.0f, dyn * 0.5f * (envDb + 18.0f)));
        }
    }

    // -- the saturator itself (runs at the oversampled rate)
    auto shapeBlock = [&] (float* const* ch, int count, int factor)
    {
        for (int j = 0; j < count; ++j)
        {
            const float t  = (float) (j + 1) / (float) count;
            const float dl = st.prevDriveLin + (driveLin - st.prevDriveLin) * t;
            const float g  = dg[std::min (len - 1, j / factor)];
            const float pre = dl * g;
            const float inv = 1.0f / (std::sqrt (dl) * g);
            for (int c = 0; c < 2; ++c)
                ch[c][j] = shape (style, ch[c][j] * pre) * inv;
        }
    };

    if (osMode == 0)
    {
        float* ch[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };
        shapeBlock (ch, len, 1);
    }
    else
    {
        juce::dsp::AudioBlock<float> block (buf);
        auto sub = block.getSubBlock (0, (size_t) len);
        auto& os = osMode == 1 ? *os2[b] : *os4[b];
        auto up = os.processSamplesUp (sub);
        float* ch[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        shapeBlock (ch, (int) up.getNumSamples(), 1 << osMode);
        os.processSamplesDown (sub);
    }

    // -- DC block, tone tilt, mix / level / mute, accumulate
    const float dcR = 1.0f - 6.2831853f * 5.0f / (float) sr;
    const float a   = std::exp (-6.2831853f * (float) bandFc[b] / (float) sr);
    const float lgT = dbToLin (-toneDb * 0.5f),            hgT = dbToLin (toneDb * 0.5f);
    const float lgP = dbToLin (-st.prevToneDb * 0.5f),     hgP = dbToLin (st.prevToneDb * 0.5f);

    for (int c = 0; c < 2; ++c)
    {
        float* w = buf.getWritePointer (c);
        const float* d = bandDry[b].getReadPointer (c);
        float* out = io[c] + start;
        for (int i = 0; i < len; ++i)
        {
            const float t = (float) (i + 1) * invLen;
            const float x = w[i];
            const float y = x - st.dcX[c] + dcR * st.dcY[c];
            st.dcX[c] = x; st.dcY[c] = y;

            st.lp[c] += (1.0f - a) * (y - st.lp[c]);
            const float hi  = y - st.lp[c];
            const float wet = st.lp[c] * (lgP + (lgT - lgP) * t) + hi * (hgP + (hgT - hgP) * t);

            const float m  = st.prevMix   + (mixTarget - st.prevMix) * t;
            const float lv = st.prevLevel + (levelLin  - st.prevLevel) * t;
            const float mg = st.prevMute  + (muteG     - st.prevMute) * t;
            out[i] += (d[i] * (1.0f - m) + wet * m) * lv * mg;
        }
    }

    st.prevDriveLin = driveLin; st.prevLevel = levelLin; st.prevMix = mixTarget;
    st.prevMute = muteG;        st.prevToneDb = toneDb;
}

//==============================================================================
juce::AudioProcessorEditor* RedSunAudioProcessor::createEditor() { return new RedSunAudioProcessorEditor (*this); }

void RedSunAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void RedSunAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RedSunAudioProcessor(); }
