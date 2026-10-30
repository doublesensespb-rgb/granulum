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

void GranulumLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                     Slider::SliderStyle, Slider& s)
{
    auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto tr = r.withSizeKeepingCentre (r.getWidth(), 6.0f);
    g.setColour (Palette::track);
    g.fillRoundedRectangle (tr, 3.0f);
    g.setColour (Palette::accent.withAlpha (s.isEnabled() ? 0.9f : 0.3f));
    g.fillRoundedRectangle (tr.withRight (pos), 3.0f);
    const float tw = 12.0f;
    g.setColour (s.isEnabled() ? Palette::knob : Palette::knobDark.withAlpha (0.5f));
    g.fillEllipse (pos - tw * 0.5f, r.getCentreY() - tw * 0.5f, tw, tw);
}

void GranulumLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float rad = 6.0f;
    if (b.getToggleState())
    {
        g.setColour (Palette::accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
        g.setColour (Palette::accent.interpolatedWith (Colours::white, over ? 0.2f : 0.0f));
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
    g.setFont (uiFont (b.getHeight() < 30 ? 11.5f : 13.5f, false, 0.08f));
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
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (getWidth() - 2, 90), boxH);
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
        const auto col = Palette::lfo (l);
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
// Screen (ribbon waveform + grains)

WaveformView::WaveformView (GranulumProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    startTimerHz (40);
}

void WaveformView::timerCallback()
{
    auto& e = proc.vizEngine();
    const int n = e.vizCount.load (std::memory_order_acquire);
    grainX.resize ((size_t) n);
    grainA.resize ((size_t) n);
    grainHit.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        grainX[(size_t) i]   = e.vizPos[(size_t) i].load (std::memory_order_relaxed);
        grainA[(size_t) i]   = e.vizAmp[(size_t) i].load (std::memory_order_relaxed);
        grainHit[(size_t) i] = e.vizHit[(size_t) i].load (std::memory_order_relaxed);
    }
    const int h = e.vizHeadCount.load (std::memory_order_acquire);
    heads.resize ((size_t) h);
    for (int i = 0; i < h; ++i)
        heads[(size_t) i] = e.vizHead[(size_t) i].load (std::memory_order_relaxed);

    phase += 0.035f;
    repaint();
}

