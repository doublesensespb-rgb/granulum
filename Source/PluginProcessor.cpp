#include <complex>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#ifndef GRAINFORM_VERSION
 #define GRAINFORM_VERSION "dev"
#endif

using namespace juce;

namespace
{
    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    String fmt (float v, int dec, const char* unit) { return (dec <= 0 ? String (roundToInt (v)) : String (v, dec)) + unit; }

    constexpr int kChunk = 32;

    const char* const kModTargetIds[kNumModTargets] =
    {
        ParamID::osc1Vol, ParamID::osc1Oct, ParamID::osc1Semi, ParamID::osc2Vol, ParamID::osc2Oct, ParamID::osc2Semi,
        ParamID::osc3Vol, ParamID::osc3Color,
        ParamID::bloom, ParamID::smear, ParamID::scatter, ParamID::shimmer, ParamID::drift, ParamID::chime,
        ParamID::gsize, ParamID::speed, ParamID::position, ParamID::density, ParamID::gmix,
        ParamID::cutoff, ParamID::reso, ParamID::cutoff2, ParamID::reso2,
        ParamID::distDrive, ParamID::distTone, ParamID::distMix,
        ParamID::delayTime, ParamID::delayFb, ParamID::delayTone, ParamID::delayMix,
        ParamID::revMix, ParamID::revSize, ParamID::revLowCut,
        ParamID::porta, ParamID::output,
        "lfo1Rate", "lfo2Rate", "lfo3Rate",
        ParamID::peakGain, ParamID::peakGain2,
        ParamID::subVol,
        ParamID::osc1Pan, ParamID::osc2Pan, ParamID::osc3Pan, ParamID::spread,
        ParamID::subH1, ParamID::subH2, ParamID::subH3, ParamID::subH4, ParamID::subH5,
        ParamID::peakQ, ParamID::peakQ2,
        ParamID::subSemi,
        ParamID::osc1Tune, ParamID::osc2Tune, ParamID::pitch, ParamID::osc1WaveMix
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
    static const StringArray s { "FOLD", "TUBE", "HARD CLIP" };   // menu order (FOLD first); BITCRUSH removed in 0.99.5
    return s;
}

float Distortion::shape (int type, float x, float drive, int ch)
{
    switch (type)
    {
        case 0:
        {
            const float u = x * (1.0f + 16.0f * drive * drive);
            return u >= 0.0f ? std::tanh (u) : 0.8f * std::tanh (u * 0.8f);
        }
        case 1:
            return jlimit (-1.0f, 1.0f, x * (1.0f + 22.0f * drive * drive));
        case 2:
            return std::sin (x * (1.0f + 10.0f * drive) * MathConstants<float>::halfPi);
        case 3:
        {
            const float bits = 16.0f - 13.0f * drive;
            const float levels = std::exp2 (bits - 1.0f);
            const int factor = 4 * (1 + (int) (drive * drive * 40.0f));   // x4: it runs oversampled
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
    const double osr = sr * 4.0;
    const float fc = 400.0f * std::pow (50.0f, jlimit (0.0f, 1.0f, tone));
    const float a = std::exp (-MathConstants<float>::twoPi * jmin (fc, (float) sr * 0.45f) / (float) osr);
    const float dcK = (float) std::exp (-MathConstants<double>::twoPi * 8.0 / osr);          // ~8 Hz DC blocker
    const float envK = 1.0f - (float) std::exp (-1.0 / (0.12 * osr));                         // ~120 ms level followers
    static constexpr int menuToShape[3] = { 2, 0, 1 };
    const int shp = menuToShape[jlimit (0, 2, type)];
    const float preK = 1.0f - (float) std::exp (-MathConstants<double>::twoPi * 7000.0 / osr);
    const float bias = 0.06f * drive;
    const float biasOut = shape (shp, bias, drive, 0);

    float* chans[2] = { L, R };
    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    auto up = os.processSamplesUp (block);
    const int m = (int) up.getNumSamples();
    for (int c = 0; c < 2; ++c)
    {
        float* x = up.getChannelPointer ((size_t) c);
        for (int i = 0; i < m; ++i)
        {
            const float in = x[i];
            // 0.99.5: the shaper hears the signal without its very top (one pole ~7 kHz) and with a slight tube-like
            // lean (bias): noise turns into a warm crackle, a noise + tone mix "gurgles" (the bias is removed after)
            pre[c] += (in - pre[c]) * preK;
            const float y = shape (shp, pre[c] + bias, drive, c) - biasOut;
            const float d = y - dcX[c] + dcK * dcY[c];
            dcX[c] = y; dcY[c] = d;
            lp[c] = (1.0f - a) * d + a * lp[c];
            envIn[c]  += (in * in - envIn[c]) * envK;
            envWet[c] += (lp[c] * lp[c] - envWet[c]) * envK;
            x[i] = in * (1.0f - mix) + lp[c] * mix * agc;
        }
    }
    // auto level: the distorted signal follows the level of what came in (-24 .. +6 dB), smoothly
    const float ein = envIn[0] + envIn[1], ewet = envWet[0] + envWet[1];
    if (ein > 1.0e-9f && ewet > 1.0e-9f)
    {
        const float want = jlimit (0.063f, 2.0f, std::sqrt (ein / ewet));
        agc += (want - agc) * 0.05f;
    }
    os.processSamplesDown (block);
}

// ===========================================================================
// FilterUnit

const StringArray& FilterUnit::typeNames()
{
    static const StringArray s { "LOW PASS 12", "LOW PASS 24", "HIGH PASS", "BAND PASS", "PEAK", "COMB", "VOWEL" };
    return s;
}

void FilterUnit::Svf::set (float fc, float Q, double sampleRate)
{
    const float g = std::tan (MathConstants<float>::pi * jlimit (10.0f, (float) sampleRate * 0.45f, fc) / (float) sampleRate);
    k = 1.0f / jmax (0.3f, Q);
    a1 = 1.0f / (1.0f + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;
}

void FilterUnit::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& c : comb) c.assign ((size_t) (sr / 15.0) + 4, 0.0f);
    reset();
}

void FilterUnit::reset()
{
    for (auto* arr : { s1, s2 }) for (int c = 0; c < 2; ++c) { arr[c].ic1 = arr[c].ic2 = 0.0f; }
    for (auto& f : fm) for (auto& c : f) c.ic1 = c.ic2 = 0.0f;
    for (auto& c : comb) std::fill (c.begin(), c.end(), 0.0f);
    combW = 0; combLp[0] = combLp[1] = 0.0f;
}

float FilterUnit::response (int type, float hz, float fc, float q, double sr, float peakGainDb)
{
    using C = std::complex<double>;
    const double fcl = jlimit (10.0, sr * 0.45, (double) fc);
    auto svf = [] (double f, double fcut, double Q, int out)   // analog SVF prototype
    {
        const C s (0.0, f / fcut);
        const double k = 1.0 / jmax (0.3, Q);
        const C d = s * s + k * s + 1.0;
        switch (out) { case 0: return C (1.0) / d; case 1: return s * s / d; case 2: return k * s / d; case 4: return s / d; default: return (s * s + 1.0) / d; }
    };
    switch (type)
    {
        case LP12:  return (float) std::abs (svf (hz, fcl, q, 0));
        case HP:    return (float) std::abs (svf (hz, fcl, q, 1));
        case BP:    return (float) std::abs (svf (hz, fcl, q, 2));
        case Peak:  { const double a2m1 = std::pow (10.0, peakGainDb / 20.0) - 1.0; return (float) std::abs (1.0 + a2m1 * svf (hz, fcl, jlimit (0.3, 16.0, (double) q), 2)); }
        case LP24:  return (float) (std::abs (svf (hz, fcl, 0.54, 0)) * std::abs (svf (hz, fcl, q * 1.3, 0)));
        case Comb:
        {
            const double d = sr / jmax (15.0, fcl);
            const double fb = jlimit (0.3, 0.97, 0.55 + 0.045 * q);
            const double w = MathConstants<double>::twoPi * hz / sr;
            const C z1 = std::polar (1.0, -w), zd = std::polar (1.0, -w * d);
            const C damp = 0.75 / (1.0 - 0.25 * z1);
            return (float) (std::abs ((1.0 - fb) * 1.6 / (1.0 - fb * damp * zd)));
        }
        default:
        {
            static const float table[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2140, 2950 },
                                               { 450, 800, 2830 }, { 325, 700, 2530 } };
            const float pos = jlimit (0.0f, 3.999f, std::log (jmax (80.0f, fc) / 80.0f) / std::log (100.0f) * 4.0f);
            const int a = (int) pos;
            const float t = pos - (float) a;
            const double gains[3] = { 1.0, 0.6, 0.35 };
            const double fq = 4.0 + 2.0 * q;
            C sum;
            for (int f = 0; f < 3; ++f)
                sum += gains[f] * svf (hz, table[a][f] + (table[a + 1][f] - table[a][f]) * t, fq, 4);
            return (float) (std::abs (sum) / fq);
        }
    }
}

void FilterUnit::process (float* L, float* R, int n, int type, float fc, float q, float peakGainDb)
{
    float* ch[2] = { L, R };
    switch (type)
    {
        case LP12: case HP: case BP:
        {
            for (auto& s : s1) s.set (fc, q, sr);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float lp, bp, hp;
                    s1[c].tick (ch[c][i], lp, bp, hp);
                    ch[c][i] = type == LP12 ? lp : type == HP ? hp : bp * s1[c].k;
                }
            break;
        }
        case Peak:
        {
            // EQ bell: boost or cut one frequency, medium width (Q 1)
            const float a2m1 = Decibels::decibelsToGain (peakGainDb) - 1.0f;
            for (auto& s : s1) s.set (fc, jlimit (0.3f, 16.0f, q), sr);   // q = the PEAK's own Q knob
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float lp, bp, hp;
                    const float x = ch[c][i];
                    s1[c].tick (x, lp, bp, hp);
                    ch[c][i] = x + a2m1 * s1[c].k * bp;
                }
            break;
        }
        case LP24:
        {
            // two stages; the resonance sits on the second one, the first stays gentle
            for (auto& s : s1) s.set (fc, 0.54f, sr);
            for (auto& s : s2) s.set (fc, q * 1.3f, sr);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float lp, bp, hp, lp2;
                    s1[c].tick (ch[c][i], lp, bp, hp);
                    s2[c].tick (lp, lp2, bp, hp);
                    ch[c][i] = lp2;
                }
            break;
        }
        case Comb:
        {
            // feedback comb tuned to the cutoff (which follows the note): metallic, string-like ringing
            const int size = (int) comb[0].size();
            const float d = jlimit (2.0f, (float) size - 2.0f, (float) (sr / jmax (15.0f, fc)));
            const float fb = jlimit (0.3f, 0.97f, 0.55f + 0.045f * q);
            const float damp = 0.25f;
            for (int i = 0; i < n; ++i)
            {
                float rp = (float) combW - d;
                if (rp < 0.0f) rp += (float) size;
                const int i0 = (int) rp;
                const float fr = rp - (float) i0;
                const int i1 = (i0 + 1) % size;
                for (int c = 0; c < 2; ++c)
                {
                    const float del = comb[c][(size_t) i0] + (comb[c][(size_t) i1] - comb[c][(size_t) i0]) * fr;
                    combLp[c] = del + (combLp[c] - del) * damp;
                    const float y = ch[c][i] + combLp[c] * fb;
                    comb[c][(size_t) combW] = y;
                    ch[c][i] = y * (1.0f - fb) * 1.6f;
                }
                if (++combW >= size) combW = 0;
            }
            break;
        }
        default:
        {
            // VOWEL: cutoff morphs A -> E -> I -> O -> U, resonance sharpens the formants
            static const float table[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2140, 2950 },
                                               { 450, 800, 2830 }, { 325, 700, 2530 } };
            const float pos = jlimit (0.0f, 3.999f, std::log (jmax (80.0f, fc) / 80.0f) / std::log (100.0f) * 4.0f);
            const int a = (int) pos;
            const float t = pos - (float) a;
            const float gains[3] = { 1.0f, 0.6f, 0.35f };
            const float fq = 4.0f + 2.0f * q;
            for (int f = 0; f < 3; ++f)
            {
                const float hz = table[a][f] + (table[a + 1][f] - table[a][f]) * t;
                for (auto& s : fm[f]) s.set (hz, fq, sr);
            }
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    float out = 0.0f;
                    for (int f = 0; f < 3; ++f)
                    {
                        float lp, bp, hp;
                        fm[f][c].tick (ch[c][i], lp, bp, hp);
                        out += bp * gains[f];
                    }
                    ch[c][i] = out * 1.0f;
                }
            break;
        }
    }
}

