#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainEngine.h"

namespace ParamID
{
    // OSC 1 (ADD)
    inline constexpr const char* osc1On    = "osc1On";
    inline constexpr const char* osc1Vol   = "osc1Vol";
    inline constexpr const char* osc1Oct   = "osc1Oct";
    inline constexpr const char* osc1Semi  = "osc1Semi";
    inline constexpr const char* osc1Grain = "osc1Grain";
    inline constexpr const char* chord     = "chord";
    // OSC 2 (SAMPLE)
    inline constexpr const char* osc2On    = "osc2On";
    inline constexpr const char* osc2Vol   = "osc2Vol";
    inline constexpr const char* osc2Oct   = "osc2Oct";
    inline constexpr const char* osc2Semi  = "osc2Semi";
    inline constexpr const char* osc2Grain = "osc2Grain";
    // OSC 3 (NOISE, built-in)
    inline constexpr const char* osc3On    = "osc3On";
    inline constexpr const char* osc3Vol   = "osc3Vol";
    inline constexpr const char* osc3Type  = "osc3Type";
    inline constexpr const char* osc3Color = "osc3Color";
    inline constexpr const char* osc3Grain = "osc3Grain";
    // GRANULAR macros
    inline constexpr const char* grainOn   = "grainOn";
    inline constexpr const char* bloom     = "bloom";
    inline constexpr const char* smear     = "smear";
    inline constexpr const char* scatter   = "scatter";
    inline constexpr const char* shimmer   = "shimmer";
    inline constexpr const char* drift     = "drift";
    inline constexpr const char* gsize     = "gsize";
    inline constexpr const char* loopMode  = "loopMode";
    inline constexpr const char* peakGain  = "peakGain";
    inline constexpr const char* peakGain2 = "peakGain2";
    inline constexpr const char* peakQ     = "peakQ";
    inline constexpr const char* subSemi   = "subSemi";
    inline constexpr const char* peakQ2    = "peakQ2";
    inline constexpr const char* loopStart = "loopStart";
    inline constexpr const char* loopEnd   = "loopEnd";
    inline constexpr const char* speed     = "speed";
    inline constexpr const char* position  = "position";
    inline constexpr const char* density   = "density";
    inline constexpr const char* gmix      = "gmix";
    inline constexpr const char* evolve    = "evolve";
    inline constexpr const char* chime     = "chime";
    // FILTER
    inline constexpr const char* filterOn  = "filterOn";
    inline constexpr const char* cutoff    = "cutoff";
    inline constexpr const char* reso      = "reso";
    inline constexpr const char* ftype     = "ftype";
    inline constexpr const char* filter2On = "filter2On";
    inline constexpr const char* cutoff2   = "cutoff2";
    inline constexpr const char* reso2     = "reso2";
    inline constexpr const char* ftype2    = "ftype2";
    // DISTORTION
    inline constexpr const char* distOn    = "distOn";
    inline constexpr const char* distType  = "distType";
    inline constexpr const char* distDrive = "distDrive";
    inline constexpr const char* distTone  = "distTone";
    inline constexpr const char* distMix   = "distMix";
    // DELAY
    inline constexpr const char* delayOn   = "delayOn";
    inline constexpr const char* delayTime = "delayTime";
    inline constexpr const char* delaySync = "delaySync";
    inline constexpr const char* delayDiv  = "delayDiv";
    inline constexpr const char* delayFb   = "delayFb";
    inline constexpr const char* delayTone = "delayTone";
    inline constexpr const char* delayMix  = "delayMix";
    inline constexpr const char* delayPing = "delayPing";
    // REVERB
    inline constexpr const char* revOn     = "revOn";
    inline constexpr const char* revMix    = "revMix";
    inline constexpr const char* revSize   = "revSize";
    inline constexpr const char* revLowCut = "revLowCut";
    // VOICE / global
    inline constexpr const char* mono      = "mono";
    // AMP envelope
    inline constexpr const char* ampA      = "ampA";
    inline constexpr const char* ampD      = "ampD";
    inline constexpr const char* ampS      = "ampS";
    inline constexpr const char* ampR      = "ampR";
    inline constexpr const char* ampOn     = "ampOn";
    inline constexpr const char* porta     = "porta";
    inline constexpr const char* output    = "output";
    inline constexpr const char* clipOn    = "clipOn";
    inline constexpr const char* osc1Tune  = "osc1Tune";    // 0.99.2: smooth pitch, -48..+48 semitones (like Serum COARSE)
    inline constexpr const char* osc2Tune  = "osc2Tune";
    inline constexpr const char* osc1Mode  = "osc1Mode";    // 0.99.5: 0 ADDITIVE (sticks), 1 WAVE (a full waveform)
    inline constexpr const char* osc1Wave  = "osc1Wave";    // SINE SAW SQUARE TRIANGLE (WAVE mode)
    inline constexpr const char* osc1WaveMix = "osc1WaveMix"; // MIX mode: 0 = sticks only .. 1 = wave only
    inline constexpr const char* pitch     = "pitch";       // master pitch of the whole synth, -24..+24 semitones, smooth (0.99.3)
    inline constexpr const char* osc1Pan   = "osc1Pan";
    inline constexpr const char* osc2Pan   = "osc2Pan";
    inline constexpr const char* osc3Pan   = "osc3Pan";
    inline constexpr const char* spread    = "spread";
    // OSC 4 SUB: 3 harmonics straight to the output (no grains, no filters, no effects)
    inline constexpr const char* subOn     = "subOn";
    inline constexpr const char* subVol    = "subVol";
    inline constexpr const char* subOct    = "subOct";
    inline constexpr const char* subH1     = "subH1";
    inline constexpr const char* subH2     = "subH2";
    inline constexpr const char* subH3     = "subH3";
    inline constexpr const char* subH4     = "subH4";
    inline constexpr const char* subH5     = "subH5";
    // LFOs: "lfo1Rate", "lfo1Sync", "lfo1Div", "lfo1Mode", "lfo1Rise" ...
    inline juce::String lfo (int index, const char* what) { return "lfo" + juce::String (index + 1) + what; }
    // which oscillators go through a block: "filterR1", "distR2", "delayR3", "revR1" ...
    inline juce::String route (const char* block, int osc) { return juce::String (block) + "R" + juce::String (osc + 1); }
}

