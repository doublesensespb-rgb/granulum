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
        ParamID::osc1Vol, ParamID::osc1Oct, ParamID::osc1Semi, ParamID::osc2Vol, ParamID::osc2Oct, ParamID::osc2Semi,
        ParamID::bloom, ParamID::smear, ParamID::scatter, ParamID::shimmer, ParamID::drift,
        ParamID::gsize, ParamID::speed, ParamID::position, ParamID::density, ParamID::gmix,
        ParamID::cutoff, ParamID::reso,
        ParamID::distDrive, ParamID::distTone, ParamID::distMix, ParamID::revMix, ParamID::revSize,
        ParamID::porta, ParamID::output,
        "lfo1Rate", "lfo2Rate"
    };

    String hzWithNote (float hz)
    {
        const String f = hz < 1000.0f ? String (roundToInt (hz)) + " Hz" : String (hz / 1000.0f, 1) + " kHz";
        const int note = roundToInt (69.0f + 12.0f * std::log2 (jmax (1.0f, hz) / 440.0f));
        return f + "  " + MidiMessage::getMidiNoteName (jlimit (0, 127, note), true, true, 3);
    }
}

// ===========================================================================
// Distortion

const StringArray& Distortion::typeNames()
{
    static const StringArray s { "TUBE", "HARD CLIP", "FOLD", "BITCRUSH", "RECTIFY" };
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

const StringArray& GranulumProcessor::chordNames()
{
    static const StringArray s { "OFF", "MAJ", "MIN", "MAJ7", "MIN7", "MAJ9", "MIN9", "DARK", "SPACE" };
    return s;
}

std::vector<int> GranulumProcessor::chordIntervals (int i)
{
    switch (i)
    {
        case 1:  return { 0, 4, 7 };
        case 2:  return { 0, 3, 7 };
        case 3:  return { 0, 4, 7, 11 };
        case 4:  return { 0, 3, 7, 10 };
        case 5:  return { 0, 4, 7, 11, 14 };
        case 6:  return { 0, 3, 7, 10, 14 };
        case 7:  return { 0, 1 };
        case 8:  return { 0, 7 };
        default: return { 0 };
    }
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
    static const StringArray s { "HILL", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "PUMP" };
    return s;
}

std::array<LfoPoint, kLfoPoints> GranulumProcessor::lfoPreset (int i)
{
    switch (i)
    {
        case 1:  return { LfoPoint { 0, 0, 0 }, { 0.5f, 1, 0 }, { 0.5f, 1, 0 }, { 1, 0, 0 } };
        case 2:  return { LfoPoint { 0, 0, 0 }, { 0.333f, 0.333f, 0 }, { 0.667f, 0.667f, 0 }, { 1, 1, 0 } };
        case 3:  return { LfoPoint { 0, 1, 0 }, { 0.333f, 0.667f, 0 }, { 0.667f, 0.333f, 0 }, { 1, 0, 0 } };
        case 4:  return { LfoPoint { 0, 1, 0 }, { 0.5f, 1, 0 }, { 0.5f, 0, 0 }, { 1, 0, 0 } };
        case 5:  return { LfoPoint { 0, 0, -0.75f }, { 0.4f, 0.8f, -0.3f }, { 0.75f, 0.95f, 0 }, { 1, 1, 0 } };
        default: return { LfoPoint { 0, 0, -0.45f }, { 0.4f, 1, 0 }, { 0.6f, 1, 0.45f }, { 1, 0, 0 } };
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
    auto secFmt  = A().withStringFromValueFunction ([] (float v, int) { return v < 0.01f ? String ("OFF") : fmt (v, v < 1.0f ? 2 : 1, " s"); });

    // OSC 1 + OSC 2
    for (int o = 1; o <= 2; ++o)
    {
        const String n = "Osc " + String (o) + " ";
        ps.push_back (std::make_unique<B> (ParameterID { o == 1 ? ParamID::osc1On : ParamID::osc2On, 1 }, n + "On", o == 1));
        ps.push_back (std::make_unique<P> (ParameterID { o == 1 ? ParamID::osc1Vol : ParamID::osc2Vol, 1 }, n + "Volume", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
        ps.push_back (std::make_unique<P> (ParameterID { o == 1 ? ParamID::osc1Oct : ParamID::osc2Oct, 1 }, n + "Octave", NormalisableRange<float> (-3.0f, 3.0f, 1.0f), 0.0f, octFmt));
        ps.push_back (std::make_unique<P> (ParameterID { o == 1 ? ParamID::osc1Semi : ParamID::osc2Semi, 1 }, n + "Semi", NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f, semiFmt));
        ps.push_back (std::make_unique<B> (ParameterID { o == 1 ? ParamID::osc1Grain : ParamID::osc2Grain, 1 }, n + "Granular", true));
    }
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::chord, 1 }, "Chord", chordNames(), 0));

    // GRANULAR
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::grainOn, 1 }, "Granular On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::bloom, 1 },   "Bloom",   NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::smear, 1 },   "Smear",   NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::scatter, 1 }, "Scatter", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::shimmer, 1 }, "Shimmer", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::drift, 1 },   "Drift",   NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::gsize, 1 },   "Size",    NormalisableRange<float> (0.0f, 1.0f), 0.35f,
                                       A().withStringFromValueFunction ([] (float v, int) { const float ms = 12.0f * std::pow (80.0f, v); return ms < 1000.0f ? fmt (ms, 0, " ms") : fmt (ms / 1000.0f, 2, " s"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::speed, 1 },   "Speed",   NormalisableRange<float> (-2.0f, 2.0f, 0.001f), 1.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.01f ? String ("FREEZE") : fmt (v, 2, "x"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::position, 1 }, "Position", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::density, 1 }, "Density", NormalisableRange<float> (0.0f, 1.0f), 0.4f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (3.0f * std::pow (60.0f, v), 0, " /s"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::gmix, 1 },    "Grain Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::evolve, 1 },  "Evolve",  skewed (0.0f, 8.0f, 1.5f), 0.0f, secFmt));

    // FILTER (key-tracked from C3 under the hood)
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::filterOn, 1 }, "Filter On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::cutoff, 1 }, "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 2000.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return hzWithNote (v); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reso, 1 }, "Resonance", skewed (0.5f, 10.0f, 1.5f), 0.707f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, ""); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::ftype, 1 }, "Filter Type", StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));

    // DISTORTION
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::distOn, 1 }, "Distortion On", false));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::distType, 1 }, "Distortion Type", Distortion::typeNames(), 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::distDrive, 1 }, "Distortion Drive", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::distTone, 1 },  "Distortion Tone",  NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::distMix, 1 },   "Distortion Mix",   NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    // REVERB
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::revOn, 1 }, "Reverb On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::revMix, 1 },  "Reverb Mix",  NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::revSize, 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));

    // VOICE / global
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::mono, 1 }, "Mono", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::porta, 1 }, "Portamento", skewed (0.0f, 3000.0f, 200.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? String ("OFF") : v < 1000.0f ? fmt (v, 0, " ms") : fmt (v / 1000.0f, 2, " s"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -6.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 1, " dB"); })));

    // LFOs
    for (int l = 0; l < kNumLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfo (l, "Rate"), 1 }, n + "Rate", skewed (0.02f, 30.0f, 2.0f), 1.0f,
                                           A().withStringFromValueFunction ([] (float v, int) { return fmt (v, v < 1.0f ? 2 : 1, " Hz"); })));
        ps.push_back (std::make_unique<B> (ParameterID { ParamID::lfo (l, "Sync"), 1 }, n + "Sync", true));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Div"), 1 }, n + "Division", divisionNames(), 4));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Mode"), 1 }, n + "Mode", StringArray { "FREE", "RETRIG", "ENV" }, 0));
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfo (l, "Rise"), 1 }, n + "Rise", skewed (0.0f, 8.0f, 1.5f), 0.0f, secFmt));
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

    pOsc1On    = apvts.getRawParameterValue (ParamID::osc1On);
    pOsc2On    = apvts.getRawParameterValue (ParamID::osc2On);
    pOsc1Grain = apvts.getRawParameterValue (ParamID::osc1Grain);
    pOsc2Grain = apvts.getRawParameterValue (ParamID::osc2Grain);
    pChord     = apvts.getRawParameterValue (ParamID::chord);
    pGrainOn   = apvts.getRawParameterValue (ParamID::grainOn);
    pEvolve    = apvts.getRawParameterValue (ParamID::evolve);
    pFilterOn  = apvts.getRawParameterValue (ParamID::filterOn);
    pFtype     = apvts.getRawParameterValue (ParamID::ftype);
    pDistOn    = apvts.getRawParameterValue (ParamID::distOn);
    pDistType  = apvts.getRawParameterValue (ParamID::distType);
    pRevOn     = apvts.getRawParameterValue (ParamID::revOn);
    pMono      = apvts.getRawParameterValue (ParamID::mono);
    for (int l = 0; l < kNumLfos; ++l)
    {
        pLfoSync[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Sync"));
        pLfoDiv[(size_t) l]  = apvts.getRawParameterValue (ParamID::lfo (l, "Div"));
        pLfoMode[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Mode"));
        pLfoRise[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Rise"));
    }

    harmonics = {};
    harmonics[0] = 1.0f;
    for (int l = 0; l < kNumLfos; ++l) setLfoPoints (l, lfoPreset (0));

    timerCallback();
    startTimerHz (30);
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
    engineAdd.prepare (sampleRate);
    engineSmp.prepare (sampleRate);

    dsp::ProcessSpec spec { sampleRate, (uint32) jmax (kChunk, samplesPerBlock), 2 };
    filter.prepare (spec); filter.reset();
    reverb.prepare (spec); reverb.reset();
    distortion.prepare (sampleRate);

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));

    const int n = jmax (kChunk, samplesPerBlock);
    addBuf.setSize (2, n, false, true, true);
    smpBuf.setSize (2, n, false, true, true);
    work.setSize   (2, n, false, true, true);

    heldNotes = 0;
    sinceTrigger = 1.0e9;
    for (int l = 0; l < kNumLfos; ++l) { lfoPhase[(size_t) l] = 0.0; lfoValue[(size_t) l] = lfoShapeAt (l, 0.0f); lfoDone[(size_t) l] = false; }
}

