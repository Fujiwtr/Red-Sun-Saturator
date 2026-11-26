#include "PluginEditor.h"
#include "SpaceArt.h"

using namespace juce;
using RS::bandColour;

//==============================================================================
namespace
{
// Frosted glass: a blurred copy of the backdrop, a violet tint, a soft top highlight and a hairline border.
void drawGlassPanel (Graphics& g, Rectangle<int> r, const Image& frost, int W, int H, float radius = 18.0f)
{
    const auto f = r.toFloat();

    for (int i = 10; i >= 1; --i)   // soft drop shadow
    {
        g.setColour (Colour::fromFloatRGBA (0.02f, 0.0f, 0.08f, 0.035f));
        g.fillRoundedRectangle (f.expanded ((float) i * 1.2f).translated (0.0f, (float) i * 0.8f), radius + (float) i * 1.2f);
    }

    Path clip;
    clip.addRoundedRectangle (f, radius);
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip, {});
        g.setOpacity (1.0f);
        g.drawImage (frost, 0, 0, W, H, 0, 0, frost.getWidth(), frost.getHeight());

        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (0.62f, 0.50f, 0.98f, 0.20f), 0.0f, f.getY(),
                                           Colour::fromFloatRGBA (0.10f, 0.04f, 0.26f, 0.55f), 0.0f, f.getBottom(), false));
        g.fillRect (f);

        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 1.0f, 1.0f, 0.10f), f.getX(), f.getY(),
                                           Colours::transparentWhite, f.getX() + f.getWidth() * 0.8f, f.getY() + f.getHeight() * 0.6f, true));
        g.fillRect (f);
    }

    ColourGradient hl (Colours::transparentWhite, f.getX() + radius, f.getY(), Colours::transparentWhite, f.getRight() - radius, f.getY(), false);
    hl.addColour (0.5, Colours::white.withAlpha (0.55f));
    g.setGradientFill (hl);
    g.fillRect (f.getX() + radius, f.getY() + 0.5f, f.getWidth() - radius * 2.0f, 1.2f);

    g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (0.85f, 0.75f, 1.0f, 0.50f), 0.0f, f.getY(),
                                       Colour::fromFloatRGBA (0.50f, 0.35f, 0.90f, 0.18f), 0.0f, f.getBottom(), false));
    g.drawRoundedRectangle (f.reduced (0.5f), radius, 1.0f);
}

void engraved (Graphics& g, const String& text, Rectangle<float> r, float size, Justification j, float kern = 0.14f, Colour col = Colour (0xffc4c6cc))
{
    g.setFont (Font (FontOptions (size, Font::bold).withKerningFactor (kern)));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.drawText (text, r.translated (0, 1), j);
    g.setColour (col);
    g.drawText (text, r, j);
}

String fmtHz (double f) { return f >= 1000.0 ? String (f / 1000.0, 2) + " kHz" : String ((int) std::round (f)) + " Hz"; }

