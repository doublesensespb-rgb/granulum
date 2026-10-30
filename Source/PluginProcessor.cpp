#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    String fmt (float v, int dec, const char* unit) { return String (v, dec) + unit; }

    constexpr int kChunk = 32;

    const char* const kModTargetIds[kNumModTargets] =
    {
        ParamID::subVol, ParamID::subOct, ParamID::subSemi,
        ParamID::addVol, ParamID::addOct, ParamID::addSemi,
        ParamID::smpVol, ParamID::smpOct, ParamID::smpSemi,
        ParamID::tone, ParamID::texture, ParamID::motion, ParamID::amount,
        ParamID::cutoff, ParamID::reso,
        ParamID::dirtDrive, ParamID::dirtTone, ParamID::dirtMix,
        ParamID::spaceAmt, ParamID::spaceSize,
        ParamID::glide, ParamID::pitch, ParamID::output,
        "lfo1Rate", "lfo2Rate"
    };
}

// ===========================================================================
// Distortion

const StringArray& Distortion::typeNames()
{
    static const StringArray s { "TUBE", "HARD", "FOLD", "CRUSH", "RECTIFY" };
    return s;
}

float Distortion::shape (int type, float x, float drive, int ch)
{
    switch (type)
    {
        case 0:
        {
            const float u = x * (1.0f + 20.0f * drive * drive);
            return u >= 0.0f ? std::tanh (u) : 0.8f * std::tanh (u * 0.8f);
        }
        case 1:
            return jlimit (-1.0f, 1.0f, x * (1.0f + 30.0f * drive * drive));
        case 2:
            return std::sin (x * (1.0f + 12.0f * drive) * MathConstants<float>::halfPi);
        case 3:
        {
            const float bits = 16.0f - 13.0f * drive;
            const float levels = std::exp2 (bits - 1.0f);
            const int factor = 1 + (int) (drive * drive * 40.0f);
            if (++count[ch] >= factor)
            {
                count[ch] = 0;
                hold[ch] = std::round (jlimit (-1.0f, 1.0f, x) * levels) / levels;
            }
            return hold[ch];
        }
        default:
            return std::tanh (std::abs (x) * (1.0f + 10.0f * drive * drive)) * 1.6f;
    }
}

void Distortion::process (float* L, float* R, int n, int type, float drive, float tone, float mix)
{
    const float fc = 400.0f * std::pow (50.0f, jlimit (0.0f, 1.0f, tone));
    const float a = std::exp (-MathConstants<float>::twoPi * jmin (fc, (float) sr * 0.45f) / (float) sr);
    float* ch[2] = { L, R };
    for (int c = 0; c < 2; ++c)
    {
        float* x = ch[c];
        for (int i = 0; i < n; ++i)
        {
            const float y = shape (type, x[i], drive, c);
            const float d = y - dcX[c] + 0.995f * dcY[c];
            dcX[c] = y; dcY[c] = d;
            lp[c] = (1.0f - a) * d + a * lp[c];
            x[i] = x[i] * (1.0f - mix) + lp[c] * mix;
        }
    }
}

// ===========================================================================
// Static helpers

const char* GranulumProcessor::modTargetId (int t) { return (t >= 0 && t < kNumModTargets) ? kModTargetIds[t] : ""; }

int GranulumProcessor::modIndexOf (const String& id)
{
    for (int t = 0; t < kNumModTargets; ++t)
        if (id == kModTargetIds[t]) return t;
    return -1;
}

