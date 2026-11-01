#include "PluginEditor.h"

using namespace juce;

namespace
{
    Font uiFont (float size, bool bold = false, float kerning = 0.0f)
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
            g.setGradientFill (ColourGradient (on.withAlpha (0.45f * level), ctr.x, ctr.y,
                                               on.withAlpha (0.0f), ctr.x + r * 2.6f, ctr.y, true));
            g.fillEllipse (c.expanded (r * 1.6f));
        }
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.expanded (1.5f));
        g.setColour (off.interpolatedWith (on, level));
        g.fillEllipse (c);
        g.setColour (Colours::white.withAlpha (0.10f + 0.25f * level));
        g.fillEllipse (ctr.x - r * 0.5f, ctr.y - r * 0.65f, r, r * 0.6f);
    }

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
    if (flatKnobs)
    {
        // flat, quiet knob: a plain disc with one pointer line
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start);
        const float br = r * 0.78f;
        g.setColour (s.isMouseOverOrDragging() ? Palette::knob.brighter (0.12f) : Palette::knob);
        g.fillEllipse (Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (c));
        const auto p1 = c.getPointOnCircumference (br * 0.12f, angle);
        const auto p2 = c.getPointOnCircumference (br * 0.78f, angle);
        g.setColour (Palette::track);
        g.drawLine ({ p1, p2 }, jmax (1.6f, br * 0.09f));
        return;
    }
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
    g.setGradientFill (ColourGradient (Palette::knob.brighter (0.12f), c.x, body.getY(),
                                       Palette::knobDark, c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawEllipse (body.reduced (0.6f), 0.8f);

    const auto p1 = c.getPointOnCircumference (br * 0.15f, angle);
    const auto p2 = c.getPointOnCircumference (br * 0.85f, angle);
    g.setColour (Palette::track);
    g.drawLine ({ p1, p2 }, jmax (2.0f, br * 0.09f));
}

void GranulumLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float rad = 6.0f;
    if (b.getToggleState())
    {
        switch (toggleStyle)
        {
            case 1:   // muted teal plate
                g.setColour (Colour (0xff2f8a93).interpolatedWith (Colours::white, over ? 0.08f : 0.0f));
                g.fillRoundedRectangle (r, rad);
                g.setColour (Colours::white.withAlpha (0.10f));
                g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
                return;
            case 2:   // dark plate + thin teal outline
                g.setColour (Palette::screen.withAlpha (0.85f));
                g.fillRoundedRectangle (r, rad);
                g.setColour (Palette::accent.withAlpha (over ? 0.95f : 0.75f));
                g.drawRoundedRectangle (r.reduced (0.6f), rad, 1.2f);
                return;
            case 3:   // frosted: lighter grey-teal plate, soft inner light
                g.setGradientFill (ColourGradient (Colour (0xff5d8f99), 0, r.getY(), Colour (0xff4a7a85), 0, r.getBottom(), false));
                g.fillRoundedRectangle (r, rad);
                g.setColour (Palette::ice.withAlpha (0.35f));
                g.drawRoundedRectangle (r.reduced (0.6f), rad, 1.0f);
                return;
            case 4:   // deep petrol with a quiet teal glow line underneath
                g.setColour (Colour (0xff24606b));
                g.fillRoundedRectangle (r, rad);
                g.setColour (Palette::accent.withAlpha (0.7f));
                g.fillRoundedRectangle (r.withTop (r.getBottom() - 2.0f).reduced (rad, 0.0f), 1.0f);
                return;
            case 5:   // between bright and muted, closer to muted
            case 6:
            {
                const auto c = Colour (0xff2f8a93).interpolatedWith (Palette::accent, toggleStyle == 5 ? 0.30f : 0.42f);
                g.setColour (c.withAlpha (0.18f));
                g.fillRoundedRectangle (r.expanded (1.5f), rad + 1.5f);
                g.setColour (c.interpolatedWith (Colours::white, over ? 0.08f : 0.0f));
                g.fillRoundedRectangle (r, rad);
                g.setColour (Colours::white.withAlpha (0.12f));
                g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
                return;
            }
            default:
                g.setColour (Palette::accent.withAlpha (0.22f));
                g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
                g.setColour (Palette::accent.interpolatedWith (Colours::white, over ? 0.2f : 0.0f));
                g.fillRoundedRectangle (r, rad);
                return;
        }
    }
    g.setColour (down ? Palette::button.darker (0.2f) : over ? Palette::buttonHi : Palette::button);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

void GranulumLook::drawButtonText (Graphics& g, TextButton& b, bool over, bool)
{
    g.setFont (uiFont (b.getHeight() < 30 ? 11.5f : 13.5f, false, 0.08f));
    const auto onText = toggleStyle == 0 ? Palette::screen : toggleStyle == 2 ? Palette::accent.brighter (0.2f)
                      : toggleStyle >= 5 ? Colours::white : Palette::ice;
    g.setColour (b.getToggleState() ? onText : (over ? Colours::white : Palette::text));
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

void GranulumLook::drawCallOutBoxBackground (CallOutBox&, Graphics& g, const Path& path, Image&)
{
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillPath (path, AffineTransform::translation (0.0f, 3.0f));
    g.setColour (Palette::panel);
    g.fillPath (path);
    g.setColour (Palette::accent.withAlpha (0.35f));
    g.strokePath (path, PathStrokeType (1.2f));
}

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
                + "Drag up/down: depth. Double-click: remove.");
}

void ModChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = Palette::lfo (lfo).withMultipliedAlpha (proc.isLfoOn (lfo) ? 1.0f : 0.35f);
    const float d = proc.getModulationDepth (owner.target, lfo);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (r.expanded (1.0f));
    g.setColour (Palette::screen);
    g.fillEllipse (r);
    // depth ring
    const auto c = r.getCentre();
    const float rad = r.getWidth() * 0.5f - 1.5f;
    const float sweep = d * MathConstants<float>::twoPi * 0.999f;
    g.setColour (col.withAlpha (0.25f));
    g.drawEllipse (r.reduced (1.5f), 1.5f);
    g.setColour (col);
    g.strokePath (arcPath (c, rad, 0.0f, sweep), PathStrokeType (2.2f));
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
    const float d = jlimit (-1.0f, 1.0f, startDepth - (float) e.getDistanceFromDragStartY() / sens);
    proc.setModulation (owner.target, lfo, d);
    updateTooltip();
    owner.repaint();
}

void ModChip::mouseDoubleClick (const MouseEvent&)
{
    proc.removeModulation (owner.target, lfo);
}

// ============================================================================
// Knob

Knob::Knob (GranulumProcessor& p, const String& paramId, const String& t, Size sz)
    : target (GranulumProcessor::modIndexOf (paramId)), proc (p), title (t), size (sz)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    auto* param = proc.apvts.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, slider);

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
    const int titleH = size == Size::Hero ? 20 : size == Size::Small ? 13 : 16;
    const int boxH   = size == Size::Hero ? 20 : size == Size::Small ? 15 : 18;
    auto r = getLocalBounds();
    r.removeFromTop (titleH);
    if (compact) slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    else         slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (getWidth() - 2, 90), boxH);
    slider.setBounds (r);
    refreshMods();
}

void Knob::paint (Graphics& g)
{
    const int titleH = size == Size::Hero ? 20 : size == Size::Small ? 13 : 16;
    g.setColour (size == Size::Hero ? Palette::text : Palette::label);
    g.setFont (uiFont (size == Size::Hero ? 13.5f : size == Size::Small ? 10.0f : 11.0f, size == Size::Hero, 0.08f));
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

    // modulation ranges just outside the value ring
    float ringR = d.r + d.track * 0.2f;
    for (int l = 0; l < kNumLfos; ++l)
    {
        if (! proc.hasModulation (target, l)) continue;
        const float depth = proc.getModulationDepth (target, l);
        const auto col = Palette::lfo (l).withMultipliedAlpha (proc.isLfoOn (l) ? 1.0f : 0.3f);
        g.setColour (col.withAlpha (0.25f));
        g.strokePath (arcPath (d.c, ringR, angleOf (base), angleOf (base + depth)), PathStrokeType (d.track * 1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (arcPath (d.c, ringR, angleOf (base), angleOf (base + depth)), PathStrokeType (d.track * 0.7f, PathStrokeType::curved, PathStrokeType::rounded));
        ringR += d.track * 1.1f;
    }

    // live modulated value
    const float live = proc.modNorm[(size_t) target].load();
    const auto p = d.c.getPointOnCircumference (d.r - d.track * 0.5f, angleOf (live));
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
        // chips sit outside the ring, at the upper right
        const float a = MathConstants<float>::pi * (0.22f + 0.22f * (float) shown++);
        auto pos = d.c.getPointOnCircumference (d.r + chip * 0.55f, a);
        auto rect = Rectangle<float> (chip, chip).withCentre (pos).toNearestInt();
        rect = rect.constrainedWithin (getLocalBounds());
        c->setBounds (rect);
        c->updateTooltip();
        c->repaint();
    }
    repaint();
}

void Knob::tick()
{
    if (anyMod) repaint();
}

bool Knob::isInterestedInDragSource (const SourceDetails& s)
{
    return target >= 0 && s.description.toString().startsWith ("lfo:");
}

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
// GRAIN FIELD — glowing waveform + crystalline shards

namespace
{
    Colour shardColour (float tint)   // 0 deep teal .. 1 icy white
    {
        const Colour a = Palette::accentSoft.darker (0.35f), b = Palette::accent, c = Palette::ice;
        return tint < 0.5f ? a.interpolatedWith (b, tint * 2.0f) : b.interpolatedWith (c, (tint - 0.5f) * 2.0f);
    }
}

GrainField::GrainField (GranulumProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setOpaque (true);
    shards.reserve (520);
    for (int i = 0; i < 46; ++i)
        motes.push_back ({ rng.nextFloat(), rng.nextFloat() * 2.0f - 1.0f,
                           (rng.nextFloat() - 0.5f) * 0.01f, (rng.nextFloat() - 0.5f) * 0.02f,
                           rng.nextFloat() * 6.28f, 0.6f + rng.nextFloat() * 1.4f });
    lastTick = Time::getMillisecondCounterHiRes();
    startTimerHz (50);
}

Rectangle<float> GrainField::inner() const
{
    return getLocalBounds().toFloat().reduced (16.0f, 18.0f);
}

void GrainField::resized()
{
    const int cols = jmax (2, (int) inner().getWidth() / 3);
    heat.assign ((size_t) cols, 0.0f);
    env.assign ((size_t) cols, 0.0f);
    lastPeaks = nullptr;
}

void GrainField::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jlimit (0.005, 0.06, (now - lastTick) * 0.001);
    lastTick = now;
    simulate (dt);
    repaint();
}

void GrainField::simulate (float dt)
{
    time += dt;
    auto& s = proc.apvts;
    const auto mv = [this] (int t) { return proc.modNorm[(size_t) t].load (std::memory_order_relaxed); };
    const float bloom = mv (mtBloom), smear = mv (mtSmear), scatter = mv (mtScatter), shimmer = mv (mtShimmer), drift = mv (mtDrift);
    const bool grains = s.getRawParameterValue (ParamID::grainOn)->load() > 0.5f;

    // ---- envelope of the shown source ----
    auto peaks = proc.getPeaks();
    if (peaks != lastPeaks && ! env.empty())
    {
        lastPeaks = peaks;
        const int cols = (int) env.size();
        std::vector<float> raw ((size_t) cols, 0.0f);
        if (peaks != nullptr && ! peaks->maxs.empty())
        {
            const int bins = (int) peaks->maxs.size();
            for (int x = 0; x < cols; ++x)
            {
                const int b0 = x * bins / cols, b1 = jmax (b0 + 1, (x + 1) * bins / cols);
                float m = 0.0f;
                for (int b = b0; b < b1 && b < bins; ++b) m = jmax (m, peaks->maxs[(size_t) b], -peaks->mins[(size_t) b]);
                raw[(size_t) x] = m;
            }
        }
        for (int x = 0; x < cols; ++x)
        {
            float acc = 0, w = 0;
            for (int k = -3; k <= 3; ++k)
            {
                const float kw = 1.0f - std::abs ((float) k) / 4.0f;
                acc += raw[(size_t) jlimit (0, cols - 1, x + k)] * kw; w += kw;
            }
            env[(size_t) x] = acc / w;
        }
    }
    auto envAt = [this] (float xn)
    {
        if (env.empty()) return 0.0f;
        return env[(size_t) jlimit (0, (int) env.size() - 1, (int) (xn * (float) env.size()))];
    };

    // ---- read heads ----
    auto& eng = proc.vizEngine();
    {
        const int h = eng.vizHeadCount.load (std::memory_order_acquire);
        heads.resize ((size_t) h);
        for (int i = 0; i < h; ++i) heads[(size_t) i] = eng.vizHead[(size_t) i].load (std::memory_order_relaxed);
    }

    // ---- new grains -> shards ----
    const int which = proc.showingSample() ? 1 : 0;
    granulum::GrainEngine* engines[2] = { &proc.engineAdd, &proc.engineSmp };
    for (int e = 0; e < 2; ++e)
    {
        auto& en = *engines[e];
        const uint32_t w = en.eventWrite.load (std::memory_order_acquire);
        uint32_t r = lastEvent[e];
        if (w - r > (uint32_t) granulum::GrainEngine::kEventRing) r = w - (uint32_t) granulum::GrainEngine::kEventRing;
        if (e != which) { lastEvent[e] = w; continue; }
        int budget = 40;
        for (; r != w && budget > 0; ++r, --budget)
        {
            const auto ev = en.events[(size_t) (r % granulum::GrainEngine::kEventRing)];
            if (shards.size() >= 500) shards.erase (shards.begin());
            Shard sh;
            sh.x = jlimit (0.0f, 1.0f, ev.pos);
            const float a = jmax (0.08f, envAt (sh.x));
            sh.y = (rng.nextFloat() * 2.0f - 1.0f) * a * 0.9f;
            const float ang = rng.nextFloat() * MathConstants<float>::twoPi;
            const float sp = 0.05f + 0.35f * scatter + 0.12f * bloom + rng.nextFloat() * 0.08f;
            sh.vx = std::cos (ang) * sp * 0.35f + ev.pan * (0.04f + 0.25f * scatter);
            sh.vy = std::sin (ang) * sp + (sh.y >= 0 ? 1.0f : -1.0f) * 0.05f;
            sh.len   = (0.010f + 0.035f * std::sqrt (jlimit (0.0f, 2.5f, ev.len))) * (1.0f + 2.8f * smear);
            sh.width = 0.0035f + 0.004f * rng.nextFloat() + 0.004f * bloom;
            sh.life  = (0.6f + 1.4f * rng.nextFloat()) * (1.0f + 0.9f * smear + 0.5f * bloom);
            sh.spin  = (rng.nextFloat() - 0.5f) * (1.0f + 6.0f * drift);
            sh.angle = std::atan2 (sh.vy, sh.vx);
            sh.kind  = ev.kind;
            sh.tint  = ev.kind == 1 ? 1.0f : jlimit (0.0f, 0.85f, 0.15f + 0.5f * rng.nextFloat() + 0.25f * shimmer);
            sh.size  = 0.7f + 0.6f * rng.nextFloat() + 0.5f * bloom;
            shards.push_back (sh);
            if (! heat.empty())
            {
                const int c = jlimit (0, (int) heat.size() - 1, (int) (sh.x * (float) heat.size()));
                for (int k = -2; k <= 2; ++k)
                {
                    auto& hc = heat[(size_t) jlimit (0, (int) heat.size() - 1, c + k)];
                    hc = jmin (1.5f, hc + 0.22f * (1.0f - std::abs ((float) k) * 0.3f));
                }
            }
        }
        lastEvent[e] = r;
    }

    // ---- motion ----
    const float aspect = jmax (0.2f, inner().getHeight() / jmax (1.0f, inner().getWidth()));
    const float drag = std::exp (-dt * (1.2f - 0.7f * smear));
    for (auto& sh : shards)
    {
        sh.age += dt;
        // DRIFT: swirling flow field
        const float fx = std::sin (sh.y * 3.1f + time * 0.9f + sh.x * 5.0f);
        const float fy = std::cos (sh.x * 7.3f - time * 0.7f + sh.y * 2.0f);
        sh.vx += (fx * 0.10f - sh.y * 0.05f) * drift * dt * 3.0f;
        sh.vy += (fy * 0.25f + (sh.x - 0.5f) * 0.25f) * drift * dt * 3.0f;
        sh.vx *= drag; sh.vy *= drag;
        sh.x += sh.vx * dt * aspect;
        sh.y += sh.vy * dt;
        const float target = std::atan2 (sh.vy, sh.vx * aspect + 1.0e-5f);
        float diff = target - sh.angle;
        while (diff >  MathConstants<float>::pi) diff -= MathConstants<float>::twoPi;
        while (diff < -MathConstants<float>::pi) diff += MathConstants<float>::twoPi;
        sh.angle += diff * jmin (1.0f, dt * (2.0f + 6.0f * smear)) + sh.spin * dt * (1.0f - smear);

        if (sh.trailN < (int) sh.trail.size()) ++sh.trailN;
        for (int i = sh.trailN - 1; i > 0; --i) sh.trail[(size_t) i] = sh.trail[(size_t) i - 1];
        sh.trail[0] = { sh.x, sh.y };
    }
    shards.erase (std::remove_if (shards.begin(), shards.end(), [] (const Shard& sh)
                  { return sh.age >= sh.life || sh.x < -0.1f || sh.x > 1.1f || std::abs (sh.y) > 1.3f; }), shards.end());

    for (auto& m : motes)
    {
        m.phase += dt;
        m.x += (m.vx + std::sin (m.phase * 0.4f + m.y * 3.0f) * 0.004f * (1.0f + 4.0f * drift)) * dt;
        m.y += (m.vy + std::cos (m.phase * 0.3f + m.x * 5.0f) * 0.01f) * dt;
        m.x -= std::floor (m.x);
        if (m.y < -1) m.y += 2;
        if (m.y > 1)  m.y -= 2;
    }

    for (auto& hc : heat) hc *= std::exp (-dt * 2.2f);
    pulse = jmax (proc.outputLevel.load(), pulse * std::exp (-dt * 4.0f));
    (void) grains;
}