// ===========================================================================
// Delay

void Delay::prepare (double sampleRate)
{
    sr = sampleRate;
    const int n = (int) (sr * 2.6) + 8;
    bufL.assign ((size_t) n, 0.0f);
    bufR.assign ((size_t) n, 0.0f);
    reset();
}

void Delay::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    write = 0; curDelay = -1.0f; lpL = lpR = 0.0f;
}

void Delay::process (float* L, float* R, int n, float timeMs, float feedback, float tone, float mix, bool ping)
{
    const int size = (int) bufL.size();
    if (size < 8) return;
    const float target = jlimit (2.0f, (float) (size - 4), timeMs * 0.001f * (float) sr);
    if (curDelay < 0.0f) curDelay = target;
    const float glide = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));   // smooth time changes (tape-like)
    const float fc = 600.0f * std::pow (25.0f, jlimit (0.0f, 1.0f, tone));
    const float a = std::exp (-MathConstants<float>::twoPi * jmin (fc, (float) sr * 0.45f) / (float) sr);
    const float fb = jlimit (0.0f, 0.95f, feedback);

    auto read = [size] (const std::vector<float>& b, float pos)
    {
        const int i0 = (int) pos;
        const float f = pos - (float) i0;
        const float x0 = b[(size_t) ((i0 % size + size) % size)];
        const float x1 = b[(size_t) (((i0 + 1) % size + size) % size)];
        return x0 + (x1 - x0) * f;
    };

    for (int i = 0; i < n; ++i)
    {
        curDelay += (target - curDelay) * glide;
        const float rp = (float) write - curDelay;
        float dl = read (bufL, rp < 0.0f ? rp + (float) size : rp);
        float dr = read (bufR, rp < 0.0f ? rp + (float) size : rp);
        lpL = (1.0f - a) * dl + a * lpL;
        lpR = (1.0f - a) * dr + a * lpR;
        dl = lpL; dr = lpR;

        const float inL = L[i], inR = R[i];
        if (ping)
        {
            bufL[(size_t) write] = (inL + inR) * 0.5f + dr * fb;
            bufR[(size_t) write] = dl * fb;
        }
        else
        {
            bufL[(size_t) write] = inL + dl * fb;
            bufR[(size_t) write] = inR + dr * fb;
        }
        if (++write >= size) write = 0;

        L[i] = inL * (1.0f - 0.5f * mix) + dl * mix;
        R[i] = inR * (1.0f - 0.5f * mix) + dr * mix;
    }
}

// ===========================================================================
// Static helpers

const char* GranulumProcessor::modTargetId (int t)
{
    static const char* const harmIds[16] = { "harm1", "harm2", "harm3", "harm4", "harm5", "harm6", "harm7", "harm8",
                                             "harm9", "harm10", "harm11", "harm12", "harm13", "harm14", "harm15", "harm16" };
    if (isHarmTarget (t)) return harmIds[t - kHarmTarget0];
    return (t >= 0 && t < kNumModTargets) ? kModTargetIds[t] : "";
}

