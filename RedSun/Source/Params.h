#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace RS
{
constexpr int numBands  = 4;
constexpr int numStyles = 11;

inline const juce::StringArray& styleNames()
{
    static const juce::StringArray n { "Warm Tube", "Clean Tube", "Hot Tube", "Tape", "Soft Clip", "Hard Clip",
                                       "Diode", "Fuzz", "Foldback", "Rectifier", "Crunch" };
    return n;
}

inline juce::String bandId (const char* base, int band) { return juce::String (base) + juce::String (band + 1); }

// Keeps crossover points ordered and inside the audible / Nyquist range. Shared by DSP and GUI.
inline void orderCrossovers (double& x1, double& x2, double& x3, double sr) noexcept
{
    const double maxF = 0.46 * sr;
    x1 = juce::jlimit (30.0, maxF / 1.5625, x1);
    x2 = juce::jlimit (x1 * 1.25, maxF / 1.25, x2);
    x3 = juce::jlimit (x2 * 1.25, maxF, x3);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    using Fmt = std::function<String (float, int)>;
    const Fmt dbF  = [] (float v, int) { return String (v, 1) + " dB"; };
    const Fmt pctF = [] (float v, int) { return String ((int) std::round (v)) + " %"; };
    const Fmt bipF = [] (float v, int) { const int i = (int) std::round (v); return (i > 0 ? "+" : "") + String (i) + " %"; };
    const Fmt hzF  = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; };
    const Fmt msF  = [] (float v, int) { return v < 10.0f ? String (v, 1) + " ms" : String ((int) std::round (v)) + " ms"; };

    auto addF = [&] (const String& id, const String& name, NormalisableRange<float> r, float def, const Fmt& f)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (f)));
    };
    auto addB = [&] (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // ---- Master
    addF ("inGain",      "Input",        { -24.0f, 24.0f, 0.01f }, 0.0f,   dbF);
    addF ("outGain",     "Output",       { -24.0f, 24.0f, 0.01f }, 0.0f,   dbF);
    addF ("masterDrive", "Master Drive", { 0.0f, 100.0f, 0.1f },   0.0f,   pctF);
    addF ("masterMix",   "Master Mix",   { 0.0f, 100.0f, 0.1f },   100.0f, pctF);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "oversample", 1 }, "Oversampling",
                                                        StringArray { "Off", "2x", "4x" }, 1));

    // ---- Crossovers
    addF ("xover1", "Crossover 1", { 30.0f, 16000.0f, 0.0f, 0.30f },  150.0f, hzF);
    addF ("xover2", "Crossover 2", { 30.0f, 16000.0f, 0.0f, 0.30f }, 1200.0f, hzF);
    addF ("xover3", "Crossover 3", { 30.0f, 16000.0f, 0.0f, 0.30f }, 6000.0f, hzF);

    // ---- Bands
    const int defStyle[numBands] = { 0, 3, 2, 1 };
    for (int b = 0; b < numBands; ++b)
    {
        const String n = "Band " + String (b + 1) + " ";
        addF (bandId ("drive", b), n + "Drive", { 0.0f, 100.0f, 0.1f },    b == 0 ? 30.0f : 25.0f, pctF);
        addF (bandId ("level", b), n + "Level", { -24.0f, 12.0f, 0.01f },  0.0f,   dbF);
        addF (bandId ("mix",   b), n + "Mix",   { 0.0f, 100.0f, 0.1f },    100.0f, pctF);
        addF (bandId ("tone",  b), n + "Tone",  { -100.0f, 100.0f, 0.1f }, 0.0f,   bipF);
        addF (bandId ("dyn",   b), n + "Dynamics", { -100.0f, 100.0f, 0.1f }, 0.0f, bipF);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { bandId ("style", b), 1 }, n + "Style",
                                                            styleNames(), defStyle[b]));
        addB (bandId ("mute", b), n + "Mute", false);
        addB (bandId ("solo", b), n + "Solo", false);
    }

    // ---- Sidechain Savior
    addB ("scOn", "Sidechain Savior", false);
    addF ("scLo",      "SC Low",       { 20.0f, 20000.0f, 0.0f, 0.30f }, 120.0f, hzF);
    addF ("scHi",      "SC High",      { 20.0f, 20000.0f, 0.0f, 0.30f }, 800.0f, hzF);
    addF ("scDepth",   "SC Depth",     { 0.0f, 30.0f, 0.1f },            12.0f,  dbF);
    addF ("scThresh",  "SC Threshold", { -60.0f, 0.0f, 0.1f },           -30.0f, dbF);
    addF ("scAttack",  "SC Attack",    { 0.1f, 100.0f, 0.0f, 0.4f },     5.0f,   msF);
    addF ("scRelease", "SC Release",   { 5.0f, 1000.0f, 0.0f, 0.4f },    120.0f, msF);

    return layout;
}
} // namespace RS