void GrainField::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    Path clip; clip.addRoundedRectangle (r, 10.0f);
    g.setColour (Palette::panel);
    g.fillAll();
    g.reduceClipRegion (clip);

    // deep background with a soft breathing core
    const float breathe = 0.5f + 0.5f * std::sin (time * 0.8f);
    g.setGradientFill (ColourGradient (Palette::screen.brighter (0.10f + 0.10f * pulse + 0.03f * breathe), r.getCentreX(), r.getCentreY(),
                                       Palette::deep, r.getX(), r.getY(), true));
    g.fillRect (r);

    const auto in = inner();
    const float mid = in.getCentreY();
    const float halfH = in.getHeight() * 0.5f;
    auto P = [&] (float xn, float yn) { return Point<float> (in.getX() + xn * in.getWidth(), mid - yn * halfH); };

    auto& s = proc.apvts;
    const auto mv = [this] (int t) { return proc.modNorm[(size_t) t].load (std::memory_order_relaxed); };
    const float smear = mv (mtSmear), shimmer = mv (mtShimmer), bloom = mv (mtBloom);
    const bool grainsOn = s.getRawParameterValue (ParamID::grainOn)->load() > 0.5f;

    // faint dust
    for (auto& m : motes)
    {
        const float a = 0.05f + 0.06f * (0.5f + 0.5f * std::sin (m.phase * 1.7f));
        g.setColour (Palette::ice.withAlpha (a));
        const auto p = P (m.x, m.y * 0.95f);
        g.fillEllipse (p.x - m.size * 0.5f, p.y - m.size * 0.5f, m.size, m.size);
    }

    if (env.empty() || lastPeaks == nullptr || lastPeaks->maxs.empty())
    {
        g.setFont (uiFont (18.0f, false, 0.1f));
        g.setColour (dragHover ? Colours::white : Palette::accentSoft);
        g.drawText ("Drop a WAV file here", getLocalBounds(), Justification::centred);
        return;
    }

    // ---- glowing waveform body ----
    const int cols = (int) env.size();
    const float amp = 0.92f + 0.08f * pulse;
    Path body;
    for (int x = 0; x < cols; ++x)
    {
        const float xn = (float) x / (float) (cols - 1);
        const auto p = P (xn, env[(size_t) x] * amp);
        if (x == 0) body.startNewSubPath (p); else body.lineTo (p);
    }
    for (int x = cols - 1; x >= 0; --x)
        body.lineTo (P ((float) x / (float) (cols - 1), -env[(size_t) x] * amp));
    body.closeSubPath();

    g.setGradientFill (ColourGradient (Palette::accent.withAlpha (0.20f + 0.12f * pulse), in.getCentreX(), mid,
                                       Palette::accent.withAlpha (0.02f), in.getCentreX(), in.getY(), false));
    g.fillPath (body);

    // heat: columns light up where grains are being read
    for (int x = 0; x < cols; ++x)
    {
        const float hv = jmin (1.0f, heat[(size_t) x]);
        if (hv < 0.02f) continue;
        const float xn = (float) x / (float) (cols - 1);
        const float e = env[(size_t) x] * amp;
        const auto top = P (xn, e), bot = P (xn, -e);
        g.setColour (Palette::ice.withAlpha (0.10f + 0.35f * hv));
        g.fillRect (top.x - 1.0f, top.y, 3.0f, bot.y - top.y);
    }

    // contour strands
    for (int k = 0; k < 7; ++k)
    {
        const float t = (float) k / 6.0f;
        const float base = std::cos (t * MathConstants<float>::pi);
        const float edge = std::abs (base);
        Path p;
        for (int x = 0; x < cols; x += 2)
        {
            const float xn = (float) x / (float) (cols - 1);
            const float wob = 0.07f * std::sin (xn * 19.0f + (float) k * 0.9f + time * 1.3f);
            const auto q = P (xn, env[(size_t) x] * amp * (base + wob * (1.0f - edge)));
            if (x == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        if (edge > 0.99f)
        {
            g.setColour (Palette::accent.withAlpha (0.10f + 0.10f * pulse));
            g.strokePath (p, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (Palette::accent.interpolatedWith (Palette::ice, 0.3f * edge).withAlpha (0.12f + 0.5f * edge));
        g.strokePath (p, PathStrokeType (edge > 0.99f ? 1.4f : 0.8f));
    }

    // ---- position marker + read heads ----
    {
        const float pos = proc.modNorm[(size_t) mtPosition].load();
        const float x = P (pos, 0).x;
        ColourGradient cg (Palette::ice.withAlpha (0.0f), x - 9.0f, 0, Palette::ice.withAlpha (0.0f), x + 9.0f, 0, false);
        cg.addColour (0.5, Palette::ice.withAlpha (0.16f));
        g.setGradientFill (cg);
        g.fillRect (x - 9.0f, r.getY(), 18.0f, r.getHeight());
        g.setColour (Palette::ice.withAlpha (0.8f));
        g.fillRect (x - 0.75f, r.getY(), 1.5f, r.getHeight());
        Path tri; tri.addTriangle (x - 6.0f, r.getY() + 3.0f, x + 6.0f, r.getY() + 3.0f, x, r.getY() + 11.0f);
        g.fillPath (tri);
    }
    for (float hpos : heads)
    {
        const float x = P (hpos, 0).x;
        g.setColour (Palette::accent.withAlpha (0.10f)); g.fillRect (x - 3.0f, r.getY(), 6.0f, r.getHeight());
        g.setColour (Palette::accent.withAlpha (0.55f)); g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
    }

    // ---- shards ----
    const float W = in.getWidth(), H = halfH;
    for (auto& sh : shards)
    {
        const float lt = sh.age / sh.life;
        const float fade = jmin (1.0f, sh.age * 14.0f) * (1.0f - lt) * (1.0f - lt * 0.4f);
        if (fade <= 0.01f) continue;
        const auto col = shardColour (sh.tint);
        const auto p = P (sh.x, sh.y);

        // trail
        if (sh.trailN > 2)
        {
            for (int i = 1; i < sh.trailN; ++i)
            {
                const auto a = P (sh.trail[(size_t) i - 1].x, sh.trail[(size_t) i - 1].y);
                const auto b = P (sh.trail[(size_t) i].x, sh.trail[(size_t) i].y);
                const float k = 1.0f - (float) i / (float) sh.trailN;
                g.setColour (col.withAlpha (fade * k * (0.22f + 0.25f * smear)));
                g.drawLine ({ a, b }, (1.0f + 3.0f * smear) * k * sh.size);
            }
        }

        // the splinter: a long kite with one bright facet
        const float L = sh.len * W * sh.size;
        const float Wd = jmax (1.2f, sh.width * W * sh.size * (1.0f - 0.45f * smear));
        const Point<float> d (std::cos (sh.angle), -std::sin (sh.angle));
        const Point<float> n (-d.y, d.x);
        const auto front = p + d * (L * 0.62f);
        const auto back  = p - d * (L * 0.38f);
        const auto sideA = p + d * (L * 0.12f) + n * Wd;
        const auto sideB = p + d * (L * 0.05f) - n * (Wd * 0.7f);

        Path kite;
        kite.startNewSubPath (front); kite.lineTo (sideA); kite.lineTo (back); kite.lineTo (sideB); kite.closeSubPath();
        Path facet;
        facet.startNewSubPath (front); facet.lineTo (sideA); facet.lineTo (back); facet.closeSubPath();

        // glow
        g.setColour (col.withAlpha (fade * (0.10f + 0.12f * bloom + 0.15f * smear)));
        g.strokePath (kite, PathStrokeType (Wd * (2.5f + 3.0f * smear), PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col.withAlpha (fade * (0.85f - 0.35f * smear)));
        g.fillPath (kite);
        g.setColour (Colours::white.withAlpha (fade * 0.45f));
        g.fillPath (facet);

        if (sh.kind == 2)   // reverse grains: a thin ice edge
        {
            g.setColour (Palette::ice.withAlpha (fade * 0.6f));
            g.strokePath (kite, PathStrokeType (0.8f));
        }
        if (sh.kind == 1)   // SHIMMER sparks: twinkling star
        {
            const float tw = 0.5f + 0.5f * std::sin (sh.age * 22.0f + sh.x * 40.0f);
            const float sr = (3.0f + 5.0f * tw) * sh.size;
            g.setColour (Colours::white.withAlpha (fade * (0.5f + 0.5f * tw)));
            g.drawLine (front.x - sr, front.y, front.x + sr, front.y, 1.1f);
            g.drawLine (front.x, front.y - sr, front.x, front.y + sr, 1.1f);
            g.setColour (Colours::white.withAlpha (fade * 0.25f * tw));
            g.fillEllipse (front.x - sr * 0.6f, front.y - sr * 0.6f, sr * 1.2f, sr * 1.2f);
        }
    }

    // vignette
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                       Palette::deep.withAlpha (0.55f), r.getX(), r.getY(), true));
    g.fillRect (r);

    if (! grainsOn)
    {
        g.setColour (Palette::textDim.withAlpha (0.55f));
        g.setFont (uiFont (11.0f, false, 0.12f));
        g.drawText ("GRANULAR OFF", r.reduced (12.0f, 8.0f), Justification::bottomRight, false);
    }
    (void) shimmer; (void) H;

    if (dragHover)
    {
        g.setColour (Palette::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (2.0f), 10.0f, 2.0f);
    }
}

void GrainField::setPositionFromMouse (const MouseEvent& e)
{
    const auto in = inner();
    const float norm = jlimit (0.0f, 1.0f, (e.position.x - in.getX()) / in.getWidth());
    if (auto* p = proc.apvts.getParameter (ParamID::position))
        p->setValueNotifyingHost (norm);
}

void GrainField::mouseDown (const MouseEvent& e)
{
    if (auto* p = proc.apvts.getParameter (ParamID::position)) p->beginChangeGesture();
    setPositionFromMouse (e);
}

void GrainField::mouseDrag (const MouseEvent& e) { setPositionFromMouse (e); }

void GrainField::mouseUp (const MouseEvent&)
{
    if (auto* p = proc.apvts.getParameter (ParamID::position)) p->endChangeGesture();
}

// ============================================================================
// Additive bars (16, wide) + gold GRAIN row

static File settingsFile()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("GRAIN FORM").getChildFile ("settings.xml");
}

void HarmonicEditor::rememberHintSeen()
{
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    XmlElement x ("GRAINFORM");
    x.setAttribute ("selectHintSeen", 1);
    x.writeTo (f);
}

HarmonicEditor::HarmonicEditor (GranulumProcessor& p) : proc (p)
{
    if (auto x = parseXML (settingsFile()))
        showHint = x->getIntAttribute ("selectHintSeen", 0) == 0;
    h = proc.getHarmonics();
    mask = proc.getGrainMask();
    setMouseCursor (MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

void HarmonicEditor::timerCallback()
{
    const float dt = 1.0f / 30.0f;
    time += dt;
    introT += dt;
    {
        const bool grains = grainsActive();
        for (int k = 0; k < granulum::kNumHarmonics; ++k)
        {
            if (! grains || ! ((mask >> k) & 1u) || h[(size_t) k] < 0.01f) continue;
            if (rng.nextFloat() < dt * 14.0f)
            {
                Crystal c;
                c.bar = (float) k;
                c.dx = 0.15f + 0.7f * rng.nextFloat();            // across the bar width
                c.y = rng.nextFloat() * 0.6f;                     // fraction of the bar height
                c.vy = 0.25f + 0.35f * rng.nextFloat();            // bar heights per second
                c.life = 0.8f + 1.0f * rng.nextFloat();
                c.size = 1.2f + 1.1f * rng.nextFloat();
                c.white = rng.nextFloat() < 0.35f;
                if (crystals.size() < 260) crystals.push_back (c);
            }
        }
        for (auto& c : crystals)
        {
            c.age += dt;
            c.y += c.vy * dt;
            c.dx = jlimit (0.1f, 0.9f, c.dx + std::sin (time * 2.0f + c.life * 7.0f) * 0.004f);
        }
        crystals.erase (std::remove_if (crystals.begin(), crystals.end(), [] (const Crystal& c)
                        { return c.age >= c.life || c.y > 0.98f; }), crystals.end());
    }
    if (! editing)
    {
        const auto now = proc.getHarmonics();
        if (now != h) h = now;
        if (lastPill < 0) mask = proc.getGrainMask();
    }
    repaint();
}

bool HarmonicEditor::grainsActive() const
{
    auto& s = proc.apvts;
    return s.getRawParameterValue (ParamID::grainOn)->load() > 0.5f
        && s.getRawParameterValue (ParamID::osc1Grain)->load() > 0.5f
        && s.getRawParameterValue (ParamID::osc1On)->load() > 0.5f;
}

static constexpr float kGrainLabelW = 46.0f;

Rectangle<float> HarmonicEditor::barArea() const
{
    auto r = getLocalBounds().toFloat().reduced (7.0f, 6.0f);
    r.removeFromLeft (kGrainLabelW - 7.0f);
    r.removeFromBottom (20.0f);
    return r;
}

Rectangle<float> HarmonicEditor::pillRow() const
{
    auto a = barArea();
    return { a.getX(), a.getBottom() + 5.0f, a.getWidth(), 13.0f };
}

int HarmonicEditor::barAt (float x) const
{
    auto a = barArea();
    return jlimit (0, granulum::kNumHarmonics - 1, (int) ((x - a.getX()) / a.getWidth() * granulum::kNumHarmonics));
}

void HarmonicEditor::applyMouse (const MouseEvent& e, bool first)
{
    auto a = barArea();
    const int bar = barAt (e.position.x);
    const float value = erasing ? 0.0f : jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());

    if (first || lastBar < 0)
        h[(size_t) bar] = value;
    else
    {
        const int from = lastBar, to = bar;
        const int steps = std::abs (to - from);
        for (int i = 0; i <= steps; ++i)
        {
            const int b = from + (to > from ? i : -i);
            const float t = steps == 0 ? 1.0f : (float) i / (float) steps;
            h[(size_t) b] = lastValue + (value - lastValue) * t;
        }
    }
    lastBar = bar;
    lastValue = value;
    proc.setHarmonics (h);
    repaint();
}

void HarmonicEditor::applyPill (int bar)
{
    const int from = lastPill < 0 ? bar : lastPill;
    for (int b = jmin (from, bar); b <= jmax (from, bar); ++b)
        mask = pillValue ? (mask | (1u << b)) : (mask & ~(1u << b));
    lastPill = bar;
    proc.setGrainMask (mask);
    repaint();
}

void HarmonicEditor::mouseMove (const MouseEvent& e)
{
    if (e.position.y >= pillRow().getY() - 3.0f && e.position.x >= barArea().getX())
    {
        const int b = barAt (e.position.x);
        const bool on = (mask >> b) & 1u;
        hoverBar = b;
        setTooltip ("Harmonic " + String (b + 1) + (on ? ":  GRAIN ON.  Click to keep it clean & mono."
                                                       : ":  CLEAN & MONO.  Click to send it into the grains."));
        setMouseCursor (MouseCursor::PointingHandCursor);
    }
    else if (e.position.x < barArea().getX())
    {
        hoverBar = -1;
        setTooltip ("SELECT: lit dot = this harmonic turns into grains, empty = it stays clean & mono.\nKeep the lowest ones clean for a solid sub.");
        setMouseCursor (MouseCursor::NormalCursor);
    }
    else
    {
        hoverBar = -1;
        setTooltip ("Draw the harmonics with the mouse. Right button erases.\nGold dots below: which harmonics go into the grains.");
        setMouseCursor (MouseCursor::CrosshairCursor);
    }
}

void HarmonicEditor::mouseDown (const MouseEvent& e)
{
    if (e.position.y >= pillRow().getY() - 3.0f && e.position.x >= barArea().getX())
    {
        pillMode = true;
        editing = true;
        lastPill = -1;
        const int b = barAt (e.position.x);
        pillValue = ! ((mask >> b) & 1u);
        applyPill (b);
        if (showHint) { showHint = false; rememberHintSeen(); }
        return;
    }
    pillMode = false;
    editing = true;
    erasing = e.mods.isRightButtonDown();
    lastBar = -1;
    applyMouse (e, true);
}

void HarmonicEditor::mouseDrag (const MouseEvent& e)
{
    if (! editing) return;
    if (pillMode) applyPill (barAt (e.position.x));
    else          applyMouse (e, false);
}

void HarmonicEditor::mouseExit (const MouseEvent&) { hoverBar = -1; }

void HarmonicEditor::mouseUp (const MouseEvent&) { editing = false; lastBar = -1; lastPill = -1; pillMode = false; }

void HarmonicEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 9.0f);

    const bool active = proc.apvts.getRawParameterValue (ParamID::osc1On)->load() > 0.5f;
    const bool grains = grainsActive();
    auto a = barArea();

    // resulting waveform behind the bars
    {
        Path wave;
        const int pts = 160;
        std::vector<float> ys ((size_t) pts);
        float peak = 0.0f;
        for (int i = 0; i < pts; ++i)
        {
            const float ph = MathConstants<float>::twoPi * (float) i / (float) (pts - 1);
            float v = 0.0f;
            for (int k = 0; k < granulum::kNumHarmonics; ++k) v += h[(size_t) k] * std::sin (ph * (float) (k + 1));
            ys[(size_t) i] = v;
            peak = jmax (peak, std::abs (v));
        }
        const float sc = peak > 1.0e-4f ? 1.0f / peak : 0.0f;
        for (int i = 0; i < pts; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (float) (pts - 1);
            const float y = a.getCentreY() - ys[(size_t) i] * sc * a.getHeight() * 0.40f;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (Palette::ice.withAlpha (active ? 0.18f : 0.07f));
        g.strokePath (wave, PathStrokeType (1.2f));
    }

    const float bw = a.getWidth() / (float) granulum::kNumHarmonics;
    for (int k = 0; k < granulum::kNumHarmonics; ++k)
    {
        auto slot = Rectangle<float> (a.getX() + bw * (float) k, a.getY(), bw, a.getHeight()).reduced (bw * 0.12f, 0.0f);
        const bool shattered = grains && ((mask >> k) & 1u);
        if (hoverBar == k)
        {
            g.setColour (Colour (0xfff1e4cb).withAlpha (0.10f));
            g.fillRoundedRectangle (slot.expanded (bw * 0.08f, 2.0f), 4.0f);
        }
        // granulated harmonics sit on a slightly warmer, sandy background
        if (barStyle == 5)
        {
            g.setColour ((shattered ? Colour (0xffc9ae84) : Colours::white).withAlpha (shattered ? 0.14f : 0.06f));
            g.fillRect (slot.getCentreX() - 0.5f, slot.getY(), 1.0f, slot.getHeight());
        }
        else
        {
            g.setColour (shattered ? Palette::track.interpolatedWith (Colour (0xffc9ae84), 0.16f) : Palette::track.withAlpha (0.85f));
            g.fillRoundedRectangle (slot, 3.0f);
        }
        const float v = h[(size_t) k];
        if (v <= 0.001f) continue;
        auto bar = slot.withTop (slot.getBottom() - slot.getHeight() * v);
        const auto col = active ? Palette::accent : Palette::accentSoft.withAlpha (0.35f);

        const Colour sandLight (0xffead9b8);
        const float head = std::fmod (time * 0.45f, 1.4f) - 0.2f;             // shared light wave, -0.2 .. 1.2
        auto waveAt = [head] (float t) { const float d = (t - head) / 0.22f; return std::exp (-d * d); };
        auto grainsInside = [&] (Rectangle<float> area)
        {
            for (auto& c : crystals)
            {
                if ((int) c.bar != k) continue;
                const float lt = c.age / c.life;
                const float fa = jmin (1.0f, c.age * 5.0f) * (1.0f - lt);
                const float cx = area.getX() + area.getWidth() * c.dx;
                const float cy = area.getBottom() - area.getHeight() * c.y;
                g.setColour ((c.white ? Colours::white : Colour (0xfff7ecd6)).withAlpha (0.85f * fa));
                g.fillEllipse (cx - c.size * 0.5f, cy - c.size * 0.5f, c.size, c.size);
            }
        };

        if (barStyle == 5)
        {
            // STRINGS: a thin string with a round head; granulated strings turn sand, tremble softly
            // and carry tiny dew drops running up along them
            const float x = bar.getCentreX();
            const float vib = shattered ? std::sin (time * 9.0f + (float) k) * 1.2f : 0.0f;
            Path str;
            str.startNewSubPath (x, bar.getBottom());
            str.quadraticTo (x + vib, bar.getCentreY(), x, bar.getY());
            const auto sc = shattered ? sandLight : col;
            g.setColour (sc.withAlpha (0.20f));
            g.strokePath (str, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (sc);
            g.strokePath (str, PathStrokeType (3.2f, PathStrokeType::curved, PathStrokeType::rounded));
            if (shattered)
                for (auto& c : crystals)
                {
                    if ((int) c.bar != k) continue;
                    const float fa = jmin (1.0f, c.age * 5.0f) * (1.0f - c.age / c.life);
                    g.setColour (Colour (0xfffff3dc).withAlpha (fa));
                    g.fillEllipse (x + (c.dx - 0.5f) * 5.0f - 1.4f, bar.getBottom() - bar.getHeight() * c.y - 1.4f, 2.8f, 2.8f);
                }
            // bright tip instead of a ball: just a clean stick
            g.setColour ((shattered ? Colour (0xfffff3dc) : Palette::ice));
            g.fillRoundedRectangle (x - 1.6f, bar.getY() - 1.0f, 3.2f, jmin (6.0f, bar.getHeight() + 1.0f), 1.6f);
        }
        else if (barStyle == 1)
        {
            // FROSTED GLASS: translucent glass column, bright rim on top and left edge;
            // granulated = warm light breathing inside the glass
            const auto base = shattered ? sandLight : col;
            g.setGradientFill (ColourGradient (base.withAlpha (0.55f), bar.getX(), 0, base.withAlpha (0.28f), bar.getRight(), 0, false));
            g.fillRoundedRectangle (bar, 4.0f);
            if (shattered)
            {
                Graphics::ScopedSaveState ss (g);
                Path cp; cp.addRoundedRectangle (bar, 4.0f); g.reduceClipRegion (cp);
                const float yHead = bar.getBottom() - bar.getHeight() * head;
                g.setGradientFill (ColourGradient (sandLight.withAlpha (0.0f), 0, yHead - 18.0f, sandLight.withAlpha (0.0f), 0, yHead + 18.0f, false));
                ColourGradient cg (sandLight.withAlpha (0.0f), 0, yHead - 18.0f, sandLight.withAlpha (0.0f), 0, yHead + 18.0f, false);
                cg.addColour (0.5, Colours::white.withAlpha (0.55f));
                g.setGradientFill (cg);
                g.fillRect (bar);
                grainsInside (bar);
            }
            g.setColour (Colours::white.withAlpha (0.55f));
            g.drawRoundedRectangle (bar.reduced (0.5f), 4.0f, 0.8f);
            g.setColour (Colours::white.withAlpha (0.8f));
            g.fillRoundedRectangle (bar.withHeight (jmin (2.0f, bar.getHeight())).reduced (1.0f, 0.0f), 1.0f);
        }
        else if (barStyle == 2)
        {
            // CAPSULE + FILAMENT: soft rounded capsules; granulated ones carry a glowing sand filament inside
            const float rad = bar.getWidth() * 0.5f;
            g.setGradientFill (ColourGradient (col.interpolatedWith (Palette::ice, 0.25f), 0, bar.getY(), col.darker (0.25f), 0, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, rad);
            if (shattered)
            {
                const float fw = jmax (2.0f, bar.getWidth() * 0.22f);
                auto fil = bar.withSizeKeepingCentre (fw, jmax (0.0f, bar.getHeight() - rad));
                fil.setY (bar.getY() + rad * 0.5f);
                for (float y = fil.getY(); y < fil.getBottom(); y += 1.5f)
                {
                    const float t = (bar.getBottom() - y) / jmax (1.0f, bar.getHeight());
                    const float w = 0.35f + 0.65f * waveAt (t);
                    g.setColour (sandLight.withAlpha (w).interpolatedWith (Colours::white, 0.3f * w));
                    g.fillRect (fil.getX(), y, fil.getWidth(), 1.5f);
                }
                g.setColour (sandLight.withAlpha (0.18f));
                g.fillRoundedRectangle (fil.expanded (3.0f, 2.0f), 3.0f);
            }
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillEllipse (bar.getX() + rad * 0.45f, bar.getY() + rad * 0.3f, rad * 0.5f, rad * 0.5f);
        }
        else if (barStyle == 3)
        {
            // LINE + CAP: a slim stem with a round cap at the top (like a fader);
            // granulated = sand cap with a soft halo and grains drifting up the stem
            const float stemW = jmax (2.0f, bw * 0.16f);
            auto stem = bar.withSizeKeepingCentre (stemW, bar.getHeight());
            const auto stemCol = shattered ? sandLight : col;
            g.setGradientFill (ColourGradient (stemCol.withAlpha (0.9f), 0, stem.getY(), stemCol.withAlpha (0.25f), 0, stem.getBottom(), false));
            g.fillRoundedRectangle (stem, stemW * 0.5f);
            const float capR = jmin (bw * 0.36f, 7.0f);
            const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre ({ bar.getCentreX(), bar.getY() });
            if (shattered)
            {
                const float pulse = 0.5f + 0.5f * std::sin (time * 2.2f);
                g.setColour (sandLight.withAlpha (0.18f + 0.12f * pulse));
                g.fillEllipse (cap.expanded (capR * 0.9f));
                auto lane = bar.withSizeKeepingCentre (bw * 0.5f, bar.getHeight());
                grainsInside (lane);
            }
            g.setGradientFill (ColourGradient (Colours::white, cap.getX(), cap.getY(), shattered ? sandLight : col, cap.getRight(), cap.getBottom(), false));
            g.fillEllipse (cap);
        }
        else if (barStyle == 4)
        {
            // FADE: solid at the bottom, dissolving towards the top with a crisp bright edge;
            // granulated = the edge turns sand and light rises through the fade
            const auto base = shattered ? Palette::accent.interpolatedWith (sandLight, 0.55f) : col;
            g.setGradientFill (ColourGradient (base.withAlpha (0.15f), 0, bar.getY(), base.withAlpha (0.95f), 0, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 3.0f);
            if (shattered)
            {
                Graphics::ScopedSaveState ss (g);
                Path cp; cp.addRoundedRectangle (bar, 3.0f); g.reduceClipRegion (cp);
                for (float y = bar.getY(); y < bar.getBottom(); y += 1.5f)
                {
                    const float t = (bar.getBottom() - y) / jmax (1.0f, bar.getHeight());
                    g.setColour (sandLight.withAlpha (0.55f * waveAt (t)));
                    g.fillRect (bar.getX(), y, bar.getWidth(), 1.5f);
                }
                grainsInside (bar);
            }
            g.setColour (shattered ? sandLight.brighter (0.2f) : Palette::ice);
            g.fillRoundedRectangle (bar.withHeight (jmin (2.5f, bar.getHeight())), 1.2f);
        }
        else
        if (! shattered)
        {
            // clean harmonic: one solid, calm bar
            g.setColour (col.withAlpha (active ? 0.14f : 0.06f));
            g.fillRoundedRectangle (bar.expanded (2.0f, 2.0f), 4.0f);
            g.setGradientFill (ColourGradient (col.interpolatedWith (Palette::ice, active ? 0.3f : 0.0f), 0, bar.getY(), col, 0, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (Colours::white.withAlpha (active ? 0.5f : 0.15f));
            g.fillRoundedRectangle (bar.withHeight (jmin (2.5f, bar.getHeight())), 1.2f);
        }
        else
        {
            // granulated harmonic: the bar stays whole, warm sand light slowly flows up through it
            g.setColour (Colour (0xffead9b8).withAlpha (0.12f));
            g.fillRoundedRectangle (bar.expanded (3.0f, 3.0f), 5.0f);
            Graphics::ScopedSaveState ss (g);
            Path clipPath; clipPath.addRoundedRectangle (bar, 3.0f);
            g.reduceClipRegion (clipPath);
            // one soft wave of light climbs every granulated bar together, bottom to top, in the same time
            const float span = jmax (1.0f, bar.getHeight());
            const float head = std::fmod (time * 0.45f, 1.4f) - 0.2f;            // -0.2 .. 1.2, same for all bars
            for (float y = bar.getY(); y < bar.getBottom(); y += 1.5f)
            {
                const float t = (bar.getBottom() - y) / span;                         // 0 bottom .. 1 top of THIS bar
                const float d = (t - head) / 0.22f;
                const float w = 0.25f + 0.75f * std::exp (-d * d);
                const Colour sandLight (0xffead9b8);
                g.setColour (Palette::accent.interpolatedWith (sandLight, 0.10f + 0.85f * w * w));
                g.fillRect (bar.getX(), y, bar.getWidth(), 1.5f);
            }
            // micro grains drifting up inside the bar (clipped to it)
            for (auto& c : crystals)
            {
                if ((int) c.bar != k) continue;
                const float lt = c.age / c.life;
                const float fa = jmin (1.0f, c.age * 5.0f) * (1.0f - lt);
                const float cx = bar.getX() + bar.getWidth() * c.dx;
                const float cy = bar.getBottom() - bar.getHeight() * c.y;
                g.setColour ((c.white ? Colours::white : Colour (0xfff7ecd6)).withAlpha (0.85f * fa));
                g.fillEllipse (cx - c.size * 0.5f, cy - c.size * 0.5f, c.size, c.size);
            }
            g.setColour (Colours::white.withAlpha (0.75f));
            g.fillRect (bar.withHeight (jmin (2.0f, bar.getHeight())));
        }
    }

    // ---- GRAIN row: which harmonics are granulated (pearl, matches the shimmer on the bars) ----
    {
        auto row = pillRow();
        const float alpha = active ? (grains ? 1.0f : 0.4f) : 0.25f;
        const Colour pearl (0xfff1e4cb), pearlShade (0xffc9ae84);   // sand pearl, same light as the bars
        // quiet label: SELECT (echoes SELECTIVE GRANULATION), not a button
        g.setColour (pearl.withAlpha (0.85f * alpha));
        g.setFont (uiFont (9.5f, true, 0.18f));
        g.drawText ("SELECT", Rectangle<float> (r.getX() + 5.0f, row.getY() - 3.0f, kGrainLabelW, row.getHeight() + 6.0f), Justification::centredLeft, false);
        const float introX = introT < 1.8f ? (introT / 1.4f) : -1.0f;      // one light wave across the dots on open

        for (int k = 0; k < granulum::kNumHarmonics; ++k)
        {
            const auto cell = Rectangle<float> (row.getX() + bw * (float) k, row.getY(), bw, row.getHeight());
            const bool on = (mask >> k) & 1u;
            if (rowStyle == 2)
            {
                const auto c = cell.getCentre();
                const bool hot = hoverBar == k;
                const float pos = (float) k / (float) (granulum::kNumHarmonics - 1);
                const float wave = introX >= 0.0f ? std::exp (-std::pow ((pos - introX) / 0.08f, 2.0f)) : 0.0f;
                const float rad = (hot ? 4.0f : 2.8f) + 1.4f * wave;
                const auto d = Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c);
                if (wave > 0.02f || hot)
                {
                    g.setColour (pearl.withAlpha ((0.25f * wave + (hot ? 0.18f : 0.0f)) * alpha));
                    g.fillEllipse (d.expanded (3.0f));
                }
                if (on) { g.setColour (pearl.interpolatedWith (Colours::white, wave).withAlpha (alpha)); g.fillEllipse (d); }
                else    { g.setColour (pearlShade.withAlpha ((hot ? 0.9f : 0.55f) * alpha)); g.drawEllipse (d, hot ? 1.3f : 1.0f); }
            }
            else if (rowStyle == 0)
            {
                // little lamp: lit = warm sand light, off = dark lamp
                const auto c = cell.getCentre();
                const float rad = jmin (4.5f, bw * 0.28f);
                const auto dot = Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c);
                if (on)
                {
                    g.setGradientFill (ColourGradient (Colours::white.withAlpha (alpha), c.x - rad * 0.35f, c.y - rad * 0.45f,
                                                       pearlShade.withAlpha (alpha), c.x + rad, c.y + rad, true));
                    g.fillEllipse (dot);
                }
                else
                {
                    g.setColour (Palette::track.brighter (0.25f).withAlpha (alpha));
                    g.fillEllipse (dot);
                    g.setColour (Colours::black.withAlpha (0.25f * alpha));
                    g.drawEllipse (dot.reduced (0.4f), 0.8f);
                }
            }
            else
            {
                // glowing line
                auto line = cell.reduced (bw * 0.14f, 0.0f).withSizeKeepingCentre (cell.getWidth() - bw * 0.28f, 3.0f);
                if (on)
                {
                    g.setColour (pearl.withAlpha (0.25f * alpha));
                    g.fillRoundedRectangle (line.expanded (2.0f, 3.0f), 4.0f);
                    g.setColour (pearl.withAlpha (alpha));
                    g.fillRoundedRectangle (line, 1.5f);
                }
                else
                {
                    g.setColour (pearlShade.withAlpha (0.25f * alpha));
                    g.fillRoundedRectangle (line.withSizeKeepingCentre (line.getWidth(), 1.5f), 1.0f);
                }
            }
        }
    }

    if (showHint && active)
    {
        // first-run hint: disappears after the first click on a dot, never comes back
        const auto row = pillRow();
        const String txt ("Click the dots: choose which harmonics turn into grains");
        const auto f = uiFont (10.0f, false, 0.04f);
        const float w = (float) GlyphArrangement::getStringWidthInt (f, txt) + 20.0f;
        auto plate = Rectangle<float> (w, 20.0f).withCentre ({ row.getCentreX(), row.getY() - 15.0f });
        plate.setX (jmax (r.getX() + 4.0f, plate.getX()));
        const float pulse = 0.85f + 0.15f * std::sin (time * 3.0f);
        g.setColour (Palette::deep.withAlpha (0.88f));
        g.fillRoundedRectangle (plate, 6.0f);
        g.setColour (Colour (0xfff1e4cb).withAlpha (0.55f * pulse));
        g.drawRoundedRectangle (plate.reduced (0.5f), 6.0f, 1.0f);
        Path arrow;
        arrow.addTriangle (plate.getCentreX() - 5.0f, plate.getBottom(), plate.getCentreX() + 5.0f, plate.getBottom(), plate.getCentreX(), plate.getBottom() + 5.0f);
        g.setColour (Palette::deep.withAlpha (0.88f));
        g.fillPath (arrow);
        g.setColour (Colour (0xfff1e4cb).withAlpha (pulse));
        g.setFont (f);
        g.drawText (txt, plate, Justification::centred, false);
    }

    if (! active)
    {
        g.setColour (Palette::textDim.withAlpha (0.8f));
        g.setFont (uiFont (12.0f, false, 0.08f));
        g.drawText ("OSC 1 OFF", a, Justification::centred, false);
    }
}

// ============================================================================
// Peak meter

PeakMeter::PeakMeter (GranulumProcessor& p) : proc (p)
{
    setTooltip ("Output level. The red zone means the sound is hitting the ceiling.\nClick CLIP to reset it.");
    startTimerHz (40);
}

void PeakMeter::timerCallback()
{
    const float in[2] = { proc.peakL.exchange (0.0f), proc.peakR.exchange (0.0f) };
    for (int c = 0; c < 2; ++c)
    {
        level[c] = jmax (in[c], level[c] * 0.86f);
        if (level[c] >= hold[c]) { hold[c] = level[c]; holdAge[c] = 0; }
        else if (++holdAge[c] > 40) hold[c] *= 0.93f;
    }
    if (proc.clipped.load()) clipShown = true;
    repaint();
}

void PeakMeter::mouseDown (const MouseEvent&)
{
    clipShown = false;
    proc.clipped.store (false);
    hold[0] = hold[1] = 0.0f;
}

void PeakMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto clipBox = r.removeFromRight (34.0f).withSizeKeepingCentre (32.0f, 15.0f);
    r.removeFromRight (6.0f);
    auto scale = r.removeFromBottom (11.0f);
    const float barH = 5.0f;
    auto bars = r.withSizeKeepingCentre (r.getWidth(), barH * 2.0f + 4.0f);

    // dB -> x: -48 .. 0 dB, a little more room near the top
    auto xOf = [&bars] (float db)
    {
        const float t = jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        return bars.getX() + bars.getWidth() * std::pow (t, 1.6f);
    };
    const float redFrom = xOf (-3.0f), top = xOf (0.0f);

    for (int c = 0; c < 2; ++c)
    {
        auto lane = Rectangle<float> (bars.getX(), bars.getY() + (float) c * (barH + 4.0f), bars.getWidth(), barH);
        g.setColour (Palette::track);
        g.fillRoundedRectangle (lane, 2.0f);
        g.setColour (Palette::clip.withAlpha (0.18f));
        g.fillRect (lane.withLeft (redFrom).withRight (top));

        const float db = Decibels::gainToDecibels (level[c], -60.0f);
        const float x = xOf (db);
        if (x > lane.getX() + 0.5f)
        {
            auto lit = lane.withRight (jmin (x, redFrom));
            g.setGradientFill (ColourGradient (Palette::accent.withAlpha (0.85f), lane.getX(), 0, Palette::ice, redFrom, 0, false));
            g.fillRoundedRectangle (lit, 2.0f);
            if (x > redFrom)
            {
                g.setColour (Palette::clip);
                g.fillRect (lane.withLeft (redFrom).withRight (jmin (x, top)));
            }
        }
        const float hx = xOf (Decibels::gainToDecibels (hold[c], -60.0f));
        if (hold[c] > 0.001f)
        {
            g.setColour (hx > redFrom ? Palette::clip : Palette::ice);
            g.fillRect (hx - 1.0f, lane.getY() - 1.0f, 2.0f, lane.getHeight() + 2.0f);
        }
    }

    g.setFont (uiFont (8.5f, false, 0.05f));
    for (float db : { -24.0f, -12.0f, -6.0f, 0.0f })
    {
        const float x = xOf (db);
        g.setColour (db >= 0.0f ? Palette::clip.withAlpha (0.8f) : Palette::textDim.withAlpha (0.7f));
        g.drawText (db == 0.0f ? String ("0") : String ((int) db), Rectangle<float> (x - 12.0f, scale.getY(), 24.0f, scale.getHeight()), Justification::centred, false);
    }

    g.setColour (clipShown ? Palette::clip : Palette::track);
    g.fillRoundedRectangle (clipBox, 4.0f);
    if (! clipShown)
    {
        g.setColour (Palette::clip.withAlpha (0.35f));
        g.drawRoundedRectangle (clipBox.reduced (0.5f), 4.0f, 1.0f);
    }
    g.setColour (clipShown ? Colours::white : Palette::textDim.withAlpha (0.6f));
    g.setFont (uiFont (9.0f, true, 0.1f));
    g.drawText ("CLIP", clipBox, Justification::centred, false);
}

// ============================================================================
// Chord picker

ChordPicker::ChordPicker (GranulumProcessor& p) : proc (p)
{
    const auto& names = GranulumProcessor::chordNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (names[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { choose (i); };
        addAndMakeVisible (b);
    }
    buttons[7]->setTooltip ("+1 semitone");
    buttons[8]->setTooltip ("+7 semitones (fifth)");
    refresh();
    setSize (392, 214);
}

void ChordPicker::choose (int index)
{
    if (auto* prm = proc.apvts.getParameter (ParamID::chord))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
        prm->endChangeGesture();
    }
    refresh();
}

void ChordPicker::refresh()
{
    const int cur = (int) proc.apvts.getRawParameterValue (ParamID::chord)->load();
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == cur, dontSendNotification);
    repaint();
}

void ChordPicker::resized()
{
    auto r = getLocalBounds().reduced (4);
    chordLabel = r.removeFromTop (16);
    auto row1 = r.removeFromTop (28);
    const int bw = row1.getWidth() / 7;
    for (int i = 0; i < 7; ++i) buttons[i]->setBounds (row1.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (8);
    auto row2 = r.removeFromTop (28);
    intervalLabel = row2.removeFromLeft (bw * 2);
    buttons[7]->setBounds (row2.removeFromLeft (bw * 2).reduced (2, 0));
    buttons[8]->setBounds (row2.removeFromLeft (bw * 2).reduced (2, 0));
    r.removeFromTop (10);
    captionArea = r.removeFromBottom (16);
    keysArea = r;
}

void ChordPicker::paint (Graphics& g)
{
    g.setColour (Palette::label);
    g.setFont (uiFont (10.5f, false, 0.1f));
    g.drawText ("CHORD", chordLabel.withTrimmedLeft (3), Justification::centredLeft, false);
    g.drawText ("INTERVAL", intervalLabel.withTrimmedLeft (3), Justification::centredLeft, false);

    const int cur = (int) proc.apvts.getRawParameterValue (ParamID::chord)->load();
    const auto iv = GranulumProcessor::chordIntervals (cur);
    auto lit = [&iv] (int semis) { return std::find (iv.begin(), iv.end(), semis) != iv.end(); };

    // two-octave mini keyboard from C
    auto k = keysArea.toFloat();
    const int whites = 14;
    const float ww = k.getWidth() / (float) whites;
    const int whiteSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int i = 0; i < whites; ++i)
    {
        const int semi = whiteSemis[i % 7] + 12 * (i / 7);
        auto key = Rectangle<float> (k.getX() + ww * (float) i, k.getY(), ww, k.getHeight()).reduced (1.0f, 0.0f);
        const bool on = lit (semi);
        g.setColour (on ? (semi == 0 ? Palette::ice : Palette::accent) : Palette::whiteKey.withAlpha (0.85f));
        g.fillRoundedRectangle (key, 3.0f);
        if (on)
        {
            g.setColour (Palette::screen);
            g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ key.getCentreX(), key.getBottom() - 9.0f }));
        }
    }
    const int blackAfter[5] = { 0, 1, 3, 4, 5 };
    const int blackSemis[5] = { 1, 3, 6, 8, 10 };
    for (int o = 0; o < 2; ++o)
        for (int j = 0; j < 5; ++j)
        {
            const int semi = blackSemis[j] + 12 * o;
            const float x = k.getX() + ww * (float) (blackAfter[j] + 1 + 7 * o) - ww * 0.32f;
            auto key = Rectangle<float> (x, k.getY(), ww * 0.64f, k.getHeight() * 0.6f);
            const bool on = lit (semi);
            g.setColour (on ? Palette::accent : Palette::blackKey.darker (0.3f));
            g.fillRoundedRectangle (key, 2.5f);
        }

    StringArray notes;
    for (int s : iv) notes.add (MidiMessage::getMidiNoteName (60 + s, true, false, 3));
    g.setColour (Palette::textDim);
    g.setFont (uiFont (11.0f, false, 0.08f));
    g.drawText (cur == 0 ? String ("single note") : "from C:  " + notes.joinIntoString ("  "), captionArea, Justification::centred, false);
}

