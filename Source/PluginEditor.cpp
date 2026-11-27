#include <map>
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    // Inter (OFL) is embedded, so every computer shows the same font
    Typeface::Ptr interFace (bool bold)
    {
        static Typeface::Ptr reg = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf, (size_t) BinaryData::InterRegular_otfSize);
        static Typeface::Ptr semi = Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, (size_t) BinaryData::InterSemiBold_otfSize);
        return bold ? semi : reg;
    }

    Font uiFont (float size, bool bold = false, float kerning = 0.0f)
    {
        if (Palette::fontName.isNotEmpty())
            if (auto tf = interFace (bold))
                return Font { FontOptions (tf).withHeight (size * 0.96f).withKerningFactor (kerning + 0.02f) };
        return Font { FontOptions (size, bold ? Font::bold : Font::plain).withKerningFactor (kerning) };
    }

    void drawLed (Graphics& g, Rectangle<float> c, Colour on, Colour off, float level)
    {
        level = jlimit (0.0f, 1.0f, level);
        if (Palette::flatLed)
        {
            // flat: filled = on, thin ring = off
            auto d = c.reduced (c.getWidth() * 0.12f);
            if (level > 0.5f) { g.setColour (on); g.fillEllipse (d); }
            else { g.setColour (Palette::textDim.withAlpha (0.8f)); g.drawEllipse (d.reduced (0.6f), 1.2f); }
            return;
        }
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

#ifndef GRAINFORM_VERSION
 #define GRAINFORM_VERSION "dev"
#endif
// "0.9.8" -> "0.98", "1.0.0" -> "1.0", "1.2.3" -> "1.23"
static String versionString()
{
   #ifdef GRAINFORM_LABEL
    if (String (GRAINFORM_LABEL).isNotEmpty()) return GRAINFORM_LABEL;
   #endif
    auto parts = StringArray::fromTokens (GRAINFORM_VERSION, ".", "");
    if (parts.size() < 3) return GRAINFORM_VERSION;
    return parts[0] + "." + parts[1] + (parts[2] == "0" ? String() : parts[2]);
}

void GranulumLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                     float start, float end, Slider& s)
{
    if (flatKnobs && Palette::hardware)
    {
        // hardware knob: shadow, black skirt, knurled grip, brushed aluminium cap, ivory pointer
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (h < 50 ? 1.5f : 5.0f);
        const bool small = h < 50;
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start);
        const float br = r * (small ? 0.8f : 0.78f);
        auto circ = [&c] (float rr) { return Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c); };
        // shadow
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillEllipse (circ (br * 1.02f).translated (0.0f, br * 0.13f));
        g.setColour (Colours::black.withAlpha (0.18f));
        g.fillEllipse (circ (br * 1.12f).translated (0.0f, br * 0.2f));
        // skirt
        g.setGradientFill (ColourGradient (Colour (0xff3a3c3f), c.x, c.y - br, Colour (0xff121314), c.x, c.y + br, false));
        g.fillEllipse (circ (br));
        if (! small)
        {
            // skirt tick marks
            g.setColour (Colour (0xffcfc8bf).withAlpha (0.55f));
            for (int k = 0; k <= 10; ++k)
            {
                const float a = start + (end - start) * (float) k / 10.0f;
                g.drawLine (Line<float> (c.getPointOnCircumference (br * 0.9f, a), c.getPointOnCircumference (br * 0.98f, a)), 1.0f);
            }
        }
        // knurled grip
        const float gr = br * (small ? 0.86f : 0.8f);
        g.setGradientFill (ColourGradient (Colour (0xff2c2e31), c.x, c.y - gr, Colour (0xff0d0e0f), c.x, c.y + gr, false));
        g.fillEllipse (circ (gr));
        const int teeth = small ? 24 : 40;
        for (int k = 0; k < teeth; ++k)
        {
            const float a = MathConstants<float>::twoPi * (float) k / (float) teeth;
            const float lit = 0.5f + 0.5f * std::cos (a);             // light from the top
            g.setColour (Colours::white.withAlpha (0.05f + 0.13f * lit));
            g.drawLine (Line<float> (c.getPointOnCircumference (gr * 0.84f, a), c.getPointOnCircumference (gr * 0.99f, a)), 0.8f);
        }
        // cap: brushed aluminium (conical sheen)
        const float cr = gr * 0.8f;
        {
            ColourGradient cap (Colour (0xff9a9ea3), c.x - cr * 0.6f, c.y - cr, Colour (0xff3e4145), c.x + cr * 0.5f, c.y + cr, false);
            cap.addColour (0.45, Colour (0xff6d7176));
            g.setGradientFill (cap);
            g.fillEllipse (circ (cr));
            g.setColour (Colours::white.withAlpha (0.07f));
            for (int k = 1; k < 5; ++k) g.drawEllipse (circ (cr * (float) k / 5.0f), 0.6f);
            g.setColour (Colours::white.withAlpha (0.28f));
            g.drawEllipse (circ (cr).reduced (0.5f), 0.8f);
            g.setColour (Colours::black.withAlpha (0.5f));
            g.drawEllipse (circ (cr + 0.6f), 0.8f);
        }
        // pointer: an ivory line from the cap out to the skirt
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawLine ({ c.getPointOnCircumference (cr * 0.25f, angle).translated (0.0f, 0.8f), c.getPointOnCircumference (br * 0.96f, angle).translated (0.0f, 0.8f) }, jmax (1.8f, br * 0.1f));
        g.setColour (s.isMouseOverOrDragging() ? Colours::white : Colour (0xffece6df));
        g.drawLine ({ c.getPointOnCircumference (cr * 0.25f, angle), c.getPointOnCircumference (br * 0.96f, angle) }, jmax (1.4f, br * 0.075f));
        return;
    }
    if (flatKnobs)
    {
        // flat, quiet knob: a plain disc with one pointer line
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (h < 50 ? 1.5f : 5.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float angle = start + pos * (end - start);
        const float br = r * (h < 50 ? 0.8f : 0.78f);
        g.setColour (s.isMouseOverOrDragging() ? Palette::knob.brighter (0.12f) : Palette::knob);
        g.fillEllipse (Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (c));
        if (! Palette::knobRing.isTransparent())
        {
            g.setColour (Palette::knobRing);
            g.drawEllipse (Rectangle<float> (br * 2.0f, br * 2.0f).withCentre (c).reduced (0.6f), 1.2f);
        }
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
    if (! Palette::hardware) { drawButtonBase (g, b, over, down); return; }
    // hardware: a real key cap — drop shadow, the cap, then a moulded top light and a dark lower lip
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float rad = 5.0f;
    g.setColour (Colours::black.withAlpha (down ? 0.25f : 0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, down ? 0.6f : 1.8f).expanded (0.6f), rad + 0.6f);
    drawButtonBase (g, b, over, down);
    g.saveState();
    Path clip; clip.addRoundedRectangle (r, rad); g.reduceClipRegion (clip);
    ColourGradient mould (Colours::white.withAlpha (down ? 0.02f : 0.11f), 0, r.getY(), Colours::black.withAlpha (down ? 0.05f : 0.22f), 0, r.getBottom(), false);
    mould.addColour (0.45, Colours::white.withAlpha (0.0f));
    g.setGradientFill (mould); g.fillRect (r);
    g.setColour (Colours::white.withAlpha (down ? 0.04f : 0.16f));
    g.fillRect (r.withHeight (1.0f).reduced (rad * 0.6f, 0.0f).translated (0.0f, 1.0f));
    g.restoreState();
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.expanded (0.5f), rad + 0.5f, 1.0f);
}

void GranulumLook::drawButtonBase (Graphics& g, Button& b, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const float rad = 6.0f;
    if (b.getProperties().contains ("sounds"))
    {
        // the SOUNDS button: copper, with a small "library" icon, so it invites a click
        const int st = (int) b.getProperties()["sounds"];
        const bool open = b.getToggleState();
        if (st == 4)
        {
            // exactly like a lit GRAIN button
            const auto c = Palette::accent;
            g.setColour (c.withAlpha (0.18f));
            g.fillRoundedRectangle (r.expanded (1.5f), rad + 1.5f);
            // chosen (CHORD set / menu open): a clearly brighter copper
            g.setColour (c.interpolatedWith (Colours::white, open ? (over ? 0.30f : 0.24f) : over ? 0.10f : 0.0f));
            g.fillRoundedRectangle (r, rad);
            // a soft light glint sweeps across every few seconds, so the button invites a click
            if (! open)
            {
                const double t = std::fmod (Time::getMillisecondCounterHiRes() * 0.001 + (double) b.getProperties().getWithDefault ("glintOffset", 0.0), 1.8);
                if (t < 0.9)
                {
                    const float k = (float) (t / 0.9);
                    const float ease = k * k * (3.0f - 2.0f * k);
                    const float bw = r.getWidth() * 0.38f;
                    const float x = r.getX() - bw + (r.getWidth() + bw * 2.0f) * ease;
                    g.saveState();
                    Path clip; clip.addRoundedRectangle (r, rad);
                    g.reduceClipRegion (clip);
                    ColourGradient band (Colours::white.withAlpha (0.0f), x - bw * 0.5f, r.getY(),
                                         Colours::white.withAlpha (0.0f), x + bw * 0.5f, r.getBottom(), false);
                    band.addColour (0.5, Colours::white.withAlpha (0.6f * std::sin (k * MathConstants<float>::pi)));
                    g.setGradientFill (band);
                    g.fillRect (r);
                    g.restoreState();
                }
            }
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
            return;
        }
        if (st >= 3)
        {
            g.setColour (Palette::accent.withAlpha (over || open ? 0.30f : 0.18f));
            g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
        }
        if (st >= 2)
        {
            g.setColour (Palette::accent.withAlpha (open ? 0.42f : over ? 0.32f : 0.20f));
            g.fillRoundedRectangle (r, rad);
        }
        else
        {
            g.setColour (over || open ? Palette::accent.withAlpha (0.14f) : Colours::transparentBlack);
            g.fillRoundedRectangle (r, rad);
        }
        g.setColour (Palette::accent.withAlpha (over || open ? 1.0f : 0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.2f);
        // icon: three stacked "records" with a waveform line
        const float ix = r.getX() + 8.0f, iy = r.getCentreY();
        g.setColour (Palette::accent.brighter (over ? 0.35f : 0.15f));
        for (int k = 0; k < 3; ++k)
            g.fillRoundedRectangle (ix - 2.5f, iy - 4.0f + (float) k * 3.2f, 6.0f - (float) k * 1.2f, 1.7f, 0.8f);
        return;
    }
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
                const auto c = Palette::theme >= 5 ? Palette::accent
                             : Colour (0xff2f8a93).interpolatedWith (Palette::accent, toggleStyle == 5 ? 0.30f : 0.42f);
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
    g.setColour (Palette::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

// Text with a trailing dropdown arrow: the arrow is drawn as a shape, because the embedded font has no "▾" glyph
static void drawTextWithArrow (Graphics& g, const String& text, Rectangle<int> area)
{
    const String arrowGlyph = String::fromUTF8 ("\xe2\x96\xbe");
    if (! text.endsWith (arrowGlyph)) { g.drawText (text, area, Justification::centred, false); return; }
    const auto label = text.dropLastCharacters (1).trimEnd();
    const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label);
    const float aw = jmax (5.0f, g.getCurrentFont().getHeight() * 0.42f), gap = 4.0f;
    const float x0 = (float) area.getCentreX() - (tw + gap + aw) * 0.5f;
    g.drawText (label, Rectangle<float> (x0, (float) area.getY(), tw + 2.0f, (float) area.getHeight()), Justification::centredLeft, false);
    const float ax = x0 + tw + gap, ay = (float) area.getCentreY() + 0.5f;
    Path tri; tri.addTriangle (ax, ay - aw * 0.32f, ax + aw, ay - aw * 0.32f, ax + aw * 0.5f, ay + aw * 0.36f);
    g.fillPath (tri);
}

void GranulumLook::drawButtonText (Graphics& g, TextButton& b, bool over, bool)
{
    if (b.getProperties().contains ("sounds") && (int) b.getProperties()["sounds"] == 4)
    {
        // "SOUNDS ▾" with breathing room at both ends: shrink the font until it fits
        const auto text = b.getButtonText();
        // the fitted size is measured once per size/text and kept on the button (measuring every frame of the glint
        // cost ~2.5 ms per paint)
        const String key = text + "|" + String (b.getWidth()) + "x" + String (b.getHeight());
        auto& props = b.getProperties();
        float fs;
        if (props["fitKey"].toString() == key) fs = (float) props["fitFs"];
        else
        {
            const float room = (float) b.getWidth() - 16.0f;
            fs = b.getHeight() < 30 ? 11.5f : 13.5f;
            while (fs > 8.5f && (float) GlyphArrangement::getStringWidthInt (uiFont (fs, false, 0.06f), text) > room) fs -= 0.25f;
            props.set ("fitKey", key); props.set ("fitFs", fs);
        }
        g.setFont (uiFont (fs, false, 0.06f));
        g.setColour (Palette::onAccent());
        drawTextWithArrow (g, text, b.getLocalBounds());
        return;
    }
    if (b.getProperties().contains ("sounds"))
    {
        g.setFont (uiFont (10.5f, true, 0.06f));
        g.setColour (over || b.getToggleState() ? Palette::hi : Palette::accent.brighter (0.35f));
        g.drawText (b.getButtonText().upToFirstOccurrenceOf (" ", false, false), b.getLocalBounds().withTrimmedLeft (12), Justification::centred, false);
        return;
    }
    g.setFont (uiFont (b.getHeight() < 30 ? 11.5f : 13.5f, false, 0.08f));
    const auto onText = toggleStyle == 0 ? Palette::screen : toggleStyle == 2 ? Palette::accent.brighter (0.2f)
                      : toggleStyle >= 5 ? (Palette::theme >= 5 ? Palette::onAccent() : Colours::white) : Palette::ice;
    g.setColour (b.getToggleState() ? onText : (over ? Palette::hi : Palette::text));
    drawTextWithArrow (g, b.getButtonText(), b.getLocalBounds());
}

void GranulumLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.5f);
    g.setColour (Palette::field.withAlpha (box.isEnabled() ? 1.0f : 0.5f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Palette::theme >= 5 ? Palette::edge : Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if ((bool) box.getProperties()["noArrow"]) return;   // 1.0.2: the filter type has < > arrows instead
    Path arrow;
    const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Palette::textDim);
    g.fillPath (arrow);
}

Font GranulumLook::getComboBoxFont (ComboBox&) { return uiFont (12.0f, false, 0.05f); }

void GranulumLook::positionComboBoxText (ComboBox& box, Label& label)
{
    if ((bool) box.getProperties()["noArrow"])
    {
        label.setBounds (2, 1, box.getWidth() - 4, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (Justification::centred);
        return;
    }
    LookAndFeel_V4::positionComboBoxText (box, label);
}
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
        g.setColour (l.getParentComponent()->findColour (Slider::textBoxTextColourId));   // 1.0.5: copper for SYNC BPM note values
        g.drawText (text, r, Justification::centred, false);
        return;
    }
    LookAndFeel_V4::drawLabel (g, l);
}

// ============================================================================
// LFO depth readout (0.99.2): like Serum, the depth shows as a number while you set it

namespace
{
    String depthText (float d)
    {
        const int pct = roundToInt (d * 100.0f);
        return (pct > 0 ? String ("+") : pct < 0 ? String (CharPointer_UTF8 ("\xe2\x88\x92")) : String()) + String (std::abs (pct)) + "%";
    }
    // mouse wheel: 5 % steps on a clean grid (Shift = 1 %), so two depths are easy to make equal
    float wheelDepth (float d, const MouseWheelDetails& w, bool fine)
    {
        const float step = fine ? 0.01f : 0.05f;
        const float dir = (w.deltaY > 0 ? 1.0f : -1.0f) * (w.isReversed ? -1.0f : 1.0f);
        return jlimit (-1.0f, 1.0f, (std::round (d / step) + dir) * step);
    }
    float snapDepth (float d) { return jlimit (-1.0f, 1.0f, std::round (d * 100.0f) / 100.0f); }
    void drawDepthTag (Graphics& g, float depth, Colour col, Point<float> centre, Rectangle<float> bounds)
    {
        const auto text = depthText (depth);
        const auto f = uiFont (9.5f, true, 0.02f);
        const float w = (float) GlyphArrangement::getStringWidthInt (f, text) + 10.0f, h = 14.0f;
        auto r = Rectangle<float> (w, h).withCentre (centre);
        r = r.withX (jlimit (bounds.getX(), jmax (bounds.getX(), bounds.getRight() - w), r.getX()))
             .withY (jlimit (bounds.getY(), jmax (bounds.getY(), bounds.getBottom() - h), r.getY()));
        g.setColour (Palette::screen.withAlpha (0.94f));
        g.fillRoundedRectangle (r, h * 0.5f);
        g.setColour (col.withAlpha (0.75f));
        g.drawRoundedRectangle (r.reduced (0.5f), h * 0.5f, 1.0f);
        g.setColour (col.brighter (0.25f));
        g.setFont (f);
        g.drawText (text, r, Justification::centred, false);
    }
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
    setTooltip ("LFO " + String (lfo + 1) + ": " + depthText (d) + "\n"
                + "Drag up/down or mouse wheel: depth (wheel = 5 % steps, Shift = 1 %). Double-click: remove.");
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
    const float sens = e.mods.isShiftDown() || e.mods.isCtrlDown() ? 2400.0f : 450.0f;   // heavy, so depth is easy to set
    const float d = snapDepth (startDepth - (float) e.getDistanceFromDragStartY() / sens);
    proc.setModulation (owner.target, lfo, d);
    owner.readout.show (owner.target, lfo, true);
    updateTooltip();
    owner.repaint();
}

void ModChip::mouseEnter (const MouseEvent&) { owner.readout.show (owner.target, lfo, true); owner.repaint(); }
void ModChip::mouseExit  (const MouseEvent& e) { if (! e.mouseWasDraggedSinceMouseDown() || ! e.mods.isAnyMouseButtonDown()) owner.readout.release(); }
void ModChip::mouseUp    (const MouseEvent&) { if (! isMouseOver()) owner.readout.release(); }

void ModChip::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    proc.setModulation (owner.target, lfo, wheelDepth (proc.getModulationDepth (owner.target, lfo), w, e.mods.isShiftDown()));
    owner.readout.show (owner.target, lfo, isMouseOver(), 1200);
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
    slider.setMouseDragSensitivity (slider.baseSensitivity);
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
    auto sb = (lf != nullptr ? lf->getSliderLayout (const_cast<FineSlider&> (slider)).sliderBounds : slider.getLocalBounds())
                  .translated (slider.getX(), slider.getY()).toFloat();
    if (GranulumLook::flatKnobs)
    {
        // same geometry as the flat knob drawing, so LFO rings always sit just OUTSIDE the disc, on every knob size
        const bool small = sb.getHeight() < 50.0f;
        sb = sb.reduced (small ? 1.5f : 5.0f);
        const float r = jmin (sb.getWidth(), sb.getHeight()) * 0.5f;
        const float disc = r * (small ? 0.8f : 0.78f);
        d.track = jmax (2.0f, r * 0.08f);
        d.r = disc + d.track * 1.1f;
        d.c = sb.getCentre();
    }
    else
    {
        sb = sb.reduced (5.0f);
        d.r = jmin (sb.getWidth(), sb.getHeight()) * 0.5f;
        d.c = sb.getCentre();
        d.track = jmax (2.5f, d.r * 0.09f);
    }
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
    if (inlineMode)
    {
        r.removeFromLeft (26);
        slider.setTextBoxStyle (Slider::TextBoxRight, false, 44, 15);
        slider.setBounds (r.withWidth (jmin (r.getWidth(), r.getHeight() + 50)));
        refreshMods();
        return;
    }
    if (size == Size::Mini)
    {
        // 1.0: tiny dial with its name underneath, no value box (the value pops up while turning / shows as a tooltip)
        r.removeFromBottom (8);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setBounds (r);
        refreshMods();
        return;
    }
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
    if (inlineMode) { g.drawText (title.toUpperCase(), getLocalBounds().removeFromLeft (26), Justification::centredLeft); return; }
    if (size == Size::Mini)
    {
        g.setFont (uiFont (6.2f, true, 0.1f));
        g.drawText (title.toUpperCase(), getLocalBounds().removeFromBottom (8).expanded (6, 0), Justification::centred, false);
        return;
    }
    g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (titleH), Justification::centred);
    if (shownSynced)
    {
        // SYNC BPM: a small copper dot before the name
        const float tw = (float) GlyphArrangement::getStringWidthInt (g.getCurrentFont(), title.toUpperCase());
        g.setColour (Palette::accent);
        g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre ({ getWidth() * 0.5f - tw * 0.5f - 6.0f, titleH * 0.5f }));
    }
}

void FoldButton::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool hot = isMouseOver();
    if (folded)
    {
        g.setColour (hot ? Palette::accent.brighter (0.15f) : Palette::accent);
        g.fillRoundedRectangle (r, 5.0f);
    }
    else
    {
        g.setColour (Palette::accent.withAlpha (hot ? 0.22f : 0.08f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Palette::accent.withAlpha (hot ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }
    const auto c = r.getCentre();
    const float w = r.getWidth() * 0.2f, h = r.getHeight() * 0.11f;
    Path p;
    if (folded) { p.startNewSubPath (c.x - w, c.y - h); p.lineTo (c.x, c.y + h); p.lineTo (c.x + w, c.y - h); }
    else        { p.startNewSubPath (c.x - w, c.y + h); p.lineTo (c.x, c.y - h); p.lineTo (c.x + w, c.y + h); }
    g.setColour (folded ? Colours::white : Palette::accent.brighter (0.2f));
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
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

    // LFO depth in % (like Serum): a small tag right next to the LFO chip; the knob's own value stays visible
    if (readout.active() && readout.l >= 0 && proc.hasModulation (target, readout.l))
    {
        auto& chip = *chips[(size_t) readout.l];
        const auto cb = chip.getBounds().toFloat();
        const bool right = cb.getCentreX() < d.c.x;   // grow away from the chip, toward the free side of the knob
        const float depth = proc.getModulationDepth (target, readout.l);
        const auto f = uiFont (9.0f, true, 0.02f);
        const auto text = depthText (depth);
        const float w = (float) GlyphArrangement::getStringWidthInt (f, text) + 8.0f, h = 12.0f;
        auto r = Rectangle<float> (w, h).withCentre ({ right ? cb.getRight() + w * 0.5f + 1.0f : cb.getX() - w * 0.5f - 1.0f, cb.getCentreY() });
        const auto b = getLocalBounds().toFloat();
        r = r.withX (jlimit (b.getX(), jmax (b.getX(), b.getRight() - w), r.getX()))
             .withY (jlimit (b.getY(), jmax (b.getY(), b.getBottom() - h), r.getY()));
        const auto col = Palette::lfo (readout.l);
        g.setColour (Palette::screen.withAlpha (0.95f));
        g.fillRoundedRectangle (r, h * 0.5f);
        g.setColour (col.withAlpha (0.75f));
        g.drawRoundedRectangle (r.reduced (0.5f), h * 0.5f, 1.0f);
        g.setColour (col.brighter (0.25f));
        g.setFont (f);
        g.drawText (text, r, Justification::centred, false);
    }
}

void Knob::refreshMods()
{
    if (target < 0) return;
    anyMod = false;
    const auto d = dial();
    const bool smallKnob = d.r < 22.0f;
    const float chip = smallKnob ? 11.0f : jlimit (13.0f, 17.0f, d.r * 0.42f);
    int shown = 0;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& c = chips[(size_t) l];
        const bool on = proc.hasModulation (target, l);
        anyMod |= on;
        c->setVisible (on);
        if (! on) continue;
        // chips sit outside the ring, at the upper right
        // small knobs: lower right, so the chip never covers the knob title
        const float a = MathConstants<float>::pi * (smallKnob ? 0.68f + 0.24f * (float) shown++ : 0.22f + 0.22f * (float) shown++);
        auto pos = d.c.getPointOnCircumference (d.r + chip * 0.55f, a);
        auto rect = Rectangle<float> (chip, chip).withCentre (pos).toNearestInt();
        rect = rect.constrainedWithin (getLocalBounds());
        c->setBounds (rect);
        c->updateTooltip();
        c->repaint();
    }
    repaint();
}

void Knob::syncTick()
{
    if (synced)
        if (const bool s = synced(); s != shownSynced)
        {
            shownSynced = s;
            slider.setColour (Slider::textBoxTextColourId, s ? Palette::accent : Palette::text);
            slider.lookAndFeelChanged();
            slider.updateText();
            repaint();
        }
}

void Knob::tick()
{
    const bool gone = readout.expire();
    if (anyMod || gone) repaint();
}

bool Knob::isInterestedInDragSource (const SourceDetails& s)
{
    return target >= 0 && s.description.toString().startsWith ("lfo:");
}

void Knob::itemDragEnter (const SourceDetails&) { dragHover = true;  repaint(); }
void Knob::itemDragExit  (const SourceDetails&) { dragHover = false; repaint(); }

// 1.0.1: an LFO dropped on a control swings it from where it is all the way to the top (like Serum);
// a control already at the top swings all the way down instead
static float fullSwingDepth (float norm)
{
    norm = jlimit (0.0f, 1.0f, norm);
    return norm < 0.98f ? 1.0f - norm : -norm;
}

void Knob::itemDropped (const SourceDetails& s)
{
    dragHover = false;
    const int l = s.description.toString().fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
    if (! proc.hasModulation (target, l))
        proc.setModulation (target, l, fullSwingDepth (slider.valueToProportionOfLength (slider.getValue())));
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
    setMouseCursor (MouseCursor::NormalCursor);
    setOpaque (true);
    shards.reserve (520);
    lastPaintMs = 0.0f;
    for (int i = 0; i < 46; ++i)
        motes.push_back ({ rng.nextFloat(), rng.nextFloat() * 2.0f - 1.0f,
                           (rng.nextFloat() - 0.5f) * 0.01f, (rng.nextFloat() - 0.5f) * 0.02f,
                           rng.nextFloat() * 6.28f, 0.6f + rng.nextFloat() * 1.4f });
    lastTick = Time::getMillisecondCounterHiRes();
    // (no own timer: GranulumEditor::timerCallback calls frame() at 30 fps)
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
    if (! isShowing()) return;
    // nothing flying and nothing playing: a calm 10 frames per second is enough
    const bool busy = ! shards.empty() || ! heads.empty() || dragHover || isMouseButtonDown();
    if (busy || ++idleTicks >= 5) { idleTicks = 0; repaint(); }
}

void GrainField::simulate (float dt)
{
    time += dt;
    auto& s = proc.apvts;
    const auto mv = [this] (int t) { return proc.modNorm[(size_t) t].load (std::memory_order_relaxed); };
    const float bloom = mv (mtBloom), smear = mv (mtSmear), scatter = mv (mtScatter), shimmer = mv (mtShimmer), drift = mv (mtDrift);
    const bool grains = (  (s.getRawParameterValue (ParamID::osc1On)->load() > 0.5f && s.getRawParameterValue (ParamID::osc1Grain)->load() > 0.5f)
                       || (s.getRawParameterValue (ParamID::osc2On)->load() > 0.5f && s.getRawParameterValue (ParamID::osc2Grain)->load() > 0.5f)
                       || (s.getRawParameterValue (ParamID::osc3On)->load() > 0.5f && s.getRawParameterValue (ParamID::osc3Grain)->load() > 0.5f));

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
    // 0.99.6: every source that makes grains shows them: OSC 1 (additive or wave), the SAMPLE, the NOISE
    granulum::GrainEngine* engines[4] = { &proc.engineAdd, &proc.engineWave, &proc.engineSmp, &proc.engineNoise };
    static constexpr uint8_t srcOf[4] = { 0, 0, 1, 2 };
    for (int e = 0; e < 4; ++e)
    {
        auto& en = *engines[e];
        const uint32_t w = en.eventWrite.load (std::memory_order_acquire);
        uint32_t r = lastEvent[e];
        if (w - r > (uint32_t) granulum::GrainEngine::kEventRing) r = w - (uint32_t) granulum::GrainEngine::kEventRing;
        int budget = e == 3 ? 14 : 24;
        for (; r != w && budget > 0; ++r, --budget)
        {
            const auto ev = en.events[(size_t) (r % granulum::GrainEngine::kEventRing)];
            if ((int) shards.size() >= maxShards) shards.erase (shards.begin());
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
            sh.src   = srcOf[e];
            if (sh.src == 2) { sh.len *= 0.55f; sh.life *= 0.7f; }   // noise: short, small sparks
            sh.tint  = ev.kind == 1 ? 1.0f : jlimit (0.0f, 0.85f, 0.15f + 0.5f * rng.nextFloat() + 0.25f * shimmer);
            sh.size  = (0.7f + 0.6f * rng.nextFloat() + 0.5f * bloom) * (sh.src == 2 ? 0.6f : 1.0f);
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

void GrainField::rebuildCaches()
{
    const auto r = getLocalBounds().toFloat();
    cacheTheme = Palette::theme;
    auto makeBg = [&] (float bright)
    {
        Image im (Image::RGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
        Graphics g (im);
        g.setColour (Palette::panel);
        g.fillAll();
        Path clip; clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (ColourGradient (Palette::screen.brighter (bright), r.getCentreX(), r.getCentreY(), Palette::deep, r.getX(), r.getY(), true));
        g.fillRect (r);
        return im;
    };
    bgLow = makeBg (0.10f);
    bgHigh = makeBg (0.23f);
    vignette = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
    {
        Graphics g (vignette);
        g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                           Palette::deep.withAlpha (0.55f), r.getX(), r.getY(), true));
        g.fillRect (r);
    }
    // soft glow: an elliptic gaussian blob, tinted at draw time
    glowSprite = Image (Image::SingleChannel, 64, 32, true);
    {
        Image::BitmapData bd (glowSprite, Image::BitmapData::writeOnly);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 64; ++x)
            {
                const float dx = ((float) x + 0.5f - 32.0f) / 32.0f, dy = ((float) y + 0.5f - 16.0f) / 16.0f;
                const float d2 = dx * dx + dy * dy;
                const float a = d2 >= 1.0f ? 0.0f : std::pow (1.0f - d2, 2.2f);
                *bd.getPixelPointer (x, y) = (uint8) jlimit (0, 255, (int) (a * 255.0f));
            }
    }
}

void GrainField::paint (Graphics& g)
{
    const double paintStart = Time::getMillisecondCounterHiRes();
    struct PaintTimer { GrainField& f; double t0; ~PaintTimer() { f.notePaintTime ((float) (Time::getMillisecondCounterHiRes() - t0)); } } paintTimer { *this, paintStart };
    const auto r = getLocalBounds().toFloat();
    const float breathe = 0.5f + 0.5f * std::sin (time * 0.8f);
    if (fastPaint)
    {
        if (cacheTheme != Palette::theme || bgLow.getWidth() != getWidth() || bgLow.getHeight() != getHeight()) rebuildCaches();
        // deep background with a soft breathing core: blend two cached brightness levels
        g.drawImageAt (bgLow, 0, 0);
        const float k = jlimit (0.0f, 1.0f, (0.10f * pulse + 0.03f * breathe) / 0.13f);
        if (k > 0.01f) { g.setOpacity (k); g.drawImageAt (bgHigh, 0, 0); g.setOpacity (1.0f); }
        Path clip; clip.addRoundedRectangle (r, 10.0f);
        g.reduceClipRegion (clip);
    }
    else
    {
        Path clip; clip.addRoundedRectangle (r, 10.0f);
        g.setColour (Palette::panel);
        g.fillAll();
        g.reduceClipRegion (clip);
        g.setGradientFill (ColourGradient (Palette::screen.brighter (0.10f + 0.10f * pulse + 0.03f * breathe), r.getCentreX(), r.getCentreY(),
                                           Palette::deep, r.getX(), r.getY(), true));
        g.fillRect (r);
    }

    const auto in = inner();
    const float mid = in.getCentreY();
    const float halfH = in.getHeight() * 0.5f;
    auto P = [&] (float xn, float yn) { return Point<float> (in.getX() + xn * in.getWidth(), mid - yn * halfH); };

    auto& s = proc.apvts;
    const auto mv = [this] (int t) { return proc.modNorm[(size_t) t].load (std::memory_order_relaxed); };
    const float smear = mv (mtSmear), shimmer = mv (mtShimmer), bloom = mv (mtBloom);
    const bool grainsOn = (  (s.getRawParameterValue (ParamID::osc1On)->load() > 0.5f && s.getRawParameterValue (ParamID::osc1Grain)->load() > 0.5f)
                       || (s.getRawParameterValue (ParamID::osc2On)->load() > 0.5f && s.getRawParameterValue (ParamID::osc2Grain)->load() > 0.5f)
                       || (s.getRawParameterValue (ParamID::osc3On)->load() > 0.5f && s.getRawParameterValue (ParamID::osc3Grain)->load() > 0.5f));

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
        float pos = proc.modNorm[(size_t) mtPosition].load();
        if ((int) proc.apvts.getRawParameterValue (ParamID::loopMode)->load() != 0)
        {
            // with a LOOP, POSITION is a place inside the loop
            const float a = proc.apvts.getRawParameterValue (ParamID::loopStart)->load(), b = proc.apvts.getRawParameterValue (ParamID::loopEnd)->load();
            pos = jmin (a, b) + pos * std::abs (b - a);
        }
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
        const float fade = jmin (1.0f, sh.age * 14.0f) * (1.0f - lt) * (1.0f - lt * 0.4f) * (sh.src == 2 ? 0.6f : 1.0f);
        if (fade <= 0.01f) continue;
        const auto col = shardColour (sh.tint);
        const auto p = P (sh.x, sh.y);

        // trail
        if (detail < 1 || fade < 0.12f) {}   // busy: trails are the first thing to go
        else if (sh.trailN > 2 && ! fastPaint)
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
        else if (sh.trailN > 2)
        {
            // the same tapering, fading trail as one filled ribbon with a gradient (one fill instead of 6 strokes)
            std::array<Point<float>, 7> pts;
            const int n = jmin (sh.trailN, (int) pts.size());
            for (int i = 0; i < n; ++i) pts[(size_t) i] = P (sh.trail[(size_t) i].x, sh.trail[(size_t) i].y);
            Path ribbon;
            std::array<Point<float>, 7> right;
            for (int i = 0; i < n; ++i)
            {
                const auto dir = pts[(size_t) jmin (i + 1, n - 1)] - pts[(size_t) jmax (i - 1, 0)];
                const float len = jmax (0.001f, dir.getDistanceFromOrigin());
                const Point<float> nn (-dir.y / len, dir.x / len);
                const float k = 1.0f - (float) (i + 1) / (float) sh.trailN;
                const float hw = 0.5f * jmax (0.0f, (1.0f + 3.0f * smear) * k * sh.size);
                const auto l = pts[(size_t) i] + nn * hw;
                right[(size_t) i] = pts[(size_t) i] - nn * hw;
                if (i == 0) ribbon.startNewSubPath (l); else ribbon.lineTo (l);
            }
            for (int i = n - 1; i >= 0; --i) ribbon.lineTo (right[(size_t) i]);
            ribbon.closeSubPath();
            const float a0 = fade * (0.22f + 0.25f * smear);
            g.setGradientFill (ColourGradient (col.withAlpha (a0), pts[0], col.withAlpha (0.0f), pts[(size_t) n - 1], false));
            g.fillPath (ribbon);
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
        if (detail < 2) {}                    // very busy: no halo
        else if (fastPaint)
        {
            // the same halo as a thick rounded stroke around the kite, drawn as one filled outline (much cheaper)
            const float h = Wd * (2.5f + 3.0f * smear) * 0.5f;
            Path halo;
            halo.startNewSubPath (front + d * h);
            halo.lineTo (front + d * (h * 0.4f) + n * (h * 0.9f));
            halo.lineTo (sideA + n * h + d * (h * 0.35f));
            halo.lineTo (sideA + n * h - d * (h * 0.35f));
            halo.lineTo (back + n * (h * 0.7f));
            halo.lineTo (back - d * h);
            halo.lineTo (back - n * (h * 0.7f));
            halo.lineTo (sideB - n * h - d * (h * 0.35f));
            halo.lineTo (sideB - n * h + d * (h * 0.35f));
            halo.lineTo (front + d * (h * 0.4f) - n * (h * 0.9f));
            halo.closeSubPath();
            g.fillPath (halo);
        }
        else
            g.strokePath (kite, PathStrokeType (Wd * (2.5f + 3.0f * smear), PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col.withAlpha (fade * (0.85f - 0.35f * smear)));
        g.fillPath (kite);
        g.setColour (Colours::white.withAlpha (fade * 0.45f));
        g.fillPath (facet);

        if (sh.src == 1)    // SAMPLE grains: a light outline, so they read apart from OSC 1
        {
            g.setColour (Colours::white.withAlpha (fade * 0.55f));
            g.strokePath (kite, PathStrokeType (0.9f));
        }
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

    // LOOP region: dim what is outside, two copper flags at the edges
    if (const int lm = (int) proc.apvts.getRawParameterValue (ParamID::loopMode)->load(); lm != 0)
    {
        const float a = proc.apvts.getRawParameterValue (ParamID::loopStart)->load();
        const float b = proc.apvts.getRawParameterValue (ParamID::loopEnd)->load();
        const float xa = in.getX() + jmin (a, b) * in.getWidth(), xb = in.getX() + jmax (a, b) * in.getWidth();
        g.setColour (Palette::deep.withAlpha (0.62f));
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), xa - r.getX(), r.getHeight()));
        g.fillRect (Rectangle<float> (xb, r.getY(), r.getRight() - xb, r.getHeight()));
        for (int k = 0; k < 2; ++k)
        {
            const float x = k == 0 ? xa : xb;
            g.setColour (Palette::accent.withAlpha (0.85f));
            g.fillRect (x - 0.75f, r.getY(), 1.5f, r.getHeight());
            Path flag;   // little pennant pointing into the loop
            const float dir = k == 0 ? 1.0f : -1.0f;
            flag.addTriangle (x, r.getY() + 4.0f, x, r.getY() + 16.0f, x + dir * 10.0f, r.getY() + 10.0f);
            g.fillPath (flag);
        }
        g.setColour (Palette::accent.withAlpha (0.9f));
        g.setFont (uiFont (9.0f, true, 0.12f));
        const char* tag = lm == 1 || lm == 2 ? "LOOP" : "ONE-SHOT";
        g.drawText (tag, Rectangle<float> (xa + 14.0f, r.getY() + 3.0f, jmax (10.0f, xb - xa - 20.0f), 14.0f), Justification::centredLeft, true);
    }

    // vignette
    if (fastPaint) g.drawImageAt (vignette, 0, 0);
    else
    {
        g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                                           Palette::deep.withAlpha (0.55f), r.getX(), r.getY(), true));
        g.fillRect (r);
    }

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