const StringArray& GranulumProcessor::divisionNames()
{
    static const StringArray s { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                 "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
    return s;
}

double GranulumProcessor::divisionBeats (int i)
{
    static const double b[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 3.0, 1.5, 0.75 };
    return b[jlimit (0, (int) std::size (b) - 1, i)];
}

float GranulumProcessor::segmentShape (float t, float curve)
{
    if (std::abs (curve) < 0.001f) return t;
    const float k = curve * 6.0f;
    return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
}

const StringArray& GranulumProcessor::lfoPresetNames()
{
    static const StringArray s { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "STEPS", "PUMP" };
    return s;
}

std::vector<LfoPoint> GranulumProcessor::lfoPreset (int i)
{
    switch (i)
    {
        case 0:  return { { 0, 0, 0.3f }, { 0.25f, 0.5f, -0.3f }, { 0.5f, 1, 0.3f }, { 0.75f, 0.5f, -0.3f }, { 1, 0, 0 } };
        case 1:  return { { 0, 0, 0 }, { 0.5f, 1, 0 }, { 1, 0, 0 } };
        case 2:  return { { 0, 0, 0 }, { 1, 1, 0 } };
        case 3:  return { { 0, 1, 0 }, { 1, 0, 0 } };
        case 4:  return { { 0, 1, 0 }, { 0.5f, 1, 0 }, { 0.5f, 0, 0 }, { 1, 0, 0 } };
        case 5:  return { { 0, 0, 0 }, { 0.25f, 0, 0 }, { 0.25f, 0.33f, 0 }, { 0.5f, 0.33f, 0 }, { 0.5f, 0.66f, 0 },
                          { 0.75f, 0.66f, 0 }, { 0.75f, 1, 0 }, { 1, 1, 0 } };
        default: return { { 0, 0, -0.75f }, { 1, 1, 0 } };
    }
}

// ===========================================================================

AudioProcessorValueTreeState::ParameterLayout GranulumProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    using B = AudioParameterBool;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto pct     = A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto octFmt  = A().withStringFromValueFunction ([] (float v, int) { const int o = roundToInt (v); return (o > 0 ? "+" : "") + String (o) + " oct"; });
    auto semiFmt = A().withStringFromValueFunction ([] (float v, int) { const int s = roundToInt (v); return (s > 0 ? "+" : "") + String (s) + " st"; });
    auto hzFmt   = A().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? fmt (v, 0, " Hz") : fmt (v / 1000.0f, 1, " kHz"); });

    auto osc = [&] (const char* vol, const char* oct, const char* semi, const String& name, float defVol, float defOct)
    {
        ps.push_back (std::make_unique<P> (ParameterID { vol, 1 },  name + " Volume", NormalisableRange<float> (0.0f, 1.0f), defVol, pct));
        ps.push_back (std::make_unique<P> (ParameterID { oct, 1 },  name + " Octave", NormalisableRange<float> (-3.0f, 3.0f, 1.0f), defOct, octFmt));
        ps.push_back (std::make_unique<P> (ParameterID { semi, 1 }, name + " Semi",   NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f, semiFmt));
    };
    osc (ParamID::subVol, ParamID::subOct, ParamID::subSemi, "Sub",     0.7f, -1.0f);
    osc (ParamID::addVol, ParamID::addOct, ParamID::addSemi, "Add",     0.8f,  0.0f);
    osc (ParamID::smpVol, ParamID::smpOct, ParamID::smpSemi, "Sampler", 0.8f,  0.0f);

    // grain
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::grainAdd, 1 }, "Grain on Add", true));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::grainSmp, 1 }, "Grain on Sampler", true));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::tone, 1 }, "Tone", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + fmt (v, 1, " st"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::texture, 1 }, "Texture", NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::motion, 1 },  "Motion",  NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::amount, 1 },  "Amount",  NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    // filter
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::filterSub, 1 }, "Filter Sub", false));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::filterAdd, 1 }, "Filter Add", true));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::filterSmp, 1 }, "Filter Sampler", true));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::cutoff, 1 }, "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hzFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reso, 1 },   "Resonance", skewed (0.5f, 10.0f, 1.5f), 0.707f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, ""); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::ftype, 1 }, "Filter Type", StringArray { "LOW PASS", "BAND PASS", "HIGH PASS" }, 0));

    // dirt
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::dirtSub, 1 }, "Dirt Sub", false));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::dirtAdd, 1 }, "Dirt Add", true));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::dirtSmp, 1 }, "Dirt Sampler", true));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::dirtType, 1 }, "Dirt Type", Distortion::typeNames(), 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::dirtDrive, 1 }, "Dirt Drive", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::dirtTone, 1 },  "Dirt Tone",  NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::dirtMix, 1 },   "Dirt Mix",   NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    // space
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::spaceSub, 1 }, "Space Sub", false));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::spaceAdd, 1 }, "Space Add", true));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::spaceSmp, 1 }, "Space Sampler", true));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::spaceAmt, 1 },  "Space Amount", NormalisableRange<float> (0.0f, 1.0f), 0.12f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::spaceSize, 1 }, "Space Size",   NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));

    // voice
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::mono, 1 }, "Mono", true));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::glide, 1 }, "Glide", skewed (0.0f, 2000.0f, 150.0f), 40.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? String ("OFF") : v < 1000.0f ? fmt (v, 0, " ms") : fmt (v / 1000.0f, 2, " s"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pitch, 1 }, "Pitch", NormalisableRange<float> (-12.0f, 12.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0.005f ? "+" : "") + fmt (v, 2, " st"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -3.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 1, " dB"); })));

    // LFOs
    for (int l = 0; l < kNumLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfo (l, "Rate"), 1 }, n + "Rate", skewed (0.02f, 30.0f, 2.0f), l == 0 ? 1.0f : 0.25f,
                                           A().withStringFromValueFunction ([] (float v, int) { return fmt (v, v < 1.0f ? 2 : 1, " Hz"); })));
        ps.push_back (std::make_unique<B> (ParameterID { ParamID::lfo (l, "Sync"), 1 }, n + "Sync", true));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Div"), 1 },  n + "Division", divisionNames(), l == 0 ? 4 : 2));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Mode"), 1 }, n + "Mode", StringArray { "FREE", "RETRIG" }, 0));
    }

    return { ps.begin(), ps.end() };
}