void WaveformView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    {
        Path clip; clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);
    }

    g.setGradientFill (ColourGradient (Palette::screen.brighter (0.12f), r.getCentreX(), r.getCentreY(),
                                       Palette::screen.darker (0.25f), r.getX(), r.getY(), true));
    g.fillRect (r);

    auto inner = r.reduced (14.0f, 14.0f);
    const float mid = inner.getCentreY();
    const float halfH = inner.getHeight() * 0.5f;

    auto peaks = proc.getPeaks();
    if (peaks == nullptr || peaks->maxs.empty())
    {
        g.setFont (uiFont (18.0f, false, 0.1f));
        g.setColour (dragHover ? Colours::white : Palette::accentSoft);
        g.drawText (String::fromUTF8 ("ПЕРЕТАЩИ СЮДА СЭМПЛ"), getLocalBounds().withTrimmedBottom (24), Justification::centred);
        return;
    }

    const float position = proc.apvts.getRawParameterValue (ParamID::position)->load();
    const bool transOn   = proc.showingSample() && proc.apvts.getRawParameterValue (ParamID::transients)->load() > 0.5f;
    auto xOf = [&] (float norm) { return inner.getX() + norm * inner.getWidth(); };

    const int bins = (int) peaks->maxs.size();
    const int cols = jmax (2, (int) inner.getWidth() / 2);
    std::vector<float> env ((size_t) cols), up ((size_t) cols), dn ((size_t) cols);
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
        up[(size_t) x] = hi; dn[(size_t) x] = lo;
        env[(size_t) x] = jmax (hi, -lo);
    }
    std::vector<float> sm ((size_t) cols);
    for (int x = 0; x < cols; ++x)
    {
        float acc = 0, wsum = 0;
        for (int k = -4; k <= 4; ++k)
        {
            const int xi = jlimit (0, cols - 1, x + k);
            const float w = 1.0f - std::abs ((float) k) / 5.0f;
            acc += env[(size_t) xi] * w; wsum += w;
        }
        sm[(size_t) x] = acc / wsum;
    }

    {
        Path raw;
        raw.startNewSubPath (inner.getX(), mid - up[0] * halfH);
        for (int x = 1; x < cols; ++x) raw.lineTo (xOf ((float) x / (float) (cols - 1)), mid - up[(size_t) x] * halfH);
        for (int x = cols - 1; x >= 0; --x) raw.lineTo (xOf ((float) x / (float) (cols - 1)), mid - dn[(size_t) x] * halfH);
        raw.closeSubPath();
        g.setColour (Palette::accent.withAlpha (0.07f));
        g.fillPath (raw);
    }

    const int strands = 22;
    for (int k = 0; k < strands; ++k)
    {
        const float t = (float) k / (float) (strands - 1);
        const float base = std::cos (t * MathConstants<float>::pi);
        const float edge = std::abs (base);
        Path p;
        for (int x = 0; x < cols; ++x)
        {
            const float xn = (float) x / (float) (cols - 1);
            const float wob = 0.10f * std::sin (xn * 23.0f + (float) k * 0.7f + phase);
            const float y = mid - sm[(size_t) x] * halfH * 0.95f * (base + wob * (1.0f - edge));
            if (x == 0) p.startNewSubPath (xOf (xn), y); else p.lineTo (xOf (xn), y);
        }
        g.setColour (Palette::accent.withAlpha (0.05f));
        g.strokePath (p, PathStrokeType (3.5f));
        g.setColour (Palette::accent.interpolatedWith (Colours::white, 0.15f * edge).withAlpha (0.18f + 0.45f * edge));
        g.strokePath (p, PathStrokeType (0.9f));
    }

    if (transOn)
        for (float tpos : peaks->transients)
        {
            const float x = xOf (tpos);
            g.setColour (Palette::hit.withAlpha (0.10f)); g.fillRect (x - 3.0f, r.getY(), 6.0f, r.getHeight());
            g.setColour (Palette::hit.withAlpha (0.55f)); g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
        }

    for (float h : heads)
    {
        float hp = position + h;
        hp -= std::floor (hp);
        const float x = xOf (hp);
        g.setColour (Colours::white.withAlpha (0.10f)); g.fillRect (x - 3.0f, r.getY(), 6.0f, r.getHeight());
        g.setColour (Colours::white.withAlpha (0.45f)); g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
    }

    {
        const float x = xOf (position);
        g.setColour (Palette::purple.withAlpha (0.20f)); g.fillRect (x - 4.0f, r.getY(), 8.0f, r.getHeight());
        g.setColour (Palette::purple.brighter (0.4f)); g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
        Path tri; tri.addTriangle (x - 6.0f, r.getY() + 2.0f, x + 6.0f, r.getY() + 2.0f, x, r.getY() + 10.0f);
        g.fillPath (tri);
    }

    for (size_t i = 0; i < grainX.size(); ++i)
    {
        const bool isHit = grainHit[i] != 0;
        const float x = xOf (grainX[i]);
        const uint32 hsh = (uint32) (i * 2654435761u);
        const float yN = ((float) (hsh % 1000) / 1000.0f) * 2.0f - 1.0f;
        const float y = mid + yN * halfH * 0.8f;
        const float a = jlimit (0.0f, 1.0f, grainA[i]);
        const auto col = isHit ? Palette::hit : (i % 4 == 0 ? Palette::purple.brighter (0.5f) : Palette::accent);
        const float rad = (isHit ? 3.0f : 1.5f) + a * 3.5f;
        g.setColour (col.withAlpha (0.10f * a)); g.fillEllipse (x - rad * 3.5f, y - rad * 3.5f, rad * 7.0f, rad * 7.0f);
        g.setColour (col.withAlpha (0.30f * a)); g.fillEllipse (x - rad * 1.6f, y - rad * 1.6f, rad * 3.2f, rad * 3.2f);
        g.setColour (col.interpolatedWith (Colours::white, 0.7f).withAlpha (0.4f + 0.6f * a));
        g.fillEllipse (x - rad * 0.5f, y - rad * 0.5f, rad, rad);
    }

    if (dragHover)
    {
        g.setColour (Palette::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (2.0f), 10.0f, 2.0f);
    }
}