int GranulumProcessor::modIndexOf (const String& id)
{
    if (id.startsWith ("harm"))
    {
        const int k = id.substring (4).getIntValue();
        return k >= 1 && k <= 16 ? kHarmTarget0 + k - 1 : -1;
    }
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
    // slow -> fast; each dotted (D) and triplet (T) sits right next to its straight note value, like Serum
    static const StringArray s { "4/1", "2/1", "1/1", "1/2D", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T",
                                 "1/16D", "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    return s;
}

double GranulumProcessor::divisionBeats (int i)
{
    static const double b[] = { 16.0, 8.0, 4.0, 3.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0,
                                0.375, 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };
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

LfoShape GranulumProcessor::lfoPreset (int i)
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
        ps.push_back (std::make_unique<B> (ParameterID { o == 1 ? ParamID::osc1Grain : ParamID::osc2Grain, 1 }, n + "Granular", false));
        if (o == 1)
        {
            ps.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterID { ParamID::osc1Mode, 1 }, "OSC 1 Mode", StringArray { "ADDITIVE", "MIX", "WAVE" }, 0));
            ps.push_back (std::make_unique<P> (ParameterID { ParamID::osc1WaveMix, 1 }, "OSC 1 Wave Mix", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
            ps.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterID { ParamID::osc1Wave, 1 }, "OSC 1 Wave", StringArray { "SINE", "SAW", "SQUARE", "TRIANGLE" }, 1));
        }
        ps.push_back (std::make_unique<P> (ParameterID { o == 1 ? ParamID::osc1Tune : ParamID::osc2Tune, 1 }, n + "Tune", NormalisableRange<float> (-48.0f, 48.0f, 0.0f), 0.0f,
                                           A().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; })));
    }
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::chord, 1 }, "Chord", chordNames(), 0));

    // OSC 3: built-in noises, no key tracking
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::osc3On, 1 }, "Osc 3 On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::osc3Vol, 1 }, "Osc 3 Volume", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::osc3Type, 1 }, "Osc 3 Noise", noiseNames(), 0));
    // OSC 3 PITCH (the old COLOR id is kept so projects load): -1..1 = -12..+12 semitones
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::osc3Color, 1 }, "Osc 3 Pitch", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { const float st = v * 12.0f;
                                           return std::abs (st) < 0.05f ? String ("0 st") : (st > 0 ? "+" : "") + String (st, 1) + " st"; })));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::osc3Grain, 1 }, "Osc 3 Granular", false));

    // LOOP (OSC 2 / OSC 3): which part of the source the grains use
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::loopMode, 1 }, "Loop Mode", StringArray { "OFF", "LOOP", "PING-PONG", "ONE-SHOT" }, 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::loopStart, 1 }, "Loop Start", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::loopEnd, 1 },   "Loop End",   NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    // GRANULAR
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::grainOn, 1 }, "Granular On (legacy)", true));
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
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::chime, 1 },   "Chime",   NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    // FILTER (key-tracked from C3 under the hood)
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::filterOn, 1 }, "Filter On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::cutoff, 1 }, "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 2000.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return hzWithNote (v); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reso, 1 }, "Resonance", skewed (0.5f, 10.0f, 1.5f), 0.707f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, ""); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::ftype, 1 }, "Filter Type", FilterUnit::typeNames(), 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::peakGain, 1 }, "Filter Peak Gain", NormalisableRange<float> (-36.0f, 36.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));

    // PEAK width (Q): 1.0 = the fixed width used before 0.99.1
    auto qFmt = A().withStringFromValueFunction ([] (float v, int) { return "Q " + fmt (v, v < 10.0f ? 1 : 0, ""); });
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::peakQ, 1 },  "Filter Peak Q",   skewed (0.3f, 16.0f, 1.5f), 1.0f, qFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::peakQ2, 1 }, "Filter 2 Peak Q", skewed (0.3f, 16.0f, 1.5f), 1.0f, qFmt));

    // FILTER 2 (same engine, own settings)
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::filter2On, 1 }, "Filter 2 On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::cutoff2, 1 }, "Cutoff 2", skewed (20.0f, 20000.0f, 1000.0f), 800.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return hzWithNote (v); })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::reso2, 1 }, "Resonance 2", skewed (0.5f, 10.0f, 1.5f), 0.707f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 2, ""); })));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::ftype2, 1 }, "Filter 2 Type", FilterUnit::typeNames(), 6));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::peakGain2, 1 }, "Filter 2 Peak Gain", NormalisableRange<float> (-36.0f, 36.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; })));

    // DISTORTION
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::distOn, 1 }, "Distortion On", false));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::distType, 1 }, "Distortion Type", Distortion::typeNames(), 0));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::distDrive, 1 }, "Distortion Drive", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::distTone, 1 },  "Distortion Tone",  NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::distMix, 1 },   "Distortion Mix",   NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));

    // DELAY
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::delayOn, 1 }, "Delay On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::delayTime, 1 }, "Delay Time", skewed (5.0f, 2000.0f, 300.0f), 375.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 1000.0f ? fmt (v, 0, " ms") : fmt (v / 1000.0f, 2, " s"); })));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::delaySync, 1 }, "Delay Sync", true));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::delayDiv, 1 }, "Delay Division", divisionNames(), divisionNames().indexOf ("1/8D")));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::delayFb, 1 },   "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.4f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::delayTone, 1 }, "Delay Tone",     NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::delayMix, 1 },  "Delay Mix",      NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::delayPing, 1 }, "Delay Ping-Pong", true));

    // REVERB
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::revOn, 1 }, "Reverb On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::revMix, 1 },  "Reverb Mix",  NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::revSize, 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::revLowCut, 1 }, "Reverb Low Cut", skewed (20.0f, 1000.0f, 150.0f), 150.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 22.0f ? String ("OFF") : fmt (v, 0, " Hz"); })));

    // routing: which oscillators go through each block
    for (auto* block : kRouteBlocks)
        for (int o = 0; o < kNumOscs; ++o)
            ps.push_back (std::make_unique<B> (ParameterID { ParamID::route (block, o), 1 },
                                               String (block) + " gets Osc " + String (o + 1), true));
    // SUB routing (shown as digit "2"): off by default, the sub stays dry until the owner lights it
    for (auto* block : kRouteBlocks)
        ps.push_back (std::make_unique<B> (ParameterID { ParamID::route (block, 3), 1 }, String (block) + " gets Sub", false));

    // AMP envelope
    auto msFmt = A().withStringFromValueFunction ([] (float v, int) { return v < 0.05f ? String ("0 ms") : v < 1000.0f ? fmt (v, v < 10.0f ? 1 : 0, " ms") : fmt (v / 1000.0f, 2, " s"); });
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::ampA, 1 }, "Amp Attack",  skewed (0.0f, 10000.0f, 300.0f), 0.0f, msFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::ampD, 1 }, "Amp Decay",   skewed (1.0f, 10000.0f, 500.0f), 300.0f, msFmt));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::ampS, 1 }, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::ampOn, 1 }, "Amp Envelope On", true));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::ampR, 1 }, "Amp Release", skewed (5.0f, 10000.0f, 600.0f), 300.0f, msFmt));

    // VOICE / global
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::mono, 1 }, "Mono", true));   // 0.99.5: INIT (and a fresh plugin) starts in MONO
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::porta, 1 }, "Portamento", skewed (0.0f, 3000.0f, 200.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? String ("OFF") : v < 1000.0f ? fmt (v, 0, " ms") : fmt (v / 1000.0f, 2, " s"); })));
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::clipOn, 1 }, "Output Clipper", true));
    // PAN per oscillator (the sub stays centred) and SPREAD = stereo width of the grain cloud
    {
        auto panFmt = A().withStringFromValueFunction ([] (float v, int) { const int n = roundToInt (v * 100.0f); return n == 0 ? String ("C") : (n < 0 ? "L " : "R ") + String (std::abs (n)); })
                         .withValueFromStringFunction ([] (const String& t) { const auto u = t.trim().toUpperCase(); if (u.startsWith ("C")) return 0.0f;
                                                                               const float n = u.retainCharacters ("0123456789.").getFloatValue() / 100.0f; return u.startsWith ("L") ? -n : n; });
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::osc1Pan, 1 }, "OSC 1 Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, panFmt));
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::osc2Pan, 1 }, "OSC 2 Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, panFmt));
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::osc3Pan, 1 }, "OSC 3 Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, panFmt));
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::spread, 1 }, "Grain Spread", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    }
    // OSC 4 SUB
    ps.push_back (std::make_unique<B> (ParameterID { ParamID::subOn, 1 },  "Sub On", false));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subVol, 1 }, "Sub Volume", NormalisableRange<float> (0.0f, 1.0f), 0.7f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subOct, 1 }, "Sub Octave", NormalisableRange<float> (-3.0f, 1.0f, 1.0f), -1.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " oct"; })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subSemi, 1 }, "Sub Semitones", NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { const int n = roundToInt (v); return (n > 0 ? "+" : "") + String (n) + " st"; })));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subH1, 1 }, "Sub Harmonic 1", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subH2, 1 }, "Sub Harmonic 2", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subH3, 1 }, "Sub Harmonic 3", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subH4, 1 }, "Sub Harmonic 4", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::subH5, 1 }, "Sub Harmonic 5", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::pitch, 1 }, "Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.0f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; })));   // 0.99.3: +-2 oct, smooth
    ps.push_back (std::make_unique<P> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f,
                                       A().withStringFromValueFunction ([] (float v, int) { return fmt (v, 1, " dB"); })));

    // LFOs
    for (int l = 0; l < kNumLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfo (l, "Rate"), 1 }, n + "Rate", skewed (0.02f, 30.0f, 2.0f), 1.0f,
                                           A().withStringFromValueFunction ([] (float v, int) { return fmt (v, v < 1.0f ? 2 : 1, " Hz"); })));
        ps.push_back (std::make_unique<B> (ParameterID { ParamID::lfo (l, "Sync"), 1 }, n + "Sync", true));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Div"), 1 }, n + "Division", divisionNames(), divisionNames().indexOf ("1/4")));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Mode"), 1 }, n + "Mode", StringArray { "FREE", "RETRIG", "ENV" }, 0));
        ps.push_back (std::make_unique<P> (ParameterID { ParamID::lfo (l, "Rise"), 1 }, n + "Rise", skewed (0.0f, 8.0f, 1.5f), 0.0f, secFmt));
        ps.push_back (std::make_unique<B> (ParameterID { ParamID::lfo (l, "On"), 1 }, n + "On", true));
        ps.push_back (std::make_unique<B> (ParameterID { ParamID::lfo (l, "Grains"), 1 }, n + "Into Grains", false));
    }

    return { ps.begin(), ps.end() };
}

