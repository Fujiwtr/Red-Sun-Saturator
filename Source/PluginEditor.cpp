#include "PluginEditor.h"
#include "SpaceArt.h"

using namespace juce;
using RS::bandColour;

//==============================================================================
namespace
{
void drawScrew (Graphics& g, Point<float> c, float seed)
{
    g.setGradientFill (ColourGradient (Colour (0xffc9cbd0), c.x - 2, c.y - 2, Colour (0xff4a4c52), c.x + 3, c.y + 3, true));
    g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f, 0.8f);
    const float a = seed * 2.4f;
    g.drawLine (c.x - std::cos (a) * 3.0f, c.y - std::sin (a) * 3.0f, c.x + std::cos (a) * 3.0f, c.y + std::sin (a) * 3.0f, 1.2f);
}

void drawMetalPanel (Graphics& g, Rectangle<int> r, float alpha)
{
    const auto f = r.toFloat();
    for (int i = 1; i <= 6; ++i)
    {
        g.setColour (Colours::black.withAlpha (0.11f));
        g.fillRoundedRectangle (f.expanded ((float) i).translated (0, 2), 6.0f + (float) i);
    }
    g.setGradientFill (ColourGradient (Colour (0xff6e7077).withAlpha (alpha), 0, f.getY(), Colour (0xff2a2b2f).withAlpha (alpha), 0, f.getBottom(), false));
    g.fillRoundedRectangle (f, 6.0f);
    {   // brushed-metal grain
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (f, 6.0f);
        g.reduceClipRegion (clip, {});
        Random rnd (r.getX() * 31 + r.getY());
        for (int y = r.getY(); y < r.getBottom(); ++y)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rnd.nextFloat() * 0.07f));
            g.fillRect (f.getX(), (float) y, f.getWidth(), 1.0f);
        }
    }
    g.setColour (Colours::white.withAlpha (0.22f));
    g.drawLine (f.getX() + 6, f.getY() + 0.8f, f.getRight() - 6, f.getY() + 0.8f, 1.0f);
    g.setColour (Colour (0xff0c0c0e));
    g.drawRoundedRectangle (f.reduced (0.5f), 6.0f, 1.4f);
    drawScrew (g, { f.getX() + 8, f.getY() + 8 }, 1.0f);           drawScrew (g, { f.getRight() - 8, f.getY() + 8 }, 2.0f);
    drawScrew (g, { f.getX() + 8, f.getBottom() - 8 }, 3.0f);      drawScrew (g, { f.getRight() - 8, f.getBottom() - 8 }, 4.0f);
}

void engraved (Graphics& g, const String& text, Rectangle<float> r, float size, Justification j, float kern = 0.14f, Colour col = Colour (0xffc4c6cc))
{
    g.setFont (Font (FontOptions (size, Font::bold).withKerningFactor (kern)));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText (text, r.translated (0, 1), j);
    g.setColour (col);
    g.drawText (text, r, j);
}

void knobLabel (Graphics& g, const String& t, float cx, float y)
{
    engraved (g, t, Rectangle<float> (cx - 45, y, 90, 14), 10.5f, Justification::centred, 0.12f, Colour (0xff9fa1a8));
}

String fmtHz (double f) { return f >= 1000.0 ? String (f / 1000.0, 2) + " kHz" : String ((int) std::round (f)) + " Hz"; }
} // namespace