GranulumProcessor::GranulumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRANULUM", createLayout())
{
    formatManager.registerBasicFormats();

    for (int t = 0; t < kNumModTargets; ++t)
    {
        modParams[(size_t) t] = apvts.getParameter (kModTargetIds[t]);
        jassert (modParams[(size_t) t] != nullptr);
        modNorm[(size_t) t].store (modParams[(size_t) t]->getValue());
        lastV[(size_t) t] = modParams[(size_t) t]->convertFrom0to1 (modParams[(size_t) t]->getValue());
    }

    pGrainAdd = apvts.getRawParameterValue (ParamID::grainAdd);
    pGrainSmp = apvts.getRawParameterValue (ParamID::grainSmp);
    pFilterOn = { apvts.getRawParameterValue (ParamID::filterSub), apvts.getRawParameterValue (ParamID::filterAdd), apvts.getRawParameterValue (ParamID::filterSmp) };
    pDirtOn   = { apvts.getRawParameterValue (ParamID::dirtSub),   apvts.getRawParameterValue (ParamID::dirtAdd),   apvts.getRawParameterValue (ParamID::dirtSmp) };
    pSpaceOn  = { apvts.getRawParameterValue (ParamID::spaceSub),  apvts.getRawParameterValue (ParamID::spaceAdd),  apvts.getRawParameterValue (ParamID::spaceSmp) };
    pFtype    = apvts.getRawParameterValue (ParamID::ftype);
    pDirtType = apvts.getRawParameterValue (ParamID::dirtType);
    pMono     = apvts.getRawParameterValue (ParamID::mono);
    for (int l = 0; l < kNumLfos; ++l)
    {
        pLfoSync[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Sync"));
        pLfoDiv[(size_t) l]  = apvts.getRawParameterValue (ParamID::lfo (l, "Div"));
        pLfoMode[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Mode"));
    }

    const float init[] = { 1.0f, 0.5f, 0.3f, 0.2f, 0.12f, 0.08f, 0.05f };
    for (size_t i = 0; i < std::size (init); ++i) harmonics[i].store (init[i]);
    for (int i = 0; i < granulum::kNumHarmonics; ++i) harmonicsSmoothed[(size_t) i] = harmonics[(size_t) i].load();

    for (int l = 0; l < kNumLfos; ++l)
        setLfoPoints (l, lfoPreset (l == 0 ? 0 : 1));

    startTimerHz (20);
}

GranulumProcessor::~GranulumProcessor()
{
    stopTimer();
}

bool GranulumProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void GranulumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);

    const int n = jmax (kChunk, samplesPerBlock);
    dsp::ProcessSpec spec { sampleRate, (uint32) n, 2 };
    for (auto& f : filters) { f.prepare (spec); f.reset(); }
    for (auto& d : dirts) d.prepare (sampleRate);
    reverb.prepare (spec); reverb.reset();
    monoSplit.prepare (spec); monoSplit.reset();
    monoSplit.setCutoffFrequency (100.0f);

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));

    for (auto& s : stems) s.setSize (2, n, false, true, true);
    mixBuf.setSize (2, n, false, true, true);
    sendBuf.setSize (2, n, false, true, true);

    heldNotes = 0;
    for (int l = 0; l < kNumLfos; ++l) { lfoPhase[(size_t) l] = 0.0; lfoValue[(size_t) l] = lfoShapeAt (l, 0.0f); }
}

// ===========================================================================
// LFO shapes

std::vector<LfoPoint> GranulumProcessor::getLfoPoints (int l) const
{
    const ScopedLock sl (lfoLock);
    return lfoPoints[(size_t) l];
}

void GranulumProcessor::setLfoPoints (int l, std::vector<LfoPoint> pts)
{
    std::sort (pts.begin(), pts.end(), [] (const LfoPoint& a, const LfoPoint& b) { return a.x < b.x; });
    if (pts.size() < 2) pts = lfoPreset (1);
    pts.front().x = 0.0f;
    pts.back().x = 1.0f;
    for (auto& p : pts) { p.y = jlimit (0.0f, 1.0f, p.y); p.curve = jlimit (-1.0f, 1.0f, p.curve); }
    {
        const ScopedLock sl (lfoLock);
        lfoPoints[(size_t) l] = std::move (pts);
    }
    rebuildLfoTable (l);
}

