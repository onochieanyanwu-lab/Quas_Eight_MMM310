#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

const StringArray Q8Processor::hpfNames   { "Off", "40 Hz", "80 Hz", "120 Hz" };
const StringArray Q8Processor::lfNames    { "50", "140", "250", "400" };
const StringArray Q8Processor::mfNames    { "400", "700", "1.5k", "3k" };
const StringArray Q8Processor::hfNames    { "3k", "5k", "7.5k", "10k" };
const StringArray Q8Processor::ratioNames { "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1" };

const float Q8Processor::hpfHz[]     { 0.f, 40.f, 80.f, 120.f };
// Centre frequencies and tuning capacitors read from the MM-310 BL schematic (drawing 3891):
// LF  S3: 50/140 Hz via C37 .15uF (L1), 250/400 Hz via C36 .1uF (L2)
// MF  S4: 400/700/1.5k/3k via C35 .1, C34 .068, C33 .03, C32 .015 uF (L3)
// HF  S5A: 3k/5k/7.5k/10k via C31 .015, C30 .01, C29 .0068, C28 .0047 uF (L4)
const float Q8Processor::lfHz[]  { 50.f, 140.f, 250.f, 400.f };
const float Q8Processor::lfCap[] { .15e-6f, .15e-6f, .1e-6f, .1e-6f };
const float Q8Processor::mfHz[]  { 400.f, 700.f, 1500.f, 3000.f };
const float Q8Processor::mfCap[] { .1e-6f, .068e-6f, .03e-6f, .015e-6f };
const float Q8Processor::hfHz[]  { 3000.f, 5000.f, 7500.f, 10000.f };
const float Q8Processor::hfCap[] { .015e-6f, .01e-6f, .0068e-6f, .0047e-6f };
const float Q8Processor::ratioVals[] { 1.5f, 2.f, 3.f, 4.f, 6.f, 10.f, 20.f };

AudioProcessorValueTreeState::ParameterLayout Q8Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto dbRange = [] (float lo, float hi) { return NormalisableRange<float> (lo, hi, 0.1f); };

    // Preamp / transformer
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "preOn", 1 },  "Preamp On", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "inGain", 1 }, "Input Gain", dbRange (-20.f, 40.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "xfmr", 1 },   "Transformer",
                                                  NormalisableRange<float> (0.f, 100.f, 0.1f), 50.f));
    // Filter
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hpf", 1 }, "High-Pass", hpfNames, 0));

    // EQ
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "eqOn", 1 }, "EQ On", true));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "lfG", 1 },  "LF Gain",  dbRange (-15.f, 15.f), 0.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "lfF", 1 },  "LF Freq",  lfNames, 1));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mfG", 1 },  "MF Gain",  dbRange (-15.f, 15.f), 0.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mfF", 1 },  "MF Freq",  mfNames, 1));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "hfG", 1 },  "HF Gain",  dbRange (-15.f, 15.f), 0.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hfF", 1 },  "HF Freq",  hfNames, 1));
    // Compressor
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "compOn", 1 }, "Comp On", false));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "thr", 1 },   "Threshold", dbRange (-40.f, 0.f), -18.f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "ratio", 1 }, "Ratio", ratioNames, 2));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "att", 1 },   "Attack",
                                                   NormalisableRange<float> (0.3f, 50.f, 0.01f, 0.4f), 10.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rel", 1 },   "Release",
                                                   NormalisableRange<float> (40.f, 1500.f, 1.f, 0.5f), 250.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "makeup", 1 }, "Makeup", dbRange (0.f, 24.f), 0.f));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 },   "Comp Mix",
                                                   NormalisableRange<float> (0.f, 100.f, 0.1f), 100.f));
    // Output
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "outGain", 1 }, "Output", dbRange (-24.f, 12.f), 0.f));
    return l;
}

Q8Processor::Q8Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

bool Q8Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

void Q8Processor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    dsp::ProcessSpec spec { sampleRate, (uint32) blockSize, 2 };
    hpf.prepare (spec); hpf.reset();
    for (auto* b : { &bandLf, &bandMf, &bandHf }) b->reset();
    smLf = smMf = smHf = 0.f;

    oversampling.initProcessing ((size_t) blockSize);
    oversampling.reset();
    setLatencySamples ((int) oversampling.getLatencyInSamples());

    for (int c = 0; c < 2; ++c) { coreState[c] = dcX[c] = dcY[c] = 0.f; }
    compEnvDb = 0.f;
    prevIn = Decibels::decibelsToGain (p ("inGain"));
    prevOut = Decibels::decibelsToGain (p ("outGain"));
    prevMakeup = Decibels::decibelsToGain (p ("makeup"));
    lastHpf = lastLf = lastMf = lastHf = -1;
    lastLfG = lastMfG = lastHfG = 1e9f;
}