//==============================================================================
RedSunAudioProcessorEditor::RedSunAudioProcessorEditor (RedSunAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      display (p, selectedBand, [this] { repaint(); })
{
    setSize (W, H);
    setLookAndFeel (&laf);

    for (int b = 0; b < 4; ++b) bandR[b] = { 20 + b * 287, 490, 277, 300 };

    staticLayer = renderStaticLayer();

    //---- master / header
    addAndMakeVisible (display);
    addKnob (inKnob, "inGain"); addKnob (driveKnob, "masterDrive"); addKnob (outKnob, "outGain"); addKnob (mixKnob, "masterMix");
    addAndMakeVisible (osBox);
    osBox.addItemList ({ "Off", "2x", "4x" }, 1);
    cAtt.push_back (std::make_unique<CA> (proc.apvts, "oversample", osBox));

    //---- bands
    for (int b = 0; b < 4; ++b)
    {
        auto& ui = bands[(size_t) b];
        addKnob (ui.drive, RS::bandId ("drive", b)); addKnob (ui.level, RS::bandId ("level", b));
        addKnob (ui.mix,   RS::bandId ("mix", b));   addKnob (ui.tone,  RS::bandId ("tone", b));
        addKnob (ui.dyn,   RS::bandId ("dyn", b));

        addAndMakeVisible (ui.style);
        ui.style.addItemList (RS::styleNames(), 1);
        cAtt.push_back (std::make_unique<CA> (proc.apvts, RS::bandId ("style", b), ui.style));

        for (auto* btn : { &ui.mute, &ui.solo })
        {
            btn->setClickingTogglesState (true);
            btn->getProperties().set ("kind", btn == &ui.solo ? "solo" : "mute");
            addAndMakeVisible (*btn);
        }
        bAtt.push_back (std::make_unique<BA> (proc.apvts, RS::bandId ("mute", b), ui.mute));
        bAtt.push_back (std::make_unique<BA> (proc.apvts, RS::bandId ("solo", b), ui.solo));
    }

    //---- Sidechain Savior
    addAndMakeVisible (saviorBtn);
    bAtt.push_back (std::make_unique<BA> (proc.apvts, "scOn", saviorBtn));
    addKnob (scLoK, "scLo");        addKnob (scHiK, "scHi");        addKnob (scDepthK, "scDepth");
    addKnob (scThreshK, "scThresh"); addKnob (scAttK, "scAttack");   addKnob (scRelK, "scRelease");

    startTimerHz (30);
}

RedSunAudioProcessorEditor::~RedSunAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RedSunAudioProcessorEditor::addKnob (Slider& s, const String& id)
{
    addAndMakeVisible (s);
    sAtt.push_back (std::make_unique<SA> (proc.apvts, id, s));
}

//==============================================================================
void RedSunAudioProcessorEditor::resized()
{
    display.setBounds (displayR);
    osBox.setBounds (1010, 29, 150, 26);

    // master
    inKnob.setBounds    (846, 130, 62, 62);
    driveKnob.setBounds (913, 106, 110, 114);
    outKnob.setBounds   (1028, 130, 62, 62);
    mixKnob.setBounds   (1094, 130, 62, 62);

    // sidechain savior
    saviorBtn.setBounds (846, 240, 308, 40);
    const int cx[3] = { 900, 1000, 1100 };
    RS::Knob* row1[3] = { &scLoK, &scHiK, &scDepthK };
    RS::Knob* row2[3] = { &scThreshK, &scAttK, &scRelK };
    for (int i = 0; i < 3; ++i)
    {
        row1[i]->setBounds (cx[i] - 29, 304, 58, 58);
        row2[i]->setBounds (cx[i] - 29, 384, 58, 58);
    }

    // bands
    for (int b = 0; b < 4; ++b)
    {
        auto& ui = bands[(size_t) b];
        const auto r = bandR[b];
        ui.mute.setBounds (r.getRight() - 62, r.getY() + 5, 26, 18);
        ui.solo.setBounds (r.getRight() - 32, r.getY() + 5, 26, 18);
        ui.style.setBounds (r.getX() + 10, r.getY() + 32, 257, 24);
        ui.drive.setBounds (r.getX() + 38, r.getY() + 58, 200, 142);
        RS::Knob* k[4] = { &ui.level, &ui.mix, &ui.tone, &ui.dyn };
        for (int i = 0; i < 4; ++i) k[i]->setBounds (r.getX() + 10 + 64 * i + 3, r.getY() + 216, 58, 58);
    }
}

//==============================================================================
Image RedSunAudioProcessorEditor::renderStaticLayer()
{
    Image img = RS::SpaceArt::createBackground (W, H);
    Graphics g (img);

    //---- header
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), 0, 0, Colours::black.withAlpha (0.0f), 0, 84, false));
    g.fillRect (headerR);
    g.setColour (Colour (0xff0c0c0e).withAlpha (0.9f));  g.drawHorizontalLine (83, 0, (float) W);
    g.setColour (Colour (0xff5a5c63).withAlpha (0.6f));  g.drawHorizontalLine (84, 0, (float) W);

    // logo: little red sun
    {
        const Point<float> c (46.0f, 42.0f);
        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.3f, 0.05f, 0.65f), c.x, c.y, Colours::transparentBlack, c.x + 34, c.y, true));
        g.fillEllipse (c.x - 34, c.y - 34, 68, 68);
        for (int k = 0; k < 20; ++k)
        {
            const float a = (float) k * MathConstants<float>::twoPi / 20.0f, l = (k & 1) ? 22.0f : 27.0f;
            g.setColour (Colour (0xffff7a30).withAlpha (0.9f));
            g.drawLine (c.x + std::sin (a) * 17, c.y - std::cos (a) * 17, c.x + std::sin (a) * l, c.y - std::cos (a) * l, (k & 1) ? 1.2f : 1.8f);
        }
        g.setGradientFill (ColourGradient (Colour (0xffffe08a), c.x - 5, c.y - 6, Colour (0xffb01c10), c.x + 13, c.y + 13, true));
        g.fillEllipse (c.x - 15, c.y - 15, 30, 30);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - 15, c.y - 15, 30, 30, 1.0f);
    }
    {
        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions (40.0f, Font::bold).withKerningFactor (0.2f)), "RED SUN", 86.0f, 52.0f);
        Path p; ga.createPath (p);
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.15f, 0.05f, 0.10f)); g.strokePath (p, PathStrokeType (9.0f));
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.25f, 0.08f, 0.18f)); g.strokePath (p, PathStrokeType (4.0f));
        g.setGradientFill (ColourGradient (Colour (0xfff4f5f7), 0, 22, Colour (0xff80828a), 0, 54, false));
        g.fillPath (p);
        engraved (g, "MULTIBAND SATURATION  -  SIDECHAIN SAVIOR", Rectangle<float> (88, 58, 330, 14), 10.0f, Justification::centredLeft, 0.16f, Colour (0xff9a9ca3));
    }
    engraved (g, "IN",  Rectangle<float> ((float) meterR.getX() - 40, 26, 34, 14), 10.0f, Justification::centredRight);
    engraved (g, "OUT", Rectangle<float> ((float) meterR.getX() - 40, 44, 34, 14), 10.0f, Justification::centredRight);
    engraved (g, "OVERSAMPLING", Rectangle<float> (860, 29, 140, 26), 10.0f, Justification::centredRight, 0.14f);

    //---- panels
    drawMetalPanel (g, masterR, 0.72f);
    drawMetalPanel (g, scR, 0.72f);
    for (int b = 0; b < 4; ++b) drawMetalPanel (g, bandR[b], 0.72f);

    // master
    engraved (g, "MASTER", Rectangle<float> ((float) masterR.getX() + 18, (float) masterR.getY() + 3, 160, 18), 11.5f, Justification::centredLeft, 0.2f);
    knobLabel (g, "INPUT", 877, 196);  knobLabel (g, "MASTER DRIVE", 968, 206);
    knobLabel (g, "OUTPUT", 1059, 196); knobLabel (g, "MIX", 1125, 196);

    // sidechain savior
    const char* scNames[6] = { "LOW", "HIGH", "DEPTH", "THRESHOLD", "ATTACK", "RELEASE" };
    const int cx[3] = { 900, 1000, 1100 };
    for (int i = 0; i < 6; ++i) knobLabel (g, scNames[i], (float) cx[i % 3], i < 3 ? 364.0f : 444.0f);
    g.setColour (Colour (0xff0c0c0e).withAlpha (0.8f));
    g.drawHorizontalLine (372 + 4, (float) scR.getX() + 14, (float) scR.getRight() - 14);

    // bands
    for (int b = 0; b < 4; ++b)
    {
        const auto r = bandR[b].toFloat();
        const Colour col = bandColour (b);
        const Rectangle<float> hdr = r.withHeight (28.0f).reduced (1.0f);
        g.setGradientFill (ColourGradient (col.withAlpha (0.55f), hdr.getX(), 0, col.darker (0.8f).withAlpha (0.25f), hdr.getRight(), 0, false));
        Path hp; hp.addRoundedRectangle (hdr.getX(), hdr.getY(), hdr.getWidth(), hdr.getHeight(), 5.0f, 5.0f, true, true, false, false);
        g.fillPath (hp);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawHorizontalLine ((int) hdr.getBottom(), r.getX() + 1, r.getRight() - 1);
        engraved (g, "BAND " + String (b + 1), Rectangle<float> (r.getX() + 16, r.getY() + 3, 70, 22), 12.0f, Justification::centredLeft, 0.18f, Colour (0xfff2f2f4));

        const float mid = r.getCentreX();
        knobLabel (g, "DRIVE", mid, r.getY() + 199);
        const char* names[4] = { "LEVEL", "MIX", "TONE", "DYNAMICS" };
        for (int i = 0; i < 4; ++i) knobLabel (g, names[i], r.getX() + 10 + 64.0f * (float) i + 32.0f, r.getY() + 275);
    }
    return img;
}

