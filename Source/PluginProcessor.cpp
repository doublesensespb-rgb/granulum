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
}

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
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfoDepth, 1 },    "LFO Depth",     NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfoRate, 1 },     "LFO Speed",     skewed (0.02f, 20.0f, 1.0f), 0.5f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, v < 1.0f ? 2 : 1, " Hz"); })));
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
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::source, 1 }, "Source", StringArray { "Additive", "Sample", "Both" }, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                       A().withStringFromValueFunction ([] (float v, int) { return "ADD " + String (roundToInt (v * 100.0f)) + " %"; })));

    return { ps.begin(), ps.end() };
}

GranulumProcessor::GranulumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRANULUM", createLayout())
{
    formatManager.registerBasicFormats();

    pAttack   = apvts.getRawParameterValue (ParamID::attack);
    pHold     = apvts.getRawParameterValue (ParamID::hold);
    pSpacing  = apvts.getRawParameterValue (ParamID::spacing);
    pWave     = apvts.getRawParameterValue (ParamID::waveSpacing);
    pPosition = apvts.getRawParameterValue (ParamID::position);
    pRand     = apvts.getRawParameterValue (ParamID::rand);
    pSmear    = apvts.getRawParameterValue (ParamID::smear);
    pPan      = apvts.getRawParameterValue (ParamID::pan);
    pLfoDepth = apvts.getRawParameterValue (ParamID::lfoDepth);
    pLfoRate  = apvts.getRawParameterValue (ParamID::lfoRate);
    pTrans    = apvts.getRawParameterValue (ParamID::transients);
    pKeySync  = apvts.getRawParameterValue (ParamID::keySync);
    pSub      = apvts.getRawParameterValue (ParamID::sub);
    pXover    = apvts.getRawParameterValue (ParamID::xover);
    pDrive    = apvts.getRawParameterValue (ParamID::drive);
    pGlide    = apvts.getRawParameterValue (ParamID::glide);
    pPitch    = apvts.getRawParameterValue (ParamID::pitch);
    pMono     = apvts.getRawParameterValue (ParamID::mono);
    pCutoff   = apvts.getRawParameterValue (ParamID::cutoff);
    pReso     = apvts.getRawParameterValue (ParamID::reso);
    pFtype    = apvts.getRawParameterValue (ParamID::ftype);
    pSpace    = apvts.getRawParameterValue (ParamID::space);
    pRoom     = apvts.getRawParameterValue (ParamID::room);
    pOutput   = apvts.getRawParameterValue (ParamID::output);
    pSource   = apvts.getRawParameterValue (ParamID::source);
    pMix      = apvts.getRawParameterValue (ParamID::mix);

    // starting point: a few low partials so there's sound right away
    const float init[] = { 1.0f, 0.55f, 0.35f, 0.22f, 0.12f, 0.08f };
    for (size_t i = 0; i < std::size (init); ++i) harmonics[i] = init[i];

    timerCallback(); // build the first source immediately
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
    engine.prepare (sampleRate);

    dsp::ProcessSpec spec { sampleRate, (uint32) jmax (1, samplesPerBlock), 2 };
    filter.prepare (spec);   filter.reset();
    crossover.prepare (spec); crossover.reset();
    reverb.prepare (spec);   reverb.reset();

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (pOutput->load()));

    work.setSize (2, jmax (1, samplesPerBlock), false, true, true);
    subBuf.setSize (1, jmax (1, samplesPerBlock), false, true, true);
}

void GranulumProcessor::readParams (granulum::Params& p) const
{
    p.attack      = pAttack->load();
    p.holdMs      = pHold->load();
    p.spacingMs   = pSpacing->load();
    p.waveSpacing = pWave->load();
    p.position    = pPosition->load();
    p.rand        = pRand->load();
    p.smear       = pSmear->load();
    p.pan         = pPan->load();
    p.lfoDepth    = pLfoDepth->load();
    p.lfoRate     = pLfoRate->load();
    p.transients  = pTrans->load() > 0.5f;
    p.keySync     = pKeySync->load() > 0.5f;
    p.sub         = pSub->load();
    p.glideMs     = pGlide->load();
    p.pitch       = pPitch->load();
    p.mono        = pMono->load() > 0.5f;
}

void GranulumProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    {
        const SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked() && pendingSample != nullptr && retiredSample == nullptr)
        {
            retiredSample = std::move (activeSample);
            activeSample  = std::move (pendingSample);
            pendingSample = nullptr;
        }
    }

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (work.getNumSamples() < numSamples)   work.setSize (2, numSamples, false, false, true);
    if (subBuf.getNumSamples() < numSamples) subBuf.setSize (1, numSamples, false, false, true);
    work.clear();
    subBuf.clear();

    granulum::Params p;
    readParams (p);

    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    float* S = subBuf.getWritePointer (0);
    const granulum::SampleData* sample = activeSample.get();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, numSamples, meta.samplePosition);
        if (at > pos)
        {
            engine.render (L + pos, R + pos, S + pos, at - pos, p, sample);
            pos = at;
        }
        const auto m = meta.getMessage();
        if (m.isNoteOn())                                engine.noteOn (m.getNoteNumber(), m.getFloatVelocity(), p.mono);
        else if (m.isNoteOff())                          engine.noteOff (m.getNoteNumber(), p.mono);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < numSamples)
        engine.render (L + pos, R + pos, S + pos, numSamples - pos, p, sample);
    midi.clear();

    // ---- drive (grains only) ----
    const float drive = pDrive->load();
    if (drive > 0.001f)
    {
        const float g = 1.0f + 30.0f * drive * drive;
        const float comp = 1.0f / std::pow (g, 0.7f);
        for (int i = 0; i < numSamples; ++i)
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
    filter.setCutoffFrequency (jlimit (20.0f, (float) (getSampleRate() * 0.45), pCutoff->load()));
    filter.setResonance (pReso->load());
    auto block = dsp::AudioBlock<float> (work).getSubBlock (0, (size_t) numSamples);
    dsp::ProcessContextReplacing<float> ctx (block);
    if (! (ft == 0 && pCutoff->load() > 19500.0f && pReso->load() < 0.75f))
        filter.process (ctx);

    // ---- crossover: everything below X-Over goes mono, reverb only on the top ----
    const float xover = pXover->load();
    const bool split = xover > 21.0f;
    if (split)
    {
        crossover.setCutoffFrequency (xover);
        for (int i = 0; i < numSamples; ++i)
        {
            float lowL, highL, lowR, highR;
            crossover.processSample (0, L[i], lowL, highL);
            crossover.processSample (1, R[i], lowR, highR);
            const float lowMono = 0.5f * (lowL + lowR);
            L[i] = highL; R[i] = highR;
            S[i] += lowMono;               // mono lows ride with the sub
        }
    }

    const float space = pSpace->load();
    dsp::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.69f * pRoom->load();
    rp.damping  = 0.4f;
    rp.wetLevel = space * 0.9f;
    rp.dryLevel = 1.0f - space * 0.5f;
    rp.width    = 1.0f;
    reverb.setParameters (rp);
    if (space > 0.001f)
        reverb.process (ctx);

    // ---- output ----
    outGain.setTargetValue (Decibels::decibelsToGain (pOutput->load()));
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGain.getNextValue();
        L[i] = std::tanh ((L[i] + S[i]) * g);
        R[i] = std::tanh ((R[i] + S[i]) * g);
        peak = jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }
    outputLevel.store (jmax (peak, outputLevel.load() * 0.9f));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, work, jmin (ch, 1), 0, numSamples);
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

    fileSample = std::move (data);
    {
        const ScopedLock sl (infoLock);
        samplePath = file;
    }

    // dropping a sample while on ADD switches to SAMPLE
    if ((int) pSource->load() == 0)
        if (auto* src = apvts.getParameter (ParamID::source))
            src->setValueNotifyingHost (src->convertTo0to1 (1.0f));

    sourceDirty = true;
    timerCallback();
    return true;
}

void GranulumProcessor::setHarmonics (const std::array<float, granulum::kNumHarmonics>& h)
{
    harmonics = h;
    harmonicsDirty = true;
}

