#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <functional>
#include <vector>
#include "PluginProcessor.h"
#include "SunLookAndFeel.h"

namespace RS
{
using namespace juce;

// Spectrum analyser with draggable band crossovers, a Sidechain Savior range overlay,
// and a transfer-curve inset for the selected band.
class DisplayComponent : public Component
{
public:
    DisplayComponent (RedSunAudioProcessor& p, int& selectedBandRef, std::function<void()> onSel)
        : proc (p), selected (selectedBandRef), onSelect (std::move (onSel)),
          fft (order), window ((size_t) N, dsp::WindowingFunction<float>::hann)
    {
        fftData.assign ((size_t) N * 2, 0.0f);
        ring.assign ((size_t) N, 0.0f);
        smoothIn.assign ((size_t) N / 2 + 2, -100.0f);
        smoothOut.assign ((size_t) N / 2 + 2, -100.0f);
    }

    void tick()
    {
        analyse (proc.inRing,  smoothIn);
        analyse (proc.outRing, smoothOut);
        repaint();
    }

    void resized() override { plot = getLocalBounds().toFloat().withTrimmedLeft (36).withTrimmedTop (10).withTrimmedRight (12).withTrimmedBottom (20); }

    //==========================================================================
    void paint (Graphics& g) override
    {
        const auto all = getLocalBounds().toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff0b0720).withAlpha (0.80f), 0, 0, Colour (0xff04020c).withAlpha (0.90f), 0, all.getBottom(), false));
        g.fillRoundedRectangle (all.reduced (1.0f), 16.0f);

        drawGrid (g);
        drawBands (g);
        drawSpectra (g);
        drawSidechain (g);
        drawTransferInset (g);

        g.setColour (Colour (0xff0a0a0c));
        g.drawRoundedRectangle (all.reduced (1.0f), 16.0f, 3.0f);
        g.setColour (Colour (0xff8f7fd0));
        g.drawRoundedRectangle (all.reduced (2.5f), 15.0f, 1.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), 0, all.getY(), Colours::transparentWhite, 0, all.getY() + 60, false));
        g.fillRoundedRectangle (all.reduced (3.0f).withHeight (60), 14.0f);
    }

    //==========================================================================
    void mouseMove (const MouseEvent& e) override
    {
        const auto h = hitHandle (e.position);
        if (h != hover) { hover = h; repaint(); }
        setMouseCursor (h == Handle::none ? MouseCursor::NormalCursor : MouseCursor::LeftRightResizeCursor);
    }
    void mouseExit (const MouseEvent&) override { hover = Handle::none; repaint(); }

    void mouseDown (const MouseEvent& e) override
    {
        drag = hitHandle (e.position);
        if (drag == Handle::none)
        {
            const double f = freqFromX (e.position.x);
            double x1 = pv ("xover1"), x2 = pv ("xover2"), x3 = pv ("xover3");
            orderCrossovers (x1, x2, x3, srate());
            selected = (f > x1 ? 1 : 0) + (f > x2 ? 1 : 0) + (f > x3 ? 1 : 0);
            if (onSelect) onSelect();
            repaint();
            return;
        }
        if (auto* p = proc.apvts.getParameter (idFor (drag))) p->beginChangeGesture();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (drag == Handle::none) return;
        double f = freqFromX (e.position.x);
        double x1 = pv ("xover1"), x2 = pv ("xover2"), x3 = pv ("xover3");
        orderCrossovers (x1, x2, x3, srate());
        const double lo = pv ("scLo"), hi = pv ("scHi");

        switch (drag)
        {
            case Handle::x1:   f = jlimit (30.0, x2 / 1.25, f); break;
            case Handle::x2:   f = jlimit (x1 * 1.25, x3 / 1.25, f); break;
            case Handle::x3:   f = jlimit (x2 * 1.25, 16000.0, f); break;
            case Handle::scLo: f = jlimit (20.0, hi / 1.2, f); break;
            case Handle::scHi: f = jlimit (lo * 1.2, 20000.0, f); break;
            default: break;
        }
        if (auto* p = proc.apvts.getParameter (idFor (drag)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) f));
    }

    void mouseUp (const MouseEvent&) override
    {
        if (drag != Handle::none)
            if (auto* p = proc.apvts.getParameter (idFor (drag))) p->endChangeGesture();
        drag = Handle::none;
    }