int GrainField::flagAt (float x) const
{
    if ((int) proc.apvts.getRawParameterValue (ParamID::loopMode)->load() == 0) return 0;
    const auto in = inner();
    const float xa = in.getX() + proc.apvts.getRawParameterValue (ParamID::loopStart)->load() * in.getWidth();
    const float xb = in.getX() + proc.apvts.getRawParameterValue (ParamID::loopEnd)->load() * in.getWidth();
    const float da = std::abs (x - xa), db = std::abs (x - xb);
    if (jmin (da, db) > 8.0f) return 0;
    return da <= db ? 1 : 2;
}

void GrainField::mouseMove (const MouseEvent& e)
{
    setMouseCursor (flagAt (e.position.x) != 0 ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
}

void GrainField::mouseDown (const MouseEvent& e)
{
    const int f = flagAt (e.position.x);
    dragParam = proc.apvts.getParameter (f == 1 ? ParamID::loopStart : f == 2 ? ParamID::loopEnd : ParamID::position);
    if (dragParam != nullptr) dragParam->beginChangeGesture();
    mouseDrag (e);
}

void GrainField::mouseDrag (const MouseEvent& e)
{
    if (dragParam == nullptr) return;
    const auto in = inner();
    dragParam->setValueNotifyingHost (jlimit (0.0f, 1.0f, (e.position.x - in.getX()) / in.getWidth()));
}

void GrainField::mouseUp (const MouseEvent&)
{
    if (dragParam != nullptr) dragParam->endChangeGesture();
    dragParam = nullptr;
}

// ============================================================================
// OSC 4 SUB sticks

SubBars::SubBars (GranulumProcessor& p) : proc (p)
{
    setTooltip ("SUB sticks, left to right: root, octave, octave + fifth, 2 octaves, 2 octaves + third.");
    setRepaintsOnMouseActivity (true);
    for (auto* id : { ParamID::subH1, ParamID::subH2, ParamID::subH3, ParamID::subH4, ParamID::subH5, ParamID::subOn }) proc.apvts.addParameterListener (id, this);
}

SubBars::~SubBars()
{
    for (auto* id : { ParamID::subH1, ParamID::subH2, ParamID::subH3, ParamID::subH4, ParamID::subH5, ParamID::subOn }) proc.apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
}

RangedAudioParameter* SubBars::param (int i) const
{
    static const char* const ids[] = { ParamID::subH1, ParamID::subH2, ParamID::subH3, ParamID::subH4, ParamID::subH5 };
    return proc.apvts.getParameter (ids[jlimit (0, kN - 1, i)]);
}

bool SubBars::anyMod() const
{
    for (int i = 0; i < kN; ++i) for (int l = 0; l < kNumLfos; ++l) if (proc.hasModulation (mtSubH1 + i, l)) return true;
    return false;
}

Rectangle<float> SubBars::chipRect (int col, int slot) const
{
    const auto pl = plot();
    const float colW = pl.getWidth() / (float) kN;
    return Rectangle<float> (10.0f, 10.0f).withCentre ({ pl.getX() + colW * ((float) col + 0.5f) - 11.0f - (float) slot * 12.0f, pl.getY() + 5.0f });
}

std::pair<int, int> SubBars::chipAt (Point<float> pos) const
{
    for (int i = 0; i < kN; ++i)
    {
        int slot = 0;
        const auto pl = plot();
        const float colW = pl.getWidth() / (float) kN, x = pl.getX() + colW * ((float) i + 0.5f);
        for (int l = 0; l < kNumLfos; ++l)
            if (proc.hasModulation (mtSubH1 + i, l))
            {
                auto yOf = [&pl] (float vv) { return pl.getBottom() - jlimit (0.0f, 1.0f, vv) * (pl.getHeight() - 6.0f); };
                const float y0 = yOf (param (i)->getValue()), y1 = yOf (param (i)->getValue() + proc.getModulationDepth (mtSubH1 + i, l));
                // the range above/below the stick head only, so dragging the stick itself still moves the stick
                if (Rectangle<float> (x - 5.0f, jmin (y0, y1) - 3.0f, 10.0f, std::abs (y1 - y0) + 6.0f).contains (pos) && std::abs (pos.y - y0) > 5.0f) return { i, l };
                ++slot;
            }
    }
    return { -1, -1 };
}

void SubBars::itemDropped (const SourceDetails& s)
{
    const int i = colAt ((float) s.localPosition.x);
    const int l = s.description.toString().fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
    dropCol = -1;
    for (int o = 0; o < kNumLfos; ++o) if (o != l && proc.hasModulation (mtSubH1 + i, o)) proc.removeModulation (mtSubH1 + i, o);   // one LFO per stick
    if (! proc.hasModulation (mtSubH1 + i, l))
        proc.setModulation (mtSubH1 + i, l, fullSwingDepth (param (i)->getValue()));
    repaint();
}

void SubBars::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int i = colAt (e.position.x);
    for (int l = 0; l < kNumLfos; ++l)
        if (proc.hasModulation (mtSubH1 + i, l))
        {
            proc.setModulation (mtSubH1 + i, l, wheelDepth (proc.getModulationDepth (mtSubH1 + i, l), w, e.mods.isShiftDown()));
            readout.show (i, l, false, 1200);
            repaint();
            return;
        }
    Component::mouseWheelMove (e, w);
}

void SubBars::mouseMove (const MouseEvent& e)
{
    const auto c = chipAt (e.position);
    if (c.first >= 0)
    {
        setTooltip ("LFO " + String (c.second + 1) + " on SUB stick " + String (c.first + 1) + ":  mouse wheel (5 % steps, Shift = 1 %) or drag = depth,  right-click = remove");
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        if (! readout.hold || readout.a != c.first) { readout.show (c.first, c.second, true); repaint(); }
    }
    else
    {
        if (readout.hold) readout.release();
        setTooltip ("SUB sticks, left to right: root, octave, octave + fifth, 2 octaves, 2 octaves + third.\nDrag them up and down. Drop an LFO on a stick to move it.");
        setMouseCursor (MouseCursor::NormalCursor);
    }
}

void SubBars::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 7.0f);
    const bool on = proc.apvts.getRawParameterValue (ParamID::subOn)->load() > 0.5f;
    const auto pl = plot();
    const float colW = pl.getWidth() / (float) kN;
    // the resulting waveform behind the sticks (like OSC 1): root, octave, octave + fifth (= 3rd harmonic)
    {
        float lv[kN];
        for (int i = 0; i < kN; ++i) lv[i] = anyMod() ? proc.modNorm[(size_t) (mtSubH1 + i)].load (std::memory_order_relaxed) : param (i)->getValue();
        Path wave;
        const int pts = 120;
        float peak = 0.0001f;
        std::vector<float> ys ((size_t) pts);
        for (int n = 0; n < pts; ++n)
        {
            const float ph = MathConstants<float>::twoPi * 2.0f * (float) n / (float) (pts - 1);   // two cycles
            float y = 0.0f;
            for (int k = 0; k < kN; ++k) y += lv[k] * std::sin ((float) (k + 1) * ph);
            ys[(size_t) n] = y;
            peak = jmax (peak, std::abs (ys[(size_t) n]));
        }
        for (int n = 0; n < pts; ++n)
        {
            const float x = pl.getX() + pl.getWidth() * (float) n / (float) (pts - 1);
            const float y = pl.getCentreY() - ys[(size_t) n] / peak * pl.getHeight() * 0.38f;
            if (n == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        g.setColour (Palette::textDim.withAlpha (on ? 0.38f : 0.18f));
        g.strokePath (wave, PathStrokeType (1.2f));
    }
    if (dropCol >= 0)
    {
        g.setColour (Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (Rectangle<float> (pl.getX() + colW * (float) dropCol, pl.getY(), colW, pl.getHeight()), 3.0f);
    }
    for (int i = 0; i < kN; ++i)
    {
        // 0.99.5: a stick with an LFO moves with it (the LFO range is still shown next to it)
        bool modded = false;
        for (int l = 0; l < kNumLfos; ++l) modded = modded || proc.hasModulation (mtSubH1 + i, l);
        const float v = modded ? jlimit (0.0f, 1.0f, proc.modNorm[(size_t) (mtSubH1 + i)].load (std::memory_order_relaxed)) : param (i)->getValue();
        const float x = pl.getX() + colW * ((float) i + 0.5f);
        const float yTop = pl.getBottom() - v * (pl.getHeight() - 6.0f);
        // faint full-height guide
        g.setColour (Palette::textDim.withAlpha (0.18f));
        g.drawLine (x, pl.getY() + 3.0f, x, pl.getBottom(), 1.0f);
        // the string
        const bool hot = dragging == i || (isMouseOver() && (int) ((getMouseXYRelative().x - pl.getX()) / colW) == i);
        const auto col = (on ? Palette::accent : Palette::textDim).withAlpha (on ? 1.0f : 0.6f);
        g.setColour (col.withAlpha (0.25f));
        g.drawLine (x, yTop, x, pl.getBottom(), 6.0f);
        g.setColour (hot ? col.brighter (0.3f) : col);
        g.drawLine (x, yTop, x, pl.getBottom(), 2.0f);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ x, yTop }));

        // LFO on this stick: range line, live level, chips
        int slot = 0;
        for (int l = 0; l < kNumLfos; ++l)
        {
            if (! proc.hasModulation (mtSubH1 + i, l)) continue;
            const auto col = Palette::lfo (l).withMultipliedAlpha (proc.isLfoOn (l) ? 1.0f : 0.35f);
            const float base = param (i)->getValue(), depth = proc.getModulationDepth (mtSubH1 + i, l);
            auto yOf = [&pl] (float vv) { return pl.getBottom() - jlimit (0.0f, 1.0f, vv) * (pl.getHeight() - 6.0f); };
            // same look as OSC 1 (style B): a ghost of the stick in the LFO colour over its range + a glow at the live level
            const float y0 = yOf (base), y1 = yOf (base + depth);
            g.setColour (col.withAlpha (0.22f));
            g.fillRoundedRectangle (Rectangle<float> (x - 3.0f, jmin (y0, y1), 6.0f, std::abs (y1 - y0)), 2.5f);
            g.setColour (col.withAlpha (0.55f));
            g.fillRoundedRectangle (Rectangle<float> (x - 3.0f, y1 - 0.75f, 6.0f, 1.5f), 0.7f);
            const float live = proc.modNorm[(size_t) (mtSubH1 + i)].load (std::memory_order_relaxed);
            g.setColour (col.withAlpha (0.35f));
            g.fillEllipse (Rectangle<float> (12.0f, 6.0f).withCentre ({ x, yOf (live) }));
            g.setColour (col);
            g.fillRoundedRectangle (Rectangle<float> (10.0f, 1.8f).withCentre ({ x, yOf (live) }), 0.9f);
            ++slot;
        }
    }
}

void SubBars::setFrom (Point<float> pos)
{
    const auto pl = plot();
    if (dragging < 0) return;
    const float v = jlimit (0.0f, 1.0f, (pl.getBottom() - pos.y) / (pl.getHeight() - 6.0f));
    param (dragging)->setValueNotifyingHost (v);
}

void SubBars::mouseDown (const MouseEvent& e)
{
    if (const auto c = chipAt (e.position); c.first >= 0)
    {
        if (e.mods.isPopupMenu()) { proc.removeModulation (mtSubH1 + c.first, c.second); repaint(); return; }
        chipDrag = c;
        chipStartDepth = proc.getModulationDepth (mtSubH1 + c.first, c.second);
        chipStartY = e.position.y;
        return;
    }
    if (e.mods.isPopupMenu())
    {
        // 1.0.5: right-click anywhere on a stick that has an LFO takes the LFO off (no need to hit the small mark)
        const int col = colAt (e.position.x);
        bool had = false;
        for (int l = 0; l < kNumLfos; ++l) if (proc.hasModulation (mtSubH1 + col, l)) { proc.removeModulation (mtSubH1 + col, l); had = true; }
        if (had) { repaint(); return; }
    }
    const auto pl = plot();
    dragging = jlimit (0, kN - 1, (int) ((e.position.x - pl.getX()) / (pl.getWidth() / (float) kN)));
    param (dragging)->beginChangeGesture();
    setFrom (e.position);
}

void SubBars::mouseDrag (const MouseEvent& e)
{
    if (chipDrag.first >= 0)
    {
        const float d = snapDepth (chipStartDepth + (chipStartY - e.position.y) / jmax (20.0f, plot().getHeight()) * (e.mods.isShiftDown() ? 0.25f : 1.0f));
        proc.setModulation (mtSubH1 + chipDrag.first, chipDrag.second, d);
        readout.show (chipDrag.first, chipDrag.second, true);
        repaint();
        return;
    }
    setFrom (e.position);
}

void SubBars::paintOverChildren (Graphics& g)
{
    if (! readout.active() || ! proc.hasModulation (mtSubH1 + readout.a, readout.l)) return;
    const auto pl = plot();
    const float colW = pl.getWidth() / (float) kN, x = pl.getX() + colW * ((float) readout.a + 0.5f);
    auto yOf = [&pl] (float vv) { return pl.getBottom() - jlimit (0.0f, 1.0f, vv) * (pl.getHeight() - 6.0f); };
    const float depth = proc.getModulationDepth (mtSubH1 + readout.a, readout.l);
    const float y0 = yOf (param (readout.a)->getValue()), y1 = yOf (param (readout.a)->getValue() + depth);
    drawDepthTag (g, depth, Palette::lfo (readout.l), { x, jmin (y0, y1) - 10.0f }, getLocalBounds().toFloat().reduced (2.0f));
}

void SubBars::mouseUp (const MouseEvent&)
{
    if (chipDrag.first >= 0) readout.release();
    chipDrag = { -1, -1 };
    if (dragging >= 0) param (dragging)->endChangeGesture();
    dragging = -1;
    repaint();
}

// ============================================================================
// OSC 1 waveforms

static int  osc1ModeIndex (GranulumProcessor& p) { return juce::jlimit (0, 1, (int) p.apvts.getRawParameterValue (ParamID::osc1Mode)->load()); }   // 0 ADD 1 WAVE
static bool osc1WaveMode (GranulumProcessor& p) { return osc1ModeIndex (p) == 1; }      // the sticks are hidden
static bool osc1UsesWave (GranulumProcessor& p) { return osc1ModeIndex (p) == 1; }      // the wave buttons pick the full wave
static int  osc1WaveIndex (GranulumProcessor& p) { return juce::jlimit (0, 3, (int) p.apvts.getRawParameterValue (ParamID::osc1Wave)->load()); }
// parts of the MIX WAVE pill: [MIX WAVE] [track] [ 28%]  (the wave itself is picked by the buttons of WAVE mode)
static Rectangle<float> pillTrack (Rectangle<float> r, int k = 0) { return r.withTrimmedLeft (k == 0 ? 70.0f : 36.0f).withTrimmedRight (38.0f); }
static const char* pillParam (int k)  { return k == 0 ? ParamID::osc1WaveMix : ParamID::osc1Air; }
static int         pillTarget (int k) { return k == 0 ? mtOsc1WaveMix : mtOsc1Air; }

std::array<float, granulum::kNumHarmonics> WavePicker::shape (int i)
{
    std::array<float, granulum::kNumHarmonics> h {};
    for (int k = 1; k <= granulum::kNumHarmonics; ++k)
    {
        float a = 0.0f;
        switch (i)
        {
            case 0:  a = k == 1 ? 1.0f : 0.0f; break;                         // SINE
            case 1:  a = 1.0f / (float) k; break;                              // SAW: all harmonics, 1/n
            case 2:  a = (k % 2 == 1) ? 1.0f / (float) k : 0.0f; break;        // SQUARE: odd, 1/n
            default: a = (k % 2 == 1) ? 1.0f / (float) (k * k) : 0.0f; break; // TRIANGLE: odd, 1/n^2
        }
        h[(size_t) k - 1] = a;
    }
    return h;
}

int WavePicker::matching() const
{
    if (osc1UsesWave (proc)) return osc1WaveIndex (proc);   // MIX / WAVE: the chosen full waveform
    const auto now = proc.getHarmonics();
    for (int i = 0; i < 4; ++i)
    {
        const auto ref = shape (i);
        bool same = true;
        for (size_t k = 0; k < ref.size() && same; ++k) same = std::abs (ref[k] - now[k]) < 0.002f;
        if (same) return i;
    }
    return -1;
}

void WavePicker::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    const int sel = matching();
    const float w = r.getWidth() / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        auto seg = Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        const bool over = isMouseOver() && segmentAt ((float) getMouseXYRelative().x) == i;
        if (i == sel)
        {
            g.setColour (Palette::buttonHi);
            g.fillRoundedRectangle (seg.reduced (2.0f), 4.5f);
        }
        // the waveform icon
        auto ic = seg.reduced (seg.getWidth() * 0.22f, seg.getHeight() * 0.30f);
        Path p;
        const float x0 = ic.getX(), x1 = ic.getRight(), y0 = ic.getY(), y1 = ic.getBottom(), ym = ic.getCentreY(), xm = ic.getCentreX();
        switch (i)
        {
            case 0:
                p.startNewSubPath (x0, ym);
                for (int k = 1; k <= 16; ++k)
                {
                    const float t = (float) k / 16.0f;
                    p.lineTo (x0 + (x1 - x0) * t, ym - (ym - y0) * std::sin (t * MathConstants<float>::twoPi));
                }
                break;
            case 1: p.startNewSubPath (x0, ym); p.lineTo (xm, y0); p.lineTo (xm, y1); p.lineTo (x1, ym); break;
            case 2: p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (xm, y0); p.lineTo (xm, y1); p.lineTo (x1, y1); p.lineTo (x1, y0); break;
            default: p.startNewSubPath (x0, ym); p.lineTo (x0 + (x1 - x0) * 0.25f, y0); p.lineTo (x0 + (x1 - x0) * 0.75f, y1); p.lineTo (x1, ym); break;
        }
        g.setColour (i == sel ? Palette::accent.brighter (0.2f) : over ? Palette::hi : Palette::textDim);
        g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

void WavePicker::mouseDown (const MouseEvent& e)
{
    if (osc1UsesWave (proc))
    {
        if (auto* w = proc.apvts.getParameter (ParamID::osc1Wave)) w->setValueNotifyingHost (w->convertTo0to1 ((float) segmentAt (e.position.x)));
    }
    else
        proc.setHarmonics (shape (segmentAt (e.position.x)));
    repaint();
}

// ============================================================================
// OSC 1 mode: ADDITIVE (sticks) or WAVE (one full waveform, all harmonics). 0.99.5

void Osc1ModeSwitch::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    const int mode = osc1ModeIndex (proc);
    const char* names[2] = { "ADDITIVE", "WAVE" };
    const float w = r.getWidth() / 2.0f;
    for (int i = 0; i < 2; ++i)
    {
        auto seg = Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
        const bool on = i == mode;
        if (on)
        {
            g.setColour (Palette::accent);
            g.fillRoundedRectangle (seg, 4.5f);
        }
        else if (isMouseOver() && seg.contains (getMouseXYRelative().toFloat()))
        {
            g.setColour (Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (seg, 4.5f);
        }
        g.setColour (on ? Colours::white : Palette::textDim);
        g.setFont (uiFont (9.5f, true, 0.1f));
        g.drawText (names[i], seg, Justification::centred, false);
    }
}

void Osc1ModeSwitch::mouseDown (const MouseEvent& e)
{
    if (auto* m = proc.apvts.getParameter (ParamID::osc1Mode))
    {
        m->beginChangeGesture();
        m->setValueNotifyingHost (e.position.x > (float) getWidth() * 0.5f ? 1.0f : 0.0f);
        m->endChangeGesture();
    }
    repaint();
    if (onChange) onChange();
}

// ============================================================================
// TEST: FREEZE / SHIMMER intervals switches

int GrainSwitch::value() const { return (int) std::round (proc.apvts.getRawParameterValue (id)->load()); }

void GrainSwitch::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int v = value();
    if (labels.size() == 1)
    {
        const bool on = v > 0;
        g.setColour (on ? Palette::accent : Palette::field);
        g.fillRoundedRectangle (r.reduced (1.0f), 6.0f);
        g.setColour (on ? Palette::onAccent() : Palette::textDim);
        g.setFont (uiFont (9.5f, true, 0.12f));
        g.drawText (labels[0], r, Justification::centred, false);
        return;
    }
    if (prefix.isNotEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (uiFont (8.5f, true, 0.1f));
        g.drawText (prefix, r.removeFromLeft (56.0f), Justification::centredRight, false);
        r.removeFromLeft (5.0f);
    }
    g.setColour (Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    const float w = r.getWidth() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto seg = Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
        if (i == v)
        {
            g.setColour (i == 0 ? Palette::buttonHi.withAlpha (0.6f) : Palette::accent);
            g.fillRoundedRectangle (seg.reduced (2.0f), 5.0f);
        }
        g.setColour (i == v ? (i == 0 ? Palette::text : Palette::onAccent()) : Palette::textDim);
        g.setFont (uiFont (9.0f, i == v, 0.1f));
        g.drawText (labels[i], seg, Justification::centred, false);
    }
}

void GrainSwitch::mouseDown (const MouseEvent& e)
{
    auto* p = proc.apvts.getParameter (id);
    if (p == nullptr) return;
    float x = e.position.x;
    int idx = 0;
    if (labels.size() == 1) idx = value() > 0 ? 0 : 1;
    else
    {
        const float left = prefix.isNotEmpty() ? 61.0f : 0.0f;
        const float w = ((float) getWidth() - left) / (float) labels.size();
        if (x < left) return;
        idx = jlimit (0, labels.size() - 1, (int) ((x - left) / w));
    }
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
    p->endChangeGesture();
    repaint();
}

// ============================================================================
// LOOP mode selector

int LoopModeBar::mode() const { return (int) proc.apvts.getRawParameterValue (ParamID::loopMode)->load(); }

void LoopModeBar::paint (Graphics& g)
{
    // 1.0.5: PING-PONG removed (loads as LOOP); segments map to the parameter's indexes 0, 1, 3
    static const char* names[] = { "OFF", "LOOP", "ONE-SHOT" };
    static const int idxOf[] = { 0, 1, 3 };
    static const float widths[] = { 0.25f, 0.32f, 0.43f };
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::field);
    g.fillRoundedRectangle (r, 7.0f);
    const int m = mode() == 2 ? 1 : mode() == 3 ? 2 : mode();
    ignoreUnused (idxOf);
    float x = r.getX();
    for (int i = 0; i < 3; ++i)
    {
        auto seg = Rectangle<float> (x, r.getY(), r.getWidth() * widths[i], r.getHeight());
        x += seg.getWidth();
        if (i == m)
        {
            g.setColour (i == 0 ? Palette::buttonHi.withAlpha (0.6f) : Palette::accent);
            g.fillRoundedRectangle (seg.reduced (2.0f), 5.5f);
        }
        g.setColour (i == m ? (i == 0 ? Palette::text : Palette::onAccent()) : Palette::textDim);
        g.setFont (uiFont (9.5f, i == m, 0.12f));
        g.drawText (names[i], seg, Justification::centred, false);
    }
}

void LoopModeBar::mouseDown (const MouseEvent& e)
{
    static const float widths[] = { 0.25f, 0.32f, 0.43f };
    static const int idxOf[] = { 0, 1, 3 };
    float x = 0.0f; int idx = 3;
    for (int i = 0; i < 3; ++i) { x += widths[i] * (float) getWidth(); if (e.position.x < x) { idx = idxOf[i]; break; } }
    if (auto* p = proc.apvts.getParameter (ParamID::loopMode))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        p->endChangeGesture();
    }
    repaint();
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
    // (no own timer: GranulumEditor::timerCallback calls frame() at 30 fps)
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
    bool changed = false;
    if (! editing)
    {
        const auto now = proc.getHarmonics();
        if (now != h) { h = now; changed = true; }
        if (lastPill < 0) { const auto m = proc.getGrainMask(); changed |= m != mask; mask = m; }
    }
    // repaint only while something moves: granulated bars shimmer, LFOs on sticks, the hint, the intro wave, mouse, edits
    bool lfoOnSticks = false;
    for (int k = 0; k < granulum::kNumHarmonics && ! lfoOnSticks; ++k)
        for (int l = 0; l < kNumLfos; ++l) if (proc.hasModulation (kHarmTarget0 + k, l)) { lfoOnSticks = true; break; }
    const bool animating = (grainsActive() && mask != 0) || ! crystals.empty() || lfoOnSticks || showHint || introT < 2.0f
                        || editing || isMouseOverOrDragging() || lfoDropBar >= 0 || pillDrop >= 0
                        || proc.hasModulation (mtOsc1WaveMix, 0) || proc.hasModulation (mtOsc1WaveMix, 1) || proc.hasModulation (mtOsc1WaveMix, 2)
                        || proc.hasModulation (mtOsc1Air, 0) || proc.hasModulation (mtOsc1Air, 1) || proc.hasModulation (mtOsc1Air, 2);
    if (animating || changed || ++idleTicks > 15) { idleTicks = 0; repaint(); }
}

bool HarmonicEditor::grainsActive() const
{
    auto& s = proc.apvts;
    return s.getRawParameterValue (ParamID::osc1Grain)->load() > 0.5f
        && s.getRawParameterValue (ParamID::osc1On)->load() > 0.5f;
}

static constexpr float kGrainLabelW = 46.0f;

Rectangle<float> HarmonicEditor::barArea() const
{
    auto r = getLocalBounds().toFloat().reduced (7.0f, 6.0f);
    r.removeFromLeft (kGrainLabelW - 7.0f);
    r.removeFromBottom (rowStyle == 16 ? 28.0f : rowStyle >= 10 ? 26.0f : 20.0f);
    if (numStyle == 1 || numStyle == 3) r.removeFromBottom (9.0f);   // room for the harmonic numbers
    return r;
}

Rectangle<float> HarmonicEditor::pillRow() const
{
    auto a = barArea();
    const float numH = numStyle == 1 || numStyle == 3 ? 9.0f : 0.0f;
    return { a.getX(), a.getBottom() + 5.0f + numH, a.getWidth(), rowStyle == 16 ? 21.0f : rowStyle >= 10 ? 19.0f : 13.0f };
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

// ---- LFO on a stick ----
bool HarmonicEditor::isInterestedInDragSource (const SourceDetails& s) { return s.description.toString().startsWith ("lfo:"); }
void HarmonicEditor::itemDragMove (const SourceDetails& s)
{
    pillDrop = osc1WaveMode (proc) ? -1 : pillAt (s.localPosition.toFloat());
    lfoDropBar = pillDrop >= 0 ? -1 : barAt ((float) s.localPosition.x);
    repaint();
}
void HarmonicEditor::itemDragExit (const SourceDetails&) { lfoDropBar = -1; pillDrop = -1; repaint(); }
void HarmonicEditor::itemDropped (const SourceDetails& s)
{
    if (const int pk = osc1WaveMode (proc) ? -1 : pillAt (s.localPosition.toFloat()); pk >= 0)
    {
        // TEST: an LFO on MIX WAVE / AIR (one per pill); wheel = depth, right click = remove
        const int l = s.description.toString().fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
        const int t = pillTarget (pk);
        for (int o = 0; o < kNumLfos; ++o) if (o != l && proc.hasModulation (t, o)) proc.removeModulation (t, o);
        if (! proc.hasModulation (t, l))
        {
            const float v = proc.apvts.getParameter (pillParam (pk))->getValue();
            proc.setModulation (t, l, fullSwingDepth (v));
        }
        pillDrop = -1; lfoDropBar = -1;
        repaint();
        return;
    }
    pillDrop = -1;
    const int k = barAt ((float) s.localPosition.x);
    const int l = s.description.toString().fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
    lfoDropBar = -1;
    // one LFO per stick: a new one replaces the old one
    for (int o = 0; o < kNumLfos; ++o) if (o != l && proc.hasModulation (kHarmTarget0 + k, o)) proc.removeModulation (kHarmTarget0 + k, o);
    if (! proc.hasModulation (kHarmTarget0 + k, l))
        proc.setModulation (kHarmTarget0 + k, l, fullSwingDepth (h[(size_t) k]));   // up to the top (a full stick: down to 0)
    repaint();
}

float HarmonicEditor::lfoLineX (int harm, int slot) const
{
    const auto a = barArea();
    const float cw = a.getWidth() / (float) granulum::kNumHarmonics;
    return a.getX() + cw * ((float) harm + 0.5f) - jmin (5.0f, cw * 0.32f) - (float) slot * 2.5f;
}

// the LFO line next to a stick: hovering it explains, dragging it up/down sets depth, right-click removes
HarmonicEditor::ChipHit HarmonicEditor::chipAt (Point<float> pos) const
{
    const auto a = barArea();
    auto yOf = [&a] (float v) { return a.getBottom() - jlimit (0.0f, 1.0f, v) * a.getHeight(); };
    for (int k = 0; k < granulum::kNumHarmonics; ++k)
    {
        int slot = 0;
        for (int l = 0; l < kNumLfos; ++l)
            if (proc.hasModulation (kHarmTarget0 + k, l))
            {
                const float y0 = yOf (h[(size_t) k]), y1 = yOf (h[(size_t) k] + proc.getModulationDepth (kHarmTarget0 + k, l));
                const float cx = a.getX() + a.getWidth() / (float) granulum::kNumHarmonics * ((float) k + 0.5f);
                const float hx = lfoStickStyle == 1 || lfoStickStyle == 2 ? cx : lfoLineX (k, slot);
                const auto r = Rectangle<float> (hx - 4.0f, jmin (y0, y1) - 3.0f, 8.0f, std::abs (y1 - y0) + 6.0f);
                if (r.contains (pos)) return { k, l };
                ++slot;
            }
    }
    return {};
}

// mouse wheel over a stick that has an LFO: deeper / shallower
void HarmonicEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (const int pk = osc1WaveMode (proc) ? -1 : pillAt (e.position); pk >= 0)
    {
        for (int l = 0; l < kNumLfos; ++l)
            if (proc.hasModulation (pillTarget (pk), l))
            {
                proc.setModulation (pillTarget (pk), l, wheelDepth (proc.getModulationDepth (pillTarget (pk), l), w, e.mods.isShiftDown()));
                repaint();
                return;
            }
        return;
    }
    if (e.position.y >= pillRow().getY() - 3.0f) return;
    const int k = barAt (e.position.x);
    for (int l = 0; l < kNumLfos; ++l)
    {
        if (! proc.hasModulation (kHarmTarget0 + k, l)) continue;
        proc.setModulation (kHarmTarget0 + k, l, wheelDepth (proc.getModulationDepth (kHarmTarget0 + k, l), w, e.mods.isShiftDown()));
        readout.show (k, l, false, 1200);
        repaint();
        return;
    }
    Component::mouseWheelMove (e, w);
}

void HarmonicEditor::mouseMove (const MouseEvent& e)
{
    if (const int pk = osc1WaveMode (proc) ? -1 : pillAt (e.position); pk >= 0)
    {
        hoverBar = -1;
        if (const int l = pillLfo (pk); l >= 0 && pillChip (pk).contains (e.position))
        {
            setTooltip ("LFO " + String (l + 1) + " on " + (pk == 0 ? "MIX WAVE" : "AIR") + ":  drag up/down or mouse wheel = depth (Shift = fine),  right-click = remove");
            setMouseCursor (MouseCursor::UpDownResizeCursor);
        }
        else
        {
            setTooltip (String (pk == 0 ? "MIX WAVE: how much full wave rides on the sticks." : "AIR: bright harmonics above the 16 sticks.")
                        + "\nDrag = amount, double-click = 0. Drop an LFO here; its dot on the right sets the depth.");
            setMouseCursor (MouseCursor::NormalCursor);
        }
        repaint();
        return;
    }
    if (const auto c = chipAt (e.position); c.harm >= 0)
    {
        hoverBar = -1;
        setTooltip ("LFO " + String (c.lfo + 1) + " on harmonic " + String (c.harm + 1) + ":  mouse wheel (5 % steps, Shift = 1 %) or drag = depth,  right-click = remove");
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        if (! readout.hold || readout.a != c.harm) { readout.show (c.harm, c.lfo, true); repaint(); }
        return;
    }
    if (readout.hold) readout.release();
    if (e.position.y >= pillRow().getY() - 3.0f && e.position.x >= barArea().getX())
    {
        const int b = barAt (e.position.x);
        const bool on = (mask >> b) & 1u;
        hoverBar = b;
        setTooltip ("Harmonic " + String (b + 1) + (on ? ":  GRAIN ON.  Click to keep it clean & mono."
                                                       : ":  CLEAN & MONO.  Click to send it into the grains."));
        setMouseCursor (MouseCursor::NormalCursor);
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
    if (osc1WaveMode (proc)) return;   // WAVE mode: nothing to draw here
    if (const int pk = pillAt (e.position); pk >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            // right click: take the LFO off this pill
            for (int l = 0; l < kNumLfos; ++l) if (proc.hasModulation (pillTarget (pk), l)) proc.removeModulation (pillTarget (pk), l);
            repaint();
            return;
        }
        if (pillLfo (pk) >= 0 && pillChip (pk).contains (e.position))
        {
            pillChipDrag = pk;
            pillChipStart = proc.getModulationDepth (pillTarget (pk), pillLfo (pk));
            pillChipY = e.position.y;
            repaint();
            return;
        }
        if (e.getNumberOfClicks() > 1)
        {
            // double click: back to 0 % (the default)
            if (auto* m = proc.apvts.getParameter (pillParam (pk)))
            { m->beginChangeGesture(); m->setValueNotifyingHost (m->getDefaultValue()); m->endChangeGesture(); }
            repaint();
            return;
        }
        pillDrag = pk;
        if (auto* m = proc.apvts.getParameter (pillParam (pk))) m->beginChangeGesture();
        mouseDrag (e);
        return;
    }

    if (const auto c = chipAt (e.position); c.harm >= 0)
    {
        if (e.mods.isPopupMenu()) { proc.removeModulation (kHarmTarget0 + c.harm, c.lfo); repaint(); return; }
        chipDrag = c;
        chipStartDepth = proc.getModulationDepth (kHarmTarget0 + c.harm, c.lfo);
        chipStartY = e.position.y;
        return;
    }
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
    if (pillChipDrag >= 0)
    {
        if (const int l = pillLfo (pillChipDrag); l >= 0)
            proc.setModulation (pillTarget (pillChipDrag), l,
                                snapDepth (pillChipStart + (pillChipY - e.position.y) / 90.0f * (e.mods.isShiftDown() ? 0.25f : 1.0f)));
        repaint();
        return;
    }
    if (pillDrag >= 0)
    {
        const auto r = pillTrack (pillRect (pillDrag), pillDrag);
        if (auto* m = proc.apvts.getParameter (pillParam (pillDrag)))
            m->setValueNotifyingHost (jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / jmax (1.0f, r.getWidth())));
        repaint();
        return;
    }
    if (chipDrag.harm >= 0)
    {
        const float d = snapDepth (chipStartDepth + (chipStartY - e.position.y) / jmax (20.0f, barArea().getHeight()) * (e.mods.isShiftDown() ? 0.25f : 1.0f));
        proc.setModulation (kHarmTarget0 + chipDrag.harm, chipDrag.lfo, d);
        readout.show (chipDrag.harm, chipDrag.lfo, true);
        repaint();
        return;
    }
    if (! editing) return;
    if (pillMode) applyPill (barAt (e.position.x));
    else          applyMouse (e, false);
}

void HarmonicEditor::mouseExit (const MouseEvent&) { hoverBar = -1; if (readout.hold && chipDrag.harm < 0) readout.release(); }

void HarmonicEditor::mouseUp (const MouseEvent&) { if (pillChipDrag >= 0) { pillChipDrag = -1; repaint(); } if (pillDrag >= 0) { if (auto* m = proc.apvts.getParameter (pillParam (pillDrag))) m->endChangeGesture(); pillDrag = -1; } if (chipDrag.harm >= 0) readout.release(); chipDrag = {}; editing = false; lastBar = -1; lastPill = -1; pillMode = false; }

Rectangle<float> HarmonicEditor::pillRect (int k) const
{
    auto top = getLocalBounds().toFloat().removeFromTop (22.0f);
    auto mix = top.removeFromRight (176.0f);
    if (k == 0) return mix.reduced (6.0f, 4.0f);
    return top.removeFromRight (140.0f).reduced (2.0f, 4.0f).withTrimmedRight (-4.0f);
}

int HarmonicEditor::pillLfo (int k) const
{
    for (int l = 0; l < kNumLfos; ++l) if (proc.hasModulation (pillTarget (k), l)) return l;
    return -1;
}

int HarmonicEditor::pillAt (Point<float> p) const
{
    for (int k = 0; k < 2; ++k) if (pillRect (k).expanded (2.0f).contains (p)) return k;
    return -1;
}