// ===========================================================================
// INIT preset

void GranulumProcessor::resetToInit()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    std::array<float, granulum::kNumHarmonics> h {};
    h[0] = 1.0f;
    setHarmonics (h);
    for (auto& m : mods) m.target.store (-1);
    ++modVersion;
    for (int l = 0; l < kNumLfos; ++l) setLfoPoints (l, lfoPreset (0));
}

// ===========================================================================
// LFO shapes (4 points)

std::array<LfoPoint, kLfoPoints> GranulumProcessor::getLfoPoints (int l) const
{
    const ScopedLock sl (lfoLock);
    return lfoPoints[(size_t) l];
}

void GranulumProcessor::setLfoPoints (int l, std::array<LfoPoint, kLfoPoints> pts)
{
    pts[0].x = 0.0f;
    pts[kLfoPoints - 1].x = 1.0f;
    for (int i = 1; i < kLfoPoints - 1; ++i) pts[(size_t) i].x = jlimit (pts[(size_t) i - 1].x, 1.0f, pts[(size_t) i].x);
    for (int i = kLfoPoints - 2; i > 0; --i) pts[(size_t) i].x = jmin (pts[(size_t) i].x, pts[(size_t) i + 1].x);
    for (auto& p : pts) { p.y = jlimit (0.0f, 1.0f, p.y); p.curve = jlimit (-1.0f, 1.0f, p.curve); }
    {
        const ScopedLock sl (lfoLock);
        lfoPoints[(size_t) l] = pts;
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
        t[(size_t) i] = (b.x - a.x < 1.0e-5f) ? b.y
                      : a.y + (b.y - a.y) * segmentShape (jlimit (0.0f, 1.0f, (x - a.x) / (b.x - a.x)), a.curve);
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
        v[t] = p->convertFrom0to1 (norm);   // not snapped: Oct/Semi glide smoothly under modulation
        modNorm[(size_t) t].store (norm, std::memory_order_relaxed);
    }
}

