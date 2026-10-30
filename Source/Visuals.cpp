#include "Visuals.h"

using namespace juce;

// ============================================================================
// Organism — the living main screen

Organism::Organism (GranulumProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::CrosshairCursor);
    setOpaque (false);
    parts.reserve (520);
    for (int i = 0; i < granulum::kNumHarmonics; ++i) bars[(size_t) i] = result[(size_t) i] = proc.getHarmonic (i);
    lastEv[0] = proc.engine.grainEvents[0].load();
    lastEv[1] = proc.engine.grainEvents[1].load();
    lastNotes = proc.engine.noteEvents.load();
    startTimerHz (50);
}

void Organism::resized()
{
    const int w = jmax (1, getWidth()), h = jmax (1, getHeight());
    backdrop = Image (Image::ARGB, w, h, true);
    Graphics g (backdrop);
    auto r = Rectangle<float> (0, 0, (float) w, (float) h);

    ColourGradient bg (Palette::screen.brighter (0.25f), r.getCentreX(), r.getHeight() * 0.62f,
                       Palette::screen.darker (0.45f), 0.0f, 0.0f, true);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (r, 14.0f);

    // faint measurement grid
    g.setColour (Palette::accent.withAlpha (0.045f));
    for (int i = 1; i < 8; ++i)
        g.drawHorizontalLine ((int) (r.getHeight() * (float) i / 8.0f), 12.0f, r.getWidth() - 12.0f);

    // star dust
    Random rd (31);
    for (int i = 0; i < w * h / 1400; ++i)
    {
        g.setColour (Colours::white.withAlpha (0.03f + rd.nextFloat() * 0.07f));
        const float s = 0.6f + rd.nextFloat() * 1.2f;
        g.fillEllipse (rd.nextFloat() * (float) w, rd.nextFloat() * (float) h, s, s);
    }
}

Rectangle<float> Organism::barArea() const
{
    return getLocalBounds().toFloat().reduced (24.0f, 0.0f).withTrimmedTop (40.0f).withTrimmedBottom (26.0f);
}

Rectangle<float> Organism::slot (int k) const
{
    auto a = barArea();
    const float w = a.getWidth() / (float) granulum::kNumHarmonics;
    return { a.getX() + w * (float) k, a.getY(), w, a.getHeight() };
}

int Organism::barAt (float x) const
{
    auto a = barArea();
    return jlimit (0, granulum::kNumHarmonics - 1, (int) ((x - a.getX()) / a.getWidth() * (float) granulum::kNumHarmonics));
}

float Organism::param (const char* id) const
{
    const int t = GranulumProcessor::modIndexOf (id);
    auto* p = proc.apvts.getParameter (id);
    if (t >= 0 && p != nullptr) return p->convertFrom0to1 (proc.modNorm[(size_t) t].load());
    return proc.apvts.getRawParameterValue (id)->load();
}

void Organism::computeResult()
{
    const bool grainOn = proc.apvts.getRawParameterValue (ParamID::grainAdd)->load() > 0.5f;
    const float amount = grainOn ? param (ParamID::amount) : 0.0f;
    const float r = std::exp2 (param (ParamID::tone) / 12.0f);
    const float tex = param (ParamID::texture);
    const float spread = 0.35f + tex * 1.3f;
    const float hard = jlimit (0.0f, 1.0f, (tex - 0.15f) / 0.7f);
    float maxH = 0.0f;
    for (float b : bars) maxH = jmax (maxH, b);

    for (int k = 1; k <= granulum::kNumHarmonics; ++k)
    {
        float s = 0.0f;
        for (int j = 1; j <= granulum::kNumHarmonics; ++j)
        {
            const float d = ((float) k - (float) j * r) / spread;
            s += bars[(size_t) j - 1] * std::exp (-0.5f * d * d);
        }
        s = s / (1.0f + spread * 0.6f) * (1.0f + spread * 0.25f);
        s += hard * 0.18f * maxH * (0.4f + (float) k / 16.0f);
        const float target = jlimit (0.0f, 1.0f, bars[(size_t) k - 1] * (1.0f - amount) + s * amount);
        result[(size_t) k - 1] += (target - result[(size_t) k - 1]) * 0.25f;
    }
}

