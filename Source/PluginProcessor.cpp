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

    constexpr int kChunk = 64;

    const char* const kModTargetIds[kNumModTargets] =
    {
        ParamID::attack, ParamID::hold, ParamID::spacing, ParamID::waveSpacing, ParamID::position,
        ParamID::rand, ParamID::smear, ParamID::pan,
        ParamID::sub, ParamID::xover, ParamID::drive, ParamID::glide, ParamID::pitch,
        ParamID::cutoff, ParamID::reso, ParamID::space, ParamID::room, ParamID::output,
        ParamID::addOct, ParamID::addSemi, ParamID::mix,
        "lfo1Rate", "lfo2Rate"
    };
}

// ---------------------------------------------------------------------------
// Static helpers

const char* GranulumProcessor::modTargetId (int t) { return (t >= 0 && t < kNumModTargets) ? kModTargetIds[t] : ""; }

int GranulumProcessor::modIndexOf (const String& id)
{
    for (int t = 0; t < kNumModTargets; ++t)
        if (id == kModTargetIds[t]) return t;
    return -1;
}

const StringArray& GranulumProcessor::shapeNames()
{
    static const StringArray s { "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "S&H", "SMOOTH RND" };
    return s;
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

float GranulumProcessor::lfoShapeValue (int shape, float ph, float sh, float prev, float next)
{
    switch (shape)
    {
        case 0:  return 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * ph);
        case 1:  return ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f;
        case 2:  return ph;
        case 3:  return 1.0f - ph;
        case 4:  return ph < 0.5f ? 1.0f : 0.0f;
        case 5:  return sh;
        default: return prev + (next - prev) * (0.5f - 0.5f * std::cos (MathConstants<float>::pi * ph));
    }
}

// ---------------------------------------------------------------------------

AudioProcessorValueTreeState::ParameterLayout GranulumProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto pct   = A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto msFmt = A().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? fmt (v, v < 10.0f ? 1 : 0, " ms") : fmt (v / 1000.0f, 2, " s"); });
    auto hzFmt = A().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? fmt (v, 0, " Hz") : fmt (v / 1000.0f, 1, " kHz"); });

    // ---- grains ----
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::attack, 1 },      "Attack",        NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::hold, 1 },        "Hold",          skewed (1.0f, 1000.0f, 60.0f), 80.0f, msFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::spacing, 1 },     "Grain Spacing", skewed (1.0f, 1000.0f, 60.0f), 60.0f, msFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::waveSpacing, 1 }, "Wave Spacing",  NormalisableRange<float> (-3.0f, 3.0f, 0.001f), 1.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.005f ? String ("FREEZE") : String (roundToInt (v * 100.0f)) + " %"; })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::position, 1 },    "Start",         NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::rand, 1 },        "Rand",          NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::smear, 1 },       "Smear",         NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pan, 1 },         "Pan",           NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::transients, 1 }, "Transients", false));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::keySync, 1 },    "Key Sync",   true));

    // ---- bass ----
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::sub, 1 },   "Sub",    NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::xover, 1 }, "X-Over", skewed (20.0f, 500.0f, 120.0f), 120.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 21.0f ? String ("OFF") : fmt (v, 0, " Hz"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::drive, 1 }, "Drive",  NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::glide, 1 }, "Glide",  skewed (0.0f, 2000.0f, 150.0f), 40.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? String ("OFF") : v < 1000.0f ? fmt (v, 0, " ms") : fmt (v / 1000.0f, 2, " s"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pitch, 1 }, "Pitch",  NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + fmt (v, 1, " st"); })));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::mono, 1 }, "Mono", true));

    // ---- filter / space ----
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::cutoff, 1 }, "Cutoff",    skewed (20.0f, 20000.0f, 1000.0f), 20000.0f, hzFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reso, 1 },   "Resonance", skewed (0.5f, 10.0f, 1.5f), 0.707f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, ""); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::ftype, 1 }, "Filter Type", StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::space, 1 },  "Space",     NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::room, 1 },   "Room Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::output, 1 }, "Output",    NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -3.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 1, " dB"); })));

    // ---- source ----
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::source, 1 }, "Source", StringArray { "Additive", "Sample", "Mix" }, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                       A().withStringFromValueFunction ([] (float v, int) { return "ADD " + String (roundToInt (v * 100.0f)) + " %"; })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::addOct, 1 },  "Additive Octave", NormalisableRange<float> (-3.0f, 3.0f, 1.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { const int o = roundToInt (v); return (o > 0 ? "+" : "") + String (o) + " oct"; })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::addSemi, 1 }, "Additive Semi",   NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { const int s = roundToInt (v); return (s > 0 ? "+" : "") + String (s) + " st"; })));

    // ---- LFOs ----
    for (int l = 0; l < kNumLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Shape"), 1 }, n + "Shape", shapeNames(), l == 0 ? 0 : 1));
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfo (l, "Rate"), 1 }, n + "Rate", skewed (0.02f, 30.0f, 2.0f), l == 0 ? 1.0f : 0.25f,
                                           A().withStringFromValueFunction ([] (float v, int) { return fmt (v, v < 1.0f ? 2 : 1, " Hz"); })));
        ps.push_back (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::lfo (l, "Sync"), 1 }, n + "Sync", true));
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
    }

    pTrans   = apvts.getRawParameterValue (ParamID::transients);
    pKeySync = apvts.getRawParameterValue (ParamID::keySync);
    pMono    = apvts.getRawParameterValue (ParamID::mono);
    pFtype   = apvts.getRawParameterValue (ParamID::ftype);
    pSource  = apvts.getRawParameterValue (ParamID::source);
    for (int l = 0; l < kNumLfos; ++l)
    {
        pLfoShape[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Shape"));
        pLfoSync[(size_t) l]  = apvts.getRawParameterValue (ParamID::lfo (l, "Sync"));
        pLfoDiv[(size_t) l]   = apvts.getRawParameterValue (ParamID::lfo (l, "Div"));
        pLfoMode[(size_t) l]  = apvts.getRawParameterValue (ParamID::lfo (l, "Mode"));
        lfoPrev[(size_t) l] = lfoRng.uni(); lfoNext[(size_t) l] = lfoRng.uni(); lfoSH[(size_t) l] = lfoRng.uni();
    }

    const float init[] = { 1.0f, 0.55f, 0.35f, 0.22f, 0.12f, 0.08f };
    for (size_t i = 0; i < std::size (init); ++i) harmonics[i] = init[i];

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
    filter.prepare (spec);    filter.reset();
    crossover.prepare (spec); crossover.reset();
    reverb.prepare (spec);    reverb.reset();

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));

    const int n = jmax (kChunk, samplesPerBlock);
    addBuf.setSize (2, n, false, true, true);
    smpBuf.setSize (2, n, false, true, true);
    work.setSize   (2, n, false, true, true);
    subBuf.setSize (1, n, false, true, true);

    heldNotes = 0;
    for (int l = 0; l < kNumLfos; ++l) { lfoPhase[(size_t) l] = 0.0; lfoValue[(size_t) l] = 0.0f; }
}