void HarmonicEditor::paintOverChildren (Graphics& g)
{
    if (osc1WaveMode (proc)) return;
    for (int k = 0; k < 2; ++k)
    {
        // ADDITIVE: MIX WAVE = how much full wave rides on the sticks; AIR (TEST) = harmonics above the 16 sticks.
        // An LFO on a pill shows as a band in its colour over the track (its reach) and a live dot.
        const auto r = pillRect (k);
        const float v = proc.apvts.getParameter (pillParam (k))->getValue();
        const float live = proc.modNorm[(size_t) pillTarget (k)].load (std::memory_order_relaxed);
        int lfo = -1;
        for (int l = 0; l < kNumLfos; ++l) if (proc.hasModulation (pillTarget (k), l)) { lfo = l; break; }
        const bool on = v > 0.001f || lfo >= 0;
        g.setColour (Palette::screen.withAlpha (0.92f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (pillDrop == k ? Palette::text : Palette::accent.withAlpha (on ? 0.7f : 0.3f));
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, pillDrop == k ? 1.6f : 1.0f);
        g.setFont (uiFont (8.5f, true, 0.08f));
        g.setColour (on ? Palette::text : Palette::textDim);
        g.drawText (k == 0 ? "MIX WAVE" : "AIR", r.withTrimmedLeft (9.0f).withWidth (k == 0 ? 56.0f : 26.0f), Justification::centredLeft, false);
        auto tr = pillTrack (r, k).withSizeKeepingCentre (pillTrack (r, k).getWidth(), 3.0f);
        g.setColour (Palette::field);
        g.fillRoundedRectangle (tr, 1.5f);
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (tr.withWidth (tr.getWidth() * v), 1.5f);
        if (lfo >= 0)
        {
            const auto col = Palette::lfo (lfo).withMultipliedAlpha (proc.isLfoOn (lfo) ? 1.0f : 0.4f);
            const float d = proc.getModulationDepth (pillTarget (k), lfo);
            const float a0 = jlimit (0.0f, 1.0f, jmin (v, v + d)), a1 = jlimit (0.0f, 1.0f, jmax (v, v + d));
            g.setColour (col.withAlpha (0.55f));
            g.fillRoundedRectangle (Rectangle<float> (tr.getX() + tr.getWidth() * a0, tr.getCentreY() - 3.0f, jmax (2.0f, tr.getWidth() * (a1 - a0)), 6.0f), 2.0f);
            g.setColour (col);
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ tr.getX() + tr.getWidth() * live, tr.getCentreY() }));
        }
        g.setColour (Palette::accent);
        g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre ({ tr.getX() + tr.getWidth() * v, tr.getCentreY() }));
        if (lfo >= 0)
        {
            // LFO chip: a dot in the LFO's colour with its number, then the depth (drag it up/down)
            const auto col = Palette::lfo (lfo).withMultipliedAlpha (proc.isLfoOn (lfo) ? 1.0f : 0.4f);
            const auto chip = pillChip (k);
            const bool hot = pillChipDrag == k || (isMouseOver() && chip.contains (getMouseXYRelative().toFloat()));
            const auto dot = Rectangle<float> (12.0f, 12.0f).withCentre ({ chip.getX() + 8.0f, chip.getCentreY() });
            if (hot) { g.setColour (col.withAlpha (0.25f)); g.fillEllipse (dot.expanded (2.5f)); }
            g.setColour (col);
            g.fillEllipse (dot);
            g.setColour (Palette::screen);
            g.setFont (uiFont (8.0f, true, 0.0f));
            g.drawText (String (lfo + 1), dot.translated (0.0f, 0.5f), Justification::centred, false);
            const int dpt = roundToInt (proc.getModulationDepth (pillTarget (k), lfo) * 100.0f);
            g.setColour (hot ? Palette::text : col);
            g.setFont (uiFont (8.5f, true, 0.0f));
            g.drawText ((dpt > 0 ? "+" : "") + String (dpt), chip.withTrimmedLeft (15.0f).withTrimmedRight (4.0f), Justification::centredRight, false);
        }
        else
        {
            g.setColour (on ? Palette::text : Palette::textDim);
            g.drawText (String (roundToInt (v * 100.0f)) + "%", r.withTrimmedRight (8.0f).removeFromRight (30.0f), Justification::centredRight, false);
        }
    }

    // harmonic numbers (0.99.2): quiet, the stick under the mouse lights up
    if (numStyle > 0)
    {
        const auto a = barArea();
        const float colW = a.getWidth() / (float) granulum::kNumHarmonics;
        const int hot = isMouseOver() ? barAt ((float) getMouseXYRelative().x) : -1;
        g.setFont (uiFont (8.0f, false, 0.0f));
        for (int k = 0; k < granulum::kNumHarmonics; ++k)
        {
            const bool isHot = k == hot;
            if (numStyle == 3 && ! isHot && ! (k == 0 || (k + 1) % 4 == 0)) continue;
            const float cx = a.getX() + colW * ((float) k + 0.5f);
            Rectangle<float> r;
            if (numStyle == 2) r = Rectangle<float> (colW, 10.0f).withCentre ({ cx, a.getBottom() - jlimit (0.0f, 1.0f, h[(size_t) k]) * a.getHeight() - 8.0f });
            else               r = Rectangle<float> (colW, 9.0f).withCentre ({ cx, a.getBottom() + 7.0f });
            if (numStyle == 2 && r.getY() < a.getY() - 6.0f) r = r.withY (a.getY() - 6.0f);
            g.setColour (isHot ? Palette::text : Palette::textDim.withAlpha (0.45f));
            g.drawText (String (k + 1), r, Justification::centred, false);
        }
    }
    if (! readout.active() || ! proc.hasModulation (kHarmTarget0 + readout.a, readout.l)) return;
    const auto a = barArea();
    auto yOf = [&a] (float v) { return a.getBottom() - jlimit (0.0f, 1.0f, v) * a.getHeight(); };
    const int k = readout.a;
    const float depth = proc.getModulationDepth (kHarmTarget0 + k, readout.l);
    const float y0 = yOf (h[(size_t) k]), y1 = yOf (h[(size_t) k] + depth);
    const float cx = a.getX() + a.getWidth() / (float) granulum::kNumHarmonics * ((float) k + 0.5f);
    const float hx = lfoStickStyle == 1 || lfoStickStyle == 2 ? cx : lfoLineX (k, 0);
    drawDepthTag (g, depth, Palette::lfo (readout.l), { hx, jmin (y0, y1) - 10.0f }, getLocalBounds().toFloat().reduced (2.0f));
}

void HarmonicEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 9.0f);

    if (osc1WaveMode (proc))
    {
        // WAVE mode: the screen shows the full waveform instead of the sticks (they keep their values for ADDITIVE)
        const int wv = osc1WaveIndex (proc);
        const bool on = proc.apvts.getRawParameterValue (ParamID::osc1On)->load() > 0.5f;
        auto a = r.reduced (14.0f, 16.0f);
        const float mid = a.getCentreY(), amp = a.getHeight() * 0.42f;
        g.setColour (Palette::textDim.withAlpha (0.18f));
        g.drawHorizontalLine ((int) mid, a.getX(), a.getRight());
        Path p;
        const int pts = 400;
        for (int i = 0; i < pts; ++i)
        {
            const float t = (float) i / (float) (pts - 1) * 2.0f;          // two cycles
            const float ph = t - std::floor (t);
            float v = 0.0f;
            switch (wv)
            {
                case 0:  v = std::sin (MathConstants<float>::twoPi * ph); break;
                case 1:  v = 1.0f - 2.0f * ph; break;
                case 2:  v = ph < 0.5f ? 1.0f : -1.0f; break;
                default: v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
            }
            const float x = a.getX() + a.getWidth() * (float) i / (float) (pts - 1), y = mid - v * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        Path fill (p);
        fill.lineTo (a.getRight(), mid); fill.lineTo (a.getX(), mid); fill.closeSubPath();
        const auto cu = Palette::accent.withMultipliedAlpha (on ? 1.0f : 0.4f);
        g.setGradientFill (ColourGradient (cu.withAlpha (0.22f), a.getX(), a.getY(), cu.withAlpha (0.02f), a.getX(), mid, false));
        g.fillPath (fill);
        g.setColour (cu.withAlpha (0.25f));
        g.strokePath (p, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (cu);
        g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        const char* names[4] = { "SINE", "SAW", "SQUARE", "TRIANGLE" };
        g.setColour (Palette::textDim.withAlpha (0.75f));
        g.setFont (uiFont (9.0f, true, 0.12f));
        g.drawText (String (names[wv]) + "  \u00b7  FULL WAVE", r.reduced (10.0f, 6.0f), Justification::topLeft, false);
        return;
    }

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
        // 0.99.5: a stick with an LFO moves with it (the LFO range is still drawn over it)
        bool modded = false;
        for (int l = 0; l < kNumLfos; ++l) modded = modded || proc.hasModulation (kHarmTarget0 + k, l);
        const float v = modded ? jlimit (0.0f, 1.0f, proc.harmLive[(size_t) k].load (std::memory_order_relaxed)) : h[(size_t) k];
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

    // ---- LFO on sticks (no numbers; colour = which LFO). Style picked by lfoStickStyle (design preview) ----
    {
        const float cw = a.getWidth() / (float) granulum::kNumHarmonics;
        auto yOf = [&a] (float v) { return a.getBottom() - jlimit (0.0f, 1.0f, v) * a.getHeight(); };
        if (lfoDropBar >= 0)
        {
            g.setColour (Colours::white.withAlpha (0.08f));
            g.fillRoundedRectangle (Rectangle<float> (a.getX() + cw * (float) lfoDropBar, a.getY(), cw, a.getHeight()), 3.0f);
        }
        for (int k = 0; k < granulum::kNumHarmonics; ++k)
        {
            const float cx = a.getX() + cw * ((float) k + 0.5f);
            const float base = h[(size_t) k];
            const float live = proc.harmLive[(size_t) k].load (std::memory_order_relaxed);
            int slot = 0, count = 0;
            for (int l = 0; l < kNumLfos; ++l) count += proc.hasModulation (kHarmTarget0 + k, l) ? 1 : 0;
            for (int l = 0; l < kNumLfos; ++l)
            {
                if (! proc.hasModulation (kHarmTarget0 + k, l)) continue;
                const auto col = Palette::lfo (l).withMultipliedAlpha (proc.isLfoOn (l) ? 1.0f : 0.35f);
                const float depth = proc.getModulationDepth (kHarmTarget0 + k, l);
                const float y0 = yOf (base), y1 = yOf (base + depth);
                const float lx = lfoLineX (k, slot);
                switch (lfoStickStyle)
                {
                    case 0:   // A: a thin line in the LFO colour LEFT of the stick = the range; a short tick = where it is now
                    {
                        g.setColour (col.withAlpha (0.85f));
                        g.drawLine (lx, jmin (y0, y1), lx, jmax (y0, y1), 1.4f);
                        if (slot == 0)
                        {
                            g.setColour (col);
                            g.fillRoundedRectangle (Rectangle<float> (5.0f, 2.0f).withCentre ({ lx, yOf (live) }), 1.0f);
                        }
                        break;
                    }
                    case 1:   // B: a soft ghost of the stick in the LFO colour over its range. Several LFOs: thin ghosts side by side
                    {         // (one lane each, never mixed), and ONE glow across the stick at the summed live level
                        const float laneW = count > 1 ? 3.5f : 6.0f;
                        const float lx0 = cx - laneW * 0.5f * (float) count + laneW * (float) slot;
                        auto ghost = Rectangle<float> (lx0 + 0.4f, jmin (y0, y1), laneW - 0.8f, std::abs (y1 - y0));
                        g.setColour (col.withAlpha (0.22f));
                        g.fillRoundedRectangle (ghost, laneW * 0.4f);
                        g.setColour (col.withAlpha (0.55f));
                        g.fillRoundedRectangle (ghost.withHeight (1.5f).withY (y1 - 0.75f), 0.7f);   // end of this LFO's reach
                        if (slot == count - 1)
                        {
                            const auto glow = count > 1 ? Colour (0xfff1e4cb) : col;   // summed level of all LFOs: ivory when several
                            g.setColour (glow.withAlpha (0.35f));
                            g.fillEllipse (Rectangle<float> (12.0f, 6.0f).withCentre ({ cx, yOf (live) }));
                            g.setColour (glow);
                            g.fillRoundedRectangle (Rectangle<float> (jmin (10.0f, cw * 0.7f), 1.8f).withCentre ({ cx, yOf (live) }), 0.9f);
                        }
                        break;
                    }
                    case 2:   // C: a dotted rail in the LFO colour right on the stick's axis, a glowing bead at the live level
                    {
                        Path rail; rail.startNewSubPath (cx, y0); rail.lineTo (cx, y1);
                        Path dashed; const float dl[] = { 1.5f, 2.5f };
                        PathStrokeType (1.2f).createDashedStroke (dashed, rail, dl, 2);
                        g.setColour (col.withAlpha (0.8f)); g.fillPath (dashed);
                        if (slot == 0)
                        {
                            const auto pt = Point<float> (cx, yOf (live));
                            g.setColour (col.withAlpha (0.3f)); g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (pt));
                            g.setColour (col); g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre (pt));
                        }
                        break;
                    }
                    default:  // D: faint rail left of the stick, a small LFO-coloured dot riding on it
                    {
                        g.setColour (col.withAlpha (0.28f));
                        g.drawLine (lx, jmin (y0, y1), lx, jmax (y0, y1), 1.0f);
                        if (slot == 0)
                        {
                            const auto pt = Point<float> (lx, yOf (live));
                            g.setColour (col.withAlpha (0.35f)); g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre (pt));
                            g.setColour (col); g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (pt));
                        }
                        break;
                    }
                }
                ++slot;
            }
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
        if (rowStyle >= 10)
        {
            // the row says what it does: GRAIN + a small arrow pointing at the buttons
            const auto lab = Rectangle<float> (r.getX() + 4.0f, row.getY() - 3.0f, kGrainLabelW - 4.0f, row.getHeight() + 6.0f);
            g.setColour (Palette::accent.interpolatedWith (Colours::white, 0.15f).withAlpha (alpha));
            g.setFont (uiFont (9.5f, true, 0.12f));
            const String word = rowStyle == 15 ? "CLEAN/" : "GRAIN";
            if (rowStyle == 15)
            {
                g.setFont (uiFont (8.0f, true, 0.04f));
                g.setColour (pearl.withAlpha (0.8f * alpha));
                g.drawText ("C=CLEAN", lab.withTrimmedBottom (lab.getHeight() * 0.5f), Justification::centredLeft, false);
                g.setColour (Palette::accent.withAlpha (alpha));
                g.drawText ("G=GRAIN", lab.withTrimmedTop (lab.getHeight() * 0.5f), Justification::centredLeft, false);
            }
            else
            {
                g.drawText (word, lab.withTrimmedRight (8.0f), Justification::centredLeft, false);
                Path tri; const float tx = lab.getRight() - 6.0f, ty = lab.getCentreY();
                tri.addTriangle (tx - 3.0f, ty - 3.5f, tx - 3.0f, ty + 3.5f, tx + 2.5f, ty);
                g.fillPath (tri);
            }
        }
        else
        g.drawText ("SELECT", Rectangle<float> (r.getX() + 5.0f, row.getY() - 3.0f, kGrainLabelW, row.getHeight() + 6.0f), Justification::centredLeft, false);
        const float introX = introT < 1.8f ? (introT / 1.4f) : -1.0f;      // one light wave across the dots on open

        for (int k = 0; k < granulum::kNumHarmonics; ++k)
        {
            const auto cell = Rectangle<float> (row.getX() + bw * (float) k, row.getY(), bw, row.getHeight());
            const bool on = (mask >> k) & 1u;
            const bool hotK = hoverBar == k;
            const Colour cu = Palette::accent;
            auto grainIcon = [&g] (Point<float> c, float sz, Colour col)
            {
                // a tiny cluster of grains (same idea as the GRAIN button)
                const float d = sz * 0.26f;
                const Point<float> pts[] = { { -0.55f, 0.35f }, { 0.05f, -0.5f }, { 0.55f, 0.3f }, { -0.1f, 0.55f }, { 0.6f, -0.45f } };
                const float sizes[] = { 1.0f, 0.9f, 0.8f, 0.6f, 0.5f };
                g.setColour (col);
                for (int i = 0; i < 5; ++i)
                    g.fillEllipse (Rectangle<float> (d * sizes[i] * 2.0f, d * sizes[i] * 2.0f).withCentre (c + pts[i] * sz * 0.5f));
            };
            if (rowStyle == 10)
            {
                // A: chips — copper chip with grains when ON, empty frame with "+" when OFF
                auto chip = cell.reduced (jmax (1.5f, bw * 0.1f), 0.5f);
                if (on)
                {
                    g.setColour (cu.withAlpha ((hotK ? 1.0f : 0.92f) * alpha));
                    g.fillRoundedRectangle (chip, 3.0f);
                    grainIcon (chip.getCentre(), jmin (chip.getWidth(), chip.getHeight()) * 0.9f, Palette::deep.withAlpha (0.85f * alpha));
                }
                else
                {
                    g.setColour (pearlShade.withAlpha ((hotK ? 0.95f : 0.45f) * alpha));
                    g.drawRoundedRectangle (chip.reduced (0.5f), 3.0f, hotK ? 1.4f : 1.0f);
                    const auto c = chip.getCentre(); const float a2 = jmin (chip.getWidth(), chip.getHeight()) * 0.2f;
                    g.drawLine (c.x - a2, c.y, c.x + a2, c.y, 1.1f); g.drawLine (c.x, c.y - a2, c.x, c.y + a2, 1.1f);
                }
            }
            else if (rowStyle == 16)
            {
                // vertical switches: knob UP = into the grains (copper), DOWN = clean
                auto tr = cell.withSizeKeepingCentre (8.0f, jmin (cell.getHeight() - 1.0f, 17.0f));
                g.setColour (on ? cu.withAlpha (alpha) : Palette::button.withAlpha (alpha));
                g.fillRoundedRectangle (tr, 4.0f);
                if (hotK) { g.setColour (Colours::white.withAlpha (0.5f * alpha)); g.drawRoundedRectangle (tr.expanded (1.0f), 5.0f, 1.0f); }
                const float kr = 3.1f;
                const auto kc = Point<float> (tr.getCentreX(), on ? tr.getY() + 4.0f : tr.getBottom() - 4.0f);
                g.setColour (Colours::black.withAlpha (0.25f * alpha));
                g.fillEllipse (Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (kc.translated (0.0f, 0.6f)));
                g.setColour ((on ? Colours::white : Palette::textDim).withAlpha (alpha));
                g.fillEllipse (Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (kc));
            }
            else if (rowStyle == 11)
            {
                // B: little switches (copper = into grains)
                // a real toggle: a track a little wider than tall, the knob sits left (off) or right (on, copper)
                auto tr = cell.withSizeKeepingCentre (jlimit (12.0f, 17.0f, cell.getWidth() - 4.0f), 9.0f);
                g.setColour (on ? cu.withAlpha (alpha) : Palette::button.brighter (0.15f).withAlpha (alpha));
                g.fillRoundedRectangle (tr, 4.0f);
                g.setColour ((on ? cu.darker (0.4f) : Palette::edge.withMultipliedAlpha (2.5f)).withAlpha (alpha));
                g.drawRoundedRectangle (tr.reduced (0.5f), 4.0f, 1.0f);
                if (hotK) { g.setColour (Colours::white.withAlpha (0.5f * alpha)); g.drawRoundedRectangle (tr.expanded (1.0f), 5.0f, 1.0f); }
                const float kr = 3.0f;
                const auto kc = Point<float> (on ? tr.getRight() - 4.5f : tr.getX() + 4.5f, tr.getCentreY());
                g.setColour ((on ? Colours::white : Palette::textDim).withAlpha (alpha));
                g.fillEllipse (Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (kc));
            }
            else if (rowStyle == 12)
            {
                // C: grain icon per harmonic — glowing copper grains when ON, faint grey grains when OFF
                const auto c = cell.getCentre();
                const float sz = jmin (cell.getWidth(), cell.getHeight()) * 0.72f;
                if (on)
                {
                    g.setColour (cu.withAlpha (0.22f * alpha));
                    g.fillEllipse (Rectangle<float> (sz * 1.5f, sz * 1.5f).withCentre (c));
                    grainIcon (c, sz, cu.interpolatedWith (Colours::white, hotK ? 0.4f : 0.15f).withAlpha (alpha));
                }
                else
                    grainIcon (c, sz, pearlShade.withAlpha ((hotK ? 0.8f : 0.3f) * alpha));
            }
            else if (rowStyle == 13)
            {
                // D: one lane; ON cells light up copper and join into a band (reads like "this part goes to grains")
                auto lane = cell.withSizeKeepingCentre (cell.getWidth(), 12.0f);
                g.setColour (Palette::deep.withAlpha (0.8f * alpha));
                g.fillRect (lane);
                if (on)
                {
                    const bool prevOn = k > 0 && ((mask >> (k - 1)) & 1u);
                    const bool nextOn = k < granulum::kNumHarmonics - 1 && ((mask >> (k + 1)) & 1u);
                    auto seg = lane.reduced (0.0f, 1.5f).withTrimmedLeft (prevOn ? 0.0f : 1.5f).withTrimmedRight (nextOn ? 0.0f : 1.5f);
                    g.setColour (cu.interpolatedWith (Colours::white, hotK ? 0.25f : 0.0f).withAlpha (alpha));
                    g.fillRoundedRectangle (seg, 2.0f);
                    grainIcon (seg.getCentre(), 9.0f, Palette::deep.withAlpha (0.7f * alpha));
                }
                else
                {
                    g.setColour (pearlShade.withAlpha ((hotK ? 0.8f : 0.35f) * alpha));
                    g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (lane.getCentre()));
                }
            }
            else if (rowStyle == 14)
            {
                // E: check boxes
                auto box = cell.withSizeKeepingCentre (11.0f, 11.0f);
                if (on)
                {
                    g.setColour (cu.withAlpha (alpha)); g.fillRoundedRectangle (box, 2.5f);
                    Path tick; tick.startNewSubPath (box.getX() + 2.5f, box.getCentreY() + 0.2f); tick.lineTo (box.getX() + 4.6f, box.getBottom() - 2.6f); tick.lineTo (box.getRight() - 2.3f, box.getY() + 2.6f);
                    g.setColour (Palette::deep.withAlpha (alpha)); g.strokePath (tick, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
                }
                else
                {
                    g.setColour (pearlShade.withAlpha ((hotK ? 0.95f : 0.5f) * alpha));
                    g.drawRoundedRectangle (box.reduced (0.5f), 2.5f, hotK ? 1.4f : 1.0f);
                }
            }
            else if (rowStyle == 15)
            {
                // F: letters — G (copper) = grains, C (dim) = clean
                auto chip = cell.reduced (jmax (1.5f, bw * 0.1f), 0.5f);
                g.setColour ((on ? cu : Palette::track).withAlpha ((on ? 0.95f : 0.7f) * alpha));
                g.fillRoundedRectangle (chip, 3.0f);
                if (hotK) { g.setColour (Colours::white.withAlpha (0.6f * alpha)); g.drawRoundedRectangle (chip.expanded (0.5f), 3.5f, 1.0f); }
                g.setColour ((on ? Palette::deep : pearl.withAlpha (0.75f)).withAlpha (alpha));
                g.setFont (uiFont (9.5f, true, 0.0f));
                g.drawText (on ? "G" : "C", chip, Justification::centred, false);
            }
            else if (rowStyle == 2)
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

    if (rowStyle >= 10 && active && hoverBar >= 0 && hoverBar < granulum::kNumHarmonics)
    {
        // hover: the harmonic's bar lights up and a tag says where it goes
        const auto a = barArea(), row = pillRow();
        const float cw = a.getWidth() / (float) granulum::kNumHarmonics;
        const bool on = (mask >> hoverBar) & 1u;
        auto col = Rectangle<float> (a.getX() + cw * (float) hoverBar, a.getY(), cw, row.getBottom() - a.getY());
        g.setColour ((on ? Palette::accent : Colour (0xfff1e4cb)).withAlpha (0.10f));
        g.fillRoundedRectangle (col.expanded (1.0f, 2.0f), 3.0f);
        const String t = "H" + String (hoverBar + 1) + (on ? "  >  GRAINS" : "  >  CLEAN");
        const auto f = uiFont (9.5f, true, 0.06f);
        const float w = (float) GlyphArrangement::getStringWidthInt (f, t) + 14.0f;
        auto tag = Rectangle<float> (w, 17.0f).withCentre ({ col.getCentreX(), a.getY() + 12.0f });
        tag = tag.withX (jlimit (a.getX(), a.getRight() - w, tag.getX()));
        g.setColour (Palette::deep.withAlpha (0.92f)); g.fillRoundedRectangle (tag, 5.0f);
        g.setColour ((on ? Palette::accent : Colour (0xfff1e4cb)).withAlpha (0.9f)); g.drawRoundedRectangle (tag.reduced (0.5f), 5.0f, 1.0f);
        g.setFont (f); g.drawText (t, tag, Justification::centred, false);
    }

    if (showHint && active && rowStyle >= 10)
    {
        // first-run hint, deliberately quiet: one line of tiny dim text just above the switches + a faint pulse on one
        // switch. No box, nothing over the bars. Gone forever after the first click on the row.
        const auto a = barArea(), row = pillRow();
        const float cw = a.getWidth() / (float) granulum::kNumHarmonics;
        int target = 0;   // the first harmonic that sounds (at INIT: H1), so one click is heard at once
        for (int k = 0; k < granulum::kNumHarmonics; ++k) if (h[(size_t) k] > 0.05f) { target = k; break; }
        const auto cell = Rectangle<float> (a.getX() + cw * (float) target, row.getY(), cw, row.getHeight());
        const float pulse = 0.5f + 0.5f * std::sin (time * 2.5f);
        g.setColour (Palette::accent.withAlpha (0.12f + 0.25f * pulse));
        g.drawRoundedRectangle (cell.withSizeKeepingCentre (jmin (cell.getWidth() - 1.0f, 15.0f), 11.0f), 5.5f, 1.0f);
        const String txt ("click or drag the switches: harmonics into grains");
        bool roomForText = true;   // the line only shows over empty space, never on top of bars
        for (int k = target + 1; k < granulum::kNumHarmonics; ++k) if (h[(size_t) k] > 0.12f) roomForText = false;
        if (roomForText)
        g.setFont (uiFont (7.5f, false, 0.04f));
        if (roomForText)
        {
            g.setColour (Colour (0xfff1e4cb).withAlpha (0.42f));
            g.drawText (txt, Rectangle<float> (cell.getRight() + 3.0f, row.getY() - 11.0f, a.getRight() - cell.getRight() - 3.0f, 10.0f), Justification::centredLeft, false);
        }
    }
    else if (showHint && active)
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
    setTooltip ("Output level (click the bars to reset the peak).\n"
                "CLIP = everything over 0 dB is cut off at 0 dB, nothing else changes.\n"
                "It glows brighter while it is cutting. Click CLIP to switch it.");
    // (no own timer: GranulumEditor::timerCallback calls frame() at 30 fps)
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
    grShown = jmax (proc.limitGr.exchange (0.0f), grShown * 0.85f);   // CLIP glow: dB over 0 that was cut, falls off smoothly
    // repaint only when something visible changed (a silent meter used to repaint 30x a second)
    const float sig = level[0] + level[1] + hold[0] * 0.5f + hold[1] * 0.5f + (clipShown ? 10.0f : 0.0f) + grShown * 0.01f;
    if (std::abs (sig - lastSig) > 1.0e-4f || isMouseOver()) { lastSig = sig; repaint(); }
}

void PeakMeter::mouseMove (const MouseEvent&) { repaint(); }   // hover look on the CLIP box (no hand cursor: owner's rule)

void PeakMeter::mouseDown (const MouseEvent& e)
{
    // CLIP box: switch the output clipper on/off; anywhere else: reset the peak hold and the clip light
    if (e.position.x > (float) getWidth() - 40.0f)   // the whole CLIP box (and a little around it) is the button
    {
        if (auto* p = proc.apvts.getParameter (ParamID::clipOn))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
        }
        repaint();
        return;
    }
    clipShown = false;
    proc.clipped.store (false);
    hold[0] = hold[1] = 0.0f;
}

void PeakMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto clipBox = r.removeFromRight (36.0f).withSizeKeepingCentre (35.0f, 17.0f);
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

    // CLIP (0.99.7-test): the whole box is the button. On = copper; it glows brighter while it cuts over 0 dB.
    const bool clipOn = proc.apvts.getRawParameterValue (ParamID::clipOn)->load() > 0.5f;
    const bool hover  = isMouseOver() && getMouseXYRelative().x > getWidth() - 40;
    const float work  = clipOn ? jlimit (0.0f, 1.0f, grShown / 6.0f) : 0.0f;   // 0 = idle, 1 = 6 dB or more over 0 dB
    if (work > 0.02f)
    {
        g.setColour (Palette::accent.withAlpha (0.35f * work));
        g.fillRoundedRectangle (clipBox.expanded (2.5f), 6.0f);
    }
    g.setColour (clipOn ? Palette::accent.withMultipliedBrightness (0.72f + 0.5f * work).withAlpha (hover ? 0.95f : 0.88f)
                        : Palette::button.brighter (hover ? 0.25f : 0.0f));
    g.fillRoundedRectangle (clipBox, 4.0f);
    g.setColour (clipOn ? Palette::accent.brighter (0.2f + 0.5f * work) : Palette::edge.withMultipliedAlpha (hover ? 3.0f : 2.0f));
    g.drawRoundedRectangle (clipBox.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (clipOn ? Colours::white : Palette::textDim.withAlpha (hover ? 0.9f : 0.6f));
    g.setFont (uiFont (8.0f, true, 0.06f));
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
    setMouseCursor (MouseCursor::NormalCursor);
    setTooltip ("Click the light: switch this LFO on/off everywhere (routings are kept).\nDrag onto any knob to modulate it.\nALT + drag onto another LFO tab: copy all its settings there.");
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

    if (copyHover)
    {
        // ALT + drag target: copper frame and what will happen
        g.setColour (Palette::accent);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 2.0f);
        g.setFont (uiFont (11.0f, true, 0.08f));
        g.drawText ("COPY HERE", r.withTrimmedLeft (24.0f), Justification::centredLeft, false);
        return;
    }
    g.setColour ((selected ? Palette::hi : Palette::text).withAlpha (on ? 1.0f : 0.45f));
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
    setMouseCursor (e.position.x < 26.0f ? MouseCursor::NormalCursor : MouseCursor::NormalCursor);
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
        c->startDragging ((e.mods.isAltDown() ? "lfocopy:" : "lfo:") + String (index), this);
    }
}

void LfoTab::mouseUp (const MouseEvent&) { dragging = false; }

void LfoTab::itemDropped (const SourceDetails& s)
{
    copyHover = false;
    const int from = s.description.toString().fromFirstOccurrenceOf (":", false, false).getIntValue();
    proc.copyLfo (from, index);
    if (onSelect) onSelect (index);
    repaint();
}

// ============================================================================
// 1.0.5 LINKS

void LinksTab::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (selected ? Palette::button : Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    if (selected) { g.setColour (Palette::accent); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f); }
    for (int l = 0; l < kNumLfos; ++l)
    {
        g.setColour (Palette::lfo (l));
        g.fillEllipse (Rectangle<float> (4.5f, 4.5f).withCentre ({ r.getX() + 8.0f + 4.5f * (float) l, r.getCentreY() }));
    }
    g.setColour (selected ? Palette::hi : Palette::text);
    g.setFont (uiFont (13.0f, true, 0.08f));
    g.drawText ("LINKS", r.withTrimmedLeft (24.0f), Justification::centredLeft, false);
    int n = 0;
    for (int l = 0; l < kNumLfos; ++l) n += proc.countModulations (l);
    if (n > 0)
    {
        auto b = Rectangle<float> (18.0f, 13.0f).withCentre ({ r.getRight() - 13.0f, r.getCentreY() });
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (b, 6.5f);
        g.setColour (Palette::onAccent());
        g.setFont (uiFont (9.5f, true));
        g.drawText (String (n), b, Justification::centred, false);
    }
}

std::vector<std::pair<int, int>> LinksView::shown() const
{
    std::vector<std::pair<int, int>> out;
    for (auto& rw : rows) if (filter < 0 || rw.second == filter) out.push_back (rw);
    return out;
}

void LinksView::clampScroll()
{
    const int n = (int) shown().size();
    const int maxScroll = jmax (0, (n - kVisible + 1) / 2 * 2);   // scrolls by one row in each column
    scroll = jlimit (0, maxScroll, scroll);
}

Rectangle<float> LinksView::chip (int i) const
{
    const auto b = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
    const float w = 34.0f, gap = 5.0f;
    const float x0 = b.getRight() - 84.0f - 4.0f * w - 3.0f * gap;
    return { x0 + (float) i * (w + gap), b.getY() + 1.0f, w, 15.0f };
}

Rectangle<float> LinksView::removeAll() const
{
    const auto b = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
    return { b.getRight() - 78.0f, b.getY() + 1.0f, 78.0f, 15.0f };
}

LinksView::Geo LinksView::geo (int slot) const
{
    auto b = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
    b.removeFromTop (22.0f);
    const float colW = (b.getWidth() - 14.0f) / 2.0f, rh = b.getHeight() / (float) kPerCol;
    const int col = slot / kPerCol, row = slot % kPerCol;
    Geo g;
    g.row = { b.getX() + (float) col * (colW + 14.0f), b.getY() + rh * (float) row, colW, rh };
    auto r = g.row;
    g.dot = Rectangle<float> (12.0f, 12.0f).withCentre ({ r.getX() + 7.0f, r.getCentreY() });
    g.close = Rectangle<float> (14.0f, 14.0f).withCentre ({ r.getRight() - 8.0f, r.getCentreY() });
    g.value = Rectangle<float> (34.0f, rh).withPosition (g.close.getX() - 38.0f, r.getY());
    const float tx = r.getX() + r.getWidth() * 0.42f;
    g.track = Rectangle<float> (tx, r.getCentreY() - 6.0f, g.value.getX() - 6.0f - tx, 12.0f);
    return g;
}

String LinksView::targetName (int t) const
{
    if (isHarmTarget (t)) return "OSC 1  HARM " + String (t - kHarmTarget0 + 1);
    if (t >= mtSubH1 && t < mtSubH1 + 5) return "SUB " + String (t - mtSubH1 + 1);
    if (auto* p = proc.apvts.getParameter (GranulumProcessor::modTargetId (t))) return p->getName (40).toUpperCase();
    return GranulumProcessor::modTargetId (t);
}

void LinksView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (b, 8.0f);
    const auto all = shown();
    auto head = b.reduced (8.0f, 6.0f).removeFromTop (17.0f);
    g.setColour (Palette::text);
    g.setFont (uiFont (11.0f, true, 0.1f));
    g.drawText ("LINKS", head, Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (9.5f, false, 0.04f));
    g.drawText (String ((int) rows.size()) + ((int) rows.size() == 1 ? " link" : " links"), head.withTrimmedLeft (46.0f), Justification::centredLeft, false);
    for (int i = 0; i < 4; ++i)
    {
        const auto c = chip (i);
        const auto col = i == 0 ? Palette::accent : Palette::lfo (i - 1);
        const bool on = filter == i - 1;
        g.setColour (on ? col : Palette::field);
        g.fillRoundedRectangle (c, 7.5f);
        g.setColour (col.withAlpha (on ? 1.0f : 0.7f));
        g.drawRoundedRectangle (c.reduced (0.5f), 7.5f, 1.0f);
        g.setColour (on ? Palette::screen : col);
        g.setFont (uiFont (9.0f, true, 0.06f));
        g.drawText (i == 0 ? "ALL" : String (i), c, Justification::centred, false);
    }
    g.setColour (all.empty() ? Palette::textDim.withAlpha (0.4f) : Palette::textDim);
    g.setFont (uiFont (8.5f, true, 0.1f));
    g.drawText ("REMOVE ALL", removeAll(), Justification::centredRight, false);

    if (all.empty())
    {
        g.setColour (Palette::textDim);
        g.setFont (uiFont (11.0f, false, 0.02f));
        g.drawText ("No links yet - drag an LFO tab onto a knob, stick or pill.", b.withTrimmedTop (20.0f), Justification::centred, false);
        return;
    }
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawVerticalLine ((int) (geo (kPerCol).row.getX() - 7.0f), geo (0).row.getY(), geo (kPerCol - 1).row.getBottom());
    for (int s = 0; s < kVisible; ++s)
    {
        const int i = scroll + s;
        if (i >= (int) all.size()) break;
        const auto [t, l] = all[(size_t) i];
        const auto gm = geo (s);
        const auto col = Palette::lfo (l).withMultipliedAlpha (proc.isLfoOn (l) ? 1.0f : 0.45f);
        g.setColour (col);
        g.fillEllipse (gm.dot);
        g.setColour (Palette::screen);
        g.setFont (uiFont (8.0f, true));
        g.drawText (String (l + 1), gm.dot.translated (0.0f, 0.5f), Justification::centred, false);
        g.setColour (Palette::text);
        g.setFont (uiFont (10.0f, true, 0.04f));
        g.drawText (targetName (t), Rectangle<float> (gm.dot.getRight() + 6.0f, gm.row.getY(), gm.track.getX() - gm.dot.getRight() - 10.0f, gm.row.getHeight()),
                    Justification::centredLeft, true);
        const float d = proc.getModulationDepth (t, l);
        const auto tr = gm.track;
        const float cy = tr.getCentreY(), cx = tr.getCentreX(), xv = cx + tr.getWidth() * 0.5f * jlimit (-1.0f, 1.0f, d);
        g.setColour (Colours::white.withAlpha (0.09f));
        g.fillRoundedRectangle (tr.getX(), cy - 2.0f, tr.getWidth(), 4.0f, 2.0f);
        g.setColour (Palette::textDim.withAlpha (0.6f));
        g.fillRect (cx - 0.5f, cy - 5.0f, 1.0f, 10.0f);
        g.setColour (col);
        g.fillRoundedRectangle (jmin (cx, xv), cy - 2.0f, std::abs (xv - cx), 4.0f, 2.0f);
        g.setColour (Palette::text);
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre ({ xv, cy }));
        const int pc = roundToInt (d * 100.0f);
        g.setFont (uiFont (10.0f, true, 0.0f));
        g.drawText ((pc > 0 ? "+" : "") + String (pc), gm.value, Justification::centredRight, false);
        g.setColour (Palette::textDim);
        g.drawEllipse (gm.close.reduced (0.5f), 1.0f);
        const auto x = gm.close.reduced (4.2f);
        g.setColour (Palette::text);
        g.drawLine (x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.2f);
        g.drawLine (x.getX(), x.getBottom(), x.getRight(), x.getY(), 1.2f);
    }
    if ((int) all.size() > kVisible)
    {
        // a thin scroll mark on the right
        const float h = b.getHeight() - 36.0f, frac = (float) kVisible / (float) all.size();
        const float pos = (float) scroll / (float) jmax (1, (int) all.size() - kVisible);
        g.setColour (Colours::white.withAlpha (0.15f));
        g.fillRoundedRectangle (b.getRight() - 4.0f, b.getY() + 28.0f + (h * (1.0f - frac)) * jlimit (0.0f, 1.0f, pos), 2.0f, h * frac, 1.0f);
    }
}

