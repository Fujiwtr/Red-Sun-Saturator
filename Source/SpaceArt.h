#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

// Everything here is generated procedurally at start-up: no image assets are needed.
namespace RS::SpaceArt
{
using namespace juce;

//------------------------------------------------------------------ noise helpers
inline float hash21 (int x, int y, int seed) noexcept
{
    uint32 h = (uint32) x * 374761393u + (uint32) y * 668265263u + (uint32) seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float) (h & 0xffffffu) / 16777215.0f;
}
inline float sstep (float t) noexcept { t = jlimit (0.0f, 1.0f, t); return t * t * (3.0f - 2.0f * t); }
inline float vnoise (float x, float y, int seed) noexcept
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float a = hash21 (xi, yi, seed), b = hash21 (xi + 1, yi, seed);
    const float c = hash21 (xi, yi + 1, seed), d = hash21 (xi + 1, yi + 1, seed);
    const float u = sstep (fx), v = sstep (fy);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm (float x, float y, int seed, int oct = 5) noexcept
{
    float s = 0, a = 0.5f, f = 1.0f, n = 0;
    for (int i = 0; i < oct; ++i) { s += a * vnoise (x * f, y * f, seed + i * 17); n += a; a *= 0.5f; f *= 2.03f; }
    return s / n;
}

//------------------------------------------------------------------ the faint red sun
inline Image createSunImage (int S, float R0)
{
    Image img (Image::ARGB, S, S, true);
    {
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        const float cx = (float) S * 0.5f, cy = cx;
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                const float dx = (float) x - cx, dy = (float) y - cy;
                const float d = std::sqrt (dx * dx + dy * dy) / R0;
                Colour col = Colours::transparentBlack;

                if (d < 1.02f)
                {
                    const float dd = std::min (d, 1.0f);
                    const float mu = std::sqrt (1.0f - dd * dd);
                    const float limb = 0.38f + 0.62f * std::pow (mu, 0.65f);              // limb darkening
                    const float gran = fbm ((float) x * 0.055f, (float) y * 0.055f, 7, 4);   // granulation cells
                    const float fine = vnoise ((float) x * 0.22f, (float) y * 0.22f, 3);
                    const float sup  = fbm ((float) x * 0.014f, (float) y * 0.014f, 9, 4);
                    const float spot = sstep ((fbm ((float) x * 0.010f + 13.0f, (float) y * 0.010f, 11, 3) - 0.60f) / 0.08f);
                    const float heat = jlimit (0.0f, 1.0f, 0.30f + 0.55f * gran + 0.25f * (sup - 0.5f) + 0.10f * (fine - 0.5f)) * limb;
                    const float k = 1.0f - 0.65f * spot;
                    const float edge = jlimit (0.0f, 1.0f, (1.02f - d) / 0.035f);
                    col = Colour::fromFloatRGBA ((0.42f + 0.58f * heat) * k, (0.05f + 0.31f * heat) * k, (0.03f + 0.11f * heat) * k, 0.34f * edge);
                }
                else if (d < 1.85f)
                {
                    const float fall = std::exp (-(d - 1.0f) * 4.5f);
                    const float a = std::atan2 (dy, dx);
                    const float st = fbm (std::cos (a) * 2.4f + d * 0.9f, std::sin (a) * 2.4f + d * 0.9f, 21, 4);
                    const float alpha = 0.20f * fall * (0.5f + st) * jlimit (0.0f, 1.0f, (1.85f - d) / 0.35f);
                    col = Colour::fromFloatRGBA (1.0f, 0.28f + 0.25f * fall, 0.10f, alpha);
                }
                bd.setPixelColour (x, y, col);
            }
    }

    // prominences and spicules at the limb
    Graphics g (img);
    Random rnd (77);
    const Point<float> c ((float) S * 0.5f, (float) S * 0.5f);
    auto polar = [&] (float a, float r) { return Point<float> (c.x + std::cos (a) * r, c.y + std::sin (a) * r); };
    for (int i = 0; i < 6; ++i)
    {
        const float a = rnd.nextFloat() * MathConstants<float>::twoPi, span = 0.05f + 0.05f * rnd.nextFloat();
        const float h = 1.14f + 0.32f * rnd.nextFloat();
        Path p; p.startNewSubPath (polar (a - span, R0 * 0.99f));
        p.quadraticTo (polar (a, R0 * (1.0f + (h - 1.0f) * 1.9f)), polar (a + span, R0 * 0.99f));
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.25f, 0.08f, 0.10f)); g.strokePath (p, PathStrokeType (9.0f));
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.40f, 0.12f, 0.20f)); g.strokePath (p, PathStrokeType (4.0f));
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.65f, 0.30f, 0.32f)); g.strokePath (p, PathStrokeType (1.4f));
    }
    for (int i = 0; i < 160; ++i)
    {
        const float a = rnd.nextFloat() * MathConstants<float>::twoPi, l = 5.0f + 11.0f * rnd.nextFloat();
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.35f, 0.12f, 0.10f + 0.12f * rnd.nextFloat()));
        g.drawLine ({ polar (a, R0 * 0.995f), polar (a, R0 * 0.995f + l) }, 0.9f);
    }
    return img;
}

