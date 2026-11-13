#pragma once
#include <juce_core/juce_core.h>
#include "NineHandsFolder.h"

// Réglages communs à toutes les instances d'Octopus (fichier de l'utilisateur) : taille par défaut de la fenêtre.
namespace OctoSettings
{
    inline juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile (nhd::folderName()).getChildFile ("Octopus").getChildFile ("settings.xml");
    }

    // Écriture différée : pendant qu'on fait glisser un curseur du nuancier (couleur, glow), les réglages restent en
    // mémoire ; le fichier n'est écrit qu'une fois, à la fermeture du nuancier (flush), ou à la fermeture de la fenêtre.
    inline std::unique_ptr<juce::XmlElement>& pendingXml() { static std::unique_ptr<juce::XmlElement> p; return p; }
    inline std::unique_ptr<juce::XmlElement> current()             // réglages en cours (en attente ou ceux du fichier)
    {
        if (auto& p = pendingXml()) return std::make_unique<juce::XmlElement> (*p);
        return juce::XmlDocument::parse (file());
    }
    inline void commit (const juce::XmlElement& x)                  // écriture immédiate (inclut ce qui était en attente)
    {
        file().getParentDirectory().createDirectory();
        x.writeTo (file());
        pendingXml().reset();
    }
    inline void stage (const juce::XmlElement& x) { pendingXml() = std::make_unique<juce::XmlElement> (x); }
    inline void flush() { if (auto& p = pendingXml()) { const auto x = *p; commit (x); } }
    // indice de taille (0 = 50 %, 1 = 60 %, 2 = 75 %, 3 = 100 %) ; 60 % si rien n'a été choisi
    inline int defaultSize()
    {
        if (auto x = current())
            return juce::jlimit (1, 3, x->getIntAttribute ("defaultSize", 1));   // 50 % retire (minimum 60 %)
        return 1;
    }
    inline void setDefaultSize (int idx)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.setAttribute ("defaultSize", juce::jlimit (1, 3, idx));
        commit (x);
    }

    // halo des lettres lumineuses et du symbole de l'accordeur, en % (100 = normal), commun à toutes les fenêtres
    inline int haloPercent()
    {
        if (auto x = current())
            return juce::jlimit (0, 200, x->getIntAttribute ("halo", 100));
        return 100;
    }
    inline void setHaloPercent (int pc)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.setAttribute ("halo", juce::jlimit (0, 200, pc));
        stage (x);
    }

    // halo des anneaux lumineux des footswitchs (Glow intensity), 25 / 50 / 75 / 100 %, commun à toutes les fenêtres
    inline int glowPercent()
    {
        if (auto x = current())
            return juce::jlimit (25, 100, x->getIntAttribute ("glowPercent", 100));
        return 100;
    }
    inline void setGlowPercent (int pc)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.setAttribute ("glowPercent", juce::jlimit (25, 100, pc));
        stage (x);
    }

    // symbole de l'accordeur (B+C) : couleur (0 = vert de CHROMA) et opacité de son halo en %
    inline juce::uint32 tunerColour()
    {
        if (auto x = current()) return (juce::uint32) x->getStringAttribute ("tunerColour", "0").getHexValue64();
        return 0;
    }
    inline int tunerHalo()
    {
        if (auto x = current()) return juce::jlimit (0, 200, x->getIntAttribute ("tunerHalo", 100));
        return 100;
    }
    inline void setTuner (juce::uint32 colour, int halo)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.setAttribute ("tunerColour", juce::String::toHexString ((juce::int64) colour));
        x.setAttribute ("tunerHalo", juce::jlimit (0, 200, halo));
        stage (x);
    }

    // couleur (0 = celle d'origine) et opacité du halo en % d'un élément de l'écran : "semi" (bouton Semitones),
    // "chromaOk" / "chromaOff" (mini CHROMA : note juste / fausse) et "chroma" (halo commun du mini CHROMA)
    inline juce::uint32 colourOf (const juce::String& key)
    {
        if (auto x = current()) return (juce::uint32) x->getStringAttribute (key + "Colour", "0").getHexValue64();
        return 0;
    }
    inline int glowOf (const juce::String& key)
    {
        if (auto x = current()) return juce::jlimit (0, 200, x->getIntAttribute (key + "Glow", 100));
        return 100;
    }
    inline void setColourOf (const juce::String& key, juce::uint32 colour)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.setAttribute (key + "Colour", juce::String::toHexString ((juce::int64) colour));
        stage (x);
    }
    inline void setGlowOf (const juce::String& key, int pc)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.setAttribute (key + "Glow", juce::jlimit (0, 200, pc));
        stage (x);
    }

    // plugins favoris (nom complet donné par REAPER, ex. "VST3: Holyverb (NINE HANDS | DSP)"), communs à tous les projets
    inline juce::StringArray favourites()
    {
        juce::StringArray out;
        if (auto x = current())
            if (auto* fav = x->getChildByName ("Favourites"))
                for (auto* e : fav->getChildWithTagNameIterator ("Plugin"))
                    out.addIfNotAlreadyThere (e->getStringAttribute ("name"));
        return out;
    }
    inline void setFavourites (const juce::StringArray& names)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        x.deleteAllChildElementsWithTagName ("Favourites");
        auto* fav = x.createNewChildElement ("Favourites");
        for (auto& n : names) fav->createNewChildElement ("Plugin")->setAttribute ("name", n);
        commit (x);
    }

    // couleurs enregistrées dans le nuancier (11 emplacements, 0 = vide), communes à tous les projets
    inline juce::Array<juce::uint32> userColours()
    {
        juce::Array<juce::uint32> out;
        out.insertMultiple (0, 0u, 11);
        if (auto x = current())
        {
            auto t = juce::StringArray::fromTokens (x->getStringAttribute ("userColours"), ",", "");
            for (int i = 0; i < juce::jmin (11, t.size()); ++i) out.set (i, (juce::uint32) t[i].getHexValue64());
        }
        return out;
    }
    inline void setUserColours (const juce::Array<juce::uint32>& c)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = current()) x = *old;
        juce::StringArray t;
        for (auto v : c) t.add (v == 0 ? juce::String ("0") : juce::String::toHexString ((juce::int64) v));
        x.setAttribute ("userColours", t.joinIntoString (","));
        commit (x);
    }
}
