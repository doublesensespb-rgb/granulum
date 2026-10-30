#include "PluginEditor.h"
#include "Visuals.h"

using namespace juce;

Font uiFont (float size, bool bold, float kerning)
{
    return Font { FontOptions (size, bold ? Font::bold : Font::plain).withKerningFactor (kerning) };
}

void drawLed (Graphics& g, Rectangle<float> c, Colour on, Colour off, float level)
{
    level = jlimit (0.0f, 1.0f, level);
    const auto ctr = c.getCentre();
    const float r = c.getWidth() * 0.5f;
    if (level > 0.02f)
    {
        g.setGradientFill (ColourGradient (on.withAlpha (0.45f * level), ctr.x, ctr.y, on.withAlpha (0.0f), ctr.x + r * 2.6f, ctr.y, true));
        g.fillEllipse (c.expanded (r * 1.6f));
    }
    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillEllipse (c.expanded (1.5f));
    g.setColour (off.interpolatedWith (on, level));
    g.fillEllipse (c);
    g.setColour (Colours::white.withAlpha (0.10f + 0.25f * level));
    g.fillEllipse (ctr.x - r * 0.5f, ctr.y - r * 0.65f, r, r * 0.6f);
}

namespace
{
    Path arcPath (Point<float> c, float radius, float a0, float a1)
    {
        Path p;
        p.addCentredArc (c.x, c.y, radius, radius, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
        return p;
    }
}

// ============================================================================
// Look and feel

GranulumLook::GranulumLook()
{
    setColour (Slider::textBoxTextColourId, Palette::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Palette::field);
    setColour (Slider::textBoxHighlightColourId, Palette::accent.withAlpha (0.35f));
    setColour (Label::textColourId, Palette::text);
    setColour (ComboBox::textColourId, Palette::text);
    setColour (ComboBox::arrowColourId, Palette::textDim);
    setColour (PopupMenu::backgroundColourId, Palette::panel);
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::button);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (TextEditor::backgroundColourId, Palette::field);
    setColour (TextEditor::textColourId, Palette::text);
    setColour (TextEditor::outlineColourId, Colours::transparentBlack);
    setColour (TextEditor::focusedOutlineColourId, Palette::accent);
    setColour (TooltipWindow::backgroundColourId, Palette::panel);
    setColour (TooltipWindow::textColourId, Palette::text);
}

void GranulumLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                     float start, float end, Slider& s)
{
    auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);
    const float track = jmax (2.5f, r * 0.09f);
    const float ar = r - track * 0.5f;

    g.setColour (Palette::track);
    g.strokePath (arcPath (c, ar, start, end), PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        g.setColour (Palette::accent.withAlpha (0.18f));
        g.strokePath (arcPath (c, ar, from, angle), PathStrokeType (track * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Palette::accentSoft.interpolatedWith (Palette::accent, s.isMouseOverOrDragging() ? 0.9f : 0.4f));
        g.strokePath (arcPath (c, ar, from, angle), PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float br = r - track * 2.4f;
    auto body = Rectangle<float> (c.x - br, c.y - br, br * 2, br * 2);
    g.setColour (Colours::black.withAlpha (0.28f));
    g.fillEllipse (body.translated (0.0f, br * 0.10f).expanded (br * 0.04f));
    g.setGradientFill (ColourGradient (Palette::knob.brighter (0.12f), c.x, body.getY(), Palette::knobDark, c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawEllipse (body.reduced (0.6f), 0.8f);

    g.setColour (Palette::track);
    g.drawLine ({ c.getPointOnCircumference (br * 0.15f, angle), c.getPointOnCircumference (br * 0.85f, angle) }, jmax (2.0f, br * 0.09f));
}

void GranulumLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float rad = 6.0f;
    if (b.getToggleState())
    {
        const var stem = b.getProperties()["stem"];
        const auto col = stem.isVoid() ? Palette::accent : Palette::stem ((int) stem);
        g.setColour (col.withAlpha (0.25f));
        g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
        g.setColour (col.interpolatedWith (Colours::white, over ? 0.2f : 0.0f));
        g.fillRoundedRectangle (r, rad);
        return;
    }
    g.setColour (down ? Palette::button.darker (0.2f) : over ? Palette::buttonHi : Palette::button);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

void GranulumLook::drawButtonText (Graphics& g, TextButton& b, bool over, bool)
{
    g.setFont (uiFont (b.getHeight() < 26 ? 11.0f : 12.5f, b.getToggleState(), 0.08f));
    g.setColour (b.getToggleState() ? Palette::screen : (over ? Colours::white : Palette::text));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

void GranulumLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.5f);
    g.setColour (Palette::field.withAlpha (box.isEnabled() ? 1.0f : 0.5f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    Path arrow;
    const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::textDim);
    g.fillPath (arrow);
}

Font GranulumLook::getComboBoxFont (ComboBox&) { return uiFont (12.0f, false, 0.05f); }
Font GranulumLook::getPopupMenuFont()          { return uiFont (14.0f); }

Label* GranulumLook::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.0f, false, 0.03f));
    l->setJustificationType (Justification::centred);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineWhenEditingColourId, Palette::accent);
    return l;
}