//------------------------------------------------------------------ planets
// style: 0 rock, 1 ice, 2 gas giant, 3 lava-cracked
inline Image makePlanetTexture (int size, int seed, int style)
{
    Image img (Image::ARGB, size, size, true);
    Image::BitmapData bd (img, Image::BitmapData::writeOnly);
    Random rnd (seed);
    struct Crater { float x, y, r; };
    std::vector<Crater> craters;
    for (int i = 0; i < 18; ++i) craters.push_back ({ rnd.nextFloat() * 1.8f - 0.9f, rnd.nextFloat() * 1.8f - 0.9f, 0.04f + 0.15f * rnd.nextFloat() });

    const float Lx = 0.55f, Ly = -0.45f, Lz = 0.70f;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
        {
            const float nx = ((float) x + 0.5f) / (float) size * 2.0f - 1.0f, ny = ((float) y + 0.5f) / (float) size * 2.0f - 1.0f;
            const float r2 = nx * nx + ny * ny;
            if (r2 > 1.0f) { bd.setPixelColour (x, y, Colours::transparentBlack); continue; }
            const float nz = std::sqrt (1.0f - r2);
            const float fx = nx * 3.0f + (float) seed * 0.1f, fy = ny * 3.0f;
            const float n = fbm (fx * 1.5f, fy * 1.5f, seed, 5);

            float r, g, b, emis = 0.0f, shade = 1.0f;
            auto lerp = [] (float a, float b2, float t) { return a + (b2 - a) * t; };
            switch (style)
            {
                case 0:  r = lerp (0.23f, 0.61f, n); g = lerp (0.20f, 0.54f, n); b = lerp (0.18f, 0.46f, n); break;
                case 1: { const float band = fbm (fx * 0.6f, fy * 2.8f, seed, 4);
                          r = lerp (0.27f, 0.86f, band); g = lerp (0.39f, 0.92f, band); b = lerp (0.59f, 0.98f, band); break; }
                case 2: { const float band = 0.5f + 0.5f * std::sin (ny * 14.0f + n * 5.0f);
                          r = lerp (0.47f, 0.87f, band); g = lerp (0.27f, 0.67f, band); b = lerp (0.16f, 0.43f, band); break; }
                default: { r = lerp (0.10f, 0.27f, n); g = lerp (0.09f, 0.24f, n); b = lerp (0.09f, 0.24f, n);
                           const float v = fbm (fx * 2.2f, fy * 2.2f, seed + 5, 5);
                           emis = std::pow (std::max (0.0f, 1.0f - std::abs (v - 0.5f) * 7.0f), 2.0f); break; }
            }
            if (style != 2)
                for (auto& c : craters)
                {
                    const float d = std::sqrt ((nx - c.x) * (nx - c.x) + (ny - c.y) * (ny - c.y)) / c.r;
                    if (d < 1.0f) shade *= 0.74f + 0.26f * d * d;
                    else if (d < 1.15f) shade *= 1.0f + 0.25f * (1.15f - d) / 0.15f;
                }
            const float diff = std::max (0.0f, nx * Lx + ny * Ly + nz * Lz);
            const float light = (0.10f + 0.95f * std::pow (diff, 0.9f)) * shade;
            const float edge = sstep ((1.0f - std::sqrt (r2)) / 0.03f);
            bd.setPixelColour (x, y, Colour::fromFloatRGBA (jlimit (0.0f, 1.0f, r * light * 1.0f + emis),
                                                            jlimit (0.0f, 1.0f, g * light * 0.82f + emis * 0.4f),
                                                            jlimit (0.0f, 1.0f, b * light * 0.70f + emis * 0.1f), edge));
        }
    return img;
}

