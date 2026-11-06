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
    // indice de taille (0 = 50 %, 1 = 60 %, 2 = 75 %, 3 = 100 %) ; 60 % si rien n'a été choisi
    inline int defaultSize()
    {
        if (auto x = juce::XmlDocument::parse (file()))
            return juce::jlimit (0, 3, x->getIntAttribute ("defaultSize", 1));
        return 1;
    }
    inline void setDefaultSize (int idx)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = juce::XmlDocument::parse (file())) x = *old;
        x.setAttribute ("defaultSize", juce::jlimit (0, 3, idx));
        file().getParentDirectory().createDirectory();
        x.writeTo (file());
    }

    // halo des lettres lumineuses et du symbole de l'accordeur, en % (100 = normal), commun à toutes les fenêtres
    inline int haloPercent()
    {
        if (auto x = juce::XmlDocument::parse (file()))
            return juce::jlimit (0, 200, x->getIntAttribute ("halo", 100));
        return 100;
    }
    inline void setHaloPercent (int pc)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = juce::XmlDocument::parse (file())) x = *old;
        x.setAttribute ("halo", juce::jlimit (0, 200, pc));
        file().getParentDirectory().createDirectory();
        x.writeTo (file());
    }

    // symbole de l'accordeur (B+C) : couleur (0 = vert de CHROMA) et opacité de son halo en %
    inline juce::uint32 tunerColour()
    {
        if (auto x = juce::XmlDocument::parse (file())) return (juce::uint32) x->getStringAttribute ("tunerColour", "0").getHexValue64();
        return 0;
    }
    inline int tunerHalo()
    {
        if (auto x = juce::XmlDocument::parse (file())) return juce::jlimit (0, 200, x->getIntAttribute ("tunerHalo", 100));
        return 100;
    }
    inline void setTuner (juce::uint32 colour, int halo)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = juce::XmlDocument::parse (file())) x = *old;
        x.setAttribute ("tunerColour", juce::String::toHexString ((juce::int64) colour));
        x.setAttribute ("tunerHalo", juce::jlimit (0, 200, halo));
        file().getParentDirectory().createDirectory();
        x.writeTo (file());
    }

    // plugins favoris (nom complet donné par REAPER, ex. "VST3: Holyverb (NINE HANDS | DSP)"), communs à tous les projets
    inline juce::StringArray favourites()
    {
        juce::StringArray out;
        if (auto x = juce::XmlDocument::parse (file()))
            if (auto* fav = x->getChildByName ("Favourites"))
                for (auto* e : fav->getChildWithTagNameIterator ("Plugin"))
                    out.addIfNotAlreadyThere (e->getStringAttribute ("name"));
        return out;
    }
    inline void setFavourites (const juce::StringArray& names)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = juce::XmlDocument::parse (file())) x = *old;
        x.deleteAllChildElementsWithTagName ("Favourites");
        auto* fav = x.createNewChildElement ("Favourites");
        for (auto& n : names) fav->createNewChildElement ("Plugin")->setAttribute ("name", n);
        file().getParentDirectory().createDirectory();
        x.writeTo (file());
    }

    // couleurs enregistrées dans le nuancier (11 emplacements, 0 = vide), communes à tous les projets
    inline juce::Array<juce::uint32> userColours()
    {
        juce::Array<juce::uint32> out;
        out.insertMultiple (0, 0u, 11);
        if (auto x = juce::XmlDocument::parse (file()))
        {
            auto t = juce::StringArray::fromTokens (x->getStringAttribute ("userColours"), ",", "");
            for (int i = 0; i < juce::jmin (11, t.size()); ++i) out.set (i, (juce::uint32) t[i].getHexValue64());
        }
        return out;
    }
    inline void setUserColours (const juce::Array<juce::uint32>& c)
    {
        juce::XmlElement x ("OctopusSettings");
        if (auto old = juce::XmlDocument::parse (file())) x = *old;
        juce::StringArray t;
        for (auto v : c) t.add (v == 0 ? juce::String ("0") : juce::String::toHexString ((juce::int64) v));
        x.setAttribute ("userColours", t.joinIntoString (","));
        file().getParentDirectory().createDirectory();
        x.writeTo (file());
    }
}