void GranulumLook::drawLabel (Graphics& g, Label& l)
{
    if (dynamic_cast<Slider*> (l.getParentComponent()) != nullptr && ! l.isBeingEdited())
    {
        const auto text = l.getText();
        g.setFont (getLabelFont (l));
        const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 14.0f;
        auto r = l.getLocalBounds().toFloat().withSizeKeepingCentre (jmin ((float) l.getWidth(), jmax (48.0f, tw)), (float) l.getHeight() - 1.0f);
        g.setColour (Palette::field);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Palette::text);
        g.drawText (text, r, Justification::centred, false);
        return;
    }
    LookAndFeel_V4::drawLabel (g, l);
}

// ============================================================================
// Modulation chip

ModChip::ModChip (GranulumProcessor& p, Knob& k, int l) : lfo (l), proc (p), owner (k)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setRepaintsOnMouseActivity (true);
}

void ModChip::updateTooltip()
{
    const float d = proc.getModulationDepth (owner.target, lfo);
    setTooltip ("LFO " + String (lfo + 1) + ": " + String (roundToInt (d * 100.0f)) + " %\n"
                + String::fromUTF8 ("Тяни вверх/вниз: глубина. Двойной клик: убрать."));
}

void ModChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = Palette::lfo (lfo);
    const float d = proc.getModulationDepth (owner.target, lfo);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (r.expanded (1.0f));
    g.setColour (Palette::screen);
    g.fillEllipse (r);
    const float rad = r.getWidth() * 0.5f - 1.5f;
    g.setColour (col.withAlpha (0.25f));
    g.drawEllipse (r.reduced (1.5f), 1.5f);
    g.setColour (col);
    g.strokePath (arcPath (r.getCentre(), rad, 0.0f, d * MathConstants<float>::twoPi * 0.999f), PathStrokeType (2.2f));
    g.setColour (isMouseOver() ? Colours::white : col);
    g.setFont (uiFont (r.getHeight() * 0.62f, true));
    g.drawText (String (lfo + 1), r, Justification::centred, false);
}

void ModChip::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        m.addItem (1, "Invert");
        m.addItem (2, "Remove");
        Component::SafePointer<ModChip> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int res)
        {
            if (safe == nullptr) return;
            auto& c = *safe;
            if (res == 1) c.proc.setModulation (c.owner.target, c.lfo, -c.proc.getModulationDepth (c.owner.target, c.lfo));
            if (res == 2) c.proc.removeModulation (c.owner.target, c.lfo);
        });
        return;
    }
    startDepth = proc.getModulationDepth (owner.target, lfo);
}

void ModChip::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float sens = e.mods.isShiftDown() ? 600.0f : 150.0f;
    proc.setModulation (owner.target, lfo, jlimit (-1.0f, 1.0f, startDepth - (float) e.getDistanceFromDragStartY() / sens));
    updateTooltip();
    owner.repaint();
}

void ModChip::mouseDoubleClick (const MouseEvent&) { proc.removeModulation (owner.target, lfo); }

// ============================================================================
// Knob

Knob::Knob (GranulumProcessor& p, const String& id, const String& t, Size sz)
    : target (GranulumProcessor::modIndexOf (id)), paramId (id), proc (p), title (t), size (sz)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    auto* param = proc.apvts.getParameter (id);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, slider);

    if (target >= 0)
        for (int l = 0; l < kNumLfos; ++l)
        {
            chips[(size_t) l] = std::make_unique<ModChip> (proc, *this, l);
            addChildComponent (*chips[(size_t) l]);
        }
}

Knob::Dial Knob::dial() const
{
    Dial d;
    auto* lf = dynamic_cast<Slider::LookAndFeelMethods*> (&slider.getLookAndFeel());
    auto sb = (lf != nullptr ? lf->getSliderLayout (const_cast<Slider&> (slider)).sliderBounds : slider.getLocalBounds())
                  .translated (slider.getX(), slider.getY()).toFloat().reduced (5.0f);
    d.r = jmin (sb.getWidth(), sb.getHeight()) * 0.5f;
    d.c = sb.getCentre();
    d.track = jmax (2.5f, d.r * 0.09f);
    const auto rp = slider.getRotaryParameters();
    d.start = rp.startAngleRadians;
    d.end = rp.endAngleRadians;
    return d;
}

void Knob::resized()
{
    const int titleH = size == Size::Hero ? 20 : size == Size::Small ? 14 : 16;
    const int boxH   = size == Size::Hero ? 20 : size == Size::Small ? 16 : 18;
    auto r = getLocalBounds();
    r.removeFromTop (titleH);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (getWidth() - 2, 90), boxH);
    slider.setBounds (r);
    refreshMods();
}

void Knob::paint (Graphics& g)
{
    const int titleH = size == Size::Hero ? 20 : size == Size::Small ? 14 : 16;
    g.setColour (size == Size::Hero ? Palette::text : Palette::label);
    g.setFont (uiFont (size == Size::Hero ? 14.0f : size == Size::Small ? 10.5f : 11.0f, size == Size::Hero, 0.09f));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (titleH), Justification::centred);
}