// ============================================================================
// LFO tab

LfoTab::LfoTab (GranulumProcessor& p, int i) : index (i), proc (p)
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
    setTooltip ("Click the light: switch this LFO on/off everywhere (routings are kept).\nDrag onto any knob to modulate it.");
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
    const bool on = proc.isLfoOn (index);
    const auto ledR = Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() });
    if (on)
        drawLed (g, ledR, col, col.darker (0.6f), 0.55f + 0.45f * proc.lfoValueOut[(size_t) index].load());
    else
    {
        g.setColour (col.withAlpha (0.45f));
        g.drawEllipse (ledR.reduced (0.5f), 1.2f);
    }

    g.setColour ((selected ? Colours::white : Palette::text).withAlpha (on ? 1.0f : 0.45f));
    g.setFont (uiFont (13.0f, true, 0.08f));
    g.drawText ("LFO " + String (index + 1), r.withTrimmedLeft (24.0f), Justification::centredLeft, false);

    g.setColour (Palette::textDim.withAlpha (0.8f));
    for (int row = 0; row < 3; ++row)
        for (int c = 0; c < 2; ++c)
            g.fillEllipse (r.getRight() - 15.0f + (float) c * 5.0f, r.getCentreY() - 6.0f + (float) row * 5.0f, 2.2f, 2.2f);

    const int n = proc.countModulations (index);
    if (n > 0)
    {
        g.setColour (col);
        g.setFont (uiFont (10.5f, true));
        g.drawText (String (n), r.withTrimmedRight (22.0f), Justification::centredRight, false);
    }
}

