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
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    juce::Image renderStaticLayer();
    void addKnob (juce::Slider&, const juce::String& paramId, const juce::String& name, const juce::String& tip);
    void placeLabel (juce::Slider&, int labelTop);
    juce::String bandRangeText (int band) const;

    static constexpr int W = 1180, H = 800;

    RedSunAudioProcessor& proc;
    RS::SunLookAndFeel laf;                 // must outlive every component that uses it
    juce::TooltipWindow tooltip { this, 600 };
    juce::Image staticLayer, frost;
    int selectedBand = 0;

    const juce::Rectangle<int> headerR  { 0, 0, W, 88 };
    const juce::Rectangle<int> displayR { 20, 98, 800, 376 };
    const juce::Rectangle<int> masterR  { 840, 98, 320, 130 };
    const juce::Rectangle<int> scR      { 840, 238, 320, 236 };
    juce::Rectangle<int> bandR[4];
    const juce::Rectangle<int> meterR   { 440, 22, 300, 44 };
    const juce::Rectangle<int> scStatusR { 852, 452, 296, 16 };

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

    // every knob gets a name + live value drawn underneath it
    struct KnobLabel
    {
        juce::Slider* slider = nullptr;
        juce::String name;
        int labelTop = 0;
        juce::String lastText;
        bool lastHover = false;
    };
    std::vector<KnobLabel> knobLabels;

    float meterInDb = -90.0f, meterOutDb = -90.0f;
    juce::String lastRange[4];
    bool lastScOn = false;

    // attachments last so they are destroyed before the components they point at
    std::vector<std::unique_ptr<SA>> sAtt;
    std::vector<std::unique_ptr<BA>> bAtt;
    std::vector<std::unique_ptr<CA>> cAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RedSunAudioProcessorEditor)
};
