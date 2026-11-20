#include "PluginEditor.h"
using namespace juce;

static const Colour bg (0xff1b2736), panel (0xff243447), accent (0xffe3b04b), text (0xffd8e0ea);

Q8Editor::Knob& Q8Editor::addKnob (const String& id, const String& name)
{
    auto k = std::make_unique<Knob>();
    k->s.setSliderStyle (Slider::RotaryVerticalDrag);
    k->s.setTextBoxStyle (Slider::TextBoxBelow, false, 62, 16);
    k->s.setColour (Slider::rotarySliderFillColourId, accent);
    k->s.setColour (Slider::thumbColourId, accent);
    k->s.setColour (Slider::textBoxTextColourId, text);
    k->s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k->l.setText (name, dontSendNotification);
    k->l.setJustificationType (Justification::centred);
    k->l.setColour (Label::textColourId, text);
    k->l.setFont (Font (FontOptions (12.f)));
    k->a = std::make_unique<SA> (proc.apvts, id, k->s);
    addAndMakeVisible (k->s); addAndMakeVisible (k->l);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

Q8Editor::Combo& Q8Editor::addCombo (const String& id, const String& name, const StringArray& items)
{
    auto c = std::make_unique<Combo>();
    c->c.addItemList (items, 1);
    c->l.setText (name, dontSendNotification);
    c->l.setJustificationType (Justification::centred);
    c->l.setColour (Label::textColourId, text);
    c->l.setFont (Font (FontOptions (12.f)));
    c->a = std::make_unique<CA> (proc.apvts, id, c->c);
    addAndMakeVisible (c->c); addAndMakeVisible (c->l);
    combos.push_back (std::move (c));
    return *combos.back();
}

Q8Editor::Toggle& Q8Editor::addToggle (const String& id, const String& name)
{
    auto t = std::make_unique<Toggle>();
    t->b.setButtonText (name);
    t->b.setColour (ToggleButton::textColourId, text);
    t->b.setColour (ToggleButton::tickColourId, accent);
    t->a = std::make_unique<BA> (proc.apvts, id, t->b);
    addAndMakeVisible (t->b);
    toggles.push_back (std::move (t));
    return *toggles.back();
}

Q8Editor::Q8Editor (Q8Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    preOn  = &addToggle ("preOn", "Preamp");
    inGain = &addKnob ("inGain", "Input");
    xfmr   = &addKnob ("xfmr", "Transformer");
    hpf    = &addCombo ("hpf", "HPF", Q8Processor::hpfNames);
    eqOn   = &addToggle ("eqOn", "EQ");
    lfG = &addKnob ("lfG", "LF"); lfF = &addCombo ("lfF", "Hz", Q8Processor::lfNames);
    mfG = &addKnob ("mfG", "MF"); mfF = &addCombo ("mfF", "Hz", Q8Processor::mfNames);
    hfG = &addKnob ("hfG", "HF"); hfF = &addCombo ("hfF", "Hz", Q8Processor::hfNames);
    compOn = &addToggle ("compOn", "Compressor");
    thr    = &addKnob ("thr", "Thresh");
    ratio  = &addCombo ("ratio", "Ratio", Q8Processor::ratioNames);
    att    = &addKnob ("att", "Attack");
    rel    = &addKnob ("rel", "Release");
    makeup = &addKnob ("makeup", "Makeup");
    mix    = &addKnob ("mix", "Mix");
    outGain = &addKnob ("outGain", "Output");

    setSize (1000, 300);
    startTimerHz (20);
}

void Q8Editor::paint (Graphics& g)
{
    g.fillAll (bg);
    g.setColour (accent);
    g.setFont (Font (FontOptions (18.f, Font::bold)));
    g.drawText ("Q8 STRIP", 12, 6, 200, 24, Justification::left);
    g.setColour (text.withAlpha (0.6f));
    g.setFont (Font (FontOptions (11.f)));
    g.drawText ("transformer / discrete preamp / 3-band LC EQ (from MM-310 BL schematic) / compressor",
                140, 10, 700, 16, Justification::left);

    auto sec = [&] (Rectangle<int> r, const String& name)
    {
        g.setColour (panel);        g.fillRoundedRectangle (r.toFloat(), 6.f);
        g.setColour (text.withAlpha (0.5f)); g.setFont (Font (FontOptions (11.f, Font::bold)));
        g.drawText (name, r.getX() + 8, r.getBottom() - 18, r.getWidth() - 16, 14, Justification::centredLeft);
    };
    sec (secPre, "PREAMP"); sec (secHpf, "FILTER"); sec (secEq, "EQUALISER"); sec (secComp, "COMPRESSOR"); sec (secOut, "OUT");

    // gain-reduction bar
    g.setColour (Colours::black.withAlpha (0.4f)); g.fillRect (grBar);
    const float gr = jlimit (0.f, 20.f, proc.grMeterDb.load());
    g.setColour (accent);
    g.fillRect (grBar.withHeight ((int) (grBar.getHeight() * gr / 20.f)));
    g.setColour (text); g.setFont (Font (FontOptions (10.f)));
    g.drawText ("GR", grBar.getX() - 6, grBar.getBottom() + 2, grBar.getWidth() + 12, 12, Justification::centred);
}

void Q8Editor::resized()
{
    const int top = 40, h = 236, kw = 70, kh = 92;
    auto placeKnob = [&] (Knob* k, int x, int y)
    {
        k->l.setBounds (x, y, kw, 14);
        k->s.setBounds (x, y + 14, kw, kh);
    };
    auto placeCombo = [&] (Combo* c, int x, int y, int w = 62)
    {
        c->l.setBounds (x, y, w, 14);
        c->c.setBounds (x, y + 16, w, 22);
    };

    secPre  = { 10,  top, 170, h };
    secHpf  = { 188, top, 84,  h };
    secEq   = { 280, top, 250, h };
    secComp = { 538, top, 340, h };
    secOut  = { 886, top, 100, h };

    preOn->b.setBounds (secPre.getX() + 10, top + 8, 120, 22);
    placeKnob (inGain, secPre.getX() + 8, top + 40);
    placeKnob (xfmr,   secPre.getX() + 88, top + 40);

    placeCombo (hpf, secHpf.getX() + 11, top + 60);

    eqOn->b.setBounds (secEq.getX() + 10, top + 8, 80, 22);
    int x = secEq.getX() + 8;
    Knob* gk[] { lfG, mfG, hfG };
    Combo* fc[] { lfF, mfF, hfF };
    for (int i = 0; i < 3; ++i)
    {
        placeKnob (gk[i], x, top + 34);
        placeCombo (fc[i], x + 4, top + 34 + 14 + kh + 2);
        x += 78;
    }

    compOn->b.setBounds (secComp.getX() + 10, top + 8, 120, 22);
    x = secComp.getX() + 8;
    placeKnob (thr, x, top + 34);     x += 64;
    placeKnob (att, x, top + 34);     x += 64;
    placeKnob (rel, x, top + 34);     x += 64;
    placeKnob (makeup, x, top + 34);  x += 64;
    placeKnob (mix, x, top + 34);
    placeCombo (ratio, secComp.getX() + 12, top + 34 + 14 + kh + 2, 70);
    grBar = { secComp.getRight() - 22, top + 12, 10, 24 };
    grBar = { secComp.getRight() - 24, top + 8, 12, 24 };

    placeKnob (outGain, secOut.getX() + 15, top + 34);
}