void LfoTab::mouseMove (const MouseEvent& e)
{
    // over the light: plain arrow (it's a switch); elsewhere: the grab hand (drag onto a knob)
    setMouseCursor (e.position.x < 26.0f ? MouseCursor::NormalCursor : MouseCursor::DraggingHandCursor);
}

void LfoTab::mouseDown (const MouseEvent& e)
{
    dragging = false;
    if (e.position.x < 26.0f)
        if (auto* prm = proc.apvts.getParameter (ParamID::lfo (index, "On")))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
            repaint();
        }
    if (onSelect) onSelect (index);
}

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
// LFO editor — 4 points, bendable segments

LfoEditor::LfoEditor (GranulumProcessor& p) : proc (p)
{
    pts = proc.getLfoPoints (0);
    setRepaintsOnMouseActivity (false);
    startTimerHz (60);
}

void LfoEditor::setLfo (int l)
{
    lfo = l;
    pts = proc.getLfoPoints (l);
    dragIndex = hoverIndex = -1;
    repaint();
}

void LfoEditor::timerCallback()
{
    if (dragIndex < 0)
    {
        const auto now = proc.getLfoPoints (lfo);
        bool same = true;
        for (int i = 0; i < kLfoPoints; ++i)
            same &= now[(size_t) i].x == pts[(size_t) i].x && now[(size_t) i].y == pts[(size_t) i].y && now[(size_t) i].curve == pts[(size_t) i].curve;
        if (! same) pts = now;
    }
    repaint();
}

