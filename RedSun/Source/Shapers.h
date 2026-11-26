#pragma once
#include <cmath>
#include <algorithm>

namespace RS
{
inline float sgn (float x) noexcept { return x < 0.0f ? -1.0f : 1.0f; }

// 11 distortion styles. Input is already pre-gained by the drive stage.
inline float shape (int style, float x) noexcept
{
    switch (style)
    {
        case 0:  return std::tanh (x + 0.2f) - 0.19738f;                              // Warm Tube
        case 1:  return x >= 0.0f ? x / (1.0f + x) : x / (1.0f - 0.6f * x);           // Clean Tube
        case 2:  return std::tanh (2.0f * x + 0.3f) - 0.29131f;                       // Hot Tube
        case 3:  return 0.63662f * std::atan (1.5708f * x);                           // Tape
        case 4: { const float c = std::max (-1.0f, std::min (1.0f, x));               // Soft Clip
                  return 1.5f * (c - c * c * c / 3.0f); }
        case 5:  return std::max (-1.0f, std::min (1.0f, x));                         // Hard Clip
        case 6:  return x >= 0.0f ? 1.0f - std::exp (-x)                               // Diode
                                  : -(1.0f - std::exp (0.7f * x)) / 0.7f;
        case 7: { const float a = std::fabs (x);                                       // Fuzz
                  return 0.7f * std::tanh (6.0f * x) + 0.3f * sgn (x) * (1.0f - std::exp (-2.0f * a)); }
        case 8:  return std::sin (x);                                                  // Foldback
        case 9:  return 0.75f * std::tanh (x) + 0.25f * std::tanh (std::fabs (x));    // Rectifier
        case 10: return std::round (std::tanh (x) * 8.0f) / 8.0f;                      // Crunch
        default: return x;
    }
}
} // namespace RS