inline void drawExplodedPlanet (Graphics& g, Point<float> c, float R, int seed, int style)
{
    Random rnd (seed);
    const Image tex = makePlanetTexture ((int) (R * 2.0f), seed, style);
    auto polar = [&] (float a, float r) { return Point<float> (c.x + std::cos (a) * r, c.y + std::sin (a) * r); };

    // explosion glow, gas cloud and shockwave
    {
        ColourGradient eg (Colour::fromFloatRGBA (1.0f, 0.55f, 0.20f, 0.30f), c.x, c.y, Colour::fromFloatRGBA (0.6f, 0.1f, 0.0f, 0.0f), c.x + R * 3.2f, c.y, true);
        eg.addColour (0.3, Colour::fromFloatRGBA (0.9f, 0.30f, 0.08f, 0.14f));
        g.setGradientFill (eg);
        g.fillEllipse (c.x - R * 3.2f, c.y - R * 3.2f, R * 6.4f, R * 6.4f);
        for (int i = 0; i < 7; ++i)
        {
            const Point<float> p = polar (rnd.nextFloat() * 6.283f, R * (0.6f + 1.8f * rnd.nextFloat()));
            const float cr = R * (0.5f + 0.7f * rnd.nextFloat());
            g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (0.55f, 0.25f, 0.15f, 0.10f), p.x, p.y, Colours::transparentBlack, p.x + cr, p.y, true));
            g.fillEllipse (p.x - cr, p.y - cr, cr * 2.0f, cr * 2.0f);
        }
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.6f, 0.3f, 0.10f));
        g.drawEllipse (c.x - R * 2.0f, c.y - R * 2.0f, R * 4.0f, R * 4.0f, 2.0f);
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.7f, 0.4f, 0.05f));
        g.drawEllipse (c.x - R * 2.6f, c.y - R * 2.6f, R * 5.2f, R * 5.2f, 4.0f);
    }

    // fragments: pie-slices of the original sphere, thrown outward
    const int n = 8 + rnd.nextInt (3);
    std::vector<float> w ((size_t) n); float sum = 0;
    for (auto& v : w) { v = 0.6f + rnd.nextFloat(); sum += v; }
    float a0 = rnd.nextFloat() * 6.283f;
    for (int i = 0; i < n; ++i)
    {
        const float a1 = a0 + w[(size_t) i] / sum * 6.2832f, mid = (a0 + a1) * 0.5f;
        Path p;
        p.startNewSubPath (polar (rnd.nextFloat() * 6.283f, R * 0.05f * rnd.nextFloat()));
        for (int k = 1; k <= 3; ++k) p.lineTo (polar (a0 + (rnd.nextFloat() - 0.5f) * 0.10f, R * ((float) k / 3.5f) + (rnd.nextFloat() - 0.5f) * R * 0.08f));
        for (int s = 0; s <= 8; ++s) p.lineTo (polar (a0 + (a1 - a0) * (float) s / 8.0f, R));
        for (int k = 3; k >= 1; --k) p.lineTo (polar (a1 + (rnd.nextFloat() - 0.5f) * 0.10f, R * ((float) k / 3.5f) + (rnd.nextFloat() - 0.5f) * R * 0.08f));
        p.closeSubPath();

        const float dist = R * (0.12f + 0.60f * rnd.nextFloat()), rot = (rnd.nextFloat() - 0.5f) * 0.5f;
        const Point<float> pivot = polar (mid, R * 0.6f);
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (rot, pivot.x, pivot.y).translated (std::cos (mid) * dist, std::sin (mid) * dist));
        g.reduceClipRegion (p, {});
        g.drawImage (tex, Rectangle<float> (c.x - R, c.y - R, R * 2.0f, R * 2.0f));

        ColourGradient hot (Colour (0xffffa040).withAlpha (0.92f), c.x, c.y, Colour (0xffc02010).withAlpha (0.0f), c.x + R * 0.75f, c.y, true);
        hot.addColour (0.35, Colour (0xffe05018).withAlpha (0.55f));
        g.setGradientFill (hot);
        g.fillRect (Rectangle<float> (c.x - R, c.y - R, R * 2.0f, R * 2.0f));

        g.setColour (Colours::black.withAlpha (0.55f));  g.strokePath (p, PathStrokeType (2.4f));
        g.setColour (Colour (0xffff8a3a).withAlpha (0.45f)); g.strokePath (p, PathStrokeType (1.0f));
        a0 = a1;
    }

    // debris field
    for (int i = 0; i < 110; ++i)
    {
        const float a = rnd.nextFloat() * 6.283f, f = std::pow (rnd.nextFloat(), 1.4f);
        const float dist = R * (0.75f + 2.5f * f);
        const float sz = (1.0f + rnd.nextFloat() * 5.5f) * (1.0f - 0.5f * f) * (R / 100.0f + 0.4f);
        const Point<float> p = polar (a, dist);
        if (rnd.nextFloat() < 0.18f)
        {
            g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (1.0f, 0.5f, 0.15f, 0.55f), p.x, p.y, Colours::transparentBlack, p.x + sz * 3.0f, p.y, true));
            g.fillEllipse (p.x - sz * 3.0f, p.y - sz * 3.0f, sz * 6.0f, sz * 6.0f);
        }
        g.setColour (Colour::fromFloatRGBA (1.0f, 0.7f, 0.5f, 0.07));
        g.drawLine ({ polar (a, dist - sz * 6.0f), p }, sz * 0.5f);   // motion streak
        Path rock; const int verts = 5 + rnd.nextInt (2);
        for (int v = 0; v < verts; ++v)
        {
            const float va = (float) v / (float) verts * 6.283f, vr = sz * (0.6f + 0.6f * rnd.nextFloat());
            const Point<float> q (p.x + std::cos (va) * vr, p.y + std::sin (va) * vr);
            v == 0 ? rock.startNewSubPath (q) : rock.lineTo (q);
        }
        rock.closeSubPath();
        const float shade = 0.20f + 0.35f * rnd.nextFloat();
        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (shade * 1.5f, shade * 1.2f, shade, 0.95f), p.x - sz, p.y - sz,
                                           Colour::fromFloatRGBA (shade * 0.3f, shade * 0.25f, shade * 0.25f, 0.95f), p.x + sz, p.y + sz, false));
        g.fillPath (rock);
    }
}