// ---------------------------------------------------------------------------
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
        v[t] = p->convertFrom0to1 (norm);
        modNorm[(size_t) t].store (norm, std::memory_order_relaxed);
    }
}

void GranulumProcessor::advanceLfos (int endSample, int len, const float* v)
{
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto L = (size_t) l;
        const int  shape = (int) pLfoShape[L]->load();
        const bool sync  = pLfoSync[L]->load() > 0.5f;
        const bool free  = (int) pLfoMode[L]->load() == 0;
        const double beats = divisionBeats ((int) pLfoDiv[L]->load());

        const double old = lfoPhase[L];
        double ph;
        if (sync && free && hostPlaying && hostPpq >= 0.0)
        {
            const double ppqNow = hostPpq + (double) endSample / sr * hostBpm / 60.0;
            ph = ppqNow / beats;
            ph -= std::floor (ph);
        }
        else
        {
            const double rate = sync ? (hostBpm / 60.0) / beats : (double) v[mtLfo1Rate + l];
            ph = old + rate * (double) len / sr;
        }

        if (ph >= 1.0 || ph < old)   // new cycle
        {
            lfoSH[L] = lfoRng.uni();
            lfoPrev[L] = lfoNext[L];
            lfoNext[L] = lfoRng.uni();
        }
        ph -= std::floor (ph);
        lfoPhase[L] = ph;
        lfoValue[L] = lfoShapeValue (shape, (float) ph, lfoSH[L], lfoPrev[L], lfoNext[L]);
        lfoValueOut[L].store (lfoValue[L], std::memory_order_relaxed);
        lfoPhaseOut[L].store ((float) ph, std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// Audio

void GranulumProcessor::noteOn (int note, float velocity)
{
    if (heldNotes == 0)
        for (int l = 0; l < kNumLfos; ++l)
            if ((int) pLfoMode[(size_t) l]->load() == 1)
            {
                lfoPhase[(size_t) l] = 0.0;
                lfoValue[(size_t) l] = lfoShapeValue ((int) pLfoShape[(size_t) l]->load(), 0.0f,
                                                      lfoSH[(size_t) l], lfoPrev[(size_t) l], lfoNext[(size_t) l]);
            }
    ++heldNotes;
    const bool mono = pMono->load() > 0.5f;
    engineAdd.noteOn (note, velocity, mono);
    engineSmp.noteOn (note, velocity, mono);
}

void GranulumProcessor::noteOff (int note)
{
    heldNotes = jmax (0, heldNotes - 1);
    const bool mono = pMono->load() > 0.5f;
    engineAdd.noteOff (note, mono);
    engineSmp.noteOff (note, mono);
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
        subBuf.setSize (1, numSamples, false, false, true);
    }
    addBuf.clear(); smpBuf.clear(); work.clear(); subBuf.clear();

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
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes = 0;
            engineAdd.allNotesOff();
            engineSmp.allNotesOff();
        }
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

    granulum::Params p;
    p.attack      = v[mtAttack];
    p.holdMs      = v[mtHold];
    p.spacingMs   = v[mtSpacing];
    p.waveSpacing = v[mtWave];
    p.position    = v[mtPosition];
    p.rand        = v[mtRand];
    p.smear       = v[mtSmear];
    p.pan         = v[mtPan];
    p.lfoDepth    = 0.0f;
    p.sub         = v[mtSub];
    p.glideMs     = v[mtGlide];
    p.pitch       = v[mtPitch];
    p.transients  = pTrans->load() > 0.5f;
    p.keySync     = pKeySync->load() > 0.5f;
    p.mono        = pMono->load() > 0.5f;

    auto pa = p;
    pa.transpose = (float) roundToInt (v[mtAddOct]) * 12.0f + (float) roundToInt (v[mtAddSemi]);
    auto ps = p;
    ps.sub = 0.0f;

    const int source = (int) pSource->load();
    float addGain = 1.0f, smpGain = 0.0f;
    if (source == 1) { addGain = 0.0f; smpGain = 1.0f; }
    else if (source == 2)
    {
        const float m = jlimit (0.0f, 1.0f, v[mtMix]);
        addGain = std::sin (m * MathConstants<float>::halfPi);
        smpGain = std::cos (m * MathConstants<float>::halfPi);
    }
    const bool runAdd = source != 1;
    const bool runSmp = source != 0;

    float* aL = addBuf.getWritePointer (0) + from;
    float* aR = addBuf.getWritePointer (1) + from;
    float* sL = smpBuf.getWritePointer (0) + from;
    float* sR = smpBuf.getWritePointer (1) + from;
    float* S  = subBuf.getWritePointer (0) + from;
    float* L  = work.getWritePointer (0) + from;
    float* R  = work.getWritePointer (1) + from;

    // the additive engine always runs (it also owns the sub); without a source it only plays the sub
    engineAdd.render (aL, aR, S, len, pa, runAdd ? addSlot.active.get() : nullptr);
    engineSmp.render (sL, sR, S, len, ps, runSmp ? smpSlot.active.get() : nullptr);

    for (int i = 0; i < len; ++i)
    {
        const float t = (float) (i + 1) / (float) len;
        const float ga = lastAddGain + (addGain - lastAddGain) * t;
        const float gs = lastSmpGain + (smpGain - lastSmpGain) * t;
        L[i] = aL[i] * ga + sL[i] * gs;
        R[i] = aR[i] * ga + sR[i] * gs;
    }
    lastAddGain = addGain;
    lastSmpGain = smpGain;

    // ---- drive (grains only) ----
    const float drive = v[mtDrive];
    if (drive > 0.001f)
    {
        const float g = 1.0f + 30.0f * drive * drive;
        const float comp = 1.0f / std::pow (g, 0.7f);
        for (int i = 0; i < len; ++i)
        {
            L[i] = std::tanh (L[i] * g) * comp;
            R[i] = std::tanh (R[i] * g) * comp;
        }
    }

    // ---- filter (grains only) ----
    const int ft = (int) pFtype->load();
    filter.setType (ft == 0 ? dsp::StateVariableTPTFilterType::lowpass
                  : ft == 1 ? dsp::StateVariableTPTFilterType::bandpass
                            : dsp::StateVariableTPTFilterType::highpass);
    filter.setCutoffFrequency (jlimit (20.0f, (float) (getSampleRate() * 0.45), v[mtCutoff]));
    filter.setResonance (v[mtReso]);
    auto block = dsp::AudioBlock<float> (work).getSubBlock ((size_t) from, (size_t) len);
    dsp::ProcessContextReplacing<float> ctx (block);
    if (! (ft == 0 && v[mtCutoff] > 19500.0f && v[mtReso] < 0.75f))
        filter.process (ctx);

    // ---- crossover: lows go mono and skip the reverb ----
    if (v[mtXover] > 21.0f)
    {
        crossover.setCutoffFrequency (v[mtXover]);
        for (int i = 0; i < len; ++i)
        {
            float lowL, highL, lowR, highR;
            crossover.processSample (0, L[i], lowL, highL);
            crossover.processSample (1, R[i], lowR, highR);
            L[i] = highL; R[i] = highR;
            S[i] += 0.5f * (lowL + lowR);
        }
    }

    const float space = v[mtSpace];
    dsp::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.69f * v[mtRoom];
    rp.damping  = 0.4f;
    rp.wetLevel = space * 0.9f;
    rp.dryLevel = 1.0f - space * 0.5f;
    rp.width    = 1.0f;
    reverb.setParameters (rp);
    if (space > 0.001f)
        reverb.process (ctx);

    // ---- output ----
    outGain.setTargetValue (Decibels::decibelsToGain (v[mtOutput]));
    for (int i = 0; i < len; ++i)
    {
        const float g = outGain.getNextValue();
        L[i] = std::tanh ((L[i] + S[i]) * g);
        R[i] = std::tanh ((R[i] + S[i]) * g);
        blockPeak = jmax (blockPeak, std::abs (L[i]), std::abs (R[i]));
    }

    advanceLfos (from + len, len, v);
}