GranulumProcessor::GranulumProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "GRANULUM", createLayout())
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
    pOsc1Mode  = apvts.getRawParameterValue (ParamID::osc1Mode);
    pOsc2On    = apvts.getRawParameterValue (ParamID::osc2On);
    pOsc1Grain = apvts.getRawParameterValue (ParamID::osc1Grain);
    pOsc2Grain = apvts.getRawParameterValue (ParamID::osc2Grain);
    pChord     = apvts.getRawParameterValue (ParamID::chord);
    pGrainOn   = apvts.getRawParameterValue (ParamID::grainOn);
    pFilterOn  = apvts.getRawParameterValue (ParamID::filterOn);
    pFtype     = apvts.getRawParameterValue (ParamID::ftype);
    pFilter2On = apvts.getRawParameterValue (ParamID::filter2On);
    pFtype2    = apvts.getRawParameterValue (ParamID::ftype2);
    pDistOn    = apvts.getRawParameterValue (ParamID::distOn);
    pDistType  = apvts.getRawParameterValue (ParamID::distType);
    pRevOn     = apvts.getRawParameterValue (ParamID::revOn);
    pOsc3On    = apvts.getRawParameterValue (ParamID::osc3On);
    pOsc3Type  = apvts.getRawParameterValue (ParamID::osc3Type);
    pOsc3Grain = apvts.getRawParameterValue (ParamID::osc3Grain);
    pDelayOn   = apvts.getRawParameterValue (ParamID::delayOn);
    pDelaySync = apvts.getRawParameterValue (ParamID::delaySync);
    pDelayDiv  = apvts.getRawParameterValue (ParamID::delayDiv);
    pDelayPing = apvts.getRawParameterValue (ParamID::delayPing);
    for (int b = 0; b < kNumRouteBlocks; ++b)
        for (int o = 0; o < kNumOscs; ++o)
            pRoute[(size_t) b][(size_t) o] = apvts.getRawParameterValue (ParamID::route (kRouteBlocks[b], o));
    for (int b = 0; b < kNumRouteBlocks; ++b)
        pRouteSub[(size_t) b] = apvts.getRawParameterValue (ParamID::route (kRouteBlocks[b], 3));
    pMono      = apvts.getRawParameterValue (ParamID::mono);
    pAmpA      = apvts.getRawParameterValue (ParamID::ampA);
    pAmpD      = apvts.getRawParameterValue (ParamID::ampD);
    pAmpS      = apvts.getRawParameterValue (ParamID::ampS);
    pAmpR      = apvts.getRawParameterValue (ParamID::ampR);
    pAmpOn     = apvts.getRawParameterValue (ParamID::ampOn);
    for (int l = 0; l < kNumLfos; ++l)
    {
        pLfoSync[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Sync"));
        pLfoDiv[(size_t) l]  = apvts.getRawParameterValue (ParamID::lfo (l, "Div"));
        pLfoMode[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Mode"));
        pLfoRise[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Rise"));
        pLfoOn[(size_t) l]   = apvts.getRawParameterValue (ParamID::lfo (l, "On"));
        pLfoGrains[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Grains"));
    }

    pClipOn    = apvts.getRawParameterValue (ParamID::clipOn);
    pSubOn     = apvts.getRawParameterValue (ParamID::subOn);
    pSubOct    = apvts.getRawParameterValue (ParamID::subOct);
    pSubH      = { apvts.getRawParameterValue (ParamID::subH1), apvts.getRawParameterValue (ParamID::subH2), apvts.getRawParameterValue (ParamID::subH3),
                   apvts.getRawParameterValue (ParamID::subH4), apvts.getRawParameterValue (ParamID::subH5) };
    pLoopMode  = apvts.getRawParameterValue (ParamID::loopMode);
    pLoopStart = apvts.getRawParameterValue (ParamID::loopStart);
    pLoopEnd   = apvts.getRawParameterValue (ParamID::loopEnd);
    loadNoises();
    harmonics = {};
    harmonics[0] = 1.0f;
    for (int k = 0; k < granulum::kNumHarmonics; ++k) harmNow[(size_t) k].store (harmonics[(size_t) k]);
    for (int l = 0; l < kNumLfos; ++l) setLfoPoints (l, lfoPreset (1));   // triangle

    // factory sounds ship inside the plugin and are written to Documents/GRAIN FORM/Samples/Atmos once
    installFactorySamples();

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
    engineAddClean.prepare (sampleRate);
    engineWave.prepare (sampleRate);
    builtWave = -1;   // mips depend on the sample rate
    engineSmp.prepare (sampleRate);
    engineNoise.prepare (sampleRate);
    engineSub.prepare (sampleRate);

    dsp::ProcessSpec spec { sampleRate, (uint32) jmax (kChunk, samplesPerBlock), 2 };
    for (auto* cp : { &chains[0], &chains[1], &chains[2], &subChain, &distBusChain })
    {
        auto& c = *cp;
        c.filter.prepare (sampleRate);
        c.filter2.prepare (sampleRate);
        c.reverb.prepare (spec); c.reverb.reset();
        c.revCut.prepare (spec); c.revCut.reset();
        c.revCut.setType (dsp::StateVariableTPTFilterType::highpass);
        c.revCut.setResonance (0.6f);
        c.distortion.prepare (sampleRate);
        c.delay.prepare (sampleRate);
        c.quietSeconds = 1.0e9;
    }

    outGain.reset (sampleRate, 0.05);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (apvts.getRawParameterValue (ParamID::output)->load()));

    const int n = jmax (kChunk, samplesPerBlock);
    for (auto* b : { &addBuf, &smpBuf, &noiseBuf, &work, &wetBuf, &subBuf, &waveBuf, &distBuf }) b->setSize (2, n, false, true, true);
    colorLp[0] = colorLp[1] = colorHp[0] = colorHp[1] = 0.0f;

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
    for (int l = 0; l < kNumLfos; ++l) setLfoPoints (l, lfoPreset (1));
    setGrainMask (0xffffu);
    clearSample();            // 0.99.5: INIT also forgets the audio file of OSC 3
    presetName = "INIT";
    presetSub = {};
    soundReset.store (true);
}

// ===========================================================================
// LFO shapes (2..16 points)

LfoShape GranulumProcessor::getLfoPoints (int l) const
{
    if (l < 0 || l >= kNumLfos) return lfoPreset (1);
    const ScopedLock sl (lfoLock);
    return lfoPoints[(size_t) l];
}

void GranulumProcessor::setLfoPoints (int l, LfoShape pts)
{
    if (pts.size() < 2) pts = lfoPreset (1);
    if (pts.size() > (size_t) kMaxLfoPoints) pts.resize ((size_t) kMaxLfoPoints);
    const int n = (int) pts.size();
    pts[0].x = 0.0f;
    pts[(size_t) n - 1].x = 1.0f;
    for (int i = 1; i < n - 1; ++i) pts[(size_t) i].x = jlimit (pts[(size_t) i - 1].x, 1.0f, pts[(size_t) i].x);
    for (int i = n - 2; i > 0; --i) pts[(size_t) i].x = jmin (pts[(size_t) i].x, pts[(size_t) i + 1].x);
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
    if (target < 0 || (target >= kNumModTargets && ! isHarmTarget (target)) || lfo < 0 || lfo >= kNumLfos) return;
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

void GranulumProcessor::removeLfoEverywhere (int lfo)
{
    for (auto& m : mods)
        if (m.target.load() >= 0 && m.lfo.load() == lfo) m.target.store (-1);
    ++modVersion;
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
    harmOff.fill (0.0f);
    for (auto& m : mods)
    {
        const int t = m.target.load (std::memory_order_acquire);
        const bool harm = isHarmTarget (t);
        if (t < 0 || (t >= kNumModTargets && ! harm)) continue;
        const int l = jlimit (0, kNumLfos - 1, m.lfo.load (std::memory_order_relaxed));
        if (pLfoOn[(size_t) l]->load() < 0.5f) continue;     // LFO switched off: its routings stay, but do nothing
        const float d = m.depth.load (std::memory_order_relaxed) * lfoValue[(size_t) l];
        if (harm) harmOff[(size_t) (t - kHarmTarget0)] += d;
        else      off[t] += d;
    }
    for (int k = 0; k < granulum::kNumHarmonics; ++k)
        harmLive[(size_t) k].store (jlimit (0.0f, 1.0f, harmNow[(size_t) k].load (std::memory_order_relaxed) + harmOff[(size_t) k]), std::memory_order_relaxed);
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
        lfoRateNow[L] = rate;

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

granulum::Params GranulumProcessor::voiceParams (const float* v, int osc) const
{
    granulum::Params p;
    p.bloom    = v[mtBloom];
    p.smear    = v[mtSmear];
    p.scatter  = v[mtScatter];
    p.shimmer  = v[mtShimmer];
    p.drift    = v[mtDrift];
    p.spread   = v[mtSpread];
    p.chime    = v[mtChime];
    p.size     = v[mtSize];
    p.speed    = v[mtSpeed];
    p.position = v[mtPosition];
    p.density  = v[mtDensity];
    p.mix      = v[mtGmix];
    p.evolve   = 0.0f;   // EVOLVE knob removed
    p.mono     = pMono->load() > 0.5f;
    p.portaMs  = v[mtPorta];
    if (pAmpOn->load() > 0.5f)
    {
        p.attackMs  = pAmpA->load();
        p.decayMs   = pAmpD->load();
        p.sustain   = pAmpS->load();
        p.releaseMs = pAmpR->load();
    }
    else
    {
        // ADSR switched off: a plain gate (instant on, full level, 10 ms tail)
        p.attackMs = 0.0f; p.decayMs = 1.0f; p.sustain = 1.0f; p.releaseMs = 10.0f;
    }

    const bool grainBlock = true;   // GRANULAR block has no on/off any more: each OSC's GRAIN button decides
    if (osc == 0)
    {
        p.transpose = v[mtOsc1Oct] * 12.0f + v[mtOsc1Semi] + v[mtOsc1Tune] + v[mtPitch];
        p.grainOn = grainBlock && pOsc1Grain->load() > 0.5f;
        const auto iv = chordIntervals ((int) pChord->load());
        p.chordSize = (int) iv.size();
        for (size_t i = 0; i < iv.size() && i < p.chord.size(); ++i) p.chord[i] = iv[i];
    }
    if (osc > 0)
    {
        const float a = pLoopStart->load(), b = pLoopEnd->load();
        p.loopMode = (int) pLoopMode->load();
        p.loopA = jmin (a, b);
        p.loopB = jmax (a, b);
    }
    if (osc == 1)
    {
        p.transpose = v[mtOsc2Oct] * 12.0f + v[mtOsc2Semi] + v[mtOsc2Tune] + v[mtPitch];
        p.grainOn = grainBlock && pOsc2Grain->load() > 0.5f;
    }
    else if (osc == 2)
    {
        p.keyTrack = false;        // noise sounds the same on every note
        p.randomStart = true;      // stacked voices don't phase against each other
        p.portaMs = 0.0f;
        p.transpose = v[mtOsc3Color] * 12.0f + 3.0f + v[mtPitch];   // OSC 4 NOISE PITCH + master PITCH; 0.99.5: all noises sit 3 semitones higher
        p.grainOn = grainBlock && pOsc3Grain->load() > 0.5f;
    }
    return p;
}

granulum::Params GranulumProcessor::subParams (const float* v) const
{
    // OSC 4 SUB: same notes, envelope and glide as the synth; root note only (no chord), never granulated
    auto p = voiceParams (v, 0);
    p.grainOn = false;
    p.chordSize = 1;
    p.chord = { 0, 0, 0, 0, 0 };
    p.chime = 0.0f;
    p.transpose = std::round (pSubOct->load()) * 12.0f + v[mtSubSemi] + v[mtPitch];   // OCT + SEMI (SEMI glides under an LFO)
    // the 3 sticks are layers: their level (incl. LFO) moves at play time
    for (int k = 0; k < kSubHarms; ++k) { p.harmGain[(size_t) k] = jlimit (0.0f, 1.0f, v[mtSubH1 + k]); p.harmGainPrev[(size_t) k] = subGainPrev[(size_t) k]; }
    return p;
}

void GranulumProcessor::noteOn (int note, float velocity)
{
    if (heldNotes == 0) sinceTrigger = 0.0;
    // RETRIG / ENV LFOs start again on EVERY new note (0.99.2): before, a legato note in MONO (key pressed while the
    // previous one was still held) kept the old LFO run, so an ENV LFO had already finished and did nothing on it
    for (int l = 0; l < kNumLfos; ++l)
        if ((int) pLfoMode[(size_t) l]->load() >= 1)
        {
            lfoPhase[(size_t) l] = 0.0;
            lfoDone[(size_t) l] = false;
        }
    ++heldNotes;
    engineAdd.noteOn   (note, velocity, voiceParams (lastV.data(), 0));
    engineAddClean.noteOn (note, velocity, voiceParams (lastV.data(), 0));
    engineWave.noteOn (note, velocity, voiceParams (lastV.data(), 0));
    engineSmp.noteOn   (note, velocity, voiceParams (lastV.data(), 1));
    engineNoise.noteOn (note, velocity, voiceParams (lastV.data(), 2));
    engineSub.noteOn (note, velocity, subParams (lastV.data()));
}

void GranulumProcessor::noteOff (int note)
{
    heldNotes = jmax (0, heldNotes - 1);
    engineAdd.noteOff   (note, voiceParams (lastV.data(), 0));
    engineAddClean.noteOff (note, voiceParams (lastV.data(), 0));
    engineWave.noteOff (note, voiceParams (lastV.data(), 0));
    engineSmp.noteOff   (note, voiceParams (lastV.data(), 1));
    engineNoise.noteOff (note, voiceParams (lastV.data(), 2));
    engineSub.noteOff (note, subParams (lastV.data()));
}

void GranulumProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    addSlot.swapOnAudioThread();
    addCleanSlot.swapOnAudioThread();
    waveSlot.swapOnAudioThread();
    smpSlot.swapOnAudioThread();
    noiseSlot.swapOnAudioThread();
    subSlot.swapOnAudioThread();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // 0.99.2: after a preset change the old notes, grains, delay and reverb tails must not ring on with the new settings
    const bool cut = soundReset.exchange (false);
    if (cut)
    {
        heldNotes = 0;
        for (auto* e : { &engineAdd, &engineAddClean, &engineWave, &engineSmp, &engineNoise, &engineSub }) e->reset();
        for (auto* cp : { &chains[0], &chains[1], &chains[2], &subChain, &distBusChain })
        {
            cp->filter.reset(); cp->filter2.reset(); cp->distortion.reset(); cp->delay.reset();
            cp->reverb.reset(); cp->revCut.reset();
            cp->quietSeconds = 1.0e9;
        }
        colorLp[0] = colorLp[1] = colorHp[0] = colorHp[1] = 0.0f;
        sinceTrigger = 1.0e9;
    }

    if (work.getNumSamples() < numSamples)
        for (auto* b : { &addBuf, &smpBuf, &noiseBuf, &work, &wetBuf, &subBuf, &waveBuf, &distBuf }) b->setSize (2, numSamples, false, false, true);
    for (auto* b : { &addBuf, &smpBuf, &noiseBuf, &work, &subBuf }) b->clear();

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
            engineAdd.allNotesOff(); engineAddClean.allNotesOff(); engineWave.allNotesOff(); engineSmp.allNotesOff(); engineNoise.allNotesOff(); engineSub.allNotesOff();
        }
    }
    renderRange (pos, numSamples);
    midi.clear();

    if (cut)
    {
        // the old sound stops: glide from its last sample to zero in ~3 ms instead of a hard step (no click)
        const int n = jmin (numSamples, 128);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = work.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) n;
                d[i] += lastOut[ch] * (1.0f - t) * (1.0f - t);
            }
        }
    }
    if (numSamples > 0)
        for (int ch = 0; ch < 2; ++ch) lastOut[ch] = work.getSample (ch, numSamples - 1);

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
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;

    const float gWant[kNumOscs] = { pOsc1On->load() > 0.5f ? v[mtOsc1Vol] : 0.0f,
                                    pOsc2On->load() > 0.5f ? v[mtOsc2Vol] : 0.0f,
                                    pOsc3On->load() > 0.5f ? v[mtOsc3Vol] * 1.4f : 0.0f };
    float* lastG[kNumOscs] = { &lastG1, &lastG2, &lastG3 };
    // 0.99.4: a volume never moves faster than 0 -> 1 in ~4 ms. A RETRIG LFO restarting on a new note (or a hard LFO
    // shape) used to swing the level inside one 32-sample chunk (0.7 ms) = a click. With no voice sounding the
    // level simply jumps (nothing to hear), so the first note does not start with a stale level ramp.
    // two gentle smoothing stages (~1.5 ms each): the level glides with no corners, so a jump never clicks
    const float sm = 1.0f - std::exp (-(float) len / (float) (0.0015 * sr));
    const bool silentNow[kNumOscs] = { ! engineAdd.anyActive() && ! engineAddClean.anyActive() && ! engineWave.anyActive(), ! engineSmp.anyActive(), ! engineNoise.anyActive() };
    float g[kNumOscs];
    for (int o = 0; o < kNumOscs; ++o)
    {
        auto& st = volSmooth[(size_t) o];
        if (silentNow[o]) { st[0] = st[1] = *lastG[o] = gWant[o]; }
        st[0] += (gWant[o] - st[0]) * sm;
        st[1] += (st[0] - st[1]) * sm;
        g[o] = st[1];
    }
    AudioBuffer<float>* bufs[kNumOscs] = { &addBuf, &smpBuf, &noiseBuf };
    granulum::GrainEngine* engines[kNumOscs] = { &engineAdd, &engineSmp, &engineNoise };
    SourceSlot* slots[kNumOscs] = { &addSlot, &smpSlot, &noiseSlot };

    // ---- the three oscillators, each into its own buffer ----
    const int volTarget[kNumOscs] = { mtOsc1Vol, mtOsc2Vol, mtOsc3Vol };
    for (int o = 0; o < kNumOscs; ++o)
    {
        float* L = bufs[o]->getWritePointer (0) + from;
        float* R = bufs[o]->getWritePointer (1) + from;
        bool audible = g[o] > 0.0001f || *lastG[o] > 0.0001f;
        auto vp = voiceParams (v, o);
        if (o == 0)
            for (int k = 0; k < granulum::kNumHarmonics; ++k)
            {
                // sticks with an LFO: level now (stick + LFO) and at the last chunk, ramped inside the engine
                vp.harmGain[(size_t) k] = harmLive[(size_t) k].load (std::memory_order_relaxed);
                vp.harmGainPrev[(size_t) k] = harmGainPrev[(size_t) k];
            }
        const bool oscOn = (o == 0 ? pOsc1On : o == 1 ? pOsc2On : pOsc3On)->load() > 0.5f;
        auto ramp = [&] (float* l, float* r)
        {
            for (int i = 0; i < len; ++i)
            {
                const float gg = *lastG[o] + (g[o] - *lastG[o]) * (float) (i + 1) / (float) len;
                l[i] *= gg; r[i] *= gg;
            }
        };

        // INTO GRAINS: LFOs on this OSC's volume whose rhythm the grains should cut up
        // (gate on the OSC switch, not on g[o]: a deep LFO pulls g[o] to 0 for a chunk, and switching
        //  between in-grain volume and the plain ramp at that moment was the click at fast LFO rates)
        if (vp.grainOn && oscOn)
        {
            float base = modParams[(size_t) volTarget[o]]->getValue();
            for (auto& m : mods)
            {
                if (m.target.load (std::memory_order_acquire) != volTarget[o]) continue;
                const int l = jlimit (0, kNumLfos - 1, m.lfo.load (std::memory_order_relaxed));
                if (pLfoOn[(size_t) l]->load() < 0.5f) continue;
                const float d = m.depth.load (std::memory_order_relaxed);
                if (pLfoGrains[(size_t) l]->load() > 0.5f && vp.glfoCount < 3)
                {
                    auto& G = vp.glfo[(size_t) vp.glfoCount++];
                    G.table = lfoTables[(size_t) l][(size_t) lfoTableIndex[(size_t) l].load (std::memory_order_acquire)].data();
                    G.size = kLfoTable;
                    G.phase = lfoPhase[(size_t) l];
                    G.inc = lfoRateNow[(size_t) l] / sr;
                    G.depth = d;
                    G.rise = lfoRiseOut[(size_t) l].load (std::memory_order_relaxed);
                }
                else base += d * lfoValue[(size_t) l];
            }
            if (vp.glfoCount > 0)
            {
                vp.volInGrains = true;
                vp.volBase = base;
                vp.volScale = o == 2 ? 1.4f : 1.0f;
                vp.level = g[o];
                vp.levelPrev = *lastG[o];
                audible = true;
            }
        }

        // 0.99.5 OSC 1: ADDITIVE (sticks), WAVE (one full waveform) or MIX of the two (WAVE MIX knob)
        const int o1mode = o == 0 ? (int) pOsc1Mode->load() : 0;
        const float waveAmt = o1mode == 2 ? 1.0f : o1mode == 1 ? jlimit (0.0f, 1.0f, v[mtOsc1WaveMix]) : 0.0f;
        const float addAmt = 1.0f - waveAmt;
        // a part of OSC 1 that is fully off (ADD in WAVE mode, WAVE in ADD mode) is not computed at all
        const bool addIdle  = o == 0 && addAmt <= 0.0f && lastAddAmt <= 0.0f;
        const bool waveIdle = o == 0 && waveAmt <= 0.0f && lastAddAmt >= 1.0f;
        if (o == 0)
        {
            if (addIdle)
            {
                if (engineAdd.anyActive()) engineAdd.reset();
                if (engineAddClean.anyActive()) engineAddClean.reset();
            }
            else
            {
                // harmonics switched off in the GRAIN row: clean and mono, never granulated (normal volume)
                auto clean = vp;
                clean.grainOn = false;
                clean.volInGrains = false;
                engineAddClean.render (L, R, len, clean, audible ? addCleanSlot.active.get() : nullptr);
                if (vp.volInGrains) ramp (L, R);
            }
        }
        if (! addIdle) engines[o]->render (L, R, len, vp, audible ? slots[o]->active.get() : nullptr);
        if (o == 0 && waveIdle)
        {
            if (engineWave.anyActive()) engineWave.reset();
            lastAddAmt = addAmt;
        }
        else if (o == 0)
        {
            // additive part at (1 - mix), the full wave at mix: rendered on its own and added
            const float a0 = lastAddAmt, w0 = 1.0f - lastAddAmt;
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) (i + 1) / (float) len;
                const float ga = a0 + (addAmt - a0) * t;
                L[i] *= ga; R[i] *= ga;
            }
            float* WL = waveBuf.getWritePointer (0);
            float* WR = waveBuf.getWritePointer (1);
            FloatVectorOperations::clear (WL, len);
            FloatVectorOperations::clear (WR, len);
            engineWave.render (WL, WR, len, vp, audible ? waveSlot.active.get() : nullptr);
            for (int i = 0; i < len; ++i)
            {
                const float t = (float) (i + 1) / (float) len;
                const float gw = w0 + (waveAmt - w0) * t;
                L[i] += WL[i] * gw; R[i] += WR[i] * gw;
            }
            lastAddAmt = addAmt;
        }
        if (o == 0) harmGainPrev = vp.harmGain;
        if (! vp.volInGrains) ramp (L, R);
        *lastG[o] = g[o];

        // PAN (balance, equal power, smoothed over the chunk; centre = untouched)
        {
            const float pan = jlimit (-1.0f, 1.0f, v[mtOsc1Pan + o]);
            const float p0 = lastPan[o];
            if (std::abs (pan) > 1.0e-4f || std::abs (p0) > 1.0e-4f)
            {
                auto gains = [] (float pp, float& gl, float& gr)
                {
                    const float a = (pp + 1.0f) * 0.25f * MathConstants<float>::pi;
                    gl = jmin (1.0f, std::cos (a) * MathConstants<float>::sqrt2);
                    gr = jmin (1.0f, std::sin (a) * MathConstants<float>::sqrt2);
                };
                float l0, r0, l1, r1; gains (p0, l0, r0); gains (pan, l1, r1);
                for (int i = 0; i < len; ++i)
                {
                    const float t = (float) (i + 1) / (float) len;
                    L[i] *= l0 + (l1 - l0) * t;
                    R[i] *= r0 + (r1 - r0) * t;
                }
            }
            lastPan[o] = pan;
        }
    }

    // ---- shared block settings ----
    const float note = engineAdd.lastNote.load (std::memory_order_relaxed);
    const float keyMul = std::exp2 ((note - 60.0f) / 12.0f);
    const float cut  = jlimit (20.0f, (float) (sr * 0.45), v[mtCutoff]  * keyMul);
    const float cut2 = jlimit (20.0f, (float) (sr * 0.45), v[mtCutoff2] * keyMul);
    keyCutoff.store  ((int) pFtype->load()  == FilterUnit::Peak ? v[mtCutoff]  : cut,  std::memory_order_relaxed);
    keyCutoff2.store ((int) pFtype2->load() == FilterUnit::Peak ? v[mtCutoff2] : cut2, std::memory_order_relaxed);
    viewPeak1.store (v[mtPeakGain], std::memory_order_relaxed);
    viewPeak2.store (v[mtPeakGain2], std::memory_order_relaxed);
    viewReso1.store (v[mtReso], std::memory_order_relaxed);
    viewReso2.store (v[mtReso2], std::memory_order_relaxed);
    viewPeakQ1.store (v[mtPeakQ], std::memory_order_relaxed);
    viewPeakQ2.store (v[mtPeakQ2], std::memory_order_relaxed);
    const bool filter2On = pFilter2On->load() > 0.5f;
    const int ft2 = (int) pFtype2->load();
    const bool filterOn = pFilterOn->load() > 0.5f, distOn = pDistOn->load() > 0.5f && v[mtDistMix] > 0.001f;
    const bool delayOn = pDelayOn->load() > 0.5f, revOn = pRevOn->load() > 0.5f;
    const int ft = (int) pFtype->load();
    const float delayMs = pDelaySync->load() > 0.5f ? (float) (divisionBeats ((int) pDelayDiv->load()) * 60000.0 / hostBpm)
                                                    : v[mtDelayTime];
    dsp::Reverb::Parameters rp;
    rp.roomSize = 0.3f + 0.69f * v[mtRevSize];
    rp.damping  = 0.4f;
    rp.wetLevel = v[mtRevMix] * 0.9f;
    rp.dryLevel = 0.0f;                                  // dry is mixed back by hand, so LOW CUT touches the tail only
    const float revDry = 1.0f - v[mtRevMix] * 0.5f;
    const float revCutHz = v[mtRevLowCut];
    rp.width    = 1.0f;

    float* outL = work.getWritePointer (0) + from;
    float* outR = work.getWritePointer (1) + from;

    // ---- filter view: what comes into the filters (all oscillators, mono) ----
    {
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < len; ++i)
        {
            float x = 0.0f;
            for (int o = 0; o < kNumOscs; ++o)
                x += bufs[o]->getSample (0, from + i) + bufs[o]->getSample (1, from + i);
            scopeRing[(size_t) w] = 0.5f * x;
            w = (w + 1) & (kScopeSize - 1);
        }
        scopeWrite.store (w, std::memory_order_release);
    }

    // ---- every oscillator (and the SUB) through the blocks it is routed to ----
    const float gsWant = pSubOn->load() > 0.5f ? v[mtSubVol] : 0.0f;
    if (! engineSub.anyActive()) subSmooth[0] = subSmooth[1] = lastGSub = gsWant;
    subSmooth[0] += (gsWant - subSmooth[0]) * sm;
    subSmooth[1] += (subSmooth[0] - subSmooth[1]) * sm;
    const float gsSub = subSmooth[1];
    const bool subLive = gsSub > 0.0001f || lastGSub > 0.0001f;
    if (subLive)
    {
        // OSC 2 SUB (internally "sub"): no grains; filters/effects only where its digit is lit
        float* SL = subBuf.getWritePointer (0) + from;
        float* SR = subBuf.getWritePointer (1) + from;
        engineSub.render (SL, SR, len, subParams (v), subSlot.active.get());
        for (int k = 0; k < kSubHarms; ++k) subGainPrev[(size_t) k] = jlimit (0.0f, 1.0f, v[mtSubH1 + k]);
        for (int i = 0; i < len; ++i)
        {
            const float gg = lastGSub + (gsSub - lastGSub) * (float) (i + 1) / (float) len;
            SL[i] *= gg; SR[i] *= gg;
        }
    }
    else
        subBuf.clear (from, len);
    lastGSub = gsSub;

    // 0.99.5: DIST is one shared block (like Serum / Saturn): everything routed into it is mixed first and distorted
    // together, so a noise rides on the tone and they crackle together instead of each one fizzing on its own.
    // After DIST that mix goes on through DELAY / REVERB if they are lit for any of its members.
    float* DL = distBuf.getWritePointer (0);
    float* DR = distBuf.getWritePointer (1);
    FloatVectorOperations::clear (DL, len);
    FloatVectorOperations::clear (DR, len);
    bool distAny = false, distToDelay = false, distToReverb = false;

    auto applyReverb = [&] (FxChain& c, float* L, float* R)
    {
        float* wL = wetBuf.getWritePointer (0);
        float* wR = wetBuf.getWritePointer (1);
        std::copy (L, L + len, wL);
        std::copy (R, R + len, wR);
        auto wetBlock = dsp::AudioBlock<float> (wetBuf).getSubBlock (0, (size_t) len);
        dsp::ProcessContextReplacing<float> wctx (wetBlock);
        c.reverb.setParameters (rp);
        c.reverb.process (wctx);
        if (revCutHz > 22.0f)
        {
            c.revCut.setCutoffFrequency (revCutHz);
            c.revCut.process (wctx);
        }
        for (int i = 0; i < len; ++i)
        {
            L[i] = L[i] * revDry + wL[i];
            R[i] = R[i] * revDry + wR[i];
        }
    };

    for (int o = 0; o <= kNumOscs; ++o)
    {
        const bool isSub = o == kNumOscs;
        auto& c = isSub ? subChain : chains[(size_t) o];
        auto& buf = isSub ? subBuf : *bufs[o];
        float* L = buf.getWritePointer (0) + from;
        float* R = buf.getWritePointer (1) + from;

        float peak = 0.0f;
        for (int i = 0; i < len; ++i) peak = jmax (peak, std::abs (L[i]), std::abs (R[i]));
        c.quietSeconds = peak > 1.0e-6f ? 0.0 : c.quietSeconds + (double) len / sr;
        if (c.quietSeconds > 12.0) continue;          // silent for a long time: nothing left to ring out

        auto routed = [this, o, isSub] (int block) { return (isSub ? pRouteSub[(size_t) block] : pRoute[(size_t) block][(size_t) o])->load() > 0.5f; };

        if (filterOn && routed (0))
            c.filter.process (L, R, len, ft, ft == FilterUnit::Peak ? jlimit (20.0f, (float) (sr * 0.45), v[mtCutoff]) : cut, ft == FilterUnit::Peak ? v[mtPeakQ] : v[mtReso], v[mtPeakGain]);
        if (filter2On && routed (4))
            c.filter2.process (L, R, len, ft2, ft2 == FilterUnit::Peak ? jlimit (20.0f, (float) (sr * 0.45), v[mtCutoff2]) : cut2, ft2 == FilterUnit::Peak ? v[mtPeakQ2] : v[mtReso2], v[mtPeakGain2]);
        if (distOn && routed (1))
        {
            // into the shared DIST: its own delay / reverb are not used, the mix decides after DIST
            FloatVectorOperations::add (DL, L, len);
            FloatVectorOperations::add (DR, R, len);
            distAny = true;
            distToDelay  = distToDelay  || (delayOn && routed (2));
            distToReverb = distToReverb || (revOn && routed (3));
            continue;
        }
        if (delayOn && routed (2))
            c.delay.process (L, R, len, delayMs, v[mtDelayFb], v[mtDelayTone], v[mtDelayMix], pDelayPing->load() > 0.5f);
        if (revOn && routed (3))
            applyReverb (c, L, R);

        for (int i = 0; i < len; ++i) { outL[i] += L[i]; outR[i] += R[i]; }
    }

    {
        auto& c = distBusChain;
        float peak = 0.0f;
        for (int i = 0; i < len; ++i) peak = jmax (peak, std::abs (DL[i]), std::abs (DR[i]));
        c.quietSeconds = (distAny && peak > 1.0e-6f) ? 0.0 : c.quietSeconds + (double) len / sr;
        if (distAny) { c.busDelay = distToDelay; c.busReverb = distToReverb; }
        if (c.quietSeconds <= 12.0)
        {
            if (distAny)
                c.distortion.process (DL, DR, len, (int) pDistType->load(), v[mtDistDrive], 0.88f, v[mtDistMix]);   // no TONE knob (0.99.5)
            if (c.busDelay && delayOn)
                c.delay.process (DL, DR, len, delayMs, v[mtDelayFb], v[mtDelayTone], v[mtDelayMix], pDelayPing->load() > 0.5f);
            if (c.busReverb && revOn)
                applyReverb (c, DL, DR);
            for (int i = 0; i < len; ++i) { outL[i] += DL[i]; outR[i] += DR[i]; }
        }
    }

    // ---- output ----
    outGain.setTargetValue (Decibels::decibelsToGain (v[mtOutput]));
    float pl = 0.0f, pr = 0.0f;
    const bool clipper = pClipOn->load() > 0.5f;   // CLIP button: soft clipper on the output (never over 0 dB)
    for (int i = 0; i < len; ++i)
    {
        const float gg = outGain.getNextValue();
        const float l = outL[i] * gg, r = outR[i] * gg;
        pl = jmax (pl, std::abs (l));
        pr = jmax (pr, std::abs (r));
        outL[i] = clipper ? std::tanh (l) : l;
        outR[i] = clipper ? std::tanh (r) : r;
        blockPeak = jmax (blockPeak, std::abs (outL[i]), std::abs (outR[i]));
    }
    if (pl > peakL.load (std::memory_order_relaxed)) peakL.store (pl, std::memory_order_relaxed);
    if (pr > peakR.load (std::memory_order_relaxed)) peakR.store (pr, std::memory_order_relaxed);
    if (pl >= 1.0f || pr >= 1.0f) clipped.store (true, std::memory_order_relaxed);

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

    const auto frames = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 600.0));
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