//==============================================================================
String RedSunAudioProcessorEditor::bandRangeText (int b) const
{
    double x1 = proc.apvts.getRawParameterValue ("xover1")->load(), x2 = proc.apvts.getRawParameterValue ("xover2")->load(),
           x3 = proc.apvts.getRawParameterValue ("xover3")->load();
    const double sr = proc.getSampleRateHz() > 0 ? proc.getSampleRateHz() : 44100.0;
    RS::orderCrossovers (x1, x2, x3, sr);
    const double e[5] = { 20.0, x1, x2, x3, 20000.0 };
    return fmtHz (e[b]) + " - " + fmtHz (e[b + 1]);
}

void RedSunAudioProcessorEditor::paint (Graphics& g)
{
    g.drawImageAt (staticLayer, 0, 0);

    // selected band highlight
    {
        const auto r = bandR[selectedBand].toFloat();
        const auto c = bandColour (selectedBand);
        g.setColour (c.withAlpha (0.20f)); g.drawRoundedRectangle (r.expanded (2.0f), 7.0f, 3.5f);
        g.setColour (c.withAlpha (0.90f)); g.drawRoundedRectangle (r, 6.0f, 1.5f);
    }

    // dynamic band range text
    for (int b = 0; b < 4; ++b)
    {
        const auto r = bandR[b];
        g.setColour (Colour (0xfff2f2f4).withAlpha (0.85f));
        g.setFont (Font (FontOptions (10.5f, Font::bold)));
        g.drawText (bandRangeText (b), Rectangle<int> (r.getX() + 84, r.getY() + 4, r.getWidth() - 84 - 68, 20), Justification::centredRight);
    }

    // level meters
    {
        auto bar = [&] (float db, int y)
        {
            const Rectangle<float> r ((float) meterR.getX(), (float) y, (float) meterR.getWidth(), 9.0f);
            g.setColour (Colour (0xff0b0b0d)); g.fillRoundedRectangle (r, 2.0f);
            const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            g.setGradientFill (ColourGradient (Colour (0xff7a1410), r.getX(), 0, Colour (0xffffc060), r.getRight(), 0, false));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * f).reduced (0.5f), 2.0f);
            g.setColour (Colours::black.withAlpha (0.55f));
            for (int i = 1; i < 12; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 12.0f), r.getY(), r.getBottom());
            g.setColour (Colour (0xff4b4d54)); g.drawRoundedRectangle (r, 2.0f, 1.0f);
        };
        bar (meterInDb, 27);
        bar (meterOutDb, 45);
    }

    // sidechain status line
    {
        const bool on = proc.apvts.getRawParameterValue ("scOn")->load() > 0.5f;
        const bool connected = proc.scConnected.load();
        String msg; Colour col;
        if (! on)            { msg = "Off - engage to duck a range from another track";  col = Colour (0xff80828a); }
        else if (! connected){ msg = "Enable this plugin's sidechain input in your DAW"; col = Colour (0xffffb04a); }
        else                 { msg = "Sidechain input connected";                         col = Colour (0xff9ccaff); }
        g.setColour (col);
        g.fillEllipse ((float) scStatusR.getX() + 2, (float) scStatusR.getCentreY() - 3, 6, 6);
        g.setFont (Font (FontOptions (10.5f, Font::bold)));
        g.drawText (msg, scStatusR.withTrimmedLeft (14), Justification::centredLeft);
    }
}