private:
    enum class Handle { none, x1, x2, x3, scLo, scHi };

    static constexpr int order = 12, N = 1 << order;
    static constexpr float dbTop = 0.0f, dbBottom = -84.0f;

    RedSunAudioProcessor& proc;
    int& selected;
    std::function<void()> onSelect;
    dsp::FFT fft;
    dsp::WindowingFunction<float> window;
    std::vector<float> fftData, ring, smoothIn, smoothOut;
    Rectangle<float> plot;
    Handle hover = Handle::none, drag = Handle::none;

    double srate() const { return proc.getSampleRateHz() > 0 ? proc.getSampleRateHz() : 44100.0; }
    float pv (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    static const char* idFor (Handle h)
    {
        switch (h) { case Handle::x1: return "xover1"; case Handle::x2: return "xover2"; case Handle::x3: return "xover3";
                     case Handle::scLo: return "scLo"; case Handle::scHi: return "scHi"; default: return "xover1"; }
    }

    float xFromFreq (double f) const { return plot.getX() + plot.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); }
    double freqFromX (float x) const { return 20.0 * std::pow (1000.0, (double) ((x - plot.getX()) / plot.getWidth())); }
    float yFromDb (float db) const { return jmap (db, dbTop, dbBottom, plot.getY(), plot.getBottom()); }

    Handle hitHandle (Point<float> p) const
    {
        if (! plot.expanded (0, 6).contains (p)) return Handle::none;
        double x1 = pv ("xover1"), x2 = pv ("xover2"), x3 = pv ("xover3");
        orderCrossovers (x1, x2, x3, srate());
        struct C { Handle h; float x; };
        std::vector<C> cands { { Handle::x1, xFromFreq (x1) }, { Handle::x2, xFromFreq (x2) }, { Handle::x3, xFromFreq (x3) } };
        if (pv ("scOn") > 0.5f)
        {
            cands.push_back ({ Handle::scLo, xFromFreq (jlimit (20.0, 20000.0, (double) pv ("scLo"))) });
            cands.push_back ({ Handle::scHi, xFromFreq (jlimit (20.0, 20000.0, (double) pv ("scHi"))) });
        }
        float best = 7.0f; Handle res = Handle::none;
        for (auto& c : cands) { const float d = std::abs (p.x - c.x); if (d < best) { best = d; res = c.h; } }
        return res;
    }

    //==========================================================================
    void analyse (const std::array<float, RedSunAudioProcessor::ringSize>& r, std::vector<float>& sm)
    {
        proc.copyRing (r, ring.data(), N);
        std::copy (ring.begin(), ring.end(), fftData.begin());
        std::fill (fftData.begin() + N, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) N);
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        for (int k = 0; k <= N / 2; ++k)
        {
            const float mag = fftData[(size_t) k] / ((float) N * 0.25f);
            const float db = 20.0f * std::log10 (std::max (mag, 1.0e-6f));
            float& s = sm[(size_t) k];
            s += (db > s ? 0.55f : 0.10f) * (db - s);
        }
    }

    Path spectrumPath (const std::vector<float>& sm, bool closed) const
    {
        Path p;
        const double sr = srate();
        bool started = false;
        for (float px = 0.0f; px <= plot.getWidth(); px += 2.0f)
        {
            const double f0 = freqFromX (plot.getX() + px - 1.5f), f1 = freqFromX (plot.getX() + px + 1.5f), f = freqFromX (plot.getX() + px);
            const int b0 = jlimit (0, N / 2 - 1, (int) std::floor (f0 * N / sr)), b1 = jlimit (0, N / 2, (int) std::ceil (f1 * N / sr));
            float db;
            if (b1 - b0 <= 1)
            {
                const double bin = f * N / sr; const int i0 = jlimit (0, N / 2 - 1, (int) bin); const float fr = (float) (bin - i0);
                db = sm[(size_t) i0] * (1.0f - fr) + sm[(size_t) i0 + 1] * fr;
            }
            else { db = -200.0f; for (int k = b0; k <= b1; ++k) db = std::max (db, sm[(size_t) k]); }
            db += 4.5f * (float) std::log2 (f / 1000.0);       // 4.5 dB/oct slope, like pro analysers
            const float y = jlimit (plot.getY(), plot.getBottom(), yFromDb (db));
            const float x = plot.getX() + px;
            if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
        }
        if (closed) { p.lineTo (plot.getRight(), plot.getBottom()); p.lineTo (plot.getX(), plot.getBottom()); p.closeSubPath(); }
        return p;
    }

    //==========================================================================
    void drawGrid (Graphics& g) const
    {
        g.setFont (Font (FontOptions (10.0f)));
        for (float db = 0.0f; db >= dbBottom; db -= 12.0f)
        {
            const float y = yFromDb (db);
            g.setColour (Colours::white.withAlpha (0.06f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (Colour (0xff80828a));
            g.drawText (String ((int) db), Rectangle<float> (0, y - 7, plot.getX() - 5, 14), Justification::centredRight);
        }
        static const double fs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (double f : fs)
        {
            const float x = xFromFreq (f);
            g.setColour (Colours::white.withAlpha (0.06f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (Colour (0xff80828a));
            g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), Rectangle<float> (x - 20, plot.getBottom() + 3, 40, 14), Justification::centred);
        }
    }

    void drawBands (Graphics& g) const
    {
        double x1 = pv ("xover1"), x2 = pv ("xover2"), x3 = pv ("xover3");
        orderCrossovers (x1, x2, x3, srate());
        const double edges[5] = { 20.0, x1, x2, x3, 20000.0 };
        const Handle hs[3] = { Handle::x1, Handle::x2, Handle::x3 };

        for (int b = 0; b < numBands; ++b)
        {
            const float xa = xFromFreq (edges[b]), xb = xFromFreq (edges[b + 1]);
            const Rectangle<float> rr (xa, plot.getY(), xb - xa, plot.getHeight());
            const bool sel = (b == selected);
            const bool muted = pv (bandId ("mute", b).toRawUTF8()) > 0.5f;
            const Colour col = bandColour (b).withMultipliedAlpha (muted ? 0.35f : 1.0f);
            const float di = std::pow (pv (bandId ("drive", b).toRawUTF8()) * 0.01f, 1.2f);

            g.setGradientFill (ColourGradient (col.withAlpha (sel ? 0.30f : 0.13f), 0, rr.getY(), col.withAlpha (0.02f), 0, rr.getBottom(), false));
            g.fillRect (rr);

            // "heat bar": the band's drive amount glows along the top edge
            g.setGradientFill (ColourGradient (col.withAlpha (0.65f * di), 0, rr.getY(), col.withAlpha (0.0f), 0, rr.getY() + 46.0f, false));
            g.fillRect (rr.withHeight (46.0f));
            g.setColour (col.withAlpha (0.30f + 0.70f * di));
            g.fillRect (rr.withHeight (4.0f));

            g.setColour (col.brighter (0.3f).withAlpha (sel ? 1.0f : 0.7f));
            g.setFont (Font (FontOptions (13.0f, Font::bold)));
            g.drawText (String (b + 1), rr.withTrimmedTop (8).withHeight (18), Justification::centred);
        }

        for (int i = 0; i < 3; ++i)
        {
            const float x = xFromFreq (edges[i + 1]);
            const bool act = (hover == hs[i] || drag == hs[i]);
            g.setColour (Colour (0xffd6d8de).withAlpha (act ? 0.95f : 0.50f));
            g.drawLine (x, plot.getY(), x, plot.getBottom(), act ? 1.6f : 1.0f);
            const Rectangle<float> h (x - 5.0f, plot.getY() + 26.0f, 10.0f, 22.0f);
            g.setGradientFill (ColourGradient (Colour (0xffc4c6cc), h.getX(), 0, Colour (0xff6d6f77), h.getRight(), 0, false));
            g.fillRoundedRectangle (h, 3.0f);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (h, 3.0f, 1.0f);
            g.drawLine (x - 2, h.getY() + 6, x - 2, h.getBottom() - 6, 1.0f);
            g.drawLine (x + 2, h.getY() + 6, x + 2, h.getBottom() - 6, 1.0f);
        }
    }

    void drawSpectra (Graphics& g) const
    {
        g.setColour (Colour (0xffb8bac2).withAlpha (0.40f));
        g.strokePath (spectrumPath (smoothIn, false), PathStrokeType (1.0f));

        const Path out = spectrumPath (smoothOut, true);
        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.36f, 0.16f, 0.72f), 0, plot.getY(),
                                           Colour::fromFloatRGBA (0.45f, 0.04f, 0.04f, 0.10f), 0, plot.getBottom(), false));
        g.fillPath (out);
        g.setColour (Colour (0xffffa45c).withAlpha (0.35f));
        g.strokePath (spectrumPath (smoothOut, false), PathStrokeType (3.0f));
        g.setColour (Colour (0xffffc58a));
        g.strokePath (spectrumPath (smoothOut, false), PathStrokeType (1.2f));
    }

    void drawSidechain (Graphics& g) const
    {
        if (pv ("scOn") < 0.5f) return;
        const double lo = jlimit (20.0, 20000.0, (double) pv ("scLo")), hi = jlimit (20.0, 20000.0, (double) pv ("scHi"));
        const float xa = xFromFreq (lo), xb = xFromFreq (std::max (hi, lo * 1.2));
        const Colour sc (0xff9ccaff);
        const Rectangle<float> rr (xa, plot.getY(), xb - xa, plot.getHeight());

        g.setGradientFill (ColourGradient (sc.withAlpha (0.14f), 0, rr.getY(), sc.withAlpha (0.03f), 0, rr.getBottom(), false));
        g.fillRect (rr);

        const float red = proc.scReductionDb.load();               // live gain reduction
        const float dipH = jlimit (0.0f, 1.0f, red / 30.0f) * plot.getHeight() * 0.62f;
        if (dipH > 1.0f)
        {
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.32f), 0, rr.getY(), sc.withAlpha (0.0f), 0, rr.getY() + dipH, false));
            g.fillRect (rr.withHeight (dipH));
            g.setColour (Colours::white.withAlpha (0.8f));
            g.drawLine (rr.getX(), rr.getY() + dipH, rr.getRight(), rr.getY() + dipH, 1.4f);
        }

        const Handle hs[2] = { Handle::scLo, Handle::scHi };
        const float xs[2] = { xa, xb };
        for (int i = 0; i < 2; ++i)
        {
            const bool act = (hover == hs[i] || drag == hs[i]);
            Path line; line.startNewSubPath (xs[i], plot.getY()); line.lineTo (xs[i], plot.getBottom());
            Path dashed; const float d[] = { 5.0f, 3.0f };
            PathStrokeType (1.4f).createDashedStroke (dashed, line, d, 2);
            g.setColour (sc.withAlpha (act ? 1.0f : 0.75f));
            g.fillPath (dashed);
            const Rectangle<float> flag (xs[i] - (i == 0 ? 36.0f : 0.0f), plot.getBottom() - 20.0f, 36.0f, 16.0f);
            g.setColour (sc.withAlpha (0.85f));
            g.fillRoundedRectangle (flag, 3.0f);
            g.setColour (Colour (0xff0a1626));
            g.setFont (Font (FontOptions (10.0f, Font::bold)));
            const double f = i == 0 ? lo : std::max (hi, lo * 1.2);
            g.drawText (f >= 1000.0 ? String (f / 1000.0, 1) + "k" : String ((int) f), flag, Justification::centred);
        }

        g.setColour (sc);
        g.setFont (Font (FontOptions (11.0f, Font::bold).withKerningFactor (0.1f)));
        g.drawText ("SIDECHAIN SAVIOR  " + (red > 0.05f ? "-" + String (red, 1) + " dB" : String ("ARMED")),
                    rr.withTrimmedTop (plot.getHeight() * 0.36f).withHeight (16.0f).expanded (60.0f, 0.0f), Justification::centred);
    }

    void drawTransferInset (Graphics& g) const
    {
        const Rectangle<float> box (plot.getX() + 8.0f, plot.getBottom() - 118.0f, 108.0f, 108.0f);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (Colour (0xff4a4c54));
        g.drawRoundedRectangle (box, 5.0f, 1.0f);

        const auto inner = box.reduced (8.0f);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawHorizontalLine ((int) inner.getCentreY(), inner.getX(), inner.getRight());
        g.drawVerticalLine ((int) inner.getCentreX(), inner.getY(), inner.getBottom());
        g.drawLine (inner.getX(), inner.getBottom(), inner.getRight(), inner.getY(), 0.8f);

        const int b = selected;
        const int style = jlimit (0, numStyles - 1, (int) std::lround (pv (bandId ("style", b).toRawUTF8())));
        const float driveDb = pv (bandId ("drive", b).toRawUTF8()) * 0.36f + pv ("masterDrive") * 0.18f;
        const float gain = dbToLinear (driveDb);
        Path curve;
        for (int i = 0; i <= 100; ++i)
        {
            const float x = ((float) i / 100.0f) * 2.0f - 1.0f;
            const float y = jlimit (-1.0f, 1.0f, shape (style, x * gain) / std::sqrt (gain));
            const Point<float> p (inner.getX() + (x * 0.5f + 0.5f) * inner.getWidth(), inner.getBottom() - (y * 0.5f + 0.5f) * inner.getHeight());
            i == 0 ? curve.startNewSubPath (p) : curve.lineTo (p);
        }
        g.setColour (bandColour (b).withAlpha (0.35f));
        g.strokePath (curve, PathStrokeType (4.0f));
        g.setColour (bandColour (b).brighter (0.4f));
        g.strokePath (curve, PathStrokeType (1.5f));

        g.setColour (Colour (0xffb0b2b8));
        g.setFont (Font (FontOptions (9.5f, Font::bold)));
        g.drawText (String ("B") + String (b + 1) + " - " + styleNames()[style].toUpperCase(), box.withHeight (12.0f).translated (0, 2), Justification::centred);
    }

    static float dbToLinear (float db) { return std::pow (10.0f, db * 0.05f); }
};
} // namespace RS