// ---------------------------------------------------------------------------
// Sources

bool GranulumProcessor::isSupportedAudioFile (const String& path)
{
    const auto ext = File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"
        || ext == ".ogg" || ext == ".mp3";
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
            for (int i = s0; i < s1 && i < frames; ++i)
            {
                lo = jmin (lo, ch[(size_t) i]);
                hi = jmax (hi, ch[(size_t) i]);
            }
        pk->mins[(size_t) b] = lo;
        pk->maxs[(size_t) b] = hi;
    }
    for (int t : d.transients)
        pk->transients.push_back ((float) t / (float) frames);
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
        for (int i = 0; i < frames; ++i)
            data->channels[(size_t) c][(size_t) i] = src[i] * normGain;
    }
    data->transients = granulum::detectTransients (*data);

    auto pk = makePeaks (*data);
    {
        const ScopedLock sl (infoLock);
        samplePath = file;
        smpPeaks = std::move (pk);
        smpSeconds = frames / reader->sampleRate;
    }
    smpSlot.push (std::move (data));
    sampleLoaded.store (true);

    // loading a sample while on ADD only switches to SAMPLE
    if ((int) pSource->load() == 0)
        if (auto* src = apvts.getParameter (ParamID::source))
            src->setValueNotifyingHost (src->convertTo0to1 (1.0f));
    return true;
}

