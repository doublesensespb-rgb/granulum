#include "PluginEditor.h"

using namespace juce;

namespace
{
    Font uiFont (float size, bool bold = false, float kerning = 0.0f)
    {
        return Font { FontOptions (size, bold ? Font::bold : Font::plain).withKerningFactor (kerning) };
    }

    // soft glow text: a few blurred-ish passes then the crisp text
    void glowText (Graphics& g, const String& s, Rectangle<int> r, Justification j, Colour c, float glow)
    {
        if (glow > 0.0f)
        {
            g.setColour (c.withAlpha (0.10f * glow));
            static const int offs[6][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -2, 0 }, { 2, 0 } };
            for (auto& o : offs)
                g.drawText (s, r.translated (o[0], o[1]), j, false);
        }
        g.setColour (c);
        g.drawText (s, r, j, false);
    }

    void drawScrew (Graphics& g, Point<float> c, float r, float angle)
    {
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - r + 1.0f, c.y - r + 2.0f, r * 2, r * 2);
        ColourGradient cg (Colour (0xff8d8c88), c.x - r * 0.6f, c.y - r * 0.6f, Colour (0xff2b2b2a), c.x + r, c.y + r, true);
        g.setGradientFill (cg);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (Colour (0xff141414));
        const auto d = Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.75f);
        g.drawLine (Line<float> (c - d, c + d), 2.2f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawLine (Line<float> (c - d + Point<float> (0, 1.2f), c + d + Point<float> (0, 1.2f)), 0.8f);
    }

    void drawFrame (Graphics& g, Rectangle<float> r, const String& label, Colour led)
    {
        g.setColour (Palette::teal.withAlpha (0.08f));
        g.drawRoundedRectangle (r.expanded (1.5f), 14.0f, 3.0f);
        g.setColour (Palette::teal.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 13.0f, 1.3f);

        if (label.isNotEmpty())
        {
            g.setFont (uiFont (14.0f, false, 0.06f));
            auto lr = r.toNearestInt().withTrimmedLeft (18).withTrimmedTop (10).withHeight (20);
            if (led != Colours::transparentBlack)
            {
                GranulumLook::drawGem (g, Rectangle<float> ((float) lr.getX(), (float) lr.getY() + 4.0f, 12.0f, 12.0f), led, 0.4f);
                lr.removeFromLeft (20);
            }
            g.setColour (Palette::teal.withAlpha (0.9f));
            g.drawText (label.toUpperCase(), lr, Justification::centredLeft, false);
        }
    }

    void drawRecess (Graphics& g, Rectangle<float> r, float radius)
    {
        g.setColour (Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.expanded (3.0f), radius + 3.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (r.expanded (3.0f).translated (0, 1.0f), radius + 3.0f, 1.0f);
        g.setColour (Colour (0xff050606));
        g.fillRoundedRectangle (r, radius);
    }
}

// ============================================================================
// Look and feel

GranulumLook::GranulumLook()
{
    setColour (Slider::textBoxTextColourId, Palette::teal);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Palette::teal.withAlpha (0.35f));
    setColour (Label::textColourId, Palette::cream);
    setColour (TextButton::textColourOffId, Palette::teal);
    setColour (TextButton::textColourOnId, Palette::cream);
    setColour (ComboBox::textColourId, Palette::teal);
    setColour (ComboBox::arrowColourId, Palette::teal);
    setColour (PopupMenu::backgroundColourId, Colour (0xff141617));
    setColour (PopupMenu::textColourId, Palette::cream);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::tealDim);
    setColour (PopupMenu::highlightedTextColourId, Palette::cream);
    setColour (TextEditor::backgroundColourId, Colour (0xff0a0b0c));
    setColour (TextEditor::textColourId, Palette::teal);
    setColour (TextEditor::outlineColourId, Palette::tealDim);
    setColour (TextEditor::focusedOutlineColourId, Palette::teal);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff141617));
    setColour (TooltipWindow::textColourId, Palette::cream);
}

const Image& GranulumLook::speckle()
{
    static const Image img = []
    {
        Image im (Image::ARGB, 192, 192, true);
        Graphics g (im);
        Random r (1234);
        for (int i = 0; i < 1500; ++i)
        {
            const float x = r.nextFloat() * 192.0f, y = r.nextFloat() * 192.0f;
            const float s = 1.0f + r.nextFloat() * r.nextFloat() * 4.5f;
            const bool light = r.nextFloat() < 0.5f;
            g.setColour ((light ? Colours::white : Colours::black).withAlpha (0.10f + r.nextFloat() * (light ? 0.35f : 0.45f)));
            g.fillEllipse (x, y, s, s * (0.6f + r.nextFloat() * 0.8f));
        }
        return im;
    }();
    return img;
}