void GranulumProcessor::advanceLfos (int endSample, int len, const float* v)
{
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    sinceTrigger += (double) len / sr;
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto L = (size_t) l;
        const bool sync = pLfoSync[L]->load() > 0.5f;
        const int  mode = (int) pLfoMode[L]->load();       // 0 free, 1 retrig, 2 env
        const double beats = divisionBeats ((int) pLfoDiv[L]->load());
        const double rate = sync ? (hostBpm / 60.0) / beats : (double) v[mtLfo1Rate + l];

        double ph;
        if (mode == 2)
        {
            ph = lfoDone[L] ? 1.0 : lfoPhase[L] + rate * (double) len / sr;
            if (ph >= 1.0) { ph = 1.0; lfoDone[L] = true; }
        }
        else
        {
            if (sync && mode == 0 && hostPlaying && hostPpq >= 0.0)
                ph = (hostPpq + (double) endSample / sr * hostBpm / 60.0) / beats;
            else
                ph = lfoPhase[L] + rate * (double) len / sr;
            ph -= std::floor (ph);
        }
        lfoPhase[L] = ph;

        const float rise = pLfoRise[L]->load();
        const float riseGain = rise > 0.01f ? (float) jmin (1.0, sinceTrigger / (double) rise) : 1.0f;
        lfoValue[L] = lfoShapeAt (l, (float) ph) * riseGain;
        lfoValueOut[L].store (lfoValue[L], std::memory_order_relaxed);
        lfoPhaseOut[L].store ((float) ph, std::memory_order_relaxed);
        lfoRiseOut[L].store (riseGain, std::memory_order_relaxed);
    }
}

