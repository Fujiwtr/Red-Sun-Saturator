#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace RS
{
using namespace juce;

inline float sunTime() { return (float) (Time::getMillisecondCounterHiRes() * 0.001); }
inline Colour mixColour (Colour a, Colour b, float t) { return a.interpolatedWith (b, jlimit (0.0f, 1.0f, t)); }

// Each band gets a colour taken from the sun's spectrum: crimson -> ember -> amber -> white-hot
inline Colour bandColour (int b)
{
    static const uint32 c[4] = { 0xffc62a2a, 0xffe8602a, 0xfff09a32, 0xffffd27a };
    return Colour (c[b & 3]);
}

//==============================================================================
// Rotary knob. A "sun" knob radiates corona + flares as it is turned up.
class Knob : public Slider
{
public:
    explicit Knob (bool isSun = false) : sun (isSun)
    {
        getProperties().set ("sun", isSun);
        setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (180);
    }

    bool hitTest (int x, int y) override
    {
        if (! sun) return Slider::hitTest (x, y);
        const auto c = getLocalBounds().toFloat().getCentre();
        const float r = std::min (getWidth(), getHeight()) * 0.5f * 0.62f;
        return c.getDistanceFrom (Point<float> ((float) x, (float) y)) <= r;
    }

    void mouseEnter (const MouseEvent& e) override { Slider::mouseEnter (e); repaint(); }
    void mouseExit  (const MouseEvent& e) override { Slider::mouseExit (e);  repaint(); }

    float intensity() { return sun ? (float) std::pow (valueToProportionOfLength (getValue()), 1.25) : 0.0f; }
    bool isSun() const { return sun; }

private:
    bool sun;
};

//==============================================================================
class SaviorButton : public Button
{
public:
    SaviorButton() : Button ("Sidechain Savior") { setClickingTogglesState (true); setMouseCursor (MouseCursor::PointingHandCursor); }

    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        const bool on = getToggleState();
        const float t = sunTime();
        const float pulse = 0.75f + 0.25f * std::sin (t * 3.0f);

        if (on)
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (Colour::fromFloatRGBA (1.0f, 0.25f, 0.08f, 0.06f * pulse * (float) (5 - i)));
                g.fillRoundedRectangle (r.expanded ((float) i * 1.6f), 8.0f + (float) i);
            }

        g.setGradientFill (on ? ColourGradient (Colour (0xff6e1a14), 0, r.getY(), Colour (0xff2a0807), 0, r.getBottom(), false)
                              : ColourGradient (Colour (down ? 0xff4a3f6e : 0xff5c5088), 0, r.getY(), Colour (0xff231b3d), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);

        g.setColour (on ? Colour (0xffff7a45).withAlpha (0.55f + 0.35f * pulse) : Colour (over ? 0xffb7a6f0 : 0xff7b6bb5));
        g.drawRoundedRectangle (r, 8.0f, 1.6f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawLine (r.getX() + 8, r.getY() + 1.5f, r.getRight() - 8, r.getY() + 1.5f);

        // mini sun indicator
        const auto ic = Point<float> (r.getX() + 24.0f, r.getCentreY());
        if (on)
        {
            g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.4f, 0.1f, 0.55f * pulse), ic.x, ic.y,
                                               Colour::fromFloatRGBA (1.0f, 0.2f, 0.0f, 0.0f), ic.x + 16, ic.y, true));
            g.fillEllipse (ic.x - 16, ic.y - 16, 32, 32);
            g.setColour (Colour (0xffff9a4a).withAlpha (0.9f));
            for (int k = 0; k < 10; ++k)
            {
                const float a = (float) k * MathConstants<float>::twoPi / 10.0f + t * 0.6f;
                g.drawLine (ic.x + std::sin (a) * 8.0f, ic.y - std::cos (a) * 8.0f,
                            ic.x + std::sin (a) * (11.0f + 2.0f * std::sin (t * 4 + (float) k)), ic.y - std::cos (a) * (11.0f + 2.0f * std::sin (t * 4 + (float) k)), 1.3f);
            }
        }
        g.setGradientFill (on ? ColourGradient (Colour (0xffffe08a), ic.x - 2, ic.y - 3, Colour (0xffd8331a), ic.x + 5, ic.y + 5, true)
                              : ColourGradient (Colour (0xff8a8c93), ic.x - 2, ic.y - 3, Colour (0xff2d2e32), ic.x + 5, ic.y + 5, true));
        g.fillEllipse (ic.x - 6, ic.y - 6, 12, 12);

        g.setColour (on ? Colour (0xfffff0e6) : Colour (0xffc4c6cc));
        g.setFont (Font (FontOptions (14.5f, Font::bold).withKerningFactor (0.14f)));
        g.drawText ("SIDECHAIN SAVIOR", r.withTrimmedLeft (40).withTrimmedRight (44), Justification::centredLeft);

        g.setFont (Font (FontOptions (11.0f, Font::bold).withKerningFactor (0.1f)));
        g.setColour (on ? Colour (0xffffb27a) : Colour (0xff80828a));
        g.drawText (on ? "ON" : "OFF", r.withTrimmedRight (12), Justification::centredRight);
    }
};

