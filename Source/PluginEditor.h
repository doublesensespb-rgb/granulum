#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// "Deep petrol" flat theme
namespace Palette
{
    const juce::Colour bgTop     { 0xff1f5160 };
    const juce::Colour bgBottom  { 0xff1a4654 };
    const juce::Colour panel     { 0xff163e4b };
    const juce::Colour screen    { 0xff0f2d37 };
    const juce::Colour field     { 0xff15394a };
    const juce::Colour button    { 0xff3a6a78 };
    const juce::Colour buttonHi  { 0xff4a7d8b };
    const juce::Colour text      { 0xffdce8ea };
    const juce::Colour textDim   { 0xff9fbcc2 };
    const juce::Colour label     { 0xffc5d6da };
    const juce::Colour knob      { 0xffa3b1b5 };
    const juce::Colour knobDark  { 0xff7f9095 };
    const juce::Colour track     { 0xff13323d };
    const juce::Colour accent    { 0xff4fd6d9 };
    const juce::Colour accentSoft{ 0xff83c8cc };
    const juce::Colour hit       { 0xffffc857 };
    const juce::Colour purple    { 0xffb04fd8 };
    const juce::Colour purpleOff { 0xff4a2357 };
    const juce::Colour ledOff    { 0xff7d9399 };
    const juce::Colour whiteKey  { 0xffe9f0ee };
    const juce::Colour blackKey  { 0xff5f8d97 };
}

class GranulumLook final : public juce::LookAndFeel_V4
{
public:
    GranulumLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, bool hero = false);
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;
private:
    juce::String title;
    bool hero = false;
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
    std::vector<int> grainHit;
    std::vector<float> heads;
    float phase = 0.0f;
};

// Additive "sticks": draw the harmonic amplitudes with the mouse
class HarmonicEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit HarmonicEditor (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> barArea() const;
    int barAt (float x) const;
    void applyMouse (const juce::MouseEvent&, bool first);

    GranulumProcessor& proc;
    std::array<float, granulum::kNumHarmonics> h {};
    int lastBar = -1;
    float lastValue = 0.0f;
    bool erasing = false;
    bool editing = false;
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
    void launchChooser();
    void setSource (int s);
    void drawSectionHeader (juce::Graphics&, juce::Rectangle<int> area, const juce::String& title, bool lit);

    GranulumProcessor& proc;
    GranulumLook look;

    WaveformView waveform;
    HarmonicEditor harmonics;

    juce::TextButton randomButton { "RANDOM ALL" }, loadButton { "LOAD" };
    juce::TextButton rndGrains { "RANDOM" }, rndBass { "RANDOM" }, rndFx { "RANDOM" }, rndBars { "RANDOM" };
    juce::TextButton srcAdd { "ADD" }, srcSample { "SAMPLE" }, srcBoth { "BOTH" };
    juce::TextButton transButton { "TRANSIENTS" }, keySyncButton { "KEY SYNC" }, monoButton { "MONO" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> transAttachment, keySyncAttachment, monoAttachment;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    juce::Label sampleName;
    juce::ComboBox filterType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 700 };

    std::vector<std::unique_ptr<Knob>> heroKnobs, motionKnobs, bassKnobs, fxKnobs;

    juce::Rectangle<int> titleArea, gemsArea, screenFrame, sourceArea, mixLabelArea, grainArea, bassArea, fxArea, ledArea;
    float meter = 0.0f;
    float activity = 0.0f;
    int shownSource = -1;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
