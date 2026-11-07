#pragma once
#include <juce_graphics/juce_graphics.h>

// Repère de la pédale OCTOPUS : 1249 x 934 unités (plan du cadre fourni le 07/11/2026, tools/make_frame.py),
// images en 2x. Le boîtier, la lunette et les logos sont dans background.png ; le plugin y dessine l'écran, les footswitchs inox,
// le logo OCTOPUS, les lettres A-D et les anneaux lumineux.
namespace OctoLayout
{
    constexpr int kW = 1249, kH = 934;
    // tailles de fenêtre (largeur en pixels) : 50 %, 60 % (taille standard), 75 %, 100 % ;
    // la taille par défaut se choisit dans le menu (réglage commun à toutes les instances, 60 % au départ)
    constexpr int kNumSizes = 4;
    const int kWidths[kNumSizes] = { 828, 993, 1241, 1655 };
    const char* const kSizeNames[kNumSizes] = { "50 %", "60 %", "75 %", "100 %" };
    constexpr int kStandardSize = 1, kMinW = 828, kMaxW = 1700;

    // écran : repère local de 1064 x 651 unités, affiché dans la vitre du cadre (1177 x 720, plan du 07/11/2026) avec le facteur kG
    constexpr float kScreenH = 651.0f;
    constexpr float kG = 1176.95f / 1064.0f;
    const juce::Rectangle<float> glass { 35.85f, 67.2f, 1176.95f, 1176.95f * kScreenH / 1064.0f };
    constexpr float glassRadius = 7.5f;

    // footswitchs inox vus de dessus (sprite fs_off.png de make_art.py, écrou de 80 entre plats, un plat en haut),
    // posés à la place de ceux de la photo (effacés par make_frame.py)
    constexpr float fsY = 868.3f;
    const float fsX[4] = { 199.5f, 482.5f, 765.5f, 1048.5f };
    const float capX[4] = { 199.5f, 482.5f, 765.5f, 1048.5f };  // bouton qui s'enfonce (centré sur l'écrou)
    const float capY[4] = { 868.3f, 868.3f, 868.3f, 868.3f };
    constexpr float capR = 31.5f;
    constexpr bool drawFsSprite = true;
    constexpr float fsSprite = 150.0f;                          // côté du sprite footswitch (ombre comprise)
    constexpr float fsRadius = 44.0f;                           // zone cliquable (cercle)
    // lettres A-D (à la place de celles gravées sur la photo) : centre et hauteur de police
    const float ledXs[4] = { 199.8f, 482.8f, 765.8f, 1048.8f };   // lettre centrée sur son footswitch (plan du 07/11)
    constexpr float ledY = 808.5f, ledDx = 0.0f, letterFont = 20.0f;
    constexpr float ledR = 9.0f;
    // logos OCTOPUS et NINE HANDS | DSP : dans background.png (make_frame.py) ; l'ancien logo dessiné par le plugin n'est plus affiché
    constexpr bool drawLogo = false;
    // ancien logo OCTOPUS (dessiné par le plugin, net à toutes les tailles) : à la place de celui de la photo, bord droit à 1200
    // (comme sur le visuel), 62 de haut, centré à 53 ; dessin 1704 x 446 dans une image 1768 x 510
    constexpr float logoH = 62.0f, logoW = logoH * 1704.0f / 446.0f, logoRight = 1191.0f, logoCy = 53.0f;
    constexpr float logoPad = logoH * 32.0f / 446.0f;
    // anneau lumineux dans le footswitch, dans la gorge autour du bouton qui s'enfonce (comme la Phase 90),
    // même rayon que le diffuseur gris du sprite (make_art.py : RING_R0, RING_R1 = 27.4, 29.5)
    // gorge sombre de l'écrou photographié : rayon 33,2
    const float ringCx[4] = { 199.5f, 482.5f, 765.5f, 1048.5f };
    const float ringCy[4] = { 868.3f, 868.3f, 868.3f, 868.3f };
    const float ringRx[4] = { 33.2f, 33.2f, 33.2f, 33.2f };
    const float ringRy[4] = { 33.2f, 33.2f, 33.2f, 33.2f };
    constexpr float ringW = 2.1f;
}