void GranulumLook::drawGem (Graphics& g, Rectangle<float> c, Colour base, float glow)
{
    const float r = c.getWidth() * 0.5f;
    const auto ctr = c.getCentre();

    if (glow > 0.01f)
    {
        g.setGradientFill (ColourGradient (base.withAlpha (0.45f * glow), ctr.x, ctr.y,
                                           base.withAlpha (0.0f), ctr.x + r * 2.4f, ctr.y, true));
        g.fillEllipse (c.expanded (r * 1.4f));
    }

    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.translated (0.0f, r * 0.12f).expanded (r * 0.06f));

    const auto lit = base.interpolatedWith (Colours::white, 0.12f + 0.25f * glow);
    ColourGradient cg (lit.brighter (0.25f), ctr.x - r * 0.45f, ctr.y - r * 0.55f,
                       base.darker (0.75f), ctr.x + r * 0.9f, ctr.y + r, true);
    g.setGradientFill (cg);
    g.fillEllipse (c);

    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addEllipse (c);
        g.reduceClipRegion (clip);
        g.setOpacity (0.75f);
        g.drawImage (speckle(), c.expanded (r * 0.3f), RectanglePlacement::fillDestination);
    }

    g.setColour (Colours::white.withAlpha (0.22f));
    g.fillEllipse (ctr.x - r * 0.55f, ctr.y - r * 0.7f, r * 0.8f, r * 0.5f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (c, 0.8f);
}

void GranulumLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                     float start, float end, Slider& s)
{
    const auto accent = s.findColour (Slider::rotarySliderFillColourId);
    auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (5.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = start + pos * (end - start);
    const float track = jmax (3.0f, r * 0.12f);
    const float ar = r - track * 0.5f;

    // recessed track
    Path bgArc;
    bgArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, start, end, true);
    g.setColour (Colour (0xff070808));
    g.strokePath (bgArc, PathStrokeType (track + 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xff1d2021));
    g.strokePath (bgArc, PathStrokeType (track - 1.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // tick dots
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * (float) i / 10.0f;
        const auto p = c.getPointOnCircumference (r + 3.0f, a);
        g.setColour (Palette::cream.withAlpha (i == 0 || i == 10 ? 0.35f : 0.15f));
        g.fillEllipse (p.x - 1.0f, p.y - 1.0f, 2.0f, 2.0f);
    }

    // glowing value arc (bipolar params start from the centre)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        Path valArc;
        valArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (accent.withAlpha (0.12f));
        g.strokePath (valArc, PathStrokeType (track * 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (valArc, PathStrokeType (track * 1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent.interpolatedWith (Colours::white, 0.15f));
        g.strokePath (valArc, PathStrokeType (track * 0.7f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // gem body
    const float br = r - track * 2.2f;
    auto body = Rectangle<float> (c.x - br, c.y - br, br * 2, br * 2);
    drawGem (g, body, accent, s.isMouseOverOrDragging() ? 0.35f : 0.0f);

    // engraved pointer
    const auto p1 = c.getPointOnCircumference (br * 0.25f, angle);
    const auto p2 = c.getPointOnCircumference (br * 0.82f, angle);
    g.setColour (Colours::black.withAlpha (0.65f));
    g.drawLine ({ p1, p2 }, 3.2f);
    g.setColour (Palette::cream.withAlpha (0.9f));
    g.fillEllipse (p2.x - 2.2f, p2.y - 2.2f, 4.4f, 4.4f);
}

void GranulumLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (3.0f);
    drawRecess (g, r, 7.0f);
    if (b.getToggleState())
    {
        g.setColour (Palette::pink.withAlpha (0.18f));
        g.fillRoundedRectangle (r.expanded (3.0f), 9.0f);
        g.setGradientFill (ColourGradient (Palette::pink.brighter (0.2f), r.getX(), r.getY(),
                                           Palette::pink.darker (0.3f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (Palette::pink.brighter (0.5f));
        g.drawRoundedRectangle (r, 7.0f, 1.2f);
        return;
    }
    g.setColour (Palette::teal.withAlpha (down ? 0.30f : over ? 0.16f : 0.05f));
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (Palette::teal.withAlpha (over ? 0.9f : 0.5f));
    g.drawRoundedRectangle (r, 7.0f, 1.2f);
}

void GranulumLook::drawButtonText (Graphics& g, TextButton& b, bool over, bool)
{
    g.setFont (uiFont (b.getHeight() < 30 ? 11.5f : (b.getButtonText().length() > 7 ? 12.5f : 14.0f), true, 0.1f));
    if (b.getToggleState())
    {
        g.setColour (Palette::pinkInk);
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
        return;
    }
    glowText (g, b.getButtonText(), b.getLocalBounds(), Justification::centred,
              over ? Palette::cream : Palette::teal, over ? 1.0f : 0.5f);
}

void GranulumLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (3.0f);
    drawRecess (g, r, 6.0f);
    g.setColour (Palette::teal.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    Path arrow;
    const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::teal);
    g.fillPath (arrow);
}

Font GranulumLook::getComboBoxFont (ComboBox&) { return uiFont (12.0f, true, 0.02f); }

Label* GranulumLook::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (12.5f, false, 0.04f));
    l->setJustificationType (Justification::centred);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineWhenEditingColourId, Palette::teal);
    return l;
}

// ============================================================================
// Knob

Knob::Knob (AudioProcessorValueTreeState& state, const String& paramId, const String& t, Colour accent, bool isHero)
    : title (t), hero (isHero)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
    slider.setColour (Slider::rotarySliderFillColourId, accent);
    slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    slider.setColour (Slider::textBoxTextColourId, Palette::teal);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    auto* param = state.getParameter (paramId);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addAndMakeVisible (slider);
    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (hero ? 20 : 17);
    slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (getWidth(), 96), hero ? 20 : 18);
    slider.setBounds (r);
}

void Knob::paint (Graphics& g)
{
    g.setColour (Palette::cream.withAlpha (hero ? 0.95f : 0.7f));
    g.setFont (uiFont (hero ? 13.5f : 11.0f, true, 0.1f));
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (hero ? 20 : 17), Justification::centred);
}

// ============================================================================
// Screen (waveform + grains)

WaveformView::WaveformView (GranulumProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setOpaque (false);
    startTimerHz (40);
}

void WaveformView::resized()
{
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    glass = Image (Image::ARGB, w, h, true);
    Graphics g (glass);
    auto r = Rectangle<float> (0, 0, (float) w, (float) h);

    // grid
    const int cols = 12, rows = 6;
    for (int i = 1; i < cols; ++i)
    {
        const float x = r.getWidth() * (float) i / (float) cols;
        g.setColour (Palette::grid.withAlpha (0.06f)); g.fillRect (x - 2.0f, 0.0f, 4.0f, r.getHeight());
        g.setColour (Palette::grid.withAlpha (0.38f)); g.fillRect (x - 0.5f, 0.0f, 1.0f, r.getHeight());
    }
    for (int i = 1; i < rows; ++i)
    {
        const float y = r.getHeight() * (float) i / (float) rows;
        g.setColour (Palette::grid.withAlpha (0.06f)); g.fillRect (0.0f, y - 2.0f, r.getWidth(), 4.0f);
        g.setColour (Palette::grid.withAlpha (0.38f)); g.fillRect (0.0f, y - 0.5f, r.getWidth(), 1.0f);
    }

    // scanlines
    g.setColour (Colours::black.withAlpha (0.16f));
    for (int y = 0; y < h; y += 3)
        g.fillRect (0, y, w, 1);

    // dust on the glass
    Random rnd (77);
    for (int i = 0; i < w * h / 900; ++i)
    {
        g.setColour (Palette::cream.withAlpha (0.05f + rnd.nextFloat() * 0.12f));
        const float s = 0.8f + rnd.nextFloat() * 1.4f;
        g.fillEllipse (rnd.nextFloat() * (float) w, rnd.nextFloat() * (float) h, s, s);
    }

    // vignette + inner shadow (CRT curvature feel)
    ColourGradient vg (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                       Colours::black.withAlpha (0.75f), 0.0f, 0.0f, true);
    vg.addColour (0.55, Colours::transparentBlack);
    g.setGradientFill (vg);
    g.fillRect (r);

    // glass reflection
    ColourGradient rg (Colours::white.withAlpha (0.05f), 0, 0, Colours::transparentWhite, 0, r.getHeight() * 0.45f, false);
    g.setGradientFill (rg);
    g.fillRoundedRectangle (r.reduced (6.0f).withHeight (r.getHeight() * 0.42f), 10.0f);
}

void WaveformView::timerCallback()
{
    auto& e = proc.engine;
    const int n = e.vizCount.load (std::memory_order_acquire);
    grainX.resize ((size_t) n);
    grainA.resize ((size_t) n);
    grainHit.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        grainHit[(size_t) i] = e.vizHit[(size_t) i].load (std::memory_order_relaxed);
        grainX[(size_t) i] = e.vizPos[(size_t) i].load (std::memory_order_relaxed);
        grainA[(size_t) i] = e.vizAmp[(size_t) i].load (std::memory_order_relaxed);
    }
    const int h = e.vizHeadCount.load (std::memory_order_acquire);
    heads.resize ((size_t) h);
    for (int i = 0; i < h; ++i)
        heads[(size_t) i] = e.vizHead[(size_t) i].load (std::memory_order_relaxed);

    repaint();
}