void Knob::paintOverChildren (Graphics& g)
{
    if (target < 0) return;
    const auto d = dial();
    const float base = (float) slider.valueToProportionOfLength (slider.getValue());
    auto angleOf = [&d] (float n) { return d.start + jlimit (0.0f, 1.0f, n) * (d.end - d.start); };

    if (dragHover)
    {
        g.setColour (Palette::accent.withAlpha (0.25f));
        g.fillEllipse (Rectangle<float> (d.r * 2, d.r * 2).withCentre (d.c).expanded (4.0f));
        g.setColour (Palette::accent);
        g.drawEllipse (Rectangle<float> (d.r * 2, d.r * 2).withCentre (d.c).expanded (3.0f), 2.0f);
    }
    if (! anyMod) return;

    float ringR = d.r + d.track * 0.2f;
    for (int l = 0; l < kNumLfos; ++l)
    {
        if (! proc.hasModulation (target, l)) continue;
        const float depth = proc.getModulationDepth (target, l);
        const auto col = Palette::lfo (l);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (arcPath (d.c, ringR, angleOf (base), angleOf (base + depth)), PathStrokeType (d.track * 1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (arcPath (d.c, ringR, angleOf (base), angleOf (base + depth)), PathStrokeType (d.track * 0.7f, PathStrokeType::curved, PathStrokeType::rounded));
        ringR += d.track * 1.1f;
    }
    const auto p = d.c.getPointOnCircumference (d.r - d.track * 0.5f, angleOf (proc.modNorm[(size_t) target].load()));
    g.setColour (Colours::white.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (p));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (p));
}

void Knob::refreshMods()
{
    if (target < 0) return;
    anyMod = false;
    const auto d = dial();
    const float chip = jlimit (13.0f, 17.0f, d.r * 0.42f);
    int shown = 0;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& c = chips[(size_t) l];
        const bool on = proc.hasModulation (target, l);
        anyMod |= on;
        c->setVisible (on);
        if (! on) continue;
        const float a = MathConstants<float>::pi * (0.22f + 0.22f * (float) shown++);
        auto rect = Rectangle<float> (chip, chip).withCentre (d.c.getPointOnCircumference (d.r + chip * 0.55f, a)).toNearestInt();
        c->setBounds (rect.constrainedWithin (getLocalBounds()));
        c->updateTooltip();
        c->repaint();
    }
    repaint();
}

void Knob::tick() { if (anyMod) repaint(); }

bool Knob::isInterestedInDragSource (const SourceDetails& s) { return target >= 0 && s.description.toString().startsWith ("lfo:"); }
void Knob::itemDragEnter (const SourceDetails&) { dragHover = true;  repaint(); }
void Knob::itemDragExit  (const SourceDetails&) { dragHover = false; repaint(); }

void Knob::itemDropped (const SourceDetails& s)
{
    dragHover = false;
    const int l = s.description.toString().fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
    if (! proc.hasModulation (target, l))
        proc.setModulation (target, l, 0.35f);
    refreshMods();
}

// ============================================================================
// Dice button

DiceButton::DiceButton (const String& text) : Button ("dice"), label (text)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip (text.isEmpty() ? String::fromUTF8 ("Случайный звук для этого блока") : String::fromUTF8 ("Случайный звук для всего плагина"));
    phase = Random::getSystemRandom().nextFloat() * 6.0f;
    startTimerHz (30);
}

void DiceButton::clicked()
{
    flash = 1.0f;
    spin = 1.0f;
}

void DiceButton::timerCallback()
{
    phase += 0.09f;
    flash *= 0.86f;
    spin *= 0.85f;
    repaint();
}

void DiceButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const float breath = 0.5f + 0.5f * std::sin (phase);
    const float rad = r.getHeight() * 0.5f;

    // halo
    const float halo = 0.18f + 0.18f * breath + (over ? 0.2f : 0.0f) + flash * 0.5f;
    g.setColour (Palette::violet.withAlpha (jmin (0.8f, halo)));
    g.fillRoundedRectangle (r.expanded (3.0f + 2.0f * breath + flash * 4.0f), rad + 3.0f);

    // body
    ColourGradient body (Palette::violetHi.interpolatedWith (Colours::white, flash * 0.6f), r.getX(), r.getY(),
                         down ? Palette::violetDeep.darker (0.2f) : Palette::violetDeep, r.getX(), r.getBottom(), false);
    body.addColour (0.45, Palette::violet.interpolatedWith (Colours::white, over ? 0.12f : 0.0f));
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.fillRoundedRectangle (r.reduced (4.0f, 0.0f).withHeight (r.getHeight() * 0.4f).translated (0, 2.0f), rad * 0.6f);

    // dice icon
    const float ds = r.getHeight() * 0.56f;
    const float dx = label.isEmpty() ? r.getCentreX() : r.getX() + rad + 2.0f;
    auto die = Rectangle<float> (ds, ds).withCentre ({ dx, r.getCentreY() });
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (spin * MathConstants<float>::twoPi + 0.15f * std::sin (phase * 0.7f), die.getCentreX(), die.getCentreY()));
    g.setColour (Colours::white);
    g.fillRoundedRectangle (die, ds * 0.22f);
    g.setColour (Palette::violetDeep);
    const float pr = ds * 0.13f;
    static const float dots[5][2] = { { 0.27f, 0.27f }, { 0.73f, 0.27f }, { 0.5f, 0.5f }, { 0.27f, 0.73f }, { 0.73f, 0.73f } };
    for (auto& d : dots)
        g.fillEllipse (Rectangle<float> (pr * 2, pr * 2).withCentre ({ die.getX() + die.getWidth() * d[0], die.getY() + die.getHeight() * d[1] }));
    g.addTransform (AffineTransform::rotation (-(spin * MathConstants<float>::twoPi + 0.15f * std::sin (phase * 0.7f)), die.getCentreX(), die.getCentreY()));

    if (label.isNotEmpty())
    {
        g.setColour (Colours::white);
        g.setFont (uiFont (13.0f, true, 0.1f));
        g.drawText (label, r.withTrimmedLeft (rad * 2.0f + 4.0f), Justification::centredLeft, false);
    }
}

// ============================================================================
// LFO tab

