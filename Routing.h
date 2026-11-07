#pragma once
#include <juce_core/juce_core.h>
#include "ChainHost.h"

// Voies parallèles (split / merge) : quatre petits effets JS de NINE HANDS | DSP, écrits par Octopus dans le dossier
// Effects de REAPER (Effects/NINE HANDS/Routing), placés dans la chaîne autour des blocs :
//
//     Split  [voie A ...]  Path B  [voie B ...]  (Path C  [voie C ...])  Merge
//
// Tous les plugins restent sur les canaux 1/2 (aucun routage de broches) : Split copie l'entrée sur 3/4, 5/6 (voies B, C)
// et 7/8 (signal sec, pour couper le groupe) ; Path B échange 1/2 et 3/4 (la voie A est rangée, la voie B arrive sur 1/2),
// Path C échange 1/2 et 5/6 ; Merge mélange les voies (niveau, pan, phase, mute) sur 1/2 et vide les autres canaux.
// La piste passe à 8 canaux. Les voies sont donc simplement les tronçons de la chaîne entre ces repères : déplacer un bloc
// d'une voie à l'autre, c'est le déplacer dans la chaîne, et les presets d'Octopus les gardent sans rien de plus.
namespace Routing
{
    enum Role { none = 0, split, pathB, pathC, merge };
    constexpr int kMaxLanes = 3, kChannels = 8;

    inline const char* fileName (int r)
    {
        switch (r) { case split: return "nh_split.jsfx"; case pathB: return "nh_path_b.jsfx"; case pathC: return "nh_path_c.jsfx"; case merge: return "nh_merge.jsfx"; default: return ""; }
    }
    inline juce::String addName (int r) { return juce::String ("JS:NINE HANDS/Routing/") + fileName (r); }

    inline int roleOf (const FxInfo& f)
    {
        const auto s = (f.original + "|" + f.name).toLowerCase();
        if (s.contains ("nh_split") || s.contains ("nine hands split")) return split;
        if (s.contains ("nh_path_b") || s.contains ("nine hands path b")) return pathB;
        if (s.contains ("nh_path_c") || s.contains ("nine hands path c")) return pathC;
        if (s.contains ("nh_merge") || s.contains ("nine hands merge")) return merge;
        return none;
    }
    inline int markerRole (int lane) { return lane == 1 ? pathB : pathC; }

    // paramètres de Merge (curseurs du JS, dans l'ordre)
    enum MixParam { mixLevel = 0, mixPan, mixPhase, mixMute, mixPresent, kMixPerLane };
    constexpr int pPaths = 0, pGroupOn = 1, pLane0 = 2;
    inline int laneParam (int lane, int what) { return pLane0 + lane * kMixPerLane + what; }
    // valeurs réelles <-> valeurs normalisées de REAPER (0..1 sur l'étendue du curseur)
    inline double toNorm (int what, double v)
    {
        switch (what)
        {
            case mixLevel: return juce::jlimit (0.0, 1.0, (v + 60.0) / 72.0);       // -60 .. +12 dB
            case mixPan:   return juce::jlimit (0.0, 1.0, (v + 100.0) / 200.0);     // -100 (L) .. +100 (R)
            default:       return v >= 0.5 ? 1.0 : 0.0;
        }
    }
    inline double fromNorm (int what, double n)
    {
        switch (what)
        {
            case mixLevel: return std::round ((n * 72.0 - 60.0) * 10.0) / 10.0;
            case mixPan:   return std::round (n * 200.0 - 100.0);
            default:       return n >= 0.5 ? 1.0 : 0.0;
        }
    }
    inline double pathsNorm (int lanes) { return lanes >= 3 ? 1.0 : 0.0; }        // curseur 2..3