void WaveformView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    {
        Path clip; clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);
    }

    // phosphor background
    g.setGradientFill (ColourGradient (Colour (0xff0f2321), r.getCentreX(), r.getCentreY(),
                                       Palette::screen, r.getX(), r.getY(), true));
    g.fillRect (r);

    auto inner = r.reduced (14.0f, 16.0f);
    const float mid = inner.getCentreY();
    const float halfH = inner.getHeight() * 0.5f;

    auto peaks = proc.getPeaks();
    if (peaks == nullptr || peaks->maxs.empty())
    {
        g.setFont (uiFont (17.0f, false, 0.08f));
        glowText (g, String::fromUTF8 ("ПЕРЕТАЩИ СЮДА СЭМПЛ"), getLocalBounds().withTrimmedBottom (24),
                  Justification::centred, dragHover ? Palette::cream : Palette::teal, 1.0f);
        g.setFont (uiFont (12.0f, false, 0.08f));
        g.setColour (Palette::tealDim);
        g.drawText ("WAV  /  AIFF  /  FLAC  /  MP3", getLocalBounds().withTrimmedTop (34), Justification::centred);
        g.drawImageAt (glass, 0, 0);
        return;
    }

    const float position = proc.apvts.getRawParameterValue (ParamID::position)->load();
    const bool transOn   = proc.apvts.getRawParameterValue (ParamID::transients)->load() > 0.5f;
    auto xOf = [&] (float norm) { return inner.getX() + norm * inner.getWidth(); };

    // transient markers
    if (transOn)
        for (float t : peaks->transients)
        {
            const float x = xOf (t);
            g.setColour (Palette::pink.withAlpha (0.10f)); g.fillRect (x - 3.0f, r.getY(), 6.0f, r.getHeight());
            g.setColour (Palette::pink.withAlpha (0.55f)); g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
            Path tri; tri.addTriangle (x - 4.0f, r.getBottom() - 2.0f, x + 4.0f, r.getBottom() - 2.0f, x, r.getBottom() - 8.0f);
            g.setColour (Palette::pink.withAlpha (0.8f)); g.fillPath (tri);
        }

    // waveform
    {
        const int bins = (int) peaks->maxs.size();
        const int cols = jmax (2, (int) inner.getWidth());
        Path fill, top;
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
        fill.startNewSubPath (inner.getX(), tops[0]);
        top.startNewSubPath (inner.getX(), tops[0]);
        for (int x = 1; x < cols; ++x)
        {
            fill.lineTo (inner.getX() + (float) x, tops[(size_t) x]);
            top.lineTo (inner.getX() + (float) x, tops[(size_t) x]);
        }
        Path bottom;
        bottom.startNewSubPath (inner.getX() + (float) (cols - 1), bots[(size_t) cols - 1]);
        for (int x = cols - 1; x >= 0; --x)
        {
            fill.lineTo (inner.getX() + (float) x, bots[(size_t) x]);
            bottom.lineTo (inner.getX() + (float) x, bots[(size_t) x]);
        }
        fill.closeSubPath();

        g.setColour (Palette::grid.withAlpha (0.16f));
        g.fillPath (fill);
        for (auto* p : { &top, &bottom })
        {
            g.setColour (Palette::grid.withAlpha (0.12f));
            g.strokePath (*p, PathStrokeType (4.0f));
            g.setColour (Palette::grid.withAlpha (0.85f));
            g.strokePath (*p, PathStrokeType (1.1f));
        }
    }

    // voice playheads
    for (float h : heads)
    {
        float hp = position + h;
        hp -= std::floor (hp);
        const float x = xOf (hp);
        g.setColour (Palette::aqua.withAlpha (0.15f)); g.fillRect (x - 3.0f, r.getY(), 6.0f, r.getHeight());
        g.setColour (Palette::aqua.withAlpha (0.6f));  g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
    }

    // position marker
    {
        const float x = xOf (position);
        g.setColour (Palette::pink.withAlpha (0.18f)); g.fillRect (x - 4.0f, r.getY(), 8.0f, r.getHeight());
        g.setColour (Palette::pink.interpolatedWith (Colours::white, 0.2f)); g.fillRect (x - 1.0f, r.getY(), 2.0f, r.getHeight());
        Path tri; tri.addTriangle (x - 6.0f, r.getY() + 2.0f, x + 6.0f, r.getY() + 2.0f, x, r.getY() + 10.0f);
        g.fillPath (tri);
    }

    // grains: glowing phosphor dots
    for (size_t i = 0; i < grainX.size(); ++i)
    {
        const bool isHit = i < grainHit.size() && grainHit[i] != 0;
        const float x = xOf (grainX[i]);
        const uint32 hsh = (uint32) (i * 2654435761u);
        const float yN = ((float) (hsh % 1000) / 1000.0f) * 2.0f - 1.0f;
        const float y = mid + yN * halfH * 0.85f;
        const float a = jlimit (0.0f, 1.0f, grainA[i]);
        const auto col = (isHit || i % 3 == 0) ? Palette::pink : Palette::grid;
        const float rad = (isHit ? 3.0f : 1.5f) + a * 3.5f;
        g.setColour (col.withAlpha (0.10f * a)); g.fillEllipse (x - rad * 3.5f, y - rad * 3.5f, rad * 7.0f, rad * 7.0f);
        g.setColour (col.withAlpha (0.30f * a)); g.fillEllipse (x - rad * 1.6f, y - rad * 1.6f, rad * 3.2f, rad * 3.2f);
        g.setColour (col.interpolatedWith (Colours::white, 0.7f).withAlpha (0.4f + 0.6f * a));
        g.fillEllipse (x - rad * 0.5f, y - rad * 0.5f, rad, rad);
    }

    g.drawImageAt (glass, 0, 0);

    if (dragHover)
    {
        g.setColour (Palette::cream.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (2.0f), 10.0f, 2.0f);
    }
}