Rectangle<float> LfoEditor::area() const { return getLocalBounds().toFloat().reduced (10.0f, 9.0f); }

Point<float> LfoEditor::toScreen (float x, float y) const
{
    const auto a = area();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

Point<float> LfoEditor::handlePos (int seg) const
{
    const auto& a = pts[(size_t) seg];
    const auto& b = pts[(size_t) seg + 1];
    const float y = a.y + (b.y - a.y) * GranulumProcessor::segmentShape (0.5f, a.curve);
    return toScreen ((a.x + b.x) * 0.5f, y);
}

int LfoEditor::hitTest (Point<float> m, bool& isHandle) const
{
    isHandle = false;
    int best = -1;
    float bestD = 11.0f;
    for (int i = 0; i < kLfoPoints; ++i)
    {
        const float d = toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) return best;
    for (int s = 0; s < kLfoPoints - 1; ++s)
    {
        const float d = handlePos (s).getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = s; isHandle = true; }
    }
    return best;
}

void LfoEditor::mouseMove (const MouseEvent& e)
{
    bool h = false;
    const int i = hitTest (e.position, h);
    if (i != hoverIndex || h != hoverHandle)
    {
        hoverIndex = i; hoverHandle = h;
        setMouseCursor (i < 0 ? MouseCursor::NormalCursor : h ? MouseCursor::UpDownResizeCursor : MouseCursor::DraggingHandCursor);
    }
}

void LfoEditor::mouseExit (const MouseEvent&) { hoverIndex = -1; }

void LfoEditor::mouseDown (const MouseEvent& e)
{
    pts = proc.getLfoPoints (lfo);
    dragIndex = hitTest (e.position, dragHandle);
    dragStart = e.position;
    if (dragIndex >= 0 && dragHandle) dragStartCurve = pts[(size_t) dragIndex].curve;
}

void LfoEditor::mouseDrag (const MouseEvent& e)
{
    if (dragIndex < 0) return;
    const auto a = area();
    if (dragHandle)
    {
        auto& p0 = pts[(size_t) dragIndex];
        const auto& p1 = pts[(size_t) dragIndex + 1];
        const float up = (dragStart.y - e.position.y) / (a.getHeight() * 0.6f);
        p0.curve = jlimit (-1.0f, 1.0f, dragStartCurve + (p1.y >= p0.y ? -up : up));
    }
    else
    {
        auto& p = pts[(size_t) dragIndex];
        p.y = jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
        if (dragIndex > 0 && dragIndex < kLfoPoints - 1)
            p.x = jlimit (pts[(size_t) dragIndex - 1].x, pts[(size_t) dragIndex + 1].x, (e.position.x - a.getX()) / a.getWidth());
    }
    proc.setLfoPoints (lfo, pts);
    repaint();
}

void LfoEditor::mouseUp (const MouseEvent&) { dragIndex = -1; }

void LfoEditor::mouseDoubleClick (const MouseEvent& e)
{
    bool h = false;
    const int i = hitTest (e.position, h);
    if (i >= 0 && h)
    {
        pts[(size_t) i].curve = 0.0f;
        proc.setLfoPoints (lfo, pts);
    }
}

void LfoEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 8.0f);
    const auto a = area();
    const auto col = Palette::lfo (lfo);

    g.setColour (Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i) g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 4.0f), a.getY(), a.getBottom());
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

    Path p, fill;
    const int n = jmax (8, (int) a.getWidth());
    size_t seg = 0;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / (float) n;
        while (seg + 2 < pts.size() && x > pts[seg + 1].x) ++seg;
        const auto& p0 = pts[seg];
        const auto& p1 = pts[seg + 1];
        const float y = (p1.x - p0.x < 1.0e-5f) ? p1.y
                      : p0.y + (p1.y - p0.y) * GranulumProcessor::segmentShape (jlimit (0.0f, 1.0f, (x - p0.x) / (p1.x - p0.x)), p0.curve);
        const auto q = toScreen (x, y);
        if (i == 0) { p.startNewSubPath (q); fill.startNewSubPath (a.getX(), a.getBottom()); }
        p.lineTo (q);
        fill.lineTo (q);
    }
    fill.lineTo (a.getRight(), a.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (col.withAlpha (0.28f), 0, a.getY(), col.withAlpha (0.02f), 0, a.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // curve handles
    for (int s = 0; s < kLfoPoints - 1; ++s)
    {
        if (pts[(size_t) s + 1].x - pts[(size_t) s].x < 0.02f) continue;
        const auto hp = handlePos (s);
        const bool hot = (hoverIndex == s && hoverHandle) || (dragIndex == s && dragHandle);
        g.setColour (Palette::screen);
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (hp));
        g.setColour (col.withAlpha (hot ? 1.0f : 0.6f));
        g.drawEllipse (Rectangle<float> (hot ? 10.0f : 8.0f, hot ? 10.0f : 8.0f).withCentre (hp), 1.3f);
    }
    // points
    for (int i = 0; i < kLfoPoints; ++i)
    {
        const auto q = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
        const bool hot = (hoverIndex == i && ! hoverHandle) || (dragIndex == i && ! dragHandle);
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (Rectangle<float> (hot ? 18.0f : 14.0f, hot ? 18.0f : 14.0f).withCentre (q));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (q));
    }

    // playhead
    const float ph   = proc.lfoPhaseOut[(size_t) lfo].load();
    const float val  = proc.lfoValueOut[(size_t) lfo].load();
    const float rise = proc.lfoRiseOut[(size_t) lfo].load();
    const auto q = toScreen (ph, rise > 0.001f ? val / rise : val);
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawVerticalLine ((int) q.x, a.getY(), a.getBottom());
    g.setColour (col.withAlpha (0.35f * rise + 0.1f));
    g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (q));
    g.setColour (Colours::white.withAlpha (0.4f + 0.6f * rise));
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (q));

    const bool sync = proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Sync"))->load() > 0.5f;
    const String rate = sync ? GranulumProcessor::divisionNames()[(int) proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Div"))->load()]
                             : String (proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Rate"))->load(), 2) + " Hz";
    g.setColour (Palette::textDim);
    g.setFont (uiFont (10.5f, false, 0.05f));
    g.drawText (rate, r.reduced (10.0f, 5.0f), Justification::topRight, false);
}

// ============================================================================
// Lamp

void Lamp::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    drawLed (g, Rectangle<float> (11.0f, 11.0f).withCentre ({ r.getX() + 8.0f, r.getCentreY() }),
             Palette::accent, Palette::ledOff.darker (0.3f), on ? 1.0f : 0.0f);
    const auto f = uiFont (fontSize, false, 0.08f);
    g.setFont (f);
    g.setColour (on ? (over ? Colours::white : Palette::text) : Palette::textDim.withAlpha (over ? 0.9f : 0.6f));
    auto tr = r.withTrimmedLeft (21.0f);
    g.drawText (getButtonText(), tr, Justification::centredLeft, false);
    if (sub.isNotEmpty())
    {
        const float w = (float) GlyphArrangement::getStringWidthInt (f, getButtonText()) + 8.0f;
        g.setColour (Palette::textDim.withAlpha (on ? 0.9f : 0.5f));
        g.setFont (uiFont (fontSize * 0.75f, false, 0.1f));
        g.drawText (sub, tr.withTrimmedLeft (w), Justification::centredLeft, false);
    }
}

// ============================================================================
// Routing digits "1 2 3"

struct RouteButtons::Digit final : public Button
{
    Digit (int i, RouteButtons& o) : Button (String (i + 1)), index (i), owner (o)
    {
        setClickingTogglesState (true);
        setMouseCursor (MouseCursor::PointingHandCursor);
    }
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const bool on = getToggleState();
        const int st = RouteButtons::style;
        if (st == 1)
        {
            // plain numerals: lit = bright with a small dot under it
            g.setFont (uiFont (r.getHeight() * 0.62f, on, 0.0f));
            g.setColour (on ? Palette::text : Palette::textDim.withAlpha (over ? 0.8f : 0.4f));
            g.drawText (getButtonText(), r.withTrimmedBottom (5.0f), Justification::centred, false);
            if (on)
            {
                g.setColour (Palette::accent);
                g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre ({ r.getCentreX(), r.getBottom() - 2.5f }));
            }
            return;
        }
        if (st == 2)
        {
            // circles: lit = soft filled ring, off = thin outline
            const auto c = Rectangle<float> (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight())).withCentre (r.getCentre()).reduced (1.0f);
            if (on)
            {
                g.setColour (Palette::accent.withAlpha (0.22f));
                g.fillEllipse (c);
                g.setColour (Palette::accent);
                g.drawEllipse (c.reduced (0.6f), 1.2f);
            }
            else
            {
                g.setColour (Palette::textDim.withAlpha (over ? 0.6f : 0.3f));
                g.drawEllipse (c.reduced (0.6f), 1.0f);
            }
            g.setFont (uiFont (c.getHeight() * 0.55f, true));
            g.setColour (on ? Palette::text : Palette::textDim.withAlpha (0.55f));
            g.drawText (getButtonText(), c, Justification::centred, false);
            return;
        }
        if (st == 3)
        {
            // soft segmented, same family as the FX selector
            if (index == 0 || true)
            {
                auto seg = r.expanded (1.5f, 0.0f);
                g.setColour (Palette::screen.withAlpha (0.7f));
                g.fillRect (seg);
            }
            if (on)
            {
                g.setColour (Palette::buttonHi.withAlpha (0.65f));
                g.fillRoundedRectangle (r.reduced (1.5f), 4.0f);
            }
            g.setFont (uiFont (r.getHeight() * 0.56f, on));
            g.setColour (on ? Palette::text : Palette::textDim.withAlpha (over ? 0.8f : 0.45f));
            g.drawText (getButtonText(), r, Justification::centred, false);
            return;
        }
        if (st == 4)
        {
            // lamps: a tiny light next to each number
            const auto led = Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + 6.0f, r.getCentreY() });
            if (on)
            {
                g.setColour (Palette::accent.withAlpha (0.3f));
                g.fillEllipse (led.expanded (2.5f));
                g.setColour (Palette::accent.brighter (0.2f));
                g.fillEllipse (led);
            }
            else
            {
                g.setColour (Palette::textDim.withAlpha (0.45f));
                g.drawEllipse (led.reduced (0.4f), 1.0f);
            }
            g.setFont (uiFont (r.getHeight() * 0.58f, on));
            g.setColour (on ? Palette::text : Palette::textDim.withAlpha (0.55f));
            g.drawText (getButtonText(), r.withTrimmedLeft (11.0f), Justification::centredLeft, false);
            return;
        }
        if (on)
        {
            g.setColour (Palette::accent.withAlpha (over ? 1.0f : 0.85f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        else
        {
            g.setColour (Palette::field);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Palette::textDim.withAlpha (over ? 0.9f : 0.45f));
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        }
        g.setColour (on ? Palette::screen : Palette::textDim.withAlpha (over ? 1.0f : 0.7f));
        g.setFont (uiFont (r.getHeight() * 0.68f, true));
        g.drawText (getButtonText(), r, Justification::centred, false);
    }
    void mouseEnter (const MouseEvent& e) override { Button::mouseEnter (e); if (owner.onHover) owner.onHover (index); }
    void mouseExit  (const MouseEvent& e) override { Button::mouseExit (e);  if (owner.onHover) owner.onHover (-1); }
    const int index;
    RouteButtons& owner;
};

RouteButtons::RouteButtons (GranulumProcessor& p, const char* block, const String& blockName)
{
    for (int o = 0; o < kNumOscs; ++o)
    {
        auto* d = digits.add (new Digit (o, *this));
        d->setTooltip (blockName + " on OSC " + String (o + 1) + ".\nLit = this oscillator goes through " + blockName + ".");
        addAndMakeVisible (d);
        atts.add (new AudioProcessorValueTreeState::ButtonAttachment (p.apvts, ParamID::route (block, o), *d));
    }
}

RouteButtons::~RouteButtons() { atts.clear(); }

void RouteButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / kNumOscs;
    for (auto* d : digits) d->setBounds (r.removeFromLeft (w));
}

// ============================================================================
// Effect tab

FxTab::FxTab (GranulumProcessor& p, const char* onParam, const String& n)
    : param (p.apvts.getParameter (onParam)), name (n)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip ("Click the light to switch " + n + " on/off, click the name to edit it");
}

static void paintFxTabFilled (Graphics& g, Rectangle<int> bounds, bool on, bool selected, bool over, const String& name)
{
    auto r = bounds.toFloat().reduced (1.5f);
    if (selected)
    {
        g.setColour (Palette::accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r.expanded (1.5f), 8.0f);
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (r, 7.0f);
    }
    else
    {
        g.setColour (over ? Palette::buttonHi : Palette::button);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
    }
    // on/off light: a ring that glows when the effect is running
    const auto led = Rectangle<float> (11.0f, 11.0f).withCentre ({ r.getX() + 14.0f, r.getCentreY() });
    if (on)
    {
        g.setColour ((selected ? Colours::white : Palette::accent).withAlpha (0.35f));
        g.fillEllipse (led.expanded (3.0f));
        g.setColour (selected ? Palette::screen : Palette::accent.brighter (0.3f));
        g.fillEllipse (led);
    }
    else
    {
        g.setColour ((selected ? Palette::screen : Palette::textDim).withAlpha (0.55f));
        g.drawEllipse (led.reduced (0.5f), 1.4f);
    }
    g.setColour (selected ? Palette::screen : (on ? Palette::text : Palette::textDim));
    g.setFont (uiFont (13.5f, true, 0.1f));
    g.drawText (name, r.withTrimmedLeft (26.0f).withTrimmedRight (4.0f), Justification::centred, false);
}

