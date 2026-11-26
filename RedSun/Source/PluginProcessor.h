#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "Params.h"
#include "Dsp.h"
#include "Shapers.h"

class RedSunAudioProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    RedSunAudioProcessor();
    ~RedSunAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Red Sun"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Visualisation data (read by the editor)
    static constexpr int ringSize = 16384;
    std::array<float, ringSize> inRing {}, outRing {};
    std::atomic<int> ringPos { 0 };
    void copyRing (const std::array<float, ringSize>& ring, float* dest, int n) const;

    std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f }, scReductionDb { 0.0f };
    std::atomic<bool>  scConnected { false };
    double getSampleRateHz() const { return sr; }

private:
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void updateLatency();

    void processChunk (float* const* io, const float* const* sc, int start, int len);
    void processBand  (int band, int start, int len, float masterDriveDb, bool anySolo, float* const* io);

    struct BandParams { std::atomic<float> *drive, *level, *mix, *tone, *dyn, *style, *mute, *solo; };
    struct BandState
    {
        double env = 0.0;
        float dcX[2] {}, dcY[2] {}, lp[2] {};
        float prevDriveLin = 1.0f, prevLevel = 1.0f, prevMix = 0.0f, prevMute = 1.0f, prevToneDb = 0.0f;
    };

    std::atomic<float> *pIn, *pOut, *pMasterDrive, *pMasterMix, *pOS, *pX[3];
    std::atomic<float> *pScOn, *pScLo, *pScHi, *pScDepth, *pScThresh, *pScAttack, *pScRelease;
    BandParams bandParams[RS::numBands];
    BandState  bandState[RS::numBands];

    double sr = 44100.0;
    int maxBlock = 512;

    RS::LR4 xo[3], apA, apB, apC;
    double bandFc[RS::numBands] {};

    juce::AudioBuffer<float> bandBuf[RS::numBands], bandDry[RS::numBands], globalDry;
    std::vector<float> dynGain[RS::numBands];

    std::unique_ptr<juce::dsp::Oversampling<float>> os2[RS::numBands], os4[RS::numBands];
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> bandDelay[RS::numBands], globalDelay;
    int osMode = 0, latencyNow = 0;
    int latencyForMode[3] { 0, 0, 0 };

    float prevIn = 1.0f, prevOut = 1.0f, prevMasterMix = 1.0f;

    // Sidechain Savior
    RS::Biquad scHp[2], scLp[2];
    RS::LR4 duckA, duckB, duckAP;
    double scEnv = 0.0;
    float scGain = 1.0f;
    bool prevScOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedSunAudioProcessor)
};