// ===========================================================================
// Audio

granulum::Params GranulumProcessor::voiceParams (const float* v, bool forAdd) const
{
    granulum::Params p;
    p.bloom    = v[mtBloom];
    p.smear    = v[mtSmear];
    p.scatter  = v[mtScatter];
    p.shimmer  = v[mtShimmer];
    p.drift    = v[mtDrift];
    p.size     = v[mtSize];
    p.speed    = v[mtSpeed];
    p.position = v[mtPosition];
    p.density  = v[mtDensity];
    p.mix      = v[mtGmix];
    p.evolve   = pEvolve->load();
    p.mono     = pMono->load() > 0.5f;
    p.portaMs  = v[mtPorta];

    const bool grainBlock = pGrainOn->load() > 0.5f;
    if (forAdd)
    {
        p.transpose = v[mtOsc1Oct] * 12.0f + v[mtOsc1Semi];
        p.grainOn = grainBlock && pOsc1Grain->load() > 0.5f;
        const auto iv = chordIntervals ((int) pChord->load());
        p.chordSize = (int) iv.size();
        for (size_t i = 0; i < iv.size() && i < p.chord.size(); ++i) p.chord[i] = iv[i];
    }
    else
    {
        p.transpose = v[mtOsc2Oct] * 12.0f + v[mtOsc2Semi];
        p.grainOn = grainBlock && pOsc2Grain->load() > 0.5f;
    }
    return p;
}

void GranulumProcessor::noteOn (int note, float velocity)
{
    if (heldNotes == 0)
    {
        sinceTrigger = 0.0;
        for (int l = 0; l < kNumLfos; ++l)
            if ((int) pLfoMode[(size_t) l]->load() >= 1)
            {
                lfoPhase[(size_t) l] = 0.0;
                lfoDone[(size_t) l] = false;
            }
    }
    ++heldNotes;
    engineAdd.noteOn (note, velocity, voiceParams (lastV.data(), true));
    engineSmp.noteOn (note, velocity, voiceParams (lastV.data(), false));
}

void GranulumProcessor::noteOff (int note)
{
    heldNotes = jmax (0, heldNotes - 1);
    engineAdd.noteOff (note, voiceParams (lastV.data(), true));
    engineSmp.noteOff (note, voiceParams (lastV.data(), false));
}

void GranulumProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    addSlot.swapOnAudioThread();
    smpSlot.swapOnAudioThread();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (work.getNumSamples() < numSamples)
    {
        addBuf.setSize (2, numSamples, false, false, true);
        smpBuf.setSize (2, numSamples, false, false, true);
        work.setSize   (2, numSamples, false, false, true);
    }
    addBuf.clear(); smpBuf.clear(); work.clear();

    hostBpm = 120.0; hostPpq = -1.0; hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())         hostBpm = jlimit (20.0, 999.0, *b);
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
            hostPlaying = pos->getIsPlaying();
        }

    blockPeak = 0.0f;
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, numSamples, meta.samplePosition);
        renderRange (pos, at);
        pos = jmax (pos, at);
        const auto m = meta.getMessage();
        if (m.isNoteOn())       noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { heldNotes = 0; engineAdd.allNotesOff(); engineSmp.allNotesOff(); }
    }
    renderRange (pos, numSamples);
    midi.clear();

    outputLevel.store (jmax (blockPeak, outputLevel.load() * 0.9f));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, work, jmin (ch, 1), 0, numSamples);
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

    const auto p1 = voiceParams (v, true);
    const auto p2 = voiceParams (v, false);
    const float g1 = pOsc1On->load() > 0.5f ? v[mtOsc1Vol] : 0.0f;
    const float g2 = pOsc2On->load() > 0.5f ? v[mtOsc2Vol] : 0.0f;

    float* aL = addBuf.getWritePointer (0) + from;
    float* aR = addBuf.getWritePointer (1) + from;
    float* sL = smpBuf.getWritePointer (0) + from;
    float* sR = smpBuf.getWritePointer (1) + from;
    float* L  = work.getWritePointer (0) + from;
    float* R  = work.getWritePointer (1) + from;

    engineAdd.render (aL, aR, len, p1, (g1 > 0.0001f || lastG1 > 0.0001f) ? addSlot.active.get() : nullptr);
    engineSmp.render (sL, sR, len, p2, (g2 > 0.0001f || lastG2 > 0.0001f) ? smpSlot.active.get() : nullptr);

    for (int i = 0; i < len; ++i)
    {
        const float t = (float) (i + 1) / (float) len;
        const float a = lastG1 + (g1 - lastG1) * t;
        const float b = lastG2 + (g2 - lastG2) * t;
        L[i] = aL[i] * a + sL[i] * b;
        R[i] = aR[i] * a + sR[i] * b;
    }
    lastG1 = g1;
    lastG2 = g2;

    auto block = dsp::AudioBlock<float> (work).getSubBlock ((size_t) from, (size_t) len);
    dsp::ProcessContextReplacing<float> ctx (block);

    // ---- FILTER: key-tracked from C3 ----
    const float note = engineAdd.lastNote.load (std::memory_order_relaxed);
    const float cut = jlimit (20.0f, (float) (getSampleRate() * 0.45), v[mtCutoff] * std::exp2 ((note - 60.0f) / 12.0f));
    keyCutoff.store (cut, std::memory_order_relaxed);
    if (pFilterOn->load() > 0.5f)
    {
        const int ft = (int) pFtype->load();
        filter.setType (ft == 0 ? dsp::StateVariableTPTFilterType::lowpass
                      : ft == 1 ? dsp::StateVariableTPTFilterType::bandpass
                                : dsp::StateVariableTPTFilterType::highpass);
        filter.setCutoffFrequency (cut);
        filter.setResonance (v[mtReso]);
        filter.process (ctx);
    }

    // ---- DISTORTION ----
    if (pDistOn->load() > 0.5f && v[mtDistMix] > 0.001f)
        distortion.process (L, R, len, (int) pDistType->load(), v[mtDistDrive], v[mtDistTone], v[mtDistMix]);

    // ---- REVERB ----
    if (pRevOn->load() > 0.5f)
    {
        const float mix = v[mtRevMix];
        dsp::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.69f * v[mtRevSize];
        rp.damping  = 0.4f;
        rp.wetLevel = mix * 0.9f;
        rp.dryLevel = 1.0f - mix * 0.5f;
        rp.width    = 1.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);
    }

    // ---- output ----
    outGain.setTargetValue (Decibels::decibelsToGain (v[mtOutput]));
    for (int i = 0; i < len; ++i)
    {
        const float g = outGain.getNextValue();
        L[i] = std::tanh (L[i] * g);
        R[i] = std::tanh (R[i] * g);
        blockPeak = jmax (blockPeak, std::abs (L[i]), std::abs (R[i]));
    }

    advanceLfos (from + len, len, v);
}

// ===========================================================================
// Sources

bool GranulumProcessor::isSupportedAudioFile (const String& path)
{
    const auto ext = File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg" || ext == ".mp3";
}

std::shared_ptr<WaveformPeaks> GranulumProcessor::makePeaks (const granulum::SampleData& d)
{
    auto pk = std::make_shared<WaveformPeaks>();
    const int frames = d.numFrames();
    if (frames <= 0) return pk;
    const int bins = 2048;
    pk->mins.assign (bins, 0.0f);
    pk->maxs.assign (bins, 0.0f);
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((int64) b * frames / bins);
        const int s1 = jmax (s0 + 1, (int) ((int64) (b + 1) * frames / bins));
        float lo = 0.0f, hi = 0.0f;
        for (const auto& ch : d.channels)
            for (int i = s0; i < s1 && i < frames; ++i) { lo = jmin (lo, ch[(size_t) i]); hi = jmax (hi, ch[(size_t) i]); }
        pk->mins[(size_t) b] = lo;
        pk->maxs[(size_t) b] = hi;
    }
    return pk;
}

