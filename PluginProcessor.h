#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class Q8Processor : public juce::AudioProcessor
{
public:
    Q8Processor();
    ~Q8Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Q8 Strip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // choice tables shared with the editor
    static const juce::StringArray hpfNames, lfNames, mfNames, hfNames, ratioNames;
    static const float hpfHz[], lfHz[], mfHz[], hfHz[], lfCap[], mfCap[], hfCap[], ratioVals[];

    std::atomic<float> grMeterDb { 0.f };

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    void updateFilters();
    float transformer (float x, int ch, float amount) noexcept;
    static float preampStage (float x) noexcept;

    juce::dsp::Oversampling<float> oversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };

    Filter hpf;

    // EQ band: analogue prototype from the MM-310 BL schematic, bilinear-transformed, double precision
    struct Band
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1[2] {}, z2[2] {};
        void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
        { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }
        void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
        float process (float x, int ch) noexcept
        {
            const double in = x, out = b0 * in + z1[ch];
            z1[ch] = b1 * in - a1 * out + z2[ch];
            z2[ch] = b2 * in - a2 * out;
            return (float) out;
        }
    };
    static void designBand (Band&, double fs, double f0, double capF, double gainDb, double Qu);
    Band bandLf, bandMf, bandHf;
    float smLf = 0.f, smMf = 0.f, smHf = 0.f;
    double sr = 44100.0;
    float coreState[2] { 0.f, 0.f };
    float dcX[2] { 0.f, 0.f }, dcY[2] { 0.f, 0.f };
    float compEnvDb = 0.f;
    float prevIn = 1.f, prevOut = 1.f, prevMakeup = 1.f;

    // cached values so coefficients are only rebuilt on change
    int   lastHpf = -1;
    float lastLfG = 1e9f, lastMfG = 1e9f, lastHfG = 1e9f;
    int   lastLf = -1, lastMf = -1, lastHf = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Q8Processor)
};