void FxTab::paint (Graphics& g)
{
    const bool on = param->getValue() > 0.5f;
    const bool over = isMouseOver();
    auto r = getLocalBounds().toFloat();

    if (style == 0) { paintFxTabFilled (g, getLocalBounds(), on, selected, over, name); return; }

    // the on/off light: small, calm, glows only when the effect runs
    auto lamp = [&] (Point<float> c)
    {
        const auto led = Rectangle<float> (7.0f, 7.0f).withCentre (c);
        if (on)
        {
            g.setColour (Palette::accent.withAlpha (0.28f));
            g.fillEllipse (led.expanded (3.0f));
            g.setColour (Palette::accent.brighter (0.2f));
            g.fillEllipse (led);
        }
        else
        {
            g.setColour (Palette::textDim.withAlpha (0.5f));
            g.drawEllipse (led.reduced (0.3f), 1.1f);
        }
    };

    // layout is measured with the bold font in every state, so the light and the name never move when a tab is selected
    const float fs = getWidth() < 100 ? 11.5f : 12.5f;
    const auto font = uiFont (fs, selected, 0.16f);
    const float tw = (float) GlyphArrangement::getStringWidthInt (uiFont (fs, true, 0.16f), name);
    const float groupW = tw + 15.0f;
    const float gx = r.getCentreX() - groupW * 0.5f;
    ledX = style == 0 ? 14.0f : gx + 3.5f;     // remembered for the click zone

    if (style == 1)
    {
        // text tabs: bright name + thin accent underline on the selected one
        lamp ({ gx + 3.5f, r.getCentreY() - 1.0f });
        g.setFont (font);
        g.setColour (selected ? Palette::text : Palette::textDim.withAlpha (over ? 0.95f : 0.7f));
        g.drawText (name, Rectangle<float> (gx + 15.0f, r.getY(), tw + 2.0f, r.getHeight() - 2.0f), Justification::centredLeft, false);
        if (selected)
        {
            g.setColour (Palette::accent);
            g.fillRoundedRectangle (Rectangle<float> (gx, r.getBottom() - 3.0f, groupW, 2.0f), 1.0f);
        }
        else
        {
            g.setColour (Colours::white.withAlpha (0.06f));
            g.fillRect (Rectangle<float> (r.getX() + 6.0f, r.getBottom() - 1.5f, r.getWidth() - 12.0f, 1.0f));
        }
        return;
    }

    if (style == 2)
    {
        // soft segmented: the selected segment is a slightly lighter, quiet plate
        if (selected)
        {
            g.setColour (Palette::buttonHi.withAlpha (0.55f));
            g.fillRoundedRectangle (r.reduced (3.0f), 6.0f);
            g.setColour (Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (r.reduced (3.5f), 6.0f, 1.0f);
        }
        lamp ({ gx + 3.5f, r.getCentreY() });
        g.setFont (font);
        g.setColour (selected ? Palette::text : Palette::textDim.withAlpha (over ? 0.95f : 0.7f));
        g.drawText (name, Rectangle<float> (gx + 15.0f, r.getY(), tw + 2.0f, r.getHeight()), Justification::centredLeft, false);
        return;
    }

    // style 3: outline pills
    auto pr = r.reduced (2.5f, 3.0f);
    g.setColour (selected ? Palette::accent.withAlpha (0.10f) : Colours::transparentBlack);
    g.fillRoundedRectangle (pr, pr.getHeight() * 0.5f);
    g.setColour (selected ? Palette::accent.withAlpha (0.85f) : Colours::white.withAlpha (over ? 0.18f : 0.10f));
    g.drawRoundedRectangle (pr, pr.getHeight() * 0.5f, 1.0f);
    lamp ({ gx + 3.5f, r.getCentreY() });
    g.setFont (font);
    g.setColour (selected ? Palette::accent.brighter (0.3f) : Palette::textDim.withAlpha (over ? 0.95f : 0.75f));
    g.drawText (name, Rectangle<float> (gx + 15.0f, r.getY(), tw + 2.0f, r.getHeight()), Justification::centredLeft, false);
}

void FxTab::mouseMove (const MouseEvent& e)
{
    setMouseCursor (overLed (e.position) ? MouseCursor::NormalCursor : MouseCursor::PointingHandCursor);
}

void FxTab::mouseDown (const MouseEvent& e)
{
    if (overLed (e.position))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
    }
    if (onSelect) onSelect();
    repaint();
}

// ============================================================================
// Preset bar

PresetBar::PresetBar (GranulumProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next, &save }) addAndMakeVisible (*b);
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    save.onClick = [this] { askSave(); };
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip ("Save the current sound as a preset");
    setMouseCursor (MouseCursor::PointingHandCursor);
    startTimerHz (4);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (58));
    r.removeFromRight (6);
    next.setBounds (r.removeFromRight (28));
    prev.setBounds (r.removeFromLeft (28));
    nameArea = r.reduced (4, 0);
}

void PresetBar::paint (Graphics& g)
{
    auto r = nameArea.toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (Palette::text);
    g.setFont (uiFont (13.0f, false, 0.08f));
    g.drawText (proc.getPresetName().toUpperCase(), r.reduced (10.0f, 0.0f), Justification::centred, true);
    g.setColour (Palette::textDim);
    Path arrow;
    const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
    g.fillPath (arrow);
}

void PresetBar::mouseDown (const MouseEvent& e)
{
    if (nameArea.contains (e.getPosition())) showMenu();
}

void PresetBar::step (int dir)
{
    const auto list = proc.listPresets();
    if (list.isEmpty()) return;
    int cur = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].getFileNameWithoutExtension() == proc.getPresetName()) cur = i;
    const int n = list.size();
    const int idx = cur < 0 ? (dir > 0 ? 0 : n - 1) : ((cur + dir) % n + n) % n;
    proc.loadPreset (list[idx]);
    if (onLoaded) onLoaded();
    repaint();
}

void PresetBar::showMenu()
{
    const auto list = proc.listPresets();
    PopupMenu m;
    m.addItem (1, "INIT");
    m.addSeparator();
    for (int i = 0; i < list.size(); ++i)
        m.addItem (100 + i, list[i].getFileNameWithoutExtension(), true, list[i].getFileNameWithoutExtension() == proc.getPresetName());
    if (list.isEmpty()) m.addItem (2, "(no saved presets yet)", false);
    m.addSeparator();
    m.addItem (3, "Save as...");
    m.addItem (4, "Open presets folder");
    Component::SafePointer<PresetBar> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe, list] (int res)
    {
        if (safe == nullptr || res == 0) return;
        auto& b = *safe;
        if (res == 1) b.proc.resetToInit();
        else if (res == 3) { b.askSave(); return; }
        else if (res == 4) { GranulumProcessor::presetFolder().startAsProcess(); return; }
        else if (res >= 100) b.proc.loadPreset (list[res - 100]);
        if (b.onLoaded) b.onLoaded();
        b.repaint();
    });
}

void PresetBar::askSave()
{
    saveWindow = std::make_unique<AlertWindow> ("Save preset", "Name:", MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.getPresetName() == "INIT" ? String ("My Sound") : proc.getPresetName());
    saveWindow->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    saveWindow->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    saveWindow->setColour (AlertWindow::backgroundColourId, Palette::panel);
    saveWindow->setColour (AlertWindow::textColourId, Palette::text);
    saveWindow->setColour (AlertWindow::outlineColourId, Palette::accent.withAlpha (0.4f));
    Component::SafePointer<PresetBar> safe (this);
    saveWindow->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr) return;
        if (result == 1)
        {
            safe->proc.savePreset (safe->saveWindow->getTextEditorContents ("name"));   // same name = update that preset
        }
        safe->saveWindow.reset();
        safe->repaint();
    }), false);
}

// ============================================================================
// OSC 3 noise picker

NoisePicker::NoisePicker (GranulumProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next }) addAndMakeVisible (*b);
    prev.onClick = [this] { setIndex (index() - 1); };
    next.onClick = [this] { setIndex (index() + 1); };
    setMouseCursor (MouseCursor::PointingHandCursor);
    startTimerHz (10);
}

int NoisePicker::index() const { return (int) proc.apvts.getRawParameterValue (ParamID::osc3Type)->load(); }

void NoisePicker::setIndex (int i)
{
    const int n = GranulumProcessor::noiseNames().size();
    i = ((i % n) + n) % n;
    if (auto* prm = proc.apvts.getParameter (ParamID::osc3Type))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        prm->endChangeGesture();
    }
    repaint();
}

void NoisePicker::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (24).withSizeKeepingCentre (24, jmin (r.getHeight(), 26)));
    next.setBounds (r.removeFromRight (24).withSizeKeepingCentre (24, jmin (r.getHeight(), 26)));
    body = r.reduced (4, 0);
}

void NoisePicker::paint (Graphics& g)
{
    auto r = body.toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 7.0f);

    const bool on = proc.apvts.getRawParameterValue (ParamID::osc3On)->load() > 0.5f;
    // mini waveform across the box
    if (auto pk = proc.getNoisePeaks (index()); pk != nullptr && ! pk->maxs.empty())
    {
        auto w = r.reduced (8.0f, 5.0f);
        const int cols = jmax (2, (int) w.getWidth() / 2);
        const int bins = (int) pk->maxs.size();
        Path p;
        for (int x = 0; x < cols; ++x)
        {
            const int b0 = x * bins / cols, b1 = jmax (b0 + 1, (x + 1) * bins / cols);
            float m = 0.0f;
            for (int b = b0; b < b1; ++b) m = jmax (m, pk->maxs[(size_t) b], -pk->mins[(size_t) b]);
            const float px = w.getX() + w.getWidth() * (float) x / (float) (cols - 1);
            p.addRectangle (px, w.getCentreY() - m * w.getHeight() * 0.5f, 1.2f, jmax (1.0f, m * w.getHeight()));
        }
        g.setColour (Palette::accent.withAlpha (on ? 0.28f : 0.12f));
        g.fillPath (p);
    }
    g.setColour (on ? Palette::ice : Palette::textDim);
    g.setFont (uiFont (13.5f, true, 0.14f));
    g.drawText (GranulumProcessor::noiseNames()[index()], r, Justification::centred, false);
}

void NoisePicker::mouseDown (const MouseEvent& e)
{
    if (! body.contains (e.getPosition())) return;
    PopupMenu m;
    const auto& names = GranulumProcessor::noiseNames();
    for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == index());
    Component::SafePointer<NoisePicker> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int res)
    {
        if (safe != nullptr && res > 0) safe->setIndex (res - 1);
    });
}

// ============================================================================
// Editor

