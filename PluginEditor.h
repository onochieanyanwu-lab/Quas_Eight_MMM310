#pragma once
#include "PluginProcessor.h"

class Q8Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Q8Editor (Q8Processor&);
    ~Q8Editor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob  { juce::Slider s; juce::Label l; std::unique_ptr<SA> a; };
    struct Combo { juce::ComboBox c; juce::Label l; std::unique_ptr<CA> a; };
    struct Toggle { juce::ToggleButton b; std::unique_ptr<BA> a; };

    Knob& addKnob (const juce::String& id, const juce::String& name);
    Combo& addCombo (const juce::String& id, const juce::String& name, const juce::StringArray& items);
    Toggle& addToggle (const juce::String& id, const juce::String& name);
    void timerCallback() override { repaint (grBar); }

    Q8Processor& proc;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    std::vector<std::unique_ptr<Toggle>> toggles;
    Knob *inGain, *xfmr, *lfG, *mfG, *hfG, *thr, *att, *rel, *makeup, *mix, *outGain;
    Combo *hpf, *lfF, *mfF, *hfF, *ratio;
    Toggle *preOn, *eqOn, *compOn;
    juce::Rectangle<int> secPre, secHpf, secEq, secComp, secOut, grBar;
};