void GranulumProcessor::clearSample()
{
    auto silent = std::make_shared<granulum::SampleData>();
    silent->sampleRate = 44100.0;
    silent->channels.assign (2, std::vector<float> (256, 0.0f));
    {
        const ScopedLock sl (infoLock);
        samplePath = File();
        smpPeaks = nullptr;
        smpSeconds = 0.0;
    }
    smpSlot.push (std::move (silent));
    sampleLoaded.store (false);
}

void GranulumProcessor::setHarmonics (const std::array<float, granulum::kNumHarmonics>& h)
{
    harmonics = h;
    for (int k = 0; k < granulum::kNumHarmonics; ++k) harmNow[(size_t) k].store (h[(size_t) k]);
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

File GranulumProcessor::factorySampleDir()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("GRAIN FORM").getChildFile ("Samples");
}

void GranulumProcessor::installFactorySamples()
{
    const auto dir = factorySampleDir().getChildFile ("Atmos");
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const String file = BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]);
        if (! file.endsWithIgnoreCase (".flac") || CharacterFunctions::isDigit (file[0])) continue;   // skip the noises
        const auto target = dir.getChildFile (file);
        if (target.existsAsFile()) continue;
        int size = 0;
        if (const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size))
        {
            dir.createDirectory();
            target.replaceWithData (data, (size_t) size);
        }
    }
}