//==============================================================================
// "RED SUN": every letter is shattered into wedge-shaped fragments that drift apart,
// then corrupted with glitch slices, an RGB split, pixel debris, embers and scanlines.
void drawCorruptTitle (Graphics& g, float x0, float baseline)
{
    const Font font (FontOptions (46.0f, Font::bold));
    const char* word = "RED SUN";
    Random rnd (20260611);

    struct Letter { Path path; Rectangle<float> b; bool hot; };
    std::vector<Letter> letters;

    float x = x0;
    for (int i = 0; word[i] != 0; ++i)
    {
        const String s = String::charToString ((juce_wchar) word[i]);
        const float adv = GlyphArrangement::getStringWidth (font, s);
        if (word[i] != ' ')
        {
            GlyphArrangement ga;
            ga.addLineOfText (font, s, x, baseline);
            Path p;
            ga.createPath (p);
            letters.push_back ({ p, p.getBounds(), i > 3 });
        }
        x += adv + 5.0f;
    }
    const float xEnd = x;
    const float top = baseline - 36.0f;

    auto topCol = [] (bool hot) { return hot ? Colour (0xfffff3cc) : Colour (0xffff8070); };
    auto botCol = [] (bool hot) { return hot ? Colour (0xffff5a1c) : Colour (0xff98101f); };
    auto fillLetter = [&] (const Letter& L)
    {
        g.setGradientFill (ColourGradient (topCol (L.hot), 0.0f, top, botCol (L.hot), 0.0f, baseline, false));
        g.fillPath (L.path);
    };

    // 1. outer glow
    for (auto& L : letters)
    {
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.18f, 0.08f, 0.07f)); g.strokePath (L.path, PathStrokeType (12.0f));
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.30f, 0.10f, 0.12f)); g.strokePath (L.path, PathStrokeType (5.0f));
    }

    // 2. blast light behind each letter
    for (auto& L : letters)
    {
        const auto c = L.b.getCentre();
        const float rad = jmax (L.b.getWidth(), L.b.getHeight()) * 1.5f;
        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.55f, 0.18f, 0.30f), c.x, c.y,
                                           Colour::fromFloatRGBA (0.9f, 0.10f, 0.05f, 0.0f), c.x + rad, c.y, true));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
    }

    // 3. chromatic aberration ghosts
    for (auto& L : letters)
    {
        g.setColour (Colour (0xff35e4ff).withAlpha (0.32f)); g.fillPath (L.path, AffineTransform::translation (-3.5f, 0.8f));
        g.setColour (Colour (0xffff3fd0).withAlpha (0.30f)); g.fillPath (L.path, AffineTransform::translation (3.5f, -0.8f));
    }

    // 4. shattered fragments
    for (auto& L : letters)
    {
        const auto c = L.b.getCentre();
        const float rad = jmax (L.b.getWidth(), L.b.getHeight()) * 1.2f;
        const int n = 6 + rnd.nextInt (3);
        const float step = MathConstants<float>::twoPi / (float) n;
        const float a0 = rnd.nextFloat() * MathConstants<float>::twoPi;

        std::vector<float> ang ((size_t) n + 1);
        for (int k = 0; k < n; ++k) ang[(size_t) k] = a0 + (float) k * step + (rnd.nextFloat() - 0.5f) * step * 0.5f;
        ang[(size_t) n] = ang[0] + MathConstants<float>::twoPi;

        for (int s = 0; s < n; ++s)
        {
            const float aa = ang[(size_t) s], ab = ang[(size_t) s + 1], mid = (aa + ab) * 0.5f;
            Path wedge;
            wedge.startNewSubPath (c);
            for (int k = 0; k <= 5; ++k)
            {
                const float t = aa + (ab - aa) * (float) k / 5.0f;
                wedge.lineTo (c.x + std::cos (t) * rad, c.y + std::sin (t) * rad);
            }
            wedge.closeSubPath();

            const float dist = 1.0f + rnd.nextFloat() * (L.hot ? 9.0f : 7.0f);
            const float rot = (rnd.nextFloat() - 0.5f) * 0.20f;

            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (rot, c.x, c.y).translated (std::cos (mid) * dist, std::sin (mid) * dist));
            g.reduceClipRegion (wedge, {});
            fillLetter (L);
            g.setColour (topCol (L.hot).withAlpha (0.55f));
            g.strokePath (L.path, PathStrokeType (1.0f));
            g.setColour (Colour (0xff12041f).withAlpha (0.60f));
            g.strokePath (wedge, PathStrokeType (2.4f));
        }
    }

    // 5. glitch slices: horizontal strips of the whole word thrown sideways
    for (int i = 0; i < 12; ++i)
    {
        const float y = top + rnd.nextFloat() * (baseline - top);
        const float h = 1.5f + rnd.nextFloat() * 5.5f;
        const float dx = (rnd.nextBool() ? 1.0f : -1.0f) * (3.0f + rnd.nextFloat() * 18.0f);

        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (Rectangle<int> ((int) (x0 - 30.0f), (int) y, (int) (xEnd - x0 + 70.0f), (int) std::ceil (h)));
        g.addTransform (AffineTransform::translation (dx, 0.0f));
        if (i % 3 == 0)
        {
            g.setColour (Colour (0xff35e4ff).withAlpha (0.65f));
            for (auto& L : letters) g.fillPath (L.path);
        }
        else
            for (auto& L : letters) fillLetter (L);
    }

    // 6. pixel debris
    for (int i = 0; i < 80; ++i)
    {
        const auto& L = letters[(size_t) rnd.nextInt ((int) letters.size())];
        const auto c = L.b.getCentre();
        const float ang = rnd.nextFloat() * MathConstants<float>::twoPi;
        const float d = jmax (L.b.getWidth(), L.b.getHeight()) * (0.45f + 0.85f * rnd.nextFloat());
        const float px = c.x + std::cos (ang) * d * 1.15f, py = c.y + std::sin (ang) * d * 0.85f;
        const float sz = 1.0f + rnd.nextFloat() * 3.2f;
        const float pick = rnd.nextFloat();
        const Colour col = pick < 0.16f ? Colour (0xff35e4ff) : pick < 0.30f ? Colour (0xffff3fd0) : (L.hot ? Colour (0xffffd88a) : Colour (0xffff5a46));
        g.setColour (col.withAlpha (0.25f + 0.65f * rnd.nextFloat()));
        g.fillRect (px, py, sz * (1.0f + rnd.nextFloat() * 1.8f), sz);
    }

    // 7. embers
    for (int i = 0; i < 14; ++i)
    {
        const float px = x0 + rnd.nextFloat() * (xEnd - x0), py = top - 8.0f + rnd.nextFloat() * (baseline - top + 18.0f);
        const float r = 2.0f + rnd.nextFloat() * 4.0f;
        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.65f, 0.25f, 0.55f), px, py,
                                           Colour::fromFloatRGBA (1.0f, 0.20f, 0.05f, 0.0f), px + r * 2.5f, py, true));
        g.fillEllipse (px - r * 2.5f, py - r * 2.5f, r * 5.0f, r * 5.0f);
    }

    // 8. data-corruption bars and displaced translucent blocks
    for (int i = 0; i < 9; ++i)
    {
        const float y = top + rnd.nextFloat() * (baseline - top);
        const float xx = x0 - 18.0f + rnd.nextFloat() * (xEnd - x0), w = 8.0f + rnd.nextFloat() * 42.0f;
        const Colour c = rnd.nextBool() ? Colour (0xff35e4ff) : Colour (0xffff3fd0);
        g.setColour (c.withAlpha (0.35f + 0.40f * rnd.nextFloat()));
        g.fillRect (xx, y, w, 1.0f + (rnd.nextFloat() < 0.3f ? 1.0f : 0.0f));
    }
    for (int i = 0; i < 3; ++i)
    {
        const float y = top + rnd.nextFloat() * (baseline - top - 8.0f);
        const float xx = x0 + rnd.nextFloat() * (xEnd - x0 - 30.0f);
        g.setColour ((i == 1 ? Colour (0xff35e4ff) : Colour (0xffff3fd0)).withAlpha (0.16f));
        g.fillRect (xx, y, 18.0f + rnd.nextFloat() * 30.0f, 3.0f + rnd.nextFloat() * 6.0f);
    }

    // 9. scanlines
    g.setColour (Colours::black.withAlpha (0.16f));
    for (float y = top - 6.0f; y < baseline + 10.0f; y += 3.0f)
        g.fillRect (x0 - 24.0f, y, xEnd - x0 + 50.0f, 1.0f);
}
} // namespace

