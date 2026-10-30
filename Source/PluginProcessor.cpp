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

    auto pct = A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });

    auto msFmt = A().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? fmt (v, v < 10.0f ? 1 : 0, " ms") : fmt (v / 1000.0f, 2, " s"); });

    ps.push_back (std::make_unique<P> (ParameterID { ParamID::attack, 1 },      "Attack",       NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::hold, 1 },        "Hold",         skewed (1.0f, 1000.0f, 60.0f), 80.0f, msFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::spacing, 1 },     "Grain Spacing",skewed (1.0f, 1000.0f, 60.0f), 60.0f, msFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::waveSpacing, 1 }, "Wave Spacing", NormalisableRange<float> (-3.0f, 3.0f, 0.001f), 1.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return std::abs (v) < 0.005f ? String ("FREEZE") : String (roundToInt (v * 100.0f)) + " %"; })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::position, 1 },    "Start",        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::rand, 1 },        "Rand",         NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::smear, 1 },       "Smear",        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pan, 1 },         "Pan",          NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfoDepth, 1 },    "LFO Depth",    NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfoRate, 1 },     "LFO Speed",    skewed (0.02f, 20.0f, 1.0f), 0.5f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, v < 1.0f ? 2 : 1, " Hz"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pitch, 1 },       "Pitch",        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + fmt (v, 2, " st"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pitchSpray, 1 },  "Pitch Spray",  skewed (0.0f, 12.0f, 1.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return String::fromUTF8 ("\xc2\xb1") + fmt (v, 2, " st"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::octave, 1 },      "Octaves",      NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reverse, 1 },     "Reverse",      NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::transients, 1 }, "Transients", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::cutoff, 1 },     "Cutoff",      skewed (20.0f, 20000.0f, 1000.0f), 20000.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? fmt (v, 0, " Hz") : fmt (v / 1000.0f, 1, " kHz"); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reso, 1 },       "Resonance",   skewed (0.5f, 10.0f, 1.5f), 0.707f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, ""); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::ftype, 1 }, "Filter Type", StringArray { "Low Pass", "Band Pass", "High Pass" }, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::space, 1 },      "Space",       NormalisableRange<float> (0.0f, 1.0f), 0.25f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::room, 1 },       "Room Size",   NormalisableRange<float> (0.0f, 1.0f), 0.75f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::output, 1 },     "Output",      NormalisableRange<float> (-36.0f, 12.0f, 0.1f), -3.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 1, " dB"); })));

    return { ps.begin(), ps.end() };
}

GranulumProcessor::GranulumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GRANULUM", createLayout())
{
    formatManager.registerBasicFormats();

    pAttack     = apvts.getRawParameterValue (ParamID::attack);
    pHold       = apvts.getRawParameterValue (ParamID::hold);
    pSpacing    = apvts.getRawParameterValue (ParamID::spacing);
    pWave       = apvts.getRawParameterValue (ParamID::waveSpacing);
    pPosition   = apvts.getRawParameterValue (ParamID::position);
    pRand       = apvts.getRawParameterValue (ParamID::rand);
    pSmear      = apvts.getRawParameterValue (ParamID::smear);
    pPan        = apvts.getRawParameterValue (ParamID::pan);
    pLfoDepth   = apvts.getRawParameterValue (ParamID::lfoDepth);
    pLfoRate    = apvts.getRawParameterValue (ParamID::lfoRate);
    pPitch      = apvts.getRawParameterValue (ParamID::pitch);
    pPitchSpray = apvts.getRawParameterValue (ParamID::pitchSpray);
    pOctave     = apvts.getRawParameterValue (ParamID::octave);
    pReverse    = apvts.getRawParameterValue (ParamID::reverse);
    pTrans      = apvts.getRawParameterValue (ParamID::transients);
    pCutoff     = apvts.getRawParameterValue (ParamID::cutoff);
    pReso       = apvts.getRawParameterValue (ParamID::reso);
    pFtype      = apvts.getRawParameterValue (ParamID::ftype);
    pSpace      = apvts.getRawParameterValue (ParamID::space);
    pRoom       = apvts.getRawParameterValue (ParamID::room);
    pOutput     = apvts.getRawParameterValue (ParamID::output);

    startTimerHz (4);
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
    filter.prepare (spec);
    filter.reset();
    reverb.prepare (spec);
    reverb.reset();

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (pOutput->load()));

    work.setSize (2, jmax (1, samplesPerBlock), false, true, true);
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
    p.pitch       = pPitch->load();
    p.pitchSpray  = pPitchSpray->load();
    p.octaveProb  = pOctave->load();
    p.reverseProb = pReverse->load();
    p.transients  = pTrans->load() > 0.5f;
}

void GranulumProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // pick up a newly loaded sample without blocking
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

    if (work.getNumSamples() < numSamples)
        work.setSize (2, numSamples, false, false, true);
    work.clear();

    granulum::Params p;
    readParams (p);

    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    const granulum::SampleData* sample = activeSample.get();

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, numSamples, meta.samplePosition);
        if (at > pos)
        {
            engine.render (L + pos, R + pos, at - pos, p, sample);
            pos = at;
        }

        const auto m = meta.getMessage();
        if (m.isNoteOn())                         engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                   engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < numSamples)
        engine.render (L + pos, R + pos, numSamples - pos, p, sample);

    midi.clear();

    // ---- filter ----
    const int ft = (int) pFtype->load();
    filter.setType (ft == 0 ? dsp::StateVariableTPTFilterType::lowpass
                  : ft == 1 ? dsp::StateVariableTPTFilterType::bandpass
                            : dsp::StateVariableTPTFilterType::highpass);
    filter.setCutoffFrequency (jlimit (20.0f, (float) (getSampleRate() * 0.45), pCutoff->load()));
    filter.setResonance (pReso->load());

    const bool filterBypassed = (ft == 0 && pCutoff->load() > 19500.0f && pReso->load() < 0.75f);

    auto block = dsp::AudioBlock<float> (work).getSubBlock (0, (size_t) numSamples);
    dsp::ProcessContextReplacing<float> ctx (block);
    if (! filterBypassed)
        filter.process (ctx);

    // ---- space ----
    const float space = pSpace->load();
    dsp::Reverb::Parameters rp;
    rp.roomSize   = 0.3f + 0.69f * pRoom->load();
    rp.damping    = 0.35f;
    rp.wetLevel   = space * 0.9f;
    rp.dryLevel   = 1.0f - space * 0.55f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    if (space > 0.001f)
        reverb.process (ctx);

    // ---- output ----
    outGain.setTargetValue (Decibels::decibelsToGain (pOutput->load()));
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGain.getNextValue();
        L[i] = std::tanh (L[i] * g);   // gentle safety saturation
        R[i] = std::tanh (R[i] * g);
        peak = jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }
    outputLevel.store (jmax (peak, outputLevel.load() * 0.9f));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, work, jmin (ch, 1), 0, numSamples);
}

// ---------------------------------------------------------------------------

bool GranulumProcessor::isSupportedAudioFile (const String& path)
{
    const auto ext = File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac"
        || ext == ".ogg" || ext == ".mp3";
}

bool GranulumProcessor::loadSample (const File& file, String& error)
{
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Can't read this file";
        return false;
    }

    const double maxSeconds = 600.0;
    const auto frames = (int) jmin<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * maxSeconds));
    if (frames < 16)
    {
        error = "File is empty";
        return false;
    }

    const int chans = jlimit (1, 2, (int) reader->numChannels);
    AudioBuffer<float> tmp (chans, frames);
    reader->read (&tmp, 0, frames, 0, true, chans > 1);

    // normalise to -1 dBFS so quiet samples still sing
    float mag = tmp.getMagnitude (0, frames);
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

    // waveform overview
    auto pk = std::make_shared<WaveformPeaks>();
    for (int t : data->transients)
        pk->transients.push_back ((float) t / (float) frames);
    const int bins = 2048;
    pk->mins.assign (bins, 0.0f);
    pk->maxs.assign (bins, 0.0f);
    for (int b = 0; b < bins; ++b)
    {
        const int s0 = (int) ((int64) b * frames / bins);
        const int s1 = jmax (s0 + 1, (int) ((int64) (b + 1) * frames / bins));
        float lo = 0.0f, hi = 0.0f;
        for (int c = 0; c < chans; ++c)
            for (int i = s0; i < s1; ++i)
            {
                const float v = data->channels[(size_t) c][(size_t) i];
                lo = jmin (lo, v); hi = jmax (hi, v);
            }
        pk->mins[(size_t) b] = lo;
        pk->maxs[(size_t) b] = hi;
    }

    {
        const SpinLock::ScopedLockType lock (sampleLock);
        pendingSample = std::move (data);
    }
    {
        const ScopedLock sl (infoLock);
        samplePath = file;
        peaks = std::move (pk);
        sampleSeconds = frames / reader->sampleRate;
    }
    return true;
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
    // free the previous sample on the message thread, never on the audio thread
    std::shared_ptr<granulum::SampleData> toFree;
    {
        const SpinLock::ScopedLockType lock (sampleLock);
        toFree = std::move (retiredSample);
        retiredSample = nullptr;
    }
}

void GranulumProcessor::randomize()
{
    randomizeSection (0);
    randomizeSection (1);
    randomizeSection (2);
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
        set (ParamID::lfoDepth,    maybe (0.4f, r.nextFloat() * 0.5f));
        set (ParamID::lfoRate,     r.nextFloat());
    }
    else if (section == 1)
    {
        set (ParamID::pitch,       r.nextFloat() < 0.5f ? 0.5f : 0.5f + (float) (r.nextInt (5) - 2) * 12.0f / 48.0f);
        set (ParamID::pitchSpray,  maybe (0.5f, r.nextFloat() * 0.5f));
        set (ParamID::octave,      maybe (0.5f, r.nextFloat() * 0.6f));
        set (ParamID::reverse,     maybe (0.5f, r.nextFloat()));
    }
    else
    {
        // keep it musical: never kill the sound completely
        const int type = r.nextInt (3);
        set (ParamID::ftype,  (float) type / 2.0f);
        set (ParamID::cutoff, type == 0 ? 0.45f + r.nextFloat() * 0.55f
                            : type == 1 ? 0.35f + r.nextFloat() * 0.4f
                                        : r.nextFloat() * 0.55f);
        set (ParamID::reso,   r.nextFloat() * 0.7f);
        set (ParamID::space,  r.nextFloat() * 0.8f);
        set (ParamID::room,   0.3f + r.nextFloat() * 0.7f);
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
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GranulumProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            const String path = state.getProperty ("samplePath").toString();
            apvts.replaceState (state);

            if (path.isNotEmpty())
            {
                File f (path);
                String err;
                if (f.existsAsFile())
                    loadSample (f, err);
            }
        }
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