bool GranulumProcessor::isFactorySample() const
{
    const ScopedLock sl (infoLock);
    return samplePath != File() && samplePath.isAChildOf (factorySampleDir());
}

void GranulumProcessor::loadAdjacentFactorySample (int direction)
{
    auto files = factorySampleDir().findChildFiles (File::findFiles, true, "*.wav;*.flac;*.aif;*.aiff");
    if (files.isEmpty()) return;
    files.sort();
    File current; { const ScopedLock sl (infoLock); current = samplePath; }
    int idx = files.indexOf (current);
    idx = idx < 0 ? (direction > 0 ? 0 : files.size() - 1) : (idx + direction + files.size()) % files.size();
    String err;
    loadSample (files[idx], err);
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
    addCleanSlot.collect();
    waveSlot.collect();
    {
        // OSC 1 WAVE: build the band-limited mips of the chosen waveform when it (or the sample rate) changes
        const int wv = jlimit (0, 3, (int) apvts.getRawParameterValue (ParamID::osc1Wave)->load());
        const bool used = apvts.getRawParameterValue (ParamID::osc1Mode)->load() > 0.5f;
        const double rate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        if (used && (wv != builtWave || rate != builtWaveRate))
        {
            builtWave = wv; builtWaveRate = rate;
            waveSlot.push (std::make_shared<granulum::SampleData> (granulum::renderWaveMips (wv, rate)));
        }
    }
    subSlot.collect();
    smpSlot.collect();
    noiseSlot.collect();

    if (const int t = (int) pOsc3Type->load(); t != shownNoise && t >= 0 && t < (int) noises.size())
    {
        shownNoise = t;
        noiseSlot.push (noises[(size_t) t]);
    }

    // OSC 4 SUB source: 3 harmonics (rebuilt when the sticks move)
    {
        std::array<float, kSubHarms> now {};
        for (int k = 0; k < kSubHarms; ++k) now[(size_t) k] = pSubH[(size_t) k]->load();
        if (now != subShown)
        {
            subShown = now;
            std::array<float, granulum::kNumHarmonics> amps {};
            for (int k = 0; k < kSubHarms; ++k) amps[(size_t) k] = now[(size_t) k];
            subSlot.push (std::make_shared<granulum::SampleData> (granulum::renderAdditiveSplit (amps, 0xffffu, 0x1fu).first));   // 5 layers: LFO-able
        }
    }

    // sticks with an LFO get their own layer (rebuilt when that set changes)
    uint32_t layerMask = 0;
    for (auto& m : mods)
    {
        const int t = m.target.load();
        if (isHarmTarget (t)) layerMask |= 1u << (t - kHarmTarget0);
    }
    if (layerMask != layerMaskBuilt) { layerMaskBuilt = layerMask; harmonicsDirty.store (true); }

    if (harmonicsDirty.exchange (false))
    {
        auto parts = granulum::renderAdditiveSplit (harmonics, grainMask.load(), layerMaskBuilt);
        granulum::SampleData full = parts.first;
        for (size_t i = 0; i < full.channels[0].size(); ++i) full.channels[0][i] += parts.second.channels[0][i];
        for (auto* part : { &parts.first, &parts.second })
            for (size_t j = 0; j < part->layers.size(); ++j)
            {
                const float a = harmonics[(size_t) part->layerHarm[j]];
                for (size_t i = 0; i < full.channels[0].size(); ++i) full.channels[0][i] += a * part->layers[j][i];
            }
        auto pk = makePeaks (full);
        {
            const ScopedLock sl (infoLock);
            addPeaks = std::move (pk);
            addSeconds = full.numFrames() / full.sampleRate;
        }
        addSlot.push (std::make_shared<granulum::SampleData> (std::move (parts.first)));
        addCleanSlot.push (std::make_shared<granulum::SampleData> (std::move (parts.second)));
    }
}

