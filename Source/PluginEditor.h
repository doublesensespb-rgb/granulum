#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Colour theme. Theme 0 is the shipping teal; 1-4 are design variants previewed by the snapshot tool.
namespace Palette
{
    inline juce::Colour bgTop;
    inline juce::Colour bgBottom;
    inline juce::Colour panel;
    inline juce::Colour screen;
    inline juce::Colour deep;
    inline juce::Colour field;
    inline juce::Colour button;
    inline juce::Colour buttonHi;
    inline juce::Colour text;
    inline juce::Colour textDim;
    inline juce::Colour label;
    inline juce::Colour knob;
    inline juce::Colour knobDark;
    inline juce::Colour track;
    inline juce::Colour accent;
    inline juce::Colour accentSoft;
    inline juce::Colour ice;
    inline juce::Colour ledOff;
    inline juce::Colour whiteKey;
    inline juce::Colour blackKey;
    inline juce::Colour hit { 0xffffc857 };

    inline void applyTheme (int t)
    {
        static const juce::uint32 themes[][20] =
        {
            { 0xff1f5160, 0xff1a4654, 0xff163e4b, 0xff0f2d37, 0xff081c23, 0xff15394a, 0xff3a6a78, 0xff4a7d8b, 0xffdce8ea, 0xff9fbcc2, 0xffc5d6da, 0xffa3b1b5, 0xff7f9095, 0xff13323d, 0xff4fd6d9, 0xff83c8cc, 0xffd8fbff, 0xff7d9399, 0xffe9f0ee, 0xff5f8d97 },
            { 0xff2c4a61, 0xff263f53, 0xff213a4d, 0xff172b3a, 0xff0c1823, 0xff1d3446, 0xff3f6585, 0xff4d7596, 0xfff7f8f0, 0xffa9c2d6, 0xffcddbe6, 0xffb8c6d2, 0xff8b9cab, 0xff182e3e, 0xff9cd5ff, 0xff7aaace, 0xfff7f8f0, 0xff7d93a6, 0xfff7f8f0, 0xff55799a },
            { 0xff355872, 0xff2f4f67, 0xff2a475d, 0xff1d3346, 0xff10202d, 0xff284459, 0xff5a87ab, 0xff6896ba, 0xfff7f8f0, 0xffc4d9e8, 0xffdde8f0, 0xffd9dee0, 0xffa5b0b8, 0xff213a4d, 0xff9cd5ff, 0xff7aaace, 0xfff7f8f0, 0xff8aa3b8, 0xfff7f8f0, 0xff5a7f9e },
            { 0xfff7f8f0, 0xffeceee5, 0xffe2e9ec, 0xff2b4a63, 0xff172b3a, 0xffd4e0e8, 0xff7aaace, 0xff8cb8d8, 0xff1f3a50, 0xff4e6a80, 0xff355872, 0xffc3ccd2, 0xff98a5ae, 0xffc9d7e2, 0xff5f9bcc, 0xff9cd5ff, 0xfff7f8f0, 0xff9aaebd, 0xfff7f8f0, 0xff7aaace },
            { 0xff7aaace, 0xff6c9cc1, 0xff355872, 0xff24405a, 0xff142636, 0xff2d4c63, 0xff4b7598, 0xff5b86aa, 0xfff7f8f0, 0xffc8dcec, 0xffe6eef4, 0xffe2e6e2, 0xffaab4bb, 0xff1f384b, 0xff9cd5ff, 0xff7aaace, 0xfff7f8f0, 0xff8aa3b8, 0xfff7f8f0, 0xff55799a },
        };
        const auto& c = themes[juce::jlimit (0, (int) std::size (themes) - 1, t)];
        juce::Colour* dst[] = { &bgTop, &bgBottom, &panel, &screen, &deep, &field, &button, &buttonHi, &text, &textDim, &label, &knob, &knobDark, &track, &accent, &accentSoft, &ice, &ledOff, &whiteKey, &blackKey };
        for (size_t i = 0; i < std::size (dst); ++i) *dst[i] = juce::Colour (c[i]);
    }
    inline const bool themeReady = (applyTheme (0), true);

    inline juce::Colour lfo (int i) { return i == 0 ? juce::Colour (0xffffc857) : juce::Colour (0xffbff3ff); }
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
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    int getCallOutBoxBorderSize (const juce::CallOutBox&) override { return 16; }
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

