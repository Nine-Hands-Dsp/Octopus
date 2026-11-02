#pragma once
#include <juce_core/juce_core.h>

// Réglages communs à toutes les instances d'Octopus (fichier de l'utilisateur) : taille par défaut de la fenêtre.
namespace OctoSettings
{
    inline juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Studio Wiga").getChildFile ("Octopus").getChildFile ("settings.xml");
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

    // plugins favoris (nom complet donné par REAPER, ex. "VST3: Holyverb (Studio Wiga)"), communs à tous les projets
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
}