static constexpr int kNumLfos = 3;
static constexpr int kMaxMods = 48;
static constexpr int kLfoTable = 512;
static constexpr int kLfoPoints = 4;        // points in the built-in shapes
static constexpr int kMaxLfoPoints = 16;    // the owner can add points up to this many
static constexpr int kNumOscs = 3;
// LFO on an OSC 1 stick: "virtual" mod targets (no parameter behind them), saved as "harm1".."harm16"
static constexpr int kHarmTarget0 = 1000;
inline bool isHarmTarget (int t) { return t >= kHarmTarget0 && t < kHarmTarget0 + 16; }
static constexpr int kNumRouteBlocks = 5;
static constexpr const char* kRouteBlocks[kNumRouteBlocks] = { "filter", "dist", "delay", "rev", "filter2" };

enum ModTarget
{
    mtOsc1Vol, mtOsc1Oct, mtOsc1Semi, mtOsc2Vol, mtOsc2Oct, mtOsc2Semi, mtOsc3Vol, mtOsc3Color,
    mtBloom, mtSmear, mtScatter, mtShimmer, mtDrift, mtChime, mtSize, mtSpeed, mtPosition, mtDensity, mtGmix,
    mtCutoff, mtReso, mtCutoff2, mtReso2,
    mtDistDrive, mtDistTone, mtDistMix, mtDelayTime, mtDelayFb, mtDelayTone, mtDelayMix, mtRevMix, mtRevSize, mtRevLowCut,
    mtPorta, mtOutput,
    mtLfo1Rate, mtLfo2Rate, mtLfo3Rate,
    mtPeakGain, mtPeakGain2,
    mtSubVol,
    mtOsc1Pan, mtOsc2Pan, mtOsc3Pan, mtSpread,
    mtSubH1, mtSubH2, mtSubH3, mtSubH4, mtSubH5,
    mtPeakQ, mtPeakQ2,
    mtSubSemi,
    mtOsc1Tune, mtOsc2Tune, mtPitch, mtOsc1WaveMix,
    kNumModTargets
};