void WaveformView::setPositionFromMouse (const MouseEvent& e)
{
    auto inner = getLocalBounds().toFloat().reduced (14.0f, 14.0f);
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
// Harmonic sticks

HarmonicEditor::HarmonicEditor (GranulumProcessor& p) : proc (p)
{
    h = proc.getHarmonics();
    setMouseCursor (MouseCursor::CrosshairCursor);
    setTooltip (String::fromUTF8 ("Рисуй гармоники мышкой. Правая кнопка стирает."));
    startTimerHz (20);
}

void HarmonicEditor::timerCallback()
{
    if (editing) return;
    const auto now = proc.getHarmonics();
    if (now != h) h = now;
    repaint();
}

Rectangle<float> HarmonicEditor::barArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 8.0f).withTrimmedBottom (10.0f);
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

void HarmonicEditor::mouseDown (const MouseEvent& e)
{
    editing = true;
    erasing = e.mods.isRightButtonDown();
    lastBar = -1;
    applyMouse (e, true);
}

void HarmonicEditor::mouseDrag (const MouseEvent& e) { if (editing) applyMouse (e, false); }
void HarmonicEditor::mouseUp (const MouseEvent&)     { editing = false; lastBar = -1; }

void HarmonicEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 10.0f);

    const bool active = (int) proc.apvts.getRawParameterValue (ParamID::source)->load() != 1;
    auto a = barArea();

    {
        Path wave;
        const int pts = 160;
        std::vector<float> ys ((size_t) pts);
        float peak = 0.0f;
        for (int i = 0; i < pts; ++i)
        {
            const float ph = MathConstants<float>::twoPi * (float) i / (float) (pts - 1);
            float v = 0.0f;
            for (int k = 0; k < granulum::kNumHarmonics; ++k)
                v += h[(size_t) k] * std::sin (ph * (float) (k + 1));
            ys[(size_t) i] = v;
            peak = jmax (peak, std::abs (v));
        }
        const float sc = peak > 1.0e-4f ? 1.0f / peak : 0.0f;
        for (int i = 0; i < pts; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (float) (pts - 1);
            const float y = a.getCentreY() - ys[(size_t) i] * sc * a.getHeight() * 0.42f;
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (Palette::purple.withAlpha (active ? 0.45f : 0.15f));
        g.strokePath (wave, PathStrokeType (1.4f));
    }

    const float bw = a.getWidth() / (float) granulum::kNumHarmonics;
    for (int k = 0; k < granulum::kNumHarmonics; ++k)
    {
        auto slot = Rectangle<float> (a.getX() + bw * (float) k, a.getY(), bw, a.getHeight()).reduced (bw * 0.18f, 0.0f);
        g.setColour (Palette::track.withAlpha (0.8f));
        g.fillRoundedRectangle (slot, 2.0f);
        const float v = h[(size_t) k];
        if (v > 0.001f)
        {
            auto bar = slot.withTop (slot.getBottom() - slot.getHeight() * v);
            const auto col = active ? Palette::accent : Palette::accentSoft.withAlpha (0.35f);
            g.setColour (col.withAlpha (active ? 0.18f : 0.08f));
            g.fillRoundedRectangle (bar.expanded (1.5f, 1.5f), 2.5f);
            g.setColour (col);
            g.fillRoundedRectangle (bar, 2.0f);
        }
    }

    g.setColour (Palette::textDim.withAlpha (0.55f));
    g.setFont (uiFont (9.5f));
    for (int k : { 1, 8, 16, 24, 32 })
        g.drawText (String (k), Rectangle<float> (a.getX() + bw * ((float) k - 0.5f) - 12.0f, a.getBottom() + 1.0f, 24.0f, 10.0f), Justification::centred, false);

    if (! active)
    {
        g.setColour (Palette::textDim.withAlpha (0.7f));
        g.setFont (uiFont (12.0f, false, 0.08f));
        g.drawText ("ADDITIVE OFF", a, Justification::centred, false);
    }
}

// ============================================================================
// LFO tab + display

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

    // grip dots = draggable
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

