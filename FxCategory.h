#pragma once
#include <juce_graphics/juce_graphics.h>

// Famille d'un effet, devinée d'après son nom (icône et couleur du bloc à l'écran, rubriques du navigateur d'effets).
namespace FxCat
{
    enum Kind { tuner, amp, cab, drive, dynamics, eq, filter, delay, reverb, mod, pitch, special, utility, other, numKinds };

    inline const char* label (int k)
    {
        static const char* n[numKinds] = { "Tuner", "Amp", "Cab / IR", "Drive", "Dynamics", "EQ", "Wah / Filter",
                                            "Delay", "Reverb", "Modulation", "Pitch", "Special", "Utility", "Other" };
        return n[juce::jlimit (0, numKinds - 1, k)];
    }

    inline juce::Colour colour (int k)
    {
        static const juce::uint32 c[numKinds] = {
            0xff33c8e8, // tuner
            0xffd9443b, // amp
            0xffb0703f, // cab
            0xffe8742c, // drive
            0xff4cb85c, // dynamics
            0xff9cc843, // eq
            0xffe6c531, // filter / wah
            0xff1fb5a6, // delay
            0xff3f7fe0, // reverb
            0xff9a5be0, // modulation
            0xffd9479a, // pitch
            0xffc77ef0, // special
            0xffeef1f5, // utility : blanc (le gris se confondait avec un bloc éteint)
            0xffd6e2ee  // other : blanc légèrement bleuté
        };
        return juce::Colour (c[juce::jlimit (0, numKinds - 1, k)]);
    }

    // "VST3: Holyverb (NINE HANDS | DSP)" -> "Holyverb" ; "JS: Utility/volume" -> "volume"
    inline juce::String shortName (const juce::String& full)
    {
        auto s = full.trim();
        const int colon = s.indexOfChar (':');
        if (colon > 0 && colon < 7 && ! s.substring (0, colon).containsChar (' '))
            s = s.substring (colon + 1).trim();
        if (s.endsWithChar (')'))
        {
            const int open = s.lastIndexOfChar ('(');
            if (open > 0) s = s.substring (0, open).trim();
        }
        return s.isEmpty() ? full : s;
    }

    // "VST3: Holyverb (NINE HANDS | DSP)" -> "NINE HANDS | DSP"
    inline juce::String vendor (const juce::String& full)
    {
        auto s = full.trim();
        if (! s.endsWithChar (')')) return {};
        const int open = s.lastIndexOfChar ('(');
        return open > 0 ? s.substring (open + 1, s.length() - 1).trim() : juce::String();
    }

    // plugins de la marque : NINE HANDS | DSP, ou NINE HANDS | DSP pour les anciennes versions
    inline bool isOwnVendor (const juce::String& v)
    {
        return v.equalsIgnoreCase ("NINE HANDS | DSP") || v.equalsIgnoreCase ("Studio Wiga");
    }

    // "VST3: x" -> "VST3"
    inline juce::String typeOf (const juce::String& full)
    {
        const int colon = full.indexOfChar (':');
        return colon > 0 && colon < 7 ? full.substring (0, colon).trim() : juce::String();
    }