void WaveformView::setPositionFromMouse (const MouseEvent& e)
{
    auto inner = getLocalBounds().toFloat().reduced (14.0f, 16.0f);
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
// Pink sample block

SampleBlock::SampleBlock()
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setTooltip (String::fromUTF8 ("Нажми, чтобы загрузить сэмпл"));
}

void SampleBlock::setText (const String& top, const String& main)
{
    if (top != topText || main != mainText)
    {
        topText = top; mainText = main;
        repaint();
    }
}

void SampleBlock::paint (Graphics& g)
{
    auto outer = getLocalBounds().toFloat();

    // translucent plastic housing
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (outer.translated (0, 3.0f), 12.0f);
    g.setColour (Colour (0xff6d6262).withAlpha (0.55f));
    g.fillRoundedRectangle (outer, 12.0f);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (outer.reduced (0.5f), 12.0f, 1.0f);

    auto block = outer.reduced (12.0f);
    auto base = hover ? Palette::pink.brighter (0.12f) : Palette::pink;

    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (block.translated (0, 3.0f), 8.0f);
    g.setGradientFill (ColourGradient (base.brighter (0.15f), block.getX(), block.getY(),
                                       base.darker (0.35f), block.getRight(), block.getBottom(), false));
    g.fillRoundedRectangle (block, 8.0f);

    {
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (block, 8.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (0.9f);
        const auto& sp = GranulumLook::speckle();
        for (float y = block.getY(); y < block.getBottom(); y += 120.0f)
            for (float x = block.getX(); x < block.getRight(); x += 120.0f)
                g.drawImage (sp, Rectangle<float> (x, y, 120.0f, 120.0f));
    }

    // bevel
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawLine (block.getX() + 8, block.getY() + 1, block.getRight() - 8, block.getY() + 1, 1.2f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawLine (block.getX() + 8, block.getBottom() - 1, block.getRight() - 8, block.getBottom() - 1, 1.5f);

    // engraved text
    auto tr = block.toNearestInt().reduced (14, 12);
    auto topR = tr.removeFromTop (20);
    auto engrave = [&] (const String& s, Rectangle<int> rr, Font f, int lines)
    {
        g.setFont (f);
        g.setColour (base.brighter (0.4f).withAlpha (0.6f));
        g.drawFittedText (s, rr.translated (0, 1), Justification::centred, lines, 0.8f);
        g.setColour (Palette::pinkInk);
        g.drawFittedText (s, rr, Justification::centred, lines, 0.8f);
    };
    engrave (topText, topR, uiFont (13.0f, false, 0.08f), 1);
    engrave (mainText, tr, uiFont (22.0f, true, 0.06f), 2);
}

// ============================================================================
// Editor

GranulumEditor::GranulumEditor (GranulumProcessor& p)
    : AudioProcessorEditor (&p), proc (p), waveform (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);

    auto& s = proc.apvts;
    auto add = [&] (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* name, Colour c, bool hero = false)
    {
        group.push_back (std::make_unique<Knob> (s, id, name, c, hero));
        addAndMakeVisible (*group.back());
    };

    add (heroKnobs,   ParamID::attack,      "Attack",        Palette::pink, true);
    add (heroKnobs,   ParamID::hold,        "Hold",          Palette::pink, true);
    add (heroKnobs,   ParamID::spacing,     "Gr. Spacing",   Palette::pink, true);
    add (heroKnobs,   ParamID::waveSpacing, "Wave Spacing",  Palette::pink, true);

    add (motionKnobs, ParamID::position,    "Start",         Palette::pink);
    add (motionKnobs, ParamID::rand,        "Rand",          Palette::pink);
    add (motionKnobs, ParamID::smear,       "Smear",         Palette::pink);
    add (motionKnobs, ParamID::pan,         "Pan",           Palette::pink);
    add (motionKnobs, ParamID::lfoDepth,    "LFO Depth",     Palette::pink);
    add (motionKnobs, ParamID::lfoRate,     "LFO Speed",     Palette::pink);

    add (pitchKnobs,  ParamID::pitch,       "Pitch",         Palette::pink);
    add (pitchKnobs,  ParamID::pitchSpray,  "Pitch Spray",   Palette::pink);
    add (pitchKnobs,  ParamID::octave,      "Octaves",       Palette::pink);
    add (pitchKnobs,  ParamID::reverse,     "Reverse",       Palette::pink);

    add (fxKnobs,     ParamID::cutoff,      "Cutoff",        Palette::pink);
    add (fxKnobs,     ParamID::reso,        "Reso",          Palette::pink);
    add (fxKnobs,     ParamID::space,       "Space",         Palette::pink);
    add (fxKnobs,     ParamID::room,        "Room",          Palette::pink);
    add (fxKnobs,     ParamID::output,      "Output",        Palette::pink);

    transButton.setClickingTogglesState (true);
    addAndMakeVisible (transButton);
    transAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, ParamID::transients, transButton);

    filterType.addItemList ({ "LOW PASS", "BAND PASS", "HIGH PASS" }, 1);
    addAndMakeVisible (filterType);
    filterTypeAttachment = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, ParamID::ftype, filterType);

    addAndMakeVisible (waveform);

    for (auto* b : { &rndGrains, &rndPitch, &rndFx }) addAndMakeVisible (*b);
    rndGrains.onClick = [this] { proc.randomizeSection (0); };
    rndPitch.onClick  = [this] { proc.randomizeSection (1); };
    rndFx.onClick     = [this] { proc.randomizeSection (2); };

    sampleName.setFont (uiFont (14.0f, false, 0.06f));
    sampleName.setColour (Label::textColourId, Palette::cream.withAlpha (0.8f));
    sampleName.setJustificationType (Justification::centredLeft);
    sampleName.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sampleName);

    addAndMakeVisible (randomButton);
    addAndMakeVisible (loadButton);
    randomButton.onClick = [this] { proc.randomize(); };
    loadButton.onClick   = [this] { launchChooser(); };

    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffcfc6ba));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff141516));
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff6f6a63));
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Palette::teal.withAlpha (0.85f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::pink.withAlpha (0.35f));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.4f));
    keyboard.setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, Colour (0xff141516));
    keyboard.setColour (MidiKeyboardComponent::upDownButtonArrowColourId, Palette::teal);
    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (18.0f);
    keyboard.setLowestVisibleKey (36);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (1080.0 / 740.0);
    setResizeLimits (860, 589, 1620, 1110);
    setSize (1080, 740);
    startTimerHz (20);
}