void LinksView::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 4; ++i) if (chip (i).contains (e.position)) { filter = i - 1; scroll = 0; repaint(); return; }
    const auto all = shown();
    if (removeAll().contains (e.position))
    {
        for (auto [t, l] : all) proc.removeModulation (t, l);
        refresh();
        return;
    }
    for (int s = 0; s < kVisible && scroll + s < (int) all.size(); ++s)
    {
        const auto gm = geo (s);
        const auto [t, l] = all[(size_t) (scroll + s)];
        if (gm.close.expanded (3.0f).contains (e.position) || (e.mods.isPopupMenu() && gm.row.contains (e.position)))
        {
            proc.removeModulation (t, l);
            refresh();
            return;
        }
        if (gm.track.expanded (6.0f, 4.0f).contains (e.position))
        {
            dragRow = scroll + s;
            dragStartDepth = proc.getModulationDepth (t, l);
            dragStartX = e.position.x;
            // a click on the track jumps there
            proc.setModulation (t, l, jlimit (-1.0f, 1.0f, (e.position.x - gm.track.getCentreX()) / (gm.track.getWidth() * 0.5f)));
            dragStartDepth = proc.getModulationDepth (t, l);
            repaint();
            return;
        }
    }
}

void LinksView::mouseDrag (const MouseEvent& e)
{
    const auto all = shown();
    if (dragRow < 0 || dragRow >= (int) all.size()) return;
    const auto [t, l] = all[(size_t) dragRow];
    const auto gm = geo (dragRow - scroll);
    const float k = e.mods.isShiftDown() ? 0.25f : 1.0f;
    float d = dragStartDepth + (e.position.x - dragStartX) / (gm.track.getWidth() * 0.5f) * k;
    d = std::round (jlimit (-1.0f, 1.0f, d) * 100.0f) / 100.0f;
    proc.setModulation (t, l, d);
    repaint();
}

void LinksView::mouseDoubleClick (const MouseEvent& e)
{
    // double-click a slider: back to a full swing from the knob
    const auto all = shown();
    for (int s = 0; s < kVisible && scroll + s < (int) all.size(); ++s)
        if (geo (s).track.expanded (6.0f, 4.0f).contains (e.position))
        {
            const auto [t, l] = all[(size_t) (scroll + s)];
            proc.setModulation (t, l, proc.getModulationDepth (t, l) < 0.0f ? -1.0f : 1.0f);
            repaint();
            return;
        }
}

void LinksView::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    scroll += w.deltaY < 0.0f ? 2 : -2;
    clampScroll();
    repaint();
}

// ============================================================================
// LFO editor — 4 points, bendable segments

LfoEditor::LfoEditor (GranulumProcessor& p) : proc (p)
{
    pts = proc.getLfoPoints (0);
    setRepaintsOnMouseActivity (false);
    setTooltip ("Drag points. Double-click an empty spot = add a point (up to 16).\n"
                "Double-click or right-click a point = delete it. Drag the small circles to bend a segment.");
    // (no own timer: GranulumEditor::timerCallback calls frame() at 30 fps)
}

void LfoEditor::setLfo (int l)
{
    if (l < 0 || l >= kNumLfos) return;   // the ADSR tab (index 3) is not an LFO: INIT / preset load with ADSR open crashed here
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
        bool same = now.size() == pts.size();
        for (size_t i = 0; same && i < now.size(); ++i)
            same &= now[i].x == pts[i].x && now[i].y == pts[i].y && now[i].curve == pts[i].curve;
        if (! same) { pts = now; repaint(); return; }
    }
    // repaint only while visible and the playhead moves (or while dragging)
    if (! isShowing()) return;
    const float ph = proc.lfoPhaseOut[(size_t) lfo].load (std::memory_order_relaxed);
    if (ph != lastPhaseShown || dragIndex >= 0 || isMouseOver() || ++idleTicks > 20) { lastPhaseShown = ph; idleTicks = 0; repaint(); }
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
    const int n = (int) pts.size();
    for (int i = 0; i < n; ++i)
    {
        const float d = toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) return best;
    for (int s = 0; s < n - 1; ++s)
    {
        const float d = handlePos (s).getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = s; isHandle = true; }
    }
    return best;
}

Rectangle<float> LfoEditor::removeRect() const
{
    return { 8.0f, 5.0f, 74.0f, 17.0f };
}

void LfoEditor::mouseMove (const MouseEvent& e)
{
    const bool overRemove = false;
    if (overRemove != hoverRemove) { hoverRemove = overRemove; repaint(); }
    if (overRemove) { setMouseCursor (MouseCursor::NormalCursor); return; }
    bool h = false;
    const int i = hitTest (e.position, h);
    if (i != hoverIndex || h != hoverHandle)
    {
        hoverIndex = i; hoverHandle = h;
        setMouseCursor (i < 0 ? MouseCursor::NormalCursor : h ? MouseCursor::UpDownResizeCursor : MouseCursor::NormalCursor);
    }
}

void LfoEditor::mouseExit (const MouseEvent&) { hoverIndex = -1; }

void LfoEditor::mouseDown (const MouseEvent& e)
{
    // right click on a point (not the ends) = delete it
    if (e.mods.isPopupMenu())
    {
        pts = proc.getLfoPoints (lfo);
        bool h = false;
        const int i = hitTest (e.position, h);
        if (i > 0 && ! h && i < (int) pts.size() - 1) { pts.erase (pts.begin() + i); proc.setLfoPoints (lfo, pts); repaint(); }
        dragIndex = -1;
        return;
    }
    if (false)
    {
        // REMOVE: takes this LFO off every knob (unlike the light, which only pauses it)
        proc.removeLfoEverywhere (lfo);
        hoverRemove = false;
        dragIndex = -1;
        repaint();
        return;
    }
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
        if (dragIndex > 0 && dragIndex < (int) pts.size() - 1)
            p.x = jlimit (pts[(size_t) dragIndex - 1].x, pts[(size_t) dragIndex + 1].x, (e.position.x - a.getX()) / a.getWidth());
    }
    proc.setLfoPoints (lfo, pts);
    repaint();
}

void LfoEditor::mouseUp (const MouseEvent&) { dragIndex = -1; }

void LfoEditor::mouseDoubleClick (const MouseEvent& e)
{
    pts = proc.getLfoPoints (lfo);
    bool h = false;
    const int i = hitTest (e.position, h);
    if (i >= 0 && h)                                   // curve handle: straighten the segment
    {
        pts[(size_t) i].curve = 0.0f;
    }
    else if (i >= 0)                                   // a point: delete it (the two ends stay)
    {
        if (i == 0 || i == (int) pts.size() - 1) return;
        pts.erase (pts.begin() + i);
    }
    else if ((int) pts.size() < kMaxLfoPoints)         // empty place: add a point there
    {
        const auto a = area();
        const float x = jlimit (0.001f, 0.999f, (e.position.x - a.getX()) / a.getWidth());
        const float y = jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
        size_t at = 1;
        while (at < pts.size() && pts[at].x < x) ++at;
        pts.insert (pts.begin() + (long) at, LfoPoint { x, y, 0.0f });
    }
    proc.setLfoPoints (lfo, pts);
    dragIndex = -1;
    repaint();
}

// ============================================================================
// FilterScope

FilterScope::FilterScope (GranulumProcessor& p) : proc (p)
{
    specDb.fill (-120.0f);
    setMouseCursor (MouseCursor::LeftRightResizeCursor);
    setTooltip ("Filter view. Drag left/right = CUTOFF, up/down = RESO. Faint = what comes in, bright = what passes.");
    // (no own timer: GranulumEditor::timerCallback calls frame() at 30 fps)
    for (auto* id : { ParamID::cutoff, ParamID::cutoff2, ParamID::reso, ParamID::reso2, ParamID::ftype, ParamID::ftype2,
                      ParamID::peakGain, ParamID::peakGain2, ParamID::peakQ, ParamID::peakQ2, ParamID::filterOn, ParamID::filter2On })
        proc.apvts.addParameterListener (id, this);
}

FilterScope::~FilterScope()
{
    for (auto* id : { ParamID::cutoff, ParamID::cutoff2, ParamID::reso, ParamID::reso2, ParamID::ftype, ParamID::ftype2,
                      ParamID::peakGain, ParamID::peakGain2, ParamID::peakQ, ParamID::peakQ2, ParamID::filterOn, ParamID::filter2On })
        proc.apvts.removeParameterListener (id, this);
    cancelPendingUpdate();
}

float FilterScope::xToHz (float x) const
{
    const float t = jlimit (0.0f, 1.0f, x / jmax (1.0f, (float) getWidth()));
    return 30.0f * std::pow (18000.0f / 30.0f, t);
}

void FilterScope::timerCallback()
{
    if (! isVisible() || getWidth() <= 0) return;
    const int N = 2048;
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    // idle: no sound coming in and the spectrum has already faded out -> no FFT, no repaint (knob changes repaint on their own)
    {
        float mx = 0.0f;
        for (int i = 0; i < N; i += 4) mx = jmax (mx, std::abs (proc.scopeRing[(size_t) ((w - N + i) & (GranulumProcessor::kScopeSize - 1))]));
        if (mx < 1.0e-5f) { if (++silentFrames > 40) return; }
        else silentFrames = 0;
    }
    for (int i = 0; i < N; ++i)
    {
        const float win = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (N - 1));
        fftData[(size_t) i] = proc.scopeRing[(size_t) ((w - N + i) & (GranulumProcessor::kScopeSize - 1))] * win;
    }
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const int cols = jmin ((int) specDb.size(), jmax (1, getWidth()));
    for (int c = 0; c < cols; ++c)
    {
        const float f0 = xToHz ((float) c), f1 = xToHz ((float) c + 1.0f);
        const int b0 = jlimit (1, N / 2 - 1, (int) (f0 / sr * N)), b1 = jlimit (b0, N / 2 - 1, (int) (f1 / sr * N));
        float m = 0.0f;
        for (int b = b0; b <= b1; ++b) m = jmax (m, fftData[(size_t) b]);
        const float db = Decibels::gainToDecibels (m / (float) (N / 4), -120.0f);
        auto& d = specDb[(size_t) c];
        d = db > d ? d + (db - d) * 0.6f : jmax (db, d - 2.5f);
    }
    repaint();
}

void FilterScope::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 5.0f);
    auto a = r.reduced (2.0f, 2.0f);

    auto& s = proc.apvts;
    const bool on   = s.getRawParameterValue (which == 0 ? ParamID::filterOn : ParamID::filter2On)->load() > 0.5f;
    const int  type = (int) s.getRawParameterValue (which == 0 ? ParamID::ftype : ParamID::ftype2)->load();
    // straight from the knobs when nothing modulates them (instant), else what the audio thread computed
    auto modulated = [this] (const char* id) { const int t = GranulumProcessor::modIndexOf (id);
                                               for (int l = 0; l < kNumLfos; ++l) if (t >= 0 && proc.hasModulation (t, l)) return true; return false; };
    const char* cutId  = which == 0 ? ParamID::cutoff : ParamID::cutoff2;
    const bool isPeak = type == FilterUnit::Peak;
    const char* resId  = isPeak ? (which == 0 ? ParamID::peakQ : ParamID::peakQ2) : (which == 0 ? ParamID::reso : ParamID::reso2);
    const char* gainId = which == 0 ? ParamID::peakGain : ParamID::peakGain2;
    const float keyMul = type == FilterUnit::Peak ? 1.0f : std::exp2 ((proc.engineAdd.lastNote.load (std::memory_order_relaxed) - 60.0f) / 12.0f);
    const float fc  = modulated (cutId)  ? (which == 0 ? proc.keyCutoff : proc.keyCutoff2).load (std::memory_order_relaxed)
                                         : s.getRawParameterValue (cutId)->load() * keyMul;
    const float q   = modulated (resId)  ? (isPeak ? (which == 0 ? proc.viewPeakQ1 : proc.viewPeakQ2) : (which == 0 ? proc.viewReso1 : proc.viewReso2)).load (std::memory_order_relaxed)
                                         : s.getRawParameterValue (resId)->load();
    const float pk  = modulated (gainId) ? (which == 0 ? proc.viewPeak1 : proc.viewPeak2).load (std::memory_order_relaxed)
                                         : s.getRawParameterValue (gainId)->load();
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const float widthNow = proc.apvts.getParameter (which == 0 ? ParamID::bpWidth : ParamID::bpWidth2)
                               ->convertFrom0to1 (proc.modNorm[(size_t) (which == 0 ? mtBpWidth : mtBpWidth2)].load (std::memory_order_relaxed));

    // grid: 100 Hz, 1 kHz, 10 kHz
    g.setColour (Colours::white.withAlpha (0.05f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * std::log (hz / 30.0f) / std::log (600.0f)), a.getY(), a.getBottom());

    const float top = 40.0f, bottom = -40.0f;                      // response dB range: 0 dB in the middle, room for a +36 dB PEAK
    auto yOfResp = [&] (float db) { return a.getY() + a.getHeight() * (top - jlimit (bottom, top, db)) / (top - bottom); };
    auto yOfSpec = [&] (float db) { return a.getBottom() - a.getHeight() * jlimit (0.0f, 1.0f, (db + 78.0f) / 66.0f); };
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) yOfResp (0.0f), a.getX(), a.getRight());

    const int n = jmax (8, (int) a.getWidth());
    Path inSpec, outSpec, curve;
    inSpec.startNewSubPath (a.getX(), a.getBottom());
    outSpec.startNewSubPath (a.getX(), a.getBottom());
    float markX = -1.0f, markY = 0.0f;
    for (int i = 0; i <= n; ++i)
    {
        const float x = a.getX() + (float) i;
        const float hz = xToHz ((float) i * (float) getWidth() / (float) n);
        const float resp = on ? Decibels::gainToDecibels (FilterUnit::response (type, hz, fc, q, sr, pk, widthNow), -100.0f) : 0.0f;
        const float sp = specDb[(size_t) jlimit (0, (int) specDb.size() - 1, (int) (x))];
        inSpec.lineTo (x, yOfSpec (sp));
        outSpec.lineTo (x, yOfSpec (sp + resp));
        const float y = yOfResp (resp);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        if (markX < 0 && hz >= fc) { markX = x; markY = y; }
    }
    inSpec.lineTo (a.getRight(), a.getBottom()); inSpec.closeSubPath();
    outSpec.lineTo (a.getRight(), a.getBottom()); outSpec.closeSubPath();

    g.saveState();
    g.reduceClipRegion (a.toNearestInt());
    g.setColour (Palette::textDim.withAlpha (0.13f));
    g.fillPath (inSpec);                                             // everything that comes in
    if (on)
    {
        g.setColour (Palette::accent.withAlpha (0.30f));
        g.fillPath (outSpec);                                        // what passes the filter
    }
    // soft glow under the curve
    Path under (curve);
    under.lineTo (a.getRight(), a.getBottom()); under.lineTo (a.getX(), a.getBottom()); under.closeSubPath();
    g.setGradientFill (ColourGradient (Palette::accent.withAlpha (on ? 0.16f : 0.05f), 0, a.getY(),
                                       Palette::accent.withAlpha (0.0f), 0, a.getBottom(), false));
    g.fillPath (under);
    g.setColour (on ? Palette::accent.brighter (0.25f) : Palette::textDim.withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
    if (on && markX >= 0 && type != 6)
    {
        g.setColour (Colours::white.withAlpha (0.9f));
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ markX, jlimit (a.getY() + 2.5f, a.getBottom() - 2.5f, markY) }));
    }
    g.restoreState();
}

void FilterScope::mouseDown (const MouseEvent& e)
{
    auto& s = proc.apvts;
    auto* c = s.getParameter (which == 0 ? ParamID::cutoff : ParamID::cutoff2);
    const bool peak = (int) s.getRawParameterValue (which == 0 ? ParamID::ftype : ParamID::ftype2)->load() == FilterUnit::Peak;
    auto* q = s.getParameter (peak ? (which == 0 ? ParamID::peakGain : ParamID::peakGain2) : (which == 0 ? ParamID::reso : ParamID::reso2));
    dragCut = c->getValue(); dragRes = q->getValue();
    dragStart = e.position;
    c->beginChangeGesture(); q->beginChangeGesture();
}

void FilterScope::mouseDrag (const MouseEvent& e)
{
    auto& s = proc.apvts;
    auto* c = s.getParameter (which == 0 ? ParamID::cutoff : ParamID::cutoff2);
    const bool peak = (int) s.getRawParameterValue (which == 0 ? ParamID::ftype : ParamID::ftype2)->load() == FilterUnit::Peak;
    auto* q = s.getParameter (peak ? (which == 0 ? ParamID::peakGain : ParamID::peakGain2) : (which == 0 ? ParamID::reso : ParamID::reso2));
    const auto d = e.position - dragStart;
    c->setValueNotifyingHost (jlimit (0.0f, 1.0f, dragCut + d.x / jmax (40.0f, (float) getWidth())));
    q->setValueNotifyingHost (jlimit (0.0f, 1.0f, dragRes - d.y / 120.0f));
}

void FilterScope::mouseUp (const MouseEvent&)
{
    auto& s = proc.apvts;
    s.getParameter (which == 0 ? ParamID::cutoff : ParamID::cutoff2)->endChangeGesture();
    const bool peak = (int) s.getRawParameterValue (which == 0 ? ParamID::ftype : ParamID::ftype2)->load() == FilterUnit::Peak;
    s.getParameter (peak ? (which == 0 ? ParamID::peakGain : ParamID::peakGain2) : (which == 0 ? ParamID::reso : ParamID::reso2))->endChangeGesture();
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
    for (int s = 0; s < (int) pts.size() - 1; ++s)
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
    for (int i = 0; i < (int) pts.size(); ++i)
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

    // REMOVE: only when this LFO is routed somewhere
    if (const int n = proc.countModulations (lfo); false && n > 0)
    {
        const auto rr = removeRect();
        g.setColour ((hoverRemove ? Palette::buttonHi : Palette::field).withAlpha (hoverRemove ? 0.95f : 0.7f));
        g.fillRoundedRectangle (rr, 5.0f);
        g.setColour (hoverRemove ? Colours::white : Palette::textDim);
        g.setFont (uiFont (9.5f, true, 0.12f));
        g.drawText (String::fromUTF8 ("\xc3\x97 REMOVE ") + String (n), rr, Justification::centred, false);
    }
}


// ============================================================================
// AMP tab + ADSR editor

void AmpTab::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = Palette::accent;                 // ADSR's own colour: teal, not used by any LFO
    const bool on = param != nullptr && param->getValue() > 0.5f;
    g.setColour (selected ? Palette::button : Palette::field);
    g.fillRoundedRectangle (r, 6.0f);
    if (selected)
    {
        g.setColour (col);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);
    }
    const auto ledR = Rectangle<float> (10.0f, 10.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() });
    if (on) drawLed (g, ledR, col, col.darker (0.6f), 0.9f);
    else  { g.setColour (col.withAlpha (0.45f)); g.drawEllipse (ledR.reduced (0.5f), 1.2f); }

    const auto f = uiFont (13.0f, true, 0.08f);
    g.setFont (f);
    g.setColour ((selected ? Palette::hi : Palette::text).withAlpha (on ? 1.0f : 0.45f));
    auto tr = r.withTrimmedLeft (24.0f);
    g.drawText ("ADSR", tr, Justification::centredLeft, false);
    const float w = (float) GlyphArrangement::getStringWidthInt (f, "ADSR") + 5.0f;
    g.setFont (uiFont (10.0f, false, 0.08f));
    g.drawText ("amp", tr.withTrimmedLeft (w), Justification::centredLeft, false);   // same colour as ADSR
}

void AmpTab::mouseMove (const MouseEvent& e)
{
    setMouseCursor (e.position.x < 26.0f ? MouseCursor::NormalCursor : MouseCursor::NormalCursor);
}

void AmpTab::mouseDown (const MouseEvent& e)
{
    if (e.position.x < 26.0f && param != nullptr)
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        param->endChangeGesture();
        repaint();
    }
    if (onSelect) onSelect();
}

AmpEditor::AmpEditor (GranulumProcessor& p) : proc (p)
{
    setTooltip ("AMP envelope = the volume of every note.\nDrag the points: attack peak, decay / sustain corner, release end.");
    // (no own timer: GranulumEditor::timerCallback calls frame() at 30 fps)
}

float AmpEditor::value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void AmpEditor::setValue (const char* id, float v)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

static float envSeg (float ms, float maxMs, float maxW) { return jmax (4.0f, maxW * std::log1p (ms / 5.0f) / std::log1p (maxMs / 5.0f)); }
static float envTime (float w, float maxMs, float maxW) { return 5.0f * std::expm1 (jlimit (0.0f, 1.0f, w / maxW) * std::log1p (maxMs / 5.0f)); }

AmpEditor::Geo AmpEditor::geometry() const
{
    Geo g;
    g.a = getLocalBounds().toFloat().reduced (12.0f, 9.0f);
    g.maxW = g.a.getWidth() * 0.27f;
    g.plateau = g.a.getWidth() * 0.17f;
    const float top = g.a.getY(), bot = g.a.getBottom(), h = g.a.getHeight();
    g.p0 = { g.a.getX(), bot };
    g.p1 = { g.p0.x + envSeg (value (ParamID::ampA), 10000.0f, g.maxW), top };
    g.p2 = { g.p1.x + envSeg (value (ParamID::ampD), 10000.0f, g.maxW), top + (1.0f - value (ParamID::ampS)) * h };
    g.p3 = { g.p2.x + g.plateau, g.p2.y };
    g.p4 = { g.p3.x + envSeg (value (ParamID::ampR), 10000.0f, g.maxW), bot };
    return g;
}

void AmpEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 8.0f);
    const auto geo = geometry();
    const auto col = Palette::accent;

    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) geo.a.getBottom(), geo.a.getX(), geo.a.getRight());

    // curve: linear-ish attack, exponential decay and release
    Path p;
    p.startNewSubPath (geo.p0);
    p.quadraticTo ({ geo.p0.x + (geo.p1.x - geo.p0.x) * 0.55f, geo.p1.y + (geo.p0.y - geo.p1.y) * 0.25f }, geo.p1);
    auto expSeg = [&p] (Point<float> a, Point<float> b)
    {
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24.0f;
            const float k = (1.0f - std::exp (-4.0f * t)) / (1.0f - std::exp (-4.0f));
            p.lineTo (a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * k);
        }
    };
    expSeg (geo.p1, geo.p2);
    p.lineTo (geo.p3);
    expSeg (geo.p3, geo.p4);

    Path fill (p);
    fill.lineTo (geo.p4.x, geo.a.getBottom());
    fill.lineTo (geo.p0);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (col.withAlpha (0.26f), 0, geo.a.getY(), col.withAlpha (0.02f), 0, geo.a.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (col);
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // A D S R under their segments
    g.setFont (uiFont (9.5f, true, 0.1f));
    g.setColour (Palette::textDim.withAlpha (0.8f));
    const float ly = geo.a.getBottom() + 1.0f;
    (void) ly;
    g.setFont (uiFont (10.5f, false, 0.1f));
    g.drawText ("VOLUME", r.reduced (10.0f, 5.0f), Justification::topRight, false);

    // handles
    const Point<float> hs[3] = { geo.p1, geo.p2, geo.p4 };
    for (int i = 0; i < 3; ++i)
    {
        const bool hot = hover == i || dragging == i;
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (Rectangle<float> (hot ? 18.0f : 14.0f, hot ? 18.0f : 14.0f).withCentre (hs[i]));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (hs[i]));
    }

    // live dot: where the newest note is on its envelope right now
    const int st = proc.engineAdd.vizStage.load();
    const float lv = proc.engineAdd.vizEnv.load();
    if (st > 0)
    {
        const float S = value (ParamID::ampS);
        Point<float> d;
        if (st == 1)      d = { geo.p0.x + (geo.p1.x - geo.p0.x) * lv, geo.a.getBottom() - geo.a.getHeight() * lv };
        else if (st == 2) d = { geo.p1.x + (geo.p2.x - geo.p1.x) * jlimit (0.0f, 1.0f, (1.0f - lv) / jmax (0.001f, 1.0f - S)), geo.a.getBottom() - geo.a.getHeight() * lv };
        else if (st == 3) d = { (geo.p2.x + geo.p3.x) * 0.5f, geo.a.getBottom() - geo.a.getHeight() * lv };
        else              d = { geo.p3.x + (geo.p4.x - geo.p3.x) * jlimit (0.0f, 1.0f, std::log (jmax (0.001f, lv) / jmax (0.001f, S)) / std::log (0.001f / jmax (0.001f, S))),
                                geo.a.getBottom() - geo.a.getHeight() * lv };
        g.setColour (col.withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (d));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (d));
    }
}

void AmpEditor::mouseMove (const MouseEvent& e)
{
    const auto geo = geometry();
    const Point<float> hs[3] = { geo.p1, geo.p2, geo.p4 };
    int h = -1;
    for (int i = 0; i < 3; ++i) if (hs[i].getDistanceFrom (e.position) < 12.0f) h = i;
    if (h != hover) { hover = h; setMouseCursor (h >= 0 ? MouseCursor::NormalCursor : MouseCursor::NormalCursor); }
}

void AmpEditor::mouseDown (const MouseEvent& e)
{
    mouseMove (e);
    dragging = hover;
    const char* ids[3][2] = { { ParamID::ampA, nullptr }, { ParamID::ampD, ParamID::ampS }, { ParamID::ampR, nullptr } };
    if (dragging >= 0)
        for (auto* id : ids[dragging]) if (id != nullptr) if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
}

void AmpEditor::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const auto geo = geometry();
    if (dragging == 0)
        setValue (ParamID::ampA, jlimit (0.0f, 10000.0f, envTime (e.position.x - geo.p0.x - 4.0f, 10000.0f, geo.maxW)));
    else if (dragging == 1)
    {
        setValue (ParamID::ampD, jlimit (1.0f, 10000.0f, envTime (e.position.x - geo.p1.x, 10000.0f, geo.maxW)));
        setValue (ParamID::ampS, jlimit (0.0f, 1.0f, (geo.a.getBottom() - e.position.y) / geo.a.getHeight()));
    }
    else
        setValue (ParamID::ampR, jlimit (5.0f, 10000.0f, envTime (e.position.x - geo.p3.x, 10000.0f, geo.maxW)));
    repaint();
}

void AmpEditor::mouseUp (const MouseEvent&)
{
    const char* ids[3][2] = { { ParamID::ampA, nullptr }, { ParamID::ampD, ParamID::ampS }, { ParamID::ampR, nullptr } };
    if (dragging >= 0)
        for (auto* id : ids[dragging]) if (id != nullptr) if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture();
    dragging = -1;
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
    g.setColour (on ? (over ? Palette::hi : Palette::text) : Palette::textDim.withAlpha (over ? 0.9f : 0.6f));
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
    Digit (int i, int shown, RouteButtons& o) : Button (String (shown + 1)), index (i), owner (o)
    {
        setClickingTogglesState (true);
        setMouseCursor (MouseCursor::NormalCursor);
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
                g.setColour (Palette::theme >= 5 ? Palette::field : Palette::screen.withAlpha (0.7f));
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
        g.setColour (on ? (Palette::theme >= 5 ? Palette::onAccent() : Palette::screen) : Palette::textDim.withAlpha (over ? 1.0f : 0.7f));
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
    // shown order 1 2 3 4 = ADDITIVE, SUB, SAMPLE, NOISE; internal index 0, 3 (sub), 1, 2
    const int internal[] = { 0, 3, 1, 2 };
    for (int shown = 0; shown < 4; ++shown)
    {
        const int o = internal[shown];
        auto* d = digits.add (new Digit (o, shown, *this));
        d->setTooltip (blockName + " on OSC " + String (shown + 1) + ".\nLit = this oscillator goes through " + blockName + ".");
        addAndMakeVisible (d);
        atts.add (new AudioProcessorValueTreeState::ButtonAttachment (p.apvts, ParamID::route (block, o), *d));
    }
}

RouteButtons::~RouteButtons() { atts.clear(); }

void RouteButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / jmax (1, digits.size());
    for (auto* d : digits) d->setBounds (r.removeFromLeft (w));
}

// ============================================================================
// Effect tab

FxTab::FxTab (GranulumProcessor& p, const char* onParam, const String& n)
    : param (p.apvts.getParameter (onParam)), name (n)
{
    setMouseCursor (MouseCursor::NormalCursor);
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
    setMouseCursor (overLed (e.position) ? MouseCursor::NormalCursor : MouseCursor::NormalCursor);
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
    setMouseCursor (MouseCursor::NormalCursor);
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
    {
        // "BASS / MY SOUND": the folder small and quiet, the name as before
        const auto name = proc.getPresetName().toUpperCase();
        const auto folder = proc.getPresetFolderName().toUpperCase();
        auto text = r.reduced (8.0f, 0.0f).withTrimmedRight (8.0f);
        // long names: the font gets a bit smaller before anything is cut
        float fs = 13.0f;
        while (fs > 9.5f && (float) GlyphArrangement::getStringWidthInt (uiFont (fs, false, 0.04f), name) > text.getWidth()) fs -= 0.5f;
        const auto nf = uiFont (fs, false, fs < 13.0f ? 0.04f : 0.08f), ff = uiFont (9.0f, false, 0.06f);
        const float nw = (float) GlyphArrangement::getStringWidthInt (nf, name);
        float fw = folder.isEmpty() ? 0.0f : (float) GlyphArrangement::getStringWidthInt (ff, folder + " / ");
        if (nw + fw > text.getWidth()) fw = 0.0f;   // no room: the name wins (the folder is still in the menu)
        auto group = text.withSizeKeepingCentre (jmin (text.getWidth(), nw + fw), text.getHeight());
        if (fw > 0.0f)
        {
            g.setColour (Palette::textDim.withAlpha (0.8f));
            g.setFont (ff);
            g.drawText (folder + " / ", group.removeFromLeft (fw), Justification::centredLeft, true);
        }
        g.setColour (Palette::theme >= 5 ? Palette::onScreen : Palette::text);
        g.setFont (nf);
        g.drawText (name, group, fw > 0.0f ? Justification::centredLeft : Justification::centred, true);
    }
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
    const auto now = proc.currentPresetFile();
    for (int i = 0; i < list.size(); ++i)
        if (list[i] == now) cur = i;
    const int n = list.size();
    const int idx = cur < 0 ? (dir > 0 ? 0 : n - 1) : ((cur + dir) % n + n) % n;
    proc.loadPreset (list[idx]);
    if (onLoaded) onLoaded();
    repaint();
}

void PresetBar::showMenu()
{
    // 0.99.3: the preset list is a window in the same style as SAVE (folders left, presets right)
    auto* top = findParentComponentOfClass<AudioProcessorEditor>();
    if (top == nullptr) return;
    saveWindow = std::make_unique<PresetSaveWindow> (proc, true);
    Component::SafePointer<PresetBar> safe (this);
    saveWindow->onClose = [safe]
    {
        if (safe == nullptr) return;
        MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->saveWindow.reset(); safe->repaint(); } });
    };
    saveWindow->onLoaded = [safe] { if (safe != nullptr) { if (safe->onLoaded) safe->onLoaded(); safe->repaint(); } };
    saveWindow->onSaved = [safe]
    {
        // SAVE AS... from the list: swap to the SAVE window
        if (safe == nullptr) return;
        MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->saveWindow.reset(); safe->askSave(); } });
    };
    top->addAndMakeVisible (*saveWindow);
    saveWindow->setBounds (top->getLocalBounds());
    saveWindow->open();
}

void PresetBar::askSave()
{
    auto* top = findParentComponentOfClass<AudioProcessorEditor>();
    if (top == nullptr) return;
    saveWindow = std::make_unique<PresetSaveWindow> (proc);
    Component::SafePointer<PresetBar> safe (this);
    saveWindow->onClose = [safe]
    {
        if (safe == nullptr) return;
        MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->saveWindow.reset(); safe->repaint(); } });
    };
    saveWindow->onSaved = [safe] { if (safe != nullptr) safe->repaint(); };
    top->addAndMakeVisible (*saveWindow);
    saveWindow->setBounds (top->getLocalBounds());
    saveWindow->open();
}

// ============================================================================
// Preset SAVE window (0.99.2)