    void refreshMods();
    void tick();

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

// The central screen: glowing waveform + crystalline shards born from every grain
class GrainField final : public juce::Component, private juce::Timer
{
public:
    explicit GrainField (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool dragHover = false;
    void simulate (float dt);   // public so the snapshot tool can advance time

private:
    void timerCallback() override;
    void setPositionFromMouse (const juce::MouseEvent&);
    juce::Rectangle<float> inner() const;

    struct Shard
    {
        float x = 0, y = 0, vx = 0, vy = 0;   // x 0..1, y -1..1
        float age = 0, life = 1, len = 0.03f, width = 0.006f, spin = 0, angle = 0, tint = 0, size = 1;
        uint8_t kind = 0;
        std::array<juce::Point<float>, 7> trail {};
        int trailN = 0;
    };
    struct Mote { float x, y, vx, vy, phase, size; };

    GranulumProcessor& proc;
    std::vector<Shard> shards;
    std::vector<Mote> motes;
    std::vector<float> heat;              // per column glow where grains are read
    std::vector<float> env;               // smoothed waveform envelope per column
    std::shared_ptr<const WaveformPeaks> lastPeaks;
    std::vector<float> heads;
    uint32_t lastEvent[2] { 0, 0 };
    juce::Random rng;
    float time = 0.0f, pulse = 0.0f;
    double lastTick = 0.0;
};

// Additive bars: draw the 16 harmonic amplitudes with the mouse
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

// Chord / interval picker that pops out of the OSC 1 header
class ChordPicker final : public juce::Component
{
public:
    explicit ChordPicker (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void choose (int index);
    void refresh();
    GranulumProcessor& proc;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::Rectangle<int> chordLabel, intervalLabel, keysArea, captionArea;
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

// 4-point drawable LFO envelope: drag points, drag the round handles on each segment to bend it
class LfoEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit LfoEditor (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void setLfo (int l);
    int lfo = 0;
private:
    void timerCallback() override;
    juce::Rectangle<float> area() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> handlePos (int seg) const;
    int hitTest (juce::Point<float>, bool& isHandle) const;

    GranulumProcessor& proc;
    std::array<LfoPoint, kLfoPoints> pts {};
    int dragIndex = -1, hoverIndex = -1;
    bool dragHandle = false, hoverHandle = false;
    float dragStartCurve = 0.0f;
    juce::Point<float> dragStart;
};

// Block on/off: LED + title, click toggles
class Lamp final : public juce::Button
{
public:
    explicit Lamp (const juce::String& name, const juce::String& subtitle = {})
        : juce::Button (name), sub (subtitle) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String sub;
    float fontSize = 14.5f;
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

    GrainField& grainField() { return field; }
    GranulumLook& lookAndFeel() { return look; }

private:
    void timerCallback() override;
    void openFile (const juce::File&);
    void launchChooser();
    void selectLfo (int i);
    void showChordPicker();

    GranulumProcessor& proc;
    GranulumLook look;

    GrainField field;
    HarmonicEditor harmonics;

    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    Lamp osc1Lamp { "OSC 1", "ADD" }, osc2Lamp { "OSC 2", "SAMPLE" };
    Lamp grainLamp { "GRANULAR" }, filterLamp { "FILTER" }, distLamp { "DISTORTION" }, revLamp { "REVERB" };
    std::unique_ptr<ButtonAtt> osc1LampA, osc2LampA, grainLampA, filterLampA, distLampA, revLampA;

    juce::TextButton initButton { "INIT" }, randomButton { "RANDOM ALL" }, loadButton { "LOAD" }, chordButton { "CHORD: OFF" };
    juce::TextButton rndGrains { "RANDOM" }, rndFilter { "RANDOM" }, rndFx { "RANDOM" }, rndBars { "RANDOM" };
    juce::TextButton osc1Grain { "GRAIN" }, osc2Grain { "GRAIN" };
    juce::TextButton polyButton { "POLY" }, monoButton { "MONO" };
    std::unique_ptr<ButtonAtt> osc1GrainA, osc2GrainA;
    juce::ComboBox filterType, distType;
    std::unique_ptr<ComboAtt> filterTypeA, distTypeA;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> heroKnobs, smallKnobs, osc1Knobs, osc2Knobs, filterKnobs, distKnobs, revKnobs, globalKnobs;
    std::vector<Knob*> allKnobs;

    struct LfoControls
    {
        juce::ComboBox preset, div, mode;
        juce::TextButton sync { "SYNC" };
        std::unique_ptr<Knob> rate, rise;
        std::unique_ptr<ComboAtt> divA, modeA;
        std::unique_ptr<ButtonAtt> syncA;
    };
    std::array<std::unique_ptr<LfoTab>, kNumLfos> lfoTabs;
    std::array<std::unique_ptr<LfoControls>, kNumLfos> lfoControls;
    LfoEditor lfoEditor;
    int selectedLfo = 0;

    juce::Rectangle<int> titleArea, gemsArea, screenFrame, osc1Area, osc2Area, osc1Bars, dropZone,
                         grainArea, filterArea, fxArea, ledArea, lfoFrame, voiceArea,
                         lfoPresetLabel, lfoModeLabel, lfoDivLabel, distTypeLabel, fxLabel;
    float meter = 0.0f;
    float activity = 0.0f;
    uint32_t shownModVersion = 0;
    juce::String statusText;
    bool fileHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