GranulumEditor::~GranulumEditor()
{
    setLookAndFeel (nullptr);
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
    const auto name = proc.getSampleName();
    const String text = statusText.isNotEmpty() ? "ERROR: " + statusText
                      : name.isEmpty() ? String()
                      : String::fromUTF8 ("\xe2\x80\x94  ") + File::createFileWithoutCheckingPath (name).getFileNameWithoutExtension()
                        + "   " + String (proc.getSampleLengthSeconds(), 2) + " s";
    if (sampleName.getText() != text) sampleName.setText (text, dontSendNotification);

    meter    = jmax (proc.outputLevel.load(), meter * 0.85f);
    activity = jmax ((float) proc.engine.vizCount.load() / 20.0f, activity * 0.8f);
    repaint (gemsArea.expanded (20));
    repaint (ledArea.expanded (16));
}

// ----------------------------------------------------------------------------

void GranulumEditor::rebuildBackground()
{
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    const float sc = (float) w / 1080.0f;

    background = Image (Image::RGB, w, h, false);
    {
        // per-pixel grain noise on charcoal
        Image::BitmapData bd (background, Image::BitmapData::writeOnly);
        Random rnd (4242);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const int n = rnd.nextInt (19) - 9;
                const int v = jlimit (0, 255, 30 + n);
                bd.setPixelColour (x, y, Colour ((uint8) v, (uint8) (v + 1), (uint8) (v + 3)));
            }
    }

    Graphics g (background);
    Random rnd (99);

    // mottled blotches
    for (int i = 0; i < 60; ++i)
    {
        const float x = rnd.nextFloat() * (float) w, y = rnd.nextFloat() * (float) h;
        const float rr = (40.0f + rnd.nextFloat() * 160.0f) * sc;
        const bool light = rnd.nextBool();
        g.setGradientFill (ColourGradient ((light ? Colours::white : Colours::black).withAlpha (light ? 0.025f : 0.08f), x, y,
                                           Colours::transparentBlack, x + rr, y, true));
        g.fillEllipse (x - rr, y - rr, rr * 2, rr * 2);
    }

    // scratches
    for (int i = 0; i < 70; ++i)
    {
        const float x = rnd.nextFloat() * (float) w, y = rnd.nextFloat() * (float) h;
        const float a = rnd.nextFloat() * MathConstants<float>::twoPi, l = (6.0f + rnd.nextFloat() * 30.0f) * sc;
        g.setColour (Colours::white.withAlpha (0.03f + rnd.nextFloat() * 0.04f));
        g.drawLine (x, y, x + std::cos (a) * l, y + std::sin (a) * l, 0.7f);
    }

    // panel vignette + bevel
    auto full = Rectangle<float> (0, 0, (float) w, (float) h);
    ColourGradient vg (Colours::transparentBlack, full.getCentreX(), full.getCentreY() * 0.8f,
                       Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
    vg.addColour (0.6, Colours::transparentBlack);
    g.setGradientFill (vg);
    g.fillRect (full);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (full.reduced (3.0f), 18.0f, 1.5f);
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (full.reduced (1.0f), 20.0f, 2.0f);

    // screws
    const float m = 20.0f * sc;
    drawScrew (g, { m, m }, 9.0f * sc, 0.7f);
    drawScrew (g, { (float) w - m, m }, 9.0f * sc, 2.1f);
    drawScrew (g, { m, (float) h - m }, 9.0f * sc, 1.3f);
    drawScrew (g, { (float) w - m, (float) h - m }, 9.0f * sc, 0.2f);

    // title
    g.setFont (uiFont (54.0f * sc, false, 0.01f));
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawText ("Granulum", titleArea.withHeight ((int) (60 * sc)).translated (0, 2), Justification::centredLeft);
    g.setColour (Palette::cream);
    g.drawText ("Granulum", titleArea.withHeight ((int) (60 * sc)), Justification::centredLeft);
    g.setFont (uiFont (21.0f * sc, false, 0.07f));
    g.setColour (Palette::teal);
    g.drawText ("GRANULAR SAMPLER", titleArea.withTrimmedTop ((int) (58 * sc)), Justification::topLeft);

    // main frame with screen + sample block
    drawFrame (g, screenFrame.toFloat(), "Grain Field", Colours::transparentBlack);
    drawRecess (g, waveform.getBounds().toFloat(), 10.0f);

    drawFrame (g, grainArea.toFloat(), "Grains", Palette::pink);
    drawFrame (g, pitchArea.toFloat(), "Pitch",  Palette::pink);
    drawFrame (g, fxArea.toFloat(),    "Filter / Space", Palette::pink);
    if (! heroKnobs.empty())
    {
        // thin divider between the hero row and the motion row
        auto hr = heroKnobs.front()->getBounds();
        g.setColour (Palette::teal.withAlpha (0.18f));
        g.drawHorizontalLine (hr.getBottom() + 4, (float) grainArea.getX() + 18.0f, (float) grainArea.getRight() - 18.0f);
    }

    drawRecess (g, keyboard.getBounds().toFloat().expanded (2.0f), 6.0f);

    // ---------------- sand overlay ----------------
    sandOverlay = Image (Image::ARGB, w, h, true);
    Graphics sg (sandOverlay);
    Random sr (2024);

    auto noSand = [&] (float x, float y)
    {
        auto p = Point<int> ((int) x, (int) y);
        if (waveform.getBounds().reduced (6).contains (p)) return true;
        if (keyboard.getBounds().reduced (2).contains (p)) return true;
        for (auto* grp : { &heroKnobs, &motionKnobs, &pitchKnobs, &fxKnobs })
            for (auto& k : *grp)
                if (k->getBounds().reduced (6).contains (p)) return true;
        if (filterType.getBounds().contains (p) || randomButton.getBounds().contains (p) || loadButton.getBounds().contains (p) || transButton.getBounds().contains (p)
            || rndGrains.getBounds().contains (p) || rndPitch.getBounds().contains (p) || rndFx.getBounds().contains (p)) return true;
        return false;
    };

    auto grain = [&] (float x, float y, float alphaScale)
    {
        if (noSand (x, y)) return;
        const float s = (0.7f + sr.nextFloat() * sr.nextFloat() * 2.6f) * jmax (0.8f, sc);
        const float t = sr.nextFloat();
        const auto c = t < 0.33f ? Palette::sand : t < 0.66f ? Palette::sand.darker (0.35f) : Palette::sand.brighter (0.25f);
        sg.setColour (c.withAlpha ((0.35f + sr.nextFloat() * 0.6f) * alphaScale));
        sg.fillEllipse (x, y, s, s * (0.7f + sr.nextFloat() * 0.6f));
    };

    // light dusting everywhere
    for (int i = 0; i < w * h / 260; ++i)
        grain (sr.nextFloat() * (float) w, sr.nextFloat() * (float) h, 0.45f);

    // heavier at the bottom edge and corners
    for (int i = 0; i < w * 3; ++i)
    {
        const float y = (float) h - std::pow (sr.nextFloat(), 2.2f) * 60.0f * sc;
        grain (sr.nextFloat() * (float) w, y, 1.0f);
    }
    for (auto corner : { Point<float> (0, 0), Point<float> ((float) w, 0), Point<float> (0, (float) h), Point<float> ((float) w, (float) h) })
        for (int i = 0; i < 700; ++i)
        {
            const float a = sr.nextFloat() * MathConstants<float>::twoPi;
            const float d = std::pow (sr.nextFloat(), 1.8f) * 110.0f * sc;
            grain (corner.x + std::cos (a) * d, corner.y + std::sin (a) * d, 0.9f);
        }

    // sand settled on ledges (top edges of frames, bottom edges of recesses)
    auto ledge = [&] (Rectangle<int> r, bool top, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const float x = (float) r.getX() + 10.0f + sr.nextFloat() * (float) (r.getWidth() - 20);
            const float off = std::pow (sr.nextFloat(), 2.0f) * 7.0f * sc;
            const float y = top ? (float) r.getY() - 1.0f - off : (float) r.getBottom() + 1.0f + off;
            grain (x, y, 1.0f);
        }
    };
    for (auto r : { screenFrame, grainArea, pitchArea, fxArea })
    {
        ledge (r, true, r.getWidth() / 2);
        ledge (r, false, r.getWidth() / 3);
    }
    ledge (waveform.getBounds().expanded (4), false, waveform.getWidth());
    ledge (keyboard.getBounds().expanded (3), true, keyboard.getWidth() / 2);

    // a few clumps
    for (int c = 0; c < 18; ++c)
    {
        const float cx = sr.nextFloat() * (float) w;
        const float cy = sr.nextFloat() < 0.6f ? (float) h - sr.nextFloat() * 40.0f * sc : sr.nextFloat() * (float) h;
        for (int i = 0; i < 70; ++i)
            grain (cx + sr.nextFloat() * 18.0f * sc - 9.0f * sc, cy + sr.nextFloat() * 8.0f * sc - 4.0f * sc, 1.0f);
    }
}

