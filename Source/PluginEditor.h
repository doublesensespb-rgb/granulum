#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour bg        { 0xff0d0f14 };
    const juce::Colour panel     { 0xff151821 };
    const juce::Colour panelEdge { 0xff232838 };
    const juce::Colour text      { 0xffe6e9f2 };
    const juce::Colour dim       { 0xff7d8499 };
    const juce::Colour mint      { 0xff6ff2d0 };
    const juce::Colour violet    { 0xffa98bff };
    const juce::Colour pink      { 0xffff7ab8 };
    const juce::Colour wave      { 0xff3b4460 };
}

class GranulumLook final : public juce::LookAndFeel_V4
{
public:
    GranulumLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, juce::Colour accent);
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class WaveformView final : public juce::Component, private juce::Timer
{
public:
    explicit WaveformView (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool dragHover = false;
private:
    void timerCallback() override;
    void setPositionFromMouse (const juce::MouseEvent&);

    GranulumProcessor& proc;
    std::vector<float> grainX, grainA;
    std::vector<float> heads;
    float phase = 0.0f;
};

class GranulumEditor final : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    explicit GranulumEditor (GranulumProcessor&);
    ~GranulumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void openFile (const juce::File&);
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour);

    GranulumProcessor& proc;
    GranulumLook look;

    WaveformView waveform;
    juce::TextButton loadButton { "LOAD" }, randomButton { "RANDOM" };
    juce::Label sampleLabel;
    juce::ComboBox filterType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<std::unique_ptr<Knob>> grainKnobs, pitchKnobs, shapeKnobs, fxKnobs;
    juce::Rectangle<int> grainArea, pitchArea, shapeArea, fxArea, meterArea;
    float meter = 0.0f;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