void Organism::spawn (int layer, int count)
{
    const float tone = param (ParamID::tone);
    const float tex = param (ParamID::texture);
    const float motion = param (ParamID::motion);
    const bool dirtOn = proc.apvts.getRawParameterValue (layer == 0 ? ParamID::dirtAdd : ParamID::dirtSmp)->load() > 0.5f;
    const float drive = dirtOn ? param (ParamID::dirtDrive) * param (ParamID::dirtMix) : 0.0f;
    auto a = barArea();

    float total = 0.0f;
    for (float v : result) total += v;

    for (int n = 0; n < count && (int) parts.size() < 480; ++n)
    {
        Particle p {};
        float bright = 0.5f;
        if (layer == 0)
        {
            int k = 0;
            if (total > 0.001f)
            {
                float pick = rng.nextFloat() * total;
                for (k = 0; k < granulum::kNumHarmonics - 1; ++k)
                {
                    pick -= result[(size_t) k];
                    if (pick <= 0.0f) break;
                }
            }
            else k = rng.nextInt (granulum::kNumHarmonics);
            auto s = slot (k);
            p.x = s.getCentreX() + (rng.nextFloat() - 0.5f) * s.getWidth() * 0.5f;
            p.y = a.getBottom() - result[(size_t) k] * a.getHeight() + (rng.nextFloat() - 0.5f) * 10.0f;
            bright = jlimit (0.0f, 1.0f, (float) k / 15.0f + tone / 30.0f);
            p.col = Palette::accent.interpolatedWith (Palette::violetHi, bright).interpolatedWith (Colours::white, rng.nextFloat() * 0.25f);
        }
        else
        {
            p.x = a.getX() + rng.nextFloat() * a.getWidth();
            p.y = a.getCentreY() + (rng.nextFloat() - 0.5f) * a.getHeight() * 0.6f;
            p.col = Palette::violet.interpolatedWith (Palette::violetHi, rng.nextFloat()).interpolatedWith (Colours::white, rng.nextFloat() * 0.2f);
        }

        float speed = 1.0f;
        if (drive > 0.02f && rng.nextFloat() < drive * 0.6f)
        {
            p.col = Palette::hot.interpolatedWith (Colours::white, rng.nextFloat() * 0.4f);
            speed = 1.7f;
        }

        p.kind = tex < 0.33f ? 0 : tex < 0.66f ? 1 : 2;
        p.size = jmap (tex, 7.5f, 2.6f) * (0.6f + 0.8f * rng.nextFloat());
        p.vy = (-(28.0f + tone * 7.0f) - rng.nextFloat() * 35.0f) * speed;
        p.vx = (rng.nextFloat() - 0.5f) * (40.0f + motion * 230.0f) * speed;
        p.maxLife = p.life = jmap (tex, 1.7f, 0.6f) * (0.7f + 0.6f * rng.nextFloat());
        p.rot = rng.nextFloat() * MathConstants<float>::twoPi;
        p.vrot = (rng.nextFloat() - 0.5f) * (1.5f + tex * 14.0f);
        p.seed = rng.nextFloat() * 10.0f;
        parts.push_back (p);
    }
}

void Organism::timerCallback()
{
    const float dt = 0.02f;
    t += dt;

    if (! editing)
        for (int i = 0; i < granulum::kNumHarmonics; ++i) bars[(size_t) i] = proc.getHarmonic (i);

    auto glowTo = [] (float& g, float level) { g += (jlimit (0.0f, 1.0f, level * 3.5f) - g) * 0.18f; };
    glowTo (subGlow, proc.stemLevel[stemSub].load());
    glowTo (addGlow, proc.stemLevel[stemAdd].load());
    glowTo (smpGlow, proc.stemLevel[stemSmp].load());
    energy = jmax (subGlow, addGlow, smpGlow);

    const auto notes = proc.engine.noteEvents.load();
    if (notes != lastNotes) { lastNotes = notes; flash = 1.0f; }
    flash *= 0.88f;

    for (int l = 0; l < 2; ++l)
    {
        const auto ev = proc.engine.grainEvents[l].load();
        const auto d = ev - lastEv[l];
        lastEv[l] = ev;
        const float lvl = l == 0 ? addGlow : smpGlow;
        if (d > 0 && lvl > 0.02f)
            spawn (l, jlimit (1, 7, (int) std::round (std::sqrt ((float) d) * 0.9f)));
    }
    // clean (non-grain) ADD still breathes a few soft sparks
    if (addGlow > 0.05f && proc.apvts.getRawParameterValue (ParamID::grainAdd)->load() < 0.5f && rng.nextFloat() < 0.5f)
        spawn (0, 1);

    const float motion = param (ParamID::motion);
    for (auto& p : parts)
    {
        p.vx += std::sin (p.y * 0.025f + t * 2.1f + p.seed) * motion * 160.0f * dt;
        p.vy += std::cos (p.x * 0.018f + t * 1.6f + p.seed) * motion * 90.0f * dt;
        p.vx *= 0.985f;
        p.vy *= 0.985f;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.rot += p.vrot * dt;
        p.life -= dt;
    }
    parts.erase (std::remove_if (parts.begin(), parts.end(), [] (const Particle& p) { return p.life <= 0.0f; }), parts.end());

    computeResult();
    repaint();
}