void PresetSaveWindow::Rows::paint (Graphics& g)
{
    for (int i = 0; i < items.size(); ++i)
    {
        auto r = Rectangle<float> (0.0f, (float) (i * rowH), (float) getWidth(), (float) rowH).reduced (2.0f, 1.0f);
        const bool sel = i == selected, hov = i == hover;
        if (sel || hov)
        {
            g.setColour (sel ? Palette::accent.withAlpha (0.16f) : Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        if (sel)
        {
            g.setColour (Palette::accent.withAlpha (0.7f));
            g.fillRoundedRectangle (r.withWidth (2.5f), 1.2f);
        }
        auto tr = r.withTrimmedLeft (10.0f);
        if (folders)
        {
            // little folder glyph
            auto ic = tr.removeFromLeft (14.0f).withSizeKeepingCentre (12.0f, 9.0f);
            Path p;
            p.addRoundedRectangle (ic.getX(), ic.getY() + 1.5f, ic.getWidth(), ic.getHeight() - 1.5f, 1.5f);
            p.addRoundedRectangle (ic.getX(), ic.getY(), ic.getWidth() * 0.45f, 3.0f, 1.0f);
            g.setColour ((sel ? Palette::accent : Palette::textDim).withAlpha (sel ? 0.9f : 0.6f));
            g.fillPath (p);
            tr.removeFromLeft (6.0f);
        }
        g.setColour (sel ? Palette::text : Palette::text.withAlpha (hov ? 0.95f : 0.78f));
        g.setFont (uiFont (12.0f, sel, 0.02f));
        g.drawText (items[i], tr, Justification::centredLeft, true);
    }
}

void PresetSaveWindow::Rows::mouseMove (const MouseEvent& e)
{
    const int h = e.y / rowH;
    const int nh = h < items.size() ? h : -1;
    if (nh != hover) { hover = nh; repaint(); }
}

void PresetSaveWindow::Rows::mouseDown (const MouseEvent& e)
{
    const int i = e.y / rowH;
    if (i < 0 || i >= items.size()) return;
    if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (i); return; }
    if (onClick) onClick (i);
}

void PresetSaveWindow::Rows::mouseDoubleClick (const MouseEvent& e)
{
    const int i = e.y / rowH;
    if (i >= 0 && i < items.size() && onDoubleClick) onDoubleClick (i);
}

PresetSaveWindow::PresetSaveWindow (GranulumProcessor& p, bool browseMode) : browse (browseMode), proc (p)
{
    setWantsKeyboardFocus (true);
    folderRows.folders = true;
    for (auto* v : { &folderView, &presetView })
    {
        v->setScrollBarsShown (true, false);
        v->setScrollBarThickness (6);
        addAndMakeVisible (*v);
    }
    folderView.setViewedComponent (&folderRows, false);
    presetView.setViewedComponent (&presetRows, false);
    for (auto* c : { (Component*) &nameEdit, (Component*) &newFolder, (Component*) &saveBtn, (Component*) &cancelBtn, (Component*) &newVerBtn }) addAndMakeVisible (*c);
    newVerBtn.onClick = [this] { doSaveNewVersion(); };
    if (browse)
    {
        nameEdit.setVisible (false);
        saveBtn.setVisible (false);
        newVerBtn.setVisible (false);
        cancelBtn.setButtonText ("CLOSE");
        for (auto* b : { &initBtn, &openDirBtn, &saveAsBtn, &deleteBtn, &renameBtn }) addAndMakeVisible (*b);
        deleteBtn.onClick = [this] { if (presetRows.selected >= 0) askDeletePreset (presetRows.selected); };
        renameBtn.onClick = [this] { if (presetRows.selected >= 0) startPresetRename (presetRows.selected); };
        deleteBtn.setTooltip ("Delete the selected preset (asks first)");
        renameBtn.setTooltip ("Rename the selected preset");
        openDirBtn.setTooltip ("Open this folder on your computer");
        initBtn.onClick = [this] { proc.resetToInit(); presetRows.selected = -1; presetRows.repaint(); if (onLoaded) onLoaded(); };
        openDirBtn.onClick = [this] { folderFile (folderRows.selected).startAsProcess(); };
        saveAsBtn.onClick = [this] { if (onSaved) onSaved(); };
        initBtn.setTooltip ("Start from a clean sound");
    }
    addChildComponent (folderEdit);
    addChildComponent (presetEdit);
    presetEdit.onReturnKey = [this] { finishPresetRename (true); };
    presetEdit.onEscapeKey = [this] { finishPresetRename (false); };
    presetEdit.onFocusLost = [this] { finishPresetRename (true); };
    for (auto* te : { &nameEdit, &folderEdit, &presetEdit })
    {
        te->setFont (uiFont (13.0f, false, 0.02f));
        te->setJustification (Justification::centredLeft);
        te->setIndents (8, 4);
    }
    nameEdit.setTextToShowWhenEmpty ("Preset name", Palette::textDim);
    nameEdit.onTextChange = [this] { updateSaveLabel(); };
    nameEdit.onReturnKey  = [this] { doSave(); };
    nameEdit.onEscapeKey  = [this] { if (onClose) onClose(); };
    folderEdit.onReturnKey = [this] { finishFolderEdit (true); };
    folderEdit.onEscapeKey = [this] { finishFolderEdit (false); };
    folderEdit.onFocusLost = [this] { finishFolderEdit (true); };

    newFolder.setTooltip ("Make a new folder for your presets");
    newFolder.onClick = [this] { startFolderEdit (-1); };
    saveBtn.onClick   = [this] { doSave(); };
    cancelBtn.onClick = [this] { if (onClose) onClose(); };

    folderRows.onClick = [this] (int i) { folderRows.selected = i; folderRows.repaint(); refreshPresets(); updateSaveLabel(); };
    folderRows.onDoubleClick = [this] (int i) { if (i > 0) startFolderEdit (i); };
    folderRows.onRightClick = [this] (int i)
    {
        folderRows.selected = i; folderRows.repaint(); refreshPresets(); updateSaveLabel();
        PopupMenu m;
        const bool sub = i > 0;
        const bool empty = sub && folderFile (i).findChildFiles (File::findFilesAndDirectories, false).isEmpty();
        m.addItem (1, "Rename", sub);
        m.addItem (2, empty || ! sub ? String ("Delete") : String ("Delete (only empty folders)"), sub && empty);
        m.addItem (3, "Show in folder");
        Component::SafePointer<PresetSaveWindow> safe (this);
        m.showMenuAsync (PopupMenu::Options(), [safe, i] (int res)
        {
            if (safe == nullptr) return;
            if (res == 1) safe->startFolderEdit (i);
            if (res == 2) { safe->folderFile (i).deleteRecursively(); safe->refreshFolders ({}); }
            if (res == 3) safe->folderFile (i).startAsProcess();
        });
    };
    presetRows.onClick = [this] (int i)
    {
        presetRows.selected = i; presetRows.repaint();
        if (browse) loadRow (i);        // one click = load and hear it, the window stays open
        else nameEdit.setText (presetRows.items[i], true);
    };
    presetRows.onDoubleClick = [this] (int i) { if (browse) { loadRow (i); if (onClose) onClose(); } };
    presetRows.onRightClick = [this] (int i)
    {
        presetRows.selected = i; presetRows.repaint();
        PopupMenu m;
        m.addItem (2, "Rename");
        m.addItem (1, "Delete");
        Component::SafePointer<PresetSaveWindow> safe (this);
        m.showMenuAsync (PopupMenu::Options(), [safe, i] (int res)
        {
            if (safe == nullptr) return;
            if (res == 1) safe->askDeletePreset (i);
            if (res == 2) safe->startPresetRename (i);
        });
    };
}

void PresetSaveWindow::loadRow (int i)
{
    const auto f = folderFile (folderRows.selected).getChildFile (presetRows.items[i] + ".gpreset");
    if (f.existsAsFile() && proc.loadPreset (f) && onLoaded) onLoaded();
}

void PresetSaveWindow::open()
{
    const auto cur = proc.getPresetFolderName();
    refreshFolders (cur.isEmpty() ? String ("\x01root") : cur);
    nameEdit.setText (proc.getPresetName() == "INIT" ? String ("My Sound") : proc.getPresetName(), false);
    updateSaveLabel();
    resized();
    if (browse)
    {
        presetRows.selected = presetRows.items.indexOf (proc.getPresetName());
        presetRows.repaint();
        grabKeyboardFocus();
        return;
    }
    nameEdit.grabKeyboardFocus();
    nameEdit.selectAll();
}

File PresetSaveWindow::folderFile (int index) const
{
    return index <= 0 || index >= folders.size() ? GranulumProcessor::presetFolder() : folders[index];
}

void PresetSaveWindow::refreshFolders (const String& select)
{
    folders.clear();
    folders.add (GranulumProcessor::presetFolder());
    StringArray names { "Presets" };
    for (auto& d : GranulumProcessor::listPresetFolders()) { folders.add (d); names.add (d.getFileName()); }
    folderRows.setItems (names);
    int sel = 0;
    for (int i = 1; i < folders.size(); ++i) if (folders[i].getFileName() == select) sel = i;
    folderRows.selected = sel;
    refreshPresets();
    updateSaveLabel();
}

void PresetSaveWindow::refreshPresets()
{
    StringArray names;
    for (auto& f : GranulumProcessor::presetsIn (folderFile (folderRows.selected))) names.add (f.getFileNameWithoutExtension());
    presetRows.setItems (names);
    presetRows.selected = names.indexOf (browse ? proc.getPresetName() : nameEdit.getText().trim());
    if (browse && folderFile (folderRows.selected) != proc.currentPresetFile().getParentDirectory()) presetRows.selected = -1;
    presetView.setViewPosition (0, 0);
}

void PresetSaveWindow::updateSaveLabel()
{
    const auto name = File::createLegalFileName (nameEdit.getText().trim());
    const bool exists = name.isNotEmpty() && folderFile (folderRows.selected).getChildFile (name + ".gpreset").existsAsFile();
    saveBtn.setButtonText ("SAVE");   // 0.99.9: always SAVE (the tooltip says when it replaces)
    saveBtn.setColour (TextButton::textColourOffId, Palette::text);
    saveBtn.setTooltip (exists ? "A preset with this name is already in this folder: it will be replaced" : "Save into the selected folder");
    saveBtn.setEnabled (name.isNotEmpty());
    newVerBtn.setEnabled (name.isNotEmpty());
    newVerBtn.setTooltip (name.isNotEmpty() ? "Save a new copy as \"" + nextVersionName() + "\" (nothing is replaced)" : String());
    if (! browse) presetRows.selected = presetRows.items.indexOf (name);
    presetRows.repaint();
    repaint();
}

void PresetSaveWindow::startFolderEdit (int renameIndex)
{
    renaming = renameIndex;
    folderEdit.setText (renameIndex >= 1 ? folders[renameIndex].getFileName() : String ("New Folder"), false);
    folderEdit.setVisible (true);
    resized();
    folderEdit.grabKeyboardFocus();
    folderEdit.selectAll();
}

void PresetSaveWindow::finishFolderEdit (bool accept)
{
    if (renaming == -2) return;
    const int which = renaming;
    renaming = -2;
    folderEdit.setVisible (false);
    const auto name = File::createLegalFileName (folderEdit.getText().trim());
    String select = folderRows.selected > 0 ? folders[folderRows.selected].getFileName() : String();
    if (accept && name.isNotEmpty())
    {
        auto target = GranulumProcessor::presetFolder().getChildFile (name);
        if (which == -1) { target.createDirectory(); select = name; }
        else if (which >= 1 && which < folders.size() && ! target.exists())
        {
            const auto from = folders[which].getFileName();
            if (folders[which].moveFileTo (target)) proc.presetFolderRenamed (from, name);   // keeps the preset bar right
            select = name;
        }
    }
    refreshFolders (select);
    nameEdit.grabKeyboardFocus();
}

// TEST: delete asks first (a small menu right at the button: Delete "name" / Cancel); the file goes to the bin when it can
void PresetSaveWindow::askDeletePreset (int i)
{
    if (i < 0 || i >= presetRows.items.size()) return;
    PopupMenu m;
    m.addSectionHeader ("Delete \"" + presetRows.items[i] + "\"?");
    m.addItem (1, "Delete");
    m.addItem (2, "Cancel");
    Component::SafePointer<PresetSaveWindow> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&deleteBtn), [safe, i] (int res)
    {
        if (safe == nullptr || res != 1 || i >= safe->presetRows.items.size()) return;
        auto f = safe->folderFile (safe->folderRows.selected).getChildFile (safe->presetRows.items[i] + ".gpreset");
        if (! f.moveToTrash()) f.deleteFile();
        safe->presetRows.selected = -1;
        safe->refreshPresets();
        safe->updateSaveLabel();
    });
}

void PresetSaveWindow::startPresetRename (int i)
{
    if (i < 0 || i >= presetRows.items.size()) return;
    presetRenaming = i;
    presetEdit.setText (presetRows.items[i], false);
    presetEdit.setVisible (true);
    resized();
    presetEdit.grabKeyboardFocus();
    presetEdit.selectAll();
}

void PresetSaveWindow::finishPresetRename (bool accept)
{
    if (presetRenaming < 0) return;
    const int i = presetRenaming;
    presetRenaming = -1;
    presetEdit.setVisible (false);
    resized();
    if (! accept || i >= presetRows.items.size()) return;
    const auto from = presetRows.items[i];
    const auto to = File::createLegalFileName (presetEdit.getText().trim());
    if (to.isEmpty() || to == from) return;
    const auto dir = folderFile (folderRows.selected);
    const auto target = dir.getChildFile (to + ".gpreset");
    if (target.exists())
    {
        PopupMenu m;
        m.addSectionHeader ("\"" + to + "\" already exists");
        m.addItem (1, "OK");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&renameBtn), [] (int) {});
        return;
    }
    if (dir.getChildFile (from + ".gpreset").moveFileTo (target))
        proc.presetRenamed (folderRows.selected > 0 ? dir.getFileName() : String(), from, to);
    refreshPresets();
    for (int k = 0; k < presetRows.items.size(); ++k) if (presetRows.items[k] == to) presetRows.selected = k;
    presetRows.repaint();
}

void PresetSaveWindow::doSave()
{
    const auto name = nameEdit.getText().trim();
    if (File::createLegalFileName (name).isEmpty()) return;
    proc.savePreset (name, folderFile (folderRows.selected));
    if (onSaved) onSaved();
    if (onClose) onClose();
}

// NEW VERSION: BASS -> BASS_02, BASS_02 -> BASS_03, BASS_9 -> BASS_10, BASS_099 -> BASS_100; always at least 2 digits; skips names that exist
String PresetSaveWindow::nextVersionName() const
{
    const auto name = File::createLegalFileName (nameEdit.getText().trim());
    if (name.isEmpty()) return {};
    String base = name;
    int n = 2, digits = 2;
    const int us = name.lastIndexOfChar ('_');
    if (us > 0 && us < name.length() - 1 && name.substring (us + 1).containsOnly ("0123456789"))
    {
        base = name.substring (0, us);
        digits = jmax (2, name.length() - us - 1);
        n = name.substring (us + 1).getIntValue() + 1;
    }
    const auto dir = folderFile (folderRows.selected);
    for (;; ++n)
    {
        const auto cand = base + "_" + String (n).paddedLeft ('0', digits);
        if (! dir.getChildFile (cand + ".gpreset").existsAsFile()) return cand;
    }
}

void PresetSaveWindow::doSaveNewVersion()
{
    const auto name = nextVersionName();
    if (name.isEmpty()) return;
    proc.savePreset (name, folderFile (folderRows.selected));
    if (onSaved) onSaved();
    if (onClose) onClose();
}

Rectangle<int> PresetSaveWindow::panel() const
{
    return getLocalBounds().withSizeKeepingCentre (jmin (getWidth() - 40, 560), jmin (getHeight() - 40, 400));
}

void PresetSaveWindow::resized()
{
    auto r = panel().reduced (18, 16);
    r.removeFromTop (30);                                    // title
    auto bottom = r.removeFromBottom (34);
    r.removeFromBottom (12);
    auto left = r.removeFromLeft (180);
    r.removeFromLeft (12);
    // left: folders + "+ FOLDER"
    auto lb = left.removeFromBottom (28);
    newFolder.setBounds (lb);
    left.removeFromBottom (6);
    if (folderEdit.isVisible()) { folderEdit.setBounds (left.removeFromBottom (26)); left.removeFromBottom (4); }
    left.removeFromTop (18);
    folderView.setBounds (left);
    folderRows.setSize (left.getWidth() - (folderRows.getHeight() > left.getHeight() ? 8 : 0), folderRows.getHeight());
    // right: presets of that folder
    r.removeFromTop (18);
    if (presetEdit.isVisible()) { presetEdit.setBounds (r.removeFromBottom (26)); r.removeFromBottom (4); }
    presetView.setBounds (r);
    presetRows.setSize (r.getWidth() - (presetRows.getHeight() > r.getHeight() ? 8 : 0), presetRows.getHeight());
    if (browse)
    {
        cancelBtn.setBounds (bottom.removeFromRight (86));
        bottom.removeFromRight (8);
        saveAsBtn.setBounds (bottom.removeFromRight (96));
        bottom.removeFromRight (8);
        initBtn.setBounds (bottom.removeFromLeft (62));
        bottom.removeFromLeft (8);
        openDirBtn.setBounds (bottom.removeFromLeft (72));
        bottom.removeFromLeft (8);
        deleteBtn.setBounds (bottom.removeFromLeft (72));
        bottom.removeFromLeft (8);
        renameBtn.setBounds (bottom.removeFromLeft (76));
        return;
    }
    // bottom: name, CANCEL, SAVE, NEW VERSION
    newVerBtn.setBounds (bottom.removeFromRight (112));
    bottom.removeFromRight (8);
    saveBtn.setBounds (bottom.removeFromRight (96));
    bottom.removeFromRight (8);
    cancelBtn.setBounds (bottom.removeFromRight (86));
    bottom.removeFromRight (12);
    nameEdit.setBounds (bottom);
}

void PresetSaveWindow::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.55f));
    const auto p = panel().toFloat();
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (p.translated (0.0f, 4.0f).expanded (2.0f), 14.0f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (p, 12.0f);
    g.setColour (Palette::accent.withAlpha (0.35f));
    g.drawRoundedRectangle (p.reduced (0.5f), 12.0f, 1.0f);

    auto r = panel().reduced (18, 16).toFloat();
    g.setColour (Palette::text);
    g.setFont (uiFont (15.0f, true, 0.12f));
    g.drawText (browse ? "PRESETS" : "SAVE PRESET", r.removeFromTop (30.0f), Justification::centredLeft, false);
    if (browse)
    {
        g.setColour (Palette::textDim);
        g.setFont (uiFont (9.5f, false, 0.04f));
        g.drawText ("click = listen   double-click = load and close", panel().reduced (18, 16).toFloat().removeFromTop (30.0f), Justification::centredRight, false);
    }
    g.setColour (Palette::textDim);
    g.setFont (uiFont (9.5f, true, 0.12f));
    const auto lv = folderView.getBounds().toFloat(), pv = presetView.getBounds().toFloat();
    g.drawText ("FOLDERS", lv.withY (lv.getY() - 18.0f).withHeight (16.0f), Justification::centredLeft, false);
    const auto fname = folderRows.selected > 0 ? folders[folderRows.selected].getFileName().toUpperCase() : String ("PRESETS");
    g.drawText ("IN " + fname + "  (" + String (presetRows.items.size()) + ")", pv.withY (pv.getY() - 18.0f).withHeight (16.0f), Justification::centredLeft, false);
    // list wells
    for (auto b : { lv, pv })
    {
        g.setColour (Palette::screen);
        g.fillRoundedRectangle (b.expanded (2.0f), 6.0f);
    }
    if (presetRows.items.isEmpty())
    {
        g.setColour (Palette::textDim.withAlpha (0.6f));
        g.setFont (uiFont (11.0f, false, 0.02f));
        g.drawText ("No presets in this folder yet", pv, Justification::centred, false);
    }
}

void PresetSaveWindow::mouseDown (const MouseEvent& e)
{
    if (! panel().contains (e.getPosition()) && onClose) onClose();
}

bool PresetSaveWindow::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

// ============================================================================
// OSC 3 noise picker

NoisePicker::NoisePicker (GranulumProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next }) addAndMakeVisible (*b);
    prev.onClick = [this] { setIndex (index() - 1); };
    next.onClick = [this] { setIndex (index() + 1); };
    setMouseCursor (MouseCursor::NormalCursor);
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
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard), lfoEditor (p), ampEditor (p)
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
    add (smallKnobs, ParamID::spread,   "Spread",   Knob::Size::Small);
    add (smallKnobs, ParamID::gmix,     "Mix",      Knob::Size::Small);
    add (smallKnobs, ParamID::gshape,   "Shape",    Knob::Size::Small);   // TEST
    // 1.0.5 SYNC BPM: SIZE and DENSITY read as note values while it is on
    for (auto& k : smallKnobs)
    {
        const bool isSize = k->target == mtSize, isDens = k->target == mtDensity;
        if (! isSize && ! isDens) continue;
        auto* prm = proc.apvts.getParameter (isSize ? ParamID::gsize : ParamID::density);
        auto syncOn = [this] { return proc.apvts.getRawParameterValue (ParamID::grainSync)->load() > 0.5f; };
        k->synced = syncOn;
        auto old = k->slider.textFromValueFunction;
        k->slider.textFromValueFunction = [prm, syncOn, isSize, old] (double val)
        {
            if (! syncOn()) return old ? old (val) : String (val);
            const auto& names = isSize ? GranulumProcessor::syncLenNames() : GranulumProcessor::syncRateNames();
            return names[GranulumProcessor::syncIndex (prm->convertTo0to1 ((float) val), names.size())];
        };
        k->slider.updateText();
    }

    add (osc1Knobs, ParamID::osc1Vol,  "Vol",  Knob::Size::Small);
    add (osc1Knobs, ParamID::osc1Oct,  "Oct",  Knob::Size::Small);
    add (osc1Knobs, ParamID::osc1Semi, "Semi", Knob::Size::Small);
    add (osc1Knobs, ParamID::osc1Tune, "Tune", Knob::Size::Small);
    add (osc1Knobs, ParamID::osc1Pan,  "Pan",  Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Vol,  "Vol",  Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Oct,  "Oct",  Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Semi, "Semi", Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Tune, "Tune", Knob::Size::Small);
    add (osc2Knobs, ParamID::osc2Pan,  "Pan",  Knob::Size::Small);
    add (subKnobs, ParamID::subVol, "Vol", Knob::Size::Small);
    add (subKnobs, ParamID::subOct, "Oct", Knob::Size::Small);
    add (subKnobs, ParamID::subSemi, "Semi", Knob::Size::Small);
    add (subKnobs, ParamID::subTune, "Tune", Knob::Size::Small);   // TEST
    add (osc3Knobs, ParamID::osc3Vol,   "Vol",   Knob::Size::Small);
    add (osc3Knobs, ParamID::osc3Color, "Pitch", Knob::Size::Small);
    add (osc3Knobs, ParamID::osc3Pan,   "Pan",   Knob::Size::Small);

    add (filterKnobs, ParamID::cutoff, "Cutoff");
    add (filterKnobs, ParamID::reso,   "Reso");
    add (filter2Knobs, ParamID::cutoff2, "Cutoff");
    add (filter2Knobs, ParamID::reso2,   "Reso");
    add (peakKnobs, ParamID::peakGain,  "Gain");
    add (peakKnobs, ParamID::peakGain2, "Gain");
    for (auto& k : peakKnobs) { k->setVisible (false); k->slider.setTooltip ("PEAK: lift or cut the CUTOFF frequency (-36..+36 dB)"); }
    add (peakQKnobs, ParamID::peakQ,  "Q");
    add (peakQKnobs, ParamID::peakQ2, "Q");
    for (auto& k : peakQKnobs) { k->setVisible (false); k->slider.setTooltip ("Q: width of the PEAK. Low = wide and soft, high = narrow and sharp"); }
    add (widthKnobs, ParamID::bpWidth,  "Width");
    add (widthKnobs, ParamID::bpWidth2, "Width");
    for (auto& k : widthKnobs) { k->setVisible (false); k->slider.setTooltip ("WIDTH: how wide the BAND PASS is, in octaves. Small = narrow, telephone-like; big = wide and open"); }

    add (distKnobs,  ParamID::distDrive, "Drive",    Knob::Size::Small);
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
    globalKnobs.back()->slider.baseSensitivity = 750;   // 1.0.1: OUTPUT 2.5x finer (Shift / Ctrl: 10x finer still)
    add (globalKnobs, ParamID::pitch,  "Pitch",  Knob::Size::Small);
    globalKnobs[2]->slider.setTooltip ("PITCH: moves the whole synth up or down, smoothly, up to 2 octaves each way (all oscillators, SUB and grains). Double-click = 0");
    for (auto* k : { osc1Knobs[3].get(), osc2Knobs[3].get() })
        k->slider.setTooltip ("TUNE: smooth pitch of this oscillator, -48..+48 semitones, no steps. Put an LFO on it for rises, drops and pitch sweeps. Double-click = 0");

    heroKnobs[0]->slider.setTooltip ("BLOOM: more, longer, wider grains. The sound opens up into a cloud.");
    heroKnobs[1]->slider.setTooltip ("SMEAR: grains melt into long soft strokes with a gentle detune.");
    heroKnobs[2]->slider.setTooltip ("SCATTER: grains jump around the source and across the stereo field.");
    heroKnobs[3]->slider.setTooltip ("SHIMMER: some grains sparkle an octave (or a twelfth) up, some play backwards.");
    heroKnobs[4]->slider.setTooltip ("DRIFT: pitch, position and pan slowly wander. The sound breathes.");
    heroKnobs[5]->slider.setTooltip ("CHIME: every grain rings tuned resonances. Low = soft glassy overtones, high = long metallic bells.");
    osc3Knobs[1]->slider.setTooltip ("COLOR: left = darker, right = brighter");
    for (auto& k : osc3Knobs) k->setCompact (true);
    for (auto* k : { osc1Knobs.back().get(), osc2Knobs.back().get(), osc3Knobs.back().get() })
        k->slider.setTooltip ("PAN: where this oscillator sits left / right. Double-click = centre");
    smallKnobs[4]->slider.setTooltip ("SPREAD: stereo width of the grains. 0 = all in the centre, 100 = wide (grains always take turns left / right)");
    smallKnobs[6]->slider.setTooltip ("SHAPE: the form of each grain. Left = soft round clouds, middle = classic, right = sharp drops (fast attack, falling tail)");
    for (auto& k : osc2Knobs) k->setCompact (true);
    for (auto& k : subKnobs)  k->setCompact (true);

    // lamps
    for (auto* l : { &osc1Lamp, &osc2Lamp, &osc3Lamp }) addAndMakeVisible (*l);
    addAndMakeVisible (filterScope);
    osc1Lamp.fontSize = osc2Lamp.fontSize = osc3Lamp.fontSize = 13.5f;
    osc1LampA   = std::make_unique<ButtonAtt> (s, ParamID::osc1On,   osc1Lamp);
    osc2LampA   = std::make_unique<ButtonAtt> (s, ParamID::osc2On,   osc2Lamp);
    osc3LampA   = std::make_unique<ButtonAtt> (s, ParamID::osc3On,   osc3Lamp);
    osc4LampA   = std::make_unique<ButtonAtt> (s, ParamID::subOn,    osc4Lamp);
    addAndMakeVisible (osc4Lamp);
    addAndMakeVisible (osc1Mode);
    osc1Mode.onChange = [this] { resized(); repaint(); harmonics.repaint(); wavePicker.repaint(); };
    // 1.0.2: OSC 2-4 can no longer be folded (the fold buttons are gone, every oscillator is always open)
    for (int i = 0; i < 3; ++i) { proc.uiFold[i] = false; addChildComponent (foldBtn[i]); }
    addAndMakeVisible (subBars);
    osc4Lamp.setTooltip ("OSC 2 SUB: a clean low layer, never granulated. Filters and effects only where their digit 2 is lit.");

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
    osc2Grain.setTooltip ("Send OSC 3 through the granular block. Off: the sample loops from POSITION");
    osc3Grain.setTooltip ("Send OSC 4 through the granular block. Off: the noise plays clean");
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
    // 1.0.2: < / > step through the filter types of the filter that is shown (FILTER 1 or FILTER 2)
    for (auto* a : { &filtPrev, &filtNext })
    {
        addAndMakeVisible (*a);
        a->setTooltip (a->dir < 0 ? "Previous filter type" : "Next filter type");
        a->onClick = [this, a]
        {
            auto& box = filterType2.isVisible() ? filterType2 : filterType;
            const int n = box.getNumItems();
            if (n <= 0) return;
            box.setSelectedItemIndex ((box.getSelectedItemIndex() + a->dir + n) % n, sendNotificationSync);
        };
    }
    filterType.getProperties().set ("noArrow", true);
    filterType2.getProperties().set ("noArrow", true);
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
            filterTabs[(size_t) i]->setRepaintsOnMouseActivity (true);
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
        fxTabs[(size_t) i]->setRepaintsOnMouseActivity (true);
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
    addChildComponent (eqView);
    addAndMakeVisible (viewSwitch);
    addChildComponent (eqFlat);
    eqFlat.setTooltip ("Reset the whole EQ (every band back to 0 dB)");
    eqFlat.onClick = [this] { eqView.flat(); };
    addChildComponent (compView);
    addAndMakeVisible (voiceBox);
    viewSwitch.onChange = [this] (int view)
    {
        // the screen shows the grains, the master EQ or the compressor (0.99.9)
        eqMode = view == 1;
        field.setVisible (view == 0);
        loopBar.setVisible (view == 0);
        eqView.setVisible (view == 1);
        eqFlat.setVisible (view == 1);
        compView.setVisible (view == 2);
        repaint();
    };
    addAndMakeVisible (harmonics);
    addAndMakeVisible (noisePicker);
    addAndMakeVisible (meterView);
    addAndMakeVisible (presetBar);
    presetBar.onLoaded = [this] { lfoEditor.setLfo (selectedLfo); };

    for (juce::TextButton* b : { &rndGrains, &rndFx, &rndBars, &randomButton, static_cast<juce::TextButton*> (&initButton), &loadButton, &chordButton, &undoButton }) addAndMakeVisible (*b);
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    undoButton.setTooltip ("Undo the last change (Ctrl+Z). Ctrl+Shift+Z = redo");
    addMouseListener (this, true);   // every click starts a new undo step
    setWantsKeyboardFocus (true);
    chordButton.getProperties().set ("sounds", 4);   // copper with a glint, like SOUNDS
    chordButton.getProperties().set ("glintOffset", 0.9);
    rndGrains.onClick    = [this] { proc.randomizeSection (0); };
    rndFx.onClick        = [this] { proc.randomizeSection (2); };
    rndBars.onClick      = [this] { proc.randomizeSection (3); };
    randomButton.onClick = [this] { proc.randomize(); };
    addAndMakeVisible (randomMenuButton);
    randomMenuButton.setTooltip ("What RANDOM ALL may change");
    randomMenuButton.onClick = [this] { showRandomMenu(); };
    {
        auto syncRandomLabel = [this] { const int t = proc.randType.load(); randomButton.setButtonText ("RANDOM " + (t == 0 ? String ("ALL") : GranulumProcessor::randTypeNames()[t])); };
        syncRandomLabel();
        randomPanel.onChange = syncRandomLabel;
        addAndMakeVisible (titleBadge);
        titleBadge.onClick = [this] { about.version = versionString(); about.setVisible (true); about.toFront (false); };
        addAndMakeVisible (guidePill);
        guidePill.setRepaintsOnMouseActivity (true);
        guidePill.setTooltip ("A short tour of GRAIN FORM");
        guidePill.onClick = [this] { tour.start(); };
        addAndMakeVisible (aboutPill);
        aboutPill.setRepaintsOnMouseActivity (true);
        aboutPill.setTooltip ("Who made GRAIN FORM, version");
        aboutPill.onClick = [this] { about.version = versionString(); about.setVisible (true); about.toFront (false); };
        addChildComponent (about);
        addChildComponent (tour);
        addChildComponent (randomPanel);
        setupTour();
    }
    initButton.onClick   = [this] { proc.resetToInit(); lfoEditor.setLfo (selectedLfo); };
    initButton.onRightClick = [this]
    {
        PopupMenu m;
        m.addItem (1, "Save this sound as INIT");
        if (proc.hasUserInit()) m.addItem (2, "Back to the factory INIT");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&initButton), [this] (int r)
        {
            if (r == 1) proc.saveUserInit();
            if (r == 2) { proc.clearUserInit(); proc.resetToInit(); lfoEditor.setLfo (selectedLfo); }
        });
    };
    loadButton.onClick   = [this] { toggleSampleMenu (! sampleMenu.isVisible()); };
    loadButton.setTooltip ("Factory sounds, or load your own file");
    loadButton.getProperties().set ("sounds", soundsStyle);
    addChildComponent (sampleMenu);
    addAndMakeVisible (loopBar);
    addAndMakeVisible (freezeSwitch);
    addAndMakeVisible (shimmerSwitch);
    addAndMakeVisible (wavePicker);
    sampleMenu.onPick = [this] (const File& f)
    {
        toggleSampleMenu (false);
        if (f == File()) launchChooser(); else openFile (f);
    };
    for (auto* b : { &samplePrev, &sampleNext }) addAndMakeVisible (*b);
    samplePrev.onClick = [this] { proc.loadAdjacentFactorySample (-1); repaint(); };
    sampleNext.onClick = [this] { proc.loadAdjacentFactorySample (+1); repaint(); };
    samplePrev.setTooltip ("Previous factory sample");
    sampleNext.setTooltip ("Next factory sample. Or drop your own WAV here.");
    chordButton.onClick  = [this] { showChordPicker(); };
    initButton.setTooltip ("Start from scratch. Right click: save the current sound as your own INIT (or go back to the factory one)");
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
        c->mode.addItemList ({ "FREE", "RETRIG", "ENV" }, 1);
        c->sync.setClickingTogglesState (true);
        c->sync.setTooltip ("Lock the LFO to the project tempo");
        c->remove.setTooltip ("Removes this LFO from every knob it modulates");
        c->remove.onClick = [this, l]
        {
            proc.removeLfoEverywhere (l);
        };
        c->mode.setTooltip ("FREE: runs on its own.  RETRIG: restarts with each note.  ENV: runs once per note and holds.");
        c->rate = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rate"), "Rate", Knob::Size::Small);
        c->div  = std::make_unique<Knob> (proc, ParamID::lfo (l, "Div"),  "Rate", Knob::Size::Small);
        c->div->slider.setTooltip ("RATE in note values (SYNC is on). Turn it, or switch SYNC off for Hz");
        c->rate->slider.setTooltip ("RATE in Hz (SYNC is off). Switch SYNC on for note values");
        c->grains.setClickingTogglesState (true);
        c->grains.setTooltip ("INTO GRAINS: the grains cut up this LFO's rhythm (best with a fast LFO on an OSC VOL)");
        c->grainsA = std::make_unique<ButtonAtt> (s, ParamID::lfo (l, "Grains"), c->grains);
        c->rise = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rise"), "Rise", Knob::Size::Small);
        c->rise->slider.setTooltip ("RISE: the LFO fades in over this time after a note starts");
        allKnobs.push_back (c->rate.get());
        c->modeA = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Mode"), c->mode);
        c->syncA = std::make_unique<ButtonAtt> (s, ParamID::lfo (l, "Sync"), c->sync);
        for (Component* comp : { (Component*) &c->preset, (Component*) &c->mode, (Component*) &c->sync, (Component*) &c->grains,
                                 (Component*) &c->remove, (Component*) c->rate.get(), (Component*) c->div.get(), (Component*) c->rise.get() })
            addChildComponent (comp);
        lfoControls[(size_t) l] = std::move (c);
    }
    addAndMakeVisible (lfoEditor);
    addChildComponent (ampEditor);
    ampTab.param = proc.apvts.getParameter (ParamID::ampOn);
    addAndMakeVisible (ampTab);
    ampTab.onSelect = [this] { selectLfo (3); };
    addAndMakeVisible (linksTab);
    linksTab.onSelect = [this] { selectLfo (4); };
    addChildComponent (linksView);
    for (auto [id, nm] : { std::pair<const char*, const char*> { ParamID::ampA, "A" }, { ParamID::ampD, "D" },
                           { ParamID::ampS, "S" }, { ParamID::ampR, "R" } })
    {
        ampKnobs.push_back (std::make_unique<Knob> (proc, id, nm, Knob::Size::Small));
        ampKnobs.back()->setCompact (true, this);     // value pops up while turning (inside the plugin window)
        addChildComponent (*ampKnobs.back());
    }
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
    getConstrainer()->setFixedAspectRatio (1080.0 / 860.0);   // 860: room for the OSC 4 SUB row
    setResizeLimits (864, 688, 1620, 1290);
    setSize (1080, 860);
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
    const bool amp = i == 3, links = i == 4;
    ampTab.selected = amp;
    ampTab.repaint();
    linksTab.selected = links;
    linksTab.repaint();
    linksView.setVisible (links);
    if (links) linksView.refresh();
    ampEditor.setVisible (amp);
    lfoEditor.setVisible (! amp && ! links);
    for (auto& k : ampKnobs) k->setVisible (amp);
    repaint();
    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l]->selected = (l == i);
        lfoTabs[(size_t) l]->repaint();
        auto& c = *lfoControls[(size_t) l];
        for (Component* comp : { (Component*) &c.preset, (Component*) &c.mode, (Component*) &c.sync, (Component*) &c.grains,
                                 (Component*) &c.remove, (Component*) c.rise.get() })
            comp->setVisible (l == i);
        const bool sync = c.sync.getToggleState();
        c.rate->setVisible (l == i && ! sync);
        c.div->setVisible (l == i && sync);
    }
    if (i < kNumLfos) lfoEditor.setLfo (i);
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
    filterScope.which = i;
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

// ============================================================================
// SampleMenu

void SampleMenu::rebuild()
{
    rows.clear();
    const auto root = GranulumProcessor::factorySampleDir();
    auto addFiles = [this] (const File& dir, bool recursive)
    {
        auto files = dir.findChildFiles (File::findFiles, recursive, "*.wav;*.flac;*.aif;*.aiff");
        std::sort (files.begin(), files.end(), [] (const File& a, const File& b)
                   { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
        for (auto& f : files) rows.push_back ({ f.getFileNameWithoutExtension().toUpperCase(), f, 1 });
    };
    if (root.isDirectory())
    {
        auto loose = root.findChildFiles (File::findFiles, false, "*.wav;*.flac;*.aif;*.aiff");
        if (! loose.isEmpty()) { rows.push_back ({ "FACTORY", {}, 0 }); addFiles (root, false); }
        auto dirs = root.findChildFiles (File::findDirectories, false);
        dirs.sort();
        for (auto& d : dirs) { rows.push_back ({ d.getFileName().toUpperCase(), {}, 0 }); addFiles (d, true); }
    }
    if (rows.empty()) rows.push_back ({ "NO FACTORY SOUNDS INSTALLED", {}, 0 });
    rows.push_back ({ String::fromUTF8 ("YOUR OWN FILE\xe2\x80\xa6"), {}, 2 });
    hover = -1;
}

float SampleMenu::rowY (int i) const
{
    float y = 6.0f;
    for (int k = 0; k < i; ++k) y += rowH (k) + (rows[(size_t) k + 1].kind != 1 && k + 1 < (int) rows.size() ? 4.0f : 0.0f);
    return y;
}

int SampleMenu::preferredHeight() const
{
    return rows.empty() ? 40 : (int) (rowY ((int) rows.size() - 1) + rowH ((int) rows.size() - 1) + 6.0f);
}

int SampleMenu::rowAt (float y) const
{
    for (int i = 0; i < (int) rows.size(); ++i)
        if (y >= rowY (i) && y < rowY (i) + rowH (i)) return rows[(size_t) i].kind == 0 ? -1 : i;
    return -1;
}

void SampleMenu::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0, 3.0f), 9.0f);
    g.setColour (Palette::panel.brighter (0.06f));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (Palette::edge.withMultipliedAlpha (2.0f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);

    File current;
    { const auto n = proc.getSampleName(); if (n.isNotEmpty() && proc.isFactorySample()) current = GranulumProcessor::factorySampleDir(); }
    for (int i = 0; i < (int) rows.size(); ++i)
    {
        const auto& row = rows[(size_t) i];
        auto rr = Rectangle<float> (6.0f, rowY (i), r.getWidth() - 12.0f, rowH (i));
        if (row.kind == 0)
        {
            g.setColour (Palette::label);
            g.setFont (uiFont (9.5f, false, 0.18f));
            g.drawText (row.text, rr.withTrimmedLeft (8.0f), Justification::centredLeft, false);
            continue;
        }
        if (row.kind == 2)
        {
            g.setColour (Palette::edge.withMultipliedAlpha (2.0f));
            g.drawHorizontalLine ((int) rr.getY() - 3, rr.getX() + 4.0f, rr.getRight() - 4.0f);
        }
        const bool sel = row.kind == 1 && proc.getSampleName() == row.file.getFileName() && proc.isFactorySample();
        if (i == hover)
        {
            g.setColour (Palette::buttonHi);
            g.fillRoundedRectangle (rr, 5.0f);
        }
        if (sel)
        {
            g.setColour (Palette::accent);
            g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ rr.getX() + 11.0f, rr.getCentreY() }));
        }
        g.setColour (row.kind == 2 ? Palette::accentSoft : (sel || i == hover ? Palette::hi : Palette::text));
        g.setFont (uiFont (11.5f, sel, 0.08f));
        g.drawText (row.text, rr.withTrimmedLeft (22.0f), Justification::centredLeft, true);
    }
}

void SampleMenu::mouseMove (const MouseEvent& e)
{
    const int h = rowAt (e.position.y);
    if (h != hover) { hover = h; repaint(); }
}

void SampleMenu::mouseUp (const MouseEvent& e)
{
    const int i = rowAt (e.position.y);
    if (i < 0) return;
    if (onPick) onPick (rows[(size_t) i].kind == 2 ? File() : rows[(size_t) i].file);
}