void RedSunAudioProcessorEditor::mouseDown (const MouseEvent& e)
{
    for (int b = 0; b < 4; ++b)
        if (bandR[b].contains (e.getPosition())) { selectedBand = b; repaint(); return; }
}

void RedSunAudioProcessorEditor::timerCallback()
{
    display.tick();

    auto toDb = [] (float v) { return 20.0f * std::log10 (std::max (v, 1.0e-5f)); };
    const float i = proc.meterIn.exchange (0.0f), o = proc.meterOut.exchange (0.0f);
    meterInDb  = std::max (toDb (i), meterInDb - 2.5f);
    meterOutDb = std::max (toDb (o), meterOutDb - 2.5f);
    repaint (meterR.expanded (2, 2));
    repaint (scStatusR);

    for (int b = 0; b < 4; ++b)
    {
        auto& d = bands[(size_t) b].drive;
        if (d.intensity() > 0.01f) d.repaint();
        const auto txt = bandRangeText (b);
        if (txt != lastRange[b]) { lastRange[b] = txt; repaint (bandR[b].withHeight (28)); }
    }
    if (driveKnob.intensity() > 0.01f) driveKnob.repaint();
    if (saviorBtn.getToggleState()) saviorBtn.repaint();

    const bool scOn = saviorBtn.getToggleState();
    if (scOn != lastScOn)
    {
        lastScOn = scOn;
        for (auto* k : { &scLoK, &scHiK, &scDepthK, &scThreshK, &scAttK, &scRelK }) k->setAlpha (scOn ? 1.0f : 0.5f);
    }
}