void GranulumEditor::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImageAt (background, 0, 0);
    else
        g.fillAll (Palette::panel);

    // indicator gems (top right): light up with output level, like a meter
    const Colour gemCols[6] = { Palette::pink, Palette::pink, Palette::pink,
                                Palette::pink, Palette::pink, Palette::pink };
    const float db = Decibels::gainToDecibels (meter, -60.0f);
    const float thresholds[6] = { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, -2.0f };
    const float gw = (float) gemsArea.getWidth() / 3.0f;
    const float gh = (float) gemsArea.getHeight() / 2.0f;
    const float gs = jmin (gw, gh) * 0.72f;
    for (int i = 0; i < 6; ++i)
    {
        const float cx = (float) gemsArea.getX() + gw * ((float) (i % 3) + 0.5f);
        const float cy = (float) gemsArea.getY() + gh * ((float) (i / 3) + 0.5f);
        const float glow = jlimit (0.0f, 1.0f, (db - thresholds[i]) / 6.0f + 0.5f);
        GranulumLook::drawGem (g, { cx - gs * 0.5f, cy - gs * 0.5f, gs, gs }, gemCols[i], glow);
    }

    // two status LEDs above the sample block: grain activity + clip
    const float ls = (float) ledArea.getHeight();
    GranulumLook::drawGem (g, { (float) ledArea.getRight() - ls * 2.4f, (float) ledArea.getY(), ls, ls },
                           Palette::pink, jlimit (0.0f, 1.0f, activity));
    GranulumLook::drawGem (g, { (float) ledArea.getRight() - ls, (float) ledArea.getY(), ls, ls },
                           Palette::pink, meter > 0.89f ? 1.0f : 0.0f);
}

