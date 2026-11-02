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
    inline juce::Colour hit { 0xffd6be9c };   // muted sand / beige
    // skin extras (themes 5+): strong ink, colour of drawings on screens, knob outline, font, LED style
    inline juce::Colour hi { 0xffffffff };
    inline juce::Colour onScreen { 0xffffffff };
    inline juce::Colour knobRing { 0x00000000 };
    inline juce::Colour edge { 0x0fffffff };   // thin outline of buttons/boxes
    inline juce::Colour lfoCol[3] { juce::Colour (0xffbff3ff), juce::Colour (0xff9ee6c4), juce::Colour (0xffd6be9c) };
    inline juce::String fontName;
    inline bool flatLed = false;
    inline int theme = 0;

    inline void applyTheme (int t)
    {
        static const juce::uint32 themes[][20] =
        {
            { 0xff1f5160, 0xff1a4654, 0xff163e4b, 0xff0f2d37, 0xff081c23, 0xff15394a, 0xff3a6a78, 0xff4a7d8b, 0xffdce8ea, 0xff9fbcc2, 0xffc5d6da, 0xffa3b1b5, 0xff7f9095, 0xff13323d, 0xff4fd6d9, 0xff83c8cc, 0xffd8fbff, 0xff7d9399, 0xffe9f0ee, 0xff5f8d97 },
            { 0xff2c4a61, 0xff263f53, 0xff213a4d, 0xff172b3a, 0xff0c1823, 0xff1d3446, 0xff3f6585, 0xff4d7596, 0xfff7f8f0, 0xffa9c2d6, 0xffcddbe6, 0xffb8c6d2, 0xff8b9cab, 0xff182e3e, 0xff9cd5ff, 0xff7aaace, 0xfff7f8f0, 0xff7d93a6, 0xfff7f8f0, 0xff55799a },
            { 0xff355872, 0xff2f4f67, 0xff2a475d, 0xff1d3346, 0xff10202d, 0xff284459, 0xff5a87ab, 0xff6896ba, 0xfff7f8f0, 0xffc4d9e8, 0xffdde8f0, 0xffd9dee0, 0xffa5b0b8, 0xff213a4d, 0xff9cd5ff, 0xff7aaace, 0xfff7f8f0, 0xff8aa3b8, 0xfff7f8f0, 0xff5a7f9e },
            { 0xfff7f8f0, 0xffeceee5, 0xffe2e9ec, 0xff2b4a63, 0xff172b3a, 0xffd4e0e8, 0xff7aaace, 0xff8cb8d8, 0xff1f3a50, 0xff4e6a80, 0xff355872, 0xffc3ccd2, 0xff98a5ae, 0xffc9d7e2, 0xff5f9bcc, 0xff9cd5ff, 0xfff7f8f0, 0xff9aaebd, 0xfff7f8f0, 0xff7aaace },
            { 0xff7aaace, 0xff6c9cc1, 0xff355872, 0xff24405a, 0xff142636, 0xff2d4c63, 0xff4b7598, 0xff5b86aa, 0xfff7f8f0, 0xffc8dcec, 0xffe6eef4, 0xffe2e6e2, 0xffaab4bb, 0xff1f384b, 0xff9cd5ff, 0xff7aaace, 0xfff7f8f0, 0xff8aa3b8, 0xfff7f8f0, 0xff55799a },
            // 5 TE LIGHT: warm light grey body, black ink, black screens, one orange accent
            { 0xffe3e2de, 0xffdad9d4, 0xffeceae6, 0xff121212, 0xff0a0a0a, 0xffdcdad5, 0xffd2d1cc, 0xffc4c3be, 0xff141414, 0xff74736e, 0xff2e2e2c, 0xfff7f7f5, 0xff1a1a1a, 0xff141414, 0xffff5b1f, 0xffff8a5c, 0xffffffff, 0xffbdbcb6, 0xfff7f7f5, 0xff1c1c1c },
            // 6 TE DARK: near-black body, white/grey ink, teal accent
            { 0xff151515, 0xff111111, 0xff1b1b1b, 0xff000000, 0xff000000, 0xff050505, 0xff2a2a2a, 0xff363636, 0xffededed, 0xff8a8a8a, 0xffc9c9c9, 0xff3a3a3a, 0xff222222, 0xfff2f2f2, 0xff4fd6d9, 0xff83c8cc, 0xffffffff, 0xff3c3c3c, 0xffd9d9d9, 0xff1a1a1a },
            // 7 APPLE: white cards on light grey, graphite accent, no colour
            { 0xfff5f5f7, 0xffececef, 0xffffffff, 0xff1d1d1f, 0xff111113, 0xfff0f0f3, 0xffe8e8ed, 0xffdcdce1, 0xff1d1d1f, 0xff86868b, 0xff424245, 0xfff0f0f3, 0xffd2d2d7, 0xff1d1d1f, 0xff1d1d1f, 0xff6e6e73, 0xffffffff, 0xffc7c7cc, 0xffffffff, 0xff1d1d1f },
            // 8 PURE B/W: paper white and black only
            { 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xfff0f0f0, 0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xffffffff, 0xffffffff, 0xffffffff, 0xff000000 },
            // 9 CREAM: vintage studio gear, cream body, dark brown ink, rust accent
            { 0xffeee6d4, 0xffe6dcc6, 0xfff5efe1, 0xff1e1912, 0xff120e09, 0xffe4d9c2, 0xffddd1b8, 0xffd1c4a8, 0xff2b2219, 0xff6f6252, 0xff3d3226, 0xfffaf6ec, 0xff2b2219, 0xff2b2219, 0xffc4532f, 0xffd98a68, 0xfffff4e6, 0xffb9ab92, 0xfffaf6ec, 0xff2b2219 },
            // 10 FOG: nordic pale blue-grey, slate ink, steel blue accent
            { 0xffdfe5ea, 0xffd6dde3, 0xffeaeff3, 0xff17212b, 0xff0d141b, 0xffdbe3e9, 0xffd0d9e0, 0xffc3ced7, 0xff1f2b36, 0xff5d6c79, 0xff33424f, 0xfff5f8fa, 0xffc7d1d9, 0xff1f2b36, 0xff3f7fb5, 0xff7aa6cc, 0xffeaf4ff, 0xffaab7c2, 0xfff5f8fa, 0xff1f2b36 },
            // 11 MOSS: deep forest green, sage ink, moss accent
            { 0xff162019, 0xff121a14, 0xff1c281f, 0xff0a100b, 0xff050805, 0xff121b15, 0xff2b3a2f, 0xff36483a, 0xffe2eadf, 0xff93a597, 0xffc3d0c2, 0xff3a4d3e, 0xff243128, 0xffe2eadf, 0xff9cc77e, 0xff7fa868, 0xfff0ffe6, 0xff3b4a3e, 0xffdfe6dc, 0xff1a241c },
            // 12 GRAPHITE: dark graphite, warm white ink, copper accent
            { 0xff222426, 0xff1c1e20, 0xff292b2e, 0xff0e0f10, 0xff080909, 0xff1b1c1e, 0xff3a3d41, 0xff464a4f, 0xffece6df, 0xff9b948c, 0xffd2cbc3, 0xff45484c, 0xff2b2d30, 0xffece6df, 0xffd48c5c, 0xffb8774c, 0xfffff0e4, 0xff4a4d51, 0xffe4e0da, 0xff1e2022 },
            // 13 BLUEPRINT: cobalt body, white lines, sky accent
            { 0xff1f4f8f, 0xff1b4782, 0xff23589c, 0xff0f2a52, 0xff0a1f3e, 0xff1a467f, 0xff2f67ad, 0xff3a75bd, 0xfff4f8ff, 0xffa9c4e6, 0xffd8e6f8, 0xff2c63a8, 0xff1d4c8a, 0xfff4f8ff, 0xffa8dcff, 0xff7fc0f0, 0xffffffff, 0xff5d86bd, 0xfff4f8ff, 0xff163a6b },
            // 14 CONCRETE: light concrete grey, black ink, signal red
            { 0xffcfcdc8, 0xffc6c4bf, 0xffdcdad6, 0xff141414, 0xff0a0a0a, 0xffcfcdc8, 0xffc4c2bd, 0xffb7b5b0, 0xff111111, 0xff545350, 0xff2a2a29, 0xffe9e8e5, 0xffbab8b3, 0xff111111, 0xffe23b26, 0xffee7a68, 0xffffffff, 0xffa3a19c, 0xffefeeeb, 0xff151515 },
        };
        const auto& c = themes[juce::jlimit (0, (int) std::size (themes) - 1, t)];
        juce::Colour* dst[] = { &bgTop, &bgBottom, &panel, &screen, &deep, &field, &button, &buttonHi, &text, &textDim, &label, &knob, &knobDark, &track, &accent, &accentSoft, &ice, &ledOff, &whiteKey, &blackKey };
        for (size_t i = 0; i < std::size (dst); ++i) *dst[i] = juce::Colour (c[i]);
        theme = t;
        const bool light = text.getPerceivedBrightness() < 0.5f;
        hi = light ? juce::Colours::black : juce::Colours::white;
        onScreen = juce::Colours::white;
        knobRing = t == 5 || t == 8 ? juce::Colour (0xff141414) : t == 7 ? juce::Colour (0x22000000)
                 : t >= 9 ? text.withAlpha (light ? 0.55f : 0.3f) : juce::Colour (0x00000000);
        flatLed = t >= 5;
        edge = t == 8 ? juce::Colour (0xff000000) : t == 5 || t == 7 ? juce::Colour (0x1a000000)
             : (t >= 9 && light) ? text.withAlpha (0.14f) : juce::Colour (0x0fffffff);
        fontName = t >= 5 ? "Inter" : "";
        if (t == 5)      { lfoCol[0] = accent; lfoCol[1] = juce::Colour (0xffffffff); lfoCol[2] = juce::Colour (0xff9a9a9a); hit = juce::Colour (0xffff8a5c); }
        else if (t == 12)
        {
            // GRAPHITE: steel blue, sage, ivory - three LFOs that differ from the copper accent (ADSR, buttons) that sit in the palette
            lfoCol[0] = juce::Colour (0xff8fb4d0); lfoCol[1] = juce::Colour (0xffa9c79a); lfoCol[2] = juce::Colour (0xffe6dccd);
            hit = juce::Colour (0xffe8c4a6);
        }
        else if (t >= 9)
        {
            // accent, a lighter cousin and a neutral; all readable on the panel AND on the dark screens
            lfoCol[0] = accent; lfoCol[1] = accentSoft.interpolatedWith (juce::Colours::white, 0.25f); lfoCol[2] = juce::Colour (0xff9a9a98);
            hit = accentSoft.interpolatedWith (juce::Colours::white, 0.35f);
        }
        else if (t >= 6) { lfoCol[0] = t == 6 ? accent : juce::Colour (0xff9a9a9a); lfoCol[1] = juce::Colour (0xffbdbdbd); lfoCol[2] = juce::Colour (0xff7a7a7a); hit = t == 6 ? accent : juce::Colour (0xffd0d0d0); }
        else { lfoCol[0] = juce::Colour (0xffbff3ff); lfoCol[1] = juce::Colour (0xff9ee6c4); lfoCol[2] = juce::Colour (0xffd6be9c); hit = juce::Colour (0xffd6be9c); }
    }
    inline const bool themeReady = (applyTheme (12), true);   // GRAPHITE is the shipping look

    // LFO colours: ice, mint, sand
    // text colour that reads on top of the accent colour
    inline juce::Colour onAccent() { return accent.getPerceivedBrightness() > 0.58f ? juce::Colour (0xff111111) : juce::Colours::white; }

    inline juce::Colour lfo (int i) { return lfoCol[juce::jlimit (0, 2, i)]; }

    // signature colour: used ONLY for the per-harmonic GRAIN row
    inline const juce::Colour gold     { 0xffe6cc92 };
    inline const juce::Colour goldDeep { 0xffb8955a };
    // output meter red zone (muted)
    inline const juce::Colour clip     { 0xffe0625a };
}