    inline int classify (const juce::String& fullName)
    {
        const auto name = shortName (fullName).toLowerCase();
        juce::StringArray words;
        words.addTokens (name, " -_/.,()[]+:&", "");
        words.removeEmptyStrings();

        auto has = [&] (std::initializer_list<const char*> keys)          // sous-chaîne (mots-clés longs)
        {
            for (auto* k : keys) if (name.contains (k)) return true;
            return false;
        };
        auto word = [&] (std::initializer_list<const char*> keys)         // début de mot (mots-clés courts)
        {
            for (auto& w : words)
                for (auto* k : keys) if (w.startsWith (k)) return true;
            return false;
        };

        // l'ordre compte : du plus spécifique au plus général
        if (has ({ "tuner", "chroma", "polytune" }) || word ({ "tune" }))                                     return tuner;
        if (has ({ "cabinet", "impulse", "reacab", "cab sim" }) || word ({ "cab", "ir", "irs" }))            return cab;
        if (has ({ "ampire", "neural", "archetype", "ampli", "preamp", "guitar rig", "s-gear", "tonex",
                  "bias fx", "amplitube", "mesa", "marshall", "fortin", "nameless", "helix", "anode", "premix" })
            || word ({ "amp", "nam", "head", "stack" }))                                                     return amp;
        if (has ({ "ring think", "textur", "freeze", "ebow", "fantomine", "theremin", "granular", "synth",
                  "sitar", "jawari", "freakout", "freqout", "feedback", "infinite", "fm-9", "to midi", "coilmines", "pickup", "spindeck" }) || word ({ "arc", "ring", "omi" })) return special;
        if (has ({ "whammy", "wharry", "octav", "pitch", "semitone", "harmoni", "capo", "transpos", "fog z", "pog" })
            || word ({ "fog" }))                                                                              return pitch;
        if (has ({ "crycat", "wah", "envelope", "filter", "autowah", "talk" }) || word ({ "cry" }))         return filter;
        if (has ({ "delay", "echo", "tape", "space sonar", "timeless" }))                                                               return delay;
        if (has ({ "verb", "hall", "plate", "shimmer", "room", "space", "ambien", "lascaux", "backdraft" }))                         return reverb;
        if (has ({ "chorus", "flang", "phase", "phaser", "vibrato", "tremolo", "rotary", "leslie", "small tone",
                  "wack", "pipeline", "vibe", "ensemble", "modul", "doubler" }) || word ({ "trem", "uni" }))  return mod;
        if (has ({ "drive", "distort", "fuzz", "screamer", "doorn", "peachy", "darkbass", "thorn", "saturat", "crush", "pickle",
                  "klon", "muff", "boost", "tube", "overdrive", "exciter", "tolana" }) || word ({ "od", "rat", "dist", "sat" })) return drive;
        if (has ({ "compress", "gate", "limit", "expander", "transient", "dynamic", "reacomp", "squeez", "sustain", "grinder", "lmix" })
            || word ({ "comp", "ngate", "509" }))                                                                   return dynamics;
        if (has ({ "equaliz", "reaeq", "graphic", "parametric", "tone stack", "qmix", "mastermix" }) || word ({ "eq" }))          return eq;
        if (has ({ "volume", "utility", "meter", "analy", "scope", "stereo", "width", "routing", "spectr",
                  "gain", "pan", "polarity", "loudness", "loudmix", "mixer", "channel" }))                               return utility;
        return other;
    }

    // icônes propres à un plugin (sinon celle de sa famille)
    // v1.18.96 : une icône pour chaque plugin NINE HANDS | DSP qui avait la prise « effet inconnu »
    enum Icon { iconFamily = 0, iconTube, iconBassHead, iconLunchbox, iconRiser, iconGear, iconCave, iconCeiling, iconVu,
                iconDisc, iconFader, iconEqCurve, iconTurntable, iconHourglass, iconCassette };
    inline int iconOf (const juce::String& fullName)
    {
        const auto n = shortName (fullName).toLowerCase();
        static const std::pair<const char*, int> k[] = {
            { "anode", iconTube }, { "darkbass", iconBassHead }, { "509", iconLunchbox }, { "backdraft", iconRiser },
            { "grinder", iconGear }, { "lascaux", iconCave }, { "lmix", iconCeiling }, { "loudmix", iconVu },
            { "mastermix", iconDisc }, { "premix", iconFader }, { "qmix", iconEqCurve }, { "spindeck", iconTurntable },
            { "timeless", iconHourglass }, { "tolana", iconCassette }, { "audiotape", iconCassette } };
        for (auto& x : k) if (n.contains (x.first)) return x.second;
        return iconFamily;
    }