void GranulumEditor::toggleSampleMenu (bool show)
{
    if (show)
    {
        sampleMenu.rebuild();
        const float sc = (float) getWidth() / 1080.0f;
        const int w = (int) (230 * sc), h = sampleMenu.preferredHeight();
        auto b = loadButton.getBounds();
        int x = jlimit (6, getWidth() - w - 6, b.getRight() - w), y = b.getBottom() + (int) (4 * sc);
        if (y + h > getHeight() - 6) y = b.getY() - h - (int) (4 * sc);
        sampleMenu.setBounds (x, y, w, h);
        sampleMenu.setVisible (true);
        sampleMenu.toFront (false);
    }
    else sampleMenu.setVisible (false);
    loadButton.setToggleState (show, dontSendNotification);
    repaint (dropZone.expanded (4));
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

void GranulumEditor::showRandomMenu()
{
    randomPanel.setBounds (getLocalBounds());
    randomPanel.anchor = randomMenuButton.getBounds();
    randomPanel.open();
}

void GranulumEditor::setFold (int i, bool f)
{
    proc.uiFold[i] = f;
    resized();
    repaint();
}

void GranulumEditor::timerCallback()
{
    // a preset / project can bring its own folded oscillators
    if (foldSig != (int) proc.uiFold[0] + 2 * (int) proc.uiFold[1] + 4 * (int) proc.uiFold[2] + 8 * (int) osc1WaveMode (proc)) { resized(); repaint(); }

   #if JUCE_WINDOWS
    // JUCE 8 draws with Direct2D by default on Windows; with several animated plugin windows that made the whole
    // host stutter. The classic software renderer is light and proven in every host.
    if (! rendererChosen)
        if (auto* peer = getPeer())
        {
            const int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
            if (sw >= 0) peer->setCurrentRenderingEngine (sw);
            rendererChosen = true;
        }
   #endif

    // one tick drives every animated part, so each window repaints once per frame
    field.frame();
    harmonics.frame();
    subBars.frame();
    meterView.frame();
    if (lfoEditor.isVisible()) lfoEditor.frame();
    if (ampEditor.isVisible()) ampEditor.frame();
    filterScope.frame();

    // SOUNDS / CHORD glint: repaint only while the glint is passing (and once after)
    {
        const double t = Time::getMillisecondCounterHiRes() * 0.001;
        auto glinting = [t] (double offset) { return std::fmod (t + offset, 1.8) < 1.0; };
        if (loadButton.isShowing() && glinting (0.0)) loadButton.repaint();
        if (chordButton.isShowing() && glinting (0.9)) chordButton.repaint();
    }
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
    const bool gOn = (o1 && on (ParamID::osc1Grain)) || (o2 && on (ParamID::osc2Grain)) || (o3 && on (ParamID::osc3Grain));
    const bool fOn = on (ParamID::filterOn), f2On = on (ParamID::filter2On);
    const bool dOn = on (ParamID::distOn), lOn = on (ParamID::delayOn), rOn = on (ParamID::revOn);
    dimK (osc1Knobs, o1); dim ({ &harmonics, &chordButton, &rndBars, &wavePicker }, o1);
    {
        const bool o4 = on (ParamID::subOn);
        dimK (subKnobs, o4); dim ({ &subBars }, o4);
    }
    dimK (osc2Knobs, o2);
    dimK (osc3Knobs, o3); dim ({ &noisePicker }, o3);
    dim ({ &osc1Grain }, o1);
    dim ({ &osc2Grain }, o2);
    dim ({ &osc3Grain }, o3);
    dimK (heroKnobs, gOn); dimK (smallKnobs, gOn); dim ({ &rndGrains }, gOn);
    dimK (filterKnobs, fOn); dim ({ &filterType, routes[0].get() }, fOn);
    {
        // PEAK shows GAIN where RESO is
        const bool p1 = (int) s.getRawParameterValue (ParamID::ftype)->load() == FilterUnit::Peak;
        const bool p2 = (int) s.getRawParameterValue (ParamID::ftype2)->load() == FilterUnit::Peak;
        const bool b1 = (int) s.getRawParameterValue (ParamID::ftype)->load() == FilterUnit::BP;
        const bool b2 = (int) s.getRawParameterValue (ParamID::ftype2)->load() == FilterUnit::BP;
        auto vis = [] (Component& c, bool v) { if (c.isVisible() != v) c.setVisible (v); };
        vis (*filterKnobs[1],  selectedFilter == 0 && ! p1);
        vis (*peakKnobs[0],    selectedFilter == 0 && p1);
        vis (*filter2Knobs[1], selectedFilter == 1 && ! p2);
        vis (*peakKnobs[1],    selectedFilter == 1 && p2);
        vis (*peakQKnobs[0],   selectedFilter == 0 && p1);
        vis (*peakQKnobs[1],   selectedFilter == 1 && p2);
        peakKnobs[0]->setAlpha (fOn ? 1.0f : 0.38f);
        peakKnobs[1]->setAlpha (f2On ? 1.0f : 0.38f);
        peakQKnobs[0]->setAlpha (fOn ? 1.0f : 0.38f);
        peakQKnobs[1]->setAlpha (f2On ? 1.0f : 0.38f);
        vis (*widthKnobs[0], selectedFilter == 0 && b1);
        vis (*widthKnobs[1], selectedFilter == 1 && b2);
        widthKnobs[0]->setAlpha (fOn ? 1.0f : 0.38f);
        widthKnobs[1]->setAlpha (f2On ? 1.0f : 0.38f);
        const int l1 = p1 ? 1 : b1 ? 2 : 0, l2 = p2 ? 1 : b2 ? 2 : 0;
        if (l1 != filtLayout[0] || l2 != filtLayout[1]) { filtLayout[0] = l1; filtLayout[1] = l2; peakLayout[0] = p1; peakLayout[1] = p2; layoutFilterKnobs(); }
        filterKnobs[0]->slider.setTooltip (p1 ? "PEAK frequency (does not follow the note)" : "Cutoff follows the note you play (set at C3). The readout shows the note where it cuts.");
    }
    dimK (filter2Knobs, f2On); dim ({ &filterType2, routes[4].get() }, f2On);
    // tabs: repaint only when a lamp changes (repainting them every frame also repaints the window behind them)
    const int tabState = (fOn ? 1 : 0) | (f2On ? 2 : 0) | (on (ParamID::distOn) ? 4 : 0) | (lOn ? 8 : 0) | (rOn ? 16 : 0);
    const bool tabsChanged = tabState != shownTabState;
    shownTabState = tabState;
    if (tabsChanged) for (auto& t : filterTabs) t->repaint();
    dimK (distKnobs, dOn);  dim ({ &distType, routes[1].get() }, dOn);
    dimK (delayKnobs, lOn); dim ({ &delaySync, &delayPing, &delayDiv, routes[2].get() }, lOn);
    dimK (revKnobs, rOn);   dim ({ routes[3].get() }, rOn);
    if (tabsChanged) for (auto& t : fxTabs) t->repaint();

    // delay: TIME knob in free mode, division box when synced
    const bool dsync = on (ParamID::delaySync);
    delayKnobs[0]->setVisible (selectedFx == 1 && ! dsync);
    delayDiv.setVisible (selectedFx == 1 && dsync);

    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& c = *lfoControls[(size_t) l];
        const bool sync = c.sync.getToggleState();
        const bool shown = l == selectedLfo;
        if (c.rate->isVisible() != (shown && ! sync)) c.rate->setVisible (shown && ! sync);   // one RATE knob: Hz or note value
        if (c.div->isVisible()  != (shown && sync))   c.div->setVisible (shown && sync);
        const bool routed = proc.countModulations (l) > 0;
        c.remove.setEnabled (routed);
        c.remove.setAlpha (routed ? 1.0f : 0.35f);
    }

    if (const auto v = proc.modVersion.load(); v != shownModVersion)
    {
        shownModVersion = v;
        for (auto* k : allKnobs) k->refreshMods();
        for (auto& t : lfoTabs) t->repaint();
        linksTab.repaint();
        if (linksView.isVisible()) linksView.refresh();
    }
    for (auto* k : allKnobs) { k->syncTick(); if (k->isShowing()) k->tick(); }
    if (selectedLfo < kNumLfos) lfoTabs[(size_t) selectedLfo]->repaint();   // its lamp pulses with the LFO
    else if (selectedLfo == kNumLfos) repaint (adsrReadout);

    meter    = jmax (proc.outputLevel.load(), meter * 0.85f);
    activity = jmax ((float) proc.vizEngine().vizCount.load() / 20.0f, activity * 0.8f);
    // the sample box: only when something in it can change (hover marquee, drag, new sample, status text)
    {
        const String sig = proc.getSampleName() + String ((int) proc.hasSample()) + String ((int) dropHover) + String ((int) fileHover)
                         + String ((int) sampleMenu.isVisible()) + statusText
                         + String ((int) (proc.apvts.getRawParameterValue (ParamID::osc2On)->load() > 0.5f));
        if (sig != shownDropSig || dropHover) { shownDropSig = sig; repaint (dropZone); }
    }
}

void GranulumEditor::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const float sc = full.getWidth() / 1080.0f;

    const float cheek = Palette::hardware ? 19.0f * sc : 0.0f;
    if (Palette::hardware)
    {
        // brushed, dark-anodised aluminium faceplate (cached)
        if (hwPlate.getWidth() != getWidth() || hwPlate.getHeight() != getHeight())
        {
            hwPlate = Image (Image::RGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
            Graphics ig (hwPlate);
            ig.setGradientFill (ColourGradient (Colour (0xff2e3033), 0, 0, Colour (0xff1d1f21), 0, full.getHeight(), false));
            ig.fillAll();
            Random rnd (98);
            for (int y = 0; y < getHeight(); ++y)
            {
                float x0 = -50.0f;
                while (x0 < full.getWidth())
                {
                    const float len = 40.0f + rnd.nextFloat() * 260.0f;
                    const bool light = rnd.nextBool();
                    ig.setColour ((light ? Colours::white : Colours::black).withAlpha (0.012f + rnd.nextFloat() * (light ? 0.03f : 0.05f)));
                    ig.fillRect (x0, (float) y, len, 1.0f);
                    x0 += len * (0.6f + rnd.nextFloat());
                }
            }
            // soft top light
            ig.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), 0, 0, Colours::transparentWhite, 0, full.getHeight() * 0.45f, false));
            ig.fillAll();
        }
        g.drawImageAt (hwPlate, 0, 0);

        // walnut end cheeks
        for (int side = 0; side < 2; ++side)
        {
            auto w = side == 0 ? full.withWidth (cheek) : full.withLeft (full.getRight() - cheek);
            g.setGradientFill (ColourGradient (Colour (0xff6a3f24), w.getX(), 0, Colour (0xff3a2112), w.getRight(), 0, false));
            g.fillRect (w);
            Random rnd (7 + side);
            for (int k = 0; k < 9; ++k)
            {
                Path grain;
                const float gx = w.getX() + w.getWidth() * (0.08f + 0.84f * rnd.nextFloat());
                const float amp = 0.6f + rnd.nextFloat() * 1.8f, fr = 0.004f + rnd.nextFloat() * 0.01f, ph = rnd.nextFloat() * 6.0f;
                grain.startNewSubPath (gx, 0.0f);
                for (float yy = 0.0f; yy <= full.getHeight(); yy += 6.0f)
                    grain.lineTo (jlimit (w.getX(), w.getRight(), gx + amp * std::sin (yy * fr + ph) + 0.6f * std::sin (yy * 0.05f + ph)), yy);
                g.setColour ((rnd.nextBool() ? Colour (0xff1a0d05) : Colour (0xff7a4a2c)).withAlpha (0.35f + 0.3f * rnd.nextFloat()));
                g.strokePath (grain, PathStrokeType (0.6f + rnd.nextFloat()));
            }
            // varnish sheen + the seam to the metal
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), w.getCentreX(), 0, Colours::transparentWhite, w.getRight(), 0, false));
            g.fillRect (w);
            g.setColour (Colours::black.withAlpha (0.8f));
            g.fillRect (side == 0 ? w.withLeft (w.getRight() - 1.5f) : w.withWidth (1.5f));
        }
        // faceplate edge light + screws in the corners
        auto plate = full.reduced (cheek, 0.0f);
        g.setColour (Colours::white.withAlpha (0.08f));
        g.drawLine (plate.getX(), 1.0f, plate.getRight(), 1.0f, 1.0f);
        auto screw = [&g] (Point<float> p, float rr, float rot)
        {
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillEllipse (Rectangle<float> (rr * 2.4f, rr * 2.4f).withCentre (p.translated (0.0f, 0.8f)));
            g.setGradientFill (ColourGradient (Colour (0xffb9bcc0), p.x - rr, p.y - rr, Colour (0xff4a4d51), p.x + rr, p.y + rr, false));
            g.fillEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (p));
            g.setColour (Colour (0xff1a1b1c));
            g.drawLine (Line<float> (p.getPointOnCircumference (rr * 0.75f, rot), p.getPointOnCircumference (rr * 0.75f, rot + MathConstants<float>::pi)), rr * 0.32f);
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (p).reduced (0.4f), 0.6f);
        };
        const float sr = 4.2f * sc, inset = 9.0f * sc;
        screw ({ plate.getX() + inset, inset }, sr, 0.4f);
        screw ({ plate.getRight() - inset, inset }, sr, 1.1f);
        screw ({ plate.getX() + inset, full.getHeight() - inset }, sr, 2.3f);
        screw ({ plate.getRight() - inset, full.getHeight() - inset }, sr, 0.9f);
        screw ({ plate.getX() + inset, (float) lfoFrame.getY() - 9.0f * sc }, sr, 1.7f);
        screw ({ plate.getRight() - inset, (float) lfoFrame.getY() - 9.0f * sc }, sr, 0.2f);
    }
    else
    {
        g.setGradientFill (ColourGradient (Palette::bgTop, 0, 0, Palette::bgBottom, 0, full.getHeight(), false));
        g.fillAll();
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (full.reduced (1.0f), 14.0f, 1.0f);
    }

    // the name itself is the TitleBadge component (sheen + click for About)
    g.setColour (Palette::textDim);
    g.setFont (uiFont (16.0f * sc, false, 0.3f));
    g.drawText ("SELECTIVE GRANULATION", titleArea.withTrimmedTop ((int) (49 * sc)).withWidth ((int) (270 * sc)), Justification::topLeft, false);

    for (auto fr : { screenFrame, lfoFrame })
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (fr.toFloat().translated (0, 2.0f), 14.0f);
        g.setColour (Palette::hardware ? Palette::panel.withAlpha (0.55f) : Palette::panel);
        g.fillRoundedRectangle (fr.toFloat(), 14.0f);
        if (Palette::hardware)
        {
            // a separate, slightly raised plate: light top edge, dark lower edge
            g.setColour (Colours::white.withAlpha (0.09f));
            g.drawRoundedRectangle (fr.toFloat().reduced (0.5f).translated (0.0f, 0.5f), 14.0f, 1.0f);
            g.setColour (Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (fr.toFloat().expanded (0.5f), 14.5f, 1.0f);
        }
        if (Palette::theme == 8) { g.setColour (Palette::edge); g.drawRoundedRectangle (fr.toFloat().reduced (0.5f), 14.0f, 1.0f); }
    }
    g.setColour (Palette::label);
    g.setFont (uiFont (13.0f * sc, false, 0.12f));
    // TEST: the title is the GRAINS | EQ switch now


    for (auto a : { osc1Area, osc2Area, osc3Area, osc4Area })
    {
        if (Palette::theme == 8) { g.setColour (Palette::edge); g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 10.0f, 1.0f); continue; }
        g.setColour (Colours::black.withAlpha (Palette::theme == 5 || Palette::theme == 7 ? 0.045f : 0.14f));
        g.fillRoundedRectangle (a.toFloat(), 10.0f);
    }

    // 0.99.3: folded OSC 2 / 3 / 4: a slim strip in a copper dashed frame, with the sound's name
    for (int i = 0; i < 3; ++i)
    {
        if (! proc.uiFold[i]) continue;
        const auto a = (i == 0 ? osc4Area : i == 1 ? osc2Area : osc3Area).toFloat();
        Path border; border.addRoundedRectangle (a.reduced (0.75f), 9.0f);
        Path dashed;
        const float dl[] = { 4.0f, 3.0f };
        PathStrokeType (1.1f).createDashedStroke (dashed, border, dl, 2);
        g.setColour (Palette::accent.withAlpha (0.8f));
        g.fillPath (dashed);
        if (foldText[i].isEmpty() || i == 0) continue;   // SUB shows no picture when folded
        String name;
        if (i == 1) name = proc.hasSample() ? File::createFileWithoutCheckingPath (proc.getSampleName()).getFileNameWithoutExtension().toUpperCase() : String ("NO SAMPLE");
        else        name = GranulumProcessor::noiseNames()[(int) proc.apvts.getRawParameterValue (ParamID::osc3Type)->load()].toUpperCase();
        auto t = foldText[i].toFloat();
        g.setColour (Palette::screen.withAlpha (0.9f));
        g.fillRoundedRectangle (t, 4.0f);
        g.setColour (Palette::text.withAlpha (0.85f));
        g.setFont (uiFont (10.5f * sc, true, 0.14f));
        g.drawText (name, t.reduced (8.0f * sc, 0.0f), Justification::centredLeft, true);
    }



    // OSC 2 drop zone: always shows that a file can be dropped here
    {
        auto z = dropZone.toFloat();
        const bool has = proc.hasSample();
        const bool on2 = proc.apvts.getRawParameterValue (ParamID::osc2On)->load() > 0.5f;
        g.setColour (fileHover ? Palette::screen.interpolatedWith (Palette::accent, 0.16f) : Palette::screen.withAlpha (0.85f));
        g.fillRoundedRectangle (z, 9.0f);

        // dashed frame: quiet normally, brighter on hover, copper while a file is dragged over the plugin
        {
            Path border; border.addRoundedRectangle (z.reduced (1.0f), 8.5f);
            Path dashed;
            const float dl[] = { 4.0f, 3.5f };
            PathStrokeType (fileHover ? 1.6f : 1.1f).createDashedStroke (dashed, border, dl, 2);
            g.setColour (fileHover ? Palette::accent : dropHover ? Palette::accentSoft.withAlpha (0.85f) : Palette::textDim.withAlpha (0.38f));
            g.fillPath (dashed);
        }

        // "drop" icon: an arrow into a tray, top-right corner
        auto drawDropIcon = [&] (Point<float> c, float s, Colour col)
        {
            g.setColour (col);
            const float w = s, h = s;
            g.drawLine (c.x, c.y - h * 0.5f, c.x, c.y + h * 0.12f, 1.3f);
            Path head; head.addTriangle (c.x - w * 0.28f, c.y - h * 0.02f, c.x + w * 0.28f, c.y - h * 0.02f, c.x, c.y + h * 0.26f);
            g.fillPath (head);
            Path tray;
            tray.startNewSubPath (c.x - w * 0.5f, c.y + h * 0.12f);
            tray.lineTo (c.x - w * 0.5f, c.y + h * 0.48f);
            tray.lineTo (c.x + w * 0.5f, c.y + h * 0.48f);
            tray.lineTo (c.x + w * 0.5f, c.y + h * 0.12f);
            g.strokePath (tray, PathStrokeType (1.3f));
        };

        if (fileHover)
        {
            drawDropIcon ({ z.getCentreX(), z.getCentreY() - 6.0f }, 13.0f, Palette::accent);
            g.setColour (Palette::onScreen);
            g.setFont (uiFont (11.0f * sc, true, 0.16f));
            g.drawText ("DROP HERE", z.withTrimmedTop (z.getHeight() * 0.55f), Justification::centred, false);
        }
        else if (statusText.isNotEmpty())
        {
            g.setFont (uiFont (12.0f * sc, false, 0.05f));
            g.setColour (Palette::hit);
            g.drawText (statusText, z.reduced (10.0f, 0.0f), Justification::centred, true);
        }
        else if (! has)
        {
            drawDropIcon ({ z.getCentreX(), z.getCentreY() - 6.0f }, 12.0f, dropHover ? Palette::accent : Palette::accentSoft);
            g.setColour (dropHover ? Palette::onScreen : Palette::accentSoft);
            g.setFont (uiFont (10.5f * sc, false, 0.14f));
            g.drawText ("DROP WAV HERE", z.withTrimmedTop (z.getHeight() * 0.55f), Justification::centred, false);
        }
        else
        {
            if (auto pk = proc.getSamplePeaks(); pk != nullptr && ! pk->maxs.empty())
            {
                auto w = z.reduced (8.0f, 5.0f);
                const int cols = jmax (2, (int) w.getWidth() / 2);
                const int bins = (int) pk->maxs.size();
                g.setColour (Palette::accent.withAlpha (on2 ? 0.26f : 0.10f));
                for (int x = 0; x < cols; ++x)
                {
                    const int b0 = x * bins / cols, b1 = jmax (b0 + 1, (x + 1) * bins / cols);
                    float m = 0.0f;
                    for (int b = b0; b < b1 && b < bins; ++b) m = jmax (m, pk->maxs[(size_t) b], -pk->mins[(size_t) b]);
                    const float hh = jmax (1.0f, m * w.getHeight() * 0.5f);
                    g.fillRect (w.getX() + (float) x * 2.0f, w.getCentreY() - hh, 1.0f, hh * 2.0f);
                }
            }
            // (no corner icon here: the box is small; the dashed frame says "drop", hover says CLICK OR DROP)
            const auto name = File::createFileWithoutCheckingPath (proc.getSampleName()).getFileNameWithoutExtension().toUpperCase();
            const String arrow;   // the SOUNDS button opens the menu, so no arrow on the name
            auto nameArea = z.reduced (9.0f + (float) clearRect().getWidth() * 0.7f, 0.0f).withTrimmedBottom (z.getHeight() * 0.32f);
            // long names: shrink a little, then marquee on hover, else "..." at the end
            float fs = 12.5f * sc;
            auto widthOf = [&] (float size) { return (float) GlyphArrangement::getStringWidthInt (uiFont (size, true, 0.12f), name + arrow); };
            while (fs > 10.0f * sc && widthOf (fs) > nameArea.getWidth()) fs -= 0.5f;
            g.setFont (uiFont (fs, true, 0.12f));
            g.setColour (on2 ? Palette::onScreen : Palette::textDim);
            const float tw = widthOf (fs);
            if (tw <= nameArea.getWidth())
                g.drawText (name + arrow, nameArea, Justification::centred, false);
            else if (dropHover || sampleMenu.isVisible())
            {
                g.saveState();
                g.reduceClipRegion (nameArea.toNearestInt());
                const float travel = tw - nameArea.getWidth() + 20.0f;
                // pause, scroll to the end, pause, scroll back
                const float t = (float) std::fmod (Time::getMillisecondCounterHiRes() * 0.001 * 28.0, (double) travel * 2.0 + 60.0);
                const float off = t < 30.0f ? 0.0f : t < 30.0f + travel ? t - 30.0f : t < 60.0f + travel ? travel : travel - (t - 60.0f - travel);
                g.drawText (name + arrow, nameArea.withWidth (tw + 4.0f).translated (-off, 0.0f), Justification::centredLeft, false);
                g.restoreState();
            }
            else
            {
                g.drawText (name, nameArea, Justification::centred, true);
            }
            // x in the top-right corner: remove the sample
            {
                const auto xr = clearRect().toFloat();
                const bool xh = clearHover;
                g.setColour (xh ? Palette::accent : Palette::panel.withAlpha (0.9f));
                g.fillEllipse (xr);
                g.setColour (xh ? Palette::onAccent() : Palette::textDim);
                const auto c = xr.getCentre(); const float k = xr.getWidth() * 0.2f;
                g.drawLine (c.x - k, c.y - k, c.x + k, c.y + k, 1.4f);
                g.drawLine (c.x - k, c.y + k, c.x + k, c.y - k, 1.4f);
            }
            g.setColour (dropHover ? Palette::onScreen : Palette::accentSoft.withAlpha (on2 ? 1.0f : 0.5f));
            g.setFont (uiFont (9.5f * sc, false, 0.14f));
            const String sub = dropHover ? String ("CLICK OR DROP")
                             : (proc.isFactorySample() ? String ("FACTORY") : String ("USER")) + String::fromUTF8 (" \xc2\xb7 ") + String (proc.getSampleLengthSeconds(), 1) + " s";
            g.drawText (sub, z.reduced (8.0f, 0.0f).withTrimmedTop (z.getHeight() * 0.55f), Justification::centred, false);
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
    g.drawText ("GRANULAR", grainLabel, Justification::centredLeft, false);
    if (FxTab::style == 2 && filterTabs[0] != nullptr)
    {
        auto seg = filterTabs[0]->getBounds().getUnion (filterTabs[1]->getBounds()).toFloat();
        g.setColour (Palette::theme >= 5 ? Palette::field : Palette::screen.withAlpha (0.7f));
        g.fillRoundedRectangle (seg, 8.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (seg.reduced (0.5f), 8.0f, 1.0f);
    }
    if (FxTab::style == 2 && fxTabs[0] != nullptr)
    {
        auto seg = fxTabs[0]->getBounds().getUnion (fxTabs[2]->getBounds()).toFloat();
        g.setColour (Palette::theme >= 5 ? Palette::field : Palette::screen.withAlpha (0.7f));
        g.fillRoundedRectangle (seg, 8.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (seg.reduced (0.5f), 8.0f, 1.0f);
    }
    g.setColour (Palette::label);
    g.setFont (uiFont (10.5f * sc, false, 0.08f));
    if (selectedFx == 0) g.drawText ("TYPE", distTypeLabel, Justification::centredLeft, false);
    if (selectedFx == 1 && proc.apvts.getRawParameterValue (ParamID::delaySync)->load() > 0.5f)
        g.drawText ("TIME", distTypeLabel, Justification::centredLeft, false);

    if (selectedLfo == kNumLfos)
    {
        // ADSR values as a calm 2 x 2 readout next to the curve
        const char* ids[4]   = { ParamID::ampA, ParamID::ampD, ParamID::ampS, ParamID::ampR };
        const char* names[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
        auto area = adsrReadout.toFloat().reduced (0.0f, 6.0f);
        const float cw = area.getWidth() / 2.0f, rh = area.getHeight() / 2.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto cell = Rectangle<float> (area.getX() + cw * (float) (i % 2), area.getY() + rh * (float) (i / 2), cw, rh).reduced (6.0f, 4.0f);
            g.setColour (Palette::label);
            g.setFont (uiFont (10.0f * sc, false, 0.12f));
            g.drawText (names[i], cell.removeFromTop (cell.getHeight() * 0.42f), Justification::bottomLeft, false);
            auto* prm = proc.apvts.getParameter (ids[i]);
            g.setColour (Palette::text);
            g.setFont (uiFont (17.0f * sc, false, 0.04f));
            g.drawText (prm->getCurrentValueAsText(), cell, Justification::topLeft, false);
        }
    }
    if (selectedLfo < kNumLfos)
    {
        g.drawText ("SHAPE",    lfoPresetLabel, Justification::centredLeft, false);
        g.drawText ("MODE",     lfoModeLabel,   Justification::centredLeft, false);
        g.drawText ("TEMPO",    lfoDivLabel,    Justification::centredLeft, false);
    }
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
    if (Palette::hardware)
    {
        // every display sits behind glass in a black bezel
        Component* screens[] = { &field, &harmonics, &noisePicker, &filterScope, &lfoEditor, &ampEditor, &subBars };
        for (auto* c : screens)
        {
            if (! c->isVisible() || c->getWidth() < 8) continue;
            auto r = c->getBounds().toFloat();
            const float rad = 6.0f;
            g.setColour (Colours::black.withAlpha (0.75f));
            g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 2.0f);
            g.setColour (Colours::white.withAlpha (0.10f));
            g.drawLine (r.getX() + rad, r.getBottom() + 3.0f, r.getRight() - rad, r.getBottom() + 3.0f, 1.0f);
            g.saveState();
            Path clip; clip.addRoundedRectangle (r, rad); g.reduceClipRegion (clip);
            // glass: a soft diagonal reflection in the upper left + an inner shadow under the bezel
            Path refl;
            refl.startNewSubPath (r.getX(), r.getY());
            refl.lineTo (r.getX() + r.getWidth() * 0.62f, r.getY());
            refl.lineTo (r.getX() + r.getWidth() * 0.32f, r.getBottom());
            refl.lineTo (r.getX(), r.getBottom());
            refl.closeSubPath();
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), r.getX(), r.getY(), Colours::white.withAlpha (0.0f), r.getX() + r.getWidth() * 0.45f, r.getBottom(), false));
            g.fillPath (refl);
            g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.45f), 0, r.getY(), Colours::transparentBlack, 0, r.getY() + jmin (10.0f, r.getHeight() * 0.25f), false));
            g.fillRect (r.withHeight (jmin (10.0f, r.getHeight() * 0.25f)));
            g.restoreState();
        }
    }
    // hovering a routing digit outlines that oscillator
    if (hoverOsc < 0 || hoverOsc > 3) return;
    const auto a = (hoverOsc == 0 ? osc1Area : hoverOsc == 1 ? osc2Area : hoverOsc == 2 ? osc3Area : osc4Area).toFloat().expanded (2.0f);
    g.setColour (Palette::accent.withAlpha (0.10f));
    g.fillRoundedRectangle (a, 11.0f);
    g.setColour (Palette::accent.withAlpha (0.9f));
    g.drawRoundedRectangle (a, 11.0f, 2.0f);
}

// FILTER knobs: CUTOFF RESO, or for PEAK three in a row: CUTOFF GAIN Q
void GranulumEditor::layoutFilterKnobs()
{
    if (filterKnobs.size() < 2 || peakQKnobs.size() < 2 || widthKnobs.size() < 2) return;
    auto place = [this] (std::vector<std::unique_ptr<Knob>>& ks, Knob& gain, Knob& q, Knob& width, int mode)
    {
        auto row = filterRow1;
        if (mode == 0)
        {
            const int cw = row.getWidth() / 2;
            ks[0]->setBounds (row.withWidth (cw)); ks[1]->setBounds (row.withTrimmedLeft (cw));
            gain.setBounds (ks[1]->getBounds()); q.setBounds (ks[1]->getBounds()); width.setBounds (ks[1]->getBounds());
            return;
        }
        const int w = row.getWidth() / 3;
        ks[0]->setBounds (row.removeFromLeft (w));
        auto mid = row.removeFromLeft (w);
        gain.setBounds (mid); ks[1]->setBounds (mid);
        q.setBounds (row); width.setBounds (row);
    };
    place (filterKnobs,  *peakKnobs[0], *peakQKnobs[0], *widthKnobs[0], filtLayout[0]);
    place (filter2Knobs, *peakKnobs[1], *peakQKnobs[1], *widthKnobs[1], filtLayout[1]);
}

