#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "Engine.h"

namespace ParamID
{
    // oscillators
    inline constexpr const char* subVol   = "subVol";
    inline constexpr const char* subOct   = "subOct";
    inline constexpr const char* subSemi  = "subSemi";
    inline constexpr const char* addVol   = "addVol";
    inline constexpr const char* addOct   = "addOct";
    inline constexpr const char* addSemi  = "addSemi";
    inline constexpr const char* smpVol   = "smpVol";
    inline constexpr const char* smpOct   = "smpOct";
    inline constexpr const char* smpSemi  = "smpSemi";
    // grain
    inline constexpr const char* grainAdd = "grainAdd";
    inline constexpr const char* grainSmp = "grainSmp";
    inline constexpr const char* tone     = "tone";
    inline constexpr const char* texture  = "texture";
    inline constexpr const char* motion   = "motion";
    inline constexpr const char* amount   = "amount";
    // filter
    inline constexpr const char* filterSub = "filterSub";
    inline constexpr const char* filterAdd = "filterAdd";
    inline constexpr const char* filterSmp = "filterSmp";
    inline constexpr const char* cutoff    = "cutoff";
    inline constexpr const char* reso      = "reso";
    inline constexpr const char* ftype     = "ftype";
    // dirt
    inline constexpr const char* dirtSub   = "dirtSub";
    inline constexpr const char* dirtAdd   = "dirtAdd";
    inline constexpr const char* dirtSmp   = "dirtSmp";
    inline constexpr const char* dirtType  = "dirtType";
    inline constexpr const char* dirtDrive = "dirtDrive";
    inline constexpr const char* dirtTone  = "dirtTone";
    inline constexpr const char* dirtMix   = "dirtMix";
    // space
    inline constexpr const char* spaceSub  = "spaceSub";
    inline constexpr const char* spaceAdd  = "spaceAdd";
    inline constexpr const char* spaceSmp  = "spaceSmp";
    inline constexpr const char* spaceAmt  = "spaceAmt";
    inline constexpr const char* spaceSize = "spaceSize";
    // voice / global
    inline constexpr const char* mono      = "mono";
    inline constexpr const char* glide     = "glide";
    inline constexpr const char* pitch     = "pitch";
    inline constexpr const char* output    = "output";
    // LFOs: "lfo1Rate", "lfo1Sync", "lfo1Div", "lfo1Mode" ...
    inline juce::String lfo (int index, const char* what) { return "lfo" + juce::String (index + 1) + what; }
}

static constexpr int kNumLfos = 2;
static constexpr int kMaxMods = 48;
static constexpr int kLfoTable = 512;

enum ModTarget
{
    mtSubVol, mtSubOct, mtSubSemi, mtAddVol, mtAddOct, mtAddSemi, mtSmpVol, mtSmpOct, mtSmpSemi,
    mtTone, mtTexture, mtMotion, mtAmount,
    mtCutoff, mtReso,
    mtDirtDrive, mtDirtTone, mtDirtMix,
    mtSpaceAmt, mtSpaceSize,
    mtGlide, mtPitch, mtOutput,
    mtLfo1Rate, mtLfo2Rate,
    kNumModTargets
};

enum Stem { stemSub = 0, stemAdd = 1, stemSmp = 2, kNumStems = 3 };

struct WaveformPeaks
{
    std::vector<float> mins, maxs;
};

struct LfoPoint { float x = 0, y = 0, curve = 0; };

// Lock-free hand-over of the sample from the message thread to the audio thread
struct SampleSlot
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
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { lp[c] = dcX[c] = dcY[c] = hold[c] = 0.0f; count[c] = 0; } }
    void process (float* L, float* R, int n, int type, float drive, float tone, float mix);
    static const juce::StringArray& typeNames();
