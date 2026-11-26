#pragma once
#include <cmath>
#include <algorithm>

namespace RS
{
inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

//==============================================================================
// Plain double-precision biquad (RBJ). No heap allocation, safe on the audio thread.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] = { 0, 0 }, z2[2] = { 0, 0 };

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }

    inline double process (int ch, double x) noexcept
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (double sr, double f, double q = 0.70710678118) noexcept
    {
        const double w0 = 6.283185307179586 * f / sr, cw = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (1.0 - cw) * 0.5 / a0; b1 = (1.0 - cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0;        a2 = (1.0 - al) / a0;
    }

    void setHighpass (double sr, double f, double q = 0.70710678118) noexcept
    {
        const double w0 = 6.283185307179586 * f / sr, cw = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (1.0 + cw) * 0.5 / a0; b1 = -(1.0 + cw) / a0; b2 = b0;
        a1 = -2.0 * cw / a0;        a2 = (1.0 - al) / a0;
    }
};

//==============================================================================
// Linkwitz-Riley 4th order crossover. lo + hi == all-pass (flat magnitude).
struct LR4
{
    Biquad lp[2], hp[2];
    double freq = -1.0;

    void reset() noexcept { for (auto& b : lp) b.reset(); for (auto& b : hp) b.reset(); }

    void set (double sr, double f) noexcept
    {
        if (std::abs (f - freq) < 1.0e-6) return;
        freq = f;
        for (int i = 0; i < 2; ++i) { lp[i].setLowpass (sr, f); hp[i].setHighpass (sr, f); }
    }

    inline void split (int ch, double x, double& lo, double& hi) noexcept
    {
        lo = lp[1].process (ch, lp[0].process (ch, x));
        hi = hp[1].process (ch, hp[0].process (ch, x));
    }

    // Phase-matches a signal to the phase shift this crossover introduces.
    inline double allpass (int ch, double x) noexcept
    {
        double lo, hi;
        split (ch, x, lo, hi);
        return lo + hi;
    }
};
} // namespace RS