// ---------------------------------------------------------------------------
// Nonlinear models
// ---------------------------------------------------------------------------

// Input transformer: the low band (below ~150 Hz) is pushed through a tanh "core"
// so bass saturates first and harder, which is the classic transformer behaviour.
float Q8Processor::transformer (float x, int ch, float amount) noexcept
{
    const float a = amount * 0.01f;                          // 0..1
    const float coef = 1.f - std::exp (-MathConstants<float>::twoPi * 150.f / (float) (sr * 2.0));
    coreState[ch] += coef * (x - coreState[ch]);
    const float low = coreState[ch];
    const float high = x - low;
    const float drive = 1.f + 5.f * a;
    const float shapedLow = std::tanh (drive * low) / drive;
    return high + low + a * (shapedLow - low);               // blend by amount
}

// Discrete class-A gain stage: asymmetric tanh gives even + odd harmonics.
float Q8Processor::preampStage (float x) noexcept
{
    constexpr float ref = 0.6f, bias = 0.15f;
    const float t = std::tanh (bias);
    const float norm = 1.f / (1.f - t * t);                  // unity small-signal gain
    return ref * (std::tanh (x / ref + bias) - t) * norm;
}

// ---------------------------------------------------------------------------

// One EQ band from the MM-310 BL circuit. A2 is a differential amp (R11 = R12 = R13 = 10k, Rf = R14 = 24k).
// A series L-C branch (L derived from the schematic's tuning cap and switch frequency) is either
//  - boost: from the inverting leg to ground, so the stage gain is 1 + k*sC/D(s),  k = R14/(1+R14/R11)
//  - cut:   from the non-inverting leg to ground, forming a divider against R12||R13 = 5k:  D(s)/(D(s)+Rp*sC)
// with D(s) = LC s^2 + R C s + 1. R is the series resistance chosen by the stepped 2k control; here it is
// solved continuously for the requested peak gain. Qu models inductor loss (assumed, not from the schematic).
void Q8Processor::designBand (Band& band, double fs, double f0, double capF, double gainDb, double Qu)
{
    if (std::abs (gainDb) < 0.05) { band.set (1, 0, 0, 1, 0, 0); return; }

    const double w0 = MathConstants<double>::twoPi * f0;
    const double L = 1.0 / (w0 * w0 * capF);
    const double Rloss = w0 * L / Qu;
    const double g = std::pow (10.0, gainDb / 20.0);
    const double A0 = 1.0 + 24000.0 / 10000.0, k = 24000.0 / A0, Rp = 5000.0;

    const double n2 = L * capF, d2 = n2;
    double n1, d1;
    if (gainDb > 0.0) { const double R = jmax (Rloss, k / (g - 1.0));       n1 = (R + k) * capF; d1 = R * capF; }
    else              { const double R = jmax (Rloss, Rp * g / (1.0 - g));  n1 = R * capF;       d1 = (R + Rp) * capF; }

    const double c = w0 / std::tan (w0 / (2.0 * fs)), c2 = c * c;   // bilinear transform, prewarped at f0
    band.set (n2 * c2 + n1 * c + 1.0, 2.0 * (1.0 - n2 * c2), n2 * c2 - n1 * c + 1.0,
              d2 * c2 + d1 * c + 1.0, 2.0 * (1.0 - d2 * c2), d2 * c2 - d1 * c + 1.0);
}