private:
    float shape (int type, float x, float drive, int ch);
    double sr = 44100.0;
    float lp[2] {}, dcX[2] {}, dcY[2] {}, hold[2] {};
    int count[2] {};
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

    const juce::String getName() const override { return "Granulum"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sampler (message thread) ----
    bool loadSample (const juce::File& file, juce::String& error);
    juce::String getSampleName() const;
    bool hasSample() const { return sampleLoaded.load(); }
    std::shared_ptr<const WaveformPeaks> getPeaks() const;
    static bool isSupportedAudioFile (const juce::String& path);
    float getRootNote() const;               // detected + user shift
    bool  isRootDetected() const { return rootDetected.load(); }
    void  shiftRoot (int semitones);

    // ---- additive bars ----
    float getHarmonic (int i) const { return harmonics[(size_t) i].load(); }
    void  setHarmonic (int i, float v) { harmonics[(size_t) i].store (juce::jlimit (0.0f, 1.0f, v)); }

    // ---- smart random: returns normalised targets so the UI can glide to them ----
    struct RandomTarget { juce::String id; float norm; };
    std::vector<RandomTarget> makeRandom (int section);           // 1 grain, 2 filter, 3 dirt, 4 space
    std::array<float, granulum::kNumHarmonics> makeRandomBars();

    // ---- modulation ----
    static const char* modTargetId (int target);
    static int modIndexOf (const juce::String& paramId);
    void setModulation (int target, int lfo, float depth);
    void removeModulation (int target, int lfo);
    bool hasModulation (int target, int lfo) const;
    float getModulationDepth (int target, int lfo) const;
    int countModulations (int lfo) const;

    // ---- drawable LFO shapes ----
    std::vector<LfoPoint> getLfoPoints (int lfo) const;
    void setLfoPoints (int lfo, std::vector<LfoPoint> pts);
    float lfoShapeAt (int lfo, float phase) const;
    static std::vector<LfoPoint> lfoPreset (int index);
    static const juce::StringArray& lfoPresetNames();
    static float segmentShape (float t, float curve);
    static double divisionBeats (int index);
    static const juce::StringArray& divisionNames();

    std::atomic<uint32_t> modVersion { 1 };
    std::array<std::atomic<float>, kNumModTargets> modNorm {};
    std::array<std::atomic<float>, kNumLfos> lfoValueOut {}, lfoPhaseOut {};
    std::array<std::atomic<float>, kNumStems> stemLevel {};
    std::atomic<float> outputLevel { 0.0f };

    juce::AudioProcessorValueTreeState apvts;
    granulum::Engine engine;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void renderRange (int from, int to);
    void processChunk (int from, int len);
    void computeModulated (float* values);
    void advanceLfos (int endSample, int len, const float* values);
    granulum::Params makeParams (const float* v);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void rebuildLfoTable (int lfo);
    static juce::String pointsToString (const std::vector<LfoPoint>&);
    static std::vector<LfoPoint> pointsFromString (const juce::String&);

    juce::AudioFormatManager formatManager;
    SampleSlot smpSlot;
    std::atomic<bool> sampleLoaded { false }, rootDetected { false };
    std::atomic<float> detectedRoot { 60.0f };
    std::atomic<int> rootShift { 0 };

    std::array<std::atomic<float>, granulum::kNumHarmonics> harmonics {};
    std::array<float, granulum::kNumHarmonics> harmonicsSmoothed {};

    mutable juce::CriticalSection infoLock;
    juce::File samplePath;
    std::shared_ptr<const WaveformPeaks> peaks;

    // modulation
    struct ModSlot
    {
        std::atomic<int>   target { -1 };
        std::atomic<int>   lfo    { 0 };
        std::atomic<float> depth  { 0.0f };
    };
    std::array<ModSlot, kMaxMods> mods;
    std::array<juce::RangedAudioParameter*, kNumModTargets> modParams {};

    // LFO shapes: points (message thread) + double-buffered tables (audio thread)
    std::array<std::vector<LfoPoint>, kNumLfos> lfoPoints;
    std::array<std::array<std::array<float, kLfoTable + 1>, 2>, kNumLfos> lfoTables {};
    std::array<std::atomic<int>, kNumLfos> lfoTableIndex {};
    mutable juce::CriticalSection lfoLock;

    std::array<float, kNumModTargets> lastV {};
    std::array<double, kNumLfos> lfoPhase {};
    std::array<float, kNumLfos>  lfoValue {};
    int heldNotes = 0;
    double hostBpm = 120.0, hostPpq = -1.0;
    bool hostPlaying = false;

    // dsp
    std::array<juce::dsp::StateVariableTPTFilter<float>, kNumStems> filters;
    std::array<Distortion, kNumStems> dirts;
    juce::dsp::Reverb reverb;
    juce::dsp::LinkwitzRileyFilter<float> monoSplit;
    juce::SmoothedValue<float> outGain;
    std::array<juce::AudioBuffer<float>, kNumStems> stems;
    juce::AudioBuffer<float> mixBuf, sendBuf;
    std::array<float, kNumStems> lastVol {};
    std::array<double, kNumStems> stemEnergy {};
    float sendHp[2] {};
    float blockPeak = 0.0f;
    int blockSamples = 0;

    std::atomic<float>* pGrainAdd {}; std::atomic<float>* pGrainSmp {};
    std::array<std::atomic<float>*, kNumStems> pFilterOn {}, pDirtOn {}, pSpaceOn {};
    std::atomic<float>* pFtype {}; std::atomic<float>* pDirtType {}; std::atomic<float>* pMono {};
    std::array<std::atomic<float>*, kNumLfos> pLfoSync {}, pLfoDiv {}, pLfoMode {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumProcessor)
};