// ===========================================================================
// OSC 3 noises (FLAC files compiled into the plugin as BinaryData)

namespace
{
    struct NoiseRes { String file, name; };
    const std::vector<NoiseRes>& noiseResources()
    {
        static const std::vector<NoiseRes> list = []
        {
            std::vector<NoiseRes> v;
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                const String file = BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]);
                if (! file.endsWithIgnoreCase (".flac") || ! CharacterFunctions::isDigit (file[0])) continue;   // noises are "NN_NAME.flac"
                // "03_TAPE_HISS.flac" -> "TAPE HISS"
                const String name = file.upToLastOccurrenceOf (".", false, false).fromFirstOccurrenceOf ("_", false, false).replace ("_", " ");
                v.push_back ({ BinaryData::namedResourceList[i], name });
            }
            std::sort (v.begin(), v.end(), [] (const NoiseRes& a, const NoiseRes& b)
            {
                return String (BinaryData::getNamedResourceOriginalFilename (a.file.toRawUTF8()))
                     < String (BinaryData::getNamedResourceOriginalFilename (b.file.toRawUTF8()));
            });
            return v;
        }();
        return list;
    }
}

const StringArray& GranulumProcessor::noiseNames()
{
    static const StringArray names = []
    {
        StringArray s;
        for (auto& r : noiseResources()) s.add (r.name);
        if (s.isEmpty()) s.add ("NONE");
        return s;
    }();
    return names;
}

void GranulumProcessor::loadNoises()
{
    FlacAudioFormat flac;
    for (auto& r : noiseResources())
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (r.file.toRawUTF8(), size);
        auto d = std::make_shared<granulum::SampleData>();
        if (data != nullptr && size > 0)
            if (std::unique_ptr<AudioFormatReader> reader (flac.createReaderFor (new MemoryInputStream (data, (size_t) size, false), true)); reader != nullptr)
            {
                const int frames = (int) reader->lengthInSamples;
                const int chans = jlimit (1, 2, (int) reader->numChannels);
                AudioBuffer<float> tmp (chans, frames);
                reader->read (&tmp, 0, frames, 0, true, chans > 1);
                d->sampleRate = reader->sampleRate;
                d->channels.resize ((size_t) chans);
                for (int c = 0; c < chans; ++c)
                    d->channels[(size_t) c].assign (tmp.getReadPointer (c), tmp.getReadPointer (c) + frames);
            }
        noisePeaks.push_back (makePeaks (*d));
        noises.push_back (std::move (d));
    }
}

std::shared_ptr<const WaveformPeaks> GranulumProcessor::getNoisePeaks (int index) const
{
    return (index >= 0 && index < (int) noisePeaks.size()) ? noisePeaks[(size_t) index] : nullptr;
}

// ===========================================================================
// Presets: plain XML files in Documents/GRAIN FORM/Presets

File GranulumProcessor::presetFolder()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("GRAIN FORM").getChildFile ("Presets");
    f.createDirectory();
    // presets made in the old Granulum versions are copied over once (never overwritten)
    static bool migrated = false;
    if (! migrated)
    {
        migrated = true;
        const auto old = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Granulum").getChildFile ("Presets");
        for (auto& o : old.findChildFiles (File::findFiles, false, "*.gpreset"))
            if (! f.getChildFile (o.getFileName()).exists()) o.copyFileTo (f.getChildFile (o.getFileName()));
    }
    return f;
}

Array<File> GranulumProcessor::presetsIn (const File& folder)
{
    auto files = folder.findChildFiles (File::findFiles, false, "*.gpreset");
    files.sort();
    return files;
}

Array<File> GranulumProcessor::listPresetFolders()
{
    auto dirs = presetFolder().findChildFiles (File::findDirectories, false);
    Array<File> out;
    for (auto& d : dirs) if (! d.getFileName().startsWithChar ('.')) out.add (d);
    out.sort();
    return out;
}

Array<File> GranulumProcessor::listPresets() const
{
    auto all = presetsIn (presetFolder());
    for (auto& d : listPresetFolders()) all.addArray (presetsIn (d));
    return all;
}

File GranulumProcessor::currentPresetFile() const
{
    const auto dir = presetSub.isEmpty() ? presetFolder() : presetFolder().getChildFile (presetSub);
    return dir.getChildFile (presetName + ".gpreset");
}

bool GranulumProcessor::savePreset (const String& rawName, const File& folder)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    const auto dir = folder == File() ? presetFolder() : folder;
    dir.createDirectory();
    MemoryBlock mb;
    presetName = name;
    presetSub = dir == presetFolder() ? String() : dir.getFileName();
    getStateInformation (mb);
    if (auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        return xml->writeTo (dir.getChildFile (name + ".gpreset"));
    return false;
}

bool GranulumProcessor::loadPreset (const File& f)
{
    auto xml = parseXML (f);
    if (xml == nullptr) return false;
    MemoryBlock mb;
    copyXmlToBinary (*xml, mb);
    setStateInformation (mb.getData(), (int) mb.getSize());
    presetName = f.getFileNameWithoutExtension();
    presetSub = f.getParentDirectory() == presetFolder() ? String() : f.getParentDirectory().getFileName();
    soundReset.store (true);
    return true;
}

String GranulumProcessor::getPresetName() const { return presetName; }

// ===========================================================================
// Random