//------------------------------------------------------------------ whole backdrop
inline Image createBackground (int W, int H)
{
    Image img (Image::ARGB, W, H, true);
    Graphics g (img);
    g.setGradientFill (ColourGradient (Colour (0xff050509), 0, 0, Colour (0xff0b0508), (float) W, (float) H, false));
    g.fillAll();

    // nebula
    {
        const int nw = W / 4, nh = H / 4;
        Image neb (Image::ARGB, nw, nh, true);
        {
            Image::BitmapData bd (neb, Image::BitmapData::writeOnly);
            for (int y = 0; y < nh; ++y)
                for (int x = 0; x < nw; ++x)
                {
                    const float fx = (float) x * 4.0f, fy = (float) y * 4.0f;
                    const float n1 = fbm (fx * 0.0045f, fy * 0.0045f, 101, 5);
                    const float n2 = fbm (fx * 0.0080f + 40.0f, fy * 0.0080f, 202, 4);
                    const float m = sstep ((n1 - 0.42f) / 0.32f);
                    bd.setPixelColour (x, y, Colour::fromFloatRGBA (0.22f + 0.40f * n2, 0.05f + 0.04f * n2, 0.34f - 0.12f * n2, m * 0.32f));
                }
        }
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (neb, 0, 0, W, H, 0, 0, nw, nh);
    }

    // stars
    {
        Random rnd (20240611);
        for (int i = 0; i < 1500; ++i)
        {
            const float x = rnd.nextFloat() * (float) W, y = rnd.nextFloat() * (float) H, m = rnd.nextFloat();
            const Colour tint = rnd.nextFloat() < 0.2f ? Colour (0xffaec8ff) : rnd.nextFloat() < 0.3f ? Colour (0xffffd6b0) : Colours::white;
            if (m > 0.992f)
            {
                g.setGradientFill (ColourGradient (tint.withAlpha (0.6f), x, y, tint.withAlpha (0.0f), x + 9.0f, y, true));
                g.fillEllipse (x - 9, y - 9, 18, 18);
                g.setColour (tint.withAlpha (0.35f));
                g.drawLine (x - 8, y, x + 8, y, 0.8f); g.drawLine (x, y - 8, x, y + 8, 0.8f);
                g.setColour (Colours::white); g.fillEllipse (x - 1.2f, y - 1.2f, 2.4f, 2.4f);
            }
            else
            {
                g.setColour (tint.withAlpha (0.15f + 0.65f * m * m * m));
                g.fillRect (x, y, m > 0.9f ? 1.6f : 1.0f, m > 0.9f ? 1.6f : 1.0f);
            }
        }
    }

    // the faint red sun, far behind everything
    {
        const float cx = (float) W * 0.50f, cy = (float) H * 0.50f;
        g.setGradientFill (ColourGradient (Colour::fromFloatRGBA (0.9f, 0.12f, 0.04f, 0.10f), cx, cy, Colours::transparentBlack, cx + (float) W * 0.55f, cy, true));
        g.fillRect (0, 0, W, H);
        const int S = 640;
        const Image sun = createSunImage (S, 175.0f);
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (sun, Rectangle<float> (cx - (float) S * 0.62f, cy - (float) S * 0.62f, (float) S * 1.24f, (float) S * 1.24f));
    }

    // exploded planets
    drawExplodedPlanet (g, { (float) W * 0.68f, 44.0f },        36.0f, 11, 0);   // header
    drawExplodedPlanet (g, { (float) W - 70.0f, (float) H - 120.0f }, 105.0f, 23, 3);
    drawExplodedPlanet (g, { 70.0f, (float) H - 250.0f },        78.0f, 37, 1);
    drawExplodedPlanet (g, { (float) W * 0.30f, (float) H - 40.0f }, 52.0f, 51, 2);

    // vignette
    {
        const float dmax = std::sqrt ((float) (W * W + H * H)) * 0.5f;
        ColourGradient v (Colours::transparentBlack, (float) W * 0.5f, (float) H * 0.5f, Colour (0xaa000000), (float) W * 0.5f + dmax, (float) H * 0.5f, true);
        v.addColour (0.55, Colours::transparentBlack);
        g.setGradientFill (v);
        g.fillAll();
    }
    return img;
}
} // namespace RS::SpaceArt