LfoTab::LfoTab (GranulumProcessor& p, int i) : index (i), proc (p)
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
    setTooltip (String::fromUTF8 ("Тяни на любую ручку, чтобы модулировать её"));
}

void LfoTab::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = Palette::lfo (index);
    g.setColour (selected ? Palette::button : Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    if (selected)
    {
        g.setColour (col);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);
    }
    drawLed (g, Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() }), col, col.darker (0.6f), selected ? 1.0f : 0.4f);
    g.setColour (selected ? Colours::white : Palette::text);
    g.setFont (uiFont (13.0f, true, 0.08f));
    g.drawText ("LFO " + String (index + 1), r.withTrimmedLeft (24.0f), Justification::centredLeft, false);
    g.setColour (Palette::textDim.withAlpha (0.8f));
    for (int row = 0; row < 3; ++row)
        for (int c = 0; c < 2; ++c)
            g.fillEllipse (r.getRight() - 15.0f + (float) c * 5.0f, r.getCentreY() - 6.0f + (float) row * 5.0f, 2.2f, 2.2f);
    if (const int n = proc.countModulations (index); n > 0)
    {
        g.setColour (col);
        g.setFont (uiFont (10.5f, true));
        g.drawText (String (n), r.withTrimmedRight (22.0f), Justification::centredRight, false);
    }
}

void LfoTab::mouseDown (const MouseEvent&) { dragging = false; if (onSelect) onSelect (index); }

void LfoTab::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        c->startDragging ("lfo:" + String (index), this);
    }
}

void LfoTab::mouseUp (const MouseEvent&) { dragging = false; }

// ============================================================================
// Editor

