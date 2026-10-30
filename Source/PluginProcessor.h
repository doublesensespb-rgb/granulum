#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainEngine.h"

namespace ParamID
{
    // granular block
    inline constexpr const char* grainOn     = "grainOn";
    inline constexpr const char* attack      = "gAttack";
    inline constexpr const char* hold        = "hold";
    inline constexpr const char* spacing     = "spacing";
    inline constexpr const char* waveSpacing = "waveSpacing";
    inline constexpr const char* position    = "position";
    inline constexpr const char* rand        = "rand";
    inline constexpr const char* smear       = "smear";
    inline constexpr const char* pan         = "pan";
    inline constexpr const char* transients  = "transients";
    inline constexpr const char* keySync     = "keySync";
    // oscillators
    inline constexpr const char* osc1Vol     = "osc1Vol";
    inline constexpr const char* osc1Oct     = "osc1Oct";
    inline constexpr const char* osc1Semi    = "osc1Semi";
    inline constexpr const char* osc1Grain   = "osc1Grain";
    inline constexpr const char* osc2Vol     = "osc2Vol";
    inline constexpr const char* osc2Oct     = "osc2Oct";
    inline constexpr const char* osc2Semi    = "osc2Semi";
    inline constexpr const char* osc2Grain   = "osc2Grain";
    // filter block
    inline constexpr const char* filterOn    = "filterOn";
    inline constexpr const char* cutoff      = "cutoff";
    inline constexpr const char* reso        = "reso";
    inline constexpr const char* ftype       = "ftype";
    // effects block
    inline constexpr const char* fxOn        = "fxOn";
    inline constexpr const char* distType    = "distType";
    inline constexpr const char* distDrive   = "distDrive";
    inline constexpr const char* distTone    = "distTone";
    inline constexpr const char* distMix     = "distMix";
    inline constexpr const char* space       = "space";
    inline constexpr const char* room        = "room";
    // voice / global
    inline constexpr const char* mono        = "mono";
    inline constexpr const char* porta       = "porta";
    inline constexpr const char* portaAlways = "portaAlways";
    inline constexpr const char* portaScaled = "portaScaled";
    inline constexpr const char* output      = "output";
    // LFOs: "lfo1Shape", "lfo1Rate", "lfo1Sync", "lfo1Div", "lfo1Mode" ...
    inline juce::String lfo (int index, const char* what) { return "lfo" + juce::String (index + 1) + what; }
}

static constexpr int kNumLfos = 2;
static constexpr int kMaxMods = 48;

enum ModTarget
{
    mtAttack, mtHold, mtSpacing, mtWave, mtPosition, mtRand, mtSmear, mtPan,
    mtOsc1Vol, mtOsc1Oct, mtOsc1Semi, mtOsc2Vol, mtOsc2Oct, mtOsc2Semi,
    mtCutoff, mtReso,
    mtDistDrive, mtDistTone, mtDistMix, mtSpace, mtRoom,
    mtPorta, mtOutput,
    mtLfo1Rate, mtLfo2Rate,
    kNumModTargets
};

struct WaveformPeaks
{
    std::vector<float> mins, maxs;  // per bin, both channels merged
    std::vector<float> transients;  // normalised 0..1
};

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

// Five distortion algorithms with tone + mix
class Distortion
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { lp[c] = dcX[c] = dcY[c] = hold[c] = 0.0f; count[c] = 0; }
    }
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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sources (message thread) ----
    bool loadSample (const juce::File& file, juce::String& error);
    juce::String getSampleName() const;
    bool hasSample() const { return sampleLoaded.load(); }
    bool showingSample() const;   // waveform view shows OSC 2 (otherwise OSC 1)
    std::shared_ptr<const WaveformPeaks> getPeaks() const;
    double getSampleLengthSeconds() const;
    static bool isSupportedAudioFile (const juce::String& path);
    granulum::GrainEngine& vizEngine() { return showingSample() ? engineSmp : engineAdd; }

    std::array<float, granulum::kNumHarmonics> getHarmonics() const { return harmonics; }
    void setHarmonics (const std::array<float, granulum::kNumHarmonics>&);
    void rebuildNow() { timerCallback(); }

    void randomize();
    void randomizeSection (int section); // 0 granular, 1 filter, 2 effects, 3 additive bars

    // ---- modulation ----
    static const char* modTargetId (int target);
    static int modIndexOf (const juce::String& paramId);
    void setModulation (int target, int lfo, float depth);
    void removeModulation (int target, int lfo);
    bool hasModulation (int target, int lfo) const;
    float getModulationDepth (int target, int lfo) const;
    int countModulations (int lfo) const;

    static float lfoShapeValue (int shape, float phase, float sh, float prev, float next);
    static double divisionBeats (int index);
    static const juce::StringArray& divisionNames();
    static const juce::StringArray& shapeNames();

    std::atomic<uint32_t> modVersion { 1 };
    std::array<std::atomic<float>, kNumModTargets> modNorm {};
    std::array<std::atomic<float>, kNumLfos> lfoValueOut {}, lfoPhaseOut {};

    juce::AudioProcessorValueTreeState apvts;
    granulum::GrainEngine engineAdd, engineSmp;
    juce::MidiKeyboardState keyboardState;

    std::atomic<float> outputLevel { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    void renderRange (int from, int to);
    void processChunk (int from, int len);
    void computeModulated (float* values);
    granulum::Params voiceParams() const;
    void advanceLfos (int endSample, int len, const float* values);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    static std::shared_ptr<WaveformPeaks> makePeaks (const granulum::SampleData&);

    juce::AudioFormatManager formatManager;

    SourceSlot addSlot, smpSlot;
    std::array<float, granulum::kNumHarmonics> harmonics {};
    bool harmonicsDirty = true;
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

    std::array<double, kNumLfos> lfoPhase {};
    std::array<float, kNumLfos>  lfoValue {}, lfoSH {}, lfoPrev {}, lfoNext {};
    granulum::Random lfoRng { 0xC0FFEEull };
    int heldNotes = 0;
    double hostBpm = 120.0, hostPpq = -1.0;
    bool hostPlaying = false;
    float lastPorta = 0.0f;

    juce::dsp::StateVariableTPTFilter<float> filter;
    Distortion distortion;
    juce::dsp::Reverb reverb;
    juce::SmoothedValue<float> outGain;
    juce::AudioBuffer<float> addBuf, smpBuf, work;
    float lastG1 = 0.8f, lastG2 = 0.8f, blockPeak = 0.0f;

    std::atomic<float>* pGrainOn {};  std::atomic<float>* pTrans {};    std::atomic<float>* pKeySync {};
    std::atomic<float>* pOsc1Grain {}; std::atomic<float>* pOsc2Grain {};
    std::atomic<float>* pFilterOn {}; std::atomic<float>* pFtype {};
    std::atomic<float>* pFxOn {};     std::atomic<float>* pDistType {};
    std::atomic<float>* pMono {};     std::atomic<float>* pPortaAlways {}; std::atomic<float>* pPortaScaled {};
    std::array<std::atomic<float>*, kNumLfos> pLfoShape {}, pLfoSync {}, pLfoDiv {}, pLfoMode {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranulumProcessor)
};
