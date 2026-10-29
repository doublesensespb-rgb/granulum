#include "PluginEditor.h"

using namespace juce;

// ============================================================================
// Look and feel

GranulumLook::GranulumLook()
{
    setColour (Slider::textBoxTextColourId, Palette::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Palette::violet.withAlpha (0.4f));
    setColour (Label::textColourId, Palette::text);
    setColour (TextButton::textColourOffId, Palette::text);
    setColour (TextButton::textColourOnId, Palette::bg);
    setColour (ComboBox::backgroundColourId, Palette::bg);
    setColour (ComboBox::outlineColourId, Palette::panelEdge);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::arrowColourId, Palette::dim);
    setColour (PopupMenu::backgroundColourId, Palette::panel);
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::violet.withAlpha (0.35f));
    setColour (TextEditor::backgroundColourId, Palette::bg);
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::outlineColourId, Palette::panelEdge);
    setColour (TextEditor::focusedOutlineColourId, Palette::violet);
}

void GranulumLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                     float start, float end, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);
    const float track = jmax (2.5f, r * 0.11f);

    // track
    Path bgArc;
    bgArc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, start, end, true);
    g.setColour (Palette::panelEdge);
    g.strokePath (bgArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc (bipolar params start from the centre)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        Path valArc;
        valArc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (valArc, PathStrokeType (track * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (valArc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // body
    const float br = r - track * 2.6f;
    g.setGradientFill (ColourGradient (Colour (0xff262b3a), c.x, c.y - br, Colour (0xff12151d), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (Colour (0xff2e3446));
    g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);

    // pointer
    const Point<float> p1 = c.getPointOnCircumference (br * 0.35f, angle);
    const Point<float> p2 = c.getPointOnCircumference (br * 0.85f, angle);
    g.setColour (Palette::text);
    g.drawLine ({ p1, p2 }, 2.2f);
}

void GranulumLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const auto accent = b.findColour (TextButton::buttonOnColourId);
    g.setColour (down ? accent.withAlpha (0.35f) : over ? accent.withAlpha (0.18f) : Palette::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (over ? accent : Palette::panelEdge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

Label* GranulumLook::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (FontOptions (12.0f));
    l->setJustificationType (Justification::centred);
    return l;
}

// ============================================================================
// Knob

Knob::Knob (AudioProcessorValueTreeState& state, const String& paramId, const String& t, Colour accent)
    : title (t)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setDoubleClickReturnValue (true, state.getParameter (paramId)->convertFrom0to1 (state.getParameter (paramId)->getDefaultValue()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    slider.setBounds (r);
}

void Knob::paint (Graphics& g)
{
    g.setColour (Palette::dim);
    g.setFont (FontOptions (11.5f, Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (16), Justification::centred);
}

// ============================================================================
// Waveform

WaveformView::WaveformView (GranulumProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    startTimerHz (40);
}

void WaveformView::timerCallback()
{
    auto& e = proc.engine;
    const int n = e.vizCount.load (std::memory_order_acquire);
    grainX.resize ((size_t) n);
    grainA.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        grainX[(size_t) i] = e.vizPos[(size_t) i].load (std::memory_order_relaxed);
        grainA[(size_t) i] = e.vizAmp[(size_t) i].load (std::memory_order_relaxed);
    }
    const int h = e.vizHeadCount.load (std::memory_order_acquire);
    heads.resize ((size_t) h);
    for (int i = 0; i < h; ++i)
        heads[(size_t) i] = e.vizHead[(size_t) i].load (std::memory_order_relaxed);

    phase += 0.02f;
    repaint();
}

void WaveformView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::bg);
    g.fillRoundedRectangle (r, 10.0f);

    auto inner = r.reduced (10.0f, 12.0f);
    const float mid = inner.getCentreY();
    const float halfH = inner.getHeight() * 0.5f;

    // centre line
    g.setColour (Palette::panelEdge);
    g.drawHorizontalLine ((int) mid, inner.getX(), inner.getRight());

    auto peaks = proc.getPeaks();
    if (peaks == nullptr || peaks->maxs.empty())
    {
        g.setColour (dragHover ? Palette::mint : Palette::dim);
        g.setFont (FontOptions (17.0f));
        g.drawText (String::fromUTF8 ("Перетащи сюда WAV / AIFF / FLAC / MP3  или нажми LOAD"),
                    getLocalBounds(), Justification::centred);
        if (dragHover)
        {
            g.setColour (Palette::mint.withAlpha (0.7f));
            g.drawRoundedRectangle (r.reduced (2.0f), 10.0f, 2.0f);
        }
        return;
    }

    const float position = proc.apvts.getRawParameterValue (ParamID::position)->load();
    const float spray    = proc.apvts.getRawParameterValue (ParamID::spray)->load();
    const float sizeMs   = proc.apvts.getRawParameterValue (ParamID::size)->load();
    const double len     = jmax (0.001, proc.getSampleLengthSeconds());

    // spray zone
    auto xOf = [&] (float norm) { return inner.getX() + norm * inner.getWidth(); };
    {
        const float w = spray * inner.getWidth();
        const float gw = jmin (1.0f, (float) (sizeMs * 0.001 / len)) * inner.getWidth();
        const float cx = xOf (position);
        auto zone = Rectangle<float> (cx - w - gw * 0.5f, inner.getY(), (w * 2.0f + gw), inner.getHeight());
        g.setGradientFill (ColourGradient (Palette::violet.withAlpha (0.0f), zone.getX(), 0,
                                           Palette::violet.withAlpha (0.0f), zone.getRight(), 0, false));
        ColourGradient cg (Palette::violet.withAlpha (0.02f), zone.getX(), 0, Palette::violet.withAlpha (0.02f), zone.getRight(), 0, false);
        cg.addColour (0.5, Palette::violet.withAlpha (0.20f));
        g.setGradientFill (cg);
        g.fillRect (zone);
        // wrap-around part
        if (zone.getRight() > inner.getRight())
            g.fillRect (Rectangle<float> (inner.getX(), inner.getY(), zone.getRight() - inner.getRight(), inner.getHeight()));
        if (zone.getX() < inner.getX())
            g.fillRect (Rectangle<float> (inner.getRight() - (inner.getX() - zone.getX()), inner.getY(), inner.getX() - zone.getX(), inner.getHeight()));
    }

    // waveform
    {
        const int bins = (int) peaks->maxs.size();
        const int cols = (int) inner.getWidth();
        Path wave;
        std::vector<float> tops ((size_t) cols), bots ((size_t) cols);
        for (int x = 0; x < cols; ++x)
        {
            const int b0 = x * bins / cols;
            const int b1 = jmax (b0 + 1, (x + 1) * bins / cols);
            float lo = 0, hi = 0;
            for (int b = b0; b < b1 && b < bins; ++b)
            {
                lo = jmin (lo, peaks->mins[(size_t) b]);
                hi = jmax (hi, peaks->maxs[(size_t) b]);
            }
            tops[(size_t) x] = mid - hi * halfH;
            bots[(size_t) x] = mid - lo * halfH;
        }
        wave.startNewSubPath (inner.getX(), tops[0]);
        for (int x = 1; x < cols; ++x) wave.lineTo (inner.getX() + (float) x, tops[(size_t) x]);
        for (int x = cols - 1; x >= 0; --x) wave.lineTo (inner.getX() + (float) x, bots[(size_t) x]);
        wave.closeSubPath();

        g.setGradientFill (ColourGradient (Palette::wave.brighter (0.5f), 0, inner.getY(), Palette::wave, 0, mid, false));
        g.fillPath (wave);
    }

    // playheads per voice
    for (float h : heads)
    {
        float hp = position + h;
        hp -= std::floor (hp);
        g.setColour (Palette::mint.withAlpha (0.55f));
        g.drawVerticalLine ((int) xOf (hp), inner.getY(), inner.getBottom());
    }

    // position marker
    g.setColour (Palette::violet);
    g.fillRect (xOf (position) - 1.0f, inner.getY() - 6.0f, 2.0f, inner.getHeight() + 12.0f);
    g.fillEllipse (xOf (position) - 5.0f, inner.getY() - 10.0f, 10.0f, 10.0f);

    // grains
    for (size_t i = 0; i < grainX.size(); ++i)
    {
        const float x = xOf (grainX[i]);
        const uint32 hsh = (uint32) (i * 2654435761u);
        const float yN = ((float) (hsh % 1000) / 1000.0f) * 2.0f - 1.0f;
        const float y = mid + yN * halfH * 0.8f;
        const float a = jlimit (0.0f, 1.0f, grainA[i]);
        const float rad = 2.0f + a * 5.0f;
        const auto col = (i % 3 == 0 ? Palette::pink : Palette::mint);
        g.setColour (col.withAlpha (0.18f * a));
        g.fillEllipse (x - rad * 2.2f, y - rad * 2.2f, rad * 4.4f, rad * 4.4f);
        g.setColour (col.withAlpha (0.35f + 0.65f * a));
        g.fillEllipse (x - rad * 0.5f, y - rad * 0.5f, rad, rad);
    }

    if (dragHover)
    {
        g.setColour (Palette::mint.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (2.0f), 10.0f, 2.0f);
    }
}

void WaveformView::setPositionFromMouse (const MouseEvent& e)
{
    auto inner = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    const float norm = jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / inner.getWidth());
    if (auto* p = proc.apvts.getParameter (ParamID::position))
        p->setValueNotifyingHost (norm);
}

void WaveformView::mouseDown (const MouseEvent& e)
{
    if (auto* p = proc.apvts.getParameter (ParamID::position)) p->beginChangeGesture();
    setPositionFromMouse (e);
}

void WaveformView::mouseDrag (const MouseEvent& e) { setPositionFromMouse (e); }

void WaveformView::mouseUp (const MouseEvent&)
{
    if (auto* p = proc.apvts.getParameter (ParamID::position)) p->endChangeGesture();
}

// ============================================================================
// Editor

GranulumEditor::GranulumEditor (GranulumProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);

    auto& s = proc.apvts;
    auto add = [&] (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* name, Colour c)
    {
        group.push_back (std::make_unique<Knob> (s, id, name, c));
        addAndMakeVisible (*group.back());
    };

    add (grainKnobs, ParamID::position,   "Position",    Palette::violet);
    add (grainKnobs, ParamID::spray,      "Spray",       Palette::violet);
    add (grainKnobs, ParamID::scan,       "Scan",        Palette::violet);
    add (grainKnobs, ParamID::size,       "Size",        Palette::violet);
    add (grainKnobs, ParamID::density,    "Density",     Palette::violet);
    add (grainKnobs, ParamID::chaos,      "Chaos",       Palette::violet);

    add (pitchKnobs, ParamID::pitch,      "Pitch",       Palette::mint);
    add (pitchKnobs, ParamID::pitchSpray, "Pitch Spray", Palette::mint);
    add (pitchKnobs, ParamID::octave,     "Octaves",     Palette::mint);
    add (pitchKnobs, ParamID::reverse,    "Reverse",     Palette::mint);

    add (shapeKnobs, ParamID::shape,      "Window",      Palette::pink);
    add (shapeKnobs, ParamID::spread,     "Stereo",      Palette::pink);
    add (shapeKnobs, ParamID::attack,     "Attack",      Palette::pink);
    add (shapeKnobs, ParamID::release,    "Release",     Palette::pink);

    add (fxKnobs,    ParamID::cutoff,     "Cutoff",      Palette::text);
    add (fxKnobs,    ParamID::reso,       "Reso",        Palette::text);
    add (fxKnobs,    ParamID::space,      "Space",       Palette::text);
    add (fxKnobs,    ParamID::room,       "Room",        Palette::text);
    add (fxKnobs,    ParamID::output,     "Output",      Palette::text);

    filterType.addItemList ({ "Low Pass", "Band Pass", "High Pass" }, 1);
    addAndMakeVisible (filterType);
    filterTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, ParamID::ftype, filterType);

    addAndMakeVisible (waveform);

    loadButton.setColour (TextButton::buttonOnColourId, Palette::mint);
    randomButton.setColour (TextButton::buttonOnColourId, Palette::pink);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (randomButton);

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load a sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) openFile (f);
                              });
    };
    randomButton.onClick = [this] { proc.randomize(); };

    sampleLabel.setFont (FontOptions (13.0f));
    sampleLabel.setColour (Label::textColourId, Palette::dim);
    sampleLabel.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (sampleLabel);

    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffd9dce6));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1b1f2a));
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff8a90a3));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::violet.withAlpha (0.8f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::mint.withAlpha (0.35f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keyboard.setAvailableRange (24, 108);
    keyboard.setKeyWidth (17.0f);
    keyboard.setLowestVisibleKey (36);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (860, 600, 1600, 1100);
    setSize (1040, 660);
    startTimerHz (20);
}

GranulumEditor::~GranulumEditor()
{
    setLookAndFeel (nullptr);
}

void GranulumEditor::openFile (const File& f)
{
    String err;
    if (proc.loadSample (f, err))
        statusText.clear();
    else
        statusText = err;
}

void GranulumEditor::timerCallback()
{
    const auto name = proc.getSampleName();
    const auto len = proc.getSampleLengthSeconds();
    const String text = statusText.isNotEmpty() ? statusText
                      : name.isEmpty() ? String::fromUTF8 ("Сэмпл не загружен")
                      : name + "   " + String (len, 2) + " s";
    if (sampleLabel.getText() != text) sampleLabel.setText (text, dontSendNotification);

    meter = jmax (proc.outputLevel.load(), meter * 0.85f);
    repaint (meterArea);
}

void GranulumEditor::drawSection (Graphics& g, Rectangle<int> r, const String& title, Colour c)
{
    auto rf = r.toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (rf, 10.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (rf.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (c);
    g.fillRoundedRectangle (rf.getX() + 14.0f, rf.getY() + 13.0f, 8.0f, 8.0f, 2.0f);
    g.setColour (Palette::text);
    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawText (title, (int) rf.getX() + 28, (int) rf.getY() + 7, 200, 20, Justification::centredLeft);
}

void GranulumEditor::paint (Graphics& g)
{
    g.fillAll (Palette::bg);

    // header
    g.setFont (FontOptions (24.0f, Font::bold));
    auto title = Rectangle<int> (20, 12, 200, 34);
    ColourGradient tg (Palette::mint, (float) title.getX(), 0, Palette::violet, (float) title.getRight() - 60, 0, false);
    g.setGradientFill (tg);
    g.drawText ("GRANULUM", title, Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (FontOptions (11.0f));
    g.drawText ("granular sampler", 20, 40, 200, 14, Justification::centredLeft);

    drawSection (g, grainArea, "GRAINS", Palette::violet);
    drawSection (g, pitchArea, "PITCH", Palette::mint);
    drawSection (g, shapeArea, "SHAPE", Palette::pink);
    drawSection (g, fxArea,    "FILTER + SPACE", Palette::text);

    // output meter
    auto m = meterArea.toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (m, 3.0f);
    const float lvl = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (meter, -60.0f) + 60.0f) / 60.0f);
    auto fill = m.withTrimmedRight (m.getWidth() * (1.0f - lvl));
    g.setGradientFill (ColourGradient (Palette::mint, m.getX(), 0, Palette::pink, m.getRight(), 0, false));
    g.fillRoundedRectangle (fill, 3.0f);
}

void GranulumEditor::resized()
{
    auto r = getLocalBounds().reduced (16, 0);

    auto header = r.removeFromTop (62).reduced (0, 12);
    header.removeFromLeft (220);
    meterArea = header.removeFromRight (140).withSizeKeepingCentre (140, 8);
    header.removeFromRight (16);
    randomButton.setBounds (header.removeFromRight (90));
    header.removeFromRight (8);
    loadButton.setBounds (header.removeFromRight (90));
    header.removeFromRight (16);
    sampleLabel.setBounds (header);

    keyboard.setBounds (r.removeFromBottom (64).withTrimmedBottom (10));
    r.removeFromBottom (10);

    const int knobRows = jmax (240, (int) (r.getHeight() * 0.48f));
    auto knobs = r.removeFromBottom (knobRows);
    r.removeFromBottom (12);
    waveform.setBounds (r);

    // 10 columns: grains 3, pitch 2, shape 2, fx 3
    const int gap = 12;
    const float colW = (float) (knobs.getWidth() - gap * 3) / 10.0f;
    grainArea = knobs.removeFromLeft ((int) (colW * 3)); knobs.removeFromLeft (gap);
    pitchArea = knobs.removeFromLeft ((int) (colW * 2)); knobs.removeFromLeft (gap);
    shapeArea = knobs.removeFromLeft ((int) (colW * 2)); knobs.removeFromLeft (gap);
    fxArea    = knobs;

    auto layoutGrid = [] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> area, int cols, int extraSlots = 0)
    {
        area = area.withTrimmedTop (30).reduced (8, 6);
        const int total = (int) ks.size() + extraSlots;
        const int rows = (total + cols - 1) / cols;
        const int cw = area.getWidth() / cols;
        const int rh = area.getHeight() / rows;
        for (int i = 0; i < (int) ks.size(); ++i)
            ks[(size_t) i]->setBounds (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * rh, cw, rh);
        return Rectangle<int> (area.getX() + (total - 1) % cols * cw, area.getY() + (total - 1) / cols * rh, cw, rh);
    };

    layoutGrid (grainKnobs, grainArea, 3);
    layoutGrid (pitchKnobs, pitchArea, 2);
    layoutGrid (shapeKnobs, shapeArea, 2);
    auto comboSlot = layoutGrid (fxKnobs, fxArea, 3, 1);
    filterType.setBounds (comboSlot.withSizeKeepingCentre (jmin (comboSlot.getWidth() - 10, 110), 26));
}

bool GranulumEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (GranulumProcessor::isSupportedAudioFile (f)) return true;
    return false;
}

void GranulumEditor::fileDragEnter (const StringArray&, int, int) { waveform.dragHover = true;  waveform.repaint(); }
void GranulumEditor::fileDragExit  (const StringArray&)           { waveform.dragHover = false; waveform.repaint(); }

void GranulumEditor::filesDropped (const StringArray& files, int, int)
{
    waveform.dragHover = false;
    for (auto& f : files)
        if (GranulumProcessor::isSupportedAudioFile (f)) { openFile (File (f)); break; }
}