GranulumEditor::GranulumEditor (GranulumProcessor& p)
    : AudioProcessorEditor (&p), proc (p), field (p), harmonics (p), noisePicker (p), meterView (p), presetBar (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard), lfoEditor (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;

    auto add = [&] (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* name, Knob::Size sz = Knob::Size::Normal)
    {
        group.push_back (std::make_unique<Knob> (proc, id, name, sz));
        addAndMakeVisible (*group.back());
        allKnobs.push_back (group.back().get());
    };

    add (heroKnobs,  ParamID::bloom,    "Bloom",   Knob::Size::Hero);
    add (heroKnobs,  ParamID::smear,    "Smear",   Knob::Size::Hero);
    add (heroKnobs,  ParamID::scatter,  "Scatter", Knob::Size::Hero);
    add (heroKnobs,  ParamID::shimmer,  "Shimmer", Knob::Size::Hero);
    add (heroKnobs,  ParamID::drift,    "Drift",   Knob::Size::Hero);
    add (heroKnobs,  ParamID::chime,    "Chime",   Knob::Size::Hero);
    add (smallKnobs, ParamID::gsize,    "Size",     Knob::Size::Small);
    add (smallKnobs, ParamID::speed,    "Speed",    Knob::Size::Small);
    add (smallKnobs, ParamID::position, "Position", Knob::Size::Small);
    add (smallKnobs, ParamID::density,  "Density",  Knob::Size::Small);
    add (smallKnobs, ParamID::gmix,     "Mix",      Knob::Size::Small);

    add (osc1Knobs, ParamID::osc1Vol,  "Vol",  Knob::Size::Small);
    add (osc1Knobs, ParamID::osc1Oct,  "Oct",  Knob::Size::Small);
    add (osc1Knobs, ParamID::osc1Semi, "Semi", Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Vol,  "Vol",  Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Oct,  "Oct",  Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Semi, "Semi", Knob::Size::Small);
    add (osc3Knobs, ParamID::osc3Vol,   "Vol",   Knob::Size::Small);
    add (osc3Knobs, ParamID::osc3Color, "Color", Knob::Size::Small);

    add (filterKnobs, ParamID::cutoff, "Cutoff");
    add (filterKnobs, ParamID::reso,   "Reso");
    add (filter2Knobs, ParamID::cutoff2, "Cutoff");
    add (filter2Knobs, ParamID::reso2,   "Reso");

    add (distKnobs,  ParamID::distDrive, "Drive",    Knob::Size::Small);
    add (distKnobs,  ParamID::distTone,  "Tone",     Knob::Size::Small);
    add (distKnobs,  ParamID::distMix,   "Mix",      Knob::Size::Small);
    add (delayKnobs, ParamID::delayTime, "Time",     Knob::Size::Small);
    add (delayKnobs, ParamID::delayFb,   "Feedback", Knob::Size::Small);
    add (delayKnobs, ParamID::delayTone, "Tone",     Knob::Size::Small);
    add (delayKnobs, ParamID::delayMix,  "Mix",      Knob::Size::Small);
    add (revKnobs,   ParamID::revMix,    "Mix",      Knob::Size::Small);
    add (revKnobs,   ParamID::revSize,   "Size",     Knob::Size::Small);
    add (revKnobs,   ParamID::revLowCut, "Low Cut",  Knob::Size::Small);

    add (globalKnobs, ParamID::porta,  "Porta",  Knob::Size::Small);
    add (globalKnobs, ParamID::output, "Output", Knob::Size::Small);

    heroKnobs[0]->slider.setTooltip ("BLOOM: more, longer, wider grains. The sound opens up into a cloud.");
    heroKnobs[1]->slider.setTooltip ("SMEAR: grains melt into long soft strokes with a gentle detune.");
    heroKnobs[2]->slider.setTooltip ("SCATTER: grains jump around the source and across the stereo field.");
    heroKnobs[3]->slider.setTooltip ("SHIMMER: some grains sparkle an octave (or a twelfth) up, some play backwards.");
    heroKnobs[4]->slider.setTooltip ("DRIFT: pitch, position and pan slowly wander. The sound breathes.");
    heroKnobs[5]->slider.setTooltip ("CHIME: every grain rings tuned resonances. Low = soft glassy overtones, high = long metallic bells.");
    osc3Knobs[1]->slider.setTooltip ("COLOR: left = darker, right = brighter");
    for (auto& k : osc3Knobs) k->setCompact (true);
    for (auto& k : osc2Knobs) k->setCompact (true);

    // lamps
    for (auto* l : { &osc1Lamp, &osc2Lamp, &osc3Lamp, &grainLamp }) addAndMakeVisible (*l);
    osc1Lamp.fontSize = osc2Lamp.fontSize = osc3Lamp.fontSize = 13.5f;
    osc1LampA   = std::make_unique<ButtonAtt> (s, ParamID::osc1On,   osc1Lamp);
    osc2LampA   = std::make_unique<ButtonAtt> (s, ParamID::osc2On,   osc2Lamp);
    osc3LampA   = std::make_unique<ButtonAtt> (s, ParamID::osc3On,   osc3Lamp);
    grainLampA  = std::make_unique<ButtonAtt> (s, ParamID::grainOn,  grainLamp);

    for (auto* b : { &osc1Grain, &osc2Grain, &osc3Grain, &delaySync, &delayPing })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    osc1GrainA = std::make_unique<ButtonAtt> (s, ParamID::osc1Grain, osc1Grain);
    osc2GrainA = std::make_unique<ButtonAtt> (s, ParamID::osc2Grain, osc2Grain);
    osc3GrainA = std::make_unique<ButtonAtt> (s, ParamID::osc3Grain, osc3Grain);
    delaySyncA = std::make_unique<ButtonAtt> (s, ParamID::delaySync, delaySync);
    delayPingA = std::make_unique<ButtonAtt> (s, ParamID::delayPing, delayPing);
    osc1Grain.setTooltip ("Send OSC 1 through the granular block");
    osc2Grain.setTooltip ("Send OSC 2 through the granular block. Off: the sample loops from POSITION");
    osc3Grain.setTooltip ("Send OSC 3 through the granular block. Off: the noise plays clean");
    delaySync.setTooltip ("Lock the delay time to the project tempo");
    delayPing.setTooltip ("Echoes bounce between left and right");

    for (auto* b : { &polyButton, &monoButton }) addAndMakeVisible (*b);
    polyButton.onClick = [this] { if (auto* x = proc.apvts.getParameter (ParamID::mono)) x->setValueNotifyingHost (0.0f); };
    monoButton.onClick = [this] { if (auto* x = proc.apvts.getParameter (ParamID::mono)) x->setValueNotifyingHost (1.0f); };

    filterType.addItemList (FilterUnit::typeNames(), 1);
    filterType2.addItemList (FilterUnit::typeNames(), 1);
    distType.addItemList (Distortion::typeNames(), 1);
    delayDiv.addItemList (GranulumProcessor::divisionNames(), 1);
    for (auto* c : { &filterType, &filterType2, &distType, &delayDiv }) addAndMakeVisible (*c);
    filterTypeA  = std::make_unique<ComboAtt> (s, ParamID::ftype, filterType);
    filterType2A = std::make_unique<ComboAtt> (s, ParamID::ftype2, filterType2);
    filterType.setTooltip ("COMB rings at the played note. VOWEL: CUTOFF morphs A-E-I-O-U, RESO sharpens it.");
    filterType2.setTooltip ("COMB rings at the played note. VOWEL: CUTOFF morphs A-E-I-O-U, RESO sharpens it.");
    filter2Knobs[0]->slider.setTooltip ("Cutoff follows the note you play (set at C3).");
    {
        const char* fOn[2] = { ParamID::filterOn, ParamID::filter2On };
        const char* fName[2] = { "FILTER 1", "FILTER 2" };
        for (int i = 0; i < 2; ++i)
        {
            filterTabs[(size_t) i] = std::make_unique<FxTab> (proc, fOn[i], fName[i]);
            filterTabs[(size_t) i]->onSelect = [this, i] { selectFilter (i); };
            addAndMakeVisible (*filterTabs[(size_t) i]);
        }
    }
    distTypeA   = std::make_unique<ComboAtt> (s, ParamID::distType, distType);
    delayDivA   = std::make_unique<ComboAtt> (s, ParamID::delayDiv, delayDiv);
    filterKnobs[0]->slider.setTooltip ("Cutoff follows the note you play (set at C3). The readout shows the note where it cuts.");

    // effects tabs + routing digits
    const char* fxOn[3]   = { ParamID::distOn, ParamID::delayOn, ParamID::revOn };
    const char* fxName[3] = { "DIST", "DELAY", "REVERB" };
    for (int i = 0; i < 3; ++i)
    {
        fxTabs[(size_t) i] = std::make_unique<FxTab> (proc, fxOn[i], fxName[i]);
        fxTabs[(size_t) i]->onSelect = [this, i] { selectFx (i); };
        addAndMakeVisible (*fxTabs[(size_t) i]);
    }
    const char* routeNames[kNumRouteBlocks] = { "FILTER 1", "DISTORTION", "DELAY", "REVERB", "FILTER 2" };
    for (int b = 0; b < kNumRouteBlocks; ++b)
    {
        routes[(size_t) b] = std::make_unique<RouteButtons> (proc, kRouteBlocks[b], routeNames[b]);
        routes[(size_t) b]->onHover = [this] (int o) { setHoverOsc (o); };
        addAndMakeVisible (*routes[(size_t) b]);
    }

    addAndMakeVisible (field);
    addAndMakeVisible (harmonics);
    addAndMakeVisible (noisePicker);
    addAndMakeVisible (meterView);
    addAndMakeVisible (presetBar);
    presetBar.onLoaded = [this] { lfoEditor.setLfo (selectedLfo); };

    for (auto* b : { &rndGrains, &rndFx, &rndBars, &randomButton, &initButton, &loadButton, &chordButton }) addAndMakeVisible (*b);
    rndGrains.onClick    = [this] { proc.randomizeSection (0); };
    rndFx.onClick        = [this] { proc.randomizeSection (2); };
    rndBars.onClick      = [this] { proc.randomizeSection (3); };
    randomButton.onClick = [this] { proc.randomize(); };
    initButton.onClick   = [this] { proc.resetToInit(); lfoEditor.setLfo (selectedLfo); };
    loadButton.onClick   = [this] { launchChooser(); };
    chordButton.onClick  = [this] { showChordPicker(); };
    initButton.setTooltip ("Start from scratch: one sine harmonic, everything else off");
    chordButton.setTooltip ("Play a chord or interval from every note (OSC 1)");

    // LFO strip
    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l] = std::make_unique<LfoTab> (proc, l);
        lfoTabs[(size_t) l]->onSelect = [this] (int i) { selectLfo (i); };
        addAndMakeVisible (*lfoTabs[(size_t) l]);

        auto c = std::make_unique<LfoControls>();
        c->preset.addItemList (GranulumProcessor::lfoPresetNames(), 1);
        c->preset.setTextWhenNothingSelected ("SHAPE...");
        c->preset.onChange = [this, l]
        {
            auto& box = lfoControls[(size_t) l]->preset;
            if (box.getSelectedItemIndex() < 0) return;
            proc.setLfoPoints (l, GranulumProcessor::lfoPreset (box.getSelectedItemIndex()));
            lfoEditor.setLfo (l);
            box.setSelectedId (0, dontSendNotification);
        };
        c->div.addItemList (GranulumProcessor::divisionNames(), 1);
        c->mode.addItemList ({ "FREE", "RETRIG", "ENV" }, 1);
        c->sync.setClickingTogglesState (true);
        c->sync.setTooltip ("Lock the LFO to the project tempo");
        c->mode.setTooltip ("FREE: runs on its own.  RETRIG: restarts with each note.  ENV: runs once per note and holds.");
        c->rate = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rate"), "Rate", Knob::Size::Small);
        c->rise = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rise"), "Rise", Knob::Size::Small);
        c->rise->slider.setTooltip ("RISE: the LFO fades in over this time after a note starts");
        allKnobs.push_back (c->rate.get());
        c->divA  = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Div"), c->div);
        c->modeA = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Mode"), c->mode);
        c->syncA = std::make_unique<ButtonAtt> (s, ParamID::lfo (l, "Sync"), c->sync);
        for (Component* comp : { (Component*) &c->preset, (Component*) &c->div, (Component*) &c->mode, (Component*) &c->sync,
                                 (Component*) c->rate.get(), (Component*) c->rise.get() })
            addChildComponent (comp);
        lfoControls[(size_t) l] = std::move (c);
    }
    addAndMakeVisible (lfoEditor);
    lfoEditor.setTooltip ("Drag the points. Drag the small rings to bend a segment, double-click a ring to straighten it.");

    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Palette::whiteKey);
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Palette::blackKey);
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Palette::blackKey.withAlpha (0.6f));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent.withAlpha (0.85f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.25f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keyboard.setColour (MidiKeyboardComponent::textLabelColourId, Palette::blackKey);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, Palette::panel);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonArrowColourId, Palette::text);
    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    selectLfo (0);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (1080.0 / 800.0);
    setResizeLimits (864, 640, 1620, 1200);
    setSize (1080, 800);
    selectFx (0);
    selectFilter (0);
    startTimerHz (30);
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
        for (Component* comp : { (Component*) &c.preset, (Component*) &c.div, (Component*) &c.mode, (Component*) &c.sync,
                                 (Component*) c.rate.get(), (Component*) c.rise.get() })
            comp->setVisible (l == i);
    }
    lfoEditor.setLfo (i);
}

void GranulumEditor::selectFx (int i)
{
    selectedFx = i;
    for (int t = 0; t < 3; ++t) { fxTabs[(size_t) t]->selected = (t == i); fxTabs[(size_t) t]->repaint(); }
    for (auto& k : distKnobs)  k->setVisible (i == 0);
    for (auto& k : delayKnobs) k->setVisible (i == 1);
    for (auto& k : revKnobs)   k->setVisible (i == 2);
    distType.setVisible (i == 0);
    delaySync.setVisible (i == 1);
    delayPing.setVisible (i == 1);
    delayDiv.setVisible (i == 1);
    for (int b = 1; b < 4; ++b) routes[(size_t) b]->setVisible (b - 1 == i);
    resized();
    repaint();
}

void GranulumEditor::selectFilter (int i)
{
    selectedFilter = i;
    for (int t = 0; t < 2; ++t) { filterTabs[(size_t) t]->selected = (t == i); filterTabs[(size_t) t]->repaint(); }
    for (auto& k : filterKnobs)  k->setVisible (i == 0);
    for (auto& k : filter2Knobs) k->setVisible (i == 1);
    filterType.setVisible (i == 0);
    filterType2.setVisible (i == 1);
    routes[0]->setVisible (i == 0);
    routes[4]->setVisible (i == 1);
    repaint();
}

void GranulumEditor::setHoverOsc (int o)
{
    if (hoverOsc == o) return;
    hoverOsc = o;
    repaint();
}

void GranulumEditor::showChordPicker()
{
    auto picker = std::make_unique<ChordPicker> (proc);
    picker->setLookAndFeel (&look);
    CallOutBox::launchAsynchronously (std::move (picker), chordButton.getBounds(), this);
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
    if (proc.loadSample (f, err)) statusText.clear();
    else                          statusText = err;
    repaint (dropZone);
}

void GranulumEditor::timerCallback()
{
    auto& s = proc.apvts;

    const int chord = (int) s.getRawParameterValue (ParamID::chord)->load();
    const String ct = "CHORD: " + GranulumProcessor::chordNames()[chord] + String::fromUTF8 (" \xe2\x96\xbe");
    if (chordButton.getButtonText() != ct) chordButton.setButtonText (ct);
    chordButton.setToggleState (chord != 0, dontSendNotification);

    const bool mono = s.getRawParameterValue (ParamID::mono)->load() > 0.5f;
    polyButton.setToggleState (! mono, dontSendNotification);
    monoButton.setToggleState (mono, dontSendNotification);

    auto dim = [] (std::initializer_list<Component*> comps, bool on) { for (auto* c : comps) c->setAlpha (on ? 1.0f : 0.38f); };
    auto dimK = [] (std::vector<std::unique_ptr<Knob>>& ks, bool on) { for (auto& k : ks) k->setAlpha (on ? 1.0f : 0.38f); };
    auto on = [&s] (const char* id) { return s.getRawParameterValue (id)->load() > 0.5f; };
    const bool o1 = on (ParamID::osc1On), o2 = on (ParamID::osc2On), o3 = on (ParamID::osc3On);
    const bool gOn = on (ParamID::grainOn), fOn = on (ParamID::filterOn), f2On = on (ParamID::filter2On);
    const bool dOn = on (ParamID::distOn), lOn = on (ParamID::delayOn), rOn = on (ParamID::revOn);
    dimK (osc1Knobs, o1); dim ({ &harmonics, &chordButton, &rndBars }, o1);
    dimK (osc2Knobs, o2);
    dimK (osc3Knobs, o3); dim ({ &noisePicker }, o3);
    dim ({ &osc1Grain }, o1 && gOn);
    dim ({ &osc2Grain }, o2 && gOn);
    dim ({ &osc3Grain }, o3 && gOn);
    dimK (heroKnobs, gOn); dimK (smallKnobs, gOn); dim ({ &rndGrains }, gOn);
    dimK (filterKnobs, fOn); dim ({ &filterType, routes[0].get() }, fOn);
    dimK (filter2Knobs, f2On); dim ({ &filterType2, routes[4].get() }, f2On);
    for (auto& t : filterTabs) t->repaint();
    dimK (distKnobs, dOn);  dim ({ &distType, routes[1].get() }, dOn);
    dimK (delayKnobs, lOn); dim ({ &delaySync, &delayPing, &delayDiv, routes[2].get() }, lOn);
    dimK (revKnobs, rOn);   dim ({ routes[3].get() }, rOn);
    for (auto& t : fxTabs) t->repaint();

    // delay: TIME knob in free mode, division box when synced
    const bool dsync = on (ParamID::delaySync);
    delayKnobs[0]->setVisible (selectedFx == 1 && ! dsync);
    delayDiv.setVisible (selectedFx == 1 && dsync);

    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& c = *lfoControls[(size_t) l];
        const bool sync = c.sync.getToggleState();
        c.rate->setEnabled (! sync);
        c.rate->setAlpha (sync ? 0.35f : 1.0f);
        c.div.setEnabled (sync);
        c.div.setAlpha (sync ? 1.0f : 0.5f);
    }

    if (const auto v = proc.modVersion.load(); v != shownModVersion)
    {
        shownModVersion = v;
        for (auto* k : allKnobs) k->refreshMods();
        for (auto& t : lfoTabs) t->repaint();
    }
    for (auto* k : allKnobs) k->tick();
    lfoTabs[(size_t) selectedLfo]->repaint();

    meter    = jmax (proc.outputLevel.load(), meter * 0.85f);
    activity = jmax ((float) proc.vizEngine().vizCount.load() / 20.0f, activity * 0.8f);
    repaint (dropZone);
}