void GranulumProcessor::rebuildLfoTable (int l)
{
    const auto pts = getLfoPoints (l);
    const int next = 1 - lfoTableIndex[(size_t) l].load();
    auto& t = lfoTables[(size_t) l][(size_t) next];
    size_t seg = 0;
    for (int i = 0; i <= kLfoTable; ++i)
    {
        const float x = (float) i / (float) kLfoTable;
        while (seg + 2 < pts.size() && x > pts[seg + 1].x) ++seg;
        const auto& a = pts[seg];
        const auto& b = pts[seg + 1];
        float y;
        if (b.x - a.x < 1.0e-5f) y = b.y;
        else y = a.y + (b.y - a.y) * segmentShape (jlimit (0.0f, 1.0f, (x - a.x) / (b.x - a.x)), a.curve);
        t[(size_t) i] = y;
    }
    lfoTableIndex[(size_t) l].store (next, std::memory_order_release);
}

float GranulumProcessor::lfoShapeAt (int l, float phase) const
{
    const auto& t = lfoTables[(size_t) l][(size_t) lfoTableIndex[(size_t) l].load (std::memory_order_acquire)];
    const float p = jlimit (0.0f, 1.0f, phase) * (float) kLfoTable;
    const int i = jmin (kLfoTable - 1, (int) p);
    const float f = p - (float) i;
    return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
}

String GranulumProcessor::pointsToString (const std::vector<LfoPoint>& pts)
{
    StringArray s;
    for (auto& p : pts) s.add (String (p.x, 4) + "," + String (p.y, 4) + "," + String (p.curve, 3));
    return s.joinIntoString (";");
}

std::vector<LfoPoint> GranulumProcessor::pointsFromString (const String& str)
{
    std::vector<LfoPoint> pts;
    for (auto& e : StringArray::fromTokens (str, ";", ""))
    {
        const auto v = StringArray::fromTokens (e, ",", "");
        if (v.size() == 3) pts.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(), v[2].getFloatValue() });
    }
    return pts;
}

// ===========================================================================
// Modulation

void GranulumProcessor::setModulation (int target, int lfo, float depth)
{
    if (target < 0 || target >= kNumModTargets || lfo < 0 || lfo >= kNumLfos) return;
    depth = jlimit (-1.0f, 1.0f, depth);
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) { m.depth.store (depth); ++modVersion; return; }
    for (auto& m : mods)
        if (m.target.load() < 0)
        {
            m.lfo.store (lfo);
            m.depth.store (depth);
            m.target.store (target, std::memory_order_release);
            ++modVersion;
            return;
        }
}

void GranulumProcessor::removeModulation (int target, int lfo)
{
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) m.target.store (-1);
    ++modVersion;
}

bool GranulumProcessor::hasModulation (int target, int lfo) const
{
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) return true;
    return false;
}

float GranulumProcessor::getModulationDepth (int target, int lfo) const
{
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) return m.depth.load();
    return 0.0f;
}

int GranulumProcessor::countModulations (int lfo) const
{
    int n = 0;
    for (auto& m : mods)
        if (m.target.load() >= 0 && m.lfo.load() == lfo) ++n;
    return n;
}

void GranulumProcessor::computeModulated (float* v)
{
    float off[kNumModTargets] = {};
    for (auto& m : mods)
    {
        const int t = m.target.load (std::memory_order_acquire);
        if (t < 0 || t >= kNumModTargets) continue;
        const int l = jlimit (0, kNumLfos - 1, m.lfo.load (std::memory_order_relaxed));
        off[t] += m.depth.load (std::memory_order_relaxed) * lfoValue[(size_t) l];
    }
    for (int t = 0; t < kNumModTargets; ++t)
    {
        auto* p = modParams[(size_t) t];
        const float norm = jlimit (0.0f, 1.0f, p->getValue() + off[t]);
        v[t] = p->convertFrom0to1 (norm);   // not snapped: Oct/Semi move smoothly under modulation
        modNorm[(size_t) t].store (norm, std::memory_order_relaxed);
    }
}