//==============================================================================
class SunLookAndFeel : public LookAndFeel_V4
{
public:
    SunLookAndFeel()
    {
        setColour (ComboBox::backgroundColourId, Colour (0xff1b1430));
        setColour (ComboBox::outlineColourId,    Colour (0xff7c6cb8));
        setColour (ComboBox::textColourId,       Colour (0xffd8d9dd));
        setColour (ComboBox::arrowColourId,      Colour (0xffe0e1e5));
        setColour (PopupMenu::backgroundColourId,            Colour (0xff1b1430));
        setColour (PopupMenu::textColourId,                  Colour (0xffd0d1d6));
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff6a3fc8));
        setColour (PopupMenu::highlightedTextColourId,       Colours::white);
        setColour (TooltipWindow::backgroundColourId, Colour (0xf01b1430));
        setColour (TooltipWindow::textColourId,       Colour (0xfff0eaff));
        setColour (TooltipWindow::outlineColourId,    Colour (0xff7b6bb5));
    }

    //--------------------------------------------------------------------------
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        const bool sun = (bool) s.getProperties()["sun"];
        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto c = bounds.getCentre();
        const float gm = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;   // glow radius budget
        const float R = sun ? gm * 0.50f : gm * 0.66f;                                // knob radius
        const float arcR = sun ? R * 1.16f : gm * 0.90f;
        const float I = sun ? std::pow (pos, 1.25f) : 0.0f;                           // sun intensity 0..1
        const float t = sunTime();
        const float ang = a0 + pos * (a1 - a0);

        //---------------- corona + rays (sun knobs only)
        if (sun && I > 0.01f)
        {
            ColourGradient cg (Colour::fromFloatRGBA (1.0f, 0.45f, 0.10f, 0.75f * I), c.x, c.y,
                               Colour::fromFloatRGBA (0.8f, 0.05f, 0.0f, 0.0f), c.x + gm, c.y, true);
            cg.addColour ((double) (arcR / gm), Colour::fromFloatRGBA (1.0f, 0.30f, 0.06f, 0.50f * I));
            cg.addColour (0.62, Colour::fromFloatRGBA (0.95f, 0.15f, 0.03f, 0.18f * I));
            g.setGradientFill (cg);
            g.fillEllipse (c.x - gm, c.y - gm, gm * 2.0f, gm * 2.0f);

            const int N = 36;
            const float lenMax = gm - 2.0f - arcR;
            for (int k = 0; k < N; ++k)
            {
                const float base  = (float) k * MathConstants<float>::twoPi / (float) N + t * (0.10f + 0.25f * I);
                const float flick = 0.62f + 0.38f * std::sin (t * (1.7f + (float) (k % 5) * 0.55f) + (float) k * 1.9f);
                const float len   = lenMax * I * flick * ((k & 1) ? 0.55f : 1.0f);
                if (len < 2.0f) continue;
                const float hw = 0.045f * (0.8f + 0.5f * I), r0 = arcR + 1.0f;
                const Point<float> p0 (c.x + std::sin (base - hw) * r0, c.y - std::cos (base - hw) * r0);
                const Point<float> p1 (c.x + std::sin (base + hw) * r0, c.y - std::cos (base + hw) * r0);
                const Point<float> tip (c.x + std::sin (base) * (r0 + len), c.y - std::cos (base) * (r0 + len));
                Path ray; ray.addTriangle (p0, p1, tip);
                g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.80f, 0.35f, 0.90f * I), (p0 + p1) * 0.5f,
                                                   Colour::fromFloatRGBA (0.9f, 0.10f, 0.0f, 0.0f), tip, false));
                g.fillPath (ray);
            }
        }

        //---------------- value ring
        Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (Colour (0xff0f0b20));
        g.strokePath (track, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xff5b4f8a));
        g.strokePath (track, PathStrokeType (1.0f));

        if (pos > 0.002f)
        {
            Path val; val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);
            const Colour vc = sun ? mixColour (Colour (0xffc81e14), Colour (0xffffd25a), I) : Colour (0xffa98bff);
            g.setColour (vc.withAlpha (0.22f + 0.3f * I));
            g.strokePath (val, PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (vc);
            g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        //---------------- bezel
        g.setGradientFill (ColourGradient (Colour (0xffb7b9c0), c.x - R * 0.7f, c.y - R * 0.8f,
                                           Colour (0xff1c1d21), c.x + R * 0.7f, c.y + R * 0.8f, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.0f);

        //---------------- face (becomes a red sun)
        const float Rf = R * 0.82f;
        Path face; face.addEllipse (c.x - Rf, c.y - Rf, Rf * 2.0f, Rf * 2.0f);
        if (sun)
        {
            ColourGradient fg (mixColour (Colour (0xff8a8d94), Colour (0xffffb347), I), c.x - Rf * 0.2f, c.y - Rf * 0.25f,
                               mixColour (Colour (0xff2d2e33), Colour (0xff7a0d08), I), c.x + Rf * 0.9f, c.y + Rf * 0.9f, true);
            fg.addColour (0.55, mixColour (Colour (0xff55575d), Colour (0xffe2401c), I));
            g.setGradientFill (fg);
        }
        else
            g.setGradientFill (ColourGradient (Colour (0xff5b5d64), c.x - Rf * 0.3f, c.y - Rf * 0.4f,
                                               Colour (0xff222327), c.x + Rf, c.y + Rf, true));
        g.fillPath (face);

        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (face, {});
            if (sun && I > 0.04f)
            {
                for (int q = 0; q < 16; ++q)   // drifting plasma cells
                {
                    const float fq = (float) q * 0.61803f, ph = fq - std::floor (fq);
                    const float a  = (float) q * 2.399f + t * (0.10f + 0.04f * (float) (q % 3));
                    const float rr = Rf * (0.15f + 0.70f * ph);
                    const float cr = Rf * (0.10f + 0.14f * std::fmod ((float) q * 0.37f, 1.0f));
                    g.setColour (Colour::fromFloatRGBA (1.0f, 0.75f, 0.30f, 0.22f * I * (0.55f + 0.45f * std::sin (t * 1.4f + (float) q))));
                    g.fillEllipse (c.x + std::cos (a) * rr - cr, c.y + std::sin (a) * rr - cr, cr * 2.0f, cr * 2.0f);
                }
                for (int q = 0; q < 4; ++q)    // sunspots
                {
                    const float a = (float) q * 1.9f - t * 0.05f, rr = Rf * (0.3f + 0.15f * (float) q), cr = Rf * 0.09f;
                    g.setColour (Colour::fromFloatRGBA (0.12f, 0.0f, 0.0f, 0.30f * I));
                    g.fillEllipse (c.x + std::cos (a) * rr - cr, c.y + std::sin (a) * rr - cr, cr * 2.0f, cr * 2.0f);
                }
            }
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f * (1.0f - 0.6f * I)), c.x, c.y - Rf,
                                               Colours::transparentWhite, c.x, c.y, false));
            g.fillPath (face);
        }
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - Rf, c.y - Rf, Rf * 2.0f, Rf * 2.0f, 1.0f);

        //---------------- pointer
        const float p0 = Rf * 0.18f, p1 = Rf * 0.92f;
        g.setColour (sun ? mixColour (Colours::white, Colour (0xffffe6a0), I) : Colour (0xfff0f0f2));
        g.drawLine (c.x + std::sin (ang) * p0, c.y - std::cos (ang) * p0,
                    c.x + std::sin (ang) * p1, c.y - std::cos (ang) * p1, sun ? 3.0f : 2.2f);

    }

    //--------------------------------------------------------------------------
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const bool solo = b.getProperties()["kind"].toString() == "solo";
        const Colour onCol = solo ? Colour (0xffe8a817) : Colour (0xffd8341f);

        if (on) { g.setColour (onCol.withAlpha (0.35f)); g.fillRoundedRectangle (r.expanded (2.0f), 5.0f); }
        g.setGradientFill (on ? ColourGradient (onCol.brighter (0.2f), 0, r.getY(), onCol.darker (0.5f), 0, r.getBottom(), false)
                              : ColourGradient (Colour (down ? 0xff2d2548 : 0xff4a4070), 0, r.getY(), Colour (0xff1f1935), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colour (over ? 0xffb7a6f0 : 0xff2a2150));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        const bool solo = b.getProperties()["kind"].toString() == "solo";
        g.setFont (Font (FontOptions (11.0f, Font::bold)));
        g.setColour (on ? (solo ? Colour (0xff201400) : Colours::white) : Colour (0xffb0b2b8));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
    }

    //--------------------------------------------------------------------------
    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setGradientFill (ColourGradient (Colour (0xff2b2342), 0, 0, Colour (0xff130d26), 0, (float) h, false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (box.hasKeyboardFocus (true) ? Colour (0xffe0442c) : Colour (0xff7c6cb8));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        Path arrow;
        const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (Colour (0xffe0e1e5));
        g.fillPath (arrow);
    }

    Font getComboBoxFont (ComboBox&) override { return Font (FontOptions (12.5f, Font::bold)); }
};
} // namespace RS