void Q8Processor::updateFilters()
{
    const int h = (int) p ("hpf");
    if (h != lastHpf && h > 0)
        *hpf.state = *dsp::IIR::Coefficients<float>::makeHighPass (sr, hpfHz[h], 0.7071f);
    lastHpf = h;

    const int lfi = (int) p ("lfF"), mfi = (int) p ("mfF"), hfi = (int) p ("hfF");
    // one-pole smoothing of the gain knobs at block rate to avoid zipper noise
    smLf += 0.35f * (p ("lfG") - smLf);
    smMf += 0.35f * (p ("mfG") - smMf);
    smHf += 0.35f * (p ("hfG") - smHf);

    if (lfi != lastLf || std::abs (smLf - lastLfG) > 0.01f) { designBand (bandLf, sr, lfHz[lfi], lfCap[lfi], smLf, 12.0); lastLfG = smLf; lastLf = lfi; }
    if (mfi != lastMf || std::abs (smMf - lastMfG) > 0.01f) { designBand (bandMf, sr, mfHz[mfi], mfCap[mfi], smMf, 15.0); lastMfG = smMf; lastMf = mfi; }
    if (hfi != lastHf || std::abs (smHf - lastHfG) > 0.01f) { designBand (bandHf, sr, jmin (hfHz[hfi], (float) (sr * 0.45)), hfCap[hfi], smHf, 15.0); lastHfG = smHf; lastHf = hfi; }
}

void Q8Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nCh = jmin (2, buffer.getNumChannels());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    const bool preOn = p ("preOn") > 0.5f, eqOn = p ("eqOn") > 0.5f, compOn = p ("compOn") > 0.5f;

    // 1. input trim
    const float inG = Decibels::decibelsToGain (p ("inGain"));
    buffer.applyGainRamp (0, n, prevIn, inG);
    prevIn = inG;

    // 2. transformer + discrete preamp (2x oversampled; always run so latency is constant)
    {
        dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nCh);
        auto up = oversampling.processSamplesUp (sub);
        if (preOn)
        {
            const float amt = p ("xfmr");
            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = preampStage (transformer (d[i], ch, amt));
            }
        }
        oversampling.processSamplesDown (sub);
    }
    if (preOn) // DC blocker (asymmetric stage creates offset), ~5 Hz
    {
        const float R = 1.f - MathConstants<float>::twoPi * 5.f / (float) sr;
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                const float y = x - dcX[ch] + R * dcY[ch];
                dcX[ch] = x; dcY[ch] = y; d[i] = y;
            }
        }
    }

    // 3. filters + EQ
    updateFilters();
    {
        dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) nCh);
        dsp::ProcessContextReplacing<float> ctx (sub);
        if (lastHpf > 0) hpf.process (ctx);
    }
    if (eqOn)
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                d[i] = bandHf.process (bandMf.process (bandLf.process (d[i], ch), ch), ch);
        }

    // 4. compressor (stereo-linked, log-domain detector, soft knee)
    if (compOn)
    {
        const float thr = p ("thr"), ratio = ratioVals[(int) p ("ratio")];
        const float attC = std::exp (-1.f / (p ("att") * 0.001f * (float) sr));
        const float relC = std::exp (-1.f / (p ("rel") * 0.001f * (float) sr));
        const float mix = p ("mix") * 0.01f;
        const float mk = Decibels::decibelsToGain (p ("makeup"));
        const float slope = 1.f / ratio - 1.f, knee = 6.f;
        float grPeak = 0.f;

        for (int i = 0; i < n; ++i)
        {
            float det = 0.f;
            for (int ch = 0; ch < nCh; ++ch) det = jmax (det, std::abs (buffer.getReadPointer (ch)[i]));
            const float lvl = Decibels::gainToDecibels (det, -100.f);
            const float over = lvl - thr;
            float target = 0.f;
            if (2.f * over > knee)              target = slope * over;
            else if (2.f * over > -knee)        target = slope * square (over + knee * 0.5f) / (2.f * knee);
            const float c = target < compEnvDb ? attC : relC;
            compEnvDb = c * compEnvDb + (1.f - c) * target;
            const float g = Decibels::decibelsToGain (compEnvDb);
            grPeak = jmax (grPeak, -compEnvDb);
            for (int ch = 0; ch < nCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float dry = d[i], wet = dry * g * mk;
                d[i] = dry * (1.f - mix) + wet * mix;
            }
        }
        grMeterDb.store (grPeak);
    }
    else { compEnvDb = 0.f; grMeterDb.store (0.f); }

    // 5. line-amp stage + output trim
    if (preOn)
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) d[i] = std::tanh (d[i]);
        }
    const float outG = Decibels::decibelsToGain (p ("outGain"));
    buffer.applyGainRamp (0, n, prevOut, outG);
    prevOut = outG;
}

AudioProcessorEditor* Q8Processor::createEditor() { return new Q8Editor (*this); }

void Q8Processor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void Q8Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Q8Processor(); }