class GranulumLook final : public juce::LookAndFeel_V4
{
public:
    GranulumLook();
    static inline bool flatKnobs = true;
    static inline int toggleStyle = 5;   // pressed/on buttons: teal between bright and muted (owner choice)
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

// Slider with a heavier feel: longer drag for the full range, Shift (or Ctrl) = 8x finer
class FineSlider final : public juce::Slider
{
public:
    int baseSensitivity = 500;   // pixels for the full range (JUCE default is 250)
    void mouseDown (const juce::MouseEvent& e) override
    {
        setMouseDragSensitivity (e.mods.isShiftDown() || e.mods.isCtrlDown() ? baseSensitivity * 8 : baseSensitivity);
        juce::Slider::mouseDown (e);
    }
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
    void setCompact (bool c) { compact = c; slider.setPopupDisplayEnabled (c, true, nullptr); resized(); }   // no value box: value pops up while dragging

    FineSlider slider;
    const int target;       // modulation target index, -1 if not modulatable
private:
    struct Dial { juce::Point<float> c; float r = 0, track = 0, start = 0, end = 0; };
    Dial dial() const;

    GranulumProcessor& proc;
    juce::String title;
    Size size;
    bool dragHover = false, anyMod = false, compact = false;
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

    // speed: the static layers are drawn once and reused every frame
    void rebuildCaches();
    juce::Image bgLow, bgHigh, vignette, glowSprite;
    int cacheTheme = -1;
public:
    static inline bool fastPaint = true;   // the snapshot tool flips this to compare old/new drawing
};

// Additive bars: draw the 16 harmonic amplitudes; the gold GRAIN row below picks which harmonics are granulated
class HarmonicEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit HarmonicEditor (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    int hoverBar = -1;          // public so the snapshot tool can show the hover look
    bool showHint = true;       // first-run hint over the dots
    float introT = 0.0f;        // time since open, drives the one-off light wave

private:
    void rememberHintSeen();
    void timerCallback() override;
    juce::Rectangle<float> barArea() const;
    juce::Rectangle<float> pillRow() const;
    int barAt (float x) const;
    void applyMouse (const juce::MouseEvent&, bool first);
    void applyPill (int bar);
    bool grainsActive() const;

    GranulumProcessor& proc;
    std::array<float, granulum::kNumHarmonics> h {};
    uint32_t mask = 0xffffu;
    int lastBar = -1, lastPill = -1;
    float lastValue = 0.0f, time = 0.0f;
    bool erasing = false, editing = false, pillMode = false, pillValue = true;
    struct Crystal { float bar = 0, dx = 0, y = 0, vy = 0, age = 0, life = 1, size = 2, rot = 0, spin = 0; bool white = false; };
    std::vector<Crystal> crystals;
    juce::Random rng;
public:
    int rowStyle = 2;
    static inline int barStyle = 5;   // 5 strings, 0 sand wave, 1 frosted glass, 2 capsule + filament, 3 line + cap, 4 fade   // 0 = pearl beads, 1 = glowing line (design preview)
};

// Slim stereo peak meter with a red zone, peak hold and a CLIP flag (click to reset)
class PeakMeter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PeakMeter (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    GranulumProcessor& proc;
    float level[2] {}, hold[2] {};
    int holdAge[2] {};
    bool clipShown = false;
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
    void mouseMove (const juce::MouseEvent&) override;
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

// Tiny filter view: response curve of the selected filter over a faint live spectrum of what comes in.
// Drag left/right = CUTOFF, up/down = RESO.
class FilterScope final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit FilterScope (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int which = 0;   // 0 = FILTER 1, 1 = FILTER 2
private:
    void timerCallback() override;
    float xToHz (float x) const;
    GranulumProcessor& proc;
    juce::dsp::FFT fft { 11 };
    std::array<float, 4096> fftData {};
    std::array<float, 1024> specDb {};
    float dragCut = 0, dragRes = 0;
    juce::Point<float> dragStart;
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
    juce::Rectangle<float> removeRect() const;
public:
    bool hoverRemove = false;   // public for the snapshot preview
private:
    int hitTest (juce::Point<float>, bool& isHandle) const;

    GranulumProcessor& proc;
    std::array<LfoPoint, kLfoPoints> pts {};
    int dragIndex = -1, hoverIndex = -1;
    bool dragHandle = false, hoverHandle = false;
    float dragStartCurve = 0.0f;
    juce::Point<float> dragStart;
};

// "1 2 3": which oscillators go through a block. Hovering a digit highlights that oscillator.
class RouteButtons final : public juce::Component
{
public:
    RouteButtons (GranulumProcessor&, const char* block, const juce::String& blockName);
    ~RouteButtons() override;
    void resized() override;
    std::function<void (int)> onHover;   // oscillator index, -1 = none
    static inline int style = 3;          // soft segmented (owner choice); 0 squares, 1 numerals, 2 circles, 4 lamps
private:
    struct Digit;
    juce::OwnedArray<Digit> digits;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> atts;
};

// Effect tab: LED on the left turns the effect on/off, the name selects the tab
class FxTab final : public juce::Component, public juce::SettableTooltipClient
{
public:
    FxTab (GranulumProcessor&, const char* onParam, const juce::String& name);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    std::function<void()> onSelect;
    bool selected = false;
    static inline int style = 2;   // 0 filled buttons, 1 text + underline, 2 soft segmented, 3 outline
private:
    juce::RangedAudioParameter* param;
    juce::String name;
    float ledX = 14.0f;   // where the light is drawn; clicks within ~12 px of it switch the effect
    bool overLed (juce::Point<float> p) const { return std::abs (p.x - ledX) < 12.0f; }
};

// Header preset browser: < NAME > SAVE
class PresetBar final : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void()> onLoaded;
private:
    void timerCallback() override { repaint(); }
    void step (int dir);
    void showMenu();
    void askSave();
    GranulumProcessor& proc;
    juce::TextButton prev { juce::String::fromUTF8 ("\xe2\x97\x80") }, next { juce::String::fromUTF8 ("\xe2\x96\xb6") }, save { "SAVE" };
    juce::Rectangle<int> nameArea;
    std::unique_ptr<juce::AlertWindow> saveWindow;
};

// OSC 3 noise picker: name + mini waveform, arrows to step, click for the full list
class NoisePicker final : public juce::Component, private juce::Timer
{
public:
    explicit NoisePicker (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    void setIndex (int i);
    int index() const;
    GranulumProcessor& proc;
    juce::TextButton prev { juce::String::fromUTF8 ("\xe2\x97\x80") }, next { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    juce::Rectangle<int> body;
};

// Factory sample menu, opened from the SOUNDS button in OSC 2: folders as headers, then "your own file"
class SampleMenu final : public juce::Component
{
public:
    explicit SampleMenu (GranulumProcessor& p) : proc (p) {}
    void rebuild();                           // rescan the factory folder
    int preferredHeight() const;
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void (const juce::File&)> onPick;   // empty file = "load your own"
    std::function<void()> onClose;
private:
    struct Row { juce::String text; juce::File file; int kind; };   // 0 header, 1 sample, 2 own file
    int rowAt (float y) const;
    float rowY (int i) const;
    float rowH (int i) const { return rows[(size_t) i].kind == 0 ? 22.0f : 24.0f; }
    GranulumProcessor& proc;
    std::vector<Row> rows;
    int hover = -1;
};

// AMP tab in the LFO strip: speaker icon + "AMP"
class AmpTab final : public juce::Component, public juce::SettableTooltipClient
{
public:
    AmpTab() { setTooltip ("ADSR amp: the volume envelope of every note.\nClick the light to switch it off (notes become a plain on/off gate)."); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void()> onSelect;
    bool selected = false;
    juce::RangedAudioParameter* param = nullptr;
};

// ADSR drawn as a curve; drag the three points (attack peak, decay/sustain corner, release end)
class AmpEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit AmpEditor (GranulumProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    struct Geo { juce::Point<float> p0, p1, p2, p3, p4; juce::Rectangle<float> a; float maxW = 0, plateau = 0; };
    Geo geometry() const;
    float value (const char* id) const;
    void setValue (const char* id, float v);
    GranulumProcessor& proc;
    int dragging = -1, hover = -1;
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
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    // snapshot tool: 0 normal, 1 mouse over the sample box, 2 a file is being dragged over the plugin
    void previewDropState (int s) { dropHover = s == 1; fileHover = s == 2; repaint(); }

    GrainField& grainField() { return field; }
    HarmonicEditor& harmonicEditor() { return harmonics; }
    GranulumLook& lookAndFeel() { return look; }
    void selectFx (int i);
    void selectFilter (int i);
    void showAmp() { selectLfo (3); }
    static inline int removeStyle = 0;   // 0 = SYNC | CLEAR halves, 1 = SYNC + small X

private:
    void timerCallback() override;
    void openFile (const juce::File&);
    void launchChooser();
    void selectLfo (int i);
    void showChordPicker();
    void setHoverOsc (int o);

    GranulumProcessor& proc;
    GranulumLook look;

    GrainField field;
    HarmonicEditor harmonics;
    NoisePicker noisePicker;
    PeakMeter meterView;
    PresetBar presetBar;

    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    FilterScope filterScope { proc };
    SampleMenu sampleMenu { proc };
    void toggleSampleMenu (bool show);
public:
    void openSampleMenuForPreview (bool show = true) { toggleSampleMenu (show); }
    static inline int soundsStyle = 4;   // 4 = same as a lit GRAIN button (owner's choice); 1-3 earlier variants
private:
    juce::TextButton samplePrev { juce::String::fromUTF8 ("\xe2\x97\x80") }, sampleNext { juce::String::fromUTF8 ("\xe2\x96\xb6") };
    Lamp osc1Lamp { "OSC 1", "ADDITIVE" }, osc2Lamp { "OSC 2", "SAMPLE" }, osc3Lamp { "OSC 3", "NOISE" };
    std::unique_ptr<ButtonAtt> osc1LampA, osc2LampA, osc3LampA;

    juce::TextButton undoButton { "UNDO" }, initButton { "INIT" }, randomButton { "RANDOM ALL" }, loadButton { juce::String::fromUTF8 ("SOUNDS \xe2\x96\xbe") }, chordButton { "CHORD: OFF" };
    juce::TextButton rndGrains { "RANDOM" }, rndFx { "RANDOM" }, rndBars { "RANDOM" };
    juce::TextButton osc1Grain { "GRAIN" }, osc2Grain { "GRAIN" }, osc3Grain { "GRAIN" };
    juce::TextButton polyButton { "POLY" }, monoButton { "MONO" };
    juce::TextButton delaySync { "SYNC" }, delayPing { "PING-PONG" };
    std::unique_ptr<ButtonAtt> osc1GrainA, osc2GrainA, osc3GrainA, delaySyncA, delayPingA;
    juce::ComboBox filterType, filterType2, distType, delayDiv;
    std::unique_ptr<ComboAtt> filterTypeA, filterType2A, distTypeA, delayDivA;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<Knob>> heroKnobs, smallKnobs, osc1Knobs, osc2Knobs, osc3Knobs, filterKnobs, filter2Knobs,
                                       distKnobs, delayKnobs, revKnobs, globalKnobs;
    std::vector<Knob*> allKnobs;

    // effects: tabs + routing
    std::array<std::unique_ptr<FxTab>, 3> fxTabs;
    std::array<std::unique_ptr<RouteButtons>, kNumRouteBlocks> routes;   // filter, dist, delay, reverb, filter 2
    std::array<std::unique_ptr<FxTab>, 2> filterTabs;
    int selectedFx = 0, selectedFilter = 0;
    int hoverOsc = -1;

    struct LfoControls
    {
        juce::ComboBox preset, mode;
        juce::TextButton sync { "SYNC" }, remove { "REMOVE" }, grains { "INTO GRAINS" };
        std::unique_ptr<Knob> rate, rise, div;   // RATE knob shows the division when SYNC is on (like Serum)
        std::unique_ptr<ButtonAtt> grainsA;
        std::unique_ptr<ComboAtt> modeA;
        std::unique_ptr<ButtonAtt> syncA;
    };
    std::array<std::unique_ptr<LfoTab>, kNumLfos> lfoTabs;
    std::array<std::unique_ptr<LfoControls>, kNumLfos> lfoControls;
    LfoEditor lfoEditor;
    AmpTab ampTab;
    AmpEditor ampEditor;
    std::vector<std::unique_ptr<Knob>> ampKnobs;
    int selectedLfo = 0;     // 0..2 = LFO, 3 = AMP

    juce::Rectangle<int> titleArea, gemsArea, screenFrame, osc1Area, osc2Area, osc3Area, dropZone,
                         grainArea, filterArea, fxArea, ledArea, lfoFrame, voiceArea, fxContent,
                         lfoPresetLabel, lfoModeLabel, lfoDivLabel, distTypeLabel, fxLabel, grainLabel, adsrReadout;
    float meter = 0.0f;
    float activity = 0.0f;
    uint32_t shownModVersion = 0;
    juce::String statusText;
    bool fileHover = false, dropHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumEditor)
};