void GranulumEditor::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const float sc = full.getWidth() / 1080.0f;

    g.setGradientFill (ColourGradient (Palette::bgTop, 0, 0, Palette::bgBottom, 0, full.getHeight(), false));
    g.fillAll();
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (full.reduced (1.0f), 14.0f, 1.0f);

    g.setColour (Palette::text);
    g.setFont (uiFont (40.0f * sc, false, 0.04f));
    g.drawText ("GRAIN FORM", titleArea.withHeight ((int) (46 * sc)), Justification::bottomLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (16.0f * sc, false, 0.3f));
    g.drawText ("SELECTIVE GRANULATION", titleArea.withTrimmedTop ((int) (49 * sc)), Justification::topLeft);

    for (auto fr : { screenFrame, lfoFrame })
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (fr.toFloat().translated (0, 2.0f), 14.0f);
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (fr.toFloat(), 14.0f);
    }
    g.setColour (Palette::label);
    g.setFont (uiFont (13.0f * sc, false, 0.12f));
    g.drawText ("GRAIN FIELD", screenFrame.getX() + (int) (18 * sc), screenFrame.getY() + (int) (8 * sc), (int) (160 * sc), (int) (26 * sc), Justification::centredLeft);


    for (auto a : { osc1Area, osc2Area, osc3Area })
    {
        g.setColour (Colours::black.withAlpha (0.14f));
        g.fillRoundedRectangle (a.toFloat(), 10.0f);
    }

    // OSC 2 drop zone
    {
        auto z = dropZone.toFloat();
        const bool has = proc.hasSample();
        g.setColour (Palette::screen.withAlpha (fileHover ? 1.0f : 0.8f));
        g.fillRoundedRectangle (z, 9.0f);
        if (! has || fileHover)
        {
            Path border; border.addRoundedRectangle (z.reduced (1.5f), 8.0f);
            Path dashed;
            const float dl[] = { 5.0f, 4.0f };
            PathStrokeType (1.2f).createDashedStroke (dashed, border, dl, 2);
            g.setColour ((fileHover ? Palette::accent : Palette::accentSoft).withAlpha (fileHover ? 0.9f : 0.45f));
            g.fillPath (dashed);
        }
        g.setFont (uiFont (13.0f * sc, false, 0.05f));
        if (statusText.isNotEmpty())
        {
            g.setColour (Palette::hit);
            g.drawText (statusText, z.reduced (10.0f, 0.0f), Justification::centred, true);
        }
        else if (! has)
        {
            g.setColour (fileHover ? Colours::white : Palette::accentSoft);
            g.drawText ("Drop a WAV file here", z, Justification::centred, false);
        }
        else
        {
            const auto name = File::createFileWithoutCheckingPath (proc.getSampleName()).getFileNameWithoutExtension();
            g.setColour (Palette::ice);
            g.drawText (name, z.reduced (12.0f, 0.0f).withTrimmedBottom (z.getHeight() * 0.3f), Justification::centredLeft, true);
            g.setColour (Palette::accentSoft);
            g.setFont (uiFont (11.0f * sc, false, 0.05f));
            g.drawText (String (proc.getSampleLengthSeconds(), 2) + " s", z.reduced (12.0f, 0.0f).withTrimmedTop (z.getHeight() * 0.45f), Justification::centredLeft, false);
        }
    }

    // dividers
    if (! heroKnobs.empty())
    {
        auto hr = heroKnobs.front()->getBounds();
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine (hr.getBottom() + 3, (float) grainArea.getX(), (float) grainArea.getRight());
    }
    g.setColour (Colours::white.withAlpha (0.06f));
    for (auto x : { filterArea.getX() - 9, fxArea.getX() - 9 })
        g.drawVerticalLine (x, (float) grainArea.getY() + 4.0f, (float) grainArea.getBottom() - 4.0f);

    g.setColour (Palette::text);
    g.setFont (uiFont (14.5f * sc, false, 0.08f));
    g.drawText ("EFFECTS", fxLabel, Justification::centredLeft, false);
    if (FxTab::style == 2 && filterTabs[0] != nullptr)
    {
        auto seg = filterTabs[0]->getBounds().getUnion (filterTabs[1]->getBounds()).toFloat();
        g.setColour (Palette::screen.withAlpha (0.7f));
        g.fillRoundedRectangle (seg, 8.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (seg.reduced (0.5f), 8.0f, 1.0f);
    }
    if (FxTab::style == 2 && fxTabs[0] != nullptr)
    {
        auto seg = fxTabs[0]->getBounds().getUnion (fxTabs[2]->getBounds()).toFloat();
        g.setColour (Palette::screen.withAlpha (0.7f));
        g.fillRoundedRectangle (seg, 8.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (seg.reduced (0.5f), 8.0f, 1.0f);
    }
    g.setColour (Palette::label);
    g.setFont (uiFont (10.5f * sc, false, 0.08f));
    if (selectedFx == 0) g.drawText ("TYPE", distTypeLabel, Justification::centredLeft, false);
    if (selectedFx == 1 && proc.apvts.getRawParameterValue (ParamID::delaySync)->load() > 0.5f)
        g.drawText ("TIME", distTypeLabel, Justification::centredLeft, false);

    g.drawText ("SHAPE",    lfoPresetLabel, Justification::centredLeft, false);
    g.drawText ("MODE",     lfoModeLabel,   Justification::centredLeft, false);
    g.drawText ("DIVISION", lfoDivLabel,    Justification::centredLeft, false);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawVerticalLine (voiceArea.getX() - 9, (float) voiceArea.getY(), (float) voiceArea.getBottom());
    g.setColour (Palette::label);
    g.setFont (uiFont (12.0f * sc, true, 0.08f));
    g.drawText ("VOICE", voiceArea.withHeight ((int) (16 * sc)), Justification::centredLeft, false);

    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (keyboard.getBounds().toFloat().expanded (3.0f), 6.0f);
}

void GranulumEditor::paintOverChildren (Graphics& g)
{
    // hovering a routing digit outlines that oscillator
    if (hoverOsc < 0 || hoverOsc > 2) return;
    const auto a = (hoverOsc == 0 ? osc1Area : hoverOsc == 1 ? osc2Area : osc3Area).toFloat().expanded (2.0f);
    g.setColour (Palette::accent.withAlpha (0.10f));
    g.fillRoundedRectangle (a, 11.0f);
    g.setColour (Palette::accent.withAlpha (0.9f));
    g.drawRoundedRectangle (a, 11.0f, 2.0f);
}

void GranulumEditor::resized()
{
    const float sc = (float) getWidth() / 1080.0f;
    auto S = [sc] (float v) { return (int) std::round (v * sc); };

    auto r = getLocalBounds().reduced (S (30), S (16));

    // ---- header ----
    auto header = r.removeFromTop (S (66));
    gemsArea = header.removeFromRight (S (150)).withSizeKeepingCentre (S (150), S (44));
    meterView.setBounds (gemsArea.withTrimmedTop (S (14)));
    header.removeFromRight (S (6));
    globalKnobs[1]->setBounds (header.removeFromRight (S (68)));   // output
    header.removeFromRight (S (14));
    randomButton.setBounds (header.removeFromRight (S (118)).withSizeKeepingCentre (S (118), S (30)));
    header.removeFromRight (S (8));
    initButton.setBounds (header.removeFromRight (S (60)).withSizeKeepingCentre (S (60), S (30)));
    header.removeFromRight (S (16));
    presetBar.setBounds (header.removeFromRight (S (270)).withSizeKeepingCentre (S (270), S (30)));
    titleArea = header.withTrimmedLeft (S (4));
    r.removeFromTop (S (6));

    keyboard.setBounds (r.removeFromBottom (S (46)).reduced (S (2), S (2)));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    keyboard.setLowestVisibleKey (36);
    r.removeFromBottom (S (10));

    lfoFrame = r.removeFromBottom (S (124));
    r.removeFromBottom (S (10));

    auto blocks = r.removeFromBottom (S (214));
    r.removeFromBottom (S (10));

    // ---- screen: grain field on the left, OSC 1 / 2 / 3 stacked on the right ----
    screenFrame = r;
    auto inside = r.reduced (S (14), S (8));
    inside.removeFromTop (S (28));
    ledArea = {};

    auto oscCol = inside.removeFromRight (S (500));
    oscCol.setTop (screenFrame.getY() + S (8));
    inside.removeFromRight (S (12));
    field.setBounds (inside);

    {
        const int rowH = S (54);
        osc3Area = oscCol.removeFromBottom (rowH);
        oscCol.removeFromBottom (S (6));
        osc2Area = oscCol.removeFromBottom (rowH);
        oscCol.removeFromBottom (S (6));
        osc1Area = oscCol;

        auto a1 = osc1Area.reduced (S (8), S (5));
        auto h1 = a1.removeFromTop (S (26));
        rndBars.setBounds (h1.removeFromRight (S (70)).reduced (0, S (1)));
        h1.removeFromRight (S (6));
        osc1Grain.setBounds (h1.removeFromRight (S (60)).reduced (0, S (1)));
        h1.removeFromRight (S (6));
        chordButton.setBounds (h1.removeFromRight (S (124)).reduced (0, S (1)));
        osc1Lamp.setBounds (h1);
        a1.removeFromTop (S (4));
        auto k1 = a1.removeFromRight (S (150));
        a1.removeFromRight (S (6));
        harmonics.setBounds (a1);
        const int kw1 = k1.getWidth() / 3;
        for (auto& k : osc1Knobs) k->setBounds (k1.removeFromLeft (kw1).withSizeKeepingCentre (kw1, jmin (k1.getHeight(), S (78))));

        // OSC 2: one row  [lamp] [drop zone] [vol oct semi] [GRAIN/LOAD]
        auto a2 = osc2Area.reduced (S (8), S (4));
        osc2Lamp.setBounds (a2.removeFromLeft (S (118)));
        auto btn2 = a2.removeFromRight (S (62));
        loadButton.setBounds (btn2.removeFromBottom (btn2.getHeight() / 2).reduced (0, S (1)));
        osc2Grain.setBounds (btn2.reduced (0, S (1)));
        a2.removeFromRight (S (4));
        for (int i = (int) osc2Knobs.size() - 1; i >= 0; --i)
            osc2Knobs[(size_t) i]->setBounds (a2.removeFromRight (S (46)));
        a2.removeFromRight (S (6));
        dropZone = a2.reduced (0, S (2));

        // OSC 3: one row  [lamp] [noise picker] [vol color] [GRAIN]
        auto a3 = osc3Area.reduced (S (8), S (4));
        osc3Lamp.setBounds (a3.removeFromLeft (S (118)));
        osc3Grain.setBounds (a3.removeFromRight (S (62)).withSizeKeepingCentre (S (62), S (26)));
        a3.removeFromRight (S (4));
        for (int i = (int) osc3Knobs.size() - 1; i >= 0; --i)
            osc3Knobs[(size_t) i]->setBounds (a3.removeFromRight (S (46)));
        a3.removeFromRight (S (6));
        noisePicker.setBounds (a3.withSizeKeepingCentre (a3.getWidth(), S (38)));
    }

    // ---- blocks ----
    const int gap = S (18);
    const float colW = (float) (blocks.getWidth() - gap * 2) / 10.0f;
    grainArea  = blocks.removeFromLeft ((int) (colW * 5.0f)); blocks.removeFromLeft (gap);
    filterArea = blocks.removeFromLeft ((int) (colW * 1.85f)); blocks.removeFromLeft (gap);
    fxArea     = blocks;

    auto headerRow = [&] (Rectangle<int> area, Component& left, Button& rnd)
    {
        rnd.setBounds (area.getRight() - S (74), area.getY(), S (74), S (26));
        left.setBounds (area.getX(), area.getY(), area.getWidth() - S (80), S (26));
    };
    headerRow (grainArea,  grainLamp,  rndGrains);
    {
        auto tabs = filterArea.withHeight (S (30));
        const int tw = tabs.getWidth() / 2;
        filterTabs[0]->setBounds (tabs.removeFromLeft (tw));
        filterTabs[1]->setBounds (tabs);
    }
    rndFx.setBounds (fxArea.getRight() - S (74), fxArea.getY(), S (74), S (26));
    fxLabel = { fxArea.getX(), fxArea.getY(), fxArea.getWidth() - S (80), S (26) };

    auto layoutRow = [] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> row)
    {
        const int cw = row.getWidth() / (int) ks.size();
        for (int i = 0; i < (int) ks.size(); ++i)
            ks[(size_t) i]->setBounds (row.getX() + i * cw, row.getY(), cw, row.getHeight());
    };

    {
        auto g = grainArea.withTrimmedTop (S (32));
        auto hero = g.removeFromTop ((int) (g.getHeight() * 0.58f));
        g.removeFromTop (S (6));
        layoutRow (heroKnobs, hero);
        layoutRow (smallKnobs, g);
    }
    {
        auto f = filterArea.withTrimmedTop (S (32));
        auto row1 = f.removeFromTop ((int) (f.getHeight() * 0.64f));
        layoutRow (filterKnobs, row1);
        layoutRow (filter2Knobs, row1);
        auto row2 = f.withSizeKeepingCentre (f.getWidth(), S (28));
        routes[0]->setBounds (row2.removeFromRight (S (66)));
        routes[4]->setBounds (routes[0]->getBounds());
        row2.removeFromRight (S (6));
        filterType.setBounds (row2);
        filterType2.setBounds (row2);
    }
    {
        auto e = fxArea.withTrimmedTop (S (32));
        auto tabs = e.removeFromTop (S (32));
        const int tw = tabs.getWidth() / 3;
        for (auto& t : fxTabs) t->setBounds (tabs.removeFromLeft (tw).reduced (S (2), 0));
        e.removeFromTop (S (6));

        auto ctl = e.removeFromTop (S (26));
        for (int b = 1; b < 4; ++b) routes[(size_t) b]->setBounds (ctl.withTrimmedLeft (ctl.getWidth() - S (66)));
        fxContent = e.withTrimmedTop (S (4));

        // DIST: type + 3 knobs
        {
            auto row = fxContent;
            const int cw = row.getWidth() / 4;
            auto typeCol = row.removeFromLeft (cw);
            distTypeLabel = typeCol.removeFromTop (S (16)).withTrimmedLeft (S (4));
            distType.setBounds (typeCol.removeFromTop (S (28)).reduced (S (2), 0));
            layoutRow (distKnobs, row);
        }
        // DELAY: sync / ping on the control line, 4 knobs (TIME swaps with the division box)
        {
            auto c2 = ctl.withTrimmedRight (S (72));
            delaySync.setBounds (c2.removeFromLeft (S (58)));
            c2.removeFromLeft (S (6));
            delayPing.setBounds (c2.removeFromLeft (S (92)));
            auto row = fxContent;
            const int cw = row.getWidth() / 4;
            delayKnobs[0]->setBounds (row.getX(), row.getY(), cw, row.getHeight());
            delayDiv.setBounds (Rectangle<int> (row.getX(), row.getY() + S (18), cw, S (28)).reduced (S (2), 0));
            for (int i = 1; i < 4; ++i) delayKnobs[(size_t) i]->setBounds (row.getX() + i * cw, row.getY(), cw, row.getHeight());
        }
        // REVERB: 2 knobs
        {
            auto row = fxContent;
            const int cw = row.getWidth() / 4;
            for (auto& k : revKnobs) k->setBounds (row.removeFromLeft (cw));
        }
        if (selectedFx == 1)
            distTypeLabel = Rectangle<int> (fxContent.getX() + S (4), fxContent.getY(), fxContent.getWidth() / 4, S (16));
    }

    // ---- LFO strip + voice ----
    {
        auto lf = lfoFrame.reduced (S (14), S (10));
        auto tabs = lf.removeFromLeft (S (100));
        const int th = (tabs.getHeight() - S (8)) / kNumLfos;
        for (int l = 0; l < kNumLfos; ++l)
            lfoTabs[(size_t) l]->setBounds (tabs.getX(), tabs.getY() + l * (th + S (8)), tabs.getWidth(), th);
        lf.removeFromLeft (S (12));

        voiceArea = lf.removeFromRight (S (180));
        lf.removeFromRight (S (18));

        lfoEditor.setBounds (lf.removeFromLeft (S (330)));
        lf.removeFromLeft (S (14));
        auto col1 = lf.removeFromLeft (S (112));
        lf.removeFromLeft (S (10));
        auto col2 = lf.removeFromLeft (S (100));
        lf.removeFromLeft (S (6));
        auto rateCol = lf.removeFromLeft (lf.getWidth() / 2);
        auto riseCol = lf;

        lfoPresetLabel = col1.removeFromTop (S (14));
        auto presetBox = col1.removeFromTop (S (28));
        col1.removeFromTop (S (8));
        lfoModeLabel = col1.removeFromTop (S (14));
        auto modeBox = col1.removeFromTop (S (28));

        lfoDivLabel = col2.removeFromTop (S (14));
        auto divBox = col2.removeFromTop (S (28));
        col2.removeFromTop (S (22));
        auto syncBox = col2.removeFromTop (S (28));

        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& c = *lfoControls[(size_t) l];
            c.preset.setBounds (presetBox);
            c.mode.setBounds (modeBox);
            c.div.setBounds (divBox);
            c.sync.setBounds (syncBox);
            c.rate->setBounds (rateCol.withSizeKeepingCentre (rateCol.getWidth(), jmin (rateCol.getHeight(), S (86))));
            c.rise->setBounds (riseCol.withSizeKeepingCentre (riseCol.getWidth(), jmin (riseCol.getHeight(), S (86))));
        }

        auto v = voiceArea;
        globalKnobs[0]->setBounds (v.removeFromRight (S (66)).withSizeKeepingCentre (S (66), jmin (v.getHeight(), S (86))));   // porta
        v.removeFromRight (S (6));
        v.removeFromTop (S (24));
        auto row1 = v.removeFromTop (S (30));
        const int bw = row1.getWidth() / 2;
        polyButton.setBounds (row1.removeFromLeft (bw).reduced (S (2), 0));
        monoButton.setBounds (row1.reduced (S (2), 0));
    }
}

// ----------------------------------------------------------------------------

bool GranulumEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (GranulumProcessor::isSupportedAudioFile (f)) return true;
    return false;
}

void GranulumEditor::fileDragEnter (const StringArray&, int, int) { field.dragHover = fileHover = true;  repaint(); }
void GranulumEditor::fileDragExit  (const StringArray&)           { field.dragHover = fileHover = false; repaint(); }

void GranulumEditor::filesDropped (const StringArray& files, int, int)
{
    field.dragHover = fileHover = false;
    for (auto& f : files)
        if (GranulumProcessor::isSupportedAudioFile (f)) { openFile (File (f)); break; }
    repaint();
}