void GranulumProcessor::rebuildSource (int source, float mix)
{
    if (harmonicsDirty || additiveSample == nullptr)
    {
        additiveSample = std::make_shared<granulum::SampleData> (granulum::renderAdditive (harmonics));
        harmonicsDirty = false;
    }

    std::shared_ptr<granulum::SampleData> data;
    if (source == 0 || fileSample == nullptr)
        data = additiveSample;
    else if (source == 1)
        data = fileSample;
    else
    {
        // both: lay the additive tone (looped) under the sample
        const auto& f = *fileSample;
        const auto& a = *additiveSample;
        auto d = std::make_shared<granulum::SampleData>();
        d->sampleRate = f.sampleRate;
        d->transients = f.transients;
        d->channels = f.channels;
        const int N  = f.numFrames();
        const int AN = a.numFrames();
        const double step = a.sampleRate / f.sampleRate;
        const float* ad = a.channels[0].data();
        for (auto& ch : d->channels)
        {
            double ap = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const int i0 = (int) ap;
                const float fr = (float) (ap - i0);
                const float av = ad[i0] + (ad[(i0 + 1) % AN] - ad[i0]) * fr;
                ch[(size_t) i] = ch[(size_t) i] * (1.0f - mix) + av * mix;
                ap += step;
                if (ap >= AN) ap -= AN;
            }
        }
        float peak = 0.0f;
        for (auto& ch : d->channels) for (float v : ch) peak = jmax (peak, std::abs (v));
        if (peak > 1.0e-6f) for (auto& ch : d->channels) for (auto& v : ch) v *= 0.89f / peak;
        data = std::move (d);
    }

    auto pk = makePeaks (*data);
    {
        const ScopedLock sl (infoLock);
        peaks = std::move (pk);
        sampleSeconds = data->numFrames() / data->sampleRate;
    }
    {
        const SpinLock::ScopedLockType lock (sampleLock);
        pendingSample = std::move (data);
    }
}

String GranulumProcessor::getSampleName() const
{
    const ScopedLock sl (infoLock);
    return samplePath == File() ? String() : samplePath.getFileName();
}

std::shared_ptr<const WaveformPeaks> GranulumProcessor::getPeaks() const
{
    const ScopedLock sl (infoLock);
    return peaks;
}

double GranulumProcessor::getSampleLengthSeconds() const
{
    const ScopedLock sl (infoLock);
    return sampleSeconds;
}

void GranulumProcessor::timerCallback()
{
    // free retired data here, never on the audio thread
    std::shared_ptr<granulum::SampleData> toFree;
    {
        const SpinLock::ScopedLockType lock (sampleLock);
        toFree = std::move (retiredSample);
        retiredSample = nullptr;
    }

    const int source = (int) pSource->load();
    const float mix  = pMix->load();
    const bool mixChanged = source == 2 && std::abs (mix - lastMix) > 0.002f;
    const bool needsAdd   = source != 1 || fileSample == nullptr;

    if (sourceDirty || source != lastSource || mixChanged || (harmonicsDirty && needsAdd) || additiveSample == nullptr)
    {
        rebuildSource (source, mix);
        lastSource = source;
        lastMix = mix;
        sourceDirty = false;
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
        set (ParamID::lfoDepth,    maybe (0.5f, r.nextFloat() * 0.5f));
        set (ParamID::lfoRate,     r.nextFloat());
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

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GranulumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto state = ValueTree::fromXml (*xml);
        const String path = state.getProperty ("samplePath").toString();
        const auto hs = StringArray::fromTokens (state.getProperty ("harmonics").toString(), ",", "");
        apvts.replaceState (state);

        if (hs.size() == granulum::kNumHarmonics)
        {
            std::array<float, granulum::kNumHarmonics> h {};
            for (int i = 0; i < hs.size(); ++i) h[(size_t) i] = jlimit (0.0f, 1.0f, hs[i].getFloatValue());
            setHarmonics (h);
        }

        File f (path);
        String err;
        const int src = (int) pSource->load();
        if (path.isNotEmpty() && f.existsAsFile())
        {
            loadSample (f, err);
            if (auto* p = apvts.getParameter (ParamID::source))   // keep the saved source choice
                p->setValueNotifyingHost (p->convertTo0to1 ((float) src));
        }
        sourceDirty = true;
        timerCallback();
    }
}

AudioProcessorEditor* GranulumProcessor::createEditor()
{
    return new GranulumEditor (*this);
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GranulumProcessor();
}