void GranulumProcessor::advanceLfos (int endSample, int len, const float* v)
{
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto L = (size_t) l;
        const bool sync = pLfoSync[L]->load() > 0.5f;
        const bool free = (int) pLfoMode[L]->load() == 0;
        const double beats = divisionBeats ((int) pLfoDiv[L]->load());

        double ph;
        if (sync && free && hostPlaying && hostPpq >= 0.0)
            ph = (hostPpq + (double) endSample / sr * hostBpm / 60.0) / beats;
        else
            ph = lfoPhase[L] + (sync ? (hostBpm / 60.0) / beats : (double) v[mtLfo1Rate + l]) * (double) len / sr;
        ph -= std::floor (ph);

        lfoPhase[L] = ph;
        lfoValue[L] = lfoShapeAt (l, (float) ph);
        lfoValueOut[L].store (lfoValue[L], std::memory_order_relaxed);
        lfoPhaseOut[L].store ((float) ph, std::memory_order_relaxed);
    }
}

// ===========================================================================
// Audio

granulum::Params GranulumProcessor::makeParams (const float* v)
{
    granulum::Params p;
    p.sub.pitch = v[mtSubOct] * 12.0f + v[mtSubSemi];
    p.add.pitch = v[mtAddOct] * 12.0f + v[mtAddSemi];
    p.smp.pitch = v[mtSmpOct] * 12.0f + v[mtSmpSemi];
    p.sub.run = v[mtSubVol] > 0.0001f || lastVol[stemSub] > 0.0001f;
    p.add.run = v[mtAddVol] > 0.0001f || lastVol[stemAdd] > 0.0001f;
    p.smp.run = v[mtSmpVol] > 0.0001f || lastVol[stemSmp] > 0.0001f;
    p.add.grain = pGrainAdd->load() > 0.5f;
    p.smp.grain = pGrainSmp->load() > 0.5f;
    p.tone    = v[mtTone];
    p.texture = v[mtTexture];
    p.motion  = v[mtMotion];
    p.amount  = v[mtAmount];
    p.globalPitch = v[mtPitch];
    p.mono    = pMono->load() > 0.5f;
    p.glideMs = v[mtGlide];
    p.smpRoot = getRootNote();
    for (int i = 0; i < granulum::kNumHarmonics; ++i) p.harmonics[(size_t) i] = harmonicsSmoothed[(size_t) i];
    return p;
}

void GranulumProcessor::noteOn (int note, float velocity)
{
    if (heldNotes == 0)
        for (int l = 0; l < kNumLfos; ++l)
            if ((int) pLfoMode[(size_t) l]->load() == 1)
            {
                lfoPhase[(size_t) l] = 0.0;
                lfoValue[(size_t) l] = lfoShapeAt (l, 0.0f);
            }
    ++heldNotes;
    engine.noteOn (note, velocity, makeParams (lastV.data()), smpSlot.active.get());
}

void GranulumProcessor::noteOff (int note)
{
    heldNotes = jmax (0, heldNotes - 1);
    engine.noteOff (note, makeParams (lastV.data()));
}

void GranulumProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    smpSlot.swapOnAudioThread();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (mixBuf.getNumSamples() < numSamples)
    {
        for (auto& s : stems) s.setSize (2, numSamples, false, false, true);
        mixBuf.setSize (2, numSamples, false, false, true);
        sendBuf.setSize (2, numSamples, false, false, true);
    }
    for (auto& s : stems) s.clear();
    mixBuf.clear();
    sendBuf.clear();

    hostBpm = 120.0; hostPpq = -1.0; hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())         hostBpm = jlimit (20.0, 999.0, *b);
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
            hostPlaying = pos->getIsPlaying();
        }

    blockPeak = 0.0f;
    blockSamples = 0;
    stemEnergy = {};

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, numSamples, meta.samplePosition);
        renderRange (pos, at);
        pos = jmax (pos, at);
        const auto m = meta.getMessage();
        if (m.isNoteOn())       noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { heldNotes = 0; engine.allNotesOff(); }
    }
    renderRange (pos, numSamples);
    midi.clear();

    outputLevel.store (jmax (blockPeak, outputLevel.load() * 0.9f));
    for (int s = 0; s < kNumStems; ++s)
        stemLevel[(size_t) s].store ((float) std::sqrt (stemEnergy[(size_t) s] / jmax (1, blockSamples)));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mixBuf, jmin (ch, 1), 0, numSamples);
}

void GranulumProcessor::renderRange (int from, int to)
{
    while (from < to)
    {
        const int len = jmin (kChunk, to - from);
        processChunk (from, len);
        from += len;
    }
}