    inline juce::String source (int r)
    {
        juce::String pins;
        for (int c = 1; c <= kChannels; ++c) pins << "in_pin:" << c << "\n";
        for (int c = 1; c <= kChannels; ++c) pins << "out_pin:" << c << "\n";
        switch (r)
        {
            case split:
                return "desc:Nine Hands Split\n// Octopus (NINE HANDS | DSP) : début de voies parallèles. Copie l'entrée pour les voies B, C et le signal sec.\n"
                       "//tags: utility routing\n" + pins + "options:no_meter\n@sample\n"
                       "spl2 = spl4 = spl6 = spl0;\nspl3 = spl5 = spl7 = spl1;\n";
            case pathB:
                return "desc:Nine Hands Path B\n// Octopus (NINE HANDS | DSP) : fin de la voie A, début de la voie B.\n"
                       "//tags: utility routing\n" + pins + "options:no_meter\n@sample\n"
                       "t = spl0; spl0 = spl2; spl2 = t;\nt = spl1; spl1 = spl3; spl3 = t;\n";
            case pathC:
                return "desc:Nine Hands Path C\n// Octopus (NINE HANDS | DSP) : fin de la voie B, début de la voie C.\n"
                       "//tags: utility routing\n" + pins + "options:no_meter\n@sample\n"
                       "t = spl0; spl0 = spl4; spl4 = t;\nt = spl1; spl1 = spl5; spl5 = t;\n";
            case merge:
            {
                juce::String s = "desc:Nine Hands Merge\n// Octopus (NINE HANDS | DSP) : mélange des voies parallèles.\n//tags: utility routing\n"
                                 "slider1:2<2,3,1>Paths\nslider2:1<0,1,1{Off (dry),On}>Group\n";
                int n = 3;
                for (auto L : { "A", "B", "C" })
                {
                    s << "slider" << n++ << ":0<-60,12,0.1>" << L << " level (dB)\n";
                    s << "slider" << n++ << ":0<-100,100,1>" << L << " pan\n";
                    s << "slider" << n++ << ":0<0,1,1{Normal,Inverted}>" << L << " phase\n";
                    s << "slider" << n++ << ":0<0,1,1{Off,On}>" << L << " mute\n";
                    s << "slider" << n++ << ":1<0,1,1{No,Yes}>" << L << " present\n";
                }
                s << pins << "options:no_meter\n"
                     "@init\nga = gb = gc = 0; gg = 1;\n"
                     "function lv (db, ph, mu, pr) ( (mu || ! pr) ? 0 : (db <= -59.9 ? 0 : 10 ^ (db / 20)) * (ph ? -1 : 1) );\n"
                     "@slider\n"
                     "ta = lv (slider3, slider5, slider6, slider7);   pla = min (1, 1 - slider4 / 100);  pra = min (1, 1 + slider4 / 100);\n"
                     "tb = lv (slider8, slider10, slider11, slider12); plb = min (1, 1 - slider9 / 100);  prb = min (1, 1 + slider9 / 100);\n"
                     "tc = slider1 >= 2.5 ? lv (slider13, slider15, slider16, slider17) : 0; plc = min (1, 1 - slider14 / 100); prc = min (1, 1 + slider14 / 100);\n"
                     "tg = slider2 >= 0.5 ? 1 : 0;\n"
                     "k = 1 - exp (-1 / (0.01 * srate));\n"
                     "@sample\n"
                     "ga += (ta - ga) * k; gb += (tb - gb) * k; gc += (tc - gc) * k; gg += (tg - gg) * k;\n"
                     "slider1 >= 2.5 ? ( bl = spl4; br = spl5; cl = spl0; cr = spl1; ) : ( bl = spl0; br = spl1; cl = cr = 0; );\n"
                     "wl = ga * pla * spl2 + gb * plb * bl + gc * plc * cl;\n"
                     "wr = ga * pra * spl3 + gb * prb * br + gc * prc * cr;\n"
                     "spl0 = wl * gg + spl6 * (1 - gg);\nspl1 = wr * gg + spl7 * (1 - gg);\n"
                     "spl2 = spl3 = spl4 = spl5 = spl6 = spl7 = 0;\n";
                return s;
            }
            default: return {};
        }
    }

    // écrit (ou met à jour) les quatre effets JS dans le dossier donné ; false si impossible
    inline bool install (const juce::File& folder)
    {
        if (folder == juce::File()) return false;
        if (! folder.isDirectory() && ! folder.createDirectory()) return false;
        bool ok = true;
        for (int r = split; r <= merge; ++r)
        {
            const auto f = folder.getChildFile (fileName (r));
            const auto src = source (r);
            if (! f.existsAsFile() || f.loadFileAsString() != src) ok = f.replaceWithText (src, false, false, "\n") && ok;
        }
        return ok;
    }
}