void GranulumProcessor::setHarmonics (const std::array<float, granulum::kNumHarmonics>& h)
{
    harmonics = h;
    harmonicsDirty = true;
}

bool GranulumProcessor::showingSample() const
{
    return sampleLoaded.load() && (int) pSource->load() != 0;
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
    return showingSample() ? smpSeconds : addSeconds;
}

void GranulumProcessor::timerCallback()
{
    addSlot.collect();
    smpSlot.collect();

    if (harmonicsDirty)
    {
        harmonicsDirty = false;
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

void GranulumProcessor::randomize()
{
    for (int s = 0; s < 4; ++s)
        if (s != 3 || (int) pSource->load() != 1)
            randomizeSection (s);
}

void GranulumProcessor::randomizeSection (int section)
{
    Random r;
    auto set = [this] (const char* id, float norm)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (jlimit (0.0f, 1.0f, norm));
            p->endChangeGesture();
        }
    };
    auto maybe = [&r] (float chance, float value) { return r.nextFloat() < chance ? value : 0.0f; };

    if (section == 0)
    {
        set (ParamID::attack,      r.nextFloat());
        set (ParamID::hold,        0.1f + r.nextFloat() * 0.75f);
        set (ParamID::spacing,     0.1f + r.nextFloat() * 0.7f);
        set (ParamID::waveSpacing, r.nextFloat() < 0.25f ? 0.5f : 0.5f + (r.nextFloat() - 0.35f) * 0.55f);
        set (ParamID::position,    r.nextFloat() * 0.8f);
        set (ParamID::rand,        maybe (0.5f, r.nextFloat() * 0.6f));
        set (ParamID::smear,       maybe (0.4f, r.nextFloat() * 0.8f));
        set (ParamID::pan,         0.2f + r.nextFloat() * 0.8f);
    }
    else if (section == 1)
    {
        set (ParamID::sub,   r.nextFloat() * 0.8f);
        set (ParamID::xover, r.nextFloat() * 0.7f);
        set (ParamID::drive, r.nextFloat() * 0.8f);
        set (ParamID::glide, maybe (0.6f, r.nextFloat() * 0.6f));
        set (ParamID::pitch, r.nextFloat() < 0.5f ? 0.5f : 0.5f + (r.nextFloat() - 0.5f) * 0.5f);
    }
    else if (section == 2)
    {
        const int type = r.nextInt (3);
        set (ParamID::ftype,  (float) type / 2.0f);
        set (ParamID::cutoff, type == 0 ? 0.45f + r.nextFloat() * 0.55f
                            : type == 1 ? 0.35f + r.nextFloat() * 0.4f
                                        : r.nextFloat() * 0.45f);
        set (ParamID::reso,   r.nextFloat() * 0.7f);
        set (ParamID::space,  r.nextFloat() * 0.7f);
        set (ParamID::room,   0.3f + r.nextFloat() * 0.7f);
    }
    else
    {
        std::array<float, granulum::kNumHarmonics> h {};
        h[0] = 0.6f + 0.4f * r.nextFloat();
        for (int k = 1; k < granulum::kNumHarmonics; ++k)
        {
            const float tilt = 1.0f / (1.0f + (float) k * (0.15f + 0.4f * r.nextFloat()));
            h[(size_t) k] = r.nextFloat() < 0.55f ? r.nextFloat() * tilt * 1.6f : 0.0f;
        }
        setHarmonics (h);
    }
}

// ---------------------------------------------------------------------------

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
        if (parts.size() == 3)
            setModulation (modIndexOf (parts[0]), parts[1].getIntValue(), parts[2].getFloatValue());
    }
    ++modVersion;

    const int src = (int) pSource->load();
    File f (path);
    String err;
    if (path.isNotEmpty() && f.existsAsFile())
    {
        loadSample (f, err);
        if (auto* p = apvts.getParameter (ParamID::source))   // keep the saved source choice
            p->setValueNotifyingHost (p->convertTo0to1 ((float) src));
    }
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