void LfoTab::mouseDown (const MouseEvent&)
{
    dragging = false;
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

LfoDisplay::LfoDisplay (GranulumProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void LfoDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 8.0f);
    auto a = r.reduced (10.0f, 9.0f);

    g.setColour (Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 4.0f), a.getY(), a.getBottom());
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

    const int shape = (int) proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Shape"))->load();
    const auto col = Palette::lfo (lfo);

    // fixed pseudo-random preview for the random shapes
    const float rnd[] = { 0.35f, 0.85f, 0.15f, 0.65f, 0.95f, 0.45f, 0.05f, 0.75f, 0.35f };
    auto valueAt = [&] (float x) // x: 0..1 across the display
    {
        if (shape < 5) return GranulumProcessor::lfoShapeValue (shape, x, 0, 0, 0);
        const float pos = x * 8.0f;
        const int i = jlimit (0, 7, (int) pos);
        return GranulumProcessor::lfoShapeValue (shape, pos - (float) i, rnd[i], rnd[i], rnd[i + 1]);
    };

    Path p, fill;
    const int pts = (int) a.getWidth();
    for (int i = 0; i <= pts; ++i)
    {
        const float x = (float) i / (float) pts;
        const float y = a.getBottom() - valueAt (x) * a.getHeight();
        if (i == 0) { p.startNewSubPath (a.getX(), y); fill.startNewSubPath (a.getX(), a.getBottom()); }
        p.lineTo (a.getX() + x * a.getWidth(), y);
        fill.lineTo (a.getX() + x * a.getWidth(), y);
    }
    fill.lineTo (a.getRight(), a.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (col.withAlpha (0.25f), 0, a.getY(), col.withAlpha (0.02f), 0, a.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (p, PathStrokeType (1.8f));

    // playhead
    const float ph  = proc.lfoPhaseOut[(size_t) lfo].load();
    const float val = proc.lfoValueOut[(size_t) lfo].load();
    const float x = a.getX() + (shape < 5 ? ph : ph) * a.getWidth();
    const float y = a.getBottom() - val * a.getHeight();
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
    g.setColour (col.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre ({ x, y }));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ x, y }));

    // rate text
    const bool sync = proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Sync"))->load() > 0.5f;
    const String rate = sync ? GranulumProcessor::divisionNames()[(int) proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Div"))->load()]
                             : String (proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Rate"))->load(), 2) + " Hz";
    g.setColour (Palette::textDim);
    g.setFont (uiFont (11.0f, false, 0.05f));
    g.drawText (rate, r.reduced (10.0f, 6.0f), Justification::topRight, false);
}

// ============================================================================
// Editor

GranulumEditor::GranulumEditor (GranulumProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p), harmonics (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard), lfoDisplay (p)
{
    setLookAndFeel (&look);

    auto& s = proc.apvts;
    auto add = [&] (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* name, Knob::Size sz = Knob::Size::Normal)
    {
        group.push_back (std::make_unique<Knob> (proc, id, name, sz));
        addAndMakeVisible (*group.back());
        allKnobs.push_back (group.back().get());
    };

    add (heroKnobs,   ParamID::attack,      "Attack",       Knob::Size::Hero);
    add (heroKnobs,   ParamID::hold,        "Hold",         Knob::Size::Hero);
    add (heroKnobs,   ParamID::spacing,     "Gr. Spacing",  Knob::Size::Hero);
    add (heroKnobs,   ParamID::waveSpacing, "Wave Spacing", Knob::Size::Hero);

    add (motionKnobs, ParamID::position,    "Start");
    add (motionKnobs, ParamID::rand,        "Rand");
    add (motionKnobs, ParamID::smear,       "Smear");
    add (motionKnobs, ParamID::pan,         "Pan");

    add (bassKnobs,   ParamID::sub,         "Sub");
    add (bassKnobs,   ParamID::drive,       "Drive");
    add (bassKnobs,   ParamID::xover,       "X-Over");
    add (bassKnobs,   ParamID::glide,       "Glide");
    add (bassKnobs,   ParamID::pitch,       "Pitch");

    add (fxKnobs,     ParamID::cutoff,      "Cutoff");
    add (fxKnobs,     ParamID::reso,        "Reso");
    add (fxKnobs,     ParamID::space,       "Space");
    add (fxKnobs,     ParamID::room,        "Room");
    add (fxKnobs,     ParamID::output,      "Output");

    add (sourceKnobs, ParamID::addOct,      "Oct",  Knob::Size::Small);
    add (sourceKnobs, ParamID::addSemi,     "Semi", Knob::Size::Small);
    add (sourceKnobs, ParamID::mix,         "Mix",  Knob::Size::Small);

    for (auto* b : { &transButton, &keySyncButton, &monoButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (*b);
    }
    transAttachment   = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, ParamID::transients, transButton);
    keySyncAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, ParamID::keySync, keySyncButton);
    monoAttachment    = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, ParamID::mono, monoButton);
    keySyncButton.setTooltip (String::fromUTF8 ("Гранулы режутся в частоту нажатой ноты: любой источник превращается в тональный бас"));
    transButton.setTooltip (String::fromUTF8 ("Сохраняет удары: гранулы начинаются ровно на транзиентах"));

    filterType.addItemList ({ "LOW PASS", "BAND PASS", "HIGH PASS" }, 1);
    addAndMakeVisible (filterType);
    filterTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, ParamID::ftype, filterType);

    addAndMakeVisible (waveform);
    addAndMakeVisible (harmonics);

    int idx = 0;
    for (auto* b : { &srcAdd, &srcSample, &srcMix })
    {
        const int i = idx++;
        b->onClick = [this, i] { setSource (i); };
        addAndMakeVisible (*b);
    }
    srcAdd.setTooltip    (String::fromUTF8 ("Только аддитивный синт (палочки)"));
    srcSample.setTooltip (String::fromUTF8 ("Только сэмпл"));
    srcMix.setTooltip    (String::fromUTF8 ("Палочки + сэмпл, баланс ручкой MIX"));

    for (auto* b : { &rndGrains, &rndBass, &rndFx, &rndBars, &randomButton, &loadButton }) addAndMakeVisible (*b);
    rndGrains.onClick    = [this] { proc.randomizeSection (0); };
    rndBass.onClick      = [this] { proc.randomizeSection (1); };
    rndFx.onClick        = [this] { proc.randomizeSection (2); };
    rndBars.onClick      = [this] { proc.randomizeSection (3); };
    randomButton.onClick = [this] { proc.randomize(); };
    loadButton.onClick   = [this] { launchChooser(); };

    sampleName.setFont (uiFont (14.0f, false, 0.05f));
    sampleName.setColour (Label::textColourId, Palette::text);
    sampleName.setJustificationType (Justification::centredLeft);
    sampleName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sampleName);

    // ---- LFO strip ----
    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l] = std::make_unique<LfoTab> (proc, l);
        lfoTabs[(size_t) l]->onSelect = [this] (int i) { selectLfo (i); };
        addAndMakeVisible (*lfoTabs[(size_t) l]);

        auto c = std::make_unique<LfoControls>();
        c->shape.addItemList (GranulumProcessor::shapeNames(), 1);
        c->div.addItemList (GranulumProcessor::divisionNames(), 1);
        c->mode.addItemList ({ "FREE", "RETRIG" }, 1);
        c->sync.setClickingTogglesState (true);
        c->sync.setTooltip (String::fromUTF8 ("Синхронизация с темпом проекта"));
        c->mode.setTooltip (String::fromUTF8 ("RETRIG: LFO начинается заново с каждой ноты"));
        c->rate = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rate"), "Rate", Knob::Size::Small);
        allKnobs.push_back (c->rate.get());
        c->shapeA = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, ParamID::lfo (l, "Shape"), c->shape);
        c->divA   = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, ParamID::lfo (l, "Div"), c->div);
        c->modeA  = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, ParamID::lfo (l, "Mode"), c->mode);
        c->syncA  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, ParamID::lfo (l, "Sync"), c->sync);
        for (Component* comp : { (Component*) &c->shape, (Component*) &c->div, (Component*) &c->mode, (Component*) &c->sync, (Component*) c->rate.get() })
            addChildComponent (comp);
        lfoControls[(size_t) l] = std::move (c);
    }
    addAndMakeVisible (lfoDisplay);

    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Palette::whiteKey);
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Palette::blackKey);
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Palette::blackKey.withAlpha (0.6f));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent.withAlpha (0.85f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.25f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::transparentBlack);
    keyboard.setColour (MidiKeyboardComponent::textLabelColourId, Palette::blackKey);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, Palette::panel);
    keyboard.setColour (MidiKeyboardComponent::upDownButtonArrowColourId, Palette::text);
    keyboard.setAvailableRange (24, 84);
    keyboard.setLowestVisibleKey (24);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    selectLfo (0);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (1080.0 / 800.0);
    setResizeLimits (864, 640, 1620, 1200);
    setSize (1080, 800);
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
        for (Component* comp : { (Component*) &c.shape, (Component*) &c.div, (Component*) &c.mode, (Component*) &c.sync, (Component*) c.rate.get() })
            comp->setVisible (l == i);
    }
    lfoDisplay.lfo = i;
    lfoDisplay.repaint();
}

