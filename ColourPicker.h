#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "OctoSettings.h"

// Nuancier d'Octopus : couleurs rapides, rangée « MY COLOURS » (clic sur un emplacement vide = enregistre la couleur,
// clic droit = efface), teinte, saturation et luminosité, puis l'opacité du halo (GLOW : seulement la lumière autour
// des lettres / du symbole de l'accordeur, pas la couleur). La couleur s'applique en direct
// (étiquette, lettre, anneau, case), OK ferme ; « Default » rend la couleur par défaut quand elle existe.
class OctoColourPicker : public juce::Component
{
public:
    OctoColourPicker (const juce::String& titleText, juce::Colour start, float scale, bool withDefault)
        : title (titleText), k (scale), hasDefault (withDefault)
    {
        setHsv (start);
        setSize (juce::roundToInt (kW * k), juce::roundToInt (kH * k));
    }

    std::function<void (juce::Colour)> onChange;
    std::function<void()> onDefault;
    std::function<void()> onClose;
    std::function<void (int)> onGlow;                   // opacité du halo en % (0-200, 100 = normal)
    void setGlow (int pc) { glow = juce::jlimit (0.0f, 1.0f, (float) pc / 200.0f); repaint(); }

    void setUserForTest (const juce::Array<juce::uint32>& u, int hover) { user = u; hoverUser = hover; repaint(); }
    juce::Colour colour() const { return juce::Colour::fromHSV (hue, sat, bri, 1.0f); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        g.setColour (Colour (0xff10131a));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f * k);
        g.setColour (Colour (0xff3a414d));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f * k, 1.0f);

        g.setColour (Colour (0xffeef1f5));
        g.setFont (FontOptions (13.5f * k, Font::bold));
        g.drawText (title.toUpperCase(), R (14, 10, kW - 28, 18), Justification::centredLeft, true);

        // couleurs rapides
        for (int i = 0; i < kNumQuick; ++i)
        {
            const auto c = R (swatch (i));
            g.setColour (Colour (kQuick[i]));
            g.fillEllipse (c);
            const bool sel = colour().getARGB() == kQuick[i];
            g.setColour (sel ? Colours::white : Colours::white.withAlpha (0.18f));
            g.drawEllipse (c.expanded (sel ? 2.0f * k : 0.0f), sel ? 1.8f * k : 1.0f * k);
        }

        // mes couleurs : emplacements vides en pointillés (clic = enregistrer la couleur en cours)
        g.setColour (Colour (0xff8d96a4));
        g.setFont (FontOptions (10.5f * k, Font::bold));
        g.drawText ("MY COLOURS", R (16, 63, 140, 13), Justification::centredLeft);
        g.drawText ("click empty : save  -  right-click : remove", R (110, 63, kW - 126, 13), Justification::centredRight);
        for (int i = 0; i < kNumQuick; ++i)
        {
            const auto c = R (userSlot (i));
            const auto v = user[i];
            if (v != 0)
            {
                g.setColour (Colour (v));
                g.fillEllipse (c);
                const bool sel = colour().getARGB() == v;
                g.setColour (sel ? Colours::white : Colours::white.withAlpha (0.18f));
                g.drawEllipse (c.expanded (sel ? 2.0f * k : 0.0f), sel ? 1.8f * k : 1.0f * k);
                continue;
            }
            Path ring; ring.addEllipse (c.reduced (0.6f * k));
            Path dashed;
            const float dash[] = { 2.2f * k, 2.2f * k };
            PathStrokeType (1.0f * k).createDashedStroke (dashed, ring, dash, 2);
            g.setColour (hoverUser == i ? Colours::white.withAlpha (0.7f) : Colour (0xff59616e));
            g.fillPath (dashed);
            if (hoverUser == i)
            {
                g.setFont (FontOptions (14.0f * k, Font::bold));
                g.drawText ("+", c.translated (0.0f, -0.5f * k), Justification::centred);
            }
        }

        // barres teinte / saturation / luminosité
        const char* names[4] = { "HUE", "SATURATION", "BRIGHTNESS", "GLOW (HALO OPACITY)" };
        const float vals[4] = { hue, sat, bri, glow };
        for (int b = 0; b < 4; ++b)
        {
            const auto br = R (bar (b));
            g.setColour (Colour (0xff8d96a4));
            g.setFont (FontOptions (10.5f * k, Font::bold));
            g.drawText (names[b], R (bar (b).translated (0.0f, -15.0f).withHeight (13.0f)), Justification::centredLeft);
            g.drawText (String (roundToInt (vals[b] * (b == 0 ? 360.0f : b == 3 ? 200.0f : 100.0f))) + (b == 0 ? String (CharPointer_UTF8 ("\xc2\xb0")) : String (" %")),
                        R (bar (b).translated (0.0f, -15.0f).withHeight (13.0f)), Justification::centredRight);
            Path clip; clip.addRoundedRectangle (br, br.getHeight() * 0.5f);
            g.saveState();
            g.reduceClipRegion (clip);
            if (b == 0)
            {
                ColourGradient cg (Colour::fromHSV (0.0f, 1.0f, 1.0f, 1.0f), br.getX(), 0.0f, Colour::fromHSV (0.9999f, 1.0f, 1.0f, 1.0f), br.getRight(), 0.0f, false);
                for (int i = 1; i < 12; ++i) cg.addColour ((double) i / 12.0, Colour::fromHSV ((float) i / 12.0f, 1.0f, 1.0f, 1.0f));
                g.setGradientFill (cg);
            }
            else if (b == 1)
                g.setGradientFill (ColourGradient (Colour::fromHSV (hue, 0.0f, jmax (0.35f, bri), 1.0f), br.getX(), 0.0f,
                                                   Colour::fromHSV (hue, 1.0f, jmax (0.35f, bri), 1.0f), br.getRight(), 0.0f, false));
            else if (b == 2)
                g.setGradientFill (ColourGradient (Colours::black, br.getX(), 0.0f, Colour::fromHSV (hue, sat, 1.0f, 1.0f), br.getRight(), 0.0f, false));
            else                                       // halo : de rien à une lumière forte de la couleur choisie
                g.setGradientFill (ColourGradient (Colour (0xff1a1e26), br.getX(), 0.0f, colour().interpolatedWith (Colours::white, 0.25f), br.getRight(), 0.0f, false));
            g.fillRect (br);
            g.restoreState();
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawRoundedRectangle (br, br.getHeight() * 0.5f, 1.0f);
            // curseur
            const float x = br.getX() + vals[b] * br.getWidth();
            const auto knob = Rectangle<float> (br.getHeight() + 4.0f * k, br.getHeight() + 4.0f * k).withCentre ({ jlimit (br.getX() + 4.0f * k, br.getRight() - 4.0f * k, x), br.getCentreY() });
            g.setColour (b == 0 ? Colour::fromHSV (hue, 1.0f, 1.0f, 1.0f) : b == 1 ? Colour::fromHSV (hue, sat, jmax (0.35f, bri), 1.0f)
                                : b == 3 ? Colour (0xff1a1e26).interpolatedWith (colour(), glow) : colour());
            g.fillEllipse (knob);
            g.setColour (Colours::white);
            g.drawEllipse (knob, 2.0f * k);
            g.setColour (Colours::black.withAlpha (0.5f));
            g.drawEllipse (knob.expanded (1.2f * k), 1.0f * k);
        }

        // aperçu + code + boutons
        // repère du halo par défaut (100 %)
        {
            const auto br = R (bar (3));
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillRect (Rectangle<float> (1.0f * k, br.getHeight() + 6.0f * k).withCentre ({ br.getCentreX(), br.getCentreY() }));
        }
        const auto pv = R (14, 264, 92, 34);
        g.setColour (colour());
        g.fillRoundedRectangle (pv, 6.0f * k);
        g.setColour (Colours::white.withAlpha (0.2f));
        g.drawRoundedRectangle (pv, 6.0f * k, 1.0f);
        g.setColour (colour().getPerceivedBrightness() > 0.6f ? Colours::black : Colours::white);
        g.setFont (FontOptions (12.5f * k, Font::bold));
        g.drawText ("#" + colour().toDisplayString (false), pv, Justification::centred);
        if (hasDefault) button (g, defaultRect(), "DEFAULT", false);
        button (g, okRect(), "OK", true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position / k;
        drag = -1;
        for (int i = 0; i < kNumQuick; ++i)
            if (swatch (i).expanded (3.0f).contains (p)) { setHsv (juce::Colour (kQuick[i])); changed(); return; }
        for (int i = 0; i < kNumQuick; ++i)
            if (userSlot (i).expanded (3.0f).contains (p))
            {
                if (e.mods.isPopupMenu()) { if (user[i] != 0) { user.set (i, 0); OctoSettings::setUserColours (user); repaint(); } }
                else if (user[i] == 0) { user.set (i, colour().getARGB()); OctoSettings::setUserColours (user); repaint(); }
                else { setHsv (juce::Colour (user[i])); changed(); }
                return;
            }
        for (int b = 0; b < 4; ++b)
            if (bar (b).expanded (6.0f, 8.0f).contains (p)) { drag = b; mouseDrag (e); return; }
        if (hasDefault && defaultRect().contains (p)) { if (onDefault) onDefault(); close(); return; }
        if (okRect().contains (p)) close();
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (int i = 0; i < kNumQuick; ++i) if (user[i] == 0 && userSlot (i).expanded (3.0f).contains (e.position / k)) h = i;
        if (h != hoverUser) { hoverUser = h; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { if (hoverUser >= 0) { hoverUser = -1; repaint(); } }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        const auto br = bar (drag);
        const float v = juce::jlimit (0.0f, 1.0f, (e.position.x / k - br.getX()) / br.getWidth());
        if (drag == 3)
        {
            glow = std::abs (v - 0.5f) < 0.02f ? 0.5f : v;   // aimanté sur 100 %
            if (onGlow) onGlow (juce::roundToInt (glow * 200.0f));
            repaint();
            return;
        }
        if (drag == 0) hue = juce::jmin (v, 0.9999f); else if (drag == 1) sat = v; else bri = v;
        changed();
    }

private:
    static constexpr float kW = 320.0f, kH = 312.0f;
    static constexpr int kNumQuick = 11;
    static constexpr juce::uint32 kQuick[kNumQuick] = { 0xff6fc8ff, 0xff3ce0e8, 0xff45e070, 0xffb6e83a, 0xffffe23a, 0xffffb547,
                                                       0xffff7a2e, 0xffff4040, 0xffff5fb4, 0xffb07aff, 0xfff2f4f8 };
    juce::String title;
    float k = 1.0f, hue = 0.0f, sat = 1.0f, bri = 1.0f, glow = 0.5f;
    bool hasDefault = false;
    int drag = -1, hoverUser = -1;
    juce::Array<juce::uint32> user = OctoSettings::userColours();

    juce::Rectangle<float> R (float x, float y, float w, float h) const { return { x * k, y * k, w * k, h * k }; }
    juce::Rectangle<float> R (juce::Rectangle<float> u) const { return u * k; }
    static juce::Rectangle<float> swatch (int i) { return { 16.0f + 27.0f * (float) i, 36.0f, 20.0f, 20.0f }; }
    static juce::Rectangle<float> bar (int b) { return { 16.0f, 128.0f + 34.0f * (float) b, kW - 32.0f, 12.0f }; }
    static juce::Rectangle<float> userSlot (int i) { return { 16.0f + 27.0f * (float) i, 80.0f, 20.0f, 20.0f }; }
    static juce::Rectangle<float> okRect() { return { kW - 14.0f - 70.0f, 264.0f, 70.0f, 34.0f }; }
    static juce::Rectangle<float> defaultRect() { return { kW - 14.0f - 70.0f - 8.0f - 96.0f, 264.0f, 96.0f, 34.0f }; }

    void button (juce::Graphics& g, juce::Rectangle<float> u, const juce::String& t, bool primary) const
    {
        using namespace juce;
        const auto r = R (u);
        g.setColour (primary ? Colour (0xff1d2a36) : Colour (0xff1a1e26));
        g.fillRoundedRectangle (r, 6.0f * k);
        g.setColour (primary ? Colour (0xff4fc3f7) : Colour (0xff3a414d));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f * k, 1.2f * k);
        g.setColour (primary ? Colour (0xffeef1f5) : Colour (0xff8d96a4));
        g.setFont (FontOptions (12.5f * k, Font::bold));
        g.drawText (t, r, Justification::centred);
    }

    void setHsv (juce::Colour c)
    {
        float h = 0, s = 0, v = 0;
        c.getHSB (h, s, v);
        if (s > 0.001f) hue = h;                       // gris : on garde la teinte en cours
        sat = s; bri = v;
    }
    void changed() { if (onChange) onChange (colour()); repaint(); }
    void close() { if (onClose) onClose(); }
};