//==============================================================================
RedSunAudioProcessorEditor::RedSunAudioProcessorEditor (RedSunAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      display (p, selectedBand, [this] { repaint(); })
{
    setLookAndFeel (&laf);

    for (int b = 0; b < 4; ++b) bandR[b] = { 20 + b * 287, 490, 277, 296 };

    staticLayer = renderStaticLayer();

    //---- master / header
    addAndMakeVisible (display);
    addKnob (inKnob,    "inGain",      "INPUT",        "Input gain, applied before any processing.");
    addKnob (driveKnob, "masterDrive", "MASTER DRIVE", "Pushes extra drive into every band at once.");
    addKnob (outKnob,   "outGain",     "OUTPUT",       "Final output level.");
    addKnob (mixKnob,   "masterMix",   "MIX",          "Blend between the untouched input and the whole effect.");
    addAndMakeVisible (osBox);
    osBox.addItemList ({ "Off", "2x", "4x" }, 1);
    osBox.setTooltip ("Oversampling reduces aliasing from the saturators. Higher settings use more CPU.");
    cAtt.push_back (std::make_unique<CA> (proc.apvts, "oversample", osBox));

    //---- bands: each band has its own drive, level, mix, tone, dynamics, style, mute and solo
    for (int b = 0; b < 4; ++b)
    {
        auto& ui = bands[(size_t) b];
        addKnob (ui.drive, RS::bandId ("drive", b), "DRIVE",    "How hard this band is driven into its distortion style.");
        addKnob (ui.level, RS::bandId ("level", b), "LEVEL",    "Output level of this band after saturation.");
        addKnob (ui.mix,   RS::bandId ("mix", b),   "MIX",      "Blend between this band's clean signal and its saturated signal.");
        addKnob (ui.tone,  RS::bandId ("tone", b),  "TONE",     "Tilts this band darker (left) or brighter (right).");
        addKnob (ui.dyn,   RS::bandId ("dyn", b),   "DYNAMICS", "Negative squashes the signal feeding the saturator, positive expands it.");

        addAndMakeVisible (ui.style);
        ui.style.addItemList (RS::styleNames(), 1);
        ui.style.setTooltip ("Distortion style for this band.");
        cAtt.push_back (std::make_unique<CA> (proc.apvts, RS::bandId ("style", b), ui.style));

        for (auto* btn : { &ui.mute, &ui.solo })
        {
            btn->setClickingTogglesState (true);
            btn->getProperties().set ("kind", btn == &ui.solo ? "solo" : "mute");
            addAndMakeVisible (*btn);
        }
        ui.mute.setTooltip ("Mute this band.");
        ui.solo.setTooltip ("Solo this band.");
        bAtt.push_back (std::make_unique<BA> (proc.apvts, RS::bandId ("mute", b), ui.mute));
        bAtt.push_back (std::make_unique<BA> (proc.apvts, RS::bandId ("solo", b), ui.solo));
    }

    //---- Sidechain Savior
    addAndMakeVisible (saviorBtn);
    saviorBtn.setTooltip ("Ducks the chosen frequency range on this track whenever the sidechain track is loud in that range.");
    bAtt.push_back (std::make_unique<BA> (proc.apvts, "scOn", saviorBtn));
    addKnob (scLoK,     "scLo",       "LOW",       "Lower edge of the range that gets ducked. Also draggable in the display.");
    addKnob (scHiK,     "scHi",       "HIGH",      "Upper edge of the range that gets ducked. Also draggable in the display.");
    addKnob (scDepthK,  "scDepth",    "DEPTH",     "Maximum amount the range is turned down.");
    addKnob (scThreshK, "scThresh",   "THRESHOLD", "Sidechain level above which ducking starts.");
    addKnob (scAttK,    "scAttack",   "ATTACK",    "How fast the range ducks once the sidechain gets loud.");
    addKnob (scRelK,    "scRelease",  "RELEASE",   "How fast the range comes back after the sidechain gets quiet.");

    startTimerHz (30);

    // The size is set LAST: setSize() triggers resized(), which needs the band rectangles
    // and every child component to exist already. (Setting it first stacked the band
    // controls in the top-left corner.)
    setSize (W, H);
}

RedSunAudioProcessorEditor::~RedSunAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RedSunAudioProcessorEditor::addKnob (Slider& s, const String& id, const String& name, const String& tip)
{
    addAndMakeVisible (s);
    s.setTooltip (tip);
    sAtt.push_back (std::make_unique<SA> (proc.apvts, id, s));

    KnobLabel k;
    k.slider = &s;
    k.name = name;
    knobLabels.push_back (k);
}

void RedSunAudioProcessorEditor::placeLabel (Slider& s, int labelTop)
{
    for (auto& k : knobLabels)
        if (k.slider == &s) { k.labelTop = labelTop; return; }
}

//==============================================================================
void RedSunAudioProcessorEditor::resized()
{
    display.setBounds (displayR);
    osBox.setBounds (1010, 30, 150, 26);

    // master
    inKnob.setBounds    (850, 124, 52, 52);
    driveKnob.setBounds (912, 100, 100, 100);
    outKnob.setBounds   (1026, 124, 52, 52);
    mixKnob.setBounds   (1092, 124, 52, 52);
    for (auto* k : { &inKnob, &driveKnob, &outKnob, &mixKnob }) placeLabel (*k, 182);

    // sidechain savior
    saviorBtn.setBounds (846, 246, 308, 38);
    const int cx[3] = { 900, 1000, 1100 };
    RS::Knob* row1[3] = { &scLoK, &scHiK, &scDepthK };
    RS::Knob* row2[3] = { &scThreshK, &scAttK, &scRelK };
    for (int i = 0; i < 3; ++i)
    {
        row1[i]->setBounds (cx[i] - 23, 292, 46, 46);  placeLabel (*row1[i], 340);
        row2[i]->setBounds (cx[i] - 23, 372, 46, 46);  placeLabel (*row2[i], 420);
    }

    // bands
    for (int b = 0; b < 4; ++b)
    {
        auto& ui = bands[(size_t) b];
        const auto r = bandR[b];
        ui.mute.setBounds (r.getRight() - 62, r.getY() + 5, 26, 18);
        ui.solo.setBounds (r.getRight() - 32, r.getY() + 5, 26, 18);
        ui.style.setBounds (r.getX() + 12, r.getY() + 34, r.getWidth() - 24, 24);

        ui.drive.setBounds (r.getX() + 48, r.getY() + 62, 180, 120);
        placeLabel (ui.drive, r.getY() + 172);

        RS::Knob* k[4] = { &ui.level, &ui.mix, &ui.tone, &ui.dyn };
        for (int i = 0; i < 4; ++i)
        {
            const int kx = r.getX() + 10 + 32 + 64 * i;
            k[i]->setBounds (kx - 26, r.getY() + 206, 52, 52);
            placeLabel (*k[i], r.getY() + 260);
        }
    }
}

//==============================================================================
Image RedSunAudioProcessorEditor::renderStaticLayer()
{
    Image img = RS::SpaceArt::createBackground (W, H);
    frost = RS::SpaceArt::makeFrosted (img);      // blurred copy shown behind the glass panels
    Graphics g (img);

    //---- header veil + hairline
    g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (0.03f, 0.0f, 0.10f, 0.70f), 0.0f, 0.0f, Colours::transparentBlack, 0.0f, 92.0f, false));
    g.fillRect (0, 0, W, 92);
    {
        ColourGradient hl (Colours::transparentWhite, 0.0f, 88.0f, Colours::transparentWhite, (float) W, 88.0f, false);
        hl.addColour (0.5, Colour (0xffb7a2ff).withAlpha (0.45f));
        g.setGradientFill (hl);
        g.fillRect (0.0f, 88.0f, (float) W, 1.0f);
    }

    //---- logo: little red sun
    {
        const Point<float> c (46.0f, 44.0f);
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

    //---- exploded, corrupted title + subtitle underneath it (no overlap)
    drawCorruptTitle (g, 98.0f, 56.0f);
    engraved (g, "MULTIBAND SATURATION  -  SIDECHAIN SAVIOR", Rectangle<float> (100.0f, 70.0f, 330.0f, 14.0f), 10.0f, Justification::centredLeft, 0.16f, Colour (0xffb3a6dc));

    engraved (g, "IN",  Rectangle<float> ((float) meterR.getX() - 40, 26, 34, 14), 10.0f, Justification::centredRight, 0.14f, Colour (0xffc9bdf0));
    engraved (g, "OUT", Rectangle<float> ((float) meterR.getX() - 40, 44, 34, 14), 10.0f, Justification::centredRight, 0.14f, Colour (0xffc9bdf0));
    engraved (g, "OVERSAMPLING", Rectangle<float> (860, 30, 140, 26), 10.0f, Justification::centredRight, 0.14f, Colour (0xffc9bdf0));

    //---- glass panels
    drawGlassPanel (g, masterR, frost, W, H);
    drawGlassPanel (g, scR, frost, W, H);
    for (int b = 0; b < 4; ++b) drawGlassPanel (g, bandR[b], frost, W, H);

    engraved (g, "MASTER", Rectangle<float> ((float) masterR.getX() + 20, (float) masterR.getY() + 8, 160, 16), 11.5f, Justification::centredLeft, 0.2f, Colour (0xffe6defa));

    for (int b = 0; b < 4; ++b)
    {
        const auto r = bandR[b].toFloat();
        const Colour col = bandColour (b);
        const Rectangle<float> hdr (r.getX() + 1.0f, r.getY() + 1.0f, r.getWidth() - 2.0f, 28.0f);
        g.setGradientFill (ColourGradient (col.withAlpha (0.50f), hdr.getX(), 0.0f, col.darker (0.8f).withAlpha (0.12f), hdr.getRight(), 0.0f, false));
        Path hp;
        hp.addRoundedRectangle (hdr.getX(), hdr.getY(), hdr.getWidth(), hdr.getHeight(), 17.0f, 17.0f, true, true, false, false);
        g.fillPath (hp);
        g.setColour (Colour::fromFloatRGBA (0.8f, 0.7f, 1.0f, 0.18f));
        g.fillRect (r.getX() + 12.0f, hdr.getBottom(), r.getWidth() - 24.0f, 1.0f);
        engraved (g, "BAND " + String (b + 1), Rectangle<float> (r.getX() + 18, r.getY() + 3, 70, 22), 12.0f, Justification::centredLeft, 0.18f, Colour (0xfff7f2ff));
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
        g.setColour (c.withAlpha (0.20f)); g.drawRoundedRectangle (r.expanded (2.0f), 18.0f, 3.5f);
        g.setColour (c.withAlpha (0.85f)); g.drawRoundedRectangle (r, 16.0f, 1.5f);
    }

    // dynamic band range text
    for (int b = 0; b < 4; ++b)
    {
        const auto r = bandR[b];
        g.setColour (Colour (0xfff2f2f4).withAlpha (0.85f));
        g.setFont (Font (FontOptions (10.5f, Font::bold)));
        g.drawText (bandRangeText (b), Rectangle<int> (r.getX() + 90, r.getY() + 4, r.getWidth() - 90 - 68, 20), Justification::centredRight);
    }

    // level meters
    {
        auto bar = [&] (float db, int y)
        {
            const Rectangle<float> r ((float) meterR.getX(), (float) y, (float) meterR.getWidth(), 9.0f);
            g.setColour (Colour (0xff0a0716)); g.fillRoundedRectangle (r, 4.0f);
            const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            g.setGradientFill (ColourGradient (Colour (0xff7a1410), r.getX(), 0, Colour (0xffffc060), r.getRight(), 0, false));
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * f).reduced (0.5f), 4.0f);
            g.setColour (Colours::black.withAlpha (0.45f));
            for (int i = 1; i < 12; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 12.0f), r.getY(), r.getBottom());
            g.setColour (Colour (0xff5b4f8a)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        };
        bar (meterInDb, 27);
        bar (meterOutDb, 45);
    }

    // sidechain status line
    {
        const bool on = proc.apvts.getRawParameterValue ("scOn")->load() > 0.5f;
        const bool connected = proc.scConnected.load();
        String msg; Colour col;
        if (! on)             { msg = "Off - engage to duck a range from another track";  col = Colour (0xff9a90c0); }
        else if (! connected) { msg = "Enable this plugin's sidechain input in your DAW"; col = Colour (0xffffb04a); }
        else                  { msg = "Sidechain input connected";                         col = Colour (0xff9ccaff); }
        g.setColour (col);
        g.fillEllipse ((float) scStatusR.getX() + 2, (float) scStatusR.getCentreY() - 3, 6, 6);
        g.setFont (Font (FontOptions (10.5f, Font::bold)));
        g.drawText (msg, scStatusR.withTrimmedLeft (14), Justification::centredLeft);
    }
}