void GranulumEditor::setSource (int sIdx)
{
    if (auto* p = proc.apvts.getParameter (ParamID::source))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) sIdx));
        p->endChangeGesture();
    }
    if (sIdx != 0 && ! proc.hasSample())
        launchChooser();
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
}

void GranulumEditor::timerCallback()
{
    const int src = (int) proc.apvts.getRawParameterValue (ParamID::source)->load();
    if (src != shownSource)
    {
        shownSource = src;
        srcAdd.setToggleState    (src == 0, dontSendNotification);
        srcSample.setToggleState (src == 1, dontSendNotification);
        srcMix.setToggleState    (src == 2, dontSendNotification);
        sourceKnobs[2]->setEnabled (src == 2);
        sourceKnobs[2]->setAlpha (src == 2 ? 1.0f : 0.35f);
        for (int k = 0; k < 2; ++k)
        {
            sourceKnobs[(size_t) k]->setEnabled (src != 1);
            sourceKnobs[(size_t) k]->setAlpha (src != 1 ? 1.0f : 0.35f);
        }
        rndBars.setEnabled (src != 1);
        harmonics.repaint();
    }

    const auto name = proc.getSampleName();
    const auto stem = File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension().toUpperCase();
    const String text = statusText.isNotEmpty() ? "ERROR: " + statusText
                      : src == 0 ? String ("ADDITIVE")
                      : ! proc.hasSample() ? String::fromUTF8 ("НЕТ СЭМПЛА: НАЖМИ LOAD")
                      : (src == 2 ? "ADDITIVE + " : "") + stem + "    " + String (proc.getSampleLengthSeconds(), 2) + " s";
    if (sampleName.getText() != text) sampleName.setText (text, dontSendNotification);

    // LFO controls: rate knob only matters when not synced
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

    meter    = jmax (proc.outputLevel.load(), meter * 0.85f);
    activity = jmax ((float) proc.vizEngine().vizCount.load() / 20.0f, activity * 0.8f);
    repaint (gemsArea.expanded (24));
    repaint (ledArea.expanded (16));
}