GranulumEditor::GranulumEditor (GranulumProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;

    organism = std::make_unique<Organism> (proc);
    addAndMakeVisible (*organism);
    lfoEditor = std::make_unique<LfoEditor> (proc);
    addAndMakeVisible (*lfoEditor);

    auto add = [&] (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* name, Knob::Size sz = Knob::Size::Normal)
    {
        group.push_back (std::make_unique<Knob> (proc, id, name, sz));
        addAndMakeVisible (*group.back());
        allKnobs.push_back (group.back().get());
    };

    add (subKnobs, ParamID::subVol,  "Vol");
    add (subKnobs, ParamID::subOct,  "Oct");
    add (subKnobs, ParamID::subSemi, "Semi");
    add (addKnobs, ParamID::addVol,  "Vol");
    add (addKnobs, ParamID::addOct,  "Oct");
    add (addKnobs, ParamID::addSemi, "Semi");
    add (smpKnobs, ParamID::smpVol,  "Vol");
    add (smpKnobs, ParamID::smpOct,  "Oct");
    add (smpKnobs, ParamID::smpSemi, "Semi");

    add (grainKnobs, ParamID::tone,    "Tone",    Knob::Size::Hero);
    add (grainKnobs, ParamID::texture, "Texture", Knob::Size::Hero);
    add (grainKnobs, ParamID::motion,  "Motion",  Knob::Size::Hero);
    add (grainKnobs, ParamID::amount,  "Amount",  Knob::Size::Hero);

    add (filterKnobs, ParamID::cutoff, "Cutoff");
    add (filterKnobs, ParamID::reso,   "Reso");
    add (dirtKnobs, ParamID::dirtDrive, "Drive");
    add (dirtKnobs, ParamID::dirtTone,  "Tone");
    add (dirtKnobs, ParamID::dirtMix,   "Mix");
    add (spaceKnobs, ParamID::spaceAmt,  "Amount");
    add (spaceKnobs, ParamID::spaceSize, "Size");
    add (voiceKnobs, ParamID::glide, "Glide");
    add (voiceKnobs, ParamID::pitch, "Pitch");

    outputKnob = std::make_unique<Knob> (proc, ParamID::output, "Output", Knob::Size::Small);
    addAndMakeVisible (*outputKnob);
    allKnobs.push_back (outputKnob.get());

    // per-block source toggles
    auto toggle = [&] (TextButton& b, const char* id, int stem)
    {
        b.setClickingTogglesState (true);
        b.getProperties().set ("stem", stem);
        addAndMakeVisible (b);
        toggleAtts.push_back (std::make_unique<ButtonAtt> (s, id, b));
    };
    toggle (grainAddT, ParamID::grainAdd, stemAdd);
    toggle (grainSmpT, ParamID::grainSmp, stemSmp);
    const char* filterIds[3] = { ParamID::filterSub, ParamID::filterAdd, ParamID::filterSmp };
    const char* dirtIds[3]   = { ParamID::dirtSub,   ParamID::dirtAdd,   ParamID::dirtSmp };
    const char* spaceIds[3]  = { ParamID::spaceSub,  ParamID::spaceAdd,  ParamID::spaceSmp };
    for (int i = 0; i < 3; ++i)
    {
        toggle (filterT[i], filterIds[i], i);
        toggle (dirtT[i],   dirtIds[i],   i);
        toggle (spaceT[i],  spaceIds[i],  i);
    }
    grainAddT.setTooltip (String::fromUTF8 ("Гранулировать ADD"));
    grainSmpT.setTooltip (String::fromUTF8 ("Гранулировать SAMPLER"));

    filterType.addItemList ({ "LOW PASS", "BAND PASS", "HIGH PASS" }, 1);
    dirtType.addItemList (Distortion::typeNames(), 1);
    addAndMakeVisible (filterType);
    addAndMakeVisible (dirtType);
    filterTypeA = std::make_unique<ComboAtt> (s, ParamID::ftype, filterType);
    dirtTypeA   = std::make_unique<ComboAtt> (s, ParamID::dirtType, dirtType);

    // dice
    for (auto* d : { &diceAll, &diceBars, &diceGrain, &diceFilter, &diceDirt, &diceSpace }) addAndMakeVisible (*d);
    diceAll.onClick    = [this] { randomizeAll(); };
    diceBars.onClick   = [this] { randomizeSection (0); };
    diceGrain.onClick  = [this] { randomizeSection (1); };
    diceFilter.onClick = [this] { randomizeSection (2); };
    diceDirt.onClick   = [this] { randomizeSection (3); };
    diceSpace.onClick  = [this] { randomizeSection (4); };

    // sampler
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this] { launchChooser(); };
    for (auto* b : { &rootDown, &rootUp }) addAndMakeVisible (*b);
    rootDown.onClick = [this] { proc.shiftRoot (-1); };
    rootUp.onClick   = [this] { proc.shiftRoot (1); };
    rootDown.setTooltip (String::fromUTF8 ("Если сэмпл определился не той нотой, поправь здесь"));
    rootUp.setTooltip (rootDown.getTooltip());
    for (auto* l : { &sampleName, &rootLabel })
    {
        l->setInterceptsMouseClicks (false, false);
        l->setJustificationType (Justification::centredLeft);
        addAndMakeVisible (*l);
    }
    sampleName.setFont (uiFont (13.0f, false, 0.03f));
    sampleName.setColour (Label::textColourId, Palette::text);
    rootLabel.setFont (uiFont (14.0f, true, 0.06f));
    rootLabel.setJustificationType (Justification::centred);
    rootLabel.setColour (Label::textColourId, Palette::violetHi);

    // voice
    for (auto* b : { &polyButton, &monoButton }) addAndMakeVisible (*b);
    polyButton.onClick = [this] { if (auto* pm = proc.apvts.getParameter (ParamID::mono)) pm->setValueNotifyingHost (0.0f); };
    monoButton.onClick = [this] { if (auto* pm = proc.apvts.getParameter (ParamID::mono)) pm->setValueNotifyingHost (1.0f); };

    // LFOs
    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l] = std::make_unique<LfoTab> (proc, l);
        lfoTabs[(size_t) l]->onSelect = [this] (int i) { selectLfo (i); };
        addAndMakeVisible (*lfoTabs[(size_t) l]);

        auto c = std::make_unique<LfoControls>();
        c->div.addItemList (GranulumProcessor::divisionNames(), 1);
        c->mode.addItemList ({ "FREE", "RETRIG" }, 1);
        c->sync.setClickingTogglesState (true);
        c->sync.setTooltip (String::fromUTF8 ("Синхронизация с темпом проекта"));
        c->mode.setTooltip (String::fromUTF8 ("RETRIG: LFO начинается заново с каждой ноты"));
        c->rate = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rate"), "Rate", Knob::Size::Small);
        allKnobs.push_back (c->rate.get());
        c->divA  = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Div"), c->div);
        c->modeA = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Mode"), c->mode);
        c->syncA = std::make_unique<ButtonAtt> (s, ParamID::lfo (l, "Sync"), c->sync);
        for (Component* comp : { (Component*) &c->div, (Component*) &c->mode, (Component*) &c->sync, (Component*) c->rate.get() })
            addChildComponent (comp);
        lfoControls[(size_t) l] = std::move (c);
    }
    lfoPreset.addItemList (GranulumProcessor::lfoPresetNames(), 1);
    lfoPreset.setTextWhenNothingSelected ("SHAPE...");
    lfoPreset.onChange = [this]
    {
        if (const int id = lfoPreset.getSelectedId(); id > 0)
        {
            proc.setLfoPoints (selectedLfo, GranulumProcessor::lfoPreset (id - 1));
            lfoPreset.setSelectedId (0, dontSendNotification);
        }
    };
    lfoGrid.addItemList ({ "GRID OFF", "GRID 4", "GRID 8", "GRID 16" }, 1);
    lfoGrid.setSelectedId (3, dontSendNotification);
    lfoGrid.onChange = [this] { const int id = lfoGrid.getSelectedId(); lfoEditor->grid = id == 1 ? 0 : id == 2 ? 4 : id == 3 ? 8 : 16; };
    addAndMakeVisible (lfoPreset);
    addAndMakeVisible (lfoGrid);

    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Palette::whiteKey);
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Palette::blackKey);
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Palette::blackKey.withAlpha (0.6f));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent.withAlpha (0.85f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.25f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keyboard.setColour (MidiKeyboardComponent::textLabelColourId, Palette::blackKey);
    keyboard.setAvailableRange (24, 84);
    keyboard.setLowestVisibleKey (24);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    selectLfo (0);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (1100.0 / 860.0);
    setResizeLimits (880, 688, 1650, 1290);
    setSize (1100, 860);
    startTimerHz (40);
    timerCallback();
}

GranulumEditor::~GranulumEditor()
{
    setLookAndFeel (nullptr);
}

void GranulumEditor::selectLfo (int i)
{
    selectedLfo = i;
    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l]->selected = (l == i);
        lfoTabs[(size_t) l]->repaint();
        auto& c = *lfoControls[(size_t) l];
        for (Component* comp : { (Component*) &c.div, (Component*) &c.mode, (Component*) &c.sync, (Component*) c.rate.get() })
            comp->setVisible (l == i);
    }
    lfoEditor->lfo = i;
    lfoEditor->repaint();
}