bool GranulumProcessor::loadSample (const File& file, String& error)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr) { error = "Can't read this file"; return false; }

    const auto frames = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 600.0));
    if (frames < 16) { error = "File is empty"; return false; }

    const int chans = jlimit (1, 2, (int) reader->numChannels);
    AudioBuffer<float> tmp (chans, frames);
    reader->read (&tmp, 0, frames, 0, true, chans > 1);

    const float mag = tmp.getMagnitude (0, frames);
    const float normGain = mag > 1.0e-5f ? Decibels::decibelsToGain (-1.0f) / mag : 1.0f;

    auto data = std::make_shared<granulum::SampleData>();
    data->sampleRate = reader->sampleRate;
    data->channels.resize ((size_t) chans);
    for (int c = 0; c < chans; ++c)
    {
        data->channels[(size_t) c].resize ((size_t) frames);
        const float* src = tmp.getReadPointer (c);
        for (int i = 0; i < frames; ++i) data->channels[(size_t) c][(size_t) i] = src[i] * normGain;
    }

    auto pk = makePeaks (*data);
    {
        const ScopedLock sl (infoLock);
        samplePath = file;
        smpPeaks = std::move (pk);
        smpSeconds = frames / reader->sampleRate;
    }
    smpSlot.push (std::move (data));
    sampleLoaded.store (true);

    if (auto* on = apvts.getParameter (ParamID::osc2On))   // dropping a file switches OSC 2 on
        if (on->getValue() < 0.5f) { on->beginChangeGesture(); on->setValueNotifyingHost (1.0f); on->endChangeGesture(); }
    return true;
}

void GranulumProcessor::setHarmonics (const std::array<float, granulum::kNumHarmonics>& h)
{
    harmonics = h;
    harmonicsDirty.store (true);
}

bool GranulumProcessor::showingSample() const
{
    return sampleLoaded.load() && pOsc2On->load() > 0.5f;
}

String GranulumProcessor::getSampleName() const
{
    const ScopedLock sl (infoLock);
    return samplePath == File() ? String() : samplePath.getFileName();
}

std::shared_ptr<const WaveformPeaks> GranulumProcessor::getPeaks() const
{
    const ScopedLock sl (infoLock);
    return showingSample() ? smpPeaks : addPeaks;
}

double GranulumProcessor::getSampleLengthSeconds() const
{
    const ScopedLock sl (infoLock);
    return sampleLoaded.load() ? smpSeconds : 0.0;
}

void GranulumProcessor::timerCallback()
{
    addSlot.collect();
    smpSlot.collect();

    if (harmonicsDirty.exchange (false))
    {
        auto data = std::make_shared<granulum::SampleData> (granulum::renderAdditive (harmonics));
        auto pk = makePeaks (*data);
        {
            const ScopedLock sl (infoLock);
            addPeaks = std::move (pk);
            addSeconds = data->numFrames() / data->sampleRate;
        }
        addSlot.push (std::move (data));
    }
}

// ===========================================================================
// Random

void GranulumProcessor::randomize()
{
    for (int s = 0; s < 4; ++s) randomizeSection (s);
}