void GranulumEditor::resized()
{
    const float sc = (float) getWidth() / 1080.0f;
    auto S = [sc] (float v) { return (int) std::round (v * sc); };

    auto r = getLocalBounds().reduced (S (30), S (16));

    // ---- header ----
    auto header = r.removeFromTop (S (66));
    gemsArea = header.removeFromRight (S (150));
    {
        // 1.0.3: meter on top, GUIDE and ABOUT under it (next to CLIP), quiet
        auto ga = gemsArea;
        meterView.setBounds (ga.removeFromTop (S (40)).withTrimmedTop (S (6)));
        auto pills = ga.removeFromTop (S (24)).withTrimmedTop (S (3));
        guidePill.setBounds (pills.removeFromLeft (pills.getWidth() / 2).reduced (S (2), 0));
        aboutPill.setBounds (pills.reduced (S (2), 0));
    }
    header.removeFromRight (S (6));
    globalKnobs[1]->setBounds (header.removeFromRight (S (62)));   // output
    globalKnobs[2]->setBounds (header.removeFromRight (S (58)));   // master pitch, next to output
    header.removeFromRight (S (8));
    {
        auto rb = header.removeFromRight (S (146)).withSizeKeepingCentre (S (146), S (30));
        randomMenuButton.setBounds (rb.removeFromRight (S (22)));   // the little arrow: what RANDOM ALL may change
        rb.removeFromRight (S (2));
        randomButton.setBounds (rb);
    }
    header.removeFromRight (S (6));
    initButton.setBounds (header.removeFromRight (S (54)).withSizeKeepingCentre (S (54), S (30)));
    header.removeFromRight (S (6));
    undoButton.setBounds (header.removeFromRight (S (58)).withSizeKeepingCentre (S (58), S (30)));
    header.removeFromRight (S (10));
    presetBar.setBounds (header.removeFromRight (S (198)).withSizeKeepingCentre (S (198), S (30)));
    titleArea = header.withTrimmedLeft (S (4));
    {
        const auto tf = uiFont (40.0f * sc, false, 0.04f);
        const int tw = GlyphArrangement::getStringWidthInt (tf, "GRAIN FORM") + S (6);
        titleBadge.scale = sc;
        titleBadge.setBounds (titleArea.getX(), titleArea.getY(), jmin (tw, titleArea.getWidth()), S (46));
    }
    for (Component* o : { (Component*) &about, (Component*) &tour, (Component*) &randomPanel }) o->setBounds (getLocalBounds());
    randomPanel.anchor = randomMenuButton.getBounds();
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

    auto oscCol = inside.removeFromRight (S (560));   // 0.99.3: +60 from the grain field, so the OSC 1 sticks and GRAIN switches have room
    oscCol.setTop (screenFrame.getY() + S (8));
    inside.removeFromRight (S (12));
    field.setBounds (inside);
    eqView.setBounds (inside);
    compView.setBounds (inside);
    loopBar.setBounds (screenFrame.getX() + S (196), screenFrame.getY() + S (9), inside.getRight() - screenFrame.getX() - S (196), S (22));
    viewSwitch.setBounds (screenFrame.getX() + S (14), screenFrame.getY() + S (9), S (172), S (22));
    eqFlat.setBounds (inside.getRight() - S (60), screenFrame.getY() + S (7), S (60), S (25));

    {
        const int rowH = S (52), stripH = S (34), rowGap = S (6);
        // top to bottom: OSC 1 ADDITIVE, OSC 2 SUB, OSC 3 SAMPLE, OSC 4 NOISE (internal names stay osc2 = sample, osc3 = noise, osc4Area = sub)
        // 0.99.3: OSC 2-4 can fold into a slim strip; OSC 1 keeps its size, the freed space stays empty under the strips
        osc1Area = oscCol.removeFromTop (oscCol.getHeight() - 3 * rowH - 3 * rowGap);
        auto nextRow = [&] (int i) { oscCol.removeFromTop (rowGap); return oscCol.removeFromTop (proc.uiFold[i] ? stripH : rowH); };
        osc4Area = nextRow (0);
        osc2Area = nextRow (1);
        osc3Area = nextRow (2);
        foldSig = (int) proc.uiFold[0] + 2 * (int) proc.uiFold[1] + 4 * (int) proc.uiFold[2] + 8 * (int) osc1WaveMode (proc);

        auto a1 = osc1Area.reduced (S (8), S (5));
        auto h1 = a1.removeFromTop (S (26));
        // 1.0: RANDOM (ADDITIVE) and the wave buttons (WAVE) share the same slot on the right, so nothing else moves
        const auto slot = h1.removeFromRight (S (74)).reduced (0, S (1));
        rndBars.setBounds (slot.withTrimmedLeft (S (4)));
        wavePicker.setBounds (slot);
        h1.removeFromRight (S (6));
        osc1Grain.setBounds (h1.removeFromRight (S (60)).reduced (0, S (1)));
        h1.removeFromRight (S (6));
        chordButton.setBounds (h1.removeFromRight (S (112)).reduced (0, S (1)));
        h1.removeFromRight (S (6));
        // 0.99.6: the wave buttons only exist in WAVE mode
        // TEST: ADDITIVE | WAVE keeps its place in both modes; the wave buttons appear to its left
        osc1Mode.setBounds (h1.removeFromRight (S (122)).reduced (0, S (1)));   // ADDITIVE | WAVE
        wavePicker.setVisible (osc1WaveMode (proc));
        osc1Lamp.sub = {};                         // the ADDITIVE | WAVE switch says what OSC 1 is
        // 1.0: the VOICE window sits right after the OSC 1 name, in both modes
        osc1Lamp.setBounds (h1.removeFromLeft (S (60)));
        h1.removeFromLeft (S (2));
        voiceBox.setBounds (h1.removeFromLeft (S (90)).withSizeKeepingCentre (S (90), S (28)));
        rndBars.setVisible (! osc1WaveMode (proc));   // RANDOM sticks: only in ADDITIVE
        a1.removeFromTop (S (4));
        auto k1 = a1.removeFromRight (S (200));   // 5 knobs (TUNE added) in about the old width, so the sticks and GRAIN switches keep their size
        a1.removeFromRight (S (4));
        harmonics.setBounds (a1);
        const int kw1 = k1.getWidth() / (int) osc1Knobs.size();
        for (auto& k : osc1Knobs) k->setBounds (k1.removeFromLeft (kw1).withSizeKeepingCentre (kw1, jmin (k1.getHeight(), S (78))));

        // fold buttons sit at the right end of every OSC 2-4 row (open or folded)
        auto takeFold = [&] (int i, Rectangle<int>& a)
        {
            ignoreUnused (a);
            foldBtn[i].setVisible (false);   // 1.0.2: no folding any more
            if (true) return;
            foldBtn[i].folded = proc.uiFold[i];
            foldBtn[i].setTooltip (proc.uiFold[i] ? "Open this oscillator" : "Fold this oscillator into a strip");
            foldBtn[i].setBounds (a.removeFromRight (S (26)).withSizeKeepingCentre (S (24), S (22)));
            foldBtn[i].repaint();
            a.removeFromRight (S (6));
        };
        // a folded strip: [lamp] [sound name] [VOL o 70 %] [v]
        auto foldRow = [&] (Rectangle<int> area, int i, Component& lamp, Knob& vol, std::initializer_list<Component*> hide)
        {
            auto a = area.reduced (S (8), S (3));
            takeFold (i, a);
            lamp.setBounds (a.removeFromLeft (S (108)));
            vol.setInline (true);
            vol.setBounds (a.removeFromRight (S (104)));
            a.removeFromRight (S (8));
            foldText[i] = a.withSizeKeepingCentre (a.getWidth(), S (20));
            for (auto* h : hide) h->setVisible (false);
        };
        auto showAll = [] (std::initializer_list<Component*> cs) { for (auto* c : cs) c->setVisible (true); };

        // OSC 3 SAMPLE: [lamp] [drop zone] [vol oct semi tune pan] [GRAIN/SOUNDS]
        if (proc.uiFold[1])
        {
            foldRow (osc2Area, 1, osc2Lamp, *osc2Knobs[0], { osc2Knobs[1].get(), osc2Knobs[2].get(), osc2Knobs[3].get(), osc2Knobs[4].get(),
                                                             &loadButton, &osc2Grain, &samplePrev, &sampleNext });
            dropZone = {};
        }
        else
        {
            showAll ({ osc2Knobs[1].get(), osc2Knobs[2].get(), osc2Knobs[3].get(), osc2Knobs[4].get(), &loadButton, &osc2Grain, &samplePrev, &sampleNext });
            osc2Knobs[0]->setInline (false);
            foldText[1] = {};
            auto a2 = osc2Area.reduced (S (8), S (4));
            takeFold (1, a2);
            osc2Lamp.setBounds (a2.removeFromLeft (S (108)));
            auto btn2 = a2.removeFromRight (S (62));
            loadButton.setBounds (btn2.removeFromBottom (btn2.getHeight() / 2).reduced (0, S (1)));   // SOUNDS menu
            osc2Grain.setBounds (btn2.reduced (0, S (1)));
            a2.removeFromRight (S (4));
            for (int i = (int) osc2Knobs.size() - 1; i >= 0; --i)
                osc2Knobs[(size_t) i]->setBounds (a2.removeFromRight (S (35)));
            a2.removeFromRight (S (6));
            auto z = a2.withSizeKeepingCentre (a2.getWidth(), S (40));
            samplePrev.setBounds (z.removeFromLeft (S (20)).withSizeKeepingCentre (S (20), S (26)));
            sampleNext.setBounds (z.removeFromRight (S (20)).withSizeKeepingCentre (S (20), S (26)));
            dropZone = z.reduced (S (3), 0);
        }

        // OSC 4 NOISE: [lamp] [noise picker] [vol pitch pan] [GRAIN]
        if (proc.uiFold[2])
            foldRow (osc3Area, 2, osc3Lamp, *osc3Knobs[0], { osc3Knobs[1].get(), osc3Knobs[2].get(), &osc3Grain, &noisePicker });
        else
        {
            showAll ({ osc3Knobs[1].get(), osc3Knobs[2].get(), &osc3Grain, &noisePicker });
            osc3Knobs[0]->setInline (false);
            foldText[2] = {};
            auto a3 = osc3Area.reduced (S (8), S (4));
            takeFold (2, a3);
            osc3Lamp.setBounds (a3.removeFromLeft (S (108)));
            osc3Grain.setBounds (a3.removeFromRight (S (62)).withSizeKeepingCentre (S (62), S (26)));
            a3.removeFromRight (S (4));
            for (int i = (int) osc3Knobs.size() - 1; i >= 0; --i)
                osc3Knobs[(size_t) i]->setBounds (a3.removeFromRight (S (38)));
            a3.removeFromRight (S (6));
            noisePicker.setBounds (a3.withSizeKeepingCentre (a3.getWidth(), S (38)));
        }

        // OSC 2 SUB: [lamp] [5 sticks] [vol oct semi]
        subTag = {};
        if (proc.uiFold[0])
            foldRow (osc4Area, 0, osc4Lamp, *subKnobs[0], { subKnobs[1].get(), subKnobs[2].get(), subKnobs[3].get(), &subBars });
        else
        {
            showAll ({ subKnobs[1].get(), subKnobs[2].get(), subKnobs[3].get(), &subBars });
            subKnobs[0]->setInline (false);
            foldText[0] = {};
            auto a4 = osc4Area.reduced (S (8), S (4));
            takeFold (0, a4);
            osc4Lamp.setBounds (a4.removeFromLeft (S (108)));
            for (int i = (int) subKnobs.size() - 1; i >= 0; --i)
                subKnobs[(size_t) i]->setBounds (a4.removeFromRight (S (40)));
            a4.removeFromRight (S (8));
            subBars.setBounds (a4.withSizeKeepingCentre (a4.getWidth(), S (40)));   // the screen takes the whole free width
        }
    }

    // ---- blocks ----
    const int gap = S (18);
    const float colW = (float) (blocks.getWidth() - gap * 2) / 10.0f;
    grainArea  = blocks.removeFromLeft ((int) (colW * 5.0f)); blocks.removeFromLeft (gap);
    filterArea = blocks.removeFromLeft ((int) (colW * 1.85f)); blocks.removeFromLeft (gap);
    fxArea     = blocks;

    rndGrains.setBounds (grainArea.getRight() - S (74), grainArea.getY(), S (74), S (26));
    grainLabel = { grainArea.getX(), grainArea.getY(), grainArea.getWidth() - S (80), S (26) };
    // TEST: GRANULAR  [FREEZE]   SHIMMER [FREE|5TH|OCT]   [RANDOM]
    freezeSwitch.setBounds (grainArea.getX() + S (96), grainArea.getY() + S (2), S (76), S (22));
    shimmerSwitch.setBounds (rndGrains.getX() - S (8) - S (186), grainArea.getY() + S (2), S (186), S (22));
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
        auto row1 = f.removeFromTop ((int) (f.getHeight() * 0.58f));
        filterRow1 = row1;
        layoutFilterKnobs();
        f.removeFromTop (S (4));
        filterScope.setBounds (f.removeFromTop (S (38)).reduced (S (4), 0));
        auto row2 = f.withSizeKeepingCentre (f.getWidth(), S (28));
        routes[0]->setBounds (row2.removeFromRight (S (60)));
        routes[4]->setBounds (routes[0]->getBounds());
        row2.removeFromRight (S (5));
        filtNext.setBounds (row2.removeFromRight (S (17)).withSizeKeepingCentre (S (17), S (24)));
        row2.removeFromRight (S (2));
        filtPrev.setBounds (row2.removeFromLeft (S (17)).withSizeKeepingCentre (S (17), S (24)));
        row2.removeFromLeft (S (2));
        filterType.setBounds (row2);
        filterType2.setBounds (row2);
        // 1.0.2: full names when they fit (LOW PASS 24), else short ones (LP 24) - never squeezed or cut with "..."
        static const StringArray shortNames { "LP 12", "LP 24", "LP 48", "HP 12", "HP 24", "HP 48", "BP", "PEAK", "COMB", "VOWEL" };
        const int need = GlyphArrangement::getStringWidthInt (getLookAndFeel().getComboBoxFont (filterType), "HIGH PASS 48") + 8;
        const bool useShort = row2.getWidth() - 4 < need;
        for (auto* box : { &filterType, &filterType2 })
        {
            for (int k = 0; k < FilterUnit::typeNames().size(); ++k)
                box->changeItemText (k + 1, useShort ? shortNames[k] : FilterUnit::typeNames()[k]);
            if (box->getSelectedItemIndex() >= 0) box->setText (box->getItemText (box->getSelectedItemIndex()), dontSendNotification);
        }
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
        const int gapT = S (5);
        const int gapT5 = S (3);
        const int th = (tabs.getHeight() - gapT5 * (kNumLfos + 1)) / (kNumLfos + 2);   // 1.0.5: ADSR, LFO 1-3, LINKS
        ampTab.setBounds (tabs.getX(), tabs.getY(), tabs.getWidth(), th);          // ADSR first
        for (int l = 0; l < kNumLfos; ++l)
            lfoTabs[(size_t) l]->setBounds (tabs.getX(), tabs.getY() + (l + 1) * (th + gapT5), tabs.getWidth(), th);
        linksTab.setBounds (tabs.getX(), tabs.getY() + (kNumLfos + 1) * (th + gapT5), tabs.getWidth(), th);
        ignoreUnused (gapT);
        lf.removeFromLeft (S (12));

        voiceArea = lf.removeFromRight (S (180));
        lf.removeFromRight (S (18));

        linksView.setBounds (lf.withHeight (lf.getHeight()));                    // 1.0.5: LINKS covers the editor + its controls
        lfoEditor.setBounds (lf.removeFromLeft (S (330)));
        {
            // ADSR: the curve on top, the four knobs right under their letters
            auto box = lfoEditor.getBounds();                                        // same size as the LFO screen
            auto knobRow = box.removeFromBottom (S (54));
            adsrReadout = lfoEditor.getBounds().withLeft (lfoEditor.getRight() + S (24)).withRight (voiceArea.getX() - S (24));
            ampEditor.setBounds (box.withTrimmedBottom (S (2)));
            const int kw = knobRow.getWidth() / 4;
            for (auto& k : ampKnobs) k->setBounds (knobRow.removeFromLeft (kw));
        }
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
            c.grains.setBounds (divBox);
            {
                // REMOVE LINKS takes the whole row; SYNC becomes a small toggle on the DIVISION title line
                c.remove.setBounds (syncBox);
                c.remove.setButtonText ("REMOVE LINKS");
                c.sync.setBounds (lfoDivLabel.withLeft (lfoDivLabel.getRight() - S (44)).withHeight (S (17)).translated (0, -S (2)));
            }
            c.rate->setBounds (rateCol.withSizeKeepingCentre (rateCol.getWidth(), jmin (rateCol.getHeight(), S (86))));
            c.div->setBounds (c.rate->getBounds());
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

Rectangle<int> GranulumEditor::clearRect() const
{
    const int d = jmax (12, dropZone.getHeight() / 3);
    return { dropZone.getRight() - d - 3, dropZone.getY() + 3, d, d };
}

void GranulumEditor::mouseMove (const MouseEvent& e)
{
    if (e.eventComponent != this) return;
    {
        const bool xh = proc.hasSample() && clearRect().expanded (2).contains (e.getPosition());
        if (xh != clearHover) { clearHover = xh; repaint (dropZone.expanded (2)); }
    }
    const bool over = dropZone.contains (e.getPosition());
    if (over != dropHover) { dropHover = over; setMouseCursor (over ? MouseCursor::NormalCursor : MouseCursor::NormalCursor); repaint (dropZone.expanded (2)); }
}

void GranulumEditor::mouseExit (const MouseEvent& e)
{
    if (e.eventComponent != this) return;
    if (dropHover) { dropHover = false; setMouseCursor (MouseCursor::NormalCursor); repaint (dropZone.expanded (2)); }
}

void GranulumEditor::mouseDown (const MouseEvent& e)
{
    proc.undoManager.beginNewTransaction();          // each click (on any control) is one undo step
    if (e.eventComponent != this) return;            // clicks on children only start the undo step
    if (sampleMenu.isVisible()) { toggleSampleMenu (false); return; }
    // 0.99.9: the editor also listens to itself (addMouseListener for the undo steps), so the same click can
    // arrive twice: after x had removed the sample, the second copy saw an empty box and opened the file window
    if (e.eventTime == lastBoxClick) return;
    if (proc.hasSample() && clearRect().expanded (2).contains (e.getPosition()))
    {
        lastBoxClick = e.eventTime;
        proc.clearSample();               // x: remove the sample from OSC 2
        clearHover = false;
        repaint (dropZone.expanded (2));
        return;
    }
    if (dropZone.contains (e.getPosition())) { lastBoxClick = e.eventTime; launchChooser(); }   // click the sample box = choose your own file
}

bool GranulumEditor::keyPressed (const KeyPress& k)
{
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    {
        if (k.getModifiers().isShiftDown()) proc.undoManager.redo(); else proc.undoManager.undo();
        return true;
    }
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

// ============================================================================
// TEST: master EQ screen

namespace
{
    const char* const kEqOnIds[ParamEq::kBands] = { "eq1On", "eq2On", "eq3On", "eq4On", "eq5On" };
    constexpr float kEqMinHz = 20.0f, kEqMaxHz = 20000.0f, kEqRange = 18.0f;
}

Colour EqView::bandColour (int b)
{
    // the same soft family as the LFOs: sand, sage, ivory, steel blue, mist teal (low -> high)
    static const Colour c[ParamEq::kBands] = { Colour (0xffd8b394), Colour (0xffa9c79a), Colour (0xffe6dccd), Colour (0xff8fb4d0), Colour (0xff86c2c0) };
    return c[jlimit (0, ParamEq::kBands - 1, b)];
}

// 0.99.9: GRAINS | (lamp) EQ | (lamp) COMP
Rectangle<float> ViewSwitch::seg (int i) const
{
    auto r = getLocalBounds().toFloat();
    static const float w[3] = { 0.32f, 0.30f, 0.38f };
    float x = r.getX();
    for (int k = 0; k < i; ++k) x += r.getWidth() * w[k];
    return { x, r.getY(), r.getWidth() * w[i], r.getHeight() };
}

Rectangle<float> ViewSwitch::lampOf (int i) const
{
    if (i == 0) return {};
    // lamp + name are centred together in the segment (the bold font is measured, so nothing moves when selected)
    const auto s = seg (i);
    const float tw = (float) GlyphArrangement::getStringWidthInt (uiFont (10.0f, true, 0.12f), i == 1 ? "EQ" : "COMP");
    const float group = 7.0f + 6.0f + tw;
    return Rectangle<float> (7.0f, 7.0f).withCentre ({ s.getCentreX() - group * 0.5f + 3.5f, s.getCentreY() });
}

void ViewSwitch::timerCallback()
{
    const bool e = proc.apvts.getRawParameterValue (ParamID::eqOn)->load() > 0.5f;
    const bool c = proc.apvts.getRawParameterValue (ParamID::compOn)->load() > 0.5f;
    if (e != lastEq || c != lastComp) { lastEq = e; lastComp = c; repaint(); }
}

void ViewSwitch::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
    {
        if (! seg (i).contains (e.position)) continue;
        // the lamp (and a little around it) switches the effect; the rest of the segment shows that screen
        if (i > 0 && e.position.x < lampOf (i).getRight() + 4.0f)
        {
            if (auto* p = proc.apvts.getParameter (i == 1 ? ParamID::eqOn : ParamID::compOn))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
                p->endChangeGesture();
            }
            timerCallback();
            return;
        }
        setView (i);
        return;
    }
}

void ViewSwitch::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::field);
    g.fillRoundedRectangle (r, 7.0f);
    const char* names[3] = { "GRAINS", "EQ", "COMP" };
    const bool lit[3] = { false, proc.apvts.getRawParameterValue (ParamID::eqOn)->load() > 0.5f,
                          proc.apvts.getRawParameterValue (ParamID::compOn)->load() > 0.5f };
    for (int i = 0; i < 3; ++i)
    {
        auto s = seg (i);
        const bool on = i == view;
        if (on) { g.setColour (Palette::accent); g.fillRoundedRectangle (s.reduced (2.0f), 5.5f); }
        auto text = s;
        if (i > 0)
        {
            const auto led = lampOf (i);
            if (lit[i])
            {
                g.setColour ((on ? Colours::white : Palette::accent).withAlpha (0.28f));
                g.fillEllipse (led.expanded (3.0f));
                g.setColour (on ? Colour (0xffffe3cc) : Palette::accent.brighter (0.2f));
                g.fillEllipse (led);
            }
            else
            {
                g.setColour (on ? Colour (0xfff4cfb3) : Palette::textDim.withAlpha (0.6f));
                g.drawEllipse (led.reduced (0.3f), 1.1f);
            }
            text = text.withLeft (led.getRight() + 6.0f);
        }
        g.setColour (on ? Palette::onAccent() : Palette::textDim);
        g.setFont (uiFont (10.0f, on, 0.12f));
        g.drawText (names[i], text, i == 0 ? Justification::centred : Justification::centredLeft, false);
    }
}

// ===========================================================================
// 0.99.9 COMP screen

CompView::CompView (GranulumProcessor& p) : proc (p)
{
    const char* ids[6] = { ParamID::compThresh, ParamID::compRatio, ParamID::compAttack, ParamID::compRelease, ParamID::compGain, ParamID::compMix };
    const char* titles[6] = { "Threshold", "Ratio", "Attack", "Release", "Gain", "Mix" };
    for (int k = 0; k < 6; ++k)
    {
        knobs.push_back (std::make_unique<Knob> (proc, ids[k], titles[k], Knob::Size::Small));
        addAndMakeVisible (*knobs.back());
    }
    knobs[0]->slider.setTooltip ("THRESHOLD: above this level the sound is turned down");
    knobs[1]->slider.setTooltip ("RATIO: how hard it turns down (4 : 1 = 4 dB over the threshold come out as 1 dB)");
    knobs[2]->slider.setTooltip ("ATTACK: how fast it grabs (short = tames the clicks and hits)");
    knobs[3]->slider.setTooltip ("RELEASE: how fast it lets go");
    knobs[4]->slider.setTooltip ("GAIN: gives back the level the compressor took (about as much as the GR number shows)");
    knobs[5]->slider.setTooltip ("MIX: 100 % = only the compressed sound, less = parallel compression");
    setRepaintsOnMouseActivity (false);
}

void CompView::resized()
{
    auto r = getLocalBounds().removeFromBottom (64).reduced (8, 2);
    const int w = r.getWidth() / (int) knobs.size();
    for (auto& k : knobs) k->setBounds (r.removeFromLeft (w));
}

Rectangle<float> CompView::plot() const
{
    auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    r.removeFromBottom (66.0f);       // knobs
    r.removeFromBottom (12.0f);       // dB labels under the plot
    r.removeFromRight (116.0f);       // meters
    r.removeFromLeft (22.0f);         // dB labels left
    return r;
}

Rectangle<float> CompView::meters() const
{
    auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    r.removeFromBottom (66.0f);
    return r.removeFromRight (104.0f);
}

void CompView::timerCallback()
{
    const bool on = proc.apvts.getRawParameterValue (ParamID::compOn)->load() > 0.5f;
    const float g  = proc.compGrUi.exchange (0.0f);
    const float ip = Decibels::gainToDecibels (proc.compInUi.exchange (0.0f), -60.0f);
    const float op = Decibels::gainToDecibels (proc.compOutUi.exchange (0.0f), -60.0f);
    gr    = on ? jmax (g, gr * 0.8f) : 0.0f;
    inDb  = jmax (ip, inDb - 1.5f);
    outDb = jmax (op, outDb - 1.5f);
    repaint();
}

void CompView::paint (Graphics& g)
{
    auto all = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (all, 10.0f);
    const bool on = proc.apvts.getRawParameterValue (ParamID::compOn)->load() > 0.5f;
    const float th    = proc.apvts.getRawParameterValue (ParamID::compThresh)->load();
    const float ratio = proc.apvts.getRawParameterValue (ParamID::compRatio)->load();
    const auto p = plot();
    auto xOf = [&] (float db) { return p.getX() + (db + 48.0f) / 48.0f * p.getWidth(); };
    auto yOf = [&] (float db) { return p.getBottom() - (db + 48.0f) / 48.0f * p.getHeight(); };
    auto curve = [&] (float x)
    {
        const float knee = 6.0f, over = x - th, slope = 1.0f - 1.0f / jmax (1.0f, ratio);
        float red = 0.0f;
        if (over >= knee * 0.5f) red = slope * over;
        else if (over > -knee * 0.5f) red = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        return x - red;
    };
    // grid + labels
    g.setColour (Colour (0xff141516));
    g.fillRect (p);
    g.setFont (uiFont (8.0f, false, 0.04f));
    for (int d = -48; d <= 0; d += 12)
    {
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawVerticalLine ((int) xOf ((float) d), p.getY(), p.getBottom());
        g.drawHorizontalLine ((int) yOf ((float) d), p.getX(), p.getRight());
        g.setColour (Palette::textDim);
        g.drawText (String (d), Rectangle<float> (30.0f, 10.0f).withCentre ({ xOf ((float) d), p.getBottom() + 7.0f }), Justification::centred, false);
        g.drawText (String (d), Rectangle<float> (p.getX() - 26.0f, yOf ((float) d) - 5.0f, 22.0f, 10.0f), Justification::centredRight, false);
    }
    // 1 : 1 line and the threshold
    {
        Path diag; diag.startNewSubPath (xOf (-48.0f), yOf (-48.0f)); diag.lineTo (xOf (0.0f), yOf (0.0f));
        Path dashed; const float dl[2] = { 3.0f, 3.0f };
        PathStrokeType (1.0f).createDashedStroke (dashed, diag, dl, 2);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.fillPath (dashed);
        Path tl; tl.startNewSubPath (xOf (th), p.getY()); tl.lineTo (xOf (th), p.getBottom());
        Path td; const float d2[2] = { 2.0f, 3.0f };
        PathStrokeType (1.0f).createDashedStroke (td, tl, d2, 2);
        g.setColour (Palette::accent.withAlpha (0.45f));
        g.fillPath (td);
        g.setColour (Palette::accent.withAlpha (on ? 1.0f : 0.5f));
        g.setFont (uiFont (8.5f, true, 0.08f));
        const String tt = "THRESHOLD " + String (th, 1) + " dB";
        const bool right = xOf (th) + 110.0f < p.getRight();
        g.drawText (tt, right ? Rectangle<float> (xOf (th) + 4.0f, p.getY() + 4.0f, 110.0f, 11.0f)
                              : Rectangle<float> (xOf (th) - 114.0f, p.getY() + 4.0f, 110.0f, 11.0f),
                    right ? Justification::centredLeft : Justification::centredRight, false);
    }
    // the curve
    const auto col = on ? Palette::accent.brighter (0.25f) : Palette::textDim;
    {
        Path c;
        for (int i = 0; i <= 96; ++i)
        {
            const float x = -48.0f + 0.5f * (float) i;
            const Point<float> pt (xOf (x), yOf (jlimit (-48.0f, 0.0f, curve (x))));
            if (i == 0) c.startNewSubPath (pt); else c.lineTo (pt);
        }
        g.setColour (col);
        g.strokePath (c, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.fillEllipse (Rectangle<float> (8.0f, 8.0f).withCentre ({ xOf (th), yOf (curve (th)) }));
        if (inDb > -47.0f)
        {
            const float x = jmin (0.0f, inDb);
            g.setColour (Colour (0xfff3ede6));
            g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre ({ xOf (x), yOf (jlimit (-48.0f, 0.0f, on ? curve (x) : x)) }));
        }
        g.setColour (Palette::textDim);
        g.setFont (uiFont (8.0f, false, 0.1f));
        g.drawText ("IN dB", p.reduced (5.0f).removeFromBottom (11.0f), Justification::bottomRight, false);
    }
    // meters: GR number + GR / IN / OUT bars
    {
        auto m = meters();
        g.setColour (Colour (0xff1b1c1e));
        g.fillRoundedRectangle (m, 8.0f);
        auto top = m.removeFromTop (44.0f);
        g.setColour (on ? Palette::accent.brighter (0.35f) : Palette::textDim);
        g.setFont (uiFont (20.0f, true, 0.0f));
        g.drawText (on ? "-" + String (gr, 1) : String ("OFF"), top.removeFromTop (28.0f).withTrimmedTop (4.0f), Justification::centred, false);
        g.setColour (Palette::textDim);
        g.setFont (uiFont (7.5f, true, 0.12f));
        g.drawText ("dB GAIN REDUCTION", top, Justification::centredTop, false);
        auto bars = m.reduced (14.0f, 6.0f);
        auto lab = bars.removeFromBottom (12.0f);
        const float bw = 12.0f, gap = (bars.getWidth() - 3.0f * bw) / 2.0f;
        auto bar = [&] (int i, float frac, Colour c, bool fromTop, const char* name)
        {
            const auto b = Rectangle<float> (bars.getX() + (float) i * (bw + gap), bars.getY(), bw, bars.getHeight());
            g.setColour (Palette::field);
            g.fillRoundedRectangle (b, 3.0f);
            frac = jlimit (0.0f, 1.0f, frac);
            if (frac > 0.002f)
            {
                g.setColour (c);
                g.fillRoundedRectangle (fromTop ? b.withHeight (b.getHeight() * frac) : b.withTrimmedTop (b.getHeight() * (1.0f - frac)), 3.0f);
            }
            g.setColour (Palette::textDim);
            g.setFont (uiFont (7.5f, false, 0.08f));
            g.drawText (name, Rectangle<float> (b.getX() - 8.0f, lab.getY(), b.getWidth() + 16.0f, lab.getHeight()), Justification::centred, false);
        };
        bar (0, gr / 24.0f, Palette::accent, true, "GR");
        bar (1, (inDb + 48.0f) / 48.0f, Colour (0xff9a9da1), false, "IN");
        bar (2, (outDb + 48.0f) / 48.0f, Colour (0xffe8e4de), false, "OUT");
    }
}

EqView::EqView (GranulumProcessor& p) : proc (p)
{
    for (int b = 0; b < ParamEq::kBands; ++b)
    {
        auto& st = strips[(size_t) b];
        st.onBtn = std::make_unique<GrainSwitch> (proc, kEqOnIds[b], StringArray { "ON" }, String(), "Switch this band on / off");
        addChildComponent (*st.onBtn);
        st.type.addItemList (ParamEq::typeNames(), 1);
        addChildComponent (st.type);
        st.typeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, GranulumProcessor::eqId (b, "Type"), st.type);
        st.type.onChange = [this] { repaint(); };
        const char* what[4] = { "Freq", "Gain", "Q", "Slope" };
        const char* title[4] = { "Freq", "Gain", "Q", "Slope" };
        for (int k = 0; k < 4; ++k)
        {
            st.knobs.push_back (std::make_unique<Knob> (proc, GranulumProcessor::eqId (b, what[k]), title[k], Knob::Size::Small));
            addChildComponent (*st.knobs.back());
        }
        st.knobs[3]->slider.setTooltip ("SLOPE: how steep LOW CUT / HIGH CUT are (12, 24 or 48 dB per octave)");
    }
    setRepaintsOnMouseActivity (false);
}

EqView::~EqView() { proc.eqScopeWanted.store (false); }

void EqView::visibilityChanged()
{
    proc.eqScopeWanted.store (isShowing());
    if (isShowing()) startTimerHz (30); else stopTimer();
}

RangedAudioParameter* EqView::par (int b, const char* what) const { return proc.apvts.getParameter (GranulumProcessor::eqId (b, what)); }

ParamEq::Band EqView::baseBand (int b) const
{
    ParamEq::Band x;
    auto& s = proc.apvts;
    x.on     = s.getRawParameterValue (GranulumProcessor::eqId (b, "On"))->load() > 0.5f;
    x.type   = (int) s.getRawParameterValue (GranulumProcessor::eqId (b, "Type"))->load();
    x.slope  = (int) s.getRawParameterValue (GranulumProcessor::eqId (b, "Slope"))->load();
    x.freq   = s.getRawParameterValue (GranulumProcessor::eqId (b, "Freq"))->load();
    x.gainDb = s.getRawParameterValue (GranulumProcessor::eqId (b, "Gain"))->load();
    x.q      = s.getRawParameterValue (GranulumProcessor::eqId (b, "Q"))->load();
    return x;
}

// what the audio actually uses right now (LFOs included)
ParamEq::Band EqView::liveBand (int b) const
{
    auto x = baseBand (b);
    auto live = [this, b] (const char* what, int t, float base)
    {
        for (int l = 0; l < kNumLfos; ++l)
            if (proc.hasModulation (t, l)) return par (b, what)->convertFrom0to1 (proc.modNorm[(size_t) t].load (std::memory_order_relaxed));
        return base;
    };
    x.freq   = live ("Freq", mtEq1Freq + 3 * b, x.freq);
    x.gainDb = live ("Gain", mtEq1Gain + 3 * b, x.gainDb);
    x.q      = live ("Q",    mtEq1Q + 3 * b,    x.q);
    return x;
}

Rectangle<float> EqView::plot() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (30.0f);                                   // note + zone rulers
    if (selected >= 0) r.removeFromBottom (66.0f);             // band controls
    r.removeFromBottom (14.0f);                                // Hz labels
    r.removeFromRight (30.0f);                                 // dB labels
    r.removeFromLeft (8.0f);
    return r;
}

float EqView::xOf (float hz) const { const auto p = plot(); return p.getX() + p.getWidth() * std::log (jlimit (kEqMinHz, kEqMaxHz, hz) / kEqMinHz) / std::log (kEqMaxHz / kEqMinHz); }
float EqView::hzOf (float x) const { const auto p = plot(); return kEqMinHz * std::pow (kEqMaxHz / kEqMinHz, jlimit (0.0f, 1.0f, (x - p.getX()) / jmax (1.0f, p.getWidth()))); }
float EqView::yOf (float db) const { const auto p = plot(); return p.getY() + p.getHeight() * (kEqRange - jlimit (-kEqRange, kEqRange, db)) / (2.0f * kEqRange); }
float EqView::dbOf (float y) const { const auto p = plot(); return kEqRange - 2.0f * kEqRange * (y - p.getY()) / jmax (1.0f, p.getHeight()); }

static bool eqHasGain (int type) { return type == ParamEq::Bell || type == ParamEq::LowShelf || type == ParamEq::HighShelf; }

int EqView::bandAt (Point<float> pos) const
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    int best = -1; float bd = 11.0f;
    for (int b = 0; b < ParamEq::kBands; ++b)
    {
        const auto x = liveBand (b);
        float total = 0.0f;
        for (int k = 0; k < ParamEq::kBands; ++k) total += ParamEq::bandDb (liveBand (k), x.freq, sr);
        const Point<float> c (xOf (x.freq), yOf (eqHasGain (x.type) ? total : 0.0f));
        const float d = c.getDistanceFrom (pos);
        if (d < bd) { bd = d; best = b; }
    }
    return best;
}

void EqView::select (int b)
{
    selected = b;
    for (int i = 0; i < ParamEq::kBands; ++i)
    {
        auto& st = strips[(size_t) i];
        const bool v = i == b;
        st.onBtn->setVisible (v);
        st.type.setVisible (v);
        for (auto& k : st.knobs) k->setVisible (v);
    }
    resized();
    repaint();
}

void EqView::resized()
{
    if (selected < 0) return;
    auto r = getLocalBounds().removeFromBottom (64).reduced (8, 2);
    auto& st = strips[(size_t) selected];
    auto left = r.removeFromLeft (52);
    st.onBtn->setBounds (left.withTrimmedTop (22).withHeight (22));
    r.removeFromLeft (6);
    st.type.setBounds (r.removeFromLeft (96).withTrimmedTop (20).withHeight (24));
    r.removeFromLeft (6);
    const int w = r.getWidth() / 4;
    for (auto& k : st.knobs) k->setBounds (r.removeFromLeft (w));
}

void EqView::flat()
{
    for (int b = 0; b < ParamEq::kBands; ++b)
        for (auto* what : { "Gain", "Freq", "Q", "Type", "Slope", "On" })
            if (auto* p = par (b, what)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    repaint();
}

void EqView::timerCallback()
{
    if (selected >= 0)
    {
        // SLOPE only means something for LOW CUT / HIGH CUT
        const int t = baseBand (selected).type;
        const float a = (t == ParamEq::LowCut || t == ParamEq::HighCut) ? 1.0f : 0.3f;
        auto& k = strips[(size_t) selected].knobs[3];
        if (k->getAlpha() != a) k->setAlpha (a);
    }
    // spectrum of what comes out of the synth (after the EQ)
    const int N = 2048;
    const int w = proc.eqWrite.load (std::memory_order_acquire);
    float mx = 0.0f;
    for (int i = 0; i < N; i += 4) mx = jmax (mx, std::abs (proc.eqRing[(size_t) ((w - N + i) & 4095)]));
    if (mx < 1.0e-5f) { if (++silentFrames > 40) { if (dragging >= 0 || selected >= 0) repaint(); return; } }
    else silentFrames = 0;
    for (int i = 0; i < N; ++i)
    {
        const float win = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (N - 1));
        fftData[(size_t) i] = proc.eqRing[(size_t) ((w - N + i) & 4095)] * win;
    }
    std::fill (fftData.begin() + N, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    const auto p = plot();
    const int cols = jmax (1, (int) p.getWidth() / 2);
    if ((int) specDb.size() != cols) specDb.assign ((size_t) cols, -120.0f);
    for (int c = 0; c < cols; ++c)
    {
        const float f0 = hzOf (p.getX() + 2.0f * (float) c), f1 = hzOf (p.getX() + 2.0f * (float) (c + 1));
        const int b0 = jlimit (1, N / 2 - 1, (int) (f0 / sr * N)), b1 = jlimit (b0, N / 2 - 1, (int) (f1 / sr * N));
        float m = 0.0f;
        for (int b = b0; b <= b1; ++b) m = jmax (m, fftData[(size_t) b]);
        const float db = Decibels::gainToDecibels (m / (float) (N / 4), -120.0f);
        auto& d = specDb[(size_t) c];
        d = db > d ? d + (db - d) * 0.6f : jmax (db, d - 1.8f);
    }
    repaint();
}

void EqView::paint (Graphics& g)
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    auto all = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (all, 10.0f);
    const auto p = plot();

    // rulers: notes and zones
    g.setFont (uiFont (8.0f, false, 0.04f));
    g.setColour (Palette::textDim);
    for (int o = 1; o <= 9; ++o)
    {
        const float x = xOf (32.703f * std::exp2 ((float) (o - 1)));
        g.drawText ("C" + String (o), Rectangle<float> (24.0f, 11.0f).withCentre ({ x, 7.0f }), Justification::centred, false);
    }
    struct Z { const char* n; float a, b; };
    static const Z zones[] = { { "SUB", 20, 60 }, { "BASS", 60, 250 }, { "LOW MID", 250, 500 }, { "MID", 500, 2000 },
                               { "HIGH MID", 2000, 4000 }, { "PRS", 4000, 6000 }, { "TREBLE", 6000, 20000 } };
    g.setFont (uiFont (7.5f, true, 0.1f));
    for (auto& z : zones)
    {
        auto zr = Rectangle<float> (xOf (z.a) + 1.0f, 15.0f, xOf (z.b) - xOf (z.a) - 2.0f, 11.0f);
        g.setColour (Colours::white.withAlpha (0.045f));
        g.fillRoundedRectangle (zr, 2.0f);
        g.setColour (Palette::label);
        g.drawText (z.n, zr, Justification::centred, false);
    }

    // plot + grid
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRect (p);
    for (float f : { 30.f, 40.f, 50.f, 60.f, 70.f, 80.f, 90.f, 200.f, 300.f, 400.f, 500.f, 600.f, 700.f, 800.f, 900.f,
                     2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 7000.f, 8000.f, 9000.f })
    { g.setColour (Colours::white.withAlpha (0.035f)); g.drawVerticalLine ((int) xOf (f), p.getY(), p.getBottom()); }
    for (float f : { 100.f, 1000.f, 10000.f }) { g.setColour (Colours::white.withAlpha (0.09f)); g.drawVerticalLine ((int) xOf (f), p.getY(), p.getBottom()); }
    for (float d : { -12.f, -6.f, 6.f, 12.f }) { g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) yOf (d), p.getX(), p.getRight()); }
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine ((int) yOf (0.0f), p.getX(), p.getRight());
    g.setFont (uiFont (8.0f, false, 0.0f));
    g.setColour (Palette::textDim);
    for (int d : { 18, 12, 6, 0, -6, -12, -18 })
        g.drawText ((d > 0 ? "+" : "") + String (d), Rectangle<float> (28.0f, 10.0f).withPosition (p.getRight() + 4.0f, yOf ((float) d) - 5.0f), Justification::centredLeft, false);
    for (auto [f, t] : { std::pair<float, const char*> { 50.f, "50" }, { 100.f, "100" }, { 200.f, "200" }, { 500.f, "500" },
                         { 1000.f, "1k" }, { 2000.f, "2k" }, { 5000.f, "5k" }, { 10000.f, "10k" } })
        g.drawText (t, Rectangle<float> (30.0f, 11.0f).withCentre ({ xOf (f), p.getBottom() + 7.0f }), Justification::centred, false);

    // spectrum (copper, behind)
    if (! specDb.empty())
    {
        Path sp;
        sp.startNewSubPath (p.getX(), p.getBottom());
        for (size_t c = 0; c < specDb.size(); ++c)
        {
            const float db = jlimit (-90.0f, 6.0f, specDb[c] + 12.0f);          // -78..+18 shown on the +-18 scale
            const float y = jlimit (p.getY(), p.getBottom(), p.getY() + p.getHeight() * (1.0f - (db + 78.0f) / 96.0f));
            sp.lineTo (p.getX() + 2.0f * (float) c, y);
        }
        sp.lineTo (p.getRight(), p.getBottom());
        sp.closeSubPath();
        g.setGradientFill (ColourGradient (Palette::accent.withAlpha (0.38f), 0, p.getY(), Palette::accent.withAlpha (0.04f), 0, p.getBottom(), false));
        g.fillPath (sp);
    }

    // curve (live, with LFOs)
    std::array<ParamEq::Band, ParamEq::kBands> live;
    for (int b = 0; b < ParamEq::kBands; ++b) live[(size_t) b] = liveBand (b);
    auto total = [&] (float hz) { float t = 0.0f; for (auto& x : live) t += ParamEq::bandDb (x, hz, sr); return t; };
    Path curve;
    const int steps = jmax (60, (int) p.getWidth() / 2);
    for (int i = 0; i <= steps; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / (float) steps;
        const float y = jlimit (p.getY(), p.getBottom(), yOf (total (hzOf (x))));
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    {
        Path fill (curve);
        fill.lineTo (p.getRight(), yOf (0.0f)); fill.lineTo (p.getX(), yOf (0.0f)); fill.closeSubPath();
        g.setColour (Palette::text.withAlpha (0.06f));
        g.fillPath (fill);
    }
    g.setColour (Palette::text.withAlpha (0.92f));
    g.strokePath (curve, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));

    // points: an LFO leaves a faint "ghost" where the knob is, a thin line in the band colour to where it is now
    for (int b = 0; b < ParamEq::kBands; ++b)
    {
        const auto& x = live[(size_t) b];
        const auto base = baseBand (b);
        const auto col = bandColour (b).withMultipliedAlpha (x.on ? 1.0f : 0.35f);
        const Point<float> c (xOf (x.freq), yOf (eqHasGain (x.type) ? total (x.freq) : 0.0f));
        bool modded = false;
        for (int l = 0; l < kNumLfos; ++l)
            for (int t : { mtEq1Freq + 3 * b, mtEq1Gain + 3 * b })
                modded = modded || proc.hasModulation (t, l);
        const Point<float> c0 (xOf (base.freq), yOf (eqHasGain (base.type) ? total (base.freq) - ParamEq::bandDb (x, base.freq, sr) + ParamEq::bandDb (base, base.freq, sr) : 0.0f));
        if (modded && c0.getDistanceFrom (c) > 2.0f)
        {
            g.setColour (col.withAlpha (0.5f));
            g.drawLine ({ c0, c }, 1.0f);
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre (c0));
        }
        if (b == selected)
        {
            g.setColour (col.withAlpha (0.2f));
            g.fillEllipse (Rectangle<float> (20.0f, 20.0f).withCentre (c));
            g.setColour (col);
            g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (c));
            g.setColour (Colours::white);
            g.drawEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (c), 1.3f);
        }
        else
        {
            g.setColour (col);
            g.fillEllipse (Rectangle<float> (8.5f, 8.5f).withCentre (c));
            g.setColour (Palette::screen);
            g.drawEllipse (Rectangle<float> (8.5f, 8.5f).withCentre (c), 1.2f);
        }
        if (b == dragging || b == hover)
        {
            const String t = (x.freq < 1000.0f ? String (roundToInt (x.freq)) + " Hz" : String (x.freq / 1000.0f, 2) + " kHz")
                             + (eqHasGain (x.type) ? "  " + String (x.gainDb > 0 ? "+" : "") + String (x.gainDb, 1) + " dB" : String());
            g.setFont (uiFont (9.0f, true, 0.02f));
            auto tr = Rectangle<float> ((float) GlyphArrangement::getStringWidthInt (uiFont (9.0f, true, 0.02f), t) + 12.0f, 16.0f)
                          .withCentre ({ c.x, c.y - 16.0f }).constrainedWithin (p);
            g.setColour (Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (tr, 4.0f);
            g.setColour (col);
            g.drawText (t, tr, Justification::centred, false);
        }
    }

    // band strip
    if (selected >= 0)
    {
        auto sr2 = getLocalBounds().toFloat().removeFromBottom (66.0f);
        g.setColour (Palette::panel.withAlpha (0.9f));
        g.fillRect (sr2);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) sr2.getY(), sr2.getX(), sr2.getRight());
        const auto x = baseBand (selected);
        const String t = ParamEq::typeNames()[x.type] + String::fromUTF8 (" \xc2\xb7 ") + (x.freq < 1000.0f ? String (roundToInt (x.freq)) : String (x.freq / 1000.0f, 1) + "k");
        g.setFont (uiFont (8.5f, true, 0.08f));
        g.setColour (bandColour (selected));
        g.drawText (t, sr2.reduced (8.0f, 4.0f).withHeight (14.0f).withWidth (150.0f), Justification::centredLeft, false);
    }
}

