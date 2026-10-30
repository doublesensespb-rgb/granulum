#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// "Deep petrol" theme
namespace Palette
{
    const juce::Colour bgTop     { 0xff1f5160 };
    const juce::Colour bgBottom  { 0xff183f4c };
    const juce::Colour panel     { 0xff163e4b };
    const juce::Colour card      { 0xff1a4655 };
    const juce::Colour screen    { 0xff0b232c };
    const juce::Colour field     { 0xff123542 };
    const juce::Colour button    { 0xff35626f };
    const juce::Colour buttonHi  { 0xff467785 };
    const juce::Colour text      { 0xffdce8ea };
    const juce::Colour textDim   { 0xff9fbcc2 };
    const juce::Colour label     { 0xffc5d6da };
    const juce::Colour knob      { 0xffa3b1b5 };
    const juce::Colour knobDark  { 0xff7f9095 };
    const juce::Colour track     { 0xff11303a };
    const juce::Colour accent    { 0xff4fd6d9 };
    const juce::Colour accentSoft{ 0xff83c8cc };
    const juce::Colour violet    { 0xffb65cff };
    const juce::Colour violetHi  { 0xffd9a6ff };
    const juce::Colour violetDeep{ 0xff6f34e0 };
    const juce::Colour hot       { 0xffffc857 };
    const juce::Colour subBlue   { 0xff5f84ff };
    const juce::Colour purpleOff { 0xff3e2257 };
    const juce::Colour ledOff    { 0xff6d868c };
    const juce::Colour whiteKey  { 0xffe9f0ee };
    const juce::Colour blackKey  { 0xff5f8d97 };

    inline juce::Colour lfo (int i)   { return i == 0 ? juce::Colour (0xffc77dff) : juce::Colour (0xffffc857); }
    inline juce::Colour stem (int s)  { return s == 0 ? subBlue : s == 1 ? accent : violetHi; }
}

juce::Font uiFont (float size, bool bold = false, float kerning = 0.0f);
void drawLed (juce::Graphics&, juce::Rectangle<float>, juce::Colour on, juce::Colour off, float level);

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
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

class Knob;

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

    void refreshMods();
    void tick();

    juce::Slider slider;
    const int target;
    const juce::String paramId;
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

// Bright violet "dice" button that begs to be pressed
class DiceButton final : public juce::Button, private juce::Timer
{
public:
    explicit DiceButton (const juce::String& text = {});
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void clicked() override;
private:
    void timerCallback() override;
    juce::String label;
    float phase = 0.0f, flash = 0.0f, spin = 0.0f;
};

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

class Organism;
class LfoEditor;

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

    void randomizeSection (int section);   // 0 bars, 1 grain, 2 filter, 3 dirt, 4 space
    void randomizeAll();

private:
    void timerCallback() override;
    void openFile (const juce::File&);
    void launchChooser();
    void selectLfo (int i);
    void drawCard (juce::Graphics&, juce::Rectangle<int>, const juce::String& title, juce::Colour tag);

    GranulumProcessor& proc;
    GranulumLook look;

    std::unique_ptr<Organism> organism;
    std::unique_ptr<LfoEditor> lfoEditor;

    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    DiceButton diceAll { "RANDOM ALL" }, diceBars, diceGrain, diceFilter, diceDirt, diceSpace;

    // source toggles per block
    juce::TextButton grainAddT { "ADD" }, grainSmpT { "SMP" };
    juce::TextButton filterT[3] { juce::TextButton ("SUB"), juce::TextButton ("ADD"), juce::TextButton ("SMP") };
    juce::TextButton dirtT[3]   { juce::TextButton ("SUB"), juce::TextButton ("ADD"), juce::TextButton ("SMP") };
    juce::TextButton spaceT[3]  { juce::TextButton ("SUB"), juce::TextButton ("ADD"), juce::TextButton ("SMP") };
    std::vector<std::unique_ptr<ButtonAtt>> toggleAtts;

    juce::TextButton loadButton { "LOAD" }, rootDown { "<" }, rootUp { ">" };
    juce::TextButton polyButton { "POLY" }, monoButton { "MONO" };
    juce::Label sampleName, rootLabel;
    juce::ComboBox filterType, dirtType;
    std::unique_ptr<ComboAtt> filterTypeA, dirtTypeA;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> subKnobs, addKnobs, smpKnobs, grainKnobs, filterKnobs, dirtKnobs, spaceKnobs, voiceKnobs;
    std::unique_ptr<Knob> outputKnob;
    std::vector<Knob*> allKnobs;

    // LFO strip
    struct LfoControls
    {
        juce::ComboBox div, mode;
        juce::TextButton sync { "SYNC" };
        std::unique_ptr<Knob> rate;
        std::unique_ptr<ComboAtt> divA, modeA;
        std::unique_ptr<ButtonAtt> syncA;
    };
    std::array<std::unique_ptr<LfoTab>, kNumLfos> lfoTabs;
    std::array<std::unique_ptr<LfoControls>, kNumLfos> lfoControls;
    juce::ComboBox lfoPreset, lfoGrid;
    int selectedLfo = 0;

    // random glides
    struct Glide { juce::RangedAudioParameter* param; float from, to; };
    std::vector<Glide> glides;
    std::array<float, granulum::kNumHarmonics> barsFrom {}, barsTo {};
    bool barsGliding = false;
    float glideT = 1.0f;

    juce::Rectangle<int> titleArea, gemsArea, screenArea, subCard, addCard, smpCard,
                         grainCard, filterCard, dirtCard, spaceCard, lfoCard, voiceCard;
    juce::Rectangle<int> filterTypeLabel, dirtTypeLabel, lfoPresetLabel, lfoGridLabel, lfoDivLabel, lfoModeLabel, rootTitle;
    float meter = 0.0f;
    uint32_t shownModVersion = 0;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