void GranulumProcessor::randomizeSection (int section)
{
    Random r;
    auto set = [this] (const char* id, float real)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (real));
            p->endChangeGesture();
        }
    };
    auto range = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto maybe = [&r] (float chance, float v) { return r.nextFloat() < chance ? v : 0.0f; };

    if (section == 0)
    {
        set (ParamID::bloom,    range (0.1f, 0.9f));
        set (ParamID::smear,    range (0.0f, 0.8f));
        set (ParamID::scatter,  range (0.1f, 0.9f));
        set (ParamID::shimmer,  maybe (0.5f, range (0.1f, 0.6f)));
        set (ParamID::drift,    range (0.0f, 0.8f));
        set (ParamID::gsize,    range (0.2f, 0.8f));
        set (ParamID::density,  range (0.25f, 0.8f));
        set (ParamID::speed,    r.nextFloat() < 0.25f ? 0.0f : range (-0.5f, 1.2f));
        set (ParamID::position, range (0.0f, 0.8f));
    }
    else if (section == 1)
    {
        const int type = r.nextInt (3);
        set (ParamID::ftype,  (float) type);
        set (ParamID::cutoff, type == 0 ? 300.0f * std::pow (20.0f, r.nextFloat()) : type == 1 ? 400.0f * std::pow (6.0f, r.nextFloat()) : 80.0f * std::pow (10.0f, r.nextFloat()));
        set (ParamID::reso,   0.6f * std::pow (5.0f, r.nextFloat()));
    }
    else if (section == 2)
    {
        set (ParamID::distType,  (float) r.nextInt (5));
        set (ParamID::distDrive, range (0.2f, 0.8f));
        set (ParamID::distTone,  range (0.35f, 1.0f));
        set (ParamID::distMix,   range (0.4f, 1.0f));
        set (ParamID::revMix,    range (0.1f, 0.5f));
        set (ParamID::revSize,   range (0.3f, 0.95f));
    }
    else
    {
        std::array<float, granulum::kNumHarmonics> h {};
        h[0] = 0.6f + 0.4f * r.nextFloat();
        for (int k = 1; k < granulum::kNumHarmonics; ++k)
        {
            const float tilt = 1.0f / (1.0f + (float) k * (0.15f + 0.4f * r.nextFloat()));
            h[(size_t) k] = r.nextFloat() < 0.55f ? jmin (1.0f, r.nextFloat() * tilt * 1.6f) : 0.0f;
        }
        setHarmonics (h);
    }
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
    StringArray hs;
    for (float h : harmonics) hs.add (String (h, 4));
    state.setProperty ("harmonics", hs.joinIntoString (","), nullptr);

    StringArray ms;
    for (auto& m : mods)
        if (const int t = m.target.load(); t >= 0)
            ms.add (String (kModTargetIds[t]) + ":" + String (m.lfo.load()) + ":" + String (m.depth.load(), 4));
    state.setProperty ("mods", ms.joinIntoString (";"), nullptr);

    for (int l = 0; l < kNumLfos; ++l)
    {
        StringArray ps;
        for (auto& p : getLfoPoints (l)) ps.add (String (p.x, 4) + "," + String (p.y, 4) + "," + String (p.curve, 3));
        state.setProperty ("lfoShape" + String (l + 1), ps.joinIntoString (";"), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GranulumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto state = ValueTree::fromXml (*xml);
    const String path = state.getProperty ("samplePath").toString();
    const auto hs = StringArray::fromTokens (state.getProperty ("harmonics").toString(), ",", "");
    const auto ms = StringArray::fromTokens (state.getProperty ("mods").toString(), ";", "");
    std::array<String, kNumLfos> shapes;
    for (int l = 0; l < kNumLfos; ++l) shapes[(size_t) l] = state.getProperty ("lfoShape" + String (l + 1)).toString();
    apvts.replaceState (state);

    if (hs.size() == granulum::kNumHarmonics)
    {
        std::array<float, granulum::kNumHarmonics> h {};
        for (int i = 0; i < hs.size(); ++i) h[(size_t) i] = jlimit (0.0f, 1.0f, hs[i].getFloatValue());
        setHarmonics (h);
    }

    for (auto& m : mods) m.target.store (-1);
    for (auto& entry : ms)
    {
        const auto parts = StringArray::fromTokens (entry, ":", "");
        if (parts.size() == 3) setModulation (modIndexOf (parts[0]), parts[1].getIntValue(), parts[2].getFloatValue());
    }
    ++modVersion;

    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto items = StringArray::fromTokens (shapes[(size_t) l], ";", "");
        if (items.size() != kLfoPoints) continue;
        std::array<LfoPoint, kLfoPoints> pts {};
        for (int i = 0; i < kLfoPoints; ++i)
        {
            const auto vals = StringArray::fromTokens (items[i], ",", "");
            if (vals.size() == 3) pts[(size_t) i] = { vals[0].getFloatValue(), vals[1].getFloatValue(), vals[2].getFloatValue() };
        }
        setLfoPoints (l, pts);
    }

    const bool osc2 = apvts.getRawParameterValue (ParamID::osc2On)->load() > 0.5f;
    File f (path);
    String err;
    if (path.isNotEmpty() && f.existsAsFile() && loadSample (f, err))
        if (auto* on = apvts.getParameter (ParamID::osc2On)) on->setValueNotifyingHost (osc2 ? 1.0f : 0.0f);
    timerCallback();
}

AudioProcessorEditor* GranulumProcessor::createEditor()
{
    return new GranulumEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GranulumProcessor();
}