struct WaveformPeaks
{
    std::vector<float> mins, maxs;
};

struct LfoPoint { float x = 0, y = 0, curve = 0; };
using LfoShape = std::vector<LfoPoint>;   // 2..16 points, x from 0 to 1

// Lock-free hand-over of a source buffer from the message thread to the audio thread
struct SourceSlot
{
    juce::SpinLock lock;
    std::shared_ptr<granulum::SampleData> pending, active, retired;

    void push (std::shared_ptr<granulum::SampleData> d)
    {
        const juce::SpinLock::ScopedLockType l (lock);
        pending = std::move (d);
    }
    void swapOnAudioThread()
    {
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked() && pending != nullptr && retired == nullptr)
        {
            retired = std::move (active);
            active  = std::move (pending);
            pending = nullptr;
        }
    }
    void collect()
    {
        std::shared_ptr<granulum::SampleData> toFree;
        const juce::SpinLock::ScopedLockType l (lock);
        toFree = std::move (retired);
        retired = nullptr;
    }
};

class Distortion
{
public:
    // 0.99.5: runs 4x oversampled (the harmonics it makes no longer fold back as dirt) and keeps the level:
    // DRIVE changes the character, not the loudness
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        os.initProcessing (64);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { lp[c] = dcX[c] = dcY[c] = hold[c] = pre[c] = 0.0f; count[c] = 0; envIn[c] = envWet[c] = 0.0f; }
        agc = 1.0f;
        os.reset();
    }
    void process (float* L, float* R, int n, int type, float drive, float tone, float mix);
    static const juce::StringArray& typeNames();
private:
    float shape (int type, float x, float drive, int ch);
    double sr = 44100.0;
    float lp[2] {}, dcX[2] {}, dcY[2] {}, hold[2] {};
    int count[2] {};
    float envIn[2] {}, envWet[2] {}, agc = 1.0f;
    float pre[2] {};   // gentle top cut before the shaper: noise crackles warm instead of fizzing
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };
};

// Multi-mode filter: LP12, LP24, HP, BP, NOTCH (TPT state-variable), COMB (tuned feedback comb), VOWEL (3 formants)
class FilterUnit
{
public:
    enum Type { LP12, LP24, HP, BP, Peak, Comb, Vowel };   // PEAK took NOTCH's place (same index)
    static const juce::StringArray& typeNames();
    void prepare (double sampleRate);
    void reset();
    void process (float* L, float* R, int n, int type, float cutoffHz, float q, float peakGainDb = 0.0f);
    // magnitude (linear) of this filter at frequency hz, for the filter view
    static float response (int type, float hz, float cutoffHz, float q, double sampleRate, float peakGainDb = 0.0f);
private:
    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        float a1 = 0, a2 = 0, a3 = 0, k = 1;
        void set (float fc, float Q, double sr);
        inline void tick (float x, float& lp, float& bp, float& hp)
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
    };
    double sr = 44100.0;
    Svf s1[2], s2[2], fm[3][2];
    std::vector<float> comb[2];
    int combW = 0;
    float combLp[2] {};
};

// Stereo delay with tone filter in the feedback loop and optional ping-pong
class Delay
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* L, float* R, int n, float timeMs, float feedback, float tone, float mix, bool ping);
private:
    std::vector<float> bufL, bufR;
    int write = 0;
    double sr = 44100.0;
    float curDelay = -1.0f, lpL = 0.0f, lpR = 0.0f;
};

// One effect chain per oscillator, so every block can choose which oscillators it processes
struct FxChain
{
    FilterUnit filter, filter2;
    Distortion distortion;
    Delay delay;
    juce::dsp::Reverb reverb;
    juce::dsp::StateVariableTPTFilter<float> revCut;   // LOW CUT on the reverb tail only
    double quietSeconds = 1.0e9;
    bool busDelay = false, busReverb = false;          // shared DIST bus only: what follows it
};

