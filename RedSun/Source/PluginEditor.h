#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "SunLookAndFeel.h"
#include "DisplayComponent.h"

class RedSunAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit RedSunAudioProcessorEditor (RedSunAudioProcessor&);
    ~RedSunAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    juce::Image renderStaticLayer();
    void addKnob (juce::Slider&, const juce::String& paramId);
    juce::String bandRangeText (int band) const;

    static constexpr int W = 1180, H = 800;

    RedSunAudioProcessor& proc;
    RS::SunLookAndFeel laf;                 // must outlive every component that uses it
    juce::Image staticLayer;
    int selectedBand = 0;

    const juce::Rectangle<int> headerR { 0, 0, W, 84 };
    const juce::Rectangle<int> displayR { 20, 94, 800, 380 };
    const juce::Rectangle<int> masterR { 840, 94, 320, 130 };
    const juce::Rectangle<int> scR { 840, 234, 320, 240 };
    juce::Rectangle<int> bandR[4];
    const juce::Rectangle<int> meterR { 330, 22, 330, 44 };
    const juce::Rectangle<int> scStatusR { 850, 282, 300, 16 };

    struct BandUI
    {
        RS::Knob drive { true };
        RS::Knob level, mix, tone, dyn;
        juce::ComboBox style;
        juce::TextButton mute { "M" }, solo { "S" };
    };
    std::array<BandUI, 4> bands;

    RS::Knob inKnob, driveKnob { true }, outKnob, mixKnob;
    juce::ComboBox osBox;
    RS::SaviorButton saviorBtn;
    RS::Knob scLoK, scHiK, scDepthK, scThreshK, scAttK, scRelK;
    RS::DisplayComponent display;

    float meterInDb = -90.0f, meterOutDb = -90.0f;
    juce::String lastRange[4];
    bool lastScOn = false;

    // attachments last so they are destroyed before the components they point at
    std::vector<std::unique_ptr<SA>> sAtt;
    std::vector<std::unique_ptr<BA>> bAtt;
    std::vector<std::unique_ptr<CA>> cAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedSunAudioProcessorEditor)
};