void Organism::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    {
        Path clip; clip.addRoundedRectangle (r, 14.0f);
        g.reduceClipRegion (clip);
    }
    g.drawImageAt (backdrop, 0, 0);

    const float motion = param (ParamID::motion);
    auto a = barArea();

    // ---- aurora ribbons ----
    const Colour ribbonCols[3] = { Palette::accent, Palette::violet, Palette::subBlue };
    for (int i = 0; i < 3; ++i)
    {
        Path p;
        for (float x = 0.0f; x <= r.getWidth() + 6.0f; x += 6.0f)
        {
            const float y = r.getHeight() * (0.52f + 0.06f * (float) i)
                          + std::sin (x * 0.006f * (1.0f + (float) i * 0.4f) + t * (0.6f + 0.25f * (float) i) + (float) i * 2.0f) * r.getHeight() * (0.08f + 0.22f * energy)
                          + std::sin (x * 0.017f - t * 1.3f * (1.0f + motion)) * r.getHeight() * 0.04f * (1.0f + motion * 2.0f);
            if (x == 0.0f) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (ribbonCols[i].withAlpha (0.03f + 0.07f * energy));
        g.strokePath (p, PathStrokeType (28.0f, PathStrokeType::curved));
        g.setColour (ribbonCols[i].withAlpha (0.12f + 0.3f * energy));
        g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved));
    }

    // ---- sampler ghost waveform ----
    if (proc.hasSample())
        if (auto pk = proc.getPeaks(); pk != nullptr && ! pk->maxs.empty())
        {
            const float alpha = 0.05f + 0.22f * smpGlow;
            Path w;
            const int n = (int) pk->maxs.size();
            const float mid = a.getCentreY();
            const float hh = a.getHeight() * 0.42f;
            w.startNewSubPath (r.getX(), mid);
            for (int i = 0; i < n; i += 2) w.lineTo (r.getWidth() * (float) i / (float) n, mid - pk->maxs[(size_t) i] * hh);
            for (int i = n - 1; i >= 0; i -= 2) w.lineTo (r.getWidth() * (float) i / (float) n, mid - pk->mins[(size_t) i] * hh);
            w.closeSubPath();
            g.setColour (Palette::violet.withAlpha (alpha));
            g.fillPath (w);
        }

    // ---- sub glow ----
    {
        const float cx = r.getCentreX(), cy = r.getBottom() + 30.0f;
        const float pulse = 0.85f + 0.15f * std::sin (t * 7.0f);
        ColourGradient sg (Palette::subBlue.withAlpha ((0.10f + 0.55f * subGlow) * pulse), cx, cy,
                           Palette::subBlue.withAlpha (0.0f), cx, cy - r.getHeight() * (0.35f + 0.35f * subGlow), true);
        g.setGradientFill (sg);
        g.fillEllipse (cx - r.getWidth() * 0.6f, cy - r.getHeight() * 0.8f, r.getWidth() * 1.2f, r.getHeight() * 1.6f);
        g.setColour (Palette::subBlue.withAlpha (0.25f + 0.5f * subGlow));
        g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth(), 2.0f);
    }

    if (flash > 0.01f)
    {
        g.setColour (Colours::white.withAlpha (0.045f * flash));
        g.fillRect (r);
    }

    // ---- bars ----
    for (int k = 0; k < granulum::kNumHarmonics; ++k)
    {
        auto s = slot (k);
        const float bw = s.getWidth() * 0.6f;
        auto col = s.withSizeKeepingCentre (bw, s.getHeight());
        const float v = bars[(size_t) k];
        const bool hov = (k == hoverBar);

        g.setColour (Palette::track.withAlpha (hov ? 0.85f : 0.55f));
        g.fillRoundedRectangle (col, 7.0f);
        g.setColour (Palette::accent.withAlpha (hov ? 0.5f : 0.08f));
        g.drawRoundedRectangle (col, 7.0f, hov ? 1.5f : 1.0f);

        if (v > 0.002f)
        {
            const float wobble = std::sin (t * 4.0f + (float) k * 0.9f) * 2.5f * addGlow;
            auto bar = col.withTop (jmin (col.getBottom() - 4.0f, col.getBottom() - v * col.getHeight() + wobble));
            const float glow = 0.08f + 0.35f * addGlow * (0.4f + v) + flash * 0.15f;
            g.setColour (Palette::accent.withAlpha (jmin (0.6f, glow)));
            g.fillRoundedRectangle (bar.expanded (5.0f, 5.0f), 10.0f);
            ColourGradient liquid (Palette::accentSoft.brighter (0.35f + 0.4f * addGlow), 0, bar.getY(),
                                   Palette::accent.darker (0.55f), 0, bar.getBottom(), false);
            g.setGradientFill (liquid);
            g.fillRoundedRectangle (bar, 7.0f);
            // rising shimmer band
            const float band = std::fmod (t * 0.45f + (float) k * 0.137f, 1.0f);
            const float by = bar.getBottom() - band * bar.getHeight();
            g.setColour (Colours::white.withAlpha (0.10f + 0.18f * addGlow));
            g.fillRect (bar.getX() + 3.0f, by, bar.getWidth() - 6.0f, 2.0f);
            // meniscus
            g.setColour (Colours::white.withAlpha (0.55f + 0.35f * addGlow));
            g.fillRoundedRectangle (bar.getX() + 3.0f, bar.getY() + 2.0f, bar.getWidth() - 6.0f, 2.5f, 1.2f);
        }

        g.setColour (Palette::textDim.withAlpha (hov ? 0.95f : 0.45f));
        g.setFont (uiFont (10.5f, hov));
        g.drawText (String (k + 1), Rectangle<float> (s.getX(), a.getBottom() + 5.0f, s.getWidth(), 14.0f), Justification::centred, false);
    }

    // ---- result curve: what actually comes out of the grain engine ----
    {
        const bool grainOn = proc.apvts.getRawParameterValue (ParamID::grainAdd)->load() > 0.5f;
        const float show = grainOn ? jmin (1.0f, param (ParamID::amount) * 1.5f) : 0.0f;
        if (show > 0.02f)
        {
            Path curve;
            std::array<Point<float>, granulum::kNumHarmonics> pts;
            for (int k = 0; k < granulum::kNumHarmonics; ++k)
                pts[(size_t) k] = { slot (k).getCentreX(), a.getBottom() - result[(size_t) k] * a.getHeight() };
            curve.startNewSubPath (pts[0]);
            for (int k = 1; k < granulum::kNumHarmonics; ++k)
            {
                const auto mid = (pts[(size_t) k - 1] + pts[(size_t) k]) * 0.5f;
                curve.quadraticTo (pts[(size_t) k - 1], mid);
            }
            curve.lineTo (pts.back());
            g.setColour (Palette::violet.withAlpha (0.10f * show));
            g.strokePath (curve, PathStrokeType (12.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Palette::violet.withAlpha (0.28f * show));
            g.strokePath (curve, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Palette::violetHi.withAlpha (0.95f * show));
            g.strokePath (curve, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
            for (auto& p : pts)
            {
                g.setColour (Palette::violetHi.withAlpha (0.25f * show));
                g.fillEllipse (Rectangle<float> (12.0f, 12.0f).withCentre (p));
                g.setColour (Colours::white.withAlpha (0.9f * show));
                g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (p));
            }
        }
    }

    // ---- grain shards ----
    for (auto& p : parts)
    {
        const float age = p.maxLife - p.life;
        const float al = jmin (1.0f, age / 0.12f) * std::pow (jmax (0.0f, p.life / p.maxLife), 0.8f);
        if (al <= 0.01f) continue;
        const float trail = 0.06f * (1.0f + motion * 2.5f);

        g.setColour (p.col.withAlpha (0.30f * al));
        g.drawLine (p.x - p.vx * trail, p.y - p.vy * trail, p.x, p.y, jmax (0.8f, p.size * 0.45f));
        g.setColour (p.col.withAlpha (0.10f * al));
        g.fillEllipse (Rectangle<float> (p.size * 5.2f, p.size * 5.2f).withCentre ({ p.x, p.y }));

        const auto tr = AffineTransform::rotation (p.rot).translated (p.x, p.y);
        if (p.kind == 0)
        {
            g.setColour (p.col.withAlpha (0.9f * al));
            g.fillEllipse (Rectangle<float> (p.size * 1.4f, p.size * 1.4f).withCentre ({ p.x, p.y }));
            g.setColour (Colours::white.withAlpha (0.8f * al));
            g.fillEllipse (Rectangle<float> (p.size * 0.6f, p.size * 0.6f).withCentre ({ p.x, p.y }));
        }
        else
        {
            Path shape;
            if (p.kind == 1)
            {
                shape.startNewSubPath (p.size * 1.3f, 0.0f);
                shape.lineTo (0.0f, p.size * 0.75f);
                shape.lineTo (-p.size * 1.3f, 0.0f);
                shape.lineTo (0.0f, -p.size * 0.75f);
            }
            else
            {
                shape.startNewSubPath (p.size * 2.0f, 0.0f);
                shape.lineTo (-p.size * 1.2f, p.size * 0.55f);
                shape.lineTo (-p.size * 0.6f, -p.size * 0.45f);
            }
            shape.closeSubPath();
            shape.applyTransform (tr);
            g.setColour (p.col.withAlpha (0.92f * al));
            g.fillPath (shape);
            g.setColour (Colours::white.withAlpha (0.55f * al));
            g.strokePath (shape, PathStrokeType (0.8f));
        }
    }

    // ---- HUD ----
    g.setFont (uiFont (12.0f, true, 0.1f));
    g.setColour (Palette::label.withAlpha (0.9f));
    g.drawText ("ADD", 22, 10, 40, 20, Justification::centredLeft, false);
    g.setFont (uiFont (11.5f, false, 0.04f));
    g.setColour (Palette::textDim.withAlpha (0.7f));
    g.drawText (String::fromUTF8 ("рисуй палочки мышкой  ·  правая кнопка стирает"), 58, 10, 360, 20, Justification::centredLeft, false);

    const float tone = param (ParamID::tone);
    const String hud = "TONE " + String (tone > 0.05f ? "+" : "") + String (tone, 1) + " st     TEXTURE "
                     + String (roundToInt (param (ParamID::texture) * 100.0f)) + "%     MOTION "
                     + String (roundToInt (motion * 100.0f)) + "%";
    g.setColour (Palette::violetHi.withAlpha (0.8f));
    g.setFont (uiFont (11.5f, false, 0.08f));
    g.drawText (hud, getLocalBounds().withTrimmedRight (22).withHeight (40), Justification::centredRight, false);

    if (hoverBar >= 0)
    {
        auto s = slot (hoverBar);
        const String txt = "H" + String (hoverBar + 1) + "  " + String (roundToInt (bars[(size_t) hoverBar] * 100.0f)) + "%";
        g.setColour (Colours::white);
        g.setFont (uiFont (11.0f, true));
        g.drawText (txt, Rectangle<float> (s.getCentreX() - 40.0f, a.getY() - 18.0f, 80.0f, 14.0f), Justification::centred, false);
    }

    if (fileHover)
    {
        g.setColour (Palette::screen.withAlpha (0.6f));
        g.fillRect (r);
        g.setColour (Palette::violetHi);
        g.setFont (uiFont (20.0f, true, 0.08f));
        g.drawText (String::fromUTF8 ("ОТПУСТИ: ЗАГРУЖУ В SAMPLER"), getLocalBounds(), Justification::centred, false);
        g.drawRoundedRectangle (r.reduced (3.0f), 12.0f, 2.0f);
    }

    // glass reflection
    ColourGradient glass (Colours::white.withAlpha (0.045f), 0, 0, Colours::transparentWhite, 0, r.getHeight() * 0.35f, false);
    g.setGradientFill (glass);
    g.fillRect (r.withHeight (r.getHeight() * 0.35f));
    g.setColour (Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
}

void Organism::applyMouse (const MouseEvent& e, bool first)
{
    auto a = barArea();
    const int bar = barAt (e.position.x);
    const float value = erasing ? 0.0f : jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
    if (first || lastBar < 0)
        bars[(size_t) bar] = value;
    else
    {
        const int steps = std::abs (bar - lastBar);
        for (int i = 0; i <= steps; ++i)
        {
            const int b = lastBar + (bar > lastBar ? i : -i);
            const float tt = steps == 0 ? 1.0f : (float) i / (float) steps;
            bars[(size_t) b] = lastValue + (value - lastValue) * tt;
        }
    }
    for (int i = 0; i < granulum::kNumHarmonics; ++i) proc.setHarmonic (i, bars[(size_t) i]);
    lastBar = bar;
    lastValue = value;
    hoverBar = bar;
}

void Organism::mouseDown (const MouseEvent& e)
{
    editing = true;
    erasing = e.mods.isRightButtonDown();
    lastBar = -1;
    applyMouse (e, true);
}

void Organism::mouseDrag (const MouseEvent& e) { if (editing) applyMouse (e, false); }
void Organism::mouseUp (const MouseEvent&)     { editing = false; lastBar = -1; }
void Organism::mouseMove (const MouseEvent& e)  { hoverBar = barArea().expanded (0, 30).contains (e.position) ? barAt (e.position.x) : -1; }
void Organism::mouseExit (const MouseEvent&)    { hoverBar = -1; }

// ============================================================================
// LFO editor

LfoEditor::LfoEditor (GranulumProcessor& p) : proc (p)
{
    setMouseCursor (MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

Rectangle<float> LfoEditor::area() const { return getLocalBounds().toFloat().reduced (10.0f, 9.0f); }

Point<float> LfoEditor::toScreen (float x, float y) const
{
    auto a = area();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

Point<float> LfoEditor::handlePos (size_t seg) const
{
    const auto& p0 = pts[seg];
    const auto& p1 = pts[seg + 1];
    const float y = p0.y + (p1.y - p0.y) * GranulumProcessor::segmentShape (0.5f, p0.curve);
    return toScreen ((p0.x + p1.x) * 0.5f, y);
}

int LfoEditor::hitPoint (Point<float> m) const
{
    for (size_t i = 0; i < pts.size(); ++i)
        if (toScreen (pts[i].x, pts[i].y).getDistanceFrom (m) < 8.0f) return (int) i;
    return -1;
}

int LfoEditor::hitHandle (Point<float> m) const
{
    for (size_t i = 0; i + 1 < pts.size(); ++i)
        if (pts[i + 1].x - pts[i].x > 0.02f && handlePos (i).getDistanceFrom (m) < 7.0f) return (int) i;
    return -1;
}

float LfoEditor::snapX (float x) const
{
    x = jlimit (0.0f, 1.0f, x);
    return grid > 0 ? std::round (x * (float) grid) / (float) grid : x;
}

void LfoEditor::mouseMove (const MouseEvent& e)
{
    pts = proc.getLfoPoints (lfo);
    const int h = hitPoint (e.position), hh = hitHandle (e.position);
    if (h != hover || hh != hoverHandle) { hover = h; hoverHandle = hh; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : hh >= 0 ? MouseCursor::UpDownResizeCursor : MouseCursor::CrosshairCursor);
}

void LfoEditor::mouseDown (const MouseEvent& e)
{
    pts = proc.getLfoPoints (lfo);
    dragPoint = dragHandle = -1;
    const int h = hitPoint (e.position);

    if (e.mods.isPopupMenu())
    {
        if (h > 0 && h < (int) pts.size() - 1)
        {
            pts.erase (pts.begin() + h);
            proc.setLfoPoints (lfo, pts);
        }
        return;
    }
    if (h >= 0) { dragPoint = h; return; }

    const int hh = hitHandle (e.position);
    if (hh >= 0)
    {
        dragHandle = hh;
        handleStartCurve = pts[(size_t) hh].curve;
        dragStartY = e.position.y;
        return;
    }

    // add a new point
    auto a = area();
    LfoPoint np { snapX ((e.position.x - a.getX()) / a.getWidth()), jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight()), 0.0f };
    if (np.x <= 0.0f || np.x >= 1.0f) return;
    size_t at = 1;
    while (at < pts.size() && pts[at].x <= np.x) ++at;
    pts.insert (pts.begin() + (long) at, np);
    dragPoint = (int) at;
    proc.setLfoPoints (lfo, pts);
}

void LfoEditor::mouseDrag (const MouseEvent& e)
{
    auto a = area();
    if (dragPoint >= 0 && dragPoint < (int) pts.size())
    {
        auto& p = pts[(size_t) dragPoint];
        p.y = jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
        if (dragPoint > 0 && dragPoint < (int) pts.size() - 1)
        {
            const float lo = pts[(size_t) dragPoint - 1].x, hi = pts[(size_t) dragPoint + 1].x;
            p.x = jlimit (lo, hi, snapX ((e.position.x - a.getX()) / a.getWidth()));
        }
        proc.setLfoPoints (lfo, pts);
    }
    else if (dragHandle >= 0 && dragHandle + 1 < (int) pts.size())
    {
        const float dy = e.position.y - dragStartY;
        const float dir = pts[(size_t) dragHandle + 1].y >= pts[(size_t) dragHandle].y ? 1.0f : -1.0f;
        pts[(size_t) dragHandle].curve = jlimit (-1.0f, 1.0f, handleStartCurve + dy / 70.0f * dir);
        proc.setLfoPoints (lfo, pts);
    }
}

void LfoEditor::mouseUp (const MouseEvent&) { dragPoint = dragHandle = -1; }

void LfoEditor::mouseDoubleClick (const MouseEvent& e)
{
    pts = proc.getLfoPoints (lfo);
    const int h = hitPoint (e.position);
    if (h > 0 && h < (int) pts.size() - 1) { pts.erase (pts.begin() + h); proc.setLfoPoints (lfo, pts); return; }
    const int hh = hitHandle (e.position);
    if (hh >= 0) { pts[(size_t) hh].curve = 0.0f; proc.setLfoPoints (lfo, pts); }
}

void LfoEditor::paint (Graphics& g)
{
    if (dragPoint < 0 && dragHandle < 0) pts = proc.getLfoPoints (lfo);
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 9.0f);
    auto a = area();
    const auto col = Palette::lfo (lfo);

    g.setColour (Colours::white.withAlpha (0.05f));
    const int gl = grid > 0 ? grid : 4;
    for (int i = 1; i < gl; ++i)
        g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / (float) gl), a.getY(), a.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());

    Path line, fill;
    const int n = jmax (2, (int) a.getWidth());
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / (float) n;
        const auto p = toScreen (x, proc.lfoShapeAt (lfo, x));
        if (i == 0) { line.startNewSubPath (p); fill.startNewSubPath (a.getX(), a.getBottom()); }
        line.lineTo (p);
        fill.lineTo (p);
    }
    fill.lineTo (a.getRight(), a.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (col.withAlpha (0.28f), 0, a.getY(), col.withAlpha (0.02f), 0, a.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col.withAlpha (0.25f));
    g.strokePath (line, PathStrokeType (5.0f));
    g.setColour (col);
    g.strokePath (line, PathStrokeType (1.8f));

    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        if (pts[i + 1].x - pts[i].x <= 0.02f) continue;
        const auto hp = handlePos (i);
        const bool hv = (int) i == hoverHandle || (int) i == dragHandle;
        g.setColour (col.withAlpha (hv ? 1.0f : 0.55f));
        Path d;
        const float s = hv ? 5.0f : 3.5f;
        d.addQuadrilateral (hp.x, hp.y - s, hp.x + s, hp.y, hp.x, hp.y + s, hp.x - s, hp.y);
        g.strokePath (d, PathStrokeType (1.3f));
    }
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto p = toScreen (pts[i].x, pts[i].y);
        const bool hv = (int) i == hover || (int) i == dragPoint;
        const float s = hv ? 11.0f : 8.0f;
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (Rectangle<float> (s + 6.0f, s + 6.0f).withCentre (p));
        g.setColour (Palette::screen);
        g.fillEllipse (Rectangle<float> (s, s).withCentre (p));
        g.setColour (hv ? Colours::white : col);
        g.drawEllipse (Rectangle<float> (s, s).withCentre (p), 1.8f);
    }

    const float ph = proc.lfoPhaseOut[(size_t) lfo].load();
    const auto pp = toScreen (ph, proc.lfoValueOut[(size_t) lfo].load());
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawVerticalLine ((int) pp.x, a.getY(), a.getBottom());
    g.setColour (col.withAlpha (0.4f));
    g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre (pp));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (pp));
}