void GranulumProcessor::processChunk (int from, int len)
{
    float v[kNumModTargets];
    computeModulated (v);
    std::copy (v, v + kNumModTargets, lastV.begin());

    // bars glide smoothly to their new heights (no zipper while drawing)
    for (int i = 0; i < granulum::kNumHarmonics; ++i)
        harmonicsSmoothed[(size_t) i] += (harmonics[(size_t) i].load (std::memory_order_relaxed) - harmonicsSmoothed[(size_t) i]) * 0.2f;

    const auto p = makeParams (v);

    float* sub  = stems[stemSub].getWritePointer (0) + from;
    float* addL = stems[stemAdd].getWritePointer (0) + from;
    float* addR = stems[stemAdd].getWritePointer (1) + from;
    float* smpL = stems[stemSmp].getWritePointer (0) + from;
    float* smpR = stems[stemSmp].getWritePointer (1) + from;
    engine.render (sub, addL, addR, smpL, smpR, len, p, smpSlot.active.get());
    std::copy (sub, sub + len, stems[stemSub].getWritePointer (1) + from);

    const float vols[kNumStems] = { v[mtSubVol], v[mtAddVol], v[mtSmpVol] };

    const int ft = (int) pFtype->load();
    const auto ftype = ft == 0 ? dsp::StateVariableTPTFilterType::lowpass
                     : ft == 1 ? dsp::StateVariableTPTFilterType::bandpass
                               : dsp::StateVariableTPTFilterType::highpass;
    const bool filterActive = ! (ft == 0 && v[mtCutoff] > 19500.0f && v[mtReso] < 0.75f);
    const bool dirtActive = v[mtDirtDrive] > 0.001f && v[mtDirtMix] > 0.001f;

    float* mL = mixBuf.getWritePointer (0) + from;
    float* mR = mixBuf.getWritePointer (1) + from;
    float* sL = sendBuf.getWritePointer (0) + from;
    float* sR = sendBuf.getWritePointer (1) + from;

    for (int s = 0; s < kNumStems; ++s)
    {
        float* L = stems[(size_t) s].getWritePointer (0) + from;
        float* R = stems[(size_t) s].getWritePointer (1) + from;

        // volume (ramped)
        for (int i = 0; i < len; ++i)
        {
            const float g = lastVol[(size_t) s] + (vols[s] - lastVol[(size_t) s]) * (float) (i + 1) / (float) len;
            L[i] *= g; R[i] *= g;
        }
        lastVol[(size_t) s] = vols[s];

        if (filterActive && pFilterOn[(size_t) s]->load() > 0.5f)
        {
            auto& f = filters[(size_t) s];
            f.setType (ftype);
            f.setCutoffFrequency (jlimit (20.0f, (float) (getSampleRate() * 0.45), v[mtCutoff]));
            f.setResonance (v[mtReso]);
            auto blk = dsp::AudioBlock<float> (stems[(size_t) s]).getSubBlock ((size_t) from, (size_t) len);
            dsp::ProcessContextReplacing<float> ctx (blk);
            f.process (ctx);
        }

        if (dirtActive && pDirtOn[(size_t) s]->load() > 0.5f)
            dirts[(size_t) s].process (L, R, len, (int) pDirtType->load(), v[mtDirtDrive], v[mtDirtTone], v[mtDirtMix]);

        const bool send = pSpaceOn[(size_t) s]->load() > 0.5f;
        double e = 0.0;
        for (int i = 0; i < len; ++i)
        {
            mL[i] += L[i];
            mR[i] += R[i];
            if (send) { sL[i] += L[i]; sR[i] += R[i]; }
            e += (double) (L[i] * L[i] + R[i] * R[i]) * 0.5;
        }
        stemEnergy[(size_t) s] += e;
    }

    // ---- SPACE: reverb as a send (wet only), low end kept out of the tail ----
    const float amt = v[mtSpaceAmt];
    if (amt > 0.001f)
    {
        const float a = 1.0f - std::exp (-MathConstants<float>::twoPi * 150.0f / (float) getSampleRate());
        float* ch[2] = { sL, sR };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i)
            {
                sendHp[c] += a * (ch[c][i] - sendHp[c]);
                ch[c][i] -= sendHp[c];
            }
        dsp::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.69f * v[mtSpaceSize];
        rp.damping  = 0.45f;
        rp.wetLevel = amt;
        rp.dryLevel = 0.0f;
        rp.width    = 1.0f;
        reverb.setParameters (rp);
        auto blk = dsp::AudioBlock<float> (sendBuf).getSubBlock ((size_t) from, (size_t) len);
        dsp::ProcessContextReplacing<float> ctx (blk);
        reverb.process (ctx);
        for (int i = 0; i < len; ++i) { mL[i] += sL[i]; mR[i] += sR[i]; }
    }

    // ---- everything below 100 Hz goes mono, then output ----
    outGain.setTargetValue (Decibels::decibelsToGain (v[mtOutput]));
    for (int i = 0; i < len; ++i)
    {
        float lowL, highL, lowR, highR;
        monoSplit.processSample (0, mL[i], lowL, highL);
        monoSplit.processSample (1, mR[i], lowR, highR);
        const float low = 0.5f * (lowL + lowR);
        const float g = outGain.getNextValue();
        mL[i] = std::tanh ((low + highL) * g);
        mR[i] = std::tanh ((low + highR) * g);
        blockPeak = jmax (blockPeak, std::abs (mL[i]), std::abs (mR[i]));
    }
    blockSamples += len;

    advanceLfos (from + len, len, v);
}