void GranulumProcessor::randomize()
{
    // 0.99.5: RANDOM ALL only touches the parts ticked in its menu (randMask)
    const int m = randMask.load();
    if (m & rndGrainsBit)  randomizeSection (0);
    if (m & rndFiltersBit) randomizeSection (1);
    if (m & (rndDistBit | rndDelayBit | rndReverbBit)) randomizeSection (2);
    if ((m & rndSticksBit) && apvts.getRawParameterValue (ParamID::osc1Mode)->load() < 1.5f) randomizeSection (3);
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
        // pick a CHARACTER first, then vary it, so every press sounds clearly different
        enum { Cloud, Shatter, Bells, Freeze, Stutter, Tape, NumCharacters };
        static int lastCharacter = -1;
        int c = r.nextInt (NumCharacters);
        if (c == lastCharacter) c = (c + 1 + r.nextInt (NumCharacters - 1)) % NumCharacters;
        lastCharacter = c;
        auto v = [&] (float centre, float spread) { return jlimit (0.0f, 1.0f, centre + spread * (2.0f * r.nextFloat() - 1.0f)); };
        float bloom = 0, smear = 0, scatter = 0, shimmer = 0, drift = 0, size = 0.5f, dens = 0.5f, speed = 0.0f, pos = range (0.0f, 0.8f), chime = 0;
        switch (c)
        {
            case Cloud:   bloom = v (0.75f, 0.2f); smear = v (0.6f, 0.25f); scatter = v (0.3f, 0.2f); shimmer = maybe (0.5f, v (0.25f, 0.15f));
                          drift = v (0.5f, 0.3f); size = v (0.75f, 0.15f); dens = v (0.65f, 0.15f); speed = range (0.0f, 0.3f); chime = maybe (0.3f, v (0.3f, 0.15f)); break;
            case Shatter: bloom = v (0.2f, 0.2f); smear = v (0.1f, 0.1f); scatter = v (0.9f, 0.1f); shimmer = v (0.35f, 0.25f);
                          drift = v (0.3f, 0.3f); size = v (0.25f, 0.15f); dens = v (0.45f, 0.2f); speed = range (-0.5f, 1.0f); chime = maybe (0.4f, v (0.4f, 0.2f)); break;
            case Bells:   bloom = v (0.35f, 0.2f); smear = v (0.15f, 0.15f); scatter = v (0.65f, 0.2f); shimmer = v (0.8f, 0.2f);
                          drift = v (0.25f, 0.2f); size = v (0.35f, 0.2f); dens = v (0.4f, 0.15f); speed = range (0.0f, 0.5f); chime = v (0.7f, 0.25f); break;
            case Freeze:  bloom = v (0.6f, 0.2f); smear = v (0.85f, 0.15f); scatter = v (0.15f, 0.15f); shimmer = maybe (0.6f, v (0.4f, 0.2f));
                          drift = v (0.2f, 0.2f); size = v (0.85f, 0.1f); dens = v (0.8f, 0.15f); speed = 0.0f; chime = maybe (0.4f, v (0.45f, 0.2f)); break;
            case Stutter: bloom = v (0.1f, 0.1f); smear = 0.0f; scatter = v (0.55f, 0.3f); shimmer = maybe (0.4f, v (0.3f, 0.2f));
                          drift = v (0.1f, 0.1f); size = v (0.2f, 0.12f); dens = v (0.3f, 0.15f); speed = range (0.8f, 1.6f); chime = maybe (0.3f, v (0.3f, 0.2f)); break;
            default:      bloom = v (0.4f, 0.2f); smear = v (0.45f, 0.25f); scatter = v (0.4f, 0.25f); shimmer = v (0.35f, 0.2f);
                          drift = v (0.85f, 0.15f); size = v (0.6f, 0.2f); dens = v (0.5f, 0.2f); speed = range (-1.0f, -0.2f); chime = maybe (0.3f, v (0.35f, 0.2f)); break;
        }
        set (ParamID::bloom, bloom);     set (ParamID::smear, smear);   set (ParamID::scatter, scatter);
        set (ParamID::shimmer, shimmer); set (ParamID::drift, drift);   set (ParamID::gsize, size);
        set (ParamID::density, dens);    set (ParamID::speed, speed);   set (ParamID::position, pos);
        set (ParamID::chime, chime);
        set (ParamID::gmix, v (0.85f, 0.15f));
    }
    else if (section == 1)
    {
        const int type = r.nextInt (2);                         // LP12 / LP24 for filter 1
        set (ParamID::ftype,  (float) type);
        set (ParamID::cutoff, 300.0f * std::pow (20.0f, r.nextFloat()));
        set (ParamID::reso,   0.6f * std::pow (5.0f, r.nextFloat()));
        const int t2 = 2 + r.nextInt (5);                       // HP .. VOWEL for filter 2
        set (ParamID::ftype2,  (float) t2);
        set (ParamID::cutoff2, 150.0f * std::pow (30.0f, r.nextFloat()));
        set (ParamID::reso2,   0.6f * std::pow (6.0f, r.nextFloat()));
        set (ParamID::peakGain2, (r.nextBool() ? 1.0f : -1.0f) * (4.0f + 10.0f * r.nextFloat()));
    }
    else if (section == 2)
    {
        const int m = randMask.load();   // the FX dice also respect the RANDOM menu ticks
        if (m & rndDistBit)
        {
            set (ParamID::distType,  (float) r.nextInt (3));
            set (ParamID::distDrive, range (0.1f, 0.5f));   // 0.99.5: random never slams the distortion
            set (ParamID::distTone,  range (0.35f, 1.0f));
            set (ParamID::distMix,   range (0.4f, 1.0f));
        }
        if (m & rndReverbBit)
        {
            set (ParamID::revMix,    range (0.1f, 0.5f));
            set (ParamID::revSize,   range (0.3f, 0.95f));
            set (ParamID::revLowCut, range (80.0f, 400.0f));
        }
        if (m & rndDelayBit)
        {
            set (ParamID::delayDiv,  (float) (3 + r.nextInt (12)));
            set (ParamID::delayFb,   range (0.2f, 0.7f));
            set (ParamID::delayTone, range (0.3f, 0.9f));
            set (ParamID::delayMix,  range (0.15f, 0.45f));
        }
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
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetFolder", presetSub, nullptr);
    state.setProperty ("randMask", randMask.load(), nullptr);
    state.setProperty ("noiseOrder", 2, nullptr);
    state.setProperty ("oscFold", String ((int) uiFold[0]) + String ((int) uiFold[1]) + String ((int) uiFold[2]), nullptr);
    state.setProperty ("pluginVersion", GRAINFORM_VERSION, nullptr);
    state.setProperty ("grainMask", (int) grainMask.load(), nullptr);

    StringArray ms;
    for (auto& m : mods)
        if (const int t = m.target.load(); t >= 0)
            ms.add (String (modTargetId (t)) + ":" + String (m.lfo.load()) + ":" + String (m.depth.load(), 4));   // modTargetId: also "harm1..16" (never index the table with them)
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
    // before 0.9.8 the note values were in another order: map the old index -> name -> new index
    {
        const String ver = state.getProperty ("pluginVersion", "0").toString();
        if (ver.compareNatural ("0.9.8") < 0)
        {
            static const StringArray oldNames { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                                "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
            for (auto child : state)
            {
                const String id = child.getProperty ("id").toString();
                if (id == ParamID::delayDiv || id == "lfo1Div" || id == "lfo2Div" || id == "lfo3Div")
                {
                    const int o = jlimit (0, oldNames.size() - 1, (int) std::round ((double) child.getProperty ("value")));
                    child.setProperty ("value", jmax (0, divisionNames().indexOf (oldNames[o])), nullptr);
                }
            }
        }
    }
    // before 0.9.4 the distortion menu was TUBE, HARD CLIP, FOLD, BITCRUSH, RECTIFY; now FOLD, TUBE, HARD CLIP, BITCRUSH
    {
        const String ver = state.getProperty ("pluginVersion", "0").toString();
        const bool old = ver.compareNatural ("0.9.4") < 0;
        if (old)
            for (auto child : state)
                if (child.getProperty ("id").toString() == ParamID::distType)
                {
                    static const int oldToNew[5] = { 1, 2, 0, 3, 3 };   // RECTIFY -> BITCRUSH
                    const int o = jlimit (0, 4, (int) std::round ((double) child.getProperty ("value")));
                    child.setProperty ("value", oldToNew[o], nullptr);
                }
    }
    // 0.99.5: BITCRUSH is gone -> HARD CLIP
    for (auto child : state)
        if (child.getProperty ("id").toString() == ParamID::distType && (double) child.getProperty ("value") > 2.5)
            child.setProperty ("value", 2, nullptr);
    // 0.99.5: noise list = WHITE PINK BROWN BLUE first; STATIC / WIND / BREATH removed. Older states keep their noise.
    if (! state.hasProperty ("noiseOrder"))
        for (auto child : state)
            if (child.getProperty ("id").toString() == ParamID::osc3Type)
            {
                // old: AIR TAPE HISS VINYL STATIC WIND BREATH PAPER DUST FROST METAL RAIN
                static const char* const oldNames[10] = { "AIR", "TAPE HISS", "VINYL", "WHITE", "BROWN", "PINK", "PAPER", "DUST", "FROST", "METAL RAIN" };
                const int o = jlimit (0, 9, (int) std::round ((double) child.getProperty ("value")));
                child.setProperty ("value", jmax (0, noiseNames().indexOf (oldNames[o])), nullptr);
            }
    const String path = state.getProperty ("samplePath").toString();
    const auto hs = StringArray::fromTokens (state.getProperty ("harmonics").toString(), ",", "");
    const auto ms = StringArray::fromTokens (state.getProperty ("mods").toString(), ";", "");
    std::array<String, kNumLfos> shapes;
    for (int l = 0; l < kNumLfos; ++l) shapes[(size_t) l] = state.getProperty ("lfoShape" + String (l + 1)).toString();
    const String pn = state.getProperty ("presetName", "INIT").toString();
    const String pf = state.getProperty ("presetFolder", "").toString();
    if (state.hasProperty ("randMask")) randMask.store ((int) state.getProperty ("randMask"));
    {
        const String fo = state.getProperty ("oscFold", "000").toString();
        for (int i = 0; i < 3; ++i) uiFold[i] = fo.length() > i && fo[i] == '1';
    }
    const uint32_t gm = (uint32_t) (int) state.getProperty ("grainMask", 0xffff);
    apvts.replaceState (state);
    presetName = pn;
    presetSub = pf;

    // Saved before 0.92: OSC 3 knob was COLOR (a tone filter), now it is PITCH -> start at 0 st
    if (! state.hasProperty ("pluginVersion"))
        if (auto* q = apvts.getParameter (ParamID::osc3Color)) q->setValueNotifyingHost (q->convertTo0to1 (0.0f));

    // Old sessions/presets had a GRANULAR on/off lamp. If it was off, keep them clean:
    // switch every OSC's GRAIN off, then mark the (now hidden) block switch as on.
    if (auto* gb = apvts.getParameter (ParamID::grainOn); gb != nullptr && gb->getValue() < 0.5f)
    {
        for (auto* id : { ParamID::osc1Grain, ParamID::osc2Grain, ParamID::osc3Grain })
            if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (0.0f);
        gb->setValueNotifyingHost (1.0f);
    }
    setGrainMask (gm);

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
        if (items.size() < 2 || items.size() > kMaxLfoPoints) continue;
        LfoShape pts ((size_t) items.size());
        for (int i = 0; i < items.size(); ++i)
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
        { if (auto* on = apvts.getParameter (ParamID::osc2On)) on->setValueNotifyingHost (osc2 ? 1.0f : 0.0f); }
    else
        clearSample();   // 0.99.5: a preset without an audio file must not keep the file of the previous preset
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