void GranulumEditor::launchChooser()
{
    chooser = std::make_unique<FileChooser> ("Load a sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) openFile (f);
                          });
}

void GranulumEditor::openFile (const File& f)
{
    String err;
    statusText = proc.loadSample (f, err) ? String() : err;
}

// ---- smart random with smooth glides ----

void GranulumEditor::randomizeSection (int section)
{
    // finish any running glide first
    for (auto& gl : glides) { gl.param->setValueNotifyingHost (gl.to); gl.param->endChangeGesture(); }
    glides.clear();

    if (section == 0)
    {
        for (int i = 0; i < granulum::kNumHarmonics; ++i) barsFrom[(size_t) i] = proc.getHarmonic (i);
        barsTo = proc.makeRandomBars();
        barsGliding = true;
    }
    else
    {
        for (auto& t : proc.makeRandom (section))
            if (auto* p = proc.apvts.getParameter (t.id))
            {
                if (dynamic_cast<AudioParameterChoice*> (p) != nullptr)
                {
                    p->beginChangeGesture();
                    p->setValueNotifyingHost (t.norm);
                    p->endChangeGesture();
                    continue;
                }
                p->beginChangeGesture();
                glides.push_back ({ p, p->getValue(), t.norm });
            }
    }
    glideT = 0.0f;
}

void GranulumEditor::randomizeAll()
{
    randomizeSection (0);
    const bool bars = barsGliding;
    for (int s = 1; s <= 4; ++s)
        for (auto& t : proc.makeRandom (s))
            if (auto* p = proc.apvts.getParameter (t.id))
            {
                p->beginChangeGesture();
                if (dynamic_cast<AudioParameterChoice*> (p) != nullptr) { p->setValueNotifyingHost (t.norm); p->endChangeGesture(); continue; }
                glides.push_back ({ p, p->getValue(), t.norm });
            }
    barsGliding = bars;
    glideT = 0.0f;
}

void GranulumEditor::timerCallback()
{
    // random glides (~0.25 s, ease-out)
    if (glideT < 1.0f)
    {
        glideT = jmin (1.0f, glideT + 1.0f / (0.25f * 40.0f));
        const float e = 1.0f - std::pow (1.0f - glideT, 3.0f);
        for (auto& gl : glides) gl.param->setValueNotifyingHost (gl.from + (gl.to - gl.from) * e);
        if (barsGliding)
            for (int i = 0; i < granulum::kNumHarmonics; ++i)
                proc.setHarmonic (i, barsFrom[(size_t) i] + (barsTo[(size_t) i] - barsFrom[(size_t) i]) * e);
        if (glideT >= 1.0f)
        {
            for (auto& gl : glides) gl.param->endChangeGesture();
            glides.clear();
            barsGliding = false;
        }
    }

    // sampler info
    const auto name = proc.getSampleName();
    const String text = statusText.isNotEmpty() ? "ERROR: " + statusText
                      : name.isEmpty() ? String::fromUTF8 ("Перетащи WAV на экран или нажми LOAD") : name;
    if (sampleName.getText() != text) sampleName.setText (text, dontSendNotification);
    const String root = ! proc.hasSample() ? String ("--")
                      : MidiMessage::getMidiNoteName (roundToInt (proc.getRootNote()), true, true, 3) + (proc.isRootDetected() ? "" : " ?");
    if (rootLabel.getText() != root) rootLabel.setText (root, dontSendNotification);

    const bool mono = proc.apvts.getRawParameterValue (ParamID::mono)->load() > 0.5f;
    polyButton.setToggleState (! mono, dontSendNotification);
    monoButton.setToggleState (mono, dontSendNotification);

    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& c = *lfoControls[(size_t) l];
        const bool sync = c.sync.getToggleState();
        c.rate->setEnabled (! sync);
        c.rate->setAlpha (sync ? 0.35f : 1.0f);
        c.div.setEnabled (sync);
    }

    if (const auto v = proc.modVersion.load(); v != shownModVersion)
    {
        shownModVersion = v;
        for (auto* k : allKnobs) k->refreshMods();
        for (auto& t : lfoTabs) t->repaint();
    }
    for (auto* k : allKnobs) k->tick();

    meter = jmax (proc.outputLevel.load(), meter * 0.85f);
    repaint (gemsArea.expanded (20));
}

void GranulumEditor::drawCard (Graphics& g, Rectangle<int> area, const String& title, Colour tag)
{
    auto r = area.toFloat();
    g.setColour (Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (r.translated (0, 2.0f), 12.0f);
    g.setColour (Palette::card);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    drawLed (g, Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 18.0f, r.getY() + 19.0f }), tag, tag.darker (0.5f), 1.0f);
    g.setColour (Palette::text);
    g.setFont (uiFont (14.0f, true, 0.1f));
    g.drawText (title, (int) r.getX() + 30, (int) r.getY() + 8, 200, 22, Justification::centredLeft, false);
}