// ===========================================================================
// Sampler

bool GranulumProcessor::isSupportedAudioFile (const String& path)
{
    const auto ext = File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == ".mp3";
}

bool GranulumProcessor::loadSample (const File& file, String& error)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) { error = "Can't read this file"; return false; }

    const auto frames = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 300.0));
    if (frames < 64) { error = "File is too short"; return false; }

    const int chans = jlimit (1, 2, (int) reader->numChannels);
    AudioBuffer<float> tmp (chans, frames);
    reader->read (&tmp, 0, frames, 0, true, chans > 1);

    auto data = std::make_shared<granulum::SampleData>();
    data->sampleRate = reader->sampleRate;
    data->data.resize ((size_t) frames);
    for (int i = 0; i < frames; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < chans; ++c) s += tmp.getSample (c, i);
        data->data[(size_t) i] = s / (float) chans;
    }
    float mag = 0.0f;
    for (float s : data->data) mag = jmax (mag, std::abs (s));
    if (mag > 1.0e-5f)
        for (auto& s : data->data) s *= Decibels::decibelsToGain (-1.0f) / mag;

    const double hz = granulum::detectPitchHz (data->data, data->sampleRate);
    data->pitched = hz > 0.0;
    data->detectedRoot = hz > 0.0 ? 69.0 + 12.0 * std::log2 (hz / 440.0) : 60.0;

    auto pk = std::make_shared<WaveformPeaks>();
    const int bins = 1024;
    pk->mins.assign (bins, 0.0f);
    pk->maxs.assign (bins, 0.0f);
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((int64) b * frames / bins);
        const int s1 = jmax (s0 + 1, (int) ((int64) (b + 1) * frames / bins));
        float lo = 0.0f, hi = 0.0f;
        for (int i = s0; i < s1 && i < frames; ++i) { lo = jmin (lo, data->data[(size_t) i]); hi = jmax (hi, data->data[(size_t) i]); }
        pk->mins[(size_t) b] = lo;
        pk->maxs[(size_t) b] = hi;
    }

    {
        const ScopedLock sl (infoLock);
        samplePath = file;
        peaks = std::move (pk);
    }
    detectedRoot.store ((float) data->detectedRoot);
    rootDetected.store (data->pitched);
    rootShift.store (0);
    smpSlot.push (std::move (data));
    sampleLoaded.store (true);
    return true;
}

String GranulumProcessor::getSampleName() const
{
    const ScopedLock sl (infoLock);
    return samplePath == File() ? String() : samplePath.getFileNameWithoutExtension();
}

std::shared_ptr<const WaveformPeaks> GranulumProcessor::getPeaks() const
{
    const ScopedLock sl (infoLock);
    return peaks;
}

float GranulumProcessor::getRootNote() const
{
    return detectedRoot.load() + (float) rootShift.load();
}

void GranulumProcessor::shiftRoot (int semitones)
{
    rootShift.store (jlimit (-48, 48, rootShift.load() + semitones));
}

void GranulumProcessor::timerCallback()
{
    smpSlot.collect();
}

// ===========================================================================
// Smart random

std::vector<GranulumProcessor::RandomTarget> GranulumProcessor::makeRandom (int section)
{
    Random r;
    std::vector<RandomTarget> out;
    auto add = [&] (const char* id, float real)
    {
        if (auto* p = apvts.getParameter (id))
            out.push_back ({ id, p->convertTo0to1 (real) });
    };
    auto range = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto logRange = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };

    if (section == 1)
    {
        const float tone = r.nextFloat() < 0.45f ? range (-3.0f, 3.0f) : (r.nextBool() ? 1.0f : -1.0f) * range (4.0f, 12.0f);
        add (ParamID::tone, tone);
        add (ParamID::texture, range (0.1f, 0.85f));
        add (ParamID::motion, r.nextFloat() < 0.5f ? range (0.0f, 0.15f) : range (0.15f, 0.75f));
        add (ParamID::amount, range (0.6f, 1.0f));
    }
    else if (section == 2)
    {
        const int type = r.nextInt (3);
        if (auto* p = apvts.getParameter (ParamID::ftype)) out.push_back ({ ParamID::ftype, p->convertTo0to1 ((float) type) });
        add (ParamID::cutoff, type == 0 ? logRange (250.0f, 6000.0f) : type == 1 ? logRange (300.0f, 2500.0f) : logRange (80.0f, 900.0f));
        add (ParamID::reso, logRange (0.6f, 3.5f));
    }
    else if (section == 3)
    {
        if (auto* p = apvts.getParameter (ParamID::dirtType)) out.push_back ({ ParamID::dirtType, p->convertTo0to1 ((float) r.nextInt (5)) });
        add (ParamID::dirtDrive, range (0.25f, 0.85f));
        add (ParamID::dirtTone, range (0.35f, 1.0f));
        add (ParamID::dirtMix, range (0.4f, 1.0f));
    }
    else if (section == 4)
    {
        add (ParamID::spaceAmt, range (0.05f, 0.45f));
        add (ParamID::spaceSize, range (0.3f, 0.95f));
    }
    return out;
}

