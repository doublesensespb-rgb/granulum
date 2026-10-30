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

    inline juce::Colour lfo (int i) { return i == 0 ? juce::Colour (0xffc77dff) : juce::Colour (0xffffc857); }
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

class Knob;

// Small coloured circle next to a knob: drag up/down = modulation depth, double-click = remove
class ModChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModChip (GranulumProcessor&, Knob& owner, int lfo);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void updateTooltip();

    const int lfo;
private:
    GranulumProcessor& proc;
    Knob& owner;
    float startDepth = 0.0f;
};

class Knob final : public juce::Component, public juce::DragAndDropTarget
{
public:
    enum class Size { Small, Normal, Hero };

    Knob (GranulumProcessor&, const juce::String& paramId, const juce::String& title, Size size = Size::Normal);
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void refreshMods();     // after the modulation list changed
    void tick();            // animate the modulated value
    bool hasMods() const { return anyMod; }

    juce::Slider slider;
    const int target;       // modulation target index, -1 if not modulatable
private:
    struct Dial { juce::Point<float> c; float r = 0, track = 0, start = 0, end = 0; };
    Dial dial() const;

    GranulumProcessor& proc;
    juce::String title;
    Size size;
    bool dragHover = false, anyMod = false;
    std::array<std::unique_ptr<ModChip>, kNumLfos> chips;
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
    bool erasing = false, editing = false;
};

// LFO tab: click = select, drag onto any knob = modulate it
class LfoTab final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoTab (GranulumProcessor&, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (int)> onSelect;
    bool selected = false;
    const int index;
private:
    GranulumProcessor& proc;
    bool dragging = false;
};

class LfoDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit LfoDisplay (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    int lfo = 0;
private:
    void timerCallback() override { repaint(); }
    GranulumProcessor& proc;
};

// Block on/off: LED + title, click toggles
class Lamp final : public juce::Button
{
public:
    explicit Lamp (const juce::String& name) : juce::Button (name) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class GranulumEditor final : public juce::AudioProcessorEditor,
                             public juce::DragAndDropContainer,
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
    void selectLfo (int i);

    GranulumProcessor& proc;
    GranulumLook look;

    WaveformView waveform;
    HarmonicEditor harmonics;

    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // block lamps
    Lamp grainLamp { "GRANULAR" }, filterLamp { "FILTER" }, fxLamp { "EFFECTS" };
    std::unique_ptr<ButtonAtt> grainLampA, filterLampA, fxLampA;

    juce::TextButton randomButton { "RANDOM ALL" }, loadButton { "LOAD" };
    juce::TextButton rndGrains { "RANDOM" }, rndFilter { "RANDOM" }, rndFx { "RANDOM" }, rndBars { "RANDOM" };
    juce::TextButton osc1Grain { "GRAIN" }, osc2Grain { "GRAIN" };
    juce::TextButton transButton { "TRANSIENTS" }, keySyncButton { "KEY SYNC" };
    juce::TextButton polyButton { "POLY" }, monoButton { "MONO" }, alwaysButton { "ALWAYS" }, scaledButton { "SCALED" };
    std::unique_ptr<ButtonAtt> osc1GrainA, osc2GrainA, transA, keySyncA, alwaysA, scaledA;
    juce::Label sampleName;
    juce::ComboBox filterType, distType;
    std::unique_ptr<ComboAtt> filterTypeA, distTypeA;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> heroKnobs, motionKnobs, osc1Knobs, osc2Knobs, filterKnobs, distKnobs, revKnobs, globalKnobs;
    std::vector<Knob*> allKnobs;

    struct LfoControls
    {
        juce::ComboBox shape, div, mode;
        juce::TextButton sync { "SYNC" };
        std::unique_ptr<Knob> rate;
        std::unique_ptr<ComboAtt> shapeA, divA, modeA;
        std::unique_ptr<ButtonAtt> syncA;
    };
    std::array<std::unique_ptr<LfoTab>, kNumLfos> lfoTabs;
    std::array<std::unique_ptr<LfoControls>, kNumLfos> lfoControls;
    LfoDisplay lfoDisplay;
    int selectedLfo = 0;

    juce::Rectangle<int> titleArea, gemsArea, screenFrame, osc1Area, osc2Area, osc1Label, osc2Label,
                         grainArea, filterArea, fxArea, ledArea, lfoFrame, voiceArea,
                         lfoShapeLabel, lfoModeLabel, lfoDivLabel, distLabel, revLabel, distTypeLabel;
    float meter = 0.0f;
    float activity = 0.0f;
    uint32_t shownModVersion = 0;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