class GranulumProcessor final : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    GranulumProcessor();
    ~GranulumProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GRAIN FORM"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sources (message thread) ----
    bool loadSample (const juce::File& file, juce::String& error);
    juce::String getSampleName() const;
    // factory samples (stock atmospheres): Documents/GRAIN FORM/Samples, browsed with < > in OSC 2
    static juce::File factorySampleDir();
    static void installFactorySamples();
    bool isFactorySample() const;
    void loadAdjacentFactorySample (int direction);
    std::shared_ptr<const WaveformPeaks> getSamplePeaks() const { const juce::ScopedLock sl (infoLock); return smpPeaks; }
    bool hasSample() const { return sampleLoaded.load(); }
    void clearSample();   // the x on the sample box: OSC 2 becomes empty (and the preset no longer points to the file)
    bool showingSample() const;
    std::shared_ptr<const WaveformPeaks> getPeaks() const;
    double getSampleLengthSeconds() const;
    static bool isSupportedAudioFile (const juce::String& path);
    granulum::GrainEngine& vizEngine() { return showingSample() ? engineSmp : engineAdd; }

    // ---- OSC 3 noises (built in) ----
    static const juce::StringArray& noiseNames();
    std::shared_ptr<const WaveformPeaks> getNoisePeaks (int index) const;

    // ---- presets ----
    static juce::File presetFolder();
    juce::Array<juce::File> listPresets() const;                 // all presets: the root first, then each folder (0.99.2)
    static juce::Array<juce::File> listPresetFolders();          // sub-folders of the preset folder, A..Z
    static juce::Array<juce::File> presetsIn (const juce::File& folder);
    bool savePreset (const juce::String& name, const juce::File& folder = {});   // empty folder = the root
    bool loadPreset (const juce::File&);
    juce::String getPresetName() const;
    juce::String getPresetFolderName() const { return presetSub; }   // "" = root
    bool uiFold[3] {};        // 0.99.3: OSC 2 SUB / OSC 3 SAMPLE / OSC 4 NOISE folded into a strip (saved with the state)
    void presetFolderRenamed (const juce::String& from, const juce::String& to) { if (presetSub == from) presetSub = to; }
    juce::File currentPresetFile() const;

    std::array<float, granulum::kNumHarmonics> getHarmonics() const { return harmonics; }
    // which harmonics go through the granulator (bit k = harmonic k+1); the rest stay clean & mono
    uint32_t getGrainMask() const { return grainMask.load(); }
    void setGrainMask (uint32_t m) { grainMask.store (m & 0xffffu); harmonicsDirty.store (true); }
    void setHarmonics (const std::array<float, granulum::kNumHarmonics>&);
    void rebuildNow() { timerCallback(); }

    // ---- presets / random ----
    void resetToInit();
    void randomize();
    // 0.99.5: what RANDOM ALL may touch (ticks in its menu); kept with the project
    enum { rndSticksBit = 1, rndGrainsBit = 2, rndFiltersBit = 4, rndDistBit = 8, rndDelayBit = 16, rndReverbBit = 32,
           rndFilter2Bit = 64, rndLfo1Bit = 128, rndLfo2Bit = 256, rndLfo3Bit = 512 };   // rndFiltersBit = FILTER 1
    std::atomic<int> randMask { 1023 };
    void randomizeSection (int section); // 0 granular, 1 filter, 2 effects, 3 additive bars

    // ---- chords ----
    static const juce::StringArray& chordNames();
    static std::vector<int> chordIntervals (int index);

    // ---- modulation ----
    static const char* modTargetId (int target);
    static int modIndexOf (const juce::String& paramId);
    void setModulation (int target, int lfo, float depth);
    void removeModulation (int target, int lfo);
    bool hasModulation (int target, int lfo) const;
    float getModulationDepth (int target, int lfo) const;
    int countModulations (int lfo) const;
    bool isLfoOn (int lfo) const { return pLfoOn[(size_t) lfo]->load() > 0.5f; }

    // ---- 4-point LFO shapes ----
    LfoShape getLfoPoints (int lfo) const;
    void setLfoPoints (int lfo, LfoShape pts);
    float lfoShapeAt (int lfo, float phase) const;
    static LfoShape lfoPreset (int index);
    static const juce::StringArray& lfoPresetNames();
    static float segmentShape (float t, float curve);
    static double divisionBeats (int index);
    static const juce::StringArray& divisionNames();

    std::atomic<uint32_t> modVersion { 1 };
    std::array<std::atomic<float>, kNumModTargets> modNorm {};
    std::array<std::atomic<float>, kNumLfos> lfoValueOut {}, lfoPhaseOut {}, lfoRiseOut {};
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };   // max since the UI last read them (UI swaps in 0)
    std::atomic<bool>  clipped { false };
    std::atomic<bool>  soundReset { false };   // 0.99.2: a new preset / INIT / project state: stop the old sound and its tails (audio thread)
    float lastOut[2] {};                       // last output sample, for a click-free cut                 // set when the output hit 0 dBFS; the UI clears it
    std::atomic<float> keyCutoff { 20000.0f };
    // filter view (UI): key-tracked + modulated cutoff/reso of both filters, and a ring of the filter input
    std::atomic<float> keyCutoff2 { 20000.0f }, viewReso1 { 0.707f }, viewReso2 { 0.707f }, viewPeak1 { 0.0f }, viewPeak2 { 0.0f }, viewPeakQ1 { 1.0f }, viewPeakQ2 { 1.0f };
    static constexpr int kScopeSize = 4096;
    std::array<float, kScopeSize> scopeRing {};
    std::atomic<int> scopeWrite { 0 };

    juce::UndoManager undoManager { 30000, 60 };   // UNDO in the header: knob and switch changes
    juce::AudioProcessorValueTreeState apvts;
    granulum::GrainEngine engineAdd, engineAddClean, engineSmp, engineNoise, engineSub;
    granulum::GrainEngine engineWave;            // 0.99.5: OSC 1 WAVE (full waveform)
    std::atomic<float>* pOsc1Mode = nullptr;
    juce::AudioBuffer<float> waveBuf;
    float lastAddAmt = 1.0f;
    int builtWave = -1;
    double builtWaveRate = 0.0;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void renderRange (int from, int to);
    void processChunk (int from, int len);
    void computeModulated (float* values);
    granulum::Params voiceParams (const float* v, int osc) const;
    granulum::Params subParams (const float* v) const;
    void loadNoises();
    void advanceLfos (int endSample, int len, const float* values);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void rebuildLfoTable (int lfo);
    static std::shared_ptr<WaveformPeaks> makePeaks (const granulum::SampleData&);

    juce::AudioFormatManager formatManager;

    SourceSlot addSlot, addCleanSlot, smpSlot, noiseSlot, subSlot, waveSlot;
    static constexpr int kSubHarms = 5;   // SUB sticks: root, octave, octave + fifth, 2 octaves, 2 octaves + third
    std::array<float, 5> subShown { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float lastGSub = 0.0f;
    std::array<std::array<float, 2>, 3> volSmooth {};   // 0.99.4: OSC volume smoothing (no clicks from LFO jumps)
    std::array<float, 2> subSmooth {};
    std::atomic<uint32_t> grainMask { 0xffffu };
    std::vector<std::shared_ptr<granulum::SampleData>> noises;
    std::vector<std::shared_ptr<const WaveformPeaks>> noisePeaks;
    int shownNoise = -1;
    juce::String presetName { "INIT" };
    juce::String presetSub;   // folder of the current preset inside the preset folder ("" = root)
    std::array<float, granulum::kNumHarmonics> harmonics {};
    std::array<std::atomic<float>, granulum::kNumHarmonics> harmNow {};     // stick levels for the audio thread
    std::array<float, granulum::kNumHarmonics> harmOff {}, harmGainPrev {}; // LFO offsets per stick (audio thread)
    std::array<float, granulum::kNumHarmonics> subGainPrev {};
    uint32_t layerMaskBuilt = 0;                                            // sticks that currently have their own layer
public:
    std::array<std::atomic<float>, granulum::kNumHarmonics> harmLive {};    // stick level incl. LFO, for the UI
    void removeLfoEverywhere (int lfo);
private:
    std::atomic<bool> harmonicsDirty { true };
    std::atomic<bool> sampleLoaded { false };

    mutable juce::CriticalSection infoLock;
    juce::File samplePath;
    std::shared_ptr<const WaveformPeaks> addPeaks, smpPeaks;
    double addSeconds = 0.0, smpSeconds = 0.0;

    struct ModSlot
    {
        std::atomic<int>   target { -1 };
        std::atomic<int>   lfo    { 0 };
        std::atomic<float> depth  { 0.0f };
    };
    std::array<ModSlot, kMaxMods> mods;
    std::array<juce::RangedAudioParameter*, kNumModTargets> modParams {};
    std::array<float, kNumModTargets> lastV {};

    std::array<LfoShape, kNumLfos> lfoPoints {};
    std::array<std::array<std::array<float, kLfoTable + 1>, 2>, kNumLfos> lfoTables {};
    std::array<std::atomic<int>, kNumLfos> lfoTableIndex {};
    mutable juce::CriticalSection lfoLock;

    std::array<double, kNumLfos> lfoPhase {};
    std::array<float, kNumLfos>  lfoValue {};
    std::array<bool, kNumLfos>   lfoDone {};
    double sinceTrigger = 1.0e9;   // seconds since the first note of a phrase
    int heldNotes = 0;
    double hostBpm = 120.0, hostPpq = -1.0;
    bool hostPlaying = false;

    std::array<FxChain, kNumOscs> chains;
    FxChain subChain;   // the SUB's own effect chain (it only goes through blocks whose "2" digit is lit)
    juce::dsp::Oversampling<float> outClipOS { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true };   // CLIP (0.99.6)
    FxChain distBusChain;   // 0.99.5: the shared DIST (+ the delay / reverb that follow it for the distorted mix)
    juce::AudioBuffer<float> distBuf;
    juce::SmoothedValue<float> outGain;
    juce::AudioBuffer<float> addBuf, smpBuf, noiseBuf, work, wetBuf, subBuf;
    float lastG1 = 0.0f, lastG2 = 0.0f, lastG3 = 0.0f, blockPeak = 0.0f;
    float lastPan[kNumOscs] { 0.0f, 0.0f, 0.0f };
    float colorLp[2] {}, colorHp[2] {};

    std::atomic<float>* pOsc1On {};   std::atomic<float>* pOsc2On {};
    std::atomic<float>* pOsc1Grain {}; std::atomic<float>* pOsc2Grain {};
    std::atomic<float>* pChord {};
    std::atomic<float>* pGrainOn {};
    std::atomic<float>* pFilterOn {}; std::atomic<float>* pFtype {};
    std::atomic<float>* pFilter2On {}; std::atomic<float>* pFtype2 {};
    std::atomic<float>* pDistOn {};   std::atomic<float>* pDistType {};
    std::atomic<float>* pRevOn {};
    std::atomic<float>* pOsc3On {}; std::atomic<float>* pOsc3Type {}; std::atomic<float>* pOsc3Grain {};
    std::atomic<float>* pDelayOn {}; std::atomic<float>* pDelaySync {}; std::atomic<float>* pDelayDiv {}; std::atomic<float>* pDelayPing {};
    std::array<std::array<std::atomic<float>*, kNumOscs>, kNumRouteBlocks> pRoute {};
    std::array<std::atomic<float>*, kNumRouteBlocks> pRouteSub {};   // param "<block>R4" (index 3 = SUB)
    std::atomic<float>* pMono {};
    std::atomic<float>* pAmpA {}; std::atomic<float>* pAmpD {}; std::atomic<float>* pAmpS {}; std::atomic<float>* pAmpR {}; std::atomic<float>* pAmpOn {};
    std::array<std::atomic<float>*, kNumLfos> pLfoSync {}, pLfoDiv {}, pLfoMode {}, pLfoRise {}, pLfoOn {}, pLfoGrains {};
    std::array<double, kNumLfos> lfoRateNow {};
    std::atomic<float>* pClipOn {};
    std::atomic<float>* pSubOn {}; std::atomic<float>* pSubOct {}; std::array<std::atomic<float>*, 5> pSubH {};
    std::atomic<float>* pLoopMode {}; std::atomic<float>* pLoopStart {}; std::atomic<float>* pLoopEnd {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumProcessor)
};