std::array<float, granulum::kNumHarmonics> GranulumProcessor::makeRandomBars()
{
    Random r;
    std::array<float, granulum::kNumHarmonics> h {};
    const int style = r.nextInt (4);
    const int peak = 2 + r.nextInt (10);
    for (int k = 1; k <= granulum::kNumHarmonics; ++k)
    {
        float v = 0.0f;
        switch (style)
        {
            case 0: v = 1.0f / (float) k * (0.7f + 0.6f * r.nextFloat()); break;                         // saw-like
            case 1: v = (k % 2 == 1) ? 1.0f / (float) k * (0.7f + 0.6f * r.nextFloat()) : 0.0f; break;  // hollow / square-like
            case 2: v = r.nextFloat() < 0.4f ? r.nextFloat() / std::sqrt ((float) k) : 0.0f; break;      // sparse
            default:                                                                                       // formant bump
                v = std::exp (-0.5f * std::pow (((float) k - (float) peak) / 1.6f, 2.0f)) * (0.6f + 0.4f * r.nextFloat())
                    + 0.15f / (float) k;
        }
        h[(size_t) k - 1] = jlimit (0.0f, 1.0f, v);
    }
    h[0] = 0.65f + 0.35f * r.nextFloat();
    return h;
}

// ===========================================================================
// State

void GranulumProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const ScopedLock sl (infoLock);
        state.setProperty ("samplePath", samplePath.getFullPathName(), nullptr);
    }
    state.setProperty ("rootShift", rootShift.load(), nullptr);

    StringArray hs;
    for (auto& h : harmonics) hs.add (String (h.load(), 4));
    state.setProperty ("harmonics", hs.joinIntoString (","), nullptr);

    StringArray ms;
    for (auto& m : mods)
        if (const int t = m.target.load(); t >= 0)
            ms.add (String (kModTargetIds[t]) + ":" + String (m.lfo.load()) + ":" + String (m.depth.load(), 4));
    state.setProperty ("mods", ms.joinIntoString (";"), nullptr);

    for (int l = 0; l < kNumLfos; ++l)
        state.setProperty ("lfoShape" + String (l + 1), pointsToString (getLfoPoints (l)), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GranulumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = ValueTree::fromXml (*xml);
    const String path = state.getProperty ("samplePath").toString();
    const int shift = (int) state.getProperty ("rootShift", 0);
    const auto hs = StringArray::fromTokens (state.getProperty ("harmonics").toString(), ",", "");
    const auto ms = StringArray::fromTokens (state.getProperty ("mods").toString(), ";", "");
    std::array<String, kNumLfos> shapes;
    for (int l = 0; l < kNumLfos; ++l) shapes[(size_t) l] = state.getProperty ("lfoShape" + String (l + 1)).toString();
    apvts.replaceState (state);

    if (hs.size() == granulum::kNumHarmonics)
        for (int i = 0; i < hs.size(); ++i) setHarmonic (i, hs[i].getFloatValue());

    for (auto& m : mods) m.target.store (-1);
    for (auto& entry : ms)
    {
        const auto parts = StringArray::fromTokens (entry, ":", "");
        if (parts.size() == 3)
            setModulation (modIndexOf (parts[0]), parts[1].getIntValue(), parts[2].getFloatValue());
    }
    ++modVersion;

    for (int l = 0; l < kNumLfos; ++l)
    {
        auto pts = pointsFromString (shapes[(size_t) l]);
        if (pts.size() >= 2) setLfoPoints (l, pts);
    }

    File f (path);
    String err;
    if (path.isNotEmpty() && f.existsAsFile() && loadSample (f, err))
        rootShift.store (shift);
}

AudioProcessorEditor* GranulumProcessor::createEditor()
{
    return new GranulumEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GranulumProcessor();
}