void GranulumEditor::drawSectionHeader (Graphics& g, Rectangle<int> area, const String& title, bool lit)
{
    auto hr = area.withHeight (26);
    drawLed (g, Rectangle<float> ((float) hr.getX() + 2.0f, (float) hr.getCentreY() - 6.0f, 12.0f, 12.0f),
             Palette::accent, Palette::ledOff, lit ? 1.0f : 0.0f);
    g.setColour (Palette::text);
    g.setFont (uiFont (14.5f, false, 0.08f));
    g.drawText (title.toUpperCase(), hr.withTrimmedLeft (24), Justification::centredLeft, false);
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
    g.drawText ("GRANULUM", titleArea.withHeight ((int) (46 * sc)), Justification::bottomLeft);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (16.0f * sc, false, 0.08f));
    g.drawText ("BASS GRANULIZER", titleArea.withTrimmedTop ((int) (49 * sc)), Justification::topLeft);

    const float db = Decibels::gainToDecibels (meter, -60.0f);
    const float thresholds[6] = { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, -2.0f };
    const float gw = (float) gemsArea.getWidth() / 3.0f;
    const float gh = (float) gemsArea.getHeight() / 2.0f;
    const float gs = jmin (gw, gh) * 0.70f;
    for (int i = 0; i < 6; ++i)
    {
        const float cx = (float) gemsArea.getX() + gw * ((float) (i % 3) + 0.5f);
        const float cy = (float) gemsArea.getY() + gh * ((float) (i / 3) + 0.5f);
        const float lvl = jlimit (0.0f, 1.0f, (db - thresholds[i]) / 6.0f + 0.5f);
        auto c = Rectangle<float> (cx - gs * 0.5f, cy - gs * 0.5f, gs, gs);
        g.setColour (Colour (0xff10303b));
        g.fillEllipse (c.expanded (gs * 0.14f));
        drawLed (g, c, Palette::purple, Palette::purpleOff, lvl);
    }

    for (auto fr : { screenFrame, lfoFrame })
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (fr.toFloat().translated (0, 2.0f), 14.0f);
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (fr.toFloat(), 14.0f);
    }
    g.setColour (Palette::label);
    g.setFont (uiFont (13.0f, false, 0.08f));
    g.drawText ("GRAIN FIELD", screenFrame.getX() + 18, screenFrame.getY() + 10, 140, 26, Justification::centredLeft);

    // LFO strip labels
    g.setColour (Palette::label);
    g.setFont (uiFont (10.5f, false, 0.08f));
    g.drawText ("SHAPE", lfoShapeLabel, Justification::centredLeft, false);
    g.drawText ("MODE",  lfoModeLabel,  Justification::centredLeft, false);
    g.drawText ("DIVISION", lfoDivLabel, Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (12.0f, false, 0.03f));
    g.drawFittedText (String::fromUTF8 ("Тяни LFO на любую ручку.\nКружок у ручки: тяни вверх/вниз,\nчтобы задать глубину.\nДвойной клик: убрать."),
                      lfoHintArea, Justification::centredLeft, 4, 0.9f);

    const float ls = (float) ledArea.getHeight();
    drawLed (g, { (float) ledArea.getRight() - ls * 2.4f, (float) ledArea.getY(), ls, ls }, Palette::accent, Palette::ledOff, activity);
    drawLed (g, { (float) ledArea.getRight() - ls,        (float) ledArea.getY(), ls, ls }, Palette::purple, Palette::ledOff, meter > 0.89f ? 1.0f : 0.0f);

    drawSectionHeader (g, grainArea, "Grains", true);
    drawSectionHeader (g, bassArea,  "Bass", true);
    drawSectionHeader (g, fxArea,    "Filter / Space", true);

    if (! heroKnobs.empty())
    {
        auto hr = heroKnobs.front()->getBounds();
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine (hr.getBottom() + 4, (float) grainArea.getX(), (float) grainArea.getRight());
    }
    g.setColour (Colours::white.withAlpha (0.06f));
    for (auto x : { bassArea.getX() - 9, fxArea.getX() - 9 })
        g.drawVerticalLine (x, (float) grainArea.getY() + 4.0f, (float) grainArea.getBottom() - 4.0f);

    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (keyboard.getBounds().toFloat().expanded (3.0f), 6.0f);
}

