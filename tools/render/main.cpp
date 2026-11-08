#include "Routing.h"
#include "../../ColourPicker.h"
#include "../../OctoSettings.h"
#include "../../FxCategory.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

// Captures de l'interface + tests du moteur (presets, rappel différentiel, état) sur une chaîne simulée.
static void pump (int n = 30) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (34); }

static String chainText (OctopusEngine& e)
{
    StringArray s;
    for (auto& f : e.chain()) s.add ((f.isSelf ? "[OCTO]" : "") + f.original.fromFirstOccurrenceOf (": ", false, false).upToFirstOccurrenceOf (" (", false, false) + (f.enabled ? "" : "(off)"));
    return s.joinIntoString (" > ");
}

static int failures = 0;
static void check (bool ok, const String& what) { std::printf ("%s %s\n", ok ? "  OK  " : "  FAIL", what.toRawUTF8()); if (! ok) ++failures; }

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    OctopusProcessor proc;
    auto& e = proc.getEngine();
    e.refreshNow();

    // ---------------------------------------------------------------- tests du moteur
    std::printf ("Tests moteur\n");
    const auto initial = chainText (e);
    check (e.blocks().size() == 10, "10 blocs affichés (Octopus exclu) : " + initial);
    e.save (0);
    check (e.preset (0).saved && e.preset (0).entries.size() == 10 && e.preset (0).selfPos == 0, "1A enregistré (10 effets, Octopus en premier)");
    check (! e.preset (0).entries[0].contains ("FXID"), "FXID retiré des presets");

    e.toggle (e.blocks()[2]);                                   // Crycat ON
    check (e.modified(), "chaîne modifiée -> EDITED");
    e.save (1);
    const auto chainB = chainText (e);
    e.load (0);
    check (chainText (e) == initial, "rappel 1A (même chaîne, ON/OFF seulement) : " + chainText (e));
    e.load (1);
    check (chainText (e) == chainB, "rappel 1B");

    e.remove (e.blocks()[9]);                                   // retire Holyverb
    e.addFx ({ "VST3: Pipeline (NINE HANDS | DSP)", "" }, 0);         // ajoute Pipeline en tête
    e.moveBlock (e.blocks()[0], 3);
    const auto chainC = chainText (e);
    e.toggleAssign (0, e.chain().getReference (e.blocks()[1]).guid);
    e.toggleAssign (0, e.chain().getReference (e.blocks()[2]).guid);
    e.save (2);
    e.load (0);
    check (chainText (e) == initial, "rappel 1A après changement de structure (chaîne remplacée)");
    e.load (2);
    check (chainText (e) == chainC, "rappel 1C : " + chainC);
    e.setMode (OctopusEngine::stompMode);
    check (e.fsText (0).contains ("+"), "footswitch A (STOMP) commande 2 blocs après rappel : " + e.fsText (0));
    e.press (0);
    check (e.fsText (0).isNotEmpty() && ! e.fsLit (0), "STOMP A : les 2 blocs passent OFF");
    e.press (0);
    check (e.fsLit (0), "STOMP A : les 2 blocs repassent ON");
    e.setMode (OctopusEngine::presetMode);

    // état sauvé / relu
    MemoryBlock mb;
    proc.getStateInformation (mb);
    OctopusProcessor proc2;
    proc2.setStateInformation (mb.getData(), (int) mb.getSize());
    auto& e2 = proc2.getEngine();
    check (e2.preset (2).saved && e2.preset (2).entries.size() == e.preset (2).entries.size() && e2.current() == 2,
           "état relu (" + String ((int) mb.getSize()) + " octets)");

    // MIDI : learn + momentané + Program Change
    e.setMode (OctopusEngine::presetMode);
    e.learnTarget = OctopusEngine::fsB;
    e.midiCc (1, 80, 127);
    check (e.ccOf (OctopusEngine::fsB) == 80, "MIDI learn footswitch B = CC 80");
    e.midiCc (1, 80, 0);
    e.midiCc (1, 80, 127);
    check (e.current() == 1, "CC 80 (momentané) -> preset 1B tout de suite, sans attente");
    Thread::sleep (270); e.tick();
    e.midiProgram (1, 0);
    check (e.current() == 0, "Program Change 1 -> preset 1A");
    e.paramInput (OctopusEngine::fsC, 0.0f);
    e.paramInput (OctopusEngine::fsC, 1.0f);
    check (e.current() == 2, "paramètre fsC -> preset 1C tout de suite");
    Thread::sleep (270); e.tick();

    // bypass de la pédale (son propre interrupteur) ou de REAPER
    {
        auto& h = e.chainHost();
        int ec = -1, cc = -1;
        for (int i = 0; i < e.chain().size(); ++i)
        {
            if (e.chain().getReference (i).name.contains ("ECHOFLEX")) ec = i;
            if (OctopusEngine::isChroma (e.chain().getReference (i))) cc = i;
        }
        bool inv = false;
        check (ec >= 0 && e.pedalBypassParam (ec, inv) == 9 && ! inv && e.usesPedalBypass (ec), "ECHOFLEX : interrupteur de la pédale trouvé (Footswitch (On)), « Bypass » de REAPER ignoré");
        check (cc >= 0 && ! e.usesPedalBypass (cc), "CHROMA (sans interrupteur) : bypass de REAPER");
        const String g = e.chain().getReference (ec).guid;
        e.setEnabled (ec, true);
        e.toggle (ec);
        check (h.info (ec).enabled && h.getParam (ec, 9) < 0.5 && ! e.chain().getReference (ec).enabled, "OFF par la pédale : REAPER laisse tourner l'effet (les queues continuent), bloc affiché OFF");
        e.save (12);                                         // 4A : ECHOFLEX éteint par sa pédale
        e.toggle (ec);
        check (h.getParam (ec, 9) > 0.5 && e.chain().getReference (ec).enabled, "ON par la pédale");
        e.load (12);
        int ec2 = -1;
        for (int i = 0; i < e.chain().size(); ++i) if (e.chain().getReference (i).name.contains ("ECHOFLEX")) ec2 = i;
        check (ec2 >= 0 && e.chain().getReference (ec2).guid == g && h.getParam (ec2, 9) < 0.5 && h.info (ec2).enabled,
               "rappel du preset : interrupteur de la pédale remis sur OFF sans recharger le plugin");
        e.setBypassPref (e.chain().getReference (ec2).original, OctopusEngine::bypassReaper);
        check (! e.usesPedalBypass (ec2) && ! h.info (ec2).enabled && h.getParam (ec2, 9) > 0.5, "mode REAPER : l'effet reste OFF, coupé par REAPER, interrupteur de la pédale remis sur ON");
        e.toggle (ec2);
        check (h.info (ec2).enabled && e.chain().getReference (ec2).enabled, "mode REAPER : ON par REAPER");
        auto st = e.toTree();
        e.setBypassPref (e.chain().getReference (ec2).original, OctopusEngine::bypassAuto);
        e.fromTree (st);
        check (e.bypassPref (e.chain().getReference (ec2).original) == OctopusEngine::bypassReaper, "mode de bypass enregistré avec le projet");
        e.setBypassPref (e.chain().getReference (ec2).original, OctopusEngine::bypassAuto);
        e.clear (12);
        e.load (2);
    }

    // ---------------------------------------------------------------- captures
    std::unique_ptr<AudioProcessorEditor> edp (proc.createEditor());
    auto* ed = static_cast<OctopusEditor*> (edp.get());
    std::printf ("taille par défaut : %d x %d\n", ed->getWidth(), ed->getHeight());
    auto shot = [&] (const String& name)
    {
        pump();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        File f (out + "/" + name + ".png"); f.deleteFile();
        FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s (%d x %d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    };

    e.load (0);
    e.rename (0, "Clean Ambient");
    e.rename (1, "Crunch Wah");
    e.rename (2, "Lead Pipeline");
    e.save (3); e.rename (3, "Doom Drop G");
    e.load (0);
    shot ("octopus_preset");
    // piste armée : IN / OUT / REC rouges (clignotants si armée, fixes en enregistrement)
    for (int v = 1; v <= 3; ++v) { ed->showScreen (12, v); pump (4); shot (v == 1 ? "octopus_rec_armed_on" : v == 2 ? "octopus_rec_armed_off" : "octopus_rec_recording"); }
    ed->showScreen (12, 3);
    for (int gp : { 0, 200 }) { ed->showScreen (13, gp); pump (4); shot ("octopus_rec_glow_" + String (gp)); }
    ed->showScreen (13, 100); ed->showScreen (14); pump (6); shot ("octopus_rec_picker");
    for (auto* c : ed->getChildren()) if (auto* l = dynamic_cast<OctoPickerLayer*> (c)) l->dismiss();
    pump (2);
    ed->showScreen (12, 0); pump (2);

    // voies parallèles (split / merge)
    {
        e.load (14);                                                       // preset vide
        for (auto* n : { "VST3: NGATE (NINE HANDS | DSP)", "VST3: Darkbass (NINE HANDS | DSP)", "VST3: Ampire (NINE HANDS | DSP)",
                         "VST3: Impulse (NINE HANDS | DSP)", "VST3: Holyverb (NINE HANDS | DSP)", "VST3: ECHOFLEX (NINE HANDS | DSP)",
                         "VST3: Small Tone (NINE HANDS | DSP)", "VST3: ReaEQ (Cockos)", "VST3: Wharry (NINE HANDS | DSP)", "VST3: Crycat (NINE HANDS | DSP)" })
            e.addFx ({ n, n }, e.blocks().size());
        auto idxOf = [&] (const String& s, int nth = 0) { for (int i = 0; i < e.chain().size(); ++i) if (e.chain().getReference (i).name.contains (s) && nth-- == 0) return i; return -1; };
        e.split (idxOf ("Ampire"));
        check (e.groups().size() == 1 && e.groups()[0].lanes[1].isEmpty(), "split : groupe créé, voie B vide");
        shot ("octopus_split_empty");
        e.moveBlock (idxOf ("Impulse"), e.laneEndBlock (0, 0));            // IR dans la voie A, derrière l'ampli
        e.addFx ({ "VST3: Anode (NINE HANDS | DSP)", "" }, e.laneEndBlock (0, 1));
        e.addFx ({ "VST3: Impulse (NINE HANDS | DSP)", "" }, e.laneEndBlock (0, 1));
        check (e.groups()[0].lanes[0].size() == 2 && e.groups()[0].lanes[1].size() == 2, "voies A et B : ampli + IR chacune");
        e.rename (14, "Stack Amps");
        e.save (14);
        shot ("octopus_split");
        // footswitch A : passer de la voie A à la voie B (et retour)
        e.toggleAssign (0, e.swapKeyOf (0));
        e.setMode (OctopusEngine::stompMode);
        e.press (0); pump (3);
        check (e.soloLane (0) == 0 && e.fsText (0).startsWith ("A :"), "footswitch A/B : 1er appui, voie A seule (" + e.fsText (0) + ")");
        shot ("octopus_split_ab_a");
        e.press (0); pump (3);
        check (e.soloLane (0) == 1 && e.fsLit (0), "footswitch A/B : 2e appui, voie B seule (" + e.fsText (0) + ")");
        shot ("octopus_split_ab_b");
        e.press (0); pump (3);
        check (e.soloLane (0) == 0, "footswitch A/B : 3e appui, retour à la voie A");
        e.setMix (0, 1, Routing::mixMute, 0.0);
        e.toggleAssign (0, e.swapKeyOf (0));
        e.setMode (OctopusEngine::presetMode);
        e.setMix (0, 1, Routing::mixLevel, -4.5);
        e.setMix (0, 0, Routing::mixPan, -30.0); e.setMix (0, 1, Routing::mixPan, 30.0);
        e.setMix (0, 1, Routing::mixPhase, 1.0);
        ed->showScreen (15, 0); pump (3);
        shot ("octopus_split_mixer");
        ed->showScreen (15, -1);
        e.toggleFold (0);
        shot ("octopus_split_folded");
        e.toggleFold (0);
        e.addPath (0);
        e.addFx ({ "JS: Volume Adjustment", "" }, e.laneEndBlock (0, 2));
        check (e.groups()[0].lanes.size() == 3, "voie C ajoutée");
        shot ("octopus_split_3paths");
        e.toggle (e.groups()[0].split);
        shot ("octopus_split_off");
        e.load (0);
    }

    ed->screenHover ({ 840.0f, 37.0f });                                      // survol de STOMP : orange
    shot ("octopus_stomp_hover");
    ed->screenHover ({ 2.0f, 2.0f });
    e.toggle (e.blocks()[4]);
    ed->screenHover ({ 108.0f + 150.0f * 2.0f, 80.0f + 36.0f });
    shot ("octopus_edited_hover");

    e.load (2);
    e.setMode (OctopusEngine::stompMode);
    e.toggleAssign (1, e.chain().getReference (e.blocks()[5]).guid);
    e.toggleAssign (2, e.chain().getReference (e.blocks()[8]).guid);
    e.toggleAssign (3, e.chain().getReference (e.blocks()[9]).guid);
    shot ("octopus_stomp");

    e.setMode (OctopusEngine::presetMode);
    ed->showScreen (1, 3);
    {
        auto keep = OctoSettings::favourites();
        OctoSettings::setFavourites ({});
        ed->screenSearch ({}, 0);
        ed->screenFavourite (2); ed->screenFavourite (5);
        check (OctoSettings::favourites().size() == 2, "deux plugins ajoutés aux favoris");
        shot ("octopus_add_stars");
        ed->screenSearch ({}, 1);
        shot ("octopus_add_favorites");
        ed->screenFavourite (0);
        check (OctoSettings::favourites().size() == 1, "favori retiré d'un clic sur l'étoile");
        OctoSettings::setFavourites (keep);
    }
    ed->screenSearch ({}, 2);
    ed->screenPick (5); ed->screenPick (1); ed->screenPick (9);
    shot ("octopus_add_effect");
    ed->screenHover ({ 16.0f + 200.0f, 182.0f + 34.0f * 3.0f + 15.0f });      // survol d'un plugin NINE HANDS | DSP : sa description
    shot ("octopus_add_hover_early");                                         // tout de suite : pas encore de bulle
    Thread::sleep (850);
    shot ("octopus_add_hover");                                              // au bout de 0,8 s : la bulle
    ed->screenHover ({ 2.0f, 2.0f });
    ed->screenSearch ("rea", 0);
    shot ("octopus_add_search");
    ed->screenSearch ({}, 2);
    ed->showScreen (17, 1); pump (3);                                        // tri par type
    shot ("octopus_add_bytype");
    ed->screenSearch ({}, 0);
    ed->showScreen (17, 1); pump (3);
    shot ("octopus_add_all_bytype");
    ed->showScreen (17, 0); pump (3);
    shot ("octopus_add_all_az");

    // clic / double-clic sur un bloc (temps simulés, en ms : appui, relâchement, ...)
    {
        const int slot = 1;
        auto on = [&] { return e.chain().getReference (e.blocks()[slot]).enabled; };
        bool before = on();
        bool opened = ed->testBlockClicks (slot, { 0.0, 80.0, 250.0, 320.0 }, 800);
        check (opened && on() == before, "double-clic rapide : paramètres ouverts, effet inchangé");
        before = on();
        opened = ed->testBlockClicks (slot, { 0.0, 120.0, 560.0, 640.0 }, 800);
        check (opened && on() == before, "double-clic lent (après la bascule du 1er clic) : paramètres ouverts, effet rétabli");
        before = on();
        opened = ed->testBlockClicks (slot, { 0.0, 100.0 }, 600);
        check (! opened && on() != before, "clic simple : ON / OFF, pas de paramètres");
        before = on();
        opened = ed->testBlockClicks (slot, { 0.0, 100.0, 1000.0, 1100.0 }, 600);
        check (! opened && on() == before, "deux clics espacés d'une seconde : deux bascules");
        ed->showScreen (0);
    }

    ed->showScreen (2, e.blocks()[6]);
    shot ("octopus_params");
    ed->screenParamsMidi();
    shot ("octopus_params_midi");
    ed->showScreen (4, e.blocks()[6]);
    shot ("octopus_presets");
    // presets utilisateur enregistrés par Octopus
    {
        auto& h = e.chainHost();
        const int fi = e.blocks()[6];
        ed->screenUserPreset (3, {});
        const double before = h.getParam (fi, 3);
        h.setParam (fi, 3, 0.8);
        check (ed->screenUserPreset (1, "My Lead") == 1, "preset utilisateur enregistré");
        h.setParam (fi, 3, 0.1);
        ed->screenUserPreset (1, "Ambient Wash");
        ed->screenUserPreset (2, "1");                            // triés par nom : 0 Ambient Wash, 1 My Lead
        ed->showScreen (2, fi);
        shot ("octopus_params_preset_name");
        ed->showScreen (4, fi);
        check (std::abs (h.getParam (fi, 3) - 0.8) < 1.0e-6, "preset utilisateur rechargé (paramètre retrouvé)");
        ed->screenUserPreset (0, "Crunch Rhythm");
        shot ("octopus_presets_user");
        ed->screenUserPreset (3, {});
        h.setParam (fi, 3, before);
    }
    e.setFsColour (0, 0xff6fc8ff); e.setFsColour (1, 0xff45e070); e.setFsColour (2, 0xffffb547); e.setFsColour (3, 0xffff4040);
    ed->showScreen (3);
    shot ("octopus_banks");
    {
        // points des banks : 6 presets dans la bank 1, 3 dans la bank 2, aucun dans la bank 3
        auto keep = e.toTree();
        const int cur0 = e.current();
        for (int i : { 4, 5, 6, 7, 8 }) e.save (i);
        e.load (cur0 >= 0 ? cur0 : 0);
        e.setBank (0);
        pump (3);
        check (e.preset (6).saved && e.preset (8).saved && ! e.preset (12).saved, "banks 1 (6 presets), 2 (3 presets), 3 (vide)");
        shot ("octopus_banks_dots");
        e.fromTree (keep);
        pump (3);
    }
    ed->showScreen (0);
    shot ("octopus_fs_colours");
    ed->showScreen (6, 2);
    pump (6);
    shot ("octopus_colour_picker_fs");
    for (auto* c : ed->getChildren()) if (auto* l = dynamic_cast<OctoPickerLayer*> (c)) l->dismiss();
    pump (6);
    ed->showScreen (8, 0);
    pump (6);
    shot ("octopus_colour_picker_tuner");
    for (auto* c : ed->getChildren()) if (auto* l = dynamic_cast<OctoPickerLayer*> (c)) l->dismiss();
    pump (6);
    ed->showScreen (0);

    // accordeur : bloc CHROMA allumé, guitare 7 cordes (G1 joué 8 cents bas)
    {
        proc.prepareToPlay (48000.0, 256);
        int ci = -1;
        for (int i = 0; i < e.chain().size(); ++i) if (OctopusEngine::isChroma (e.chain().getReference (i))) ci = i;
        auto& h = e.chainHost();
        h.setParam (ci, 0, 0.0); h.setParam (ci, 1, 0.0); h.setParam (ci, 2, 0.5);
        e.setEnabled (ci, true);
        auto play = [&] (double hz, double secs)
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m;
            static double ph = 0;
            for (int blk = 0; blk < (int) (secs * 48000 / 256); ++blk)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float v = (float) (0.3 * std::sin (ph) + 0.15 * std::sin (2 * ph) + 0.08 * std::sin (3 * ph));
                    b.setSample (0, i, v); b.setSample (1, i, v);
                    ph += 2.0 * MathConstants<double>::pi * hz / 48000.0;
                }
                proc.processBlock (b, m);
                if (blk % 8 == 0) MessageManager::getInstance()->runDispatchLoopUntil (5);
            }
        };
        pump (10);
        play (49.0 * std::pow (2.0, -8.0 / 1200.0), 2.5);
        shot ("octopus_tuner_mono");
        h.setParam (ci, 1, 1.0);
        play (110.0 * std::pow (2.0, 3.0 / 1200.0), 2.5);
        shot ("octopus_tuner_strobe");
        h.setParam (ci, 1, 0.5);
        pump (10);
        play (73.42, 2.0);
        shot ("octopus_tuner_poly");
        check (proc.chroma().on && proc.chroma().notes.size() == 7, "accordeur : CHROMA allumé, accordage lu dans CHROMA (" + proc.chroma().tuningName + ")");
        {
            auto& lib = proc.tuningLibrary();
            int dropD = -1;
            for (int i = 0; i < lib.size(); ++i) if (lib.get (i).notes.size() == 6 && String (tuna::displayName (lib.get (i), false)).containsIgnoreCase ("drop d")) { dropD = i; break; }
            check (dropD >= 0, "bibliothèque de CHROMA : accordage Drop D trouvé");
            proc.chromaSelectTuning (dropD);
            pump (5);
            check (proc.chroma().notes.size() == 6 && proc.chroma().tuningName.containsIgnoreCase ("drop"), "accordage choisi dans Octopus : " + proc.chroma().tuningName);
            play (73.42, 1.5);
            shot ("octopus_tuner_tuning");
            proc.chromaSelectTuning (-1);
            pump (5);
        }
        const auto sn = proc.tunerSnapshot();
        std::printf ("  accordeur : note %d, %.1f cents\n", sn.monoMidi, sn.monoCents);
        e.setEnabled (ci, false);
        pump (5);
        check (! proc.chroma().on, "CHROMA éteint : retour à la chaîne");
        // accordeur intégré, commandé par MIDI learn (B+C ne l'ouvre plus)
        e.pressFs (1); e.pressFs (2);
        pump (5);
        check (! proc.chroma().on, "B+C n'ouvre plus l'accordeur");
        Thread::sleep (270); e.tick();
        e.learnTarget = OctopusEngine::tunerToggle; e.midiCc (1, 90, 127); e.midiCc (1, 90, 0);
        check (e.ccOf (OctopusEngine::tunerToggle) == 90, "MIDI learn de l'accordeur : CC 90");
        e.midiCc (1, 90, 127); e.midiCc (1, 90, 0);
        pump (5);
        check (proc.chroma().on && proc.chroma().internal, "CC 90 : accordeur intégré d'Octopus, sans bloc CHROMA");
        play (82.41 * std::pow (2.0, 5.0 / 1200.0), 2.0);
        shot ("octopus_tuner_builtin");
        proc.chromaSetMute (true);
        {
            AudioBuffer<float> b (2, 256); MidiBuffer m;
            for (int i = 0; i < 256; ++i) { b.setSample (0, i, 0.5f); b.setSample (1, i, 0.5f); }
            proc.processBlock (b, m);
            check (b.getMagnitude (0, 256) == 0.0f, "accordeur intégré MUTE : sortie silencieuse");
        }
        e.midiCc (1, 90, 127); e.midiCc (1, 90, 0);
        pump (5);
        check (! proc.chroma().on, "CC 90 de nouveau : retour à la chaîne");
        {
            Thread::sleep (270); e.tick();
            const int c0 = e.current();
            e.midiCc (1, 90, 127); e.midiCc (1, 90, 0);
            pump (5);
            check (proc.chroma().on && e.current() == c0, "accordeur par MIDI : preset inchangé");
            e.pressFs (1);
            pump (5);
            check (! proc.chroma().on && e.current() == c0, "accordeur ouvert : un footswitch le ferme, sans changer de preset");
            e.clearCc (OctopusEngine::tunerToggle);
            Thread::sleep (270); e.tick();
            // vrai chemin MIDI (processBlock -> file -> timer) : A+B et C+D comme sur le pédalier
            {
                auto sendCc = [&] (std::initializer_list<std::pair<int, int>> ccs, int gapSamples)
                {
                    AudioBuffer<float> buf (2, 512); buf.clear(); MidiBuffer mb;
                    int pos = 0;
                    for (auto [n, v] : ccs) { mb.addEvent (MidiMessage::controllerEvent (1, n, v), pos); pos += gapSamples; }
                    proc.processBlock (buf, mb);
                };
                e.learnTarget = OctopusEngine::fsA; e.midiCc (1, 61, 127); e.midiCc (1, 61, 0);
                e.learnTarget = OctopusEngine::fsB; e.midiCc (1, 62, 127); e.midiCc (1, 62, 0);
                e.learnTarget = OctopusEngine::fsC; e.midiCc (1, 63, 127); e.midiCc (1, 63, 0);
                e.learnTarget = OctopusEngine::fsD; e.midiCc (1, 64, 127); e.midiCc (1, 64, 0);
                Thread::sleep (300); pump (3);
                const int bk = e.bank() * OctopusEngine::kSlots;
                e.load (bk + 2); pump (3); Thread::sleep (300); pump (3);
                sendCc ({ { 61, 127 }, { 62, 127 } }, 64);  pump (3);
                sendCc ({ { 61, 0 }, { 62, 0 } }, 64);      pump (3);
                check (e.current() == bk + 4, "MIDI réel A+B (même bloc audio) : preset E (" + OctopusEngine::label (e.current()) + ")");
                Thread::sleep (300); pump (3);
                sendCc ({ { 63, 127 } }, 0); pump (1); Thread::sleep (60); sendCc ({ { 64, 127 } }, 0); pump (3);
                sendCc ({ { 63, 0 }, { 64, 0 } }, 64); pump (3);
                check (e.current() == bk + 5, "MIDI réel C puis D 60 ms après : preset F (" + OctopusEngine::label (e.current()) + ")");
                Thread::sleep (300); pump (3);
                sendCc ({ { 61, 127 } }, 0); pump (1); sendCc ({ { 61, 0 } }, 0); pump (1); Thread::sleep (400); pump (2);
                sendCc ({ { 62, 127 } }, 0); pump (1); sendCc ({ { 62, 0 } }, 0); pump (3);
                check (e.current() == bk + 1, "MIDI réel A puis B 0,4 s après : preset B seul (" + OctopusEngine::label (e.current()) + ")");
                for (int t : { 0, 1, 2, 3 }) e.clearCc (t);
                Thread::sleep (300); pump (3);
            }
        }

        // Semitones (transposition en entrée) + mini CHROMA de l'en-tête
        {
            pump (3);
            check (! proc.chroma().on, "Semitones : accordeur fermé");
            // son inchangé au réglage Standard (0)
            proc.setSemitonesPreset (0);
            {
                AudioBuffer<float> b (2, 256), ref (2, 256); MidiBuffer m;
                for (int i = 0; i < 256; ++i) { const float v = (float) std::sin (0.05 * i); b.setSample (0, i, v); b.setSample (1, i, v); }
                ref.makeCopyOf (b);
                proc.processBlock (b, m);
                float d = 0; for (int i = 0; i < 256; ++i) d = jmax (d, std::abs (b.getSample (0, i) - ref.getSample (0, i)));
                check (d == 0.0f, "Semitones Standard (0) : son identique à l'entrée");
            }
            // -12 : fréquence divisée par 2 (passages par zéro comptés sur 0,5 s)
            auto zeroCrossHz = [&] (double hz)
            {
                AudioBuffer<float> b (2, 256); MidiBuffer m; double ph = 0; int zc = 0, n = 0; float last = 0;
                for (int blk = 0; blk < 375; ++blk)
                {
                    for (int i = 0; i < 256; ++i) { const float v = (float) (0.4 * std::sin (ph)); b.setSample (0, i, v); b.setSample (1, i, v); ph += 2.0 * MathConstants<double>::pi * hz / 48000.0; }
                    proc.processBlock (b, m);
                    if (blk < 188) continue;
                    for (int i = 0; i < 256; ++i) { const float y = b.getSample (0, i); if ((last < 0) != (y < 0)) ++zc; last = y; ++n; }
                }
                return zc * 0.5 * 48000.0 / n;
            };
            proc.setSemitonesPreset (8);                               // Octave down (-12)
            const double f1 = zeroCrossHz (220.0);
            check (std::abs (f1 - 110.0) < 4.0, "Semitones Octave down : 220 Hz -> " + String (f1, 1) + " Hz");
            proc.setSemitonesPreset (2);                               // D Standard (-2)
            const double f2 = zeroCrossHz (220.0);
            check (std::abs (f2 - 220.0 * std::pow (2.0, -2.0 / 12.0)) < 4.0, "Semitones D Standard : 220 Hz -> " + String (f2, 1) + " Hz");
            // globale, mais rappelée par un preset qui la garde
            const int bk = e.bank() * OctopusEngine::kSlots;
            e.load (bk + 0);
            check (proc.semitonesPreset() == 2, "Semitones globale : inchangée au changement de preset");
            e.setPresetSemi (bk + 1, 15);                              // 1B garde Capo 5
            e.load (bk + 1);
            check (proc.semitonesPreset() == 15, "preset 1B lié : Capo 5 rappelé");
            e.load (bk + 0);
            check (proc.semitonesPreset() == 15, "preset 1A non lié : la transposition reste");
            proc.setSemitonesPreset (3);
            e.load (bk + 1); e.save (bk + 1);
            check (e.presetSemi (bk + 1) == 15 && proc.semitonesPreset() == 15, "enregistrer 1B garde sa transposition");
            {
                MemoryBlock mb2; proc.getStateInformation (mb2);
                OctopusProcessor p3; p3.setStateInformation (mb2.getData(), (int) mb2.getSize());
                check (p3.semitonesPreset() == 15 && p3.getEngine().presetSemi (bk + 1) == 15, "Semitones enregistré avec le projet (global + preset 1B)");
            }
            e.setPresetSemi (bk + 1, -1);
            e.load (bk + 0);
            proc.setSemitonesPreset (2);
            // mini CHROMA : note juste puis fausse (fenêtre ouverte : l'accordeur écoute)
            proc.setSemitonesPreset (0);
            play (82.41, 2.0);
            shot ("octopus_header_intune_standard");                 // Mi joué, sans transposition : E
            proc.setSemitonesPreset (2);
            play (82.41, 1.0);
            shot ("octopus_header_intune");                          // Mi joué, D Standard (-2) : le mini CHROMA affiche D
            play (92.5 * std::pow (2.0, 9.0 / 1200.0), 2.0);
            shot ("octopus_header_offtune");
            const auto sm = proc.tunerSnapshot();
            check (sm.monoMidi == 42, "mini CHROMA : F#2 lu fenêtre ouverte, sans bloc CHROMA (" + String (sm.monoMidi) + ")");
            // chevron du mini CHROMA : G joué 12 cents trop bas, puis 25 cents trop haut
            proc.setSemitonesPreset (0);
            play (98.0 * std::pow (2.0, -12.0 / 1200.0), 1.5); shot ("octopus_header_flat");
            play (98.0 * std::pow (2.0, 25.0 / 1200.0), 1.5); shot ("octopus_header_sharp");
            proc.setSemitonesPreset (2);
            play (0.0, 2.6); pump (8);                               // silence : la note s'efface
            shot ("octopus_header_idle");
            ed->showScreen (9);
            pump (12);
            shot ("octopus_semitones_drum");
            ed->showScreen (10, 4);
            pump (12);
            shot ("octopus_semitones_drum_scrolled");
            ed->showScreen (0);
            ed->showScreen (11);
            pump (6);
            shot ("octopus_colour_picker_semitones");
            for (auto* c : ed->getChildren()) if (auto* l = dynamic_cast<OctoPickerLayer*> (c)) l->dismiss();
            pump (6);
            proc.setSemitonesPreset (0);
        }

        // E (A+B) et F (C+D) : footswitchs comme A-D (presets 1E / 1F en mode PRESET, effets en STOMP)
        {
            const int bk = e.bank() * OctopusEngine::kSlots;
            e.load (bk + 2);
            check (e.save (bk + 4) && OctopusEngine::label (bk + 4) == "1E", "preset enregistré sous 1E");
            e.rename (bk + 4, "Solo Wah");
            e.load (bk);
            pump (3);
            e.pressFs (0); e.pressFs (1);
            pump (3);
            check (e.current() == bk + 4 && e.fsLit (4) && ! e.fsLit (0), "A+B en PRESET : charge 1E, A s'éteint");
            e.pressFs (3); e.pressFs (2);
            pump (3);
            check (e.current() == bk + 5 && e.blocks().isEmpty() && ! e.fsLit (4), "C+D en PRESET : 1F (vide) chargé");
            e.load (bk + 4);
            pump (3);
            // ancien format (4 presets par bank) : 2B (ancien n° 5) reste 2B (nouveau n° 7)
            {
                auto t = e.toTree(); t.setProperty ("version", 1, nullptr);
                for (auto c : t) if (c.hasType ("PRESET") && (int) c["idx"] == bk + 4) c.setProperty ("idx", 5, nullptr);
                OctopusEngine e2 (e.chainHost());
                e2.fromTree (t);
                check (e2.preset (7).saved && e2.preset (7).name == "Solo Wah", "ancien projet (4 presets par bank) : 2B reste 2B");
            }
            e.setProgramChange (true);
            e.midiProgram (1, 1);
            check (e.current() == bk + 1, "Program Change 2 = 1B");
            e.load (bk + 4);
            pump (3);
            shot ("octopus_combos");
        }
        e.setMode (OctopusEngine::stompMode);
        {
            const auto g5 = e.chain().getReference (e.blocks()[5]).guid, g6 = e.chain().getReference (e.blocks()[6]).guid;
            e.clearAssign (4); e.clearAssign (5);
            e.toggleAssign (4, g5); e.toggleAssign (4, g6);
            const bool was = e.chain().getReference (e.blocks()[e.blockOf (g5)]).enabled;
            e.clearAssign (0);
            const auto gA = e.chain().getReference (e.blocks()[2]).guid;
            e.toggleAssign (0, gA);
            const bool wasA = e.chain().getReference (e.blocks()[e.blockOf (gA)]).enabled;
            Thread::sleep (270); e.tick();
            e.pressFs (0);
            check (e.chain().getReference (e.blocks()[e.blockOf (gA)]).enabled != wasA, "A seul en STOMP : agit tout de suite");
            e.pressFs (1);
            pump (3);
            check (e.chain().getReference (e.blocks()[e.blockOf (g5)]).enabled != was, "A+B en STOMP : E bascule ses effets");
            check (e.chain().getReference (e.blocks()[e.blockOf (gA)]).enabled == wasA, "A+B en STOMP : le premier appui sur A est annulé");
            e.learnTarget = OctopusEngine::fsA; e.midiCc (1, 40, 127); e.midiCc (1, 40, 0);
            e.learnTarget = OctopusEngine::fsB; e.midiCc (1, 41, 127); e.midiCc (1, 41, 0);
            check (e.ccOf (OctopusEngine::fsA) == 40 && e.ccOf (OctopusEngine::fsB) == 41, "MIDI learn de A et B : CC 40 / 41");
            e.midiCc (1, 40, 127); e.midiCc (1, 41, 127); e.midiCc (1, 40, 0); e.midiCc (1, 41, 0);
            pump (3);
            check (e.chain().getReference (e.blocks()[e.blockOf (g5)]).enabled == was, "E par MIDI : CC de A + CC de B ensemble rebasculent ses effets");
            {
                auto t = e.toTree(); t.setProperty ("cc", "-1,41,,,,,,,40,", nullptr); e.fromTree (t);      // ancien learn de E qui avait pris le CC de A
                check (e.ccOf (OctopusEngine::fsA) == 40 && e.ccOf (OctopusEngine::fsE) < 0, "CC appris sur E en 1.7.0 rendu à A");
            }
            e.learnTarget = OctopusEngine::fsE; e.midiCc (1, 40, 127);
            check (e.ccOf (OctopusEngine::fsA) == 40 && e.ccOf (OctopusEngine::fsE) < 0, "pas de learn à part pour E : le CC de A reste à A");
            e.midiCc (1, 40, 0); pump (12);
            e.learnTarget = OctopusEngine::fsC; e.midiCc (1, 42, 127); e.midiCc (1, 42, 0);
            e.learnTarget = OctopusEngine::tunerToggle; e.midiCc (1, 41, 127);
            check (e.ccOf (OctopusEngine::fsB) == 41 && e.ccOf (OctopusEngine::tunerToggle) < 0, "learn de l'accordeur avec le CC de B : B garde son CC");
            e.midiCc (1, 41, 0); pump (12);
            e.clearCc (OctopusEngine::fsA); e.clearCc (OctopusEngine::fsB); e.clearCc (OctopusEngine::fsC);
            auto st2 = e.toTree(); e.clearAssign (4); e.fromTree (st2);
            check (e.isAssigned (4, g5) && e.isAssigned (4, g6), "effets de E enregistrés avec le projet");
            if (! e.chain().getReference (e.blocks()[e.blockOf (g5)]).enabled) { e.pressFs (0); e.pressFs (1); pump (3); }
            e.toggleAssign (5, e.chain().getReference (e.blocks()[9]).guid);
            pump (8);
            shot ("octopus_keys_ef");
            ed->showScreen (7, 25); pump (3); shot ("octopus_glow_25");
            ed->showScreen (7, 175); pump (3); shot ("octopus_glow_175");
            ed->showScreen (7, 100); pump (3);
            e.clearAssign (4); e.clearAssign (5);
        }
        e.setMode (OctopusEngine::presetMode);
    }

    ed->setScaleIndex (0);
    check (ed->getWidth() == 828 && ed->getHeight() == 619, "taille 50 % : " + String (ed->getWidth()) + " x " + String (ed->getHeight()));
    shot ("octopus_50pc");
    ed->setScaleIndex (-1);
    check (ed->getWidth() == 993, "taille par défaut (60 % sans réglage) : " + String (ed->getWidth()));
    ed->setScaleIndex (3);
    shot ("octopus_100pc");

    // famille utilitaire (blanc) : un bloc allumé, un bloc éteint
    {
        e.addFx ({ "JS: Utility/volume", "" }, 0);
        e.addFx ({ "VST3: Stereo Width (NINE HANDS | DSP)", "" }, 1);
        e.setEnabled (e.blocks()[1], false);
        shot ("octopus_utility_on_off");
        e.remove (e.blocks()[1]);
        e.remove (e.blocks()[0]);
    }

    // Anode : famille ampli, icône de lampe (allumé puis éteint)
    {
        e.addFx ({ "VST3: Anode (NINE HANDS | DSP)", "" }, 0);
        e.addFx ({ "VST3: Anode (NINE HANDS | DSP)", "" }, 1);
        e.setEnabled (e.blocks()[1], false);
        check (FxCat::classify ("VST3: Anode (NINE HANDS | DSP)") == FxCat::amp && FxCat::iconOf ("VST3: Anode (NINE HANDS | DSP)") == FxCat::iconTube,
               "Anode : famille ampli, icône de lampe");
        shot ("octopus_anode");
        e.remove (e.blocks()[1]);
        e.remove (e.blocks()[0]);
    }

    // preset vide : écran vide comme au démarrage ; couleur du footswitch sur l'étiquette du preset
    {
        check (! e.preset (5).saved, "2B est vide");
        check (e.load (5) && e.blocks().isEmpty() && e.current() == 5, "preset vide chargé : chaîne vide (" + String (e.blocks().size()) + " blocs)");
        shot ("octopus_empty_preset");
        e.load (2);
        check (e.blocks().size() > 5 && e.current() == 2, "retour au preset 1C");
        shot ("octopus_badge_colour");
        e.setPresetColour (2, 0xffb07aff);
        e.setPresetColour (6, 0xffff4040);                    // 2C vide, avec seulement une couleur
        auto tr = e.toTree();
        e.setPresetColour (2, 0); e.setPresetColour (6, 0);
        e.fromTree (tr);
        check (e.presetColour (2) == 0xffb07aff && e.presetColour (6) == 0xffff4040 && ! e.preset (6).saved
               && e.presetColour (3) == e.fsColour (3), "couleur propre d'un preset enregistrée et relue");
        e.load (2);
        shot ("octopus_preset_colour");
        ed->showScreen (5, 2);
        pump (2);
        for (auto* c : ed->getChildren())
            if (auto* l = dynamic_cast<OctoPickerLayer*> (c))
            {
                Array<juce::uint32> u; u.insertMultiple (0, 0u, 11);
                u.set (0, 0xffc0392b); u.set (1, 0xff7f5af0); u.set (2, 0xff2ec4b6);
                l->colourPicker().setUserForTest (u, 3);
            }
        pump (6);
        shot ("octopus_colour_picker_preset");
        for (auto* c : ed->getChildren()) if (auto* l = dynamic_cast<OctoPickerLayer*> (c)) l->dismiss();
        pump (6);
        e.setPresetColour (2, 0); e.setPresetColour (6, 0);
    }

    // longue chaîne : défilement
    for (int i = 0; i < 9; ++i) e.addFx ({ "VST3: ReaEQ (Cockos)", "" }, 99);
    shot ("octopus_long_chain");

    edp.reset();
    std::printf ("%s\n", failures == 0 ? "TOUS LES TESTS OK" : (String (failures) + " ECHEC(S)").toRawUTF8());
    return failures == 0 ? 0 : 1;
}