void GranulumEditor::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const float sc = full.getWidth() / 1100.0f;
    g.setGradientFill (ColourGradient (Palette::bgTop, 0, 0, Palette::bgBottom, 0, full.getHeight(), false));
    g.fillAll();

    g.setColour (Palette::text);
    g.setFont (uiFont (36.0f * sc, false, 0.05f));
    g.drawText ("GRANULUM", titleArea.withHeight ((int) (40 * sc)), Justification::bottomLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (14.0f * sc, false, 0.12f));
    g.drawText ("BASS GRANULIZER", titleArea.withTrimmedTop ((int) (42 * sc)), Justification::topLeft);

    // output meter
    const float db = Decibels::gainToDecibels (meter, -60.0f);
    const float thresholds[6] = { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, -2.0f };
    const float gw = (float) gemsArea.getWidth() / 3.0f, gh = (float) gemsArea.getHeight() / 2.0f;
    const float gs = jmin (gw, gh) * 0.68f;
    for (int i = 0; i < 6; ++i)
    {
        const float cx = (float) gemsArea.getX() + gw * ((float) (i % 3) + 0.5f);
        const float cy = (float) gemsArea.getY() + gh * ((float) (i / 3) + 0.5f);
        auto c = Rectangle<float> (cx - gs * 0.5f, cy - gs * 0.5f, gs, gs);
        g.setColour (Colour (0xff10303b));
        g.fillEllipse (c.expanded (gs * 0.14f));
        drawLed (g, c, Palette::violet, Palette::purpleOff, jlimit (0.0f, 1.0f, (db - thresholds[i]) / 6.0f + 0.5f));
    }

    drawCard (g, subCard, "SUB", Palette::subBlue);
    drawCard (g, addCard, "ADD", Palette::accent);
    drawCard (g, smpCard, "SAMPLER", Palette::violetHi);
    drawCard (g, grainCard, "GRAIN", Palette::violet);
    drawCard (g, filterCard, "FILTER", Palette::accent);
    drawCard (g, dirtCard, "DIRT", Palette::hot);
    drawCard (g, spaceCard, "SPACE", Palette::accentSoft);
    drawCard (g, lfoCard, "", Palette::lfo (selectedLfo));
    drawCard (g, voiceCard, "VOICE", Palette::accent);

    g.setColour (Palette::label);
    g.setFont (uiFont (10.5f, false, 0.08f));
    g.drawText ("TYPE", filterTypeLabel, Justification::centredLeft, false);
    g.drawText ("TYPE", dirtTypeLabel, Justification::centred, false);
    g.drawText ("SHAPE", lfoPresetLabel, Justification::centredLeft, false);
    g.drawText ("DRAW", lfoGridLabel, Justification::centredLeft, false);
    g.drawText ("RATE", lfoDivLabel, Justification::centredLeft, false);
    g.drawText ("MODE", lfoModeLabel, Justification::centredLeft, false);
    g.drawText ("ROOT", rootTitle, Justification::centredLeft, false);

    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (keyboard.getBounds().toFloat().expanded (3.0f), 6.0f);
}