void EqView::mouseDown (const MouseEvent& e)
{
    const int b = plot().expanded (6.0f).contains (e.position) ? bandAt (e.position) : -1;
    if (b < 0)
    {
        if (plot().contains (e.position)) select (-1);   // empty spot: hide the band controls
        return;
    }
    if (b != selected) select (b);
    if (e.mods.isPopupMenu()) { showMenu (b); return; }
    dragging = b;
    for (auto* what : { "Freq", "Gain" }) par (b, what)->beginChangeGesture();
}

void EqView::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const bool fine = e.mods.isShiftDown();
    auto* fp = par (dragging, "Freq");
    auto* gp = par (dragging, "Gain");
    float x = e.position.x, y = e.position.y;
    if (fine)
    {
        // Shift: a quarter of the movement
        const auto d = e.getOffsetFromDragStart().toFloat() * 0.25f;
        x = e.mouseDownPosition.x + d.x; y = e.mouseDownPosition.y + d.y;
    }
    fp->setValueNotifyingHost (fp->convertTo0to1 (hzOf (x)));
    if (eqHasGain (baseBand (dragging).type)) gp->setValueNotifyingHost (gp->convertTo0to1 (jlimit (-kEqRange, kEqRange, dbOf (y))));
    repaint();
}

void EqView::mouseUp (const MouseEvent&)
{
    if (dragging >= 0) for (auto* what : { "Freq", "Gain" }) par (dragging, what)->endChangeGesture();
    dragging = -1;
    repaint();
}

void EqView::mouseMove (const MouseEvent& e)
{
    const int b = plot().expanded (6.0f).contains (e.position) ? bandAt (e.position) : -1;
    if (b != hover) { hover = b; repaint(); }
}

void EqView::mouseExit (const MouseEvent&) { if (hover >= 0) { hover = -1; repaint(); } }

void EqView::mouseDoubleClick (const MouseEvent& e)
{
    const int b = bandAt (e.position);
    if (b < 0) return;
    for (auto* what : { "Gain", "Freq", "Q" })
        if (auto* p = par (b, what)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    repaint();
}

void EqView::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const int b = bandAt (e.position);
    if (b < 0) return;
    auto* qp = par (b, "Q");
    const float step = (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.01f : 0.04f);
    qp->beginChangeGesture();
    qp->setValueNotifyingHost (jlimit (0.0f, 1.0f, qp->getValue() + step));
    qp->endChangeGesture();
    if (b != selected) select (b);
    repaint();
}

void EqView::showMenu (int b)
{
    PopupMenu m;
    const auto x = baseBand (b);
    for (int t = 0; t < ParamEq::typeNames().size(); ++t) m.addItem (1 + t, ParamEq::typeNames()[t], true, x.type == t);
    if (x.type == ParamEq::LowCut || x.type == ParamEq::HighCut)
    {
        m.addSeparator();
        for (int s = 0; s < 3; ++s) m.addItem (20 + s, ParamEq::slopeNames()[s] + " / oct", true, x.slope == s);
    }
    m.addSeparator();
    m.addItem (40, x.on ? "Switch band off" : "Switch band on");
    Component::SafePointer<EqView> safe (this);
    m.showMenuAsync (PopupMenu::Options(), [safe, b] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto set = [&] (const char* what, float v) { if (auto* p = safe->par (b, what)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } };
        if (r >= 1 && r < 20) set ("Type", (float) (r - 1));
        else if (r >= 20 && r < 23) set ("Slope", (float) (r - 20));
        else if (r == 40) set ("On", safe->baseBand (b).on ? 0.0f : 1.0f);
        safe->repaint();
    });
}

// ============================================================================
// TEST: title sheen, About, guided tour, RANDOM panel

void TitleBadge::timerCallback()
{
    // every 8 s a copper sheen crosses the name in 2.6 s; the rest of the time nothing moves (and nothing repaints)
    const double t = (Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    const double cycle = std::fmod (t, 8.0);
    const float s = cycle < 2.6 ? (float) (cycle / 2.6) : -1.0f;
    if (s != sweep) { const bool was = sweep >= 0.0f; sweep = s; if (s >= 0.0f || was) repaint(); }
}

void TitleBadge::paint (Graphics& g)
{
    const float sc = scale;
    auto r = getLocalBounds().toFloat();
    g.setFont (uiFont (40.0f * sc, false, 0.04f));
    if (Palette::hardware)
    {
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawText ("GRAIN FORM", r.translated (0, 1), Justification::bottomLeft, false);
    }
    if (sweep < 0.0f) { g.setColour (Palette::text); g.drawText ("GRAIN FORM", r, Justification::bottomLeft, false); return; }
    // a soft band of warm light moving left -> right across the letters
    const float w = r.getWidth(), x = -0.3f * w + 1.6f * w * sweep;
    const auto sheen = Palette::accent.interpolatedWith (Colours::white, 0.45f);
    ColourGradient cg (Palette::text, x - 0.22f * w, 0.0f, Palette::text, x + 0.22f * w, 0.0f, false);
    cg.addColour (0.5, sheen);
    g.setGradientFill (cg);
    g.drawText ("GRAIN FORM", r, Justification::bottomLeft, false);
}

void AboutOverlay::paint (Graphics& g)
{
    const float sc = (float) getWidth() / 1080.0f;
    g.fillAll (Colours::black.withAlpha (0.45f));
    auto card = Rectangle<float> (30.0f * sc, 94.0f * sc, 300.0f * sc, 168.0f * sc);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (card.translated (0, 8.0f * sc).expanded (4.0f * sc), 16.0f * sc);
    g.setColour (Palette::panel.darker (0.25f));
    g.fillRoundedRectangle (card, 14.0f * sc);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (card.reduced (0.5f), 14.0f * sc, 1.0f);
    auto in = card.reduced (24.0f * sc, 20.0f * sc);
    g.setColour (Palette::text);
    g.setFont (uiFont (24.0f * sc, false, 0.14f));
    g.drawText ("GRAIN FORM", in.removeFromTop (30.0f * sc), Justification::centredLeft, false);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (8.5f * sc, true, 0.42f));
    g.drawText ("SELECTIVE GRANULATION", in.removeFromTop (14.0f * sc), Justification::centredLeft, false);
    in.removeFromTop (12.0f * sc);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillRect (in.removeFromTop (1.0f));
    in.removeFromTop (12.0f * sc);
    g.setColour (Palette::label);
    g.setFont (uiFont (12.0f * sc, false, 0.02f));
    g.drawText ("Made by producers", in.removeFromTop (18.0f * sc), Justification::centredLeft, false);
    g.setColour (Palette::accent.brighter (0.25f));
    g.setFont (uiFont (15.0f * sc, true, 0.04f));
    g.drawText ("Thematic & Oddkut", in.removeFromTop (22.0f * sc), Justification::centredLeft, false);
    auto bottom = in.removeFromBottom (20.0f * sc);
    const String v = "VERSION " + version;
    const auto vf = uiFont (9.0f * sc, true, 0.16f);
    auto vr = bottom.removeFromLeft ((float) GlyphArrangement::getStringWidthInt (vf, v) + 18.0f * sc).withSizeKeepingCentre ((float) GlyphArrangement::getStringWidthInt (vf, v) + 18.0f * sc, 20.0f * sc);
    g.setColour (Palette::field);
    g.fillRoundedRectangle (vr, 6.0f * sc);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.drawRoundedRectangle (vr.reduced (0.5f), 6.0f * sc, 1.0f);
    g.setColour (Palette::label);
    g.setFont (vf);
    g.drawText (v, vr, Justification::centred, false);
    g.setColour (Palette::textDim.withAlpha (0.8f));
    g.setFont (uiFont (9.0f * sc, false, 0.1f));
    g.drawText ("click to close", bottom, Justification::centredRight, false);
}

void GuidePill::paint (Graphics& g)
{
    // 1.0.3: quiet - a thin copper outline on the panel colour, no glow, no animation; brighter only on hover
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool over = isMouseOver();
    g.setColour (over ? Palette::field.brighter (0.1f) : Palette::field);
    g.fillRoundedRectangle (r, rad);
    g.setColour (Palette::accent.withAlpha (over ? 0.8f : 0.38f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    auto dot = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.2f);
    g.setColour (Palette::accent.withAlpha (over ? 1.0f : 0.7f));
    g.drawEllipse (dot.reduced (0.5f), 1.0f);
    g.setFont (uiFont (dot.getHeight() * 0.72f, true, 0.0f));
    g.drawText (info ? "i" : "?", dot.translated (0.0f, 0.3f), Justification::centred, false);
    g.setColour (over ? Palette::text : Palette::label);
    g.setFont (uiFont (r.getHeight() * 0.46f, true, 0.14f));
    g.drawText (info ? "ABOUT" : "GUIDE", r.withTrimmedRight (r.getHeight() * 0.35f), Justification::centred, false);
}

Rectangle<float> TourOverlay::cardRect() const
{
    const float sc = (float) getWidth() / 1080.0f;
    const auto a = steps[(size_t) step].area().toFloat();
    const float w = 320.0f * sc, gap = 16.0f * sc;
    // 1.0.5: the card grows with its text (the tips made some steps longer)
    AttributedString as;
    as.append (steps[(size_t) step].text, uiFont (12.0f * sc, false, 0.0f), Palette::label);
    TextLayout tl;
    tl.createLayout (as, w - 36.0f * sc);
    const float h = jmax (196.0f * sc, 32.0f * sc + 14.0f * sc + 26.0f * sc + tl.getHeight() + 6.0f * sc + 14.0f * sc + 30.0f * sc);
    const auto full = getLocalBounds().toFloat().reduced (10.0f * sc);
    Rectangle<float> c;
    if (a.getRight() + gap + w <= full.getRight())     c = { a.getRight() + gap, a.getY() + 10.0f * sc, w, h };
    else if (a.getX() - gap - w >= full.getX())        c = { a.getX() - gap - w, a.getY() + 10.0f * sc, w, h };
    else if (a.getBottom() + gap + h <= full.getBottom()) c = { a.getX() + 20.0f * sc, a.getBottom() + gap, w, h };
    else                                               c = { a.getX() + 20.0f * sc, a.getY() - gap - h, w, h };
    return c.constrainedWithin (full);
}

void TourOverlay::paint (Graphics& g)
{
    if (steps.empty()) return;
    if (steps[(size_t) step].area().isEmpty()) { paintIntro (g); return; }
    const float sc = (float) getWidth() / 1080.0f;
    const auto& st = steps[(size_t) step];
    const auto a = st.area().toFloat().expanded (4.0f * sc);
    Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    dim.addRoundedRectangle (a, 14.0f * sc);
    dim.setUsingNonZeroWinding (false);
    g.setColour (Colour (0xff0b0b0c).withAlpha (0.68f));
    g.fillPath (dim);
    g.setColour (Palette::accent.brighter (0.3f));
    g.drawRoundedRectangle (a, 14.0f * sc, 1.6f);

    auto c = cardRect();
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (c.translated (0, 8.0f * sc).expanded (3.0f * sc), 16.0f * sc);
    g.setColour (Palette::panel.darker (0.3f));
    g.fillRoundedRectangle (c, 14.0f * sc);
    g.setColour (Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f * sc, 1.0f);
    auto in = c.reduced (18.0f * sc, 16.0f * sc);
    g.setColour (Palette::accent.brighter (0.3f));
    g.setFont (uiFont (9.0f * sc, true, 0.2f));
    g.drawText ("STEP " + String (step + 1) + " OF " + String ((int) steps.size()), in.removeFromTop (14.0f * sc), Justification::centredLeft, false);
    g.setColour (Palette::text);
    g.setFont (uiFont (17.0f * sc, true, 0.0f));
    g.drawText (st.title, in.removeFromTop (26.0f * sc), Justification::centredLeft, false);
    auto bottom = in.removeFromBottom (30.0f * sc);
    auto bars = in.removeFromBottom (14.0f * sc);
    {
        AttributedString as;
        as.append (st.text, uiFont (12.0f * sc, false, 0.0f), Palette::label);
        TextLayout tl;
        tl.createLayout (as, in.getWidth());
        tl.draw (g, in);
    }
    const float bw = (bars.getWidth() - 4.0f * sc * (float) (steps.size() - 1)) / (float) steps.size();
    for (int i = 0; i < (int) steps.size(); ++i)
    {
        g.setColour (i <= step ? Palette::accent.brighter (0.3f) : Colours::white.withAlpha (0.1f));
        g.fillRoundedRectangle (Rectangle<float> (bars.getX() + (float) i * (bw + 4.0f * sc), bars.getCentreY() - 1.5f * sc, bw, 3.0f * sc), 1.5f * sc);
    }
    const bool last = step == (int) steps.size() - 1;
    btnNext = bottom.removeFromRight (70.0f * sc).reduced (0, 1.0f * sc);
    bottom.removeFromRight (6.0f * sc);
    btnBack = step > 0 ? bottom.removeFromRight (66.0f * sc).reduced (0, 1.0f * sc) : Rectangle<float>();
    btnSkip = bottom;
    g.setFont (uiFont (9.5f * sc, true, 0.14f));
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (btnNext, 8.0f * sc);
    g.setColour (Palette::onAccent());
    g.drawText (last ? "DONE" : "NEXT", btnNext, Justification::centred, false);
    if (! btnBack.isEmpty())
    {
        g.setColour (Palette::field);
        g.fillRoundedRectangle (btnBack, 8.0f * sc);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (btnBack.reduced (0.5f), 8.0f * sc, 1.0f);
        g.setColour (Palette::text);
        g.drawText ("BACK", btnBack, Justification::centred, false);
    }
    if (! last)
    {
        g.setColour (Palette::textDim);
        g.drawText ("SKIP TOUR", btnSkip, Justification::centredLeft, false);
    }
}

// 1.0.3: the welcome page of the GUIDE - what GRAIN FORM is (English). Three looks to choose from (introStyle).
void TourOverlay::paintIntro (Graphics& g)
{
    const float sc = (float) getWidth() / 1080.0f;
    const auto& st = steps[(size_t) step];
    const StringArray paras = StringArray::fromLines (st.text);
    const bool open = introStyle == 2;
    g.setColour (Colour (0xff0b0b0c).withAlpha (open ? 0.88f : 0.72f));
    g.fillAll();

    auto drawButtons = [&] (Rectangle<float> bottom, bool centred)
    {
        g.setFont (uiFont (9.5f * sc, true, 0.14f));
        if (centred)
        {
            btnNext = bottom.withSizeKeepingCentre (130.0f * sc, bottom.getHeight());
            btnSkip = btnNext.translated (0, btnNext.getHeight() + 8.0f * sc).withSizeKeepingCentre (110.0f * sc, 16.0f * sc);
        }
        else
        {
            btnNext = bottom.removeFromRight (120.0f * sc);
            btnSkip = bottom;
        }
        btnBack = {};
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (btnNext, 8.0f * sc);
        g.setColour (Palette::onAccent());
        g.drawText ("START TOUR", btnNext, Justification::centred, false);
        g.setColour (Palette::textDim);
        g.drawText ("SKIP TOUR", btnSkip, centred ? Justification::centred : Justification::centredLeft, false);
    };
    auto drawParas = [&] (Rectangle<float> area, float size, Justification j)
    {
        g.setFont (uiFont (size * sc, false, 0.0f));
        for (int k = 0; k < paras.size(); ++k)
        {
            g.setColour (k == 0 ? Palette::text : Palette::label);
            AttributedString as;
            as.append (paras[k], uiFont (size * sc, k == 0, 0.0f), k == 0 ? Palette::text : Palette::label);
            as.setJustification (j);
            as.setLineSpacing (3.0f * sc);
            TextLayout tl; tl.createLayout (as, area.getWidth());
            tl.draw (g, area.withHeight (tl.getHeight()));
            area.removeFromTop (tl.getHeight() + 11.0f * sc);
        }
    };
    auto title = [&] (Rectangle<float> r, float size, Justification j)
    {
        g.setColour (Palette::text);
        g.setFont (uiFont (size * sc, false, 0.12f));
        g.drawText ("GRAIN FORM", r.removeFromTop (size * 1.25f * sc), j, false);
        g.setColour (Palette::accent.brighter (0.3f));
        g.setFont (uiFont (8.5f * sc, true, 0.42f));
        g.drawText ("SELECTIVE GRANULATION", r.removeFromTop (14.0f * sc), j, false);
    };
    auto card = [&] (Rectangle<float> c)
    {
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (c.translated (0, 8.0f * sc).expanded (3.0f * sc), 18.0f * sc);
        g.setColour (Palette::panel.darker (0.3f));
        g.fillRoundedRectangle (c, 16.0f * sc);
        g.setColour (Colours::white.withAlpha (0.09f));
        g.drawRoundedRectangle (c.reduced (0.5f), 16.0f * sc, 1.0f);
    };

    if (introStyle == 0)          // A: a calm centred card
    {
        auto c = Rectangle<float> (520.0f * sc, 338.0f * sc).withCentre (getLocalBounds().toFloat().getCentre());
        card (c);
        auto in = c.reduced (34.0f * sc, 28.0f * sc);
        g.setColour (Palette::accent.brighter (0.3f));
        g.setFont (uiFont (9.0f * sc, true, 0.2f));
        g.drawText ("WELCOME", in.removeFromTop (16.0f * sc), Justification::centredLeft, false);
        title (in.removeFromTop (54.0f * sc), 30.0f, Justification::centredLeft);
        in.removeFromTop (12.0f * sc);
        g.setColour (Colours::white.withAlpha (0.08f)); g.fillRect (in.removeFromTop (1.0f));
        in.removeFromTop (16.0f * sc);
        auto bottom = in.removeFromBottom (32.0f * sc);
        drawParas (in, 12.5f, Justification::topLeft);
        drawButtons (bottom, false);
    }
    else if (introStyle == 1)     // B: split - name and makers on the left, the story on the right
    {
        auto c = Rectangle<float> (760.0f * sc, 320.0f * sc).withCentre (getLocalBounds().toFloat().getCentre());
        card (c);
        auto in = c.reduced (34.0f * sc, 30.0f * sc);
        auto bottom = in.removeFromBottom (32.0f * sc);
        in.removeFromBottom (14.0f * sc);
        auto left = in.removeFromLeft (250.0f * sc);
        in.removeFromLeft (30.0f * sc);
        g.setColour (Palette::accent.withAlpha (0.35f));
        g.fillRect (Rectangle<float> (in.getX() - 15.0f * sc, in.getY(), 1.0f, in.getHeight()));
        g.setColour (Palette::text);
        g.setFont (uiFont (38.0f * sc, false, 0.1f));
        g.drawText ("GRAIN", left.removeFromTop (44.0f * sc), Justification::centredLeft, false);
        g.drawText ("FORM", left.removeFromTop (44.0f * sc), Justification::centredLeft, false);
        g.setColour (Palette::accent.brighter (0.3f));
        g.setFont (uiFont (8.5f * sc, true, 0.42f));
        g.drawText ("SELECTIVE GRANULATION", left.removeFromTop (18.0f * sc), Justification::centredLeft, false);
        left.removeFromTop (22.0f * sc);
        g.setColour (Palette::textDim);
        g.setFont (uiFont (10.5f * sc, false, 0.02f));
        g.drawText ("Made by producers", left.removeFromTop (16.0f * sc), Justification::centredLeft, false);
        g.setColour (Palette::accent.brighter (0.25f));
        g.setFont (uiFont (13.0f * sc, true, 0.04f));
        g.drawText ("Thematic & Oddkut", left.removeFromTop (18.0f * sc), Justification::centredLeft, false);
        drawParas (in, 12.5f, Justification::topLeft);
        drawButtons (bottom, false);
    }
    else                          // C: open - no card, big name and centred text over the dimmed synth
    {
        auto in = Rectangle<float> (600.0f * sc, 372.0f * sc).withCentre (getLocalBounds().toFloat().getCentre());
        title (in.removeFromTop (76.0f * sc), 46.0f, Justification::centred);
        in.removeFromTop (18.0f * sc);
        g.setColour (Palette::accent.withAlpha (0.5f));
        g.fillRect (Rectangle<float> (60.0f * sc, 1.0f).withCentre ({ in.getCentreX(), in.getY() }));
        in.removeFromTop (22.0f * sc);
        auto bottom = in.removeFromBottom (58.0f * sc);
        drawParas (in, 13.5f, Justification::centredTop);
        drawButtons (bottom.removeFromTop (32.0f * sc), true);
    }
    // progress: same segments as the other steps
    const float bw = 14.0f * sc, gap = 4.0f * sc, total = (float) steps.size() * (bw + gap) - gap;
    const float y = open ? btnSkip.getBottom() + 14.0f * sc : btnNext.getBottom() + 14.0f * sc;
    for (int i = 0; i < (int) steps.size(); ++i)
    {
        g.setColour (i == step ? Palette::accent.brighter (0.3f) : Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (Rectangle<float> ((float) getWidth() * 0.5f - total * 0.5f + (float) i * (bw + gap), y, bw, 3.0f * sc), 1.5f * sc);
    }
}

void TourOverlay::mouseDown (const MouseEvent& e)
{
    const auto p = e.position;
    if (btnNext.contains (p)) { if (++step >= (int) steps.size()) { setVisible (false); return; } repaint(); return; }
    if (! btnBack.isEmpty() && btnBack.contains (p)) { step = jmax (0, step - 1); repaint(); return; }
    if (btnSkip.contains (p) && step < (int) steps.size() - 1) { setVisible (false); return; }
    ignoreUnused (p);
}

Rectangle<float> RandomPanel::panel() const
{
    const float sc = (float) getWidth() / 1080.0f;
    const float w = 300.0f * sc, h = 292.0f * sc;
    return Rectangle<float> ((float) anchor.getRight() + 40.0f * sc - w, (float) anchor.getBottom() + 7.0f * sc, w, h);
}

Rectangle<float> RandomPanel::chip (int i) const
{
    const float sc = (float) getWidth() / 1080.0f;
    auto p = panel().reduced (14.0f * sc);
    const float cw = (p.getWidth() - 12.0f * sc) / 3.0f, ch = 34.0f * sc;
    return { p.getX() + (float) (i % 3) * (cw + 6.0f * sc), p.getY() + 22.0f * sc + (float) (i / 3) * (ch + 6.0f * sc), cw, ch };
}

Rectangle<float> RandomPanel::tick (int i) const
{
    const float sc = (float) getWidth() / 1080.0f;
    auto p = panel().reduced (14.0f * sc);
    const float cw = p.getWidth() / 2.0f;
    return { p.getX() + (float) (i % 2) * cw, p.getY() + 136.0f * sc + (float) (i / 2) * 18.0f * sc, cw, 16.0f * sc };
}

Rectangle<float> RandomPanel::goButton() const
{
    const float sc = (float) getWidth() / 1080.0f;
    auto p = panel().reduced (14.0f * sc);
    return p.removeFromBottom (30.0f * sc);
}

namespace
{
    const std::pair<int, const char*> kRandItems[] = { { GranulumProcessor::rndSticksBit, "Harmonics" }, { GranulumProcessor::rndGrainsBit, "Grains" },
                                                       { GranulumProcessor::rndFiltersBit, "Filter 1" }, { GranulumProcessor::rndFilter2Bit, "Filter 2" },
                                                       { GranulumProcessor::rndDistBit, "Distortion" }, { GranulumProcessor::rndDelayBit, "Delay" },
                                                       { GranulumProcessor::rndReverbBit, "Reverb" }, { GranulumProcessor::rndLfo1Bit, "LFO 1" },
                                                       { GranulumProcessor::rndLfo2Bit, "LFO 2" }, { GranulumProcessor::rndLfo3Bit, "LFO 3" } };
}

void RandomPanel::paint (Graphics& g)
{
    const float sc = (float) getWidth() / 1080.0f;
    const auto p = panel();
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (p.translated (0, 8.0f * sc).expanded (3.0f * sc), 16.0f * sc);
    g.setColour (Palette::panel.darker (0.3f));
    g.fillRoundedRectangle (p, 14.0f * sc);
    g.setColour (Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (p.reduced (0.5f), 14.0f * sc, 1.0f);
    auto in = p.reduced (14.0f * sc);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (8.5f * sc, true, 0.2f));
    g.drawText ("SOUND TYPE", in.withHeight (14.0f * sc), Justification::centredLeft, false);
    const int t = proc.randType.load();
    for (int i = 0; i < GranulumProcessor::randTypeNames().size(); ++i)
    {
        const auto c = chip (i);
        const bool on = i == t;
        g.setColour (on ? Palette::accent : Palette::field.brighter (0.06f));
        g.fillRoundedRectangle (c, 9.0f * sc);
        g.setColour (on ? Palette::accent.brighter (0.35f) : Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (c.reduced (0.5f), 9.0f * sc, 1.0f);
        g.setColour (on ? Palette::onAccent() : Palette::label);
        g.setFont (uiFont (10.0f * sc, true, 0.14f));
        g.drawText (GranulumProcessor::randTypeNames()[i], c, Justification::centred, false);
    }
    const float sepY = chip (5).getBottom() + 12.0f * sc;
    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillRect (Rectangle<float> (in.getX(), sepY, in.getWidth(), 1.0f));
    g.setColour (Palette::textDim);
    g.setFont (uiFont (8.5f * sc, true, 0.2f));
    g.drawText ("RANDOMIZE", Rectangle<float> (in.getX(), sepY + 8.0f * sc, in.getWidth(), 14.0f * sc), Justification::centredLeft, false);
    const int m = proc.randMask.load();
    for (int i = 0; i < 10; ++i)
    {
        const auto r = tick (i);
        const bool on = (m & kRandItems[i].first) != 0;
        auto box = r.withWidth (13.0f * sc).withSizeKeepingCentre (13.0f * sc, 13.0f * sc);
        g.setColour (on ? Palette::accent : Palette::field);
        g.fillRoundedRectangle (box, 3.5f * sc);
        if (! on) { g.setColour (Colours::white.withAlpha (0.15f)); g.drawRoundedRectangle (box.reduced (0.5f), 3.5f * sc, 1.0f); }
        else
        {
            Path tickPath;
            tickPath.startNewSubPath (box.getX() + box.getWidth() * 0.25f, box.getCentreY());
            tickPath.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.28f);
            tickPath.lineTo (box.getRight() - box.getWidth() * 0.22f, box.getY() + box.getHeight() * 0.28f);
            g.setColour (Colours::white);
            g.strokePath (tickPath, PathStrokeType (1.6f * sc, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (on ? Palette::text : Palette::label);
        g.setFont (uiFont (10.5f * sc, false, 0.02f));
        g.drawText (kRandItems[i].second, r.withTrimmedLeft (20.0f * sc), Justification::centredLeft, false);
    }
    const auto b = goButton();
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (b, 9.0f * sc);
    g.setColour (Palette::onAccent());
    g.setFont (uiFont (10.0f * sc, true, 0.16f));
    g.drawText ("RANDOM " + (t == 0 ? String ("ALL") : GranulumProcessor::randTypeNames()[t]), b, Justification::centred, false);
}

void RandomPanel::mouseDown (const MouseEvent& e)
{
    const auto p = e.position;
    if (! panel().contains (p)) { setVisible (false); return; }   // a click outside closes it
    for (int i = 0; i < GranulumProcessor::randTypeNames().size(); ++i)
        if (chip (i).contains (p)) { proc.setRandType (i); if (onChange) onChange(); repaint(); return; }
    for (int i = 0; i < 10; ++i)
        if (tick (i).contains (p)) { proc.randMask.store (proc.randMask.load() ^ kRandItems[i].first); repaint(); return; }
    if (goButton().contains (p)) { proc.randomize(); repaint(); }
}

void GranulumEditor::setupTour()
{
    auto u = [] (Rectangle<int> a, Rectangle<int> b) { return a.isEmpty() ? b : b.isEmpty() ? a : a.getUnion (b); };
    tour.steps = {
        { [] { return Rectangle<int>(); }, "Welcome",
          "GRAIN FORM opens up a new way of making sound: it turns the familiar process of synthesis into working with living, ever-changing sonic textures.\n"
          "Create deep basses, atmospheric pads, expressive leads and complex experimental textures. Turn ordinary sounds into something entirely new by shaping their movement, character and structure.\n"
          "From subtle changes to radical transformations, GRAIN FORM gives you the tools to build a sound of your own." },
        { [this, u] { return u (u (presetBar.getBounds(), undoButton.getBounds()), u (randomButton.getBounds(), randomMenuButton.getBounds())); },
          "Presets & Random", "Browse with the arrows or the name. SAVE keeps your sound, NEW VERSION saves a numbered copy (_02, _03). INIT starts clean - right-click it to make your own INIT. The arrow next to RANDOM picks a SOUND TYPE and ticks which parts may change." },
        { [this] { return osc1Area; }, "Oscillator 1",
          "ADDITIVE draws 16 harmonics: drag across the sticks to draw, drag with the RIGHT button to erase. The GRAIN dots under the sticks choose which harmonics turn into grains - the others stay clean and mono. WAVE plays a classic shape. MIX WAVE and AIR add a wave and bright air (double-click = 0). VOICE stacks up to 16 voices: drag or wheel the number, double-click = 1.\n"
          "LFO on a stick: mouse wheel over the stick = depth (hold SHIFT for fine steps), right-click its LFO mark = remove." },
        { [this, u] { return u (u (osc2Area, osc3Area), osc4Area); }, "Sub, Sample & Noise",
          "OSC 2 SUB keeps the low end clean and mono: draw its sticks, right-click a stick to take its LFO off. OSC 3 plays your own WAV (drop it on the box, the small x removes it) or the built-in SOUNDS. OSC 4 adds one of 12 noises. GRAIN on each decides if it turns into grains." },
        { [this] { return screenFrame; }, "The screen",
          "GRAINS shows every grain as it plays; LOOP: drag the two flags to pick the part of the sample. EQ is a 5-band equalizer: drag a dot (SHIFT = fine), mouse wheel = width, double-click = reset, right-click = filter type and band on / off. COMP is a compressor with GAIN to win the level back. Click a lamp to switch EQ or COMP on and off, click the name to look at it." },
        { [this] { return grainArea; }, "Granular",
          "BLOOM, SMEAR, SCATTER, SHIMMER, DRIFT and CHIME give the cloud its character; SIZE, DENSITY and SPREAD shape it, SHAPE sets the grain window. SPEED 0 holds the grains on one spot.\n"
          "SYNC BPM (copper = on): SIZE and DENSITY turn into note values and the grains start on the project grid - rhythmic chops in time. With it, SCATTER swings them off the grid and, turned up, skips some. Off = free (ms and grains per second).\n"
          "Grains are random: the same note never sounds exactly the same twice - that is the life of the sound, not a bug. For a punch that hits the same every time, leave the first stick or the SUB without GRAIN." },
        { [this] { return filterArea; }, "Filters",
          "Two filters in a row: low-pass and high-pass up to 48 dB, band-pass with WIDTH, peak, comb and vowel. The arrows step through the types; 1 2 3 4 choose which oscillators go through (the same digits work on every effect). Drag inside the small picture: left / right = CUTOFF, up / down = RESO." },
        { [this] { return fxArea; }, "Effects",
          "Distortion, delay and reverb, each with its own switch, MIX and routing. Distortion keeps its level steady while you turn DRIVE." },
        { [this, u] { return u (lfoFrame, voiceArea); }, "LFO & Envelope",
          "Drag an LFO (the dots on its tab) onto any knob, stick or pill: it swings from the value to the top. Depth: mouse wheel over the knob or stick, or drag the LFO mark - hold SHIFT for fine steps. Right-click the mark = remove; REMOVE LINKS takes this LFO off everything, its lamp only pauses it. LINKS (under LFO 3) lists every link: drag a slider for depth, x removes. ALT + drag an LFO tab onto another LFO tab copies all its settings. The movement is drawn only while sound plays - in silence everything stands still, easy to grab.\n"
          "Shape: double-click = add or delete a point, drag the small handles to bend a line. RETRIG restarts on every note, ENV plays once. ADSR shapes every note. MONO plays every note cleanly; PORTA (above 0) slides between them." },
        { [this, u] { return u (globalKnobs[2]->getBounds(), u (globalKnobs[1]->getBounds(), gemsArea)); }, "Output, CLIP & help",
          "PITCH moves the whole synth, OUTPUT sets the level. CLIP cuts everything over 0 dB and nothing else. Every knob: double-click = default, SHIFT or CTRL = fine. UNDO = Ctrl+Z, redo = Ctrl+Shift+Z. GUIDE opens this tour again, ABOUT shows who made GRAIN FORM. Enjoy!" } };
}

// ============================================================================
// 1.0 VOICE window: OSC 1 unison (1..9 copies), DETUNE and BLEND as mini knobs

VoiceBox::VoiceBox (GranulumProcessor& p)
    : proc (p), detune (p, ParamID::osc1Detune, "Detune", Knob::Size::Mini), blend (p, ParamID::osc1Blend, "Blend", Knob::Size::Mini)
{
    for (auto* k : { &detune, &blend })
    {
        addAndMakeVisible (*k);
        k->slider.setPopupDisplayEnabled (true, false, nullptr);
    }
    setTooltip ("VOICE: how many copies of OSC 1 play together (1 = off). Drag up/down or mouse wheel, double-click = 1");
    startTimerHz (15);
}

int VoiceBox::voices() const { return juce::jlimit (1, 16, (int) std::round (proc.apvts.getRawParameterValue (ParamID::osc1Voices)->load())); }

void VoiceBox::setVoices (int n)
{
    if (auto* prm = proc.apvts.getParameter (ParamID::osc1Voices))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::jlimit (1, 16, n)));
    timerCallback();
}

Rectangle<float> VoiceBox::countArea() const { return getLocalBounds().toFloat().removeFromLeft (34.0f); }

void VoiceBox::resized()
{
    auto r = getLocalBounds();
    r.removeFromLeft (34);
    r = r.reduced (1, 1);
    const int w = r.getWidth() / 2;
    detune.setBounds (r.removeFromLeft (w));
    blend.setBounds (r);
}

void VoiceBox::timerCallback()
{
    const int n = voices();
    if (n != shown)
    {
        shown = n;
        const float a = n > 1 ? 1.0f : 0.45f;   // one voice: DETUNE / BLEND have nothing to do
        detune.setAlpha (a);
        blend.setAlpha (a);
        repaint();
    }
    // hover shows the value as a tooltip (no room for value boxes here)
    detune.slider.setTooltip ("DETUNE " + detune.slider.getTextFromValue (detune.slider.getValue()) + "  -  how far the voices drift apart in pitch");
    blend.slider.setTooltip ("BLEND " + blend.slider.getTextFromValue (blend.slider.getValue()) + "  -  level of the side voices against the centre one");
}

void VoiceBox::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colour (0xff1b1c1e));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::white.withAlpha (isMouseOver (true) ? 0.16f : 0.1f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    auto c = countArea().reduced (2.0f, 2.0f);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (6.4f, true, 0.16f));
    g.drawText ("VOICE", c.removeFromTop (9.0f), Justification::centred, false);
    const int n = voices();
    g.setColour (n > 1 ? Palette::accent.brighter (0.35f) : Palette::label);
    g.setFont (uiFont (12.5f, true, 0.0f));
    g.drawText (String (n), c, Justification::centred, false);
}

void VoiceBox::mouseDown (const MouseEvent& e)
{
    if (! countArea().contains (e.position)) return;
    dragStart = voices(); dragY = e.position.y; dragging = true;
    if (auto* prm = proc.apvts.getParameter (ParamID::osc1Voices)) prm->beginChangeGesture();
}

void VoiceBox::mouseDrag (const MouseEvent& e)
{
    if (! dragging) return;
    setVoices (dragStart + (int) std::round ((dragY - e.position.y) / 8.0f));   // 8 px per voice
}

void VoiceBox::mouseUp (const MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    if (auto* prm = proc.apvts.getParameter (ParamID::osc1Voices)) prm->endChangeGesture();
}

void VoiceBox::mouseDoubleClick (const MouseEvent& e) { if (countArea().contains (e.position)) setVoices (1); }

void VoiceBox::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    if (! countArea().contains (e.position)) return;
    if (w.deltaY != 0.0f) setVoices (voices() + (w.deltaY > 0.0f ? 1 : -1));
}

// ============================================================================
// 1.0.2 StepArrow: < / > next to the filter type

void StepArrow::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver();
    g.setColour (over ? Palette::field.brighter (0.12f) : Palette::field);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (Colours::white.withAlpha (over ? 0.16f : 0.08f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    const auto c = r.getCentre();
    const float s = jmin (r.getWidth(), r.getHeight()) * 0.2f;
    Path p;
    if (dir < 0) p.addTriangle (c.x - s * 0.8f, c.y, c.x + s * 0.6f, c.y - s, c.x + s * 0.6f, c.y + s);
    else         p.addTriangle (c.x + s * 0.8f, c.y, c.x - s * 0.6f, c.y - s, c.x - s * 0.6f, c.y + s);
    g.setColour (over ? Palette::text : Palette::label);
    g.fillPath (p);
}