void GranulumEditor::resized()
{
    const float sc = (float) getWidth() / 1080.0f;
    auto S = [sc] (float v) { return (int) std::round (v * sc); };

    auto r = getLocalBounds().reduced (S (34), S (20));

    auto header = r.removeFromTop (S (72));
    gemsArea  = header.removeFromRight (S (112)).withSizeKeepingCentre (S (112), S (66));
    titleArea = header.withTrimmedLeft (S (4));
    r.removeFromTop (S (6));

    keyboard.setBounds (r.removeFromBottom (S (54)).reduced (S (2), S (2)));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    keyboard.setLowestVisibleKey (24);
    r.removeFromBottom (S (12));

    lfoFrame = r.removeFromBottom (S (100));
    r.removeFromBottom (S (12));

    auto knobs = r.removeFromBottom (S (234));
    r.removeFromBottom (S (12));

    // ---- screen frame: grain field (left) + source panel (right) ----
    screenFrame = r;
    auto inside = r.reduced (S (16), S (10));
    auto top = inside.removeFromTop (S (32));
    randomButton.setBounds (top.removeFromRight (S (124)).reduced (0, S (2)));
    top.removeFromRight (S (8));
    loadButton.setBounds (top.removeFromRight (S (80)).reduced (0, S (2)));
    top.removeFromRight (S (12));
    ledArea = top.removeFromRight (S (40)).withSizeKeepingCentre (S (40), S (14));
    top.removeFromLeft (S (120));
    sampleName.setBounds (top);
    inside.removeFromTop (S (6));

    sourceArea = inside.removeFromRight (S (310));
    inside.removeFromRight (S (12));
    waveform.setBounds (inside);

    {
        auto sa = sourceArea;
        auto btns = sa.removeFromTop (S (26));
        const int bw = btns.getWidth() / 3;
        srcAdd.setBounds    (btns.removeFromLeft (bw).reduced (S (2), 0));
        srcSample.setBounds (btns.removeFromLeft (bw).reduced (S (2), 0));
        srcMix.setBounds    (btns.reduced (S (2), 0));
        sa.removeFromTop (S (6));
        auto bottom = sa.removeFromBottom (S (60));
        sa.removeFromBottom (S (4));
        harmonics.setBounds (sa);
        rndBars.setBounds (bottom.removeFromRight (S (72)).withSizeKeepingCentre (S (72), S (26)));
        bottom.removeFromRight (S (6));
        const int kw = bottom.getWidth() / 3;
        for (auto& k : sourceKnobs) k->setBounds (bottom.removeFromLeft (kw));
    }

    // ---- knob sections ----
    const int gap = S (18);
    const float colW = (float) (knobs.getWidth() - gap * 2) / 10.0f;
    grainArea = knobs.removeFromLeft ((int) (colW * 4.3f)); knobs.removeFromLeft (gap);
    bassArea  = knobs.removeFromLeft ((int) (colW * 2.6f)); knobs.removeFromLeft (gap);
    fxArea    = knobs;

    auto headerButtons = [&] (Rectangle<int> area, std::initializer_list<std::pair<Button*, int>> bs)
    {
        int x = area.getRight();
        for (auto [b, w] : bs)
        {
            x -= S ((float) w);
            b->setBounds (x, area.getY(), S ((float) w), S (26));
            x -= S (6);
        }
    };
    headerButtons (grainArea, { { &rndGrains, 76 }, { &transButton, 104 }, { &keySyncButton, 92 } });
    headerButtons (bassArea,  { { &rndBass, 72 }, { &monoButton, 58 } });
    headerButtons (fxArea,    { { &rndFx, 76 } });

    auto layoutRow = [] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> row)
    {
        const int cw = row.getWidth() / (int) ks.size();
        for (int i = 0; i < (int) ks.size(); ++i)
            ks[(size_t) i]->setBounds (row.getX() + i * cw, row.getY(), cw, row.getHeight());
    };

    {
        auto g = grainArea.withTrimmedTop (S (34));
        auto hero = g.removeFromTop ((int) (g.getHeight() * 0.56f));
        g.removeFromTop (S (8));
        layoutRow (heroKnobs, hero);
        layoutRow (motionKnobs, g);
    }
    {
        auto b = bassArea.withTrimmedTop (S (34));
        auto row1 = b.removeFromTop ((int) (b.getHeight() * 0.56f));
        b.removeFromTop (S (8));
        const int w1 = row1.getWidth() / 2;
        bassKnobs[0]->setBounds (row1.removeFromLeft (w1));
        bassKnobs[1]->setBounds (row1);
        const int w2 = b.getWidth() / 3;
        bassKnobs[2]->setBounds (b.removeFromLeft (w2));
        bassKnobs[3]->setBounds (b.removeFromLeft (w2));
        bassKnobs[4]->setBounds (b);
    }
    {
        auto f = fxArea.withTrimmedTop (S (34));
        const int cw = f.getWidth() / 3;
        const int rh = (f.getHeight() - S (8)) / 2;
        for (int i = 0; i < (int) fxKnobs.size(); ++i)
            fxKnobs[(size_t) i]->setBounds (f.getX() + (i % 3) * cw, f.getY() + (i / 3) * (rh + S (8)), cw, rh);
        auto comboSlot = Rectangle<int> (f.getX() + 2 * cw, f.getY() + rh + S (8), cw, rh);
        filterType.setBounds (comboSlot.withSizeKeepingCentre (cw - S (8), S (30)));
    }

    // ---- LFO strip ----
    {
        auto lf = lfoFrame.reduced (S (14), S (10));
        auto tabs = lf.removeFromLeft (S (104));
        const int th = (tabs.getHeight() - S (8)) / kNumLfos;
        for (int l = 0; l < kNumLfos; ++l)
            lfoTabs[(size_t) l]->setBounds (tabs.getX(), tabs.getY() + l * (th + S (8)), tabs.getWidth(), th);
        lf.removeFromLeft (S (12));

        lfoDisplay.setBounds (lf.removeFromLeft (S (360)));
        lf.removeFromLeft (S (14));

        auto col1 = lf.removeFromLeft (S (130));
        lf.removeFromLeft (S (10));
        auto col2 = lf.removeFromLeft (S (130));
        lf.removeFromLeft (S (10));
        auto rateCol = lf.removeFromLeft (S (66));
        lf.removeFromLeft (S (16));
        lfoHintArea = lf;

        lfoShapeLabel = col1.removeFromTop (S (14));
        auto shapeBox = col1.removeFromTop (S (28));
        col1.removeFromTop (S (6));
        lfoModeLabel = col1.removeFromTop (S (14));
        auto modeBox = col1.removeFromTop (S (28));

        lfoDivLabel = col2.removeFromTop (S (14));
        auto divBox = col2.removeFromTop (S (28));
        col2.removeFromTop (S (20));
        auto syncBox = col2.removeFromTop (S (28));

        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& c = *lfoControls[(size_t) l];
            c.shape.setBounds (shapeBox);
            c.mode.setBounds (modeBox);
            c.div.setBounds (divBox);
            c.sync.setBounds (syncBox);
            c.rate->setBounds (rateCol);
        }
    }
}

// ----------------------------------------------------------------------------

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