void GranulumEditor::resized()
{
    const float sc = (float) getWidth() / 1100.0f;
    auto S = [sc] (float v) { return (int) std::round (v * sc); };

    auto r = getLocalBounds().reduced (S (16));

    // header
    auto header = r.removeFromTop (S (54));
    gemsArea = header.removeFromRight (S (96)).withSizeKeepingCentre (S (96), S (50));
    header.removeFromRight (S (10));
    outputKnob->setBounds (header.removeFromRight (S (66)));
    header.removeFromRight (S (14));
    diceAll.setBounds (header.removeFromRight (S (160)).withSizeKeepingCentre (S (160), S (40)));
    titleArea = header.withTrimmedLeft (S (6));
    r.removeFromTop (S (8));

    keyboard.setBounds (r.removeFromBottom (S (48)).reduced (S (2), S (2)));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    keyboard.setLowestVisibleKey (24);
    r.removeFromBottom (S (10));

    auto bottom = r.removeFromBottom (S (104));
    r.removeFromBottom (S (10));
    auto blocks = r.removeFromBottom (S (196));
    r.removeFromBottom (S (10));
    auto sources = r.removeFromBottom (S (112));
    r.removeFromBottom (S (10));
    screenArea = r;
    organism->setBounds (screenArea);

    const int gap = S (10);
    auto knobRow = [] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> row)
    {
        const int cw = row.getWidth() / (int) ks.size();
        for (int i = 0; i < (int) ks.size(); ++i)
            ks[(size_t) i]->setBounds (row.getX() + i * cw, row.getY(), cw, row.getHeight());
    };
    auto cardBody = [&] (Rectangle<int> card) { return card.reduced (S (10), S (8)).withTrimmedTop (S (26)); };

    // ---- sources ----
    subCard = sources.removeFromLeft (S (250)); sources.removeFromLeft (gap);
    addCard = sources.removeFromLeft (S (300)); sources.removeFromLeft (gap);
    smpCard = sources;
    knobRow (subKnobs, cardBody (subCard));
    knobRow (addKnobs, cardBody (addCard).withTrimmedRight (S (4)));
    diceBars.setBounds (addCard.getRight() - S (54), addCard.getY() + S (5), S (46), S (28));
    {
        loadButton.setBounds (smpCard.getRight() - S (82), smpCard.getY() + S (6), S (72), S (26));
        auto body = cardBody (smpCard);
        auto knobs = body.removeFromRight (S (220));
        knobRow (smpKnobs, knobs);
        body.removeFromRight (S (10));
        sampleName.setBounds (body.removeFromTop (S (26)));
        body.removeFromTop (S (6));
        auto rootRow = body.removeFromTop (S (28));
        rootTitle = rootRow.removeFromLeft (S (42));
        rootDown.setBounds (rootRow.removeFromLeft (S (28)));
        rootLabel.setBounds (rootRow.removeFromLeft (S (64)));
        rootUp.setBounds (rootRow.removeFromLeft (S (28)));
    }

    // ---- blocks ----
    grainCard  = blocks.removeFromLeft (S (360)); blocks.removeFromLeft (gap);
    filterCard = blocks.removeFromLeft (S (206)); blocks.removeFromLeft (gap);
    dirtCard   = blocks.removeFromLeft (S (296)); blocks.removeFromLeft (gap);
    spaceCard  = blocks;

    auto blockDice = [&] (DiceButton& d, Rectangle<int> card) { d.setBounds (card.getRight() - S (54), card.getY() + S (5), S (46), S (28)); };
    blockDice (diceGrain, grainCard);
    blockDice (diceFilter, filterCard);
    blockDice (diceDirt, dirtCard);
    blockDice (diceSpace, spaceCard);

    auto toggles = [&] (Rectangle<int> row, std::initializer_list<TextButton*> bs)
    {
        const int w = jmin (S (54), row.getWidth() / (int) bs.size());
        int x = row.getX();
        for (auto* b : bs) { b->setBounds (x, row.getY(), w - S (4), S (22)); x += w; }
    };
    {
        auto b = cardBody (grainCard);
        toggles (b.removeFromTop (S (24)), { &grainAddT, &grainSmpT });
        b.removeFromTop (S (6));
        knobRow (grainKnobs, b);
    }
    {
        auto b = cardBody (filterCard);
        toggles (b.removeFromTop (S (24)), { &filterT[0], &filterT[1], &filterT[2] });
        b.removeFromTop (S (6));
        auto typeRow = b.removeFromBottom (S (28));
        filterTypeLabel = typeRow.removeFromLeft (S (38));
        filterType.setBounds (typeRow);
        b.removeFromBottom (S (4));
        knobRow (filterKnobs, b);
    }
    {
        auto b = cardBody (dirtCard);
        toggles (b.removeFromTop (S (24)), { &dirtT[0], &dirtT[1], &dirtT[2] });
        b.removeFromTop (S (6));
        auto typeCol = b.removeFromLeft (b.getWidth() / 4 + S (10));
        dirtTypeLabel = typeCol.withHeight (S (16)).translated (0, typeCol.getHeight() / 2 - S (26));
        dirtType.setBounds (typeCol.withSizeKeepingCentre (typeCol.getWidth() - S (4), S (28)));
        knobRow (dirtKnobs, b);
    }
    {
        auto b = cardBody (spaceCard);
        toggles (b.removeFromTop (S (24)), { &spaceT[0], &spaceT[1], &spaceT[2] });
        b.removeFromTop (S (6));
        knobRow (spaceKnobs, b);
    }

    // ---- LFO + voice ----
    lfoCard = bottom.removeFromLeft (S (780));
    bottom.removeFromLeft (gap);
    voiceCard = bottom;
    {
        auto l = lfoCard.reduced (S (12), S (10));
        auto tabs = l.removeFromLeft (S (96));
        const int th = (tabs.getHeight() - S (8)) / kNumLfos;
        for (int i = 0; i < kNumLfos; ++i)
            lfoTabs[(size_t) i]->setBounds (tabs.getX(), tabs.getY() + i * (th + S (8)), tabs.getWidth(), th);
        l.removeFromLeft (S (10));
        lfoEditor->setBounds (l.removeFromLeft (S (300)));
        l.removeFromLeft (S (12));

        auto col1 = l.removeFromLeft (S (122));
        lfoPresetLabel = col1.removeFromTop (S (14));
        lfoPreset.setBounds (col1.removeFromTop (S (28)));
        col1.removeFromTop (S (4));
        lfoGridLabel = col1.removeFromTop (S (14));
        lfoGrid.setBounds (col1.removeFromTop (S (28)));
        l.removeFromLeft (S (10));

        auto col2 = l.removeFromLeft (S (122));
        lfoDivLabel = col2.removeFromTop (S (14));
        auto divRow = col2.removeFromTop (S (28));
        col2.removeFromTop (S (4));
        lfoModeLabel = col2.removeFromTop (S (14));
        auto modeRow = col2.removeFromTop (S (28));
        l.removeFromLeft (S (8));
        for (int i = 0; i < kNumLfos; ++i)
        {
            auto& c = *lfoControls[(size_t) i];
            auto dr = divRow;
            c.sync.setBounds (dr.removeFromLeft (S (50)));
            dr.removeFromLeft (S (4));
            c.div.setBounds (dr);
            c.mode.setBounds (modeRow);
            c.rate->setBounds (l);
        }
    }
    {
        auto v = voiceCard.reduced (S (10), S (8)).withTrimmedTop (S (26));
        auto left = v.removeFromLeft (S (110));
        polyButton.setBounds (left.removeFromTop (S (28)).reduced (0, S (1)));
        left.removeFromTop (S (6));
        monoButton.setBounds (left.removeFromTop (S (28)).reduced (0, S (1)));
        v.removeFromLeft (S (8));
        knobRow (voiceKnobs, v);
    }
}

// ----------------------------------------------------------------------------

bool GranulumEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (GranulumProcessor::isSupportedAudioFile (f)) return true;
    return false;
}

void GranulumEditor::fileDragEnter (const StringArray&, int, int) { organism->fileHover = true; }
void GranulumEditor::fileDragExit  (const StringArray&)           { organism->fileHover = false; }

void GranulumEditor::filesDropped (const StringArray& files, int, int)
{
    organism->fileHover = false;
    for (auto& f : files)
        if (GranulumProcessor::isSupportedAudioFile (f)) { openFile (File (f)); break; }
}