void GranulumEditor::paintOverChildren (Graphics& g)
{
    if (sandOverlay.isValid())
        g.drawImageAt (sandOverlay, 0, 0);
}

void GranulumEditor::resized()
{
    const float sc = (float) getWidth() / 1080.0f;
    auto S = [sc] (float v) { return (int) std::round (v * sc); };

    auto r = getLocalBounds().reduced (S (40), S (26));

    // header
    auto header = r.removeFromTop (S (92));
    gemsArea  = header.removeFromRight (S (130)).withSizeKeepingCentre (S (130), S (78));
    titleArea = header.withTrimmedLeft (S (6));
    r.removeFromTop (S (10));

    // keyboard
    keysFrame = r.removeFromBottom (S (62));
    keyboard.setBounds (keysFrame.reduced (S (4), S (4)));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 36.0f);
    keyboard.setLowestVisibleKey (36);
    r.removeFromBottom (S (16));

    // knobs row
    auto knobs = r.removeFromBottom (S (238));
    r.removeFromBottom (S (16));

    // main frame
    screenFrame = r;
    auto inside = r.reduced (S (16), S (10));
    auto top = inside.removeFromTop (S (34));
    randomButton.setBounds (top.removeFromRight (S (128)));
    top.removeFromRight (S (6));
    loadButton.setBounds (top.removeFromRight (S (90)));
    top.removeFromRight (S (10));
    ledArea = top.removeFromRight (S (44)).withSizeKeepingCentre (S (44), S (16));
    top.removeFromLeft (S (120));
    sampleName.setBounds (top);
    inside.removeFromTop (S (6));
    waveform.setBounds (inside);

    // 10 columns: grains 5, pitch 2, fx 3
    const int gap = S (14);
    const float colW = (float) (knobs.getWidth() - gap * 2) / 10.0f;
    grainArea = knobs.removeFromLeft ((int) (colW * 5)); knobs.removeFromLeft (gap);
    pitchArea = knobs.removeFromLeft ((int) (colW * 2)); knobs.removeFromLeft (gap);
    fxArea    = knobs;

    auto layoutRow = [] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> row)
    {
        const int cw = row.getWidth() / (int) ks.size();
        for (int i = 0; i < (int) ks.size(); ++i)
            ks[(size_t) i]->setBounds (row.getX() + i * cw, row.getY(), cw, row.getHeight());
    };

    {
        auto g = grainArea.withTrimmedTop (S (30)).reduced (S (10), S (6));
        transButton.setBounds (grainArea.getRight() - S (132), grainArea.getY() + S (8), S (118), S (28));
        rndGrains.setBounds (transButton.getX() - S (92), grainArea.getY() + S (8), S (86), S (28));
        auto hero = g.removeFromTop ((int) (g.getHeight() * 0.58f));
        g.removeFromTop (S (8));
        layoutRow (heroKnobs, hero);
        layoutRow (motionKnobs, g);
    }

    auto layoutGrid = [&] (std::vector<std::unique_ptr<Knob>>& ks, Rectangle<int> area, int cols, int extraSlots = 0)
    {
        area = area.withTrimmedTop (S (34)).reduced (S (8), S (6));
        const int total = (int) ks.size() + extraSlots;
        const int rows = (total + cols - 1) / cols;
        const int cw = area.getWidth() / cols;
        const int rh = area.getHeight() / rows;
        for (int i = 0; i < (int) ks.size(); ++i)
            ks[(size_t) i]->setBounds (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * rh, cw, rh);
        return Rectangle<int> (area.getX() + (total - 1) % cols * cw, area.getY() + (total - 1) / cols * rh, cw, rh);
    };

    rndPitch.setBounds (pitchArea.getRight() - S (96), pitchArea.getY() + S (8), S (86), S (28));
    rndFx.setBounds    (fxArea.getRight()    - S (96), fxArea.getY()    + S (8), S (86), S (28));
    layoutGrid (pitchKnobs, pitchArea, 2);
    auto comboSlot = layoutGrid (fxKnobs, fxArea, 3, 1);
    filterType.setBounds (comboSlot.withSizeKeepingCentre (comboSlot.getWidth() - S (4), S (32)));

    rebuildBackground();
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