// Drawn on top of the knobs so the names and values stay readable over the sun glow.
void RedSunAudioProcessorEditor::paintOverChildren (Graphics& g)
{
    for (auto& k : knobLabels)
    {
        const float cx = (float) k.slider->getBounds().getCentreX();
        const float y = (float) k.labelTop;
        const bool active = k.slider->isMouseOverOrDragging();
        const String value = k.slider->getTextFromValue (k.slider->getValue());

        engraved (g, k.name, Rectangle<float> (cx - 48.0f, y, 96.0f, 12.0f), 10.0f, Justification::centred, 0.12f,
                  active ? Colour (0xfff0e8ff) : Colour (0xffb3a7d6));

        g.setFont (Font (FontOptions (11.5f, Font::bold)));
        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawText (value, Rectangle<float> (cx - 48.0f, y + 13.0f, 96.0f, 14.0f), Justification::centred);
        g.setColour (Colour (0xfff6f1ff));
        g.drawText (value, Rectangle<float> (cx - 48.0f, y + 12.0f, 96.0f, 14.0f), Justification::centred);
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

    // refresh the value text under any knob that changed or was hovered
    for (auto& k : knobLabels)
    {
        const String t = k.slider->getTextFromValue (k.slider->getValue());
        const bool hov = k.slider->isMouseOverOrDragging();
        if (t != k.lastText || hov != k.lastHover)
        {
            k.lastText = t;
            k.lastHover = hov;
            repaint (Rectangle<int> (k.slider->getBounds().getCentreX() - 50, k.labelTop, 100, 30));
        }
    }

    const bool scOn = saviorBtn.getToggleState();
    if (scOn != lastScOn)
    {
        lastScOn = scOn;
        for (auto* k : { &scLoK, &scHiK, &scDepthK, &scThreshK, &scAttK, &scRelK }) k->setAlpha (scOn ? 1.0f : 0.5f);
    }
}