// calque posé sur toute la fenêtre d'Octopus : légèrement assombri, le nuancier près de ce qu'il colore ;
// un clic à côté ferme (dans la fenêtre du plugin, sans fenêtre flottante que l'hôte pourrait cacher)
class OctoPickerLayer : public juce::Component
{
public:
    OctoPickerLayer (std::unique_ptr<OctoColourPicker> p, juce::Rectangle<int> near, juce::Rectangle<int> bounds)
        : picker (std::move (p))
    {
        setBounds (bounds);
        addAndMakeVisible (*picker);
        auto r = picker->getBounds().withPosition (near.getCentreX() - picker->getWidth() / 2, near.getBottom() + 8);
        if (r.getBottom() > getHeight() - 6) r.setY (near.getY() - 8 - r.getHeight());
        r.setX (juce::jlimit (6, juce::jmax (6, getWidth() - r.getWidth() - 6), r.getX()));
        r.setY (juce::jlimit (6, juce::jmax (6, getHeight() - r.getHeight() - 6), r.getY()));
        picker->setBounds (r);
        juce::Component::SafePointer<OctoPickerLayer> sp (this);
        picker->onClose = [sp] { if (sp != nullptr) sp->dismiss(); };
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.28f));
        const auto r = picker->getBounds().toFloat();
        for (int i = 1; i <= 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.07f));
            g.fillRoundedRectangle (r.expanded ((float) i * 2.0f).translated (0.0f, 3.0f), 10.0f + (float) i * 2.0f);
        }
    }
    void mouseDown (const juce::MouseEvent&) override { dismiss(); }
    OctoColourPicker& colourPicker() { return *picker; }
    void dismiss()
    {
        setVisible (false);
        juce::Component::SafePointer<OctoPickerLayer> sp (this);
        juce::MessageManager::callAsync ([sp] { if (sp != nullptr) delete sp.getComponent(); });
    }
private:
    std::unique_ptr<OctoColourPicker> picker;
};
