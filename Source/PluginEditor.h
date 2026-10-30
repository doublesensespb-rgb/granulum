#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// "Dusty synthwave hardware" theme
namespace Palette
{
    const juce::Colour panel     { 0xff1c1d1f };   // charcoal metal
    const juce::Colour panelDark { 0xff121315 };
    const juce::Colour cream     { 0xffe9ddd0 };
    const juce::Colour teal      { 0xff6cc4ad };
    const juce::Colour tealDim   { 0xff3f7f72 };
    const juce::Colour screen    { 0xff0a1414 };
    const juce::Colour grid      { 0xff3fbfae };
    const juce::Colour sand      { 0xffc9b48c };
    const juce::Colour pink      { 0xffff7a1a };   // main accent: juicy orange
    const juce::Colour pinkInk   { 0xff4a1d00 };

    const juce::Colour yellow    { 0xffe6c84a };
    const juce::Colour magenta   { 0xffc552c8 };
    const juce::Colour green     { 0xff79c76f };
    const juce::Colour red       { 0xffd8544f };
    const juce::Colour blue      { 0xff5f84d8 };
    const juce::Colour aqua      { 0xff63c7b4 };
}

class GranulumLook final : public juce::LookAndFeel_V4
{
public:
    GranulumLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // granite / speckle overlay used for gem knobs and the pink block
    static const juce::Image& speckle();
    static void drawGem (juce::Graphics&, juce::Rectangle<float> circle, juce::Colour base, float glow);
};

class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title, juce::Colour accent, bool hero = false);
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
    void resized() override;
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
    juce::Image glass; // grid + scanlines + vignette, cached
};

// Big pink "active sample" block — click to load a sample
class SampleBlock final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SampleBlock();
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown  (const juce::MouseEvent&) override { if (onClick) onClick(); }

    void setText (const juce::String& top, const juce::String& main);
    std::function<void()> onClick;
private:
    juce::String topText, mainText;
    bool hover = false;
};

class GranulumEditor final : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    explicit GranulumEditor (GranulumProcessor&);
    ~GranulumEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void openFile (const juce::File&);
    void launchChooser();
    void rebuildBackground();

    GranulumProcessor& proc;
    GranulumLook look;

    WaveformView waveform;
    SampleBlock sampleBlock;
    juce::TextButton randomButton { "RANDOM ALL" }, loadButton { "LOAD" };
    juce::TextButton rndGrains { "RANDOM" }, rndPitch { "RANDOM" }, rndFx { "RANDOM" };
    juce::Label sampleName;
    juce::ComboBox filterType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<std::unique_ptr<Knob>> heroKnobs, motionKnobs, pitchKnobs, fxKnobs;
    juce::TextButton transButton { "TRANSIENTS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> transAttachment;

    juce::Rectangle<int> titleArea, gemsArea, screenFrame, grainArea, pitchArea, fxArea, keysFrame, ledArea;
    juce::Image background;       // cached textured panel
    juce::Image sandOverlay;      // dust drawn over everything (cached)
    float meter = 0.0f;
    float activity = 0.0f;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