    // icône vectorielle du bloc (dans le carré r) ; icon : iconOf (nom du plugin)
    inline void drawIcon (juce::Graphics& g, int kind, juce::Rectangle<float> r, juce::Colour c, float stroke, int icon = iconFamily)
    {
        using namespace juce;
        if (icon == iconTube)           // Anode : lampe (ampoule à dôme, électrodes, culot et broches), dessinée sur une grille 72 x 72
        {
            g.setColour (c);
            const float k = r.getWidth() / 72.0f;
            auto P = [&] (float x, float y) { return Point<float> (r.getX() + x * k, r.getY() + y * k); };
            PathStrokeType st (stroke, PathStrokeType::curved, PathStrokeType::rounded);
            Path p;
            p.startNewSubPath (P (24, 52)); p.lineTo (P (24, 24));
            p.addCentredArc (P (36, 24).x, P (36, 24).y, 12 * k, 12 * k, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi);
            p.lineTo (P (48, 52));
            p.startNewSubPath (P (30, 44)); p.lineTo (P (30, 30)); p.lineTo (P (42, 30)); p.lineTo (P (42, 44));
            for (float x : { 28.0f, 36.0f, 44.0f }) { p.startNewSubPath (P (x, 61)); p.lineTo (P (x, 67)); }
            g.strokePath (p, st);
            g.drawRoundedRectangle (Rectangle<float> (P (20, 52), P (52, 61)), 2.0f * k, stroke);
            return;
        }
        if (icon >= iconLunchbox)       // icônes propres dessinées sur une grille 24 x 24
        {
            g.setColour (c);
            const float k = r.getWidth() / 24.0f;
            auto P = [&] (float x, float y) { return Point<float> (r.getX() + x * k, r.getY() + y * k); };
            auto R = [&] (float x, float y, float w, float h) { return Rectangle<float> (r.getX() + x * k, r.getY() + y * k, w * k, h * k); };
            auto dot = [&] (float x, float y, float d) { g.fillEllipse (Rectangle<float> (d * k, d * k).withCentre (P (x, y))); };
            PathStrokeType st (stroke, PathStrokeType::curved, PathStrokeType::rounded);
            Path p;
            switch (icon)
            {
                case iconLunchbox:      // 509 : châssis série 500, 4 modules avec potard et curseur
                    g.drawRoundedRectangle (R (2.5f, 4.0f, 19.0f, 16.0f), 1.6f * k, stroke);
                    for (float x : { 7.25f, 12.0f, 16.75f }) { p.startNewSubPath (P (x, 4.0f)); p.lineTo (P (x, 20.0f)); }
                    for (float x : { 4.9f, 9.6f, 14.4f, 19.1f }) { dot (x, 8.5f, 2.6f); p.startNewSubPath (P (x, 12.5f)); p.lineTo (P (x, 17.0f)); }
                    g.strokePath (p, st); return;
                case iconRiser:         // Backdraft : montée (soufflet de crescendo) vers un mur
                    p.startNewSubPath (P (20.0f, 5.0f)); p.lineTo (P (3.0f, 12.0f)); p.lineTo (P (20.0f, 19.0f));
                    p.startNewSubPath (P (21.5f, 4.0f)); p.lineTo (P (21.5f, 20.0f));
                    g.strokePath (p, st);
                    for (int i = 0; i < 3; ++i) { const float x = 9.0f + 4.0f * (float) i, hh = 1.2f + 1.5f * (float) i;
                                                  Path b; b.startNewSubPath (P (x, 12.0f - hh)); b.lineTo (P (x, 12.0f + hh)); g.strokePath (b, st); }
                    return;
                case iconGear:          // Grinder : roue dentée (8 dents)
                {
                    for (int i = 0; i < 8; ++i)
                    {
                        const float a = MathConstants<float>::twoPi * (float) i / 8.0f;
                        p.startNewSubPath (P (12.0f + 6.6f * std::cos (a), 12.0f + 6.6f * std::sin (a)));
                        p.lineTo (P (12.0f + 9.4f * std::cos (a), 12.0f + 9.4f * std::sin (a)));
                    }
                    g.strokePath (p, PathStrokeType (stroke * 1.6f, PathStrokeType::curved, PathStrokeType::rounded));
                    g.drawEllipse (R (5.4f, 5.4f, 13.2f, 13.2f), stroke);
                    g.drawEllipse (R (9.5f, 9.5f, 5.0f, 5.0f), stroke);
                    return;
                }
                case iconCave:          // Lascaux : voûte de grotte, échos à l'intérieur, sol
                    p.startNewSubPath (P (2.5f, 20.0f)); p.lineTo (P (21.5f, 20.0f));
                    p.startNewSubPath (P (4.0f, 20.0f)); p.cubicTo (P (4.0f, 6.0f), P (20.0f, 6.0f), P (20.0f, 20.0f));
                    g.strokePath (p, st);
                    { Path q; q.startNewSubPath (P (8.5f, 20.0f)); q.cubicTo (P (8.5f, 12.0f), P (15.5f, 12.0f), P (15.5f, 20.0f));
                      g.setColour (c.withMultipliedAlpha (0.6f)); g.strokePath (q, st); g.setColour (c); }
                    dot (12.0f, 17.5f, 2.2f);
                    return;
                case iconCeiling:       // LMix : plafond, la forme d'onde vient s'écraser dessous
                    p.startNewSubPath (P (2.5f, 6.0f)); p.lineTo (P (21.5f, 6.0f));
                    g.strokePath (p, st);
                    { Path w; w.startNewSubPath (P (3.0f, 18.0f)); w.lineTo (P (6.0f, 8.5f)); w.lineTo (P (9.0f, 8.5f)); w.lineTo (P (11.0f, 16.0f));
                      w.lineTo (P (14.0f, 8.5f)); w.lineTo (P (17.0f, 8.5f)); w.lineTo (P (19.0f, 14.0f)); w.lineTo (P (21.0f, 11.0f)); g.strokePath (w, st); }
                    return;
                case iconVu:            // LoudMix : vumètre (cadran, graduations, aiguille)
                    g.drawRoundedRectangle (R (2.5f, 5.0f, 19.0f, 14.0f), 1.6f * k, stroke);
                    p.addCentredArc (P (12.0f, 18.0f).x, P (12.0f, 18.0f).y, 8.0f * k, 8.0f * k, 0.0f, -1.0f, 1.0f, true);
                    p.startNewSubPath (P (12.0f, 17.0f)); p.lineTo (P (15.8f, 9.6f));
                    g.strokePath (p, st);
                    dot (12.0f, 17.0f, 2.2f);
                    return;
                case iconDisc:          // MasterMix : disque (gravure, étiquette, axe)
                    g.drawEllipse (R (2.5f, 2.5f, 19.0f, 19.0f), stroke);
                    g.setColour (c.withMultipliedAlpha (0.55f));
                    g.drawEllipse (R (5.5f, 5.5f, 13.0f, 13.0f), stroke * 0.7f);
                    g.setColour (c);
                    g.drawEllipse (R (9.0f, 9.0f, 6.0f, 6.0f), stroke);
                    dot (12.0f, 12.0f, 1.6f);
                    return;
                case iconFader:         // PreMix : tranche de console (potards et fader)
                    dot (7.0f, 5.0f, 3.0f); dot (7.0f, 10.0f, 3.0f);
                    p.startNewSubPath (P (15.0f, 3.0f)); p.lineTo (P (15.0f, 21.0f));
                    p.startNewSubPath (P (7.0f, 14.0f)); p.lineTo (P (7.0f, 21.0f));
                    g.strokePath (p, st);
                    g.fillRoundedRectangle (R (12.0f, 12.0f, 6.0f, 3.6f), 0.8f * k);
                    return;
                case iconEqCurve:       // QMix : courbe d'égaliseur et ses points
                    p.startNewSubPath (P (2.5f, 15.0f)); p.cubicTo (P (6.0f, 15.0f), P (6.0f, 6.5f), P (9.5f, 6.5f));
                    p.cubicTo (P (13.0f, 6.5f), P (12.0f, 17.5f), P (16.0f, 17.5f)); p.cubicTo (P (19.0f, 17.5f), P (19.0f, 11.0f), P (21.5f, 11.0f));
                    g.strokePath (p, st);
                    dot (9.5f, 6.5f, 3.0f); dot (16.0f, 17.5f, 3.0f);
                    return;
                case iconTurntable:     // Spindeck : platine (plateau et bras)
                    g.drawEllipse (R (2.5f, 4.0f, 15.0f, 15.0f), stroke);
                    dot (10.0f, 11.5f, 2.4f);
                    p.startNewSubPath (P (20.5f, 3.5f)); p.lineTo (P (20.5f, 13.0f)); p.lineTo (P (15.5f, 17.0f));
                    g.strokePath (p, st);
                    dot (20.5f, 3.5f, 2.6f);
                    return;
                case iconHourglass:     // Timeless : sablier
                    p.startNewSubPath (P (6.0f, 3.5f)); p.lineTo (P (18.0f, 3.5f));
                    p.startNewSubPath (P (6.0f, 20.5f)); p.lineTo (P (18.0f, 20.5f));
                    p.startNewSubPath (P (7.5f, 3.5f)); p.cubicTo (P (7.5f, 9.0f), P (12.0f, 10.0f), P (12.0f, 12.0f)); p.cubicTo (P (12.0f, 14.0f), P (7.5f, 15.0f), P (7.5f, 20.5f));
                    p.startNewSubPath (P (16.5f, 3.5f)); p.cubicTo (P (16.5f, 9.0f), P (12.0f, 10.0f), P (12.0f, 12.0f)); p.cubicTo (P (12.0f, 14.0f), P (16.5f, 15.0f), P (16.5f, 20.5f));
                    g.strokePath (p, st);
                    { Path s; s.startNewSubPath (P (9.0f, 19.6f)); s.lineTo (P (12.0f, 16.5f)); s.lineTo (P (15.0f, 19.6f)); s.closeSubPath(); g.fillPath (s); }
                    return;
                case iconCassette:      // Tolana : cassette (deux bobines, fenêtre, bas trapézoïdal)
                    g.drawRoundedRectangle (R (2.0f, 5.0f, 20.0f, 14.0f), 1.6f * k, stroke);
                    g.drawEllipse (R (5.5f, 8.0f, 4.6f, 4.6f), stroke);
                    g.drawEllipse (R (13.9f, 8.0f, 4.6f, 4.6f), stroke);
                    p.startNewSubPath (P (10.1f, 10.3f)); p.lineTo (P (13.9f, 10.3f));
                    p.startNewSubPath (P (6.5f, 19.0f)); p.lineTo (P (7.8f, 15.6f)); p.lineTo (P (16.2f, 15.6f)); p.lineTo (P (17.5f, 19.0f));
                    g.strokePath (p, st);
                    return;
                default: break;
            }
        }
        if (icon == iconBassHead)       // Darkbass : tête de basse, 4 mécaniques (2 + 2), manche et cordes ; grille 24 x 24
        {
            g.setColour (c);
            const float k = r.getWidth() / 24.0f;
            auto P = [&] (float x, float y) { return Point<float> (r.getX() + x * k, r.getY() + y * k); };
            PathStrokeType st (stroke, PathStrokeType::curved, PathStrokeType::rounded);
            Path p;
            p.startNewSubPath (P (9, 21)); p.lineTo (P (9, 10)); p.lineTo (P (7, 4)); p.lineTo (P (17, 4)); p.lineTo (P (15, 10)); p.lineTo (P (15, 21));
            p.startNewSubPath (P (11, 10)); p.lineTo (P (11, 21));
            p.startNewSubPath (P (13, 10)); p.lineTo (P (13, 21));
            g.strokePath (p, st);
            for (auto q : { P (5.4f, 6.6f), P (5.4f, 10.6f), P (18.6f, 6.6f), P (18.6f, 10.6f) })
                g.fillEllipse (Rectangle<float> (2.9f * k, 2.9f * k).withCentre (q));
            return;
        }
        g.setColour (c);
        const auto cx = r.getCentreX(), cy = r.getCentreY(), w = r.getWidth(), h = r.getHeight();
        PathStrokeType st (stroke, PathStrokeType::curved, PathStrokeType::rounded);
        Path p;
        switch (kind)
        {
            case tuner:     // aiguille + graduations
            {
                p.addCentredArc (cx, cy + h * 0.25f, w * 0.38f, w * 0.38f, 0.0f, -1.1f, 1.1f, true);
                g.strokePath (p, st);
                Path n; n.startNewSubPath (cx, cy + h * 0.25f); n.lineTo (cx + w * 0.12f, cy - h * 0.12f);
                g.strokePath (n, st);
                g.fillEllipse (cx - stroke * 1.4f, cy + h * 0.25f - stroke * 1.4f, stroke * 2.8f, stroke * 2.8f);
                return;
            }
            case amp:       // tête d'ampli : boîtier + grille + potards
            {
                auto b = r.reduced (w * 0.08f, h * 0.2f);
                g.drawRoundedRectangle (b, stroke * 1.5f, stroke);
                auto grille = b.removeFromBottom (b.getHeight() * 0.55f).reduced (w * 0.06f, h * 0.05f);
                for (int i = 1; i < 4; ++i)
                {
                    const float y = grille.getY() + grille.getHeight() * (float) i / 4.0f;
                    g.drawLine (grille.getX(), y, grille.getRight(), y, stroke * 0.7f);
                }
                for (int i = 0; i < 4; ++i)
                    g.fillEllipse (b.getX() + w * (0.1f + 0.15f * (float) i), b.getCentreY() - stroke, stroke * 2.0f, stroke * 2.0f);
                return;
            }
            case cab:       // haut-parleur
            {
                auto b = r.reduced (w * 0.12f);
                g.drawRoundedRectangle (b, stroke * 1.5f, stroke);
                g.drawEllipse (b.reduced (w * 0.12f), stroke);
                g.fillEllipse (b.withSizeKeepingCentre (w * 0.14f, w * 0.14f));
                return;
            }
            case drive:     // onde écrêtée (symbole fuzz / distorsion : Doorn, Peachy, Strong Pickle...)
            {
                p.startNewSubPath (r.getX() + w * 0.08f, cy);
                p.lineTo (r.getX() + w * 0.2f, cy - h * 0.26f); p.lineTo (r.getX() + w * 0.42f, cy - h * 0.26f);
                p.lineTo (r.getX() + w * 0.58f, cy + h * 0.26f); p.lineTo (r.getX() + w * 0.8f, cy + h * 0.26f);
                p.lineTo (r.getRight() - w * 0.08f, cy);
                g.strokePath (p, st);
                return;
            }
            case dynamics:  // courbe de compression (coude)
            {
                auto b = r.reduced (w * 0.16f);
                g.drawLine (b.getX(), b.getBottom(), b.getRight(), b.getBottom(), stroke * 0.6f);
                g.drawLine (b.getX(), b.getBottom(), b.getX(), b.getY(), stroke * 0.6f);
                p.startNewSubPath (b.getX(), b.getBottom());
                p.lineTo (b.getX() + b.getWidth() * 0.55f, b.getY() + b.getHeight() * 0.45f);
                p.quadraticTo (b.getX() + b.getWidth() * 0.68f, b.getY() + b.getHeight() * 0.32f, b.getRight(), b.getY() + b.getHeight() * 0.22f);
                g.strokePath (p, st);
                return;
            }
            case eq:        // trois curseurs
            {
                const float ys[3] = { 0.35f, 0.62f, 0.45f };
                for (int i = 0; i < 3; ++i)
                {
                    const float x = r.getX() + w * (0.27f + 0.23f * (float) i);
                    g.drawLine (x, r.getY() + h * 0.18f, x, r.getBottom() - h * 0.18f, stroke * 0.7f);
                    g.fillRoundedRectangle (x - w * 0.07f, r.getY() + h * ys[i] - h * 0.05f, w * 0.14f, h * 0.1f, stroke);
                }
                return;
            }
            case filter:    // courbe en cloche (wah)
            {
                p.startNewSubPath (r.getX() + w * 0.08f, cy + h * 0.25f);
                p.cubicTo (cx - w * 0.14f, cy + h * 0.25f, cx - w * 0.1f, cy - h * 0.32f, cx, cy - h * 0.32f);
                p.cubicTo (cx + w * 0.1f, cy - h * 0.32f, cx + w * 0.14f, cy + h * 0.25f, r.getRight() - w * 0.08f, cy + h * 0.25f);
                g.strokePath (p, st);
                return;
            }
            case delay:     // répétitions décroissantes
            {
                for (int i = 0; i < 4; ++i)
                {
                    const float d = w * (0.2f - 0.035f * (float) i);
                    g.setColour (c.withMultipliedAlpha (1.0f - 0.2f * (float) i));
                    g.fillEllipse (r.getX() + w * (0.16f + 0.21f * (float) i) - d * 0.5f + d * 0.5f, cy - d * 0.5f, d, d);
                }
                return;
            }
            case reverb:    // arcs concentriques
            {
                for (int i = 0; i < 3; ++i)
                {
                    Path a; const float rr = w * (0.14f + 0.13f * (float) i);
                    a.addCentredArc (r.getX() + w * 0.24f, cy, rr, rr, 0.0f, 0.55f, 2.6f, true);
                    g.setColour (c.withMultipliedAlpha (1.0f - 0.22f * (float) i));
                    g.strokePath (a, st);
                }
                g.setColour (c);
                g.fillEllipse (r.getX() + w * 0.24f - stroke * 1.3f, cy - stroke * 1.3f, stroke * 2.6f, stroke * 2.6f);
                return;
            }
            case mod:       // sinusoïde
            {
                p.startNewSubPath (r.getX() + w * 0.08f, cy);
                for (int i = 1; i <= 40; ++i)
                {
                    const float t = (float) i / 40.0f;
                    p.lineTo (r.getX() + w * (0.08f + 0.84f * t), cy - h * 0.24f * std::sin (t * MathConstants<float>::twoPi * 1.5f));
                }
                g.strokePath (p, st);
                return;
            }
            case pitch:     // flèche vers le haut + octave
            {
                p.startNewSubPath (cx - w * 0.12f, cy + h * 0.3f); p.lineTo (cx - w * 0.12f, cy - h * 0.28f);
                p.startNewSubPath (cx - w * 0.28f, cy - h * 0.12f); p.lineTo (cx - w * 0.12f, cy - h * 0.3f); p.lineTo (cx + w * 0.04f, cy - h * 0.12f);
                g.strokePath (p, st);
                g.drawEllipse (cx + w * 0.08f, cy + h * 0.06f, w * 0.2f, h * 0.17f, stroke);
                g.drawLine (cx + w * 0.28f, cy + h * 0.14f, cx + w * 0.28f, cy - h * 0.26f, stroke);
                return;
            }
            case special:   // étoile scintillante
            {
                for (int i = 0; i < 4; ++i)
                {
                    const float a = MathConstants<float>::halfPi * (float) i;
                    const float s = (i % 2 == 0) ? 0.34f : 0.34f;
                    p.startNewSubPath (cx, cy);
                    p.lineTo (cx + std::cos (a) * w * s, cy + std::sin (a) * h * s);
                }
                g.strokePath (p, st);
                g.fillEllipse (cx - w * 0.09f, cy - h * 0.09f, w * 0.18f, h * 0.18f);
                g.fillEllipse (cx + w * 0.2f, cy - h * 0.3f, w * 0.08f, h * 0.08f);
                return;
            }
            case utility:   // vumètre
            {
                const float hs[5] = { 0.3f, 0.55f, 0.75f, 0.5f, 0.35f };
                for (int i = 0; i < 5; ++i)
                {
                    const float bh = h * 0.6f * hs[i];
                    g.fillRoundedRectangle (r.getX() + w * (0.17f + 0.14f * (float) i), r.getBottom() - h * 0.2f - bh, w * 0.09f, bh, stroke * 0.5f);
                }
                return;
            }
            default:        // prise (effet inconnu)
            {
                auto b = r.withSizeKeepingCentre (w * 0.42f, h * 0.34f).translated (0, h * 0.04f);
                g.drawRoundedRectangle (b, stroke * 1.5f, stroke);
                g.drawLine (b.getX() + b.getWidth() * 0.3f, b.getY(), b.getX() + b.getWidth() * 0.3f, b.getY() - h * 0.16f, stroke);
                g.drawLine (b.getRight() - b.getWidth() * 0.3f, b.getY(), b.getRight() - b.getWidth() * 0.3f, b.getY() - h * 0.16f, stroke);
                g.drawLine (cx, b.getBottom(), cx, r.getBottom() - h * 0.1f, stroke);
                return;
            }
        }
    }
}
