#include "PluginEditor.h"
#include "FxCategory.h"
#include "Layout.h"
#include "UserFxPresets.h"
#include "OctoSettings.h"
#include "ColourPicker.h"
#include "Routing.h"
#include "MenuStyle.h"
#include <BinaryData.h>

using namespace juce;
namespace L = OctoLayout;

namespace
{
    constexpr int kW = L::kW, kH = L::kH;
    constexpr float kAsset = 2.0f;                               // images livrées en 2x (HiDPI)

    // ---- couleurs de l'écran
    const Colour kScrBg (0xff0a0c10), kScrPanel (0xff13161c), kScrPanel2 (0xff1a1e26), kLine (0xff59616e),
                 kScreenBlack (0xff08090b), kText (0xffeef1f5), kDim (0xff8d96a4), kFaint (0xff3a414d), kAccent (0xff4fc3f7), kAmber (0xffffb547), kStompOrange (0xffff8c3a),
                 kPresetLed (0xff6fc8ff);

    // ---- écran : repère local (unités), origine = coin haut gauche de la vitre (1064 x 624)
    constexpr float kSW = 1064.0f, kSH = L::kScreenH;
    constexpr float kHeaderH = 57.0f;                      // en-tête (v1.13.1 : 62 ; v1.17.0 : 5 de moins -> 57)
    constexpr float kHdrY = (kHeaderH - 36.0f) * 0.5f;     // éléments de l'en-tête (36 de haut) centrés
    // 3 rangées visibles ; même espace entre les carrés horizontalement et verticalement (pas de 140 = carré 70 + 70)
    constexpr int kRows = 3, kCols = 7;
    constexpr float kGridTop = 121.55f,   /* v1.18.71 : maquette, 1re rangée centrée à 161.55 */ kRowPitch = 140.0f, kColPitch = 140.0f, kColX0 = (kSW - kColPitch * (float) (kCols - 1)) * 0.5f;   // grille centrée
    constexpr float kTile = 70.0f, kTileDy = 40.0f;              // carré du bloc, centre à rowTop + kTileDy
    // cases des footswitchs : même écart entre elles et avec les bords de l'écran, décollées du bas
    constexpr float kFooterGap = 14.0f, kFooterH = 58.0f, kFooterY = kSH - kFooterH - kFooterGap, kFooterSplit = 0.0f, kFooterW = (kSW - 7.0f * kFooterGap - kFooterSplit) / 6.0f;   // A-F : même écart entre les cases et avec les bords de l'écran (côtés et bas)
    constexpr float kFooterLine = kFooterY - kFooterGap - 0.5f;                          // trait au-dessus des cases (même écart que sous les cases, maquette)
    constexpr float kMoreArrowY = 548.0f;                                                  // flèche bleue « encore des rangées » (maquette)
    const Rectangle<float> kOverlay (0.0f, kHeaderH, kSW, kSH - kHeaderH);

    Image loadPng (const void* d, int n) { return ImageFileFormat::loadFrom (d, (size_t) n).convertedToFormat (Image::ARGB); }

    // redimensionne une fois (par le timer, jamais dans paint), puis garde en cache tant que la taille ne change pas
    struct ScaledCache
    {
        Image src, img; float scale = -1.0f;
        void prepare (float s)
        {
            if (std::abs (s - scale) <= 1.0e-4f && img.isValid()) return;
            scale = s;
            const float k = s / kAsset;
            const int w = jmax (1, roundToInt ((float) src.getWidth() * k)), h = jmax (1, roundToInt ((float) src.getHeight() * k));
            img = src.rescaled (w, h, Graphics::highResamplingQuality);
        }
        const Image& get() const { return img.isValid() ? img : src; }
        float pixelsPerUnit() const { return img.isValid() ? scale : kAsset; }
    };

    // réduction nette d'une grande image : divisions par 2 successives (moyenne de 4 pixels), puis dernier pas en haute qualité
    Image crispRescale (Image im, int w, int h)
    {
        if (! im.isValid() || w <= 0 || h <= 0) return {};
        im = im.convertedToFormat (Image::ARGB);
        while (im.getWidth() >= w * 2 && im.getHeight() >= h * 2)
        {
            const int nw = im.getWidth() / 2, nh = im.getHeight() / 2;
            Image half (Image::ARGB, nw, nh, true);
            const Image::BitmapData sd (im, Image::BitmapData::readOnly);
            Image::BitmapData dd (half, Image::BitmapData::writeOnly);
            for (int y = 0; y < nh; ++y)
                for (int x = 0; x < nw; ++x)
                {
                    int acc[4] = { 0, 0, 0, 0 };       // pixels prémultipliés : la moyenne garde des bords propres
                    for (int k = 0; k < 4; ++k)
                    {
                        const auto* p = sd.getPixelPointer (x * 2 + (k & 1), y * 2 + (k >> 1));
                        for (int c = 0; c < 4; ++c) acc[c] += p[c];
                    }
                    auto* q = dd.getPixelPointer (x, y);
                    for (int c = 0; c < 4; ++c) q[c] = (uint8) ((acc[c] + 2) / 4);
                }
            im = half;
        }
        return im.getWidth() == w && im.getHeight() == h ? im : im.rescaled (w, h, Graphics::highResamplingQuality);
    }

    String fsLetter (int f) { return String::charToString ((juce_wchar) ('A' + f)); }

    // ids des boutons de l'écran
    enum ButtonId
    {
        bNone = 0, bBankPrev, bBankNext, bModePreset, bModeStomp, bSave, bBanks, bClose,
        bParamOnOff, bParamWindow, bPagePrev, bPageNext, bExport, bImport, bPc, bReload,
        bAddPicked, bClearPicked, bParamMidi, bPresets, bTunerTunings, bPresetSave, bPresetRescan, bTunerMute, bTunerMode, bTunerExit, bChroma, bSemi, bInsertWhere,
        bChip = 100,                     // + catégorie du navigateur
        bSortAZ = 150, bSortType,        // tri du navigateur : A-Z / par type
        bBank = 200,                     // + numéro de bank
        bCard = 300,                     // + slot * 10 + action
        bFsColour = 450                  // + footswitch
    };
    enum CardAction { cLoad, cSave, cRename, cCopy, cPaste, cClear, numCardActions };
    const char* const kCardText[numCardActions] = { "LOAD", "SAVE", "RENAME", "COPY", "PASTE", "CLEAR" };
    const char* const kChips[] = { "All", "Favorites", "NINE HANDS | DSP", "VST3", "VST", "CLAP", "JS", "AU" };
    constexpr int kNumChips = 8;
    constexpr int kChipFavourites = 1;

    struct Btn { Rectangle<float> r; int id; String text; bool active = false; bool enabled = true; };
}

//==============================================================================
class PedalView : public Component, private Timer
{
public:
    enum class Overlay { none, browser, params, banks };

    explicit PedalView (OctopusProcessor& p) : proc (p), eng (p.getEngine())
    {
        bg.src    = loadPng (BinaryData::background_png, BinaryData::background_pngSize);
        fsOff.src = loadPng (BinaryData::fs_off_png, BinaryData::fs_off_pngSize);
        fsOn.src  = loadPng (BinaryData::fs_on_png, BinaryData::fs_on_pngSize);
        logoSrc   = loadPng (BinaryData::logo_octopus_blanc_png, BinaryData::logo_octopus_blanc_pngSize);
        tfMedium  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
        tfBold    = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
        tfRegular = Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, BinaryData::PoppinsRegular_ttfSize);
        tfMono    = Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);
        menuLnf.f = menuFonts();
        setLookAndFeel (&menuLnf);                                  // menus des champs de texte (copier / coller) aussi
        for (auto& l : ledLevel) l = 0.0f;

        styleEditor (search, "Search plug-ins...");
        search.getProperties().remove ("roundBg");
        search.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);   // fond, cadre arrondi et loupe dessinés par l'écran
        search.setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        search.setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        search.onFocusLost = [this] { askRepaint (screenPx()); };
        search.setSelectAllWhenFocused (false);   // revenir dans la recherche ne sélectionne plus tout le texte (la touche suivante l'effaçait)
        search.onTextChange = [this] { listScroll = 0; listPos = 0.0f; askRepaint (screenPx()); };
        search.onEscapeKey = [this] { closeOverlay(); };
        search.onReturnKey = [this]
        {
            if (picked.size() > 0) { addPicked(); return; }
            const auto items = filtered();
            if (items.size() > 0) { picked.add (items.getReference (0)); addPicked(); }
        };
        addChildComponent (search);

        styleEditor (renameEd, "Preset name");
        renameEd.onReturnKey = [this] { commitRename(); };
        renameEd.onEscapeKey = [this] { renameSlot = -1; renameEd.setVisible (false); askRepaint (screenPx()); };
        renameEd.onFocusLost = [this] { commitRename(); };
        addChildComponent (renameEd);

        styleEditor (headerNameEd, "Preset name");
        headerNameEd.onReturnKey = [this] { commitHeaderName(); };
        headerNameEd.onEscapeKey = [this] { headerRenaming = false; headerNameEd.setVisible (false); askRepaint (screenPx()); };
        headerNameEd.onFocusLost = [this] { commitHeaderName(); };
        addChildComponent (headerNameEd);

        styleEditor (presetNameEd, "User preset name");
        presetNameEd.onReturnKey = [this] { commitPresetName(); };
        presetNameEd.onEscapeKey = [this] { cancelPresetName(); };
        addChildComponent (presetNameEd);

        setOpaque (true);
        setWantsKeyboardFocus (true);
        proc.setMiniTuner (true);                  // mini CHROMA de l'en-tête : l'accordeur écoute tant que la fenêtre est ouverte
        startTimerHz (30);
    }

    ~PedalView() override { PopupMenu::dismissAllActiveMenus(); setLookAndFeel (nullptr); stopTimer(); proc.setMiniTuner (false); OctoSettings::flush(); }

    // ------------------------------------------------------------ public (outil de rendu, éditeur)
    void openBrowser (int blockPos)
    {
        favs = OctoSettings::favourites();
        overlay = Overlay::browser;
        insertPos = blockPos;
        listScroll = 0; listPos = 0.0f;
        picked.clear();
        eng.installedList();
        search.setText ({}, dontSendNotification);
        search.setVisible (true);
        layoutEditors();
        if (isShowing()) search.grabKeyboardFocus();
        askRepaint (screenPx());
    }
    void openParams (int chainIndex)
    {
        if (! isPositiveAndBelow (chainIndex, eng.chain().size())) return;
        overlay = Overlay::params;
        paramGuid = eng.chain().getReference (chainIndex).guid;
        paramPage = 0;
        presetMode = false;
        midiView = false;
        paramListKey = {};
        hideEditors();
        pollParams (true);
        askRepaint (screenPx());
    }
    void openBanks()
    {
        overlay = Overlay::banks;
        hideEditors();
        askRepaint (screenPx());
    }
    void closeOverlay()
    {
        overlay = Overlay::none;
        hideEditors();
        askRepaint (screenPx());
    }
    void setChip (int c) { chip = jlimit (0, kNumChips - 1, c); listScroll = 0; listPos = 0.0f; askRepaint (screenPx()); }
    void pickVisible (int v) { const auto items = filtered(); if (isPositiveAndBelow (v, items.size())) togglePick (items.getReference (v)); }
    void showPresets() { openPresets (true); }
    void setSearch (const String& t) { search.setText (t, sendNotification); }
    void simulateHover (Point<float> unitPos) { updateHover (unitPos); }

    // anti-gel v2 : jamais de calcul lourd ici, les caches seront refaits par le timer quand la taille sera stable
    void resized() override
    {
        s = (float) getWidth() / (float) kW;
        resizeSettle = 4;
        layoutEditors();
        repaint();
    }

    void initCaches()
    {
        dispScale = physScale();
        approxScale = (float) Component::getApproximateScaleFactorForComponent (this);
        resizeSettle = 0;
        rebuildCaches();
    }

    // ------------------------------------------------------------ dessin
    void paint (Graphics& g) override
    {
        lastPaintMs = Time::getMillisecondCounterHiRes();
        repaintAskedMs = 0.0;
        g.fillAll (Colours::black);
        g.drawImage (bg.get(), getLocalBounds().toFloat());
        if (L::drawLogo && logoImg.isValid())             // logo OCTOPUS à sa taille exacte, calé sur la grille des pixels : net
        {
            const auto lr = r (Rectangle<float> (L::logoRight - L::logoW - L::logoPad, L::logoCy - L::logoH * 0.5f - L::logoPad,
                                                 L::logoW + 2.0f * L::logoPad, L::logoH + 2.0f * L::logoPad));
            g.setOpacity (0.94f);
            const bool exact = std::abs ((float) logoImg.getWidth() - lr.getWidth() * dispScale) < 1.5f;   // sinon : cache pas encore refait (redimensionnement)
            g.drawImage (logoImg, exact ? Rectangle<float> ((float) roundToInt (lr.getX()), (float) roundToInt (lr.getY()),
                                                            (float) logoImg.getWidth() / dispScale, (float) logoImg.getHeight() / dispScale)
                                        : lr);
            g.setOpacity (1.0f);
        }

        for (int f = 0; f < 4; ++f)
        {
            const auto fr = r (Rectangle<float> (L::fsSprite, L::fsSprite).withCentre ({ L::fsX[f], L::fsY }));
            if (L::drawFsSprite) g.drawImage (fsOff.get(), fr);
            if (L::drawFsSprite && ledLevel[f] > 0.003f)       // flanc de l'écrou tourné vers la LED
            {
                g.setOpacity (jmin (1.0f, ledLevel[f]) * 0.35f);
                g.drawImage (fsOn.get(), fr);
                g.setOpacity (1.0f);
            }
            if (pressedFs == f)             // comme Echoflex : le bouton s'assombrit tant qu'on appuie
            {
                const auto c = r (Point<float> (L::capX[f], L::capY[f]));
                const float pr = L::capR * s;
                g.setColour (Colours::black.withAlpha (0.25f));
                g.fillEllipse (c.x - pr, c.y - pr, pr * 2.0f, pr * 2.0f);
            }
            drawRing (g, f, ledLevel[f], ledColour[f]);         // anneau lumineux autour du bouton qui s'enfonce
            drawLetter (g, f, ledPos (f), ledLevel[f], ledColour[f]);
        }

        drawComboMark (g);
        drawScreen (g);
    }

    // sérigraphie des doubles appuis : trait fin entre deux footswitchs voisins, l'action au milieu (CHROMA, BANK +, CC 26...)
    // Sérigraphie des doubles appuis, dessin de l'utilisateur : entre A et B  ] E [ , entre C et D  ] F [ ,
    // entre B et C  — ı|ı — (accordeur). Traits, lettre et pictogramme forment une seule LED : gris gravé éteint,
    // couleur de l'effet commandé allumé (vert de CHROMA pour l'accordeur).
    static constexpr float kMarkW = 1.4f;                 // épaisseur des traits (unités)
    static Point<float> markCentre (int k) { return k == 0 ? Point<float> (338.5f, 868.05f) : k == 2 ? Point<float> (906.4f, 868.05f) : Point<float> (624.35f, 868.3f); }
    static Rectangle<float> comboRect (int k) { const auto c = markCentre (k); return k == 1 ? Rectangle<float> (c.x - 30.0f, c.y - 26.0f, 60.0f, 52.0f) : Rectangle<float> (c.x - 104.0f, c.y - 31.0f, 208.0f, 62.0f); }

    // marquages du plan du 07/11/2026 (design/svg : E.svg, F.svg, Chroma_octo.svg) : contours pleins, repère SVG 1000 x 500
    // centré en (500, 250), posés à leur place sur la façade (centre et échelle relevés sur le plan)
    static const Path& markSvg (int k)
    {
        static const Path e = Drawable::parseSVGPath ("M 900 360.99 L 563.34 360.99 L 563.34 318.8 L 483.57 318.8 L 483.57 265.6 L 554.1 265.6 L 554.1 230.71 L 483.57 230.71 L 483.57 181.21 L 563.34 181.21 L 563.34 139.01 L 900 139.01 L 900 144.77 L 569.1 144.77 L 569.1 186.97 L 489.33 186.97 L 489.33 224.94 L 559.86 224.94 L 559.86 271.36 L 489.33 271.36 L 489.33 313.03 L 569.1 313.03 L 569.1 355.23 L 900 355.23 Z M 436.66 360.99 L 100 360.99 L 100 355.22 L 430.9 355.22 L 430.9 144.77 L 100 144.77 L 100 139.01 L 436.66 139.01 Z");
        static const Path f = Drawable::parseSVGPath ("M 900 360.99 L 483.57 360.99 L 483.57 265.6 L 554.1 265.6 L 554.1 230.71 L 483.57 230.71 L 483.57 181.21 L 563.34 181.21 L 563.34 139.01 L 900 139.01 L 900 144.77 L 569.1 144.77 L 569.1 186.97 L 489.33 186.97 L 489.33 224.94 L 559.86 224.94 L 559.86 271.36 L 489.33 271.36 L 489.33 355.23 L 900 355.23 Z M 436.66 360.99 L 100 360.99 L 100 355.23 L 430.9 355.23 L 430.9 144.77 L 100 144.77 L 100 139.01 L 436.66 139.01 Z");
        static const Path t = Drawable::parseSVGPath ("M492.35,417.71V82.29c0-3.34,2.23-6.36,5.49-7.09c4.72-1.07,8.91,2.42,8.91,6.87v335.87 c0,4.45-4.19,7.94-8.91,6.87C494.58,424.07,492.35,421.05,492.35,417.71z M391.55,341.25V161.48c0-3.34,2.23-6.36,5.49-7.09c4.72-1.07,8.91,2.42,8.91,6.87v180.22 c0,4.45-4.19,7.94-8.91,6.87C393.78,347.61,391.55,344.59,391.55,341.25z M290.75,315.92v-129.1c0-3.34,2.23-6.36,5.49-7.09c4.72-1.07,8.91,2.42,8.91,6.87v129.55 c0,4.45-4.19,7.94-8.91,6.87C292.98,322.27,290.75,319.25,290.75,315.92z M694.85,315.92v-129.1c0-3.34,2.23-6.36,5.49-7.09c4.72-1.07,8.91,2.42,8.91,6.87v129.55 c0,4.45-4.19,7.94-8.91,6.87C697.08,322.28,694.85,319.26,694.85,315.92z M594.05,341.25V161.48c0-3.34,2.23-6.36,5.49-7.09c4.72-1.07,8.91,2.42,8.91,6.87v180.22 c0,4.45-4.19,7.94-8.91,6.87C596.28,347.61,594.05,344.59,594.05,341.25z");
        return k == 0 ? e : k == 2 ? f : t;
    }
    static Point<float> markPos (int k) { return k == 0 ? Point<float> (338.5f, 868.05f) : k == 2 ? Point<float> (906.4f, 868.05f) : Point<float> (624.35f, 868.3f); }
    static float markScale (int k) { return k == 1 ? 0.1145f : 0.2554f; }
    // tunerIcon && wordW < 0 : anciens traits de chaque côté du symbole de l'accordeur (plus dessinés)
    Path markPath (int k, bool tunerIcon, float wordW) const
    {
        if (k == 1 && tunerIcon && wordW < 0.0f) return {};
        Path p (markSvg (k));
        const auto c = markPos (k);
        p.applyTransform (AffineTransform::translation (-500.0f, -250.0f).scaled (markScale (k)).translated (c).scaled (s));
        return p;
    }

    void drawComboMark (Graphics& g)
    {
        for (int k = 0; k < OctopusEngine::kCombos; ++k)
        {
            const int key = OctopusEngine::keyOfCombo (k);
            const bool icon = key < 0;                              // entre B et C : toujours le symbole de l'accordeur
            const auto word = key < 0 && ! icon ? eng.comboLabel (k) : String();
            if (key < 0 && ! icon && word.isEmpty()) continue;
            const auto fWord = Font (FontOptions (tfBold).withHeight (15.0f * s)).withExtraKerningFactor (0.18f);
            const float ww = word.isEmpty() ? 0.0f : GlyphArrangement::getStringWidth (fWord, word) / s;
            const int led = key >= 0 ? key : kTunerLed;
            const auto p = markPath (k, icon, ww);
            if (icon) drawLitPath (g, markPath (k, true, -1.0f), 0.0f, ledColour[led]);      // traits de l'accordeur : jamais allumés
            drawLitPath (g, p, ledLevel[led], ledColour[led]);   // B+C (« G ») : même halo que les lettres (Letter glow)
            if (hoverCombo == k) { g.setColour (Colours::white.withAlpha (0.28f)); g.fillPath (p); }
            if (word.isNotEmpty())
            {
                const auto c = markCentre (k);
                g.setColour (Colour (0xffb9c0ca).withAlpha (0.78f));
                g.setFont (fWord);
                g.drawText (word, r (Rectangle<float> (c.x - ww * 0.5f - 4.0f, c.y - 12.0f, ww + 8.0f, 24.0f)), Justification::centred, false);
            }
        }
    }
    int comboAt (Point<float> unit) const
    {
        for (int k = 0; k < OctopusEngine::kCombos; ++k) if (comboRect (k).contains (unit)) return k;
        return -1;
    }
    void comboMenu (int k)
    {
        if (OctopusEngine::keyOfCombo (k) >= 0) { footswitchMenu (OctopusEngine::keyOfCombo (k)); return; }   // E / F : menu de A-D
        // symbole de l'accordeur : plus de double appui B+C, l'accordeur se commande par MIDI learn (ou un clic)
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofFx (FxCat::tuner, FxCat::iconFamily), "Tuner", "CHROMA");
        addLearnItems (m, OctopusEngine::tunerToggle);
        m.addSeparator();
        m.addItem (310, "Color & Glow");
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, k] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            if (r == 310) { sp->tunerColourMenu (sp->r (comboRect (k))); return; }
            sp->handleMidi (r);
            sp->askRepaint (sp->getLocalBounds());
        });
    }

private:
    OctopusProcessor& proc;
    OctopusEngine& eng;
    ScaledCache bg, fsOff, fsOn;
    Image logoSrc, logoImg;               // logo OCTOPUS : source pleine résolution, puis image à la taille exacte en pixels
    Image glow[4];
    Colour glowColour[4];
    Typeface::Ptr tfMedium, tfBold, tfRegular;
    MenuStyle::MenuLnf menuLnf;                                    // apparence des menus clic droit
    MenuStyle::Fonts menuFonts() const { return { tfRegular, tfMedium, tfBold }; }
    PopupMenu& styled (PopupMenu& m) { m.setLookAndFeel (&menuLnf); return m; }
    // en-tête de menu : pastille + titre + sous-titre, puis un filet
    void menuHeader (PopupMenu& m, MenuStyle::Badge b, const String& title, const String& sub = {})
    {
        m.addCustomItem (9001, std::make_unique<MenuStyle::Header> (menuFonts(), b, title, sub), nullptr, title);
        m.addSeparator();
    }
    MenuStyle::Badge fxBadge (int chainIndex) const
    {
        const auto& f = eng.chain().getReference (chainIndex);
        return MenuStyle::Badge::ofFx (eng.blockKind (chainIndex), FxCat::iconOf (f.name));
    }
    float s = 1.0f;

    // anti-gel v2
    float dispScale = 1.0f, approxScale = 1.0f;
    int resizeSettle = 0;
    double lastPaintMs = 0.0, repaintAskedMs = 0.0, lastScaleCheckMs = 0.0;

    // état affiché
    int drawnGen = -1;
    // éclairage : 0-3 = A B C D, 4 = E, 5 = F, 6 = pictogramme de l'accordeur (B+C)
    static constexpr int kLeds = 7, kTunerLed = 6;
    float ledLevel[kLeds] {};
    Colour ledColour[kLeds] { kPresetLed, kPresetLed, kPresetLed, kPresetLed, kPresetLed, kPresetLed, kPresetLed };
    int pressedFs = -1;

    Overlay overlay = Overlay::none;
    int firstRow = 0;                              // défilement de la grille (rangées)
    int hoverCombo = -1;
    int hoverSlot = -1, hoverBtn = bNone, hoverItem = -1, hoverFooter = -1, hoverParam = -1;
    int hoverLaneAdd = -1, hoverNodeKey = -1;      // case « + » d'une voie vide, point Y / M survolé (groupe * 2 + merge)
    Point<float> hoverPos { -100.0f, -100.0f };
    int mixGroup = -1, mixDragLane = -1, mixDragWhat = -1;   // mixeur d'un groupe (clic sur M)
    // clic / glisser sur un bloc
    int downSlot = -1;
    Point<float> downPos;
    bool dragging = false;
    Point<float> dragPos;
    int dropPos = -1;
    int pendingToggle = -1;                        // indice REAPER, basculé après le délai du double-clic
    double pendingToggleAt = 0.0;
    // navigateur
    TextEditor search, renameEd, presetNameEd, headerNameEd;
    bool headerRenaming = false;           // nom du preset chargé en cours de saisie (clic sur le nom en haut)
    int chip = 0, listScroll = 0, insertPos = 0;
    float listPos = 0.0f;
    bool searchHadFocus = false;                                          // défilement affiché (glisse vers listScroll)
    // paramètres
    String paramGuid;
    int paramPage = 0, paramDrag = -1;
    struct ParamView { int index; String name, text; double value; int steps; };
    // paramètres affichés : ceux du plugin sans le MIDI, ou seulement le MIDI (bouton MIDI) ; liste faite une fois par effet
    bool midiView = false;
    Array<int> paramList;
    String paramListKey;
    // double-clic sur un bloc : détecté par Octopus lui-même (même bloc, moins de 650 ms entre les deux appuis),
    // et la bascule ON / OFF d'un clic simple est annulée si le double-clic arrive après elle
    String lastClickGuid, toggledGuid;
    double lastClickMs = 0.0, toggledAtMs = 0.0;
public:
    int doubleClicks = 0;                       // nombre de double-clics reconnus (outil de test)
private:
    Point<float> lastClickPos;
    Array<ParamView> params;
    int paramCount = 0, pollTick = 0;
    String drawnParamSig;
    // presets du plugin (dans le panneau des paramètres)
    bool presetMode = false;
    PluginPresets plist;
    String plistGuid;
    std::vector<UserFxPreset> upresets;     // presets utilisateur enregistrés par Octopus pour ce plugin (en tête de liste)
    String upPlugin;                        // nom d'origine du plugin (clé du fichier)
    int userCur = -1;                       // preset utilisateur chargé en dernier
    int presetEdIndex = -2;                 // -2 : pas de saisie ; -1 : nouveau preset ; >= 0 : renommer ce preset
    int presetScroll = 0;
    // navigateur : effets choisis (dans l'ordre), ajoutés en une fois
    Array<InstalledFx> picked;
    // accordeur (CHROMA)
    Typeface::Ptr tfMono;
    float strobePhase = 0.0f;
    bool drawnTuner = false;
    // banks
    int renameSlot = -1;
    std::unique_ptr<FileChooser> chooser;

    // ------------------------------------------------------------ repères
    Rectangle<float> r (Rectangle<float> d) const { return d * s; }
    Point<float> r (Point<float> d) const { return d * s; }
    // écran : repère local -> pixels de la fenêtre
    // écran : repère local 1064 x 624, agrandi de kG dans la vitre
    Rectangle<float> S (Rectangle<float> local) const { return (local * L::kG + L::glass.getPosition()) * s; }
    Point<float> S (Point<float> local) const { return (local * L::kG + L::glass.getPosition()) * s; }
    float S (float v) const { return v * L::kG * s; }
    Point<float> toLocal (Point<float> px) const { return (px / s - L::glass.getPosition()) / L::kG; }
    Rectangle<int> screenPx() const { return r (L::glass).toNearestInt().expanded (2); }

    Point<float> ledPos (int f) const
    {
        if (f >= 4) return { (L::fsX[f == 4 ? 0 : f == 5 ? 2 : 1] + L::fsX[f == 4 ? 1 : f == 5 ? 3 : 2]) * 0.5f, L::fsY };   // E, F, accordeur : entre deux footswitchs
        return { L::ledXs[f] + L::ledDx, L::ledY };
    }
    Rectangle<float> glowRect (int f) const { return Rectangle<float> (f >= 4 ? 240.0f : 128.0f, f >= 4 ? 100.0f : 128.0f).withCentre (ledPos (f)); }

    float physScale() const
    {
        if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds())) return jmax (1.0f, (float) d->scale);
        return 1.0f;
    }

    void askRepaint (Rectangle<int> area)
    {
        if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
        repaint (area);
    }

    Font font (const Typeface::Ptr& tf, float unitsHigh) const { return Font (FontOptions (tf).withHeight (unitsHigh * L::kG * s)); }   // texte de l'écran

    void styleEditor (TextEditor& ed, const String& hint)
    {
        ed.setColour (TextEditor::backgroundColourId, Colours::transparentBlack);   // fond et cadre arrondis dessinés par menuLnf
        ed.getProperties().set ("roundBg", (int) kScrPanel2.getARGB());
        ed.setColour (TextEditor::outlineColourId, kFaint);
        ed.setColour (TextEditor::focusedOutlineColourId, kAccent);
        ed.setColour (TextEditor::textColourId, kText);
        ed.setColour (TextEditor::highlightColourId, kAccent.withAlpha (0.35f));
        ed.setColour (CaretComponent::caretColourId, kAccent);
        ed.setTextToShowWhenEmpty (hint, kDim);
        ed.setSelectAllWhenFocused (true);
        ed.setEscapeAndReturnKeysConsumed (true);
    }

    void layoutEditors()
    {
        const float h = 18.0f * s;
        search.setBounds (S (kSearchR.withTrimmedLeft (40.0f).withTrimmedRight (8.0f)).toNearestInt());   // à droite de la loupe
        search.setFont (font (tfMedium, 17.0f));
        search.setIndents (0, jmax (0, roundToInt ((S (kSearchR.getHeight()) - h) * 0.5f) - 1));
        if (renameSlot >= 0)
        {
            renameEd.setBounds (S (cardRect (renameSlot).withTrimmedLeft (80.0f).withWidth (250.0f).reduced (0.0f, 14.0f)).toNearestInt());
            renameEd.setFont (font (tfMedium, 18.0f));
            renameEd.setIndents (roundToInt (S (8.0f)), jmax (0, roundToInt ((S (36.0f) - h) * 0.5f) - 1));
        }
        headerNameEd.setBounds (S (headerNameRect()).toNearestInt());
        headerNameEd.setFont (font (tfMedium, 24.0f));
        headerNameEd.setIndents (roundToInt (S (6.0f)), jmax (0, roundToInt ((S (36.0f) - 24.0f * s) * 0.5f) - 1));
        presetNameEd.setBounds (S (Rectangle<float> (104.0f, 132.0f, 540.0f, 36.0f)).toNearestInt());
        presetNameEd.setFont (font (tfMedium, 18.0f));
        presetNameEd.setIndents (roundToInt (S (8.0f)), jmax (0, roundToInt ((S (36.0f) - h) * 0.5f) - 1));
    }

    void hideEditors()
    {
        search.setVisible (false);
        renameSlot = -1;
        renameEd.setVisible (false);
        presetEdIndex = -2;
        presetNameEd.setVisible (false);
        headerRenaming = false;
        headerNameEd.setVisible (false);
    }

    // ---- nom du preset chargé : clic sur le nom en haut de l'écran pour le changer
    Rectangle<float> headerNameRect() const { return { kNameX, kHeaderH * 0.5f - 26.0f, jmax (120.0f, chromaLeft() - kHG - kNameX), 34.0f }; }
    static Rectangle<float> badgeRect() { return { kHdrGap, (kHeaderH - kBadge) * 0.5f, kBadge, kBadge }; }
    void startHeaderRename()
    {
        const int cur = eng.current();
        if (cur < 0) { eng.say ("Load a preset (or SAVE the chain) to name it"); return; }
        headerRenaming = true;
        layoutEditors();
        headerNameEd.setText (eng.preset (cur).name, dontSendNotification);
        headerNameEd.setVisible (true);
        headerNameEd.grabKeyboardFocus();
        headerNameEd.selectAll();
        askRepaint (screenPx());
    }
    void commitHeaderName()
    {
        if (! headerRenaming) return;
        headerRenaming = false;
        headerNameEd.setVisible (false);
        if (eng.current() >= 0) eng.rename (eng.current(), headerNameEd.getText());
        askRepaint (screenPx());
    }

    // ------------------------------------------------------------ caches (timer seulement)
    void rebuildCaches()
    {
        if (getWidth() <= 0) return;
        const float ps = s * dispScale;
        for (auto* c : { &bg, &fsOff, &fsOn }) c->prepare (ps);
        logoImg = crispRescale (logoSrc, roundToInt ((L::logoW + 2.0f * L::logoPad) * ps), roundToInt ((L::logoH + 2.0f * L::logoPad) * ps));
        layoutEditors();
        askRepaint (getLocalBounds());
    }

    // La façade reçoit la lumière de la LED : fond x éclairement x couleur (le grain s'allume, les creux restent sombres),
    // tache serrée + diffusion large et faible, très léger halo optique autour de la lentille. Calque avec alpha
    // (seule la lumière ajoutée), affiché avec l'intensité de la LED.
    void buildGlow (int f)
    {
        glowColour[f] = ledColour[f];
        const auto& src = bg.get();
        const float ppu = bg.pixelsPerUnit();
        const auto gr = glowRect (f);
        const int x0 = roundToInt (gr.getX() * ppu), y0 = roundToInt (gr.getY() * ppu);
        const int w = roundToInt (gr.getWidth() * ppu), h = roundToInt (gr.getHeight() * ppu);
        if (w <= 0 || h <= 0) return;
        Image out (Image::ARGB, w, h, true);
        const Image::BitmapData sd (src, Image::BitmapData::readOnly);
        Image::BitmapData od (out, Image::BitmapData::writeOnly);
        const auto c = ledPos (f);
        const float cr = ledColour[f].getFloatRed(), cg = ledColour[f].getFloatGreen(), cb = ledColour[f].getFloatBlue();
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const int sx = x0 + x, sy = y0 + y;
                if (sx < 0 || sy < 0 || sx >= src.getWidth() || sy >= src.getHeight()) continue;
                const float ux = (float) sx / ppu, uy = (float) sy / ppu;
                const float d = std::hypot (ux - c.x, (uy - c.y) * 1.08f);
                const float tight = 1.0f / std::pow (1.0f + (d / 13.0f) * (d / 13.0f), 1.5f);
                const float wide = 0.20f * std::exp (-d / 36.0f);
                const float win = jlimit (0.0f, 1.0f, (gr.getWidth() * 0.5f - 4.0f - d) / 22.0f);   // retombe à zéro avant le bord du calque
                const float illum = (1.35f * tight + wide) * win * win;
                const auto p = sd.getPixelColour (sx, sy);
                const float lum = 0.25f + (p.getFloatRed() + p.getFloatGreen() + p.getFloatBlue()) / 3.0f;
                const float halo = 0.10f * std::exp (-(d / 13.0f) * (d / 13.0f));
                const float k = illum * lum * 1.9f + halo;
                const float ar = jmin (1.0f, k * cr), ag = jmin (1.0f, k * cg), ab = jmin (1.0f, k * cb);
                const float a = jmin (1.0f, jmax (ar, jmax (ag, ab)));
                if (a < 0.002f) continue;
                od.setPixelColour (x, y, Colour::fromFloatRGBA (ar / a, ag / a, ab / a, a));
            }
        glow[f] = out;
    }

    // ------------------------------------------------------------ lettre rétroéclairée A / B / C / D (à la place d'une LED)
    // Éteinte : lettre gravée dans la façade, verre fumé sombre ; le bord bas droit du creux accroche la lumière (en haut à gauche).
    // Allumée : seul le tracé de la lettre s'éclaire, dans la couleur du mode (aucune tache de lumière sur la façade).
    void drawLetter (Graphics& g, int f, Point<float> c0, float lvl, Colour col)
    {
        GlyphArrangement ga;
        ga.addLineOfText (Font (FontOptions (tfBold).withHeight (L::letterFont * s)), fsLetter (f), 0.0f, 0.0f);
        Path p;
        ga.createPath (p);
        const auto c = r (c0);
        const auto b = p.getBounds();
        p.applyTransform (AffineTransform::translation (c.x - b.getCentreX(), c.y - b.getCentreY()));
        drawLitPath (g, p, lvl, col);
    }

    void drawLitPath (Graphics& g, const Path& p, float lvl, Colour col, int halo = -1)
    {
        const auto pb = p.getBounds();

        // gravure : arête claire en bas à droite, ombre en haut à gauche dans le creux
        g.setColour (Colours::white.withAlpha (0.16f));
        g.fillPath (p, AffineTransform::translation (0.9f * s, 1.0f * s));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillPath (p, AffineTransform::translation (-0.6f * s, -0.7f * s));
        // éteinte : lettre gris clair sérigraphiée, comme sur le visuel du boîtier, à peine teintée de la couleur du footswitch
        g.setGradientFill (ColourGradient (Colour (0xffbcc2cc).interpolatedWith (col, 0.10f), 0.0f, pb.getY(),
                                           Colour (0xff9aa1ad).interpolatedWith (col, 0.07f), 0.0f, pb.getBottom(), false));
        g.fillPath (p);
        if (lvl <= 0.003f) return;

        // seule la lettre s'éclaire : verre teinté rétroéclairé, intensité modérée, très léger débord lumineux sur son contour
        const float k = lvl;
        // halo discret qui épouse la forme de la lettre (contours de plus en plus larges et transparents)
        const float h = (float) (halo >= 0 ? halo : haloPc) / 100.0f;   // opacité du halo réglée par l'utilisateur (GLOW)
        for (float wdt = 28.0f; wdt > 5.0f && h > 0.0f; wdt -= 2.5f)
        {
            g.setColour (col.withAlpha (jmin (1.0f, 0.024f * lvl * h)));
            g.strokePath (p, PathStrokeType (wdt * s, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (col.withAlpha (jmin (1.0f, 0.06f * lvl * h)));
        g.strokePath (p, PathStrokeType (3.4f * s, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col.withAlpha (jmin (1.0f, 0.16f * lvl * jmax (h, 0.5f))));
        g.strokePath (p, PathStrokeType (1.8f * s, PathStrokeType::curved, PathStrokeType::rounded));
        ColourGradient body (col.interpolatedWith (Colours::white, 0.45f).withAlpha (k), 0.0f, pb.getY(),
                             col.withMultipliedBrightness (0.95f).withAlpha (k), 0.0f, pb.getBottom(), false);
        g.setGradientFill (body);
        g.fillPath (p);
    }

    // ------------------------------------------------------------ anneau lumineux autour du footswitch (comme la Phase 90)
    // Dans le footswitch, dans la gorge entre le bouton qui s'enfonce et la bague.
    // Éteint : le diffuseur gris du visuel. Allumé : verre éclairé dans la couleur du footswitch (couleur du preset en
    // mode PRESET, du bloc en mode STOMP), cœur plus clair, et halo qui déborde sur la façade.
    void drawRing (Graphics& g, int f, float lvl, Colour col)
    {
        if (lvl <= 0.003f) return;
        const auto c = r (Point<float> (L::ringCx[f], L::ringCy[f]));
        const float rx = L::ringRx[f] * s, ry = L::ringRy[f] * s, wdt = L::ringW * s;
        Path ring;
        ring.addEllipse (c.x - rx, c.y - ry, rx * 2.0f, ry * 2.0f);
        for (float hw = 13.0f; hw > 3.0f; hw -= 1.5f)                    // halo : traits de plus en plus larges et transparents
        {
            g.setColour (col.withAlpha (0.045f * lvl));
            g.strokePath (ring, PathStrokeType (hw * s));
        }
        g.setColour (col.withAlpha (0.85f * lvl));
        g.strokePath (ring, PathStrokeType (wdt));
        g.setColour (col.interpolatedWith (Colours::white, 0.55f).withAlpha (0.9f * lvl));   // cœur du diffuseur
        g.strokePath (ring, PathStrokeType (wdt * 0.42f));
    }

    // (ancienne LED, gardée pour référence : plus dessinée)
    [[maybe_unused]] void drawLed (Graphics& g, Point<float> c0, float lvl, Colour col)
    {
        const auto c = r (c0);
        const float rr = L::ledR * s;
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - rr + 0.9f * s, c.y - rr + 1.4f * s, rr * 2.0f, rr * 2.0f);
        ColourGradient chrome (Colour (0xffd2d4d8), c.x - rr * 0.8f, c.y - rr * 0.9f, Colour (0xff2a2c31), c.x + rr * 0.8f, c.y + rr * 0.9f, false);
        chrome.addColour (0.45, Colour (0xff74777e));
        g.setGradientFill (chrome);
        g.fillEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f);
        if (lvl > 0.003f)
        {
            g.setGradientFill (ColourGradient (col.withAlpha (0.35f * lvl), c.x, c.y, col.withAlpha (0.0f), c.x + rr, c.y, true));
            g.fillEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f);
        }
        const float gr = rr * 0.77f;
        g.setGradientFill (ColourGradient (Colour (0xff020203), c.x - gr * 0.6f, c.y - gr * 0.7f, Colour (0xff16181c), c.x + gr * 0.7f, c.y + gr * 0.8f, false));
        g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);
        const float lr = rr * 0.62f;
        const auto dark = col.withMultipliedBrightness (0.28f).withMultipliedSaturation (0.8f);
        g.setGradientFill (ColourGradient (dark, c.x - lr * 0.3f, c.y - lr * 0.4f, dark.withMultipliedBrightness (0.25f), c.x + lr, c.y + lr, true));
        g.fillEllipse (c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f);
        if (lvl > 0.003f)
        {
            const float hot = jlimit (0.0f, 1.0f, lvl * 1.05f);
            ColourGradient body (Colours::white.withAlpha (hot), c.x + lr * 0.03f, c.y + lr * 0.02f, col.withMultipliedBrightness (0.6f).withAlpha (lvl), c.x + lr, c.y, true);
            body.addColour (0.18, col.interpolatedWith (Colours::white, 0.7f).withAlpha (hot));
            body.addColour (0.45, col.withAlpha (lvl));
            g.setGradientFill (body);
            g.fillEllipse (c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f);
        }
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawEllipse (c.x - lr, c.y - lr, lr * 2.0f, lr * 2.0f, 0.7f * s);
        ColourGradient sp (Colours::white.withAlpha (0.30f + 0.15f * lvl), c.x - lr * 0.38f, c.y - lr * 0.50f,
                           Colours::white.withAlpha (0.0f), c.x - lr * 0.38f + lr * 0.36f, c.y - lr * 0.50f, true);
        g.setGradientFill (sp);
        g.fillEllipse (c.x - lr * 0.74f, c.y - lr * 0.80f, lr * 0.72f, lr * 0.56f);
    }

    // ------------------------------------------------------------ grille de blocs
    // 7 colonnes ; un groupe de voies parallèles (Routing.h) occupe autant de colonnes que sa voie la plus longue,
    // ne se coupe jamais entre deux rangées, et sa rangée grandit d'un écart de voie par voie en plus (blocs de même taille).
    static constexpr float kLanePitch = 140.0f;   // voie B : un pas de rangée sous la voie A (maquette v1.18.71)
    enum CellKind { cBlock = 0, cAdd, cLaneAdd, cFolded };
    struct GCell { int slot = -1, kind = cBlock, group = -1, lane = 0, col = 0, row = 0; };
    struct GGroup { int group = -1, row = 0, col0 = 0, width = 1, lanes = 2; bool folded = false; };
    struct GRow { float top = 0.0f; int lanes = 1, lastCol = 0; };
    mutable std::vector<GCell> gCells;
    mutable std::vector<GGroup> gGroups;
    mutable std::vector<GRow> gRows;
    mutable int gridGen = -1;

    void buildGrid() const
    {
        if (gridGen == eng.generation() && ! gRows.empty()) return;
        gridGen = eng.generation();
        gCells.clear(); gGroups.clear(); gRows.clear();
        const auto& b = eng.blocks();
        int row = 0, col = 0;
        gRows.push_back ({});
        auto place = [&] (int w)
        {
            if (col > 0 && col + w > kCols) { gRows.back().lastCol = col - 1; ++row; col = 0; gRows.push_back ({}); }
        };
        for (int k = 0; k < b.size();)
        {
            const int i = b[k];
            const int gi = eng.groupAt (i);
            if (eng.roleAt (i) == Routing::split && gi >= 0)
            {
                const auto& G = eng.groups()[(size_t) gi];
                const bool fold = eng.isFolded (gi);
                int w = 1;
                if (! fold) for (auto& l : G.lanes) w = jmax (w, l.size());
                place (w);
                GGroup gg; gg.group = gi; gg.row = row; gg.col0 = col; gg.width = w; gg.lanes = (int) G.lanes.size(); gg.folded = fold;
                gGroups.push_back (gg);
                if (fold) gCells.push_back ({ k, cFolded, gi, 0, col, row });
                else
                {
                    for (int l = 0; l < (int) G.lanes.size(); ++l)
                    {
                        const auto& ln = G.lanes[(size_t) l];
                        if (ln.isEmpty()) gCells.push_back ({ -1, cLaneAdd, gi, l, col, row });
                        for (int j = 0; j < ln.size(); ++j) gCells.push_back ({ b.indexOf (ln[j]), cBlock, gi, l, col + j, row });
                    }
                    gRows.back().lanes = jmax (gRows.back().lanes, (int) G.lanes.size());
                }
                col += w;
                const int mk = b.indexOf (G.merge);
                k = mk >= 0 ? mk + 1 : k + 1;
                continue;
            }
            place (1);
            gCells.push_back ({ k, cBlock, -1, 0, col, row });
            ++col; ++k;
        }
        place (1);
        gCells.push_back ({ b.size(), cAdd, -1, 0, col, row });
        gRows.back().lastCol = col;
        float top = kGridTop;
        for (auto& r : gRows) { r.top = top; top += kRowPitch + kLanePitch * (float) (r.lanes - 1); }
    }

    int numSlots() const { return eng.blocks().size() + 1; }           // blocs + case « ajouter »
    int totalRows() const { buildGrid(); return (int) gRows.size(); }
    float rowTop (int row) const
    {
        buildGrid();
        const int f = jlimit (0, (int) gRows.size() - 1, firstRow);
        if (row >= (int) gRows.size()) return gRows.back().top - gRows[(size_t) f].top + kGridTop + kRowPitch * (float) (row - (int) gRows.size() + 1) + kLanePitch * (float) (gRows.back().lanes - 1);
        return gRows[(size_t) row].top - gRows[(size_t) f].top + kGridTop;
    }
    int rowLanes (int row) const { buildGrid(); return isPositiveAndBelow (row, (int) gRows.size()) ? gRows[(size_t) row].lanes : 1; }
    float rowCy (int row) const { return rowTop (row) + kTileDy; }
    bool rowFits (int row) const { return rowCy (row) + kLanePitch * (float) (rowLanes (row) - 1) + kTile * 0.5f + 26.0f <= kFooterLine + 4.0f; }
    void drawMoreArrows (Graphics& g)
    {
        if (overlay == Overlay::none && maxFirstRow() > 0)                                   // flèches bleues seules, sous / au-dessus du point OUT (maquette)
        {
            auto tri = [&] (Point<float> c, bool down)
            {
                Path t;
                const float hw = 5.37f, hh = 3.66f, d = down ? 1.0f : -1.0f;
                t.addTriangle (S (Point<float> (c.x - hw, c.y - hh * d)), S (Point<float> (c.x + hw, c.y - hh * d)), S (Point<float> (c.x, c.y + hh * d)));
                g.setColour (kPresetLed);
                g.fillPath (t);
            };
            if (firstRow > 0) tri (moreUpArrow().getCentre(), false);
            if (firstRow < maxFirstRow()) tri (moreDownArrow().getCentre(), true);
            // nombre de pages, discret, à gauche de la flèche du bas (variante B validée)
            g.setFont (font (tfBold, 15.0f));
            g.setColour (kDim);
            g.drawText (String (firstRow + 1) + " / " + String (maxFirstRow() + 1),
                        S (Rectangle<float> (connXR() - 14.0f - 120.0f, kMoreArrowY - 10.0f, 120.0f, 20.0f)), Justification::centredRight);
        }
    }
    Rectangle<float> moreDownArrow() const { return Rectangle<float> (24.0f, 18.0f).withCentre ({ connXR(), kMoreArrowY }); }
    Rectangle<float> moreUpArrow() const   { return Rectangle<float> (24.0f, 18.0f).withCentre ({ connXR(), kGridTop - 14.0f }); }
    bool rowVisible (int row) const { return row >= firstRow && row < totalRows() && rowFits (row); }
    int visibleRows() const { int n = 0; for (int r = firstRow; r < totalRows() && rowVisible (r); ++r) ++n; return jmax (1, n); }
    int maxFirstRow() const
    {
        const int n = totalRows();
        for (int f = 0; f < n; ++f)
        {
            const float h = gRows[(size_t) n - 1].top - gRows[(size_t) f].top + kTileDy + kLanePitch * (float) (gRows[(size_t) n - 1].lanes - 1) + kTile * 0.5f + 26.0f;
            if (kGridTop + h <= kFooterLine + 4.0f) return f;
        }
        return jmax (0, n - 1);
    }
    Point<float> cellCentre (const GCell& c) const { return { kColX0 + kColPitch * (float) c.col, rowCy (c.row) + kLanePitch * (float) c.lane }; }
    const GCell* cellOfSlot (int slot) const
    {
        buildGrid();
        for (auto& c : gCells) if (c.slot == slot && c.kind != cLaneAdd) return &c;
        return nullptr;
    }
    // points Y (split) et M (merge) d'un groupe déplié
    Point<float> nodeY (const GGroup& gg) const { return { kColX0 + kColPitch * ((float) gg.col0 - 0.5f), rowCy (gg.row) }; }
    Point<float> nodeM (const GGroup& gg) const { return { kColX0 + kColPitch * ((float) (gg.col0 + gg.width - 1) + 0.5f), rowCy (gg.row) }; }
    const GGroup* gridGroup (int g) const { buildGrid(); for (auto& gg : gGroups) if (gg.group == g) return &gg; return nullptr; }
    Point<float> slotCentre (int slot) const
    {
        if (auto* c = cellOfSlot (slot)) return cellCentre (*c);
        // repère d'un groupe (masqué) : son point Y / M
        if (isPositiveAndBelow (slot, eng.blocks().size()))
        {
            const int i = eng.blocks()[slot];
            if (auto* gg = gridGroup (eng.groupAt (i))) return eng.roleAt (i) == Routing::merge ? nodeM (*gg) : nodeY (*gg);
        }
        return { -1000.0f, -1000.0f };
    }
    Rectangle<float> tileRect (int slot) const { return Rectangle<float> (kTile, kTile).withCentre (slotCentre (slot)); }
    bool slotVisible (int slot) const { auto* c = cellOfSlot (slot); return c != nullptr && rowVisible (c->row); }
    Point<float> inDot() const  { return { kColX0 - kColPitch * 0.5f - 6.0f, rowCy (0) }; }
    Point<float> outDot() const { const auto* c = cellOfSlot (eng.blocks().size()); return { kColX0 + kColPitch * ((float) kCols - 0.5f) + 6.0f, rowCy (c != nullptr ? c->row : 0) }; }
    bool inVisible() const  { return firstRow == 0; }
    bool outVisible() const { const auto* c = cellOfSlot (eng.blocks().size()); return c != nullptr && rowVisible (c->row); }

    int slotAt (Point<float> p) const
    {
        if (p.y < kGridTop - 6.0f || p.y > kFooterLine) return -1;
        buildGrid();
        for (auto& c : gCells)
            if (c.kind != cLaneAdd && rowVisible (c.row)
                && Rectangle<float> (kTile, kTile).withCentre (cellCentre (c)).expanded (12.0f, 6.0f).withHeight (kTile + 34.0f).contains (p)) return c.slot;
        return -1;
    }
    int laneAddAt (Point<float> p) const          // case « + » d'une voie vide (indice dans gCells), -1 sinon
    {
        if (p.y < kGridTop - 6.0f || p.y > kFooterLine) return -1;
        buildGrid();
        for (int k = 0; k < (int) gCells.size(); ++k)
        {
            const auto& c = gCells[(size_t) k];
            if (c.kind == cLaneAdd && rowVisible (c.row) && Rectangle<float> (kTile, kTile).withCentre (cellCentre (c)).expanded (8.0f).contains (p)) return k;
        }
        return -1;
    }
    // point Y ou M sous la souris : groupe (et merge = true pour M), -1 sinon
    int nodeAt (Point<float> p, bool& isMerge) const
    {
        buildGrid();
        for (auto& gg : gGroups)
        {
            if (gg.folded || ! rowVisible (gg.row)) continue;
            if (nodeY (gg).getDistanceFrom (p) < 16.0f) { isMerge = false; return gg.group; }
            if (nodeM (gg).getDistanceFrom (p) < 16.0f) { isMerge = true; return gg.group; }
        }
        return -1;
    }

    // petites lettres A / B / C des voies : un clic agit comme le footswitch A <-> B
    Rectangle<float> laneLetterRect (const GGroup& gg, int l) const
    {
        const auto y0 = nodeY (gg);
        return { y0.x + 13.0f, y0.y + kLanePitch * (float) l - 22.0f, 15.0f, 14.0f };
    }
    bool laneLetterAt (Point<float> p, int& g, int& lane) const
    {
        buildGrid();
        for (auto& gg : gGroups)
        {
            if (gg.folded || ! rowVisible (gg.row)) continue;
            for (int l = 0; l < gg.lanes; ++l)
                if (laneLetterRect (gg, l).expanded (4.0f, 3.0f).contains (p)) { g = gg.group; lane = l; return true; }
        }
        return false;
    }

    // position d'insertion pendant un glisser (0 .. nombre de blocs) et repère à dessiner ; sous un bloc : split (ou voie de plus)
    int dropAt (Point<float> p) const
    {
        dropSplitSlot = -1; dropPathGroup = -1;
        buildGrid();
        const int n = eng.blocks().size();
        // sous un bloc hors groupe : split ; sous un bloc de la dernière voie d'un groupe (moins de 3 voies) : nouvelle voie
        for (auto& c : gCells)
        {
            if (c.kind != cBlock || c.slot == downSlot || ! rowVisible (c.row)) continue;
            const auto cc = cellCentre (c);
            if (std::abs (p.x - cc.x) > 52.0f || p.y < cc.y + 44.0f || p.y > cc.y + 104.0f) continue;
            if (c.group < 0) { dropSplitSlot = c.slot; dropMark = cc + Point<float> (0.0f, kLanePitch); return -1; }
            const auto* gg = gridGroup (c.group);
            if (gg != nullptr && c.lane == gg->lanes - 1 && gg->lanes < Routing::kMaxLanes)
            { dropPathGroup = c.group; dropMark = { kColX0 + kColPitch * (float) gg->col0, cc.y + kLanePitch }; return -1; }
        }
        int best = n; float bestD = 1.0e9f;
        auto consider = [&] (int pos, Point<float> at)
        {
            const float d = at.getDistanceFrom (p);
            if (d < bestD) { bestD = d; best = pos; dropMark = at; }
        };
        for (auto& c : gCells)
        {
            if (! rowVisible (c.row)) continue;
            const auto cc = cellCentre (c);
            if (c.kind == cLaneAdd) { consider (eng.laneEndBlock (c.group, c.lane), cc); continue; }
            if (c.kind == cFolded) { consider (c.slot, cc - Point<float> (kColPitch * 0.5f, 0.0f)); continue; }
            // premier bloc de la voie A d'un groupe : devant le groupe (au point Y) ou au début de la voie (plus près du bloc)
            if (c.group >= 0 && c.col == gridGroup (c.group)->col0)
            {
                const auto& G = eng.groups()[(size_t) c.group];
                consider (c.slot, cc - Point<float> (kColPitch * 0.3f, 0.0f));
                if (c.lane == 0) consider (eng.blocks().indexOf (G.split), cc - Point<float> (kColPitch * 0.62f, 0.0f));
            }
            else consider (c.slot, cc - Point<float> (kColPitch * 0.5f, 0.0f));
            // fin de voie : derrière le dernier bloc de la voie
            if (c.group >= 0)
            {
                const auto& ln = eng.groups()[(size_t) c.group].lanes[(size_t) c.lane];
                if (! ln.isEmpty() && eng.blocks().indexOf (ln.getLast()) == c.slot)
                    consider (eng.laneEndBlock (c.group, c.lane), cc + Point<float> (kColPitch * 0.3f, 0.0f));
            }
        }
        return best;
    }
    mutable int dropSplitSlot = -1, dropPathGroup = -1;
    mutable Point<float> dropMark;

    int recPreview = -1;         // rendus de test : -1 = état réel, 0 = non armée, 1 = armée, 2 = en enregistrement
    bool recBlinkOn = true;      // phase du clignotement (piste armée)
    int recShown() const
    {
        if (recPreview >= 0) return recPreview;
        if (Time::getMillisecondCounterHiRes() - recEditMs < 2500.0) return 2;     // réglage en cours : on montre l'effet
        return eng.recState();
    }
    // effet record : couleur (0 = rouge d'origine) et glow, communs à toutes les fenêtres (clic droit sur IN / OUT / REC)
    juce::uint32 recCol = OctoSettings::colourOf ("rec");
    int recGlowPc = OctoSettings::glowOf ("rec");
    double recEditMs = -1.0e9;
    int drawnRec = -1;
    Colour recColour() const { return recCol != 0 ? Colour (recCol) : Colour (0xffff3b3b); }
    float connXL() const { return kColX0 - kTile * 0.5f - (kColPitch - kTile) * 0.5f - 6.0f; }                                   // point IN
    float connXR() const { return kColX0 + kColPitch * (float) (kCols - 1) + kTile * 0.5f + (kColPitch - kTile) * 0.5f + 6.0f; }   // point OUT
    bool recMarkAt (Point<float> p) const
    {
        auto zone = [] (Point<float> d) { return Rectangle<float> (d.x - 24.0f, d.y - 26.0f, 48.0f, 54.0f); };
        return (inVisible() && zone (inDot()).contains (p)) || (outVisible() && zone (outDot()).contains (p));
    }
    void recColourMenu()
    {
        const auto d = inVisible() ? inDot() : outDot();
        const auto near = Rectangle<float> (d.x - 24.0f, d.y - 26.0f, 48.0f, 54.0f);
        auto touched = [this] { recEditMs = Time::getMillisecondCounterHiRes(); askRepaint (screenPx()); };
        openPicker ("Record colour", recColour(), S (near), true,
                    [this, touched] (Colour c) { recCol = c.getARGB(); OctoSettings::setColourOf ("rec", recCol); touched(); },
                    [this, touched] { recCol = 0; OctoSettings::setColourOf ("rec", 0); touched(); },
                    recGlowPc,
                    [this, touched] (int pc) { recGlowPc = pc; OctoSettings::setGlowOf ("rec", pc); touched(); });
        touched();
    }
    void drawConnections (Graphics& g)
    {
        buildGrid();
        Path path;
        // lignes de retour sur la verticale des points IN / OUT (maquette v1.18.71), retour 70 sous la rangée (sous sa dernière voie)
        const float xL = connXL(), xR = connXR();
        auto P = [this] (float x, float y) { return S (Point<float> (x, y)); };
        const int rows = totalRows();
        for (int r = 0; r < rows; ++r)
        {
            const float cy = rowCy (r);
            const bool lastRow = r == rows - 1;
            path.startNewSubPath (P (xL, cy));
            path.lineTo (P (xR, cy));
            if (! lastRow)
            {
                if (rowVisible (r) && ! rowVisible (r + 1) && r + 1 > firstRow)
                {
                    path.lineTo (P (xR, kMoreArrowY - 14.0f));      // rangée suivante cachée : la ligne descend jusqu'à la flèche bleue
                    continue;
                }
                const float ry = cy + kLanePitch * (float) (rowLanes (r) - 1) + kTile;
                path.lineTo (P (xR, ry)); path.lineTo (P (xL, ry)); path.lineTo (P (xL, rowCy (r + 1)));
            }
        }
        // voies des groupes dépliés : du point Y au point M
        for (auto& gg : gGroups)
        {
            if (gg.folded) continue;
            const auto y0 = nodeY (gg), m0 = nodeM (gg);
            for (int l = 1; l < gg.lanes; ++l)
            {
                const float y = y0.y + kLanePitch * (float) l;
                path.startNewSubPath (S (y0)); path.lineTo (P (y0.x, y)); path.lineTo (P (m0.x, y)); path.lineTo (S (m0));
            }
        }
        g.setColour (kLine.withAlpha (0.75f));
        g.strokePath (path, PathStrokeType (2.0f * s, PathStrokeType::mitered, PathStrokeType::rounded));

        // piste d'Octopus armée dans REAPER : IN, OUT, leurs points et REC en rouge (la ligne reste grise) ;
        // ils clignotent tant que la piste est seulement armée, fixes pendant l'enregistrement
        const int rs = recShown();
        drawnRec = rs;
        const bool recOn = rs == 1 || (rs == 2 && recBlinkOn);        // REC : fixe si armée, clignote pendant l'enregistrement
        const Colour rec = recColour();
        const float gk = (float) recGlowPc / 100.0f;                   // glow : 0-200 %, 100 = normal
        auto glowText = [&] (const String& txt, Rectangle<float> r)
        {
            if (gk > 0.0f)
                for (int l = 2; l >= 1; --l)
                    for (int a = 0; a < 8; ++a)
                    {
                        const float an = (float) a * MathConstants<float>::twoPi / 8.0f, d = 1.1f * (float) l;
                        g.setColour (rec.withAlpha (jmin (1.0f, 0.07f * gk / (float) l)));
                        g.drawText (txt, S (r.translated (d * std::cos (an), d * std::sin (an))), Justification::centred);
                    }
            g.setColour (rec);
            g.drawText (txt, S (r), Justification::centred);
        };
        // IN / OUT et leurs points restent gris ; seul REC (sous IN) passe en rouge
        auto mark = [&] (const String& txt, Point<float> dot, bool withRec)
        {
            g.setFont (font (tfBold, 15.0f));
            const Rectangle<float> tr (dot.x - 22.0f, dot.y - 24.0f, 44.0f, 18.0f);
            g.setColour (kDim);
            g.drawText (txt, S (tr), Justification::centred);
            g.fillEllipse (Rectangle<float> (S (7.0f), S (7.0f)).withCentre (S (dot)));
            if (withRec && rs > 0 && recOn) glowText ("REC", { dot.x - 22.0f, dot.y + 6.0f, 44.0f, 18.0f });   // même taille que IN / OUT
        };
        if (inVisible()) mark ("IN", inDot(), true);
        if (outVisible()) mark ("OUT", outDot(), false);
    }

    void drawBlock (Graphics& g, int slot, const FxInfo& f, Point<float> centre, float alpha, bool ghost)
    {
        const int kind = FxCat::classify (f.original);
        const auto col = FxCat::colour (kind);
        const auto t = S (Rectangle<float> (kTile, kTile).withCentre (centre));
        const bool on = f.enabled && ! f.offline;
        const bool hov = ! ghost && hoverSlot == slot;
        const float rad = S (11.0f);
        // famille utilitaire (blanc) : carreau blanc plein et icône sombre quand il est allumé, pour trancher net avec un bloc éteint
        const bool white = kind == FxCat::utility && on;

        // fond plein sous le carreau : la ligne de chaîne ne se voit jamais derrière un bloc estompé (voie coupée, bloc éteint)
        if (! ghost) { g.setColour (kScreenBlack); g.fillRoundedRectangle (t, rad); }
        // carreau
        g.setColour (kScrBg.withAlpha (alpha));
        g.fillRoundedRectangle (t, rad);
        if (on)
        {
            const float a0 = white ? 0.855f : 0.42f, a1 = white ? 0.665f : 0.16f;   // blanc : 2 x 5 % de moins
            g.setGradientFill (ColourGradient (col.withAlpha (a0 * alpha), t.getX(), t.getY(), col.withAlpha (a1 * alpha), t.getX(), t.getBottom(), false));
            g.fillRoundedRectangle (t, rad);
        }
        else
        {
            g.setColour (kScrPanel2.withAlpha (alpha));
            g.fillRoundedRectangle (t, rad);
        }
        g.setColour ((on ? col : kFaint.brighter (0.25f)).withAlpha (alpha * (hov ? 1.0f : 0.9f)));
        g.drawRoundedRectangle (t.reduced (S (0.8f)), rad, S (hov ? 2.6f : 1.8f));
        if (f.guid == paramGuid && overlay == Overlay::params)
        {
            g.setColour (kText.withAlpha (alpha));
            g.drawRoundedRectangle (t.expanded (S (4.0f)), rad + S (4.0f), S (1.6f));
        }

        // icône
        FxCat::drawIcon (g, kind, t.reduced (S (13.0f)), (white ? Colour (0xff1a1d23) : on ? col.brighter (0.35f) : kDim.withAlpha (0.55f)).withAlpha (alpha), S (2.6f), FxCat::iconOf (f.original));

        // témoin ON (coin haut droit)
        const auto led = Rectangle<float> (S (8.0f), S (8.0f)).withCentre ({ t.getRight() - S (10.0f), t.getY() + S (10.0f) });
        g.setColour (white ? Colour (0xff1a1d23).withAlpha (alpha) : on ? col.brighter (0.6f).withAlpha (alpha) : kFaint.withAlpha (alpha));
        g.fillEllipse (led);

        // badges : parallèle, fenêtre ouverte, hors ligne
        g.setFont (font (tfBold, 12.0f));
        if (f.parallel > 0)
        {
            g.setColour (kAmber.withAlpha (alpha));
            g.drawText ("||", Rectangle<float> (t.getX() + S (4.0f), t.getBottom() - S (17.0f), S (16.0f), S (14.0f)), Justification::centred);
        }
        if (f.open)
        {
            g.setColour (kText.withAlpha (0.8f * alpha));
            g.drawRect (Rectangle<float> (t.getX() + S (6.0f), t.getY() + S (6.0f), S (9.0f), S (7.0f)), S (1.2f));
        }
        if (f.offline)
        {
            g.setColour (Colour (0xffe0533f).withAlpha (alpha));
            g.drawLine ({ t.getX() + S (8.0f), t.getBottom() - S (8.0f), t.getRight() - S (8.0f), t.getY() + S (8.0f) }, S (2.0f));
        }

        // nom
        if (! ghost)
        {
            const auto nameR = S (Rectangle<float> (centre.x - kColPitch * 0.5f + 2.0f, centre.y + kTile * 0.5f + 4.0f, kColPitch - 4.0f, 24.0f));
            g.setFont (font (tfMedium, 17.5f));
            g.setColour ((on ? kText : kDim).withAlpha (alpha));
            g.drawText (FxCat::shortName (f.name), nameR, Justification::centredTop, true);
        }
    }

    void drawAddTile (Graphics& g, Point<float> centre, bool hov, const String& label, bool small)
    {
        const auto t = S (Rectangle<float> (kTile, kTile).withCentre (centre));
        g.setColour (kScrBg);
        g.fillRoundedRectangle (t, S (11.0f));
        Path dash, outline;
        outline.addRoundedRectangle (t.reduced (S (1.0f)), S (11.0f));
        const float dl[2] = { S (5.0f), S (4.0f) };
        PathStrokeType (S (1.6f)).createDashedStroke (dash, outline, dl, 2);
        g.setColour (hov ? kAccent : (small ? kAmber.withAlpha (0.65f) : kDim.withAlpha (0.7f)));
        g.fillPath (dash);
        const auto cc = t.getCentre();
        const float a = S (small ? 11.0f : 13.0f);
        g.drawLine (cc.x - a, cc.y, cc.x + a, cc.y, S (2.6f));
        g.drawLine (cc.x, cc.y - a, cc.x, cc.y + a, S (2.6f));
        g.setFont (font (tfMedium, small ? 14.0f : 16.0f));
        g.drawText (label, S (Rectangle<float> (centre.x - 70.0f, centre.y + kTile * 0.5f + 3.0f, 140.0f, 22.0f)), Justification::centredTop, true);
    }

    // point Y / M d'un groupe (ambre ; gris quand le groupe est coupé)
    void drawNode (Graphics& g, Point<float> c, const String& txt, bool on, bool hov)
    {
        const auto col = on ? kAmber : kDim;
        const auto r = S (Rectangle<float> (22.0f, 22.0f).withCentre (c));
        if (hov) { g.setColour (col.withAlpha (0.18f)); g.fillEllipse (r.expanded (S (5.0f))); }
        g.setColour (kScrBg); g.fillEllipse (r);
        g.setColour (col); g.drawEllipse (r, S (1.8f));
        g.setFont (font (tfBold, 14.0f));
        g.drawText (txt, r, Justification::centred);
    }

    void drawGrid (Graphics& g)
    {
        buildGrid();
        const auto& blocks = eng.blocks();
        const auto& fx = eng.chain();
        Graphics::ScopedSaveState ss (g);
        // quand il reste des rangées plus bas, la ligne de chaîne s'arrête au-dessus de la flèche bleue
        const float clipBottom = firstRow < maxFirstRow() ? kMoreArrowY - 14.0f : kFooterLine - 2.0f;
        g.reduceClipRegion (S (Rectangle<float> (0.0f, kGridTop - 8.0f, kSW, clipBottom - (kGridTop - 8.0f))).toNearestInt());

        drawConnections (g);

        // cases vides (repères discrets) : fin des rangées, puis rangées vides jusqu'en bas
        for (int r = firstRow; r < totalRows() + kRows; ++r)
        {
            if (r < totalRows() ? ! rowVisible (r) : ! rowFits (r)) break;
            for (int col = r < totalRows() ? gRows[(size_t) r].lastCol + 1 : 0; col < kCols; ++col)
            {
                g.setColour (kFaint.withAlpha (0.5f));
                g.fillEllipse (Rectangle<float> (S (4.0f), S (4.0f)).withCentre (S (Point<float> (kColX0 + kColPitch * (float) col, rowCy (r)))));
            }
        }

        const int dragFrom = dragging ? downSlot : -1;
        for (size_t k = 0; k < gCells.size(); ++k)
        {
            const auto& cell = gCells[k];
            if (! rowVisible (cell.row)) continue;
            const auto cc = cellCentre (cell);
            if (cell.kind == cBlock)
            {
                // voie coupée dans le mixeur (mute, ou A / B au footswitch) : bloc estompé
                const bool muted = cell.group >= 0 && eng.mixValue (cell.group, cell.lane, Routing::mixMute) > 0.5;
                drawBlock (g, cell.slot, fx.getReference (blocks[cell.slot]), cc, cell.slot == dragFrom ? 0.25f : (muted ? 0.38f : 1.0f), false);
            }
            else if (cell.kind == cAdd) drawAddTile (g, cc, hoverSlot == cell.slot, blocks.isEmpty() ? "Add an effect" : "Add", false);
            else if (cell.kind == cLaneAdd) drawAddTile (g, cc, hoverLaneAdd == (int) k, String ("Path ") + String::charToString ((juce_wchar) ('A' + cell.lane)), true);
            else if (cell.kind == cFolded)
            {
                // groupe replié : un bloc empilé (premier bloc de la voie B derrière, de la voie A devant)
                const auto& G = eng.groups()[(size_t) cell.group];
                auto front = fx.getReference (G.split);
                auto back = front;
                if (! G.lanes[0].isEmpty()) front = fx.getReference (G.lanes[0][0]);
                if (G.lanes.size() > 1 && ! G.lanes[1].isEmpty()) back = fx.getReference (G.lanes[1][0]);
                front.enabled = back.enabled = G.on;
                front.name = eng.blockName (G.split);
                front.guid = G.guid;
                front.parallel = 0; back.parallel = 0;
                drawBlock (g, -1, back, cc + Point<float> (7.0f, -7.0f), 0.85f, true);
                drawBlock (g, cell.slot, front, cc, cell.slot == dragFrom ? 0.25f : 1.0f, false);
                const auto badge = S (Rectangle<float> (cc.x - 41.0f, cc.y - kTile * 0.5f - 15.0f, 82.0f, 16.0f));
                g.setColour (G.on ? kAmber : kDim);
                g.fillRoundedRectangle (badge, S (4.0f));
                g.setColour (kScrBg);
                g.setFont (font (tfBold, 11.0f));
                g.drawText ("|| " + String ((int) G.lanes.size()) + " PATHS", badge, Justification::centred);
            }
        }

        // groupes dépliés : points Y / M et lettres des voies
        bool hovMerge = false;
        const int hovNode = nodeAt (hoverPos, hovMerge);
        for (auto& gg : gGroups)
        {
            if (gg.folded || ! rowVisible (gg.row)) continue;
            const bool on = eng.groups()[(size_t) gg.group].on;
            const auto y0 = nodeY (gg);
            for (int l = 0; l < gg.lanes; ++l)
            {
                const auto pr = S (laneLetterRect (gg, l));
                const bool live = on && eng.mixValue (gg.group, l, Routing::mixMute) < 0.5;
                const bool hovL = hoverNodeKey == 1000 + gg.group * 4 + l;
                g.setColour ((live ? kAmber : kDim).withAlpha (hovL ? 0.34f : 0.16f));
                g.fillRoundedRectangle (pr, S (3.0f));
                g.setColour (live ? kAmber : kDim);
                g.setFont (font (tfBold, 11.0f));
                g.drawText (String::charToString ((juce_wchar) ('A' + l)), pr, Justification::centred);
            }
            drawNode (g, y0, "Y", on, hovNode == gg.group && ! hovMerge);
            drawNode (g, nodeM (gg), "M", on, hovNode == gg.group && hovMerge);
        }

        // glisser : repère d'insertion (ou case de la nouvelle voie) + bloc qui suit la souris
        if (dragging && downSlot >= 0 && downSlot < blocks.size())
        {
            if (dropSplitSlot >= 0 || dropPathGroup >= 0)
            {
                const auto t = S (Rectangle<float> (kTile, kTile).withCentre (dropMark));
                g.setColour (kAmber.withAlpha (0.12f));
                g.fillRoundedRectangle (t, S (11.0f));
                Path dash, outline;
                outline.addRoundedRectangle (t, S (11.0f));
                const float dl[2] = { S (6.0f), S (4.0f) };
                PathStrokeType (S (2.0f)).createDashedStroke (dash, outline, dl, 2);
                g.setColour (kAmber);
                g.fillPath (dash);
                g.setFont (font (tfBold, 13.0f));
                g.drawText (dropSplitSlot >= 0 ? "SPLIT" : "NEW PATH", S (Rectangle<float> (dropMark.x - 60.0f, dropMark.y + kTile * 0.5f + 3.0f, 120.0f, 18.0f)), Justification::centredTop);
            }
            else if (dropPos >= 0)
            {
                g.setColour (kAccent);
                g.fillRoundedRectangle (S (Rectangle<float> (5.0f, kTile + 14.0f).withCentre (dropMark)), S (2.5f));
            }
            drawBlock (g, -1, fx.getReference (blocks[downSlot]), dragPos, 0.9f, true);
        }

        // défilement

        if (mixGroup >= 0) drawMixer (g);
    }

    // ------------------------------------------------------------ mini CHROMA + Semitones (en-tête)
    // Semitones : bouton (anneau = transposition, nom du preset) aligné à gauche de la bank ; clic = tambour vertical
    // des presets (molette, clic pour choisir). Mini CHROMA : à sa gauche, la note jouée (transposée par Semitones) dans un anneau vert / rouge
    // (juste / faux) ; clic = accordeur, clic droit = couleurs et halo.
    bool semiOpen = false;
    int semiTarget = 0, hoverDrum = -1, drawnSemi = -1;
    float semiPos = 0.0f;
    String drawnChroma;
    juce::uint32 semiCol = OctoSettings::colourOf ("semi"), chromaOkCol = OctoSettings::colourOf ("chromaOk"), chromaOffCol = OctoSettings::colourOf ("chromaOff");
    int semiGlowPc = OctoSettings::glowOf ("semi"), chromaGlowPc = OctoSettings::glowOf ("chroma");
    Colour semiColour() const      { return semiCol != 0 ? Colour (semiCol) : Colour (0xff2fa8ff); }
    Colour chromaOkColour() const  { return chromaOkCol != 0 ? Colour (chromaOkCol) : TC::green(); }
    Colour chromaOffColour() const { return chromaOffCol != 0 ? Colour (chromaOffCol) : Colour (0xffff4d4d); }

    static String semiValue (int v) { return v > 0 ? "+" + String (v) : String (v); }
    static String semiValue2 (int v) { return (v > 0 ? "+" : v < 0 ? "-" : "") + String (std::abs (v)).paddedLeft ('0', 2); }

    float unitsOf (const Font& f, const String& t) const { return GlyphArrangement::getStringWidth (f, t) / jmax (0.001f, S (1.0f)); }
    Font semiLabelFont() const { return font (tfBold, 11.0f).withExtraKerningFactor (0.18f); }
    Rectangle<float> semiRect() const
    {
        const auto& p = semi::get (proc.semitonesPreset());
        const float tw = jmax (unitsOf (font (tfBold, 16.0f), p.name), unitsOf (semiLabelFont(), "SEMITONES"));
        const float w = jlimit (80.0f, 160.0f, 26.0f + 8.0f + tw + 2.0f);
        return { kSep1X - kHG - w, kHdrY, w, 36.0f };
    }
    // mini CHROMA : anneau à gauche de la case, chevrons rouges à droite (trop bas : vers le bas, trop haut : vers le haut ;
    // deux chevrons à partir de 10 cents d'écart) pendant que la note sonne
    Rectangle<float> chromaRect() const { return { semiRect().getX() - kHG - 42.0f, kHdrY, 42.0f, 36.0f }; }
    float chromaLeft() const { return chromaRect().getX(); }

    // anneau lumineux : halo (opacité glowPc %) puis trait net
    void glowArc (Graphics& g, Point<float> c, float rad, float w, float from, float to, Colour col, int glowPc)
    {
        Path p; p.addCentredArc (c.x, c.y, rad, rad, 0.0f, from, to, true);
        const float k = (float) glowPc / 100.0f;
        if (k > 0.0f)
        {
            const float layers[3][2] = { { 4.2f, 0.07f }, { 2.6f, 0.13f }, { 1.7f, 0.22f } };
            for (auto& l : layers)
            {
                g.setColour (col.withMultipliedAlpha (jmin (1.0f, l[1] * k)));
                g.strokePath (p, PathStrokeType (w * l[0], PathStrokeType::curved, PathStrokeType::rounded));
            }
        }
        g.setColour (col);
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void drawHeaderBox (Graphics& g, Rectangle<float> b, bool hov)
    {
        const auto rr = S (b);
        g.setColour (hov ? kScrPanel2.brighter (0.15f) : kScrPanel2);
        g.fillRoundedRectangle (rr, S (7.0f));
        g.setColour (hov ? kDim : kFaint);
        g.drawRoundedRectangle (rr.reduced (S (0.6f)), S (7.0f), S (1.2f));
    }

    struct MiniNote { bool has = false; bool inTune = false, sounding = false; float alpha = 0.3f, cents = 0.0f; String letter, acc; };
    MiniNote miniNote() const
    {
        MiniNote n;
        const auto sn = proc.tunerSnapshot();
        n.has = sn.monoMidi >= 0 && (sn.monoActive || sn.monoAge < 2.0f);
        if (! n.has) return n;
        n.alpha = sn.monoActive ? 1.0f : jmap (jlimit (0.25f, 2.0f, sn.monoAge), 0.25f, 2.0f, 1.0f, 0.35f);
        n.inTune = sn.monoPrecise && std::abs (sn.monoCents) <= (float) tuna::TunerEngine::kMonoInTune;   // comme CHROMA
        n.sounding = sn.monoActive;                     // la flèche n'apparaît que pendant que la note sonne
        n.cents = sn.monoCents;
        // note qui sort d'Octopus : celle jouée, décalée par Semitones (le grand accordeur garde la note jouée, pour accorder)
        const String pc (tuna::pitchClassName (sn.monoMidi + semi::get (proc.semitonesPreset()).semis));
        n.letter = pc.substring (0, 1); n.acc = pc.substring (1);
        return n;
    }

    void drawMiniChroma (Graphics& g)
    {
        const auto b = chromaRect();
        const auto n = miniNote();
        const auto c = S (Point<float> (b.getX() + 12.0f, b.getCentreY()));
        if (hoverBtn == bChroma) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillEllipse (Rectangle<float> (S (34.0f), S (34.0f)).withCentre (c)); }
        const float rad = S (11.6f), w = S (2.1f);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        if (! n.has)
        {
            g.setColour (Colours::white.withAlpha (0.12f));
            g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), w);
        }
        else glowArc (g, c, rad, w, 0.0f, MathConstants<float>::twoPi,
                      (n.inTune ? chromaOkColour() : chromaOffColour()).withMultipliedAlpha (n.alpha), (int) (chromaGlowPc * n.alpha));
        drawMiniArrow (g, n, b);
        const auto big = font (tfBold, 16.7f), small = font (tfBold, 9.8f);
        g.setColour (n.has ? Colours::white.withAlpha (jmax (0.5f, n.alpha)) : kDim);
        if (! n.has) { g.setFont (big); g.drawText (CharPointer_UTF8 ("\xe2\x80\x93"), Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), Justification::centred); return; }
        const float wl = GlyphArrangement::getStringWidth (big, n.letter), wa = n.acc.isEmpty() ? 0.0f : GlyphArrangement::getStringWidth (small, n.acc) + S (0.4f);
        const float x = c.x - (wl + wa) * 0.5f;
        g.setFont (big);
        g.drawText (n.letter, Rectangle<float> (x, c.y - S (10.0f), wl + S (1.0f), S (20.6f)), Justification::centredLeft);
        if (n.acc.isNotEmpty())
        {
            g.setFont (small);
            g.drawText (n.acc, Rectangle<float> (x + wl + S (0.4f), c.y - S (9.5f), wa + S (1.0f), S (11.0f)), Justification::centredLeft);
        }
    }

    // chevrons à droite de l'anneau : sous la note affichée -> vers le bas, au-dessus -> vers le haut ; un chevron quand on est
    // un peu faux, deux à partir de 10 cents ; rien si la note est juste ou sans son
    void drawMiniArrow (Graphics& g, const MiniNote& n, Rectangle<float> b)
    {
        if (! n.has || ! n.sounding || n.inTune) return;
        const bool up = n.cents < 0.0f;            // trop bas : flèche vers le haut (il faut monter), trop haut : vers le bas
        const auto col = chromaOffColour();
        const float k = (float) chromaGlowPc / 100.0f;
        const float x = b.getX() + 35.0f, cy = b.getCentreY(), d = up ? -1.0f : 1.0f;
        auto chevron = [&] (float y)
        {
            const float wdt = 10.0f, hgt = 6.0f;
            Path p;
            p.startNewSubPath (S (Point<float> (x - wdt * 0.5f, y - d * hgt * 0.5f)));
            p.lineTo (S (Point<float> (x, y + d * hgt * 0.5f)));
            p.lineTo (S (Point<float> (x + wdt * 0.5f, y - d * hgt * 0.5f)));
            for (auto l : { 3.2f, 2.0f })
            {
                g.setColour (col.withAlpha (jmin (1.0f, 0.14f * k)));
                g.strokePath (p, PathStrokeType (S (2.0f * l), PathStrokeType::curved, PathStrokeType::rounded));
            }
            g.setColour (col);
            g.strokePath (p, PathStrokeType (S (2.0f), PathStrokeType::curved, PathStrokeType::rounded));
        };
        if (std::abs (n.cents) >= 10.0f) { chevron (cy - 3.6f * d); chevron (cy + 3.6f * d); }
        else chevron (cy);
    }

    void drawSemiButton (Graphics& g)
    {
        const auto b = semiRect();
        const int idx = proc.semitonesPreset();
        const auto& p = semi::get (idx);
        const bool hovS = hoverBtn == bSemi || semiOpen;
        const auto c = S (Point<float> (b.getX() + 13.0f, b.getCentreY()));
        if (hovS) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillEllipse (Rectangle<float> (S (32.0f), S (32.0f)).withCentre (c)); }
        const float rad = S (11.0f), w = S (2.26f);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), w);
        if (p.semis != 0)
        {
            const float a = MathConstants<float>::twoPi * (float) jmin (12, std::abs (p.semis)) / 12.0f;
            glowArc (g, c, rad, w, 0.0f, p.semis < 0 ? -a : a, semiColour(), semiGlowPc / 2);
        }
        g.setFont (font (tfBold, 14.1f));
        g.setColour (Colours::white);
        g.drawText (semiValue (p.semis), Rectangle<float> (rad * 2.2f, rad * 2.0f).withCentre (c), Justification::centred);
        const float tx = b.getX() + 26.0f + 8.0f;
        g.setFont (semiLabelFont());
        g.setColour (Colour (0xff9aa3b2));
        g.drawText ("SEMITONES", S (Rectangle<float> (tx, b.getY() + 4.5f, b.getRight() - tx - 2.0f, 12.0f)), Justification::centredLeft, false);
        g.setFont (font (tfBold, 16.0f));
        g.setColour (Colour (0xfff2f5f9));
        g.drawText (p.name, S (Rectangle<float> (tx, b.getY() + 15.0f, b.getRight() - tx - 2.0f, 18.0f)), Justification::centredLeft, true);
    }

    // ---- tambour : presets sur un cylindre vertical, le preset choisi dans la bande du milieu
    // tous les tambours (v1.18.3) : même cylindre que Semitones, bande de sélection toujours à kDrumGap sous le bas de son bouton
    static constexpr float kDrumGap = 66.6f, kDrumBandH = 29.7f;
    static constexpr float kDrumMidY = (kHdrY + 36.0f + kDrumGap + kDrumBandH * 0.5f), kDrumR = 96.0f, kDrumStep = 16.0f;    // v1.18.2 : tout près du bouton (le haut du cylindre passe sous l'en-tête)
    // tambour aligné sur le bouton SEMITONES : la bande de sélection commence au bord gauche du bouton
    // colonne valeurs + noms centrée sous le bouton SEMITONES ; la bande de sélection l'entoure (12 de marge)
    float drumNamesW() const
    {
        float w = 0.0f;
        for (int i = 0; i < semi::kCount; ++i) w = jmax (w, unitsOf (font (tfBold, 18.3f), semi::get (i).name));
        return w;
    }
    // centré sous le bouton (règle des tambours : toujours centrés sur leur bouton)
    static constexpr float kDrumNumW = 33.0f, kDrumNameDX = 37.5f;
    float drumCx() const { return semiRect().getCentreX(); }
    float drumContentW() const { return kDrumNameDX + drumNamesW(); }
    float drumX() const { return drumCx() - drumContentW() * 0.5f; }      // début de la colonne des valeurs
    Rectangle<float> drumBand() const { return { drumX() - 12.0f, kDrumMidY - kDrumBandH * 0.5f, drumContentW() + 24.0f, kDrumBandH }; }
    void openSemiDrum()
    {
        semiTarget = proc.semitonesPreset(); semiPos = (float) semiTarget; hoverDrum = -1;
        semiOpen = true;
        askRepaint (screenPx());
    }
    void closeSemiDrum()
    {
        if (! semiOpen) return;
        semiOpen = false; hoverDrum = -1;
        askRepaint (screenPx());
    }
    void chooseSemi (int i)
    {
        proc.setSemitonesPreset (i);
        semiTarget = i;
        const int cur = eng.current();
        if (cur >= 0 && eng.presetSemi (cur) >= 0) eng.setPresetSemi (cur, i);      // preset lié : il garde la nouvelle transposition
        askRepaint (screenPx());
    }
    float drumY (float d) const { return kDrumMidY + kDrumR * std::sin (jlimit (-5.5f, 5.5f, d) * kDrumStep * MathConstants<float>::pi / 180.0f); }
    int drumItemAt (Point<float> p) const
    {
        if (p.x < drumBand().getX() || p.x > drumBand().getRight()) return -1;
        int best = -1; float bd = 1.0e9f;
        for (int i = 0; i < semi::kCount; ++i)
        {
            const float d = (float) i - semiPos;
            if (std::abs (d) > 4.3f) continue;
            const float h = 13.5f * std::cos (d * kDrumStep * MathConstants<float>::pi / 180.0f);
            const float dy = std::abs (p.y - drumY (d));
            if (dy <= h && dy < bd) { bd = dy; best = i; }
        }
        return best;
    }

    void drawSemiDrum (Graphics& g)
    {
        const auto col = semiColour();
        // voile sur l'écran (sous l'en-tête) + zone sombre derrière le tambour
        g.setColour (Colour (0xff05070b).withAlpha (0.985f));                 // fond presque opaque + halo (lisibilité)
        g.fillRect (S (kOverlay));
        drumHalo (g, drumBand(), kDrumR, col);
        // bande du milieu
        const auto band = S (drumBand());
        const float namesW = drumNamesW();
        const float k = (float) semiGlowPc / 100.0f;
        for (int l = 3; l >= 1; --l)
        {
            g.setColour (col.withAlpha (jmin (1.0f, 0.05f * k)));
            g.fillRoundedRectangle (band.expanded (S (3.0f * (float) l)), S (6.0f + 3.0f * (float) l));
        }
        g.setColour (col.withAlpha (0.10f));
        g.fillRoundedRectangle (band, S (6.0f));
        g.setColour (col.withAlpha (0.55f));
        g.drawRoundedRectangle (band, S (6.0f), S (0.9f));
        // presets (coupés sous l'en-tête)
        Graphics::ScopedSaveState clipTop (g);
        g.reduceClipRegion (S (Rectangle<float> (0.0f, kHeaderH + 4.0f, kSW, kSH)).toNearestInt());
        for (int i = 0; i < semi::kCount; ++i)
        {
            const float d = (float) i - semiPos;
            if (std::abs (d) > 4.6f) continue;
            const float ad = std::abs (d);
            const float a = d * kDrumStep * MathConstants<float>::pi / 180.0f;
            float op = jmax (0.06f, 1.0f - ad * 0.24f);
            if (i == hoverDrum && ad > 0.5f) op = jmin (1.0f, op + 0.25f);
            const float sc = 1.0f - ad * 0.08f, sy = std::cos (a);
            const auto& p = semi::get (i);
            const bool sel = ad < 0.5f;
            const auto cy = S (Point<float> (drumCx(), drumY (d)));
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::scale (sc, sc * sy, cy.x, cy.y));
            const float rowH = S (27.3f);
            const float x0 = S (Point<float> (drumX(), 0.0f)).x;      // S (Point) : avec l'origine de l'écran
            g.setFont (font (tfBold, 15.8f));
            g.setColour (col.withAlpha (op * 0.35f));
            g.drawText (semiValue2 (p.semis), Rectangle<float> (x0, cy.y - rowH * 0.5f, S (kDrumNumW), rowH).expanded (S (0.6f), 0.0f), Justification::centred);
            g.setColour (col.withAlpha (op));
            g.drawText (semiValue2 (p.semis), Rectangle<float> (x0, cy.y - rowH * 0.5f, S (kDrumNumW), rowH), Justification::centred);
            g.setFont (sel ? font (tfBold, 18.3f) : font (tfMedium, 16.7f));
            g.setColour ((sel ? Colours::white : kText).withAlpha (op));
            g.drawText (p.name, Rectangle<float> (x0 + S (kDrumNameDX), cy.y - rowH * 0.5f, S (namesW + 4.0f), rowH), Justification::centredLeft, true);
        }
        // aide : souris + texte
        const auto hintF = font (tfBold, 11.6f).withExtraKerningFactor (0.14f);
        const String hintT = CharPointer_UTF8 ("SCROLL  \xc2\xb7  CLICK TO CHOOSE");
        const float hx = drumCx() - (13.0f + 6.0f + unitsOf (hintF, hintT)) * 0.5f;      // aide centrée sous le bouton
        const float hy = kDrumMidY + kDrumR + 33.0f;
        const auto mr = S (Rectangle<float> (hx, hy, 13.0f, 19.0f));
        g.setColour (Colour (0xff9aa4b4));
        g.drawRoundedRectangle (mr, mr.getWidth() * 0.5f, S (1.2f));
        g.setColour (col);
        g.fillRoundedRectangle (S (Rectangle<float> (hx + 5.3f, hy + 3.6f, 2.4f, 5.4f)), S (1.2f));
        g.setFont (hintF);
        g.setColour (Colour (0xff9aa4b4));
        g.drawText (hintT, S (Rectangle<float> (hx + 19.0f, hy, 220.0f, 19.0f)), Justification::centredLeft);
    }

    // clic droit sur Semitones : lien avec le preset chargé, couleur et halo
    void semiMenu()
    {
        const int cur = eng.current();
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofFx (FxCat::pitch, FxCat::iconFamily), "Semitones", semi::get (proc.semitonesPreset()).name);
        const bool can = cur >= 0 && eng.preset (cur).saved;
        const bool linked = can && eng.presetSemi (cur) >= 0;
        m.addItem (1, can ? "Save with preset " + OctopusEngine::label (cur) + " (" + eng.displayName (cur) + ")" : String ("Save with a preset (load or save a preset first)"),
                   can, linked);
        m.addSeparator();
        m.addItem (2, "Color & Glow");
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, cur, linked] (int r)
        {
            if (sp == nullptr) return;
            if (r == 1)
            {
                sp->eng.setPresetSemi (cur, linked ? -1 : sp->proc.semitonesPreset());
                sp->eng.say (linked ? "Semitones : global again (not saved with " + OctopusEngine::label (cur) + ")"
                                    : "Semitones saved with " + OctopusEngine::label (cur) + " : recalled when it loads");
            }
            else if (r == 2)
                sp->openPicker ("Semitones colour", sp->semiColour(), sp->S (sp->semiRect()), true,
                                [sp] (Colour c) { if (sp != nullptr) { sp->semiCol = c.getARGB(); OctoSettings::setColourOf ("semi", sp->semiCol); } },
                                [sp] { if (sp != nullptr) { sp->semiCol = 0; OctoSettings::setColourOf ("semi", 0); } },
                                sp->semiGlowPc,
                                [sp] (int pc) { if (sp != nullptr) { sp->semiGlowPc = pc; OctoSettings::setGlowOf ("semi", pc); sp->askRepaint (sp->screenPx()); } });
        });
    }

    // clic droit sur le mini CHROMA : couleur de la note juste, de la note fausse, halo commun
    void chromaMenu()
    {
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofFx (FxCat::tuner, FxCat::iconFamily), "Mini CHROMA", "Colours and glow");
        m.addItem (1, "Color & Glow : in tune");
        m.addItem (2, "Color & Glow : out of tune");
        m.addSeparator();
        m.addItem (3, "Open the tuner");
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            if (r == 3) { sp->eng.toggleTuner(); return; }
            const bool ok = r == 1;
            const String key = ok ? "chromaOk" : "chromaOff";
            auto glowFn = [sp] (int pc) { if (sp != nullptr) { sp->chromaGlowPc = pc; OctoSettings::setGlowOf ("chroma", pc); sp->askRepaint (sp->screenPx()); } };
            sp->openPicker (ok ? "CHROMA : in tune colour" : "CHROMA : out of tune colour", ok ? sp->chromaOkColour() : sp->chromaOffColour(),
                            sp->S (sp->chromaRect()), true,
                            [sp, ok, key] (Colour c) { if (sp != nullptr) { (ok ? sp->chromaOkCol : sp->chromaOffCol) = c.getARGB(); OctoSettings::setColourOf (key, c.getARGB()); } },
                            [sp, ok, key] { if (sp != nullptr) { (ok ? sp->chromaOkCol : sp->chromaOffCol) = 0; OctoSettings::setColourOf (key, 0); } },
                            sp->chromaGlowPc, glowFn);
        });
    }

    // timer : tambour qui glisse vers le preset visé, mini CHROMA redessiné quand la note change
    void tickHeaderWidgets()
    {
        if (semiOpen && (tunerShown() || ! eng.isReady())) closeSemiDrum();
        if (modeOpen && ! tunerShown()) closeModeDrum();
        if (insertOpen && overlay != Overlay::browser) closeInsertDrum();
        {   // recherche : prise / perte du clavier (REAPER peut la reprendre) -> cadre, liste et message redessinés
            const bool foc = search.isVisible() && search.hasKeyboardFocus (true);
            if (foc != searchHadFocus) { searchHadFocus = foc; askRepaint (screenPx()); }
        }
        if (insertOpen && std::abs (insertDrumPos - (float) insertTarget) > 0.001f)
        {
            insertDrumPos += ((float) insertTarget - insertDrumPos) * 0.35f;
            if (std::abs (insertDrumPos - (float) insertTarget) < 0.01f) insertDrumPos = (float) insertTarget;
            askRepaint (screenPx());
        }
        if (std::abs (listPos - (float) listScroll) > 0.001f)          // liste du navigateur : glisse vers la ligne visée
        {
            listPos += ((float) listScroll - listPos) * 0.3f;
            if (std::abs (listPos - (float) listScroll) < 0.01f) listPos = (float) listScroll;
            askRepaint (screenPx());
        }
        if (modeOpen && std::abs (modePos - (float) modeTarget) > 0.001f)
        {
            modePos += ((float) modeTarget - modePos) * 0.35f;
            if (std::abs (modePos - (float) modeTarget) < 0.01f) modePos = (float) modeTarget;
            askRepaint (screenPx());
        }
        if (semiOpen && std::abs (semiPos - (float) semiTarget) > 0.001f)
        {
            semiPos += ((float) semiTarget - semiPos) * 0.35f;
            if (std::abs (semiPos - (float) semiTarget) < 0.01f) semiPos = (float) semiTarget;
            askRepaint (screenPx());
        }
        if (proc.semitonesPreset() != drawnSemi) { drawnSemi = proc.semitonesPreset(); askRepaint (screenPx()); }
        if (! tunerShown())
        {
            const auto n = miniNote();
            const String sig = n.has ? n.letter + n.acc + (n.inTune ? "1" : "0") + String (roundToInt (n.alpha * 20.0f))
                                         + (n.sounding ? (n.cents > 0.0f ? "u" : "d") + String (roundToInt (n.cents)) : String()) : String ("-");
            if (sig != drawnChroma) { drawnChroma = sig; askRepaint (S (chromaRect().withLeft (chromaLeft())).expanded (S (8.0f)).toNearestInt()); }
        }
    }

    // ------------------------------------------------------------ boutons
    // en-tête : marge de 14 avec les bords de l'écran (badge du preset à gauche, BANKS à droite) et 14 entre tous les éléments
    static constexpr float kHdrGap = 14.0f;
    // en-tête épuré (v1.13) : sans cadres ; à droite, séparateurs fins entre BANK, PRESET / STOMP et SAVE / BANKS
    static constexpr float kHG = 16.0f;                                               // écart entre les éléments
    static constexpr float kBanksX = kSW - kHdrGap - 82.0f, kSaveX = kBanksX - kHG - 70.0f, kSep3X = kSaveX - kHG,
                           kStompX = kSep3X - kHG - 64.0f, kPresetX = kStompX - kHG - 72.0f, kSep2X = kPresetX - kHG,
                           kBankNextX = kSep2X - kHG - 24.0f, kBankTextX = kBankNextX - 82.0f, kBankPrevX = kBankTextX - 24.0f,
                           kSep1X = kBankPrevX - kHG;
    static constexpr float kBadge = 41.0f, kNameX = kHdrGap + kBadge + 16.0f;
    std::vector<Btn> headerButtons() const
    {
        std::vector<Btn> v;
        // de droite à gauche, même écart (kHdrGap = marge du bord de l'écran) entre tous les éléments de l'en-tête
        v.push_back ({ { kBankPrevX, kHdrY, 24.0f, 36.0f }, bBankPrev, "<" });
        v.push_back ({ { kBankNextX, kHdrY, 24.0f, 36.0f }, bBankNext, ">" });
        v.push_back ({ { kPresetX, kHdrY, 72.0f, 36.0f }, bModePreset, "PRESET", eng.mode() == OctopusEngine::presetMode });
        v.push_back ({ { kStompX, kHdrY, 64.0f, 36.0f }, bModeStomp, "STOMP", eng.mode() == OctopusEngine::stompMode });
        v.push_back ({ { kSaveX, kHdrY, 70.0f, 36.0f }, bSave, "SAVE", false, eng.isReady() });
        v.push_back ({ { kBanksX, kHdrY, 82.0f, 36.0f }, bBanks, "BANKS", overlay == Overlay::banks });
        return v;
    }

    // grille des banks : même hauteur totale que les 6 lignes de presets (134 -> 458) et même écart (6) entre les cases
    Rectangle<float> bankRect (int b) const { return { 16.0f + 81.5f * (float) (b % 4), 134.0f + 82.5f * (float) (b / 4), 75.5f, 76.5f }; }
    static Rectangle<float> colourBtnRect (int f) { return { 356.0f + 100.0f * (float) f, 480.0f, 92.0f, 44.0f }; }   // A-F, TUNER
    Rectangle<float> cardRect (int slot) const { return { 356.0f, 134.0f + 55.0f * (float) slot, 692.0f, 49.0f }; }   // 6 presets A-F

    std::vector<Btn> overlayButtons() const
    {
        std::vector<Btn> v;
        if (overlay == Overlay::none) return v;
        v.push_back ({ overlay == Overlay::browser ? Rectangle<float> (kCloseX, kRowY, 36.0f, kRowH) : Rectangle<float> (1008.0f, 84.0f, 40.0f, 36.0f),
                       bClose, CharPointer_UTF8 ("\xe2\x9c\x95") });
        if (overlay == Overlay::browser)
        {
            // onglets des filtres (texte + nombre, souligné quand actif)
            float x = 16.0f;
            for (int c = 0; c < kNumChips; ++c)
            {
                const float w = chipWidth (c);
                v.push_back ({ { x - 4.0f, kTabsY, w + 8.0f, kTabsH }, bChip + c, kChips[c], chip == c });
                x += w + kTabGap;
            }
            // ligne du titre : emplacement (menu), tri, sélection, ADD, rescan, fermer
            v.push_back ({ insertWhereRect(), bInsertWhere, insertWhereText() });
            v.push_back ({ { kSortX + 3.0f, kRowY + 3.0f, 60.0f, kRowH - 6.0f }, bSortAZ, "A - Z", ! browserByType });     // dans l'interrupteur
            v.push_back ({ { kSortX + 63.0f, kRowY + 3.0f, 60.0f, kRowH - 6.0f }, bSortType, "TYPE", browserByType });
            if (picked.size() > 0) v.push_back ({ selPillRect(), bClearPicked, String (picked.size()) + " selected", false, true });
            v.push_back ({ { kAddX, kRowY, kAddW, kRowH }, bAddPicked, picked.isEmpty() ? String ("ADD") : "ADD " + String (picked.size()), picked.size() > 0, picked.size() > 0 });
            v.push_back ({ { kReloadX, kRowY, 36.0f, kRowH }, bReload, {} });
        }
        else if (overlay == Overlay::params)
        {
            const auto* f = paramFx();
            const bool on = f != nullptr && f->enabled;
            const int pages = jmax (1, (paramCount + 7) / 8);
            v.push_back ({ { 400.0f, 84.0f, 110.0f, 36.0f }, bPresets, "PRESETS", presetMode });
            v.push_back ({ { 516.0f, 84.0f, 104.0f, 36.0f }, bParamWindow, "WINDOW", f != nullptr && f->open });
            v.push_back ({ { 626.0f, 84.0f, 76.0f, 36.0f }, bParamMidi, "MIDI", midiView && ! presetMode });
            if (presetMode)
            {
                // flèches : preset précédent / suivant (comme les flèches du menu de presets de REAPER)
                v.push_back ({ { 760.0f, 84.0f, 34.0f, 36.0f }, bPagePrev, CharPointer_UTF8 ("\xe2\x97\x80"), false, plist.count > 1 });
                v.push_back ({ { 872.0f, 84.0f, 34.0f, 36.0f }, bPageNext, CharPointer_UTF8 ("\xe2\x96\xb6"), false, plist.count > 1 });
                v.push_back ({ { 652.0f, 132.0f, 116.0f, 36.0f }, bPresetSave, presetEdIndex >= -1 ? String ("OK") : String ("SAVE USER"), presetEdIndex >= -1 });
                v.push_back ({ { 776.0f, 132.0f, 126.0f, 36.0f }, bPresetRescan, "RELOAD LIST" });
            }
            else
            {
                v.push_back ({ { 760.0f, 84.0f, 34.0f, 36.0f }, bPagePrev, CharPointer_UTF8 ("\xe2\x97\x80"), false, paramPage > 0 });
                v.push_back ({ { 872.0f, 84.0f, 34.0f, 36.0f }, bPageNext, CharPointer_UTF8 ("\xe2\x96\xb6"), false, paramPage < pages - 1 });
            }
            // BYPASS à côté de la croix : allumé quand l'effet est contourné
            v.push_back ({ { 914.0f, 84.0f, 88.0f, 36.0f }, bParamOnOff, "BYPASS", ! on });
        }
        else if (overlay == Overlay::banks)
        {
            for (int b = 0; b < OctopusEngine::kBanks; ++b)
                v.push_back ({ bankRect (b), bBank + b, String (b + 1), eng.bank() == b });
            for (int sl = 0; sl < OctopusEngine::kSlots; ++sl)
            {
                const auto c = cardRect (sl);
                const int idx = eng.bank() * OctopusEngine::kSlots + sl;
                const bool saved = eng.preset (idx).saved;
                float x = c.getRight() - 8.0f;
                for (int a = numCardActions; --a >= 0;)
                {
                    const float w = a == cRename ? 84.0f : 64.0f;
                    x -= w;
                    bool en = true;
                    if (a == cLoad || a == cRename || a == cCopy || a == cClear) en = saved;
                    if (a == cSave) en = eng.isReady();
                    if (a == cPaste) en = eng.canPaste();
                    v.push_back ({ { x, c.getY() + 7.5f, w - 6.0f, 34.0f }, bCard + sl * 10 + a, kCardText[a], false, en });
                }
            }
            for (int f = 0; f <= OctopusEngine::kFs; ++f)                  // A-F + symbole de l'accordeur
                v.push_back ({ colourBtnRect (f), bFsColour + f, f < OctopusEngine::kFs ? fsLetter (f) : String ("TUNER") });
            v.push_back ({ { 16.0f, 560.0f, 156.0f, 44.0f }, bExport, "EXPORT..." });
            v.push_back ({ { 180.0f, 560.0f, 156.0f, 44.0f }, bImport, "IMPORT..." });
            v.push_back ({ { 356.0f, 560.0f, 400.0f, 44.0f }, bPc, String ("PROGRAM CHANGE = PRESETS : ") + (eng.programChange ? "ON" : "OFF"), eng.programChange });
        }
        return v;
    }

    int buttonAt (Point<float> p) const
    {
        if (tunerShown())
        {
            for (auto& b : tunerButtons()) if (b.r.contains (p)) return b.id;
            return bNone;
        }
        if (chromaRect().contains (p)) return bChroma;
        if (semiRect().contains (p)) return bSemi;
        if (overlay != Overlay::none)
            for (auto& b : overlayButtons()) if (b.enabled && b.r.contains (p)) return b.id;
        for (auto& b : headerButtons()) if (b.enabled && b.r.contains (p)) return b.id;
        return bNone;
    }

    void drawButton (Graphics& g, const Btn& b, float textSize = 15.0f)
    {
        const auto rr = S (b.r);
        const bool hov = hoverBtn == b.id && b.enabled;
        const bool stomp = b.id == bModeStomp;                  // STOMP sélectionné : nuance d'orange
        const auto acc = stomp ? kStompOrange : kAccent;
        g.setColour (b.active ? acc.withAlpha (0.22f) : (hov ? kScrPanel2.brighter (0.15f) : kScrPanel2));   // survol : comme les autres boutons
        g.fillRoundedRectangle (rr, S (7.0f));
        g.setColour (b.active ? acc : (hov ? kDim : kFaint));
        g.drawRoundedRectangle (rr.reduced (S (0.6f)), S (7.0f), S (1.2f));
        g.setFont (font (tfBold, textSize));
        g.setColour (! b.enabled ? kFaint : (b.active ? kText : kText.withAlpha (0.85f)));
        g.drawText (b.text, rr, Justification::centred, true);
    }

    // ------------------------------------------------------------ écran complet
    void drawScreen (Graphics& g)
    {
        const auto glass = r (L::glass);
        Graphics::ScopedSaveState ss (g);
        Path clip; clip.addRoundedRectangle (glass, L::glassRadius * s);
        g.reduceClipRegion (clip);
        g.setColour (kScreenBlack);                                                 // fond noir uni (v1.17.0 : plus de dégradé)
        g.fillRect (glass);

        if (tunerShown()) drawTuner (g);
        else
        {
            drawHeader (g);
            if (! eng.isReady())
                drawNotReady (g);
            else
            {
                drawGrid (g);
                drawMoreArrows (g);                           // hors du découpage de la grille
                drawFooter (g);
            }
            if (overlay != Overlay::none) drawOverlay (g);
            if (semiOpen) drawSemiDrum (g);
        }

        // bord de la vitre (plus de reflet en biais : fond uni)
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (glass.reduced (0.5f), L::glassRadius * s, 1.0f);
    }

    void drawHeader (Graphics& g)
    {
        g.setColour (kScrPanel);
        g.fillRect (S (Rectangle<float> (0.0f, 0.0f, kSW, kHeaderH)));
        g.setColour (kFaint.withAlpha (0.6f));
        g.fillRect (S (Rectangle<float> (0.0f, kHeaderH - 1.0f, kSW, 1.0f)));

        // preset chargé : badge carré, de la couleur du footswitch de ce preset (BANKS > FOOTSWITCH COLOURS)
        const int cur = eng.current();
        const auto badge = S (badgeRect());
        const auto badgeCol = cur >= 0 ? Colour (eng.presetColour (cur)) : kScrPanel2;
        g.setColour (badgeCol);
        g.fillRoundedRectangle (badge, S (10.0f));
        g.setFont (font (tfBold, 25.0f));
        g.setColour (cur < 0 ? kDim : badgeCol.getPerceivedBrightness() > 0.55f ? kScrBg : Colours::white);
        g.drawText (cur >= 0 ? OctopusEngine::label (cur) : String ("--"), badge.translated (0.0f, S (1.0f)), Justification::centred);

        const String name = cur >= 0 ? eng.displayName (cur) : String (eng.isReady() ? "Live chain" : "OCTOPUS");
        const auto nameR = S (headerNameRect());
        const auto nameFont = font (tfBold, 24.0f);
        g.setFont (nameFont);
        g.setColour (kText);
        if (! headerRenaming) g.drawText (name, nameR, Justification::centredLeft, true);
        if (eng.modified() && ! headerRenaming)
        {
            const float tw = jmin (nameR.getWidth() - S (60.0f), GlyphArrangement::getStringWidth (nameFont, name));
            g.setFont (font (tfBold, 13.0f));
            g.setColour (kAmber);
            g.drawText ("EDITED", Rectangle<float> (nameR.getX() + tw + S (10.0f), nameR.getY() + S (9.0f), S (60.0f), S (18.0f)), Justification::centredLeft);
        }

        const auto msg = eng.message();
        String sub = eng.trackName().isNotEmpty() ? eng.trackName() : eng.statusText();
        if (msg.isNotEmpty()) sub = msg;
        g.setFont (font (tfRegular, 15.0f));
        g.setColour (msg.isNotEmpty() ? kAccent : kDim);
        g.drawText (sub, S (Rectangle<float> (kNameX, kHeaderH * 0.5f + 1.0f, headerNameRect().getWidth(), 20.0f)), Justification::centredLeft, true);

        // séparateurs fins
        for (float x : { kSep1X, kSep2X, kSep3X })
        {
            g.setColour (Colour (0xff232933));
            g.fillRect (S (Rectangle<float> (x, (kHeaderH - 34.0f) * 0.5f, 1.0f, 34.0f)));
        }

        // bank affichée : ‹ BANK 01 ›
        g.setFont (font (tfBold, 15.4f).withExtraKerningFactor (0.14f));            // même taille que PRESET / STOMP / SAVE / BANKS
        g.setColour (kText);
        g.drawText ("BANK " + String (eng.bank() + 1).paddedLeft ('0', 2), S (Rectangle<float> (kBankTextX, kHdrY, 82.0f, 36.0f)), Justification::centred);
        for (auto& b : headerButtons()) drawHeaderItem (g, b);
        drawMiniChroma (g);
        drawSemiButton (g);
    }

    // éléments de l'en-tête, sans cadre : flèches, onglets PRESET / STOMP (soulignés), SAVE / BANKS (icône + texte)
    void drawHeaderItem (Graphics& g, const Btn& b)
    {
        const bool hov = hoverBtn == b.id && b.enabled;
        const auto r0 = b.r;
        if (b.id == bBankPrev || b.id == bBankNext)
        {
            const auto cc = r0.getCentre();
            const float dx = b.id == bBankPrev ? 1.0f : -1.0f;
            Path p;
            p.startNewSubPath (S (Point<float> (cc.x + 2.0f * dx, cc.y - 4.0f)));     // hauteur des capitales du texte
            p.lineTo (S (Point<float> (cc.x - 2.0f * dx, cc.y)));
            p.lineTo (S (Point<float> (cc.x + 2.0f * dx, cc.y + 4.0f)));
            g.setColour (hov ? kText : kDim);
            g.strokePath (p, PathStrokeType (S (1.7f), PathStrokeType::curved, PathStrokeType::rounded));
            return;
        }
        if (b.id == bModePreset || b.id == bModeStomp)
        {
            const auto acc = b.id == bModeStomp ? kStompOrange : kAccent;
            const auto f = font (tfBold, 15.4f).withExtraKerningFactor (0.14f);
            g.setFont (f);                                                          // actif : écrit en bleu (PRESET) ou en orange (STOMP)
            g.setColour (b.active ? acc : (hov ? kText.withAlpha (0.85f) : kDim));
            g.drawText (b.text, S (r0), Justification::centred, false);
            return;
        }
        // SAVE / BANKS : petite icône + texte
        const auto col = ! b.enabled ? kFaint : (b.active ? kAccent : (hov ? kText : kDim));
        const auto f = font (tfBold, 15.4f).withExtraKerningFactor (0.1f);
        const float tw = unitsOf (f, b.text), iw = 15.0f, gap = 7.0f;
        const float x0 = r0.getCentreX() - (iw + gap + tw) * 0.5f, cy = r0.getCentreY();
        g.setColour (col);
        const auto ib = Rectangle<float> (x0, cy - iw * 0.5f, iw, iw);
        if (b.id == bSave)
        {
            Path p;
            p.startNewSubPath (S (Point<float> (ib.getX() + 2.0f, ib.getY() + 2.0f)));
            p.lineTo (S (Point<float> (ib.getX() + 10.5f, ib.getY() + 2.0f)));
            p.lineTo (S (Point<float> (ib.getRight() - 2.0f, ib.getY() + 4.5f)));
            p.lineTo (S (Point<float> (ib.getRight() - 2.0f, ib.getBottom() - 2.0f)));
            p.lineTo (S (Point<float> (ib.getX() + 2.0f, ib.getBottom() - 2.0f)));
            p.closeSubPath();
            p.addRectangle (S (Rectangle<float> (ib.getX() + 4.5f, ib.getY() + 2.0f, 5.0f, 3.5f)));
            p.addRectangle (S (Rectangle<float> (ib.getX() + 4.5f, ib.getY() + 8.5f, 6.0f, 4.5f)));
            g.strokePath (p, PathStrokeType (S (1.4f), PathStrokeType::mitered, PathStrokeType::rounded));
        }
        else
            for (int k = 0; k < 4; ++k)
                g.drawRoundedRectangle (S (Rectangle<float> (ib.getX() + 1.5f + (float) (k % 2) * 7.0f, ib.getY() + 1.5f + (float) (k / 2) * 7.0f, 5.0f, 5.0f)), S (1.2f), S (1.4f));
        g.setFont (f);
        g.drawText (b.text, S (Rectangle<float> (x0 + iw + gap, r0.getY(), tw + 6.0f, r0.getHeight())), Justification::centredLeft, false);
    }

    void drawNotReady (Graphics& g)
    {
        auto area = S (Rectangle<float> (60.0f, 120.0f, kSW - 120.0f, 250.0f));
        g.setFont (font (tfBold, 30.0f));
        g.setColour (kText);
        g.drawText ("OCTOPUS", area.removeFromTop (S (50.0f)), Justification::centred);
        g.setFont (font (tfMedium, 21.0f));
        g.setColour (kAccent);
        g.drawText (eng.statusText(), area.removeFromTop (S (40.0f)), Justification::centred, true);
        g.setFont (font (tfRegular, 17.0f));
        g.setColour (kDim);
        g.drawFittedText ("Insert Octopus on a track in REAPER : its screen shows that track's FX chain.\n"
                          "Add, remove, move and switch effects here ; they change in the track's FX chain at the same time.",
                          area.toNearestInt(), Justification::centredTop, 3);
    }

    void drawFooter (Graphics& g)
    {
        g.setColour (kFaint.withAlpha (0.45f));
        g.fillRect (S (Rectangle<float> (0.0f, kFooterLine, kSW, 1.0f)));
        const bool inPresetMode = eng.mode() == OctopusEngine::presetMode;
        for (int f = 0; f < OctopusEngine::kKeys; ++f)
        {
            const auto cell = footerCell (f);
            const auto c = S (cell);
            const bool lit = eng.fsLit (f);
            const int kind = eng.fsKind (f);
            const auto col = inPresetMode ? Colour (eng.presetColour (eng.bank() * OctopusEngine::kSlots + f)) : (kind >= 0 ? FxCat::colour (kind) : kDim);
            const bool hov = hoverFooter == f;
            g.setColour (lit ? col.withAlpha (0.16f) : (hov ? kScrPanel2.brighter (0.1f) : kScrPanel));
            g.fillRoundedRectangle (c, S (9.0f));
            g.setColour (lit ? col : kFaint.withAlpha (hov ? 1.0f : 0.7f));
            g.drawRoundedRectangle (c.reduced (S (0.6f)), S (9.0f), S (lit ? 2.0f : 1.2f));
            // lettre du footswitch
            const auto letter = S (Rectangle<float> (cell.getX() + 8.0f, cell.getY() + 13.0f, 34.0f, 34.0f));
            g.setColour (lit ? col : kScrPanel2.brighter (0.2f));
            g.fillRoundedRectangle (letter, S (6.0f));
            g.setFont (font (tfBold, 20.0f));
            g.setColour (lit ? kScrBg : kText.withAlpha (0.8f));
            g.drawText (fsLetter (f), letter.translated (0.0f, S (1.0f)), Justification::centred);
            g.setFont (font (tfBold, 13.0f));
            g.setColour (lit ? col.brighter (0.3f) : kDim);
            g.drawText (eng.fsTitle (f), S (Rectangle<float> (cell.getX() + 50.0f, cell.getY() + 8.0f, cell.getWidth() - 54.0f, 18.0f)), Justification::centredLeft, true);
            g.setFont (font (tfMedium, 16.0f));
            g.setColour (lit ? kText : kText.withAlpha (0.75f));
            g.drawText (eng.fsText (f), S (Rectangle<float> (cell.getX() + 50.0f, cell.getY() + 28.0f, cell.getWidth() - 54.0f, 24.0f)), Justification::centredLeft, true);
        }
    }

    Rectangle<float> footerCell (int f) const
    {
        return { kFooterGap + (kFooterW + kFooterGap) * (float) f + (f >= 4 ? kFooterSplit : 0.0f), kFooterY, kFooterW, kFooterH };
    }
    int footerAt (Point<float> p) const
    {
        if (overlay != Overlay::none || ! eng.isReady()) return -1;
        for (int f = 0; f < OctopusEngine::kKeys; ++f) if (footerCell (f).contains (p)) return f;
        return -1;
    }

    // ------------------------------------------------------------ fenêtres sur l'écran
    void drawOverlay (Graphics& g)
    {
        g.setColour (kScreenBlack);
        g.fillRect (S (kOverlay));
        g.setColour (kFaint.withAlpha (0.6f));
        g.fillRect (S (Rectangle<float> (0.0f, kOverlay.getY(), kSW, 1.0f)));
        g.setFont (font (tfBold, 20.0f));
        g.setColour (kText);
        if (overlay == Overlay::browser)
        {
            g.drawText ("ADD AN EFFECT", S (Rectangle<float> (16.0f, kRowMid - 18.0f, 220.0f, 36.0f)), Justification::centredLeft);
            // tri : interrupteur segmenté (fond commun)
            const auto seg = Rectangle<float> (kSortX, kRowY, kSortW, kRowH);
            g.setColour (kScrPanel2);
            g.fillRoundedRectangle (S (seg), S (7.0f));
            g.setColour (kFaint);
            g.drawRoundedRectangle (S (seg).reduced (S (0.6f)), S (7.0f), S (1.2f));
            // champ de recherche : fond, cadre arrondi (bleu quand on tape), loupe
            const bool foc = search.hasKeyboardFocus (true);
            g.setColour (Colour (0xff13171d));
            g.fillRoundedRectangle (S (kSearchR), S (10.0f));
            g.setColour (foc ? kAccent.withAlpha (0.8f) : Colour (0xff262c36));
            g.drawRoundedRectangle (S (kSearchR).reduced (S (0.6f)), S (10.0f), S (1.2f));
            {
                const auto c = Point<float> (kSearchR.getX() + 21.0f, kSearchR.getCentreY() - 1.0f);
                Path lens;
                lens.addEllipse (S (Rectangle<float> (13.0f, 13.0f).withCentre (c)));
                lens.startNewSubPath (S (c + Point<float> (5.0f, 5.0f)));
                lens.lineTo (S (c + Point<float> (10.0f, 10.0f)));
                g.setColour (kDim);
                g.strokePath (lens, PathStrokeType (S (1.8f), PathStrokeType::curved, PathStrokeType::rounded));
            }
            // filet sous les onglets
            g.setColour (Colour (0xff1c2129));
            g.fillRect (S (Rectangle<float> (16.0f, kTabsY + kTabsH, kSW - 32.0f, 1.0f)));
            drawBrowserList (g);
        }
        else if (overlay == Overlay::params) drawParams (g);
        else if (overlay == Overlay::banks)  drawBanks (g);
        for (auto& b : overlayButtons())
        {
            const bool colourBtn = b.id >= bFsColour && b.id <= bFsColour + OctopusEngine::kFs;
            if (b.id == bSortAZ || b.id == bSortType)                // tri : moitié de l'interrupteur segmenté
            {
                const auto rr = S (b.r);
                if (b.active) { g.setColour (kAccent); g.fillRoundedRectangle (rr, S (6.0f)); }
                else if (hoverBtn == b.id) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (rr, S (6.0f)); }
                g.setFont (font (tfBold, 12.5f).withExtraKerningFactor (0.12f));
                g.setColour (b.active ? kScrBg : kDim);
                g.drawText (b.text, rr, Justification::centred, false);
                continue;
            }
            if (b.id >= bChip && b.id < bChip + kNumChips)          // onglet : texte + nombre, souligné quand actif
            {
                const int cnt = chipCount (b.id - bChip);
                const auto f = font (tfMedium, 15.5f);
                const auto fc = font (tfMedium, 12.0f);
                const float tw = unitsOf (f, b.text), x0 = b.r.getX() + 4.0f;
                const bool hov = hoverBtn == b.id;
                const auto ty = Rectangle<float> (x0, b.r.getY(), tw + 4.0f, b.r.getHeight() - 6.0f);
                g.setFont (f);
                g.setColour (b.active ? kText : (hov ? kText.withAlpha (0.85f) : kDim));
                g.drawText (b.text, S (ty), Justification::centredLeft, false);
                if (cnt > 0)
                {
                    g.setFont (fc);
                    g.setColour (b.active ? kAccent : Colour (0xff596273));
                    g.drawText (String (cnt), S (Rectangle<float> (x0 + tw + 6.0f, b.r.getY() + 1.0f, 60.0f, b.r.getHeight() - 6.0f)), Justification::centredLeft, false);
                }
                continue;
            }
            if (b.id == bInsertWhere)                               // emplacement : pastille cliquable « → before Pipeline ▾ »
            {
                const auto rr = S (b.r);
                const bool hov = hoverBtn == b.id || insertOpen;               // même forme que ADD, sans remplissage
                g.setColour (insertOpen ? kAccent.withAlpha (0.8f) : hov ? kDim : kFaint);
                g.drawRoundedRectangle (rr.reduced (S (0.6f)), S (7.0f), S (1.2f));
                const auto fd = font (tfRegular, 14.0f), fn = font (tfMedium, 14.5f);
                const String pre = insertPos >= eng.blocks().size() ? String (CharPointer_UTF8 ("\xe2\x86\x92  at")) : String (CharPointer_UTF8 ("\xe2\x86\x92  before"));
                float x = b.r.getX() + 14.0f;
                const float pw = unitsOf (fd, pre);
                g.setFont (fd); g.setColour (kDim);
                g.drawText (pre, S (Rectangle<float> (x, b.r.getY(), pw + 4.0f, b.r.getHeight())), Justification::centredLeft, false);
                x += pw + 6.0f;
                g.setFont (fn); g.setColour (kText);
                g.drawText (b.text, S (Rectangle<float> (x, b.r.getY(), b.r.getRight() - 26.0f - x, b.r.getHeight())), Justification::centredLeft, true);
                Path tri;                                              // ▾
                const float cx = b.r.getRight() - 16.0f, cy = b.r.getCentreY();
                tri.addTriangle (S (Point<float> (cx - 4.0f, cy - 2.0f)), S (Point<float> (cx + 4.0f, cy - 2.0f)), S (Point<float> (cx, cy + 3.0f)));
                g.setColour (kDim);
                g.fillPath (tri);
                continue;
            }
            if (overlay == Overlay::browser && b.id == bClearPicked)    // « 3 selected  ✕ » : un clic vide la sélection
            {
                const auto rr = S (b.r);
                const bool hov = hoverBtn == b.id;
                g.setColour (hov ? kScrPanel2.brighter (0.15f) : kScrPanel2);
                g.fillRoundedRectangle (rr, S (7.0f));
                g.setColour (hov ? kDim : kFaint);
                g.drawRoundedRectangle (rr.reduced (S (0.6f)), S (7.0f), S (1.2f));
                g.setFont (font (tfMedium, 15.0f));
                g.setColour (kText);
                g.drawText (b.text, S (b.r.withTrimmedLeft (14.0f).withTrimmedRight (30.0f)), Justification::centredLeft, false);
                g.setFont (font (tfRegular, 15.0f));
                g.setColour (hov ? kText : kDim);
                g.drawText (CharPointer_UTF8 ("\xe2\x9c\x95"), S (b.r.withTrimmedLeft (b.r.getWidth() - 30.0f)), Justification::centred, false);
                continue;
            }
            if (overlay == Overlay::browser && b.id == bAddPicked)      // ADD : bouton principal plein quand il y a une sélection
            {
                const auto rr = S (b.r);
                const bool hov = hoverBtn == b.id && b.enabled;
                if (b.enabled)
                {
                    g.setColour (hov ? kAccent.brighter (0.15f) : kAccent);
                    g.fillRoundedRectangle (rr, S (7.0f));
                }
                else
                {
                    g.setColour (kAccent.withAlpha (0.16f));
                    g.fillRoundedRectangle (rr, S (7.0f));
                }
                g.setFont (font (tfBold, 14.5f).withExtraKerningFactor (0.08f));
                g.setColour (b.enabled ? kScrBg : kAccent.withAlpha (0.45f));
                g.drawText (b.text, rr, Justification::centred, false);
                continue;
            }
            if (b.id == bReload)                                    // rescan : icône (flèche circulaire)
            {
                auto bb = b; bb.text = {};
                drawButton (g, bb);
                const auto c = S (b.r.getCentre());
                const float rad = S (5.5f);
                Path arc;
                arc.addCentredArc (c.x, c.y, rad, rad, 0.0f, MathConstants<float>::pi * 0.45f, MathConstants<float>::pi * 1.95f, true);
                g.setColour (kText.withAlpha (hoverBtn == b.id ? 1.0f : 0.8f));
                g.strokePath (arc, PathStrokeType (S (1.6f), PathStrokeType::curved, PathStrokeType::rounded));
                const auto top = c + Point<float> (0.0f, -rad);         // bout de l'arc en haut : flèche vers la droite (sens horaire)
                Path head;
                head.addTriangle (top + Point<float> (S (3.2f), 0.0f), top + Point<float> (S (-1.2f), S (-3.2f)), top + Point<float> (S (-1.2f), S (3.2f)));
                g.fillPath (head);
                continue;
            }
            if (! colourBtn) drawButton (g, b, b.id == bClose ? 18.0f : 15.0f);
            else                                                   // pastille à gauche, nom centré dans la place qui reste
            {
                const bool tuner = b.id == bFsColour + OctopusEngine::kFs;
                auto bb = b; bb.text = {};
                drawButton (g, bb);
                const auto f = font (tfBold, 15.0f);
                g.setFont (f);
                g.setColour (kText.withAlpha (0.85f));
                const auto area = b.r.withTrimmedLeft (32.0f).withTrimmedRight (6.0f).translated (-8.0f, 0.0f);   // à droite de la pastille, 8 plus à gauche
                if (! tuner) g.drawText (b.text, S (area), Justification::centred, true);   // A-F : lettre centrée dans cette place
                else                                                   // TUNER : commence près de l'endroit où commencent les lettres
                {
                    const float letterW = GlyphArrangement::getStringWidth (f, "A") / S (1.0f);
                    g.drawText (b.text, S (area.withTrimmedLeft ((area.getWidth() - letterW) * 0.5f - 13.5f).withWidth (60.0f)), Justification::centredLeft, false);
                }
            }
            if (colourBtn)                                         // pastille de la couleur choisie
            {
                const auto sw = S (Rectangle<float> (b.r.getX() + 10.0f, b.r.getCentreY() - 8.0f, 16.0f, 16.0f));
                g.setColour (b.id - bFsColour == OctopusEngine::kFs ? tunerColour() : Colour (eng.fsColour (b.id - bFsColour)));
                g.fillEllipse (sw);
                g.setColour (Colours::white.withAlpha (0.3f));
                g.drawEllipse (sw, S (1.0f));
            }
        }
        drawFxTooltip (g);                                         // survol d'un plugin NINE HANDS | DSP : sa description
        if (insertOpen && overlay == Overlay::browser) drawInsertDrum (g);   // par-dessus les onglets et les boutons
        if (overlay == Overlay::banks) drawBankDots (g);
        if (overlay == Overlay::banks)
        {
            g.setFont (font (tfBold, 15.0f));
            g.setColour (kDim);
            g.drawText ("COLOURS AND GLOW (PRESET MODE)", S (Rectangle<float> (16.0f, 480.0f, bankRect (3).getRight() - 16.0f, 44.0f)), Justification::centredRight);
        }
    }

    // navigateur : filtre (catégorie + recherche), deux colonnes
    Array<InstalledFx> filtered()
    {
        Array<InstalledFx> out;
        const auto q = search.getText().trim().toLowerCase();
        for (auto& f : eng.installedList())
        {
            const auto type = FxCat::typeOf (f.name).toUpperCase();
            if (type.endsWith ("I")) continue;                          // instruments (VSTi, VST3i, CLAPi, AUi)
            switch (chip)
            {
                case 1: if (! favs.contains (f.name)) continue; break;
                case 2: if (! FxCat::isOwnVendor (FxCat::vendor (f.name))) continue; break;
                case 3: if (type != "VST3") continue; break;
                case 4: if (type != "VST" && type != "VST2") continue; break;
                case 5: if (type != "CLAP") continue; break;
                case 6: if (type != "JS") continue; break;
                case 7: if (type != "AU") continue; break;
                default: break;
            }
            if (q.isNotEmpty() && ! f.name.toLowerCase().contains (q)) continue;
            out.add (f);
        }
        return out;
    }

    // ---- favoris : étoile en bout de ligne (vide / pleine jaune), bouton Favorites dans les filtres
    StringArray favs = OctoSettings::favourites();
    static Rectangle<float> starRect (Rectangle<float> item) { return item.withTrimmedLeft (item.getWidth() - 34.0f).reduced (4.0f, 2.0f); }
    void toggleFavourite (const InstalledFx& f)
    {
        favs = OctoSettings::favourites();                 // une autre fenêtre d'Octopus a pu les changer
        if (favs.contains (f.name)) { favs.removeString (f.name); eng.say (FxCat::shortName (f.name) + " removed from favorites"); }
        else { favs.add (f.name); eng.say (FxCat::shortName (f.name) + " added to favorites"); }
        OctoSettings::setFavourites (favs);
        if (chip == kChipFavourites) listScroll = jlimit (0, browserMaxScroll(), listScroll);
        layoutEditors();                                   // le nombre de favoris a changé : largeur des filtres
        askRepaint (screenPx());
    }
    static Path starPath (Rectangle<float> r)
    {
        Path p;
        const auto c = r.getCentre();
        const float ro = jmin (r.getWidth(), r.getHeight()) * 0.5f, ri = ro * 0.45f;
        for (int k = 0; k < 10; ++k)
        {
            const float a = MathConstants<float>::pi * (-0.5f + (float) k / 5.0f), rr = (k % 2 == 0) ? ro : ri;
            const Point<float> q (c.x + rr * std::cos (a), c.y + rr * std::sin (a) + ro * 0.06f);
            if (k == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        p.closeSubPath();
        return p;
    }

    // description courte des plugins NINE HANDS | DSP (survol dans la fenêtre d'ajout), d'après leur nom
    static String wigaDescription (const String& shortName)
    {
        static const std::pair<const char*, const char*> d[] = {
            { "octopus",      "FX chain controller for REAPER: the track's effects on a floor-modeler screen, 96 presets, 6 footswitches and MIDI." },
            { "chroma",       "Tuner for guitar and bass: MONO, POLY (all strings at once) and STROBE modes, down to very low tunings." },
            { "ngate",        "Noise gate for guitar and bass: cuts hum and hiss between notes, keeps the attack; LEARN sets the threshold, SOFT / HARD and BASS modes." },
            { "crycat",       "Wah in the spirit of the Dunlop Cry Baby GCB95: 6 sweep ranges, Q, PURR (inductor grit), BOOST, MAN / AUTO auto-engage, expression pedal that tilts on screen." },
            { "doorn",        "Distortion in the spirit of the Lichtlaerm Thorn: from crunch to thick, saturated high gain, with a lighting boost knob and three voicings." },
            { "peachy",       "Preamp and fuzz in the spirit of the KoRn Indigo Ranch pedal: tube-style preamp, NAM-captured fuzz and Semitones octave doubler in one double pedal." },
            { "strong pickle","Fuzz in the spirit of the Way Huge Swollen Pickle: huge, thick and sustaining fuzz." },
            { "wharry",       "Pitch shifter in the spirit of the DigiTech Whammy 5: 21 programs (Whammy, Detune, Harmony), CLASSIC / CHORDS, expression pedal that tilts on screen, GLIDE, TONE, MIX." },
            { "fog z",        "Polyphonic octave generator in the spirit of the EHX POG2: -2/-1/+1/+2 octaves on every string, Attack swell, resonant LP filter, Detune, 8 presets on the knob." },
            { "semitones",    "Transposition in the spirit of the Solar CHUG Capo: play in another key without retuning." },
            { "pipeline",     "Pitch vibrato in the spirit of the EarthQuaker Aqueduct, with several modulation shapes." },
            { "small tone",   "Analog chorus in the spirit of the EHX Small Clone: RATE and DIN. / DEPTH, lush watery swirl, tempo sync on the STATUS LED." },
            { "flangerus",    "Chorus and flanger in the spirit of the Ibanez CF7: SPEED, DEPTH, DELAY TIME, REGEN., MODE chorus / flanger and KRAZY Wack'd for vibrato and wild chirps." },
            { "wack",         "Chorus and flanger in the spirit of the Ibanez CF7: SPEED, DEPTH, DELAY TIME, REGEN., MODE chorus / flanger and KRAZY Wack'd for vibrato and wild chirps." },
            { "phase",        "Phaser in the spirit of the MXR Phase 90: one SPEED knob, SCRIPT / BLOCK on the PHASE badge, tempo sync on the SYNC LED." },
            { "ring think",   "Ring modulator in the spirit of the EHX Ring Thing: ring mod, single-sideband shifter, pitch shift and phaser, 9 presets, carrier tuned to the note you play." },
            { "echoflex",     "Tape echo in the spirit of the Dunlop Echoplex EP103: warm tape repeats, AGE for worn tape, tap tempo and DAW sync." },
            { "eight echo",   "Digital delay in the spirit of the Boss DD-8: 11 modes (analog, tape, shimmer, reverse, glitch...), 40 s looper, tap tempo, DAW sync, 7 memories." },
            { "holyverb",     "Reverb in the spirit of the TC Electronic Hall of Fame: 8 modes from room to church, shimmer and spring, PRE DELAY and TAILS switches, expression pedal." },
            { "freeze",       "Freeze and shimmer reverb in the spirit of the EHX Freeze and Pico Shimmer: infinite sustain on the footswitch, shimmer pitch, scenes, two pages of knobs (MODE LED), 11 presets." },
            { "textural",     "Granular engine in the spirit of the MXR Textures: Delay, Auto Freeze and Layers modes, REVERSE, grain PITCH, 9 presets." },
            { "arc",          "EBow simulation: endless sustain and violin-like swells, NORMAL / HARM / HARM+ voices, bow-flow string display, BOW and HOLD footswitches." },
            { "freakout",     "Controlled feedback in the spirit of the DigiTech FreqOut: natural guitar feedback at any volume." },
            { "fm-9",         "6-operator FM synthesizer in the spirit of the M-VAVE FM-1: DX7 engine with 32 algorithms and DX7 SysEx banks, 6 reorderable effects, arpeggiator and 16-step sequencer; also runs the FM-1 firmwares: real update packages (.fwsc) in an emulation of the FM-1 chip (clock set automatically for each firmware, live monitoring) and web builds, key lights exactly as the firmware drives them, with a gallery catalog of 338 firmwares (favorites, new arrivals, updates filter, sort by date or name, one-click updates); FM-1 colour skins with adjustable light colour and glow, interactive screen, MIDI Learn on every knob and button with saveable MIDI maps, Arturia KeyStep 37 profile, NKS presets." },
            { "synthetika",   "Guitar synth in the spirit of the Audio Hertz Soklo GS-5: 5 engines (Deep, Vintage, Solo, Strings, Harmony) tracking single notes, bends and slides from 28 Hz (bass, 7 and 8 strings), Distortion, Modulation, Ring Mod, Bitcrush and Gated Reverb in a reorderable chain." },
            { "fantomine",    "Theremin voice played from your guitar: gliding ghost voice, pitch pedal, octave whammy, HOLD on the footswitch." },
            { "jawari",       "Sitar sound for guitar: the buzzing, resonant voice of the Indian sitar." },
            { "ampire",       "Neural Amp Modeler loader for guitar and bass: plays NAM captures of real amps and pedals (A1 and A2)." },
            { "anode",        "Tube power amp stage for guitar and bass: 6L6, EL34, EL84 or KT88, single-ended or push-pull." },
            { "darkbass",     "Bass preamp and overdrive in the spirit of the Darkglass Microtubes B7K Ultra: grit, blend and EQ." },
            { "impulse",      "Dual cabinet IR loader for guitar and bass: two impulse responses A and B with a MIX knob, and an IR library with TONE3000 sync." },
            { "screamer",     "Overdrive in the spirit of the Ibanez TS9 Tube Screamer: smooth mid-hump drive and boost." },
            { "stereo width", "Stereo width: from mono to wide stereo." } };
        const auto n = shortName.toLowerCase();
        for (auto& x : d) if (n.contains (x.first)) return x.second;
        return {};
    }
    // bulle au survol d'un plugin NINE HANDS | DSP (VST3) dans la fenêtre d'ajout, au bout de 0,8 s sur la même ligne
    static constexpr double kTipDelayMs = 800.0;
    double hoverItemMs = 0.0;
    bool tipShown = false;
    void drawFxTooltip (Graphics& g)
    {
        if (overlay != Overlay::browser || hoverItem < 0 || insertOpen) return;
        if (Time::getMillisecondCounterHiRes() - hoverItemMs < kTipDelayMs) return;      // la bulle n'apparaît qu'au bout de 0,8 s
        const auto items = filtered();
        const int i = browserItemAt (hoverItem);
        if (! isPositiveAndBelow (i, items.size())) return;
        const auto& f = items.getReference (i);
        if (! FxCat::isOwnVendor (FxCat::vendor (f.name)) || FxCat::typeOf (f.name).toUpperCase() != "VST3") return;
        const auto desc = wigaDescription (FxCat::shortName (f.name));
        if (desc.isEmpty()) return;
        const auto ir = itemRect (hoverItem);
        const float w = 440.0f, h = 84.0f;
        float x = jlimit (16.0f, kSW - 16.0f - w, ir.getX() + 40.0f);
        float y = ir.getBottom() + 6.0f;
        if (y + h > kSH - 8.0f) y = ir.getY() - 6.0f - h;
        const Rectangle<float> box (x, y, w, h);
        const int kind = FxCat::classify (f.name);
        const auto col = FxCat::colour (kind);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (S (box.translated (0.0f, 3.0f).expanded (2.0f)), S (9.0f));
        g.setColour (Colour (0xff151922));
        g.fillRoundedRectangle (S (box), S (8.0f));
        g.setColour (col.withAlpha (0.75f));
        g.drawRoundedRectangle (S (box).reduced (S (0.6f)), S (8.0f), S (1.3f));
        g.fillRoundedRectangle (S (box.withWidth (5.0f).reduced (0.0f, 8.0f).translated (8.0f, 0.0f)), S (2.5f));
        FxCat::drawIcon (g, kind, S (Rectangle<float> (26.0f, 26.0f).withCentre ({ x + 36.0f, y + 24.0f })), col.brighter (0.3f), S (1.9f), FxCat::iconOf (f.name));
        g.setFont (font (tfBold, 17.0f));
        g.setColour (kText);
        g.drawText (FxCat::shortName (f.name), S (Rectangle<float> (x + 58.0f, y + 10.0f, w - 70.0f, 24.0f)), Justification::centredLeft, true);
        g.setFont (font (tfRegular, 12.5f));
        g.setColour (col.brighter (0.2f));
        g.drawText (String (FxCat::label (kind)).toUpperCase() + "  -  NINE HANDS | DSP", S (Rectangle<float> (x + 58.0f, y + 10.0f, w - 70.0f, 24.0f)), Justification::centredRight, true);
        g.setFont (font (tfRegular, 14.5f));
        g.setColour (kText.withAlpha (0.85f));
        g.drawFittedText (desc, S (Rectangle<float> (x + 22.0f, y + 40.0f, w - 34.0f, 38.0f)).toNearestInt(), Justification::topLeft, 2, 1.0f);
    }

    static constexpr int kListRows = 12;
    Rectangle<float> itemRect (int visibleIndex) const
    {
        const int col = visibleIndex / kListRows, row = visibleIndex % kListRows;
        if (overlay == Overlay::browser) return { 16.0f + 520.0f * (float) col, kListTopB + 33.0f * (float) row, 512.0f, 30.0f };   // sous les onglets
        return { 16.0f + 520.0f * (float) col, 182.0f + 34.0f * (float) row, 512.0f, 31.0f };
    }

    // ---- navigateur : tri A-Z ou par type (titres colorés), indices « il y en a d'autres »
    bool browserByType = OctoSettings::glowOf ("browserByType") == 1;
    // lignes de la liste (v1.18.0) : deux plugins côte à côte, ou un titre de type sur toute la largeur (TYPE) ;
    // les deux tris défilent verticalement, ligne à ligne, en glissant
    struct BLine { int a = -1, b = -1, kind = -1, count = 0; bool header() const { return a < 0 && kind >= 0; } };
    static const int* kindOrder() { static const int o[] = { FxCat::amp, FxCat::cab, FxCat::drive, FxCat::dynamics, FxCat::filter, FxCat::delay, FxCat::reverb,
                                                             FxCat::mod, FxCat::pitch, FxCat::special, FxCat::tuner, FxCat::eq, FxCat::utility, FxCat::other }; return o; }
    std::vector<BLine> browserLines (const Array<InstalledFx>& items) const
    {
        std::vector<BLine> lines;
        auto pairs = [&] (const Array<int>& idx, int kind)
        {
            for (int k = 0; k < idx.size(); k += 2) lines.push_back ({ idx[k], k + 1 < idx.size() ? idx[k + 1] : -1, kind, 0 });
        };
        if (! browserByType) { Array<int> all; for (int i = 0; i < items.size(); ++i) all.add (i); pairs (all, -1); return lines; }
        for (int k = 0; k < FxCat::numKinds; ++k)
        {
            const int kind = kindOrder()[k];
            Array<int> idx;
            for (int i = 0; i < items.size(); ++i) if (FxCat::classify (items.getReference (i).name) == kind) idx.add (i);
            if (idx.isEmpty()) continue;
            lines.push_back ({ -1, -1, kind, idx.size() });
            pairs (idx, kind);
        }
        return lines;
    }
    int browserMaxScroll() { return jmax (0, (int) browserLines (filtered()).size() - kListRows); }
    int browserItemAt (int v)                                        // plugin sous la case visible v (indice dans filtered()), -1 sinon
    {
        const auto lines = browserLines (filtered());
        const int l = listScroll + v % kListRows;
        if (! isPositiveAndBelow (l, (int) lines.size())) return -1;
        return v / kListRows == 0 ? lines[(size_t) l].a : lines[(size_t) l].b;
    }
    // ---- haut du navigateur : titre, emplacement, tri, sélection, ADD ; recherche pleine largeur ; onglets
    // ligne du titre centrée entre le bandeau (62) et la recherche (130)
    static constexpr float kRowMid = (kHeaderH + 130.0f) * 0.5f, kRowH = 26.0f, kRowY = kRowMid - kRowH * 0.5f;
    // cases de droite, même hauteur et même écart (kRowGap), calées sur le bord droit de la recherche
    static constexpr float kRowGap = 8.0f, kCloseX = 1048.0f - 36.0f, kReloadX = kCloseX - kRowGap - 36.0f, kAddW = 102.0f, kAddX = kReloadX - kRowGap - kAddW,
                           kSelW = 118.0f, kSelX = kAddX - kRowGap - kSelW, kSortW = 126.0f;
    static constexpr float kSortX = kSelX - kRowGap - kSortW, kTabsY = 176.0f, kTabsH = 28.0f, kTabGap = 26.0f, kListTopB = 214.0f;
    static inline const Rectangle<float> kSearchR { 16.0f, 130.0f, 1032.0f, 38.0f };
    Rectangle<float> selPillRect() const { return { kSelX, kRowY, kSelW, kRowH }; }
    String insertWhereText() const
    {
        const int n = eng.blocks().size();
        if (insertPos >= n) return "the end of the chain";
        return eng.blockName (eng.blocks()[insertPos]);
    }
    Rectangle<float> insertWhereRect() const
    {
        const float pre = unitsOf (font (tfRegular, 14.0f), insertPos >= eng.blocks().size() ? String (CharPointer_UTF8 ("\xe2\x86\x92  at")) : String (CharPointer_UTF8 ("\xe2\x86\x92  before")));
        const float w = jmin (330.0f, 14.0f + pre + 6.0f + unitsOf (font (tfMedium, 14.5f), insertWhereText()) + 30.0f);
        const float x = 16.0f + unitsOf (font (tfBold, 20.0f), "ADD AN EFFECT") + 16.0f;   // juste après le titre
        return { x, kRowY, w, kRowH };
    }
    // ---- tambour de l'emplacement (comme celui de Semitones) : au début, avant chaque bloc (un groupe compte pour un), à la fin
    int drumBgStyle = 2;                                           // fond des tambours : 2 = presque opaque + halo (choisi, v1.18.2) ; 0 / 1 : essais
    // tous les tambours : fond presque opaque et léger halo de couleur derrière la bande du milieu (lisibilité)
    void drumHalo (Graphics& g, Rectangle<float> bandUnits, float radius, Colour col)
    {
        const auto c = S (bandUnits.getCentre());
        g.setGradientFill (ColourGradient (col.withAlpha (0.10f), c, col.withAlpha (0.0f), c + Point<float> (S (bandUnits.getWidth() * 0.8f), 0.0f), true));
        g.fillEllipse (Rectangle<float> (S (bandUnits.getWidth() * 1.6f), S (radius * 2.2f)).withCentre (c));
    }
    bool insertOpen = false; int insertTarget = 0, hoverInsert = -1; float insertDrumPos = 0.0f;
    std::vector<std::pair<int, String>> insertChoices() const
    {
        std::vector<std::pair<int, String>> v;
        const auto& bl = eng.blocks();
        for (int k = 0; k < bl.size(); ++k)
        {
            const int role = eng.roleAt (bl[k]);
            if (role != Routing::none && role != Routing::split) continue;     // repères des voies parallèles : pas un emplacement
            if (eng.groupAt (bl[k]) >= 0 && role != Routing::split) continue;   // dans un groupe : on insère avant le groupe
            v.push_back ({ k, (v.empty() ? "At the start, before " : "Before ") + eng.blockName (bl[k]).replace (" || ", String (CharPointer_UTF8 (" \xe2\x80\x96 "))) });
        }
        v.push_back ({ bl.size(), "At the end of the chain" });
        return v;
    }
    static constexpr float kInsMidY = (kRowY + kRowH + kDrumGap + kDrumBandH * 0.5f), kInsR = kDrumR, kInsStep = kDrumStep;   // comme Semitones, même écart sous la pastille
    float insY (float d) const { return kInsMidY + kInsR * std::sin (jlimit (-5.5f, 5.5f, d) * kInsStep * MathConstants<float>::pi / 180.0f); }
    float insNamesW() const { float w = 0.0f; for (auto& c : insertChoices()) w = jmax (w, unitsOf (font (tfBold, 18.3f), c.second)); return w; }
    Rectangle<float> insBand() const
    {
        const float w = insNamesW() + 48.0f, cx = jlimit (16.0f + w * 0.5f, kSW - 16.0f - w * 0.5f, insertWhereRect().getCentreX());   // centré sur la pastille
        return { cx - w * 0.5f, kInsMidY - kDrumBandH * 0.5f, w, kDrumBandH };
    }
    void openInsertDrum()
    {
        hoverItem = -1;
        const auto ch = insertChoices();
        insertTarget = (int) ch.size() - 1;
        for (int i = (int) ch.size() - 1; i >= 0; --i) if (ch[(size_t) i].first <= jmin (insertPos, eng.blocks().size())) { insertTarget = i; break; }   // dans un groupe : avant le groupe
        insertDrumPos = (float) insertTarget; hoverInsert = -1; insertOpen = true;
        search.setVisible (false);                                   // le champ (composant) passerait par-dessus le tambour
        askRepaint (screenPx());
    }
    void closeInsertDrum() { if (! insertOpen) return; insertOpen = false; hoverInsert = -1; search.setVisible (overlay == Overlay::browser); askRepaint (screenPx()); }
    void chooseInsert (int i)
    {
        const auto ch = insertChoices();
        if (! isPositiveAndBelow (i, (int) ch.size())) return;
        insertPos = ch[(size_t) i].first; insertTarget = i;
        askRepaint (screenPx());
    }
    int insertItemAt (Point<float> p) const
    {
        const auto band = insBand();
        if (p.x < band.getX() || p.x > band.getRight()) return -1;
        const int n = (int) insertChoices().size();
        int best = -1; float bd = 1.0e9f;                               // comme Semitones : la ligne la plus proche
        for (int i = 0; i < n; ++i)
        {
            const float d = (float) i - insertDrumPos;
            if (std::abs (d) > 4.3f) continue;
            const float h = 13.5f * std::cos (d * kInsStep * MathConstants<float>::pi / 180.0f);
            const float dy = std::abs (p.y - insY (d));
            if (dy <= h && dy < bd) { bd = dy; best = i; }
        }
        return best;
    }
    void drawInsertDrum (Graphics& g)
    {
        const auto col = kAccent;
        const auto ch = insertChoices();
        const float vy = kRowY + kRowH + 6.0f;
        g.setColour (Colour (0xff05070b).withAlpha (drumBgStyle == 0 ? 0.94f : 0.985f));
        g.fillRect (S (Rectangle<float> (0.0f, vy, kSW, kSH - vy)));   // tout sous la ligne du titre
        {
            const auto bd = insBand();
            const float top = jmax (vy + 6.0f, insY (-4.6f) - 14.0f), bot = kInsMidY + kInsR + 50.0f;
            if (drumBgStyle == 1)                                       // A : panneau sombre derrière la colonne
            {
                const auto pr = Rectangle<float> (bd.getX() - 26.0f, top, bd.getWidth() + 52.0f, bot - top);
                g.setColour (Colour (0xff0d1015));
                g.fillRoundedRectangle (S (pr), S (12.0f));
                g.setColour (kFaint);
                g.drawRoundedRectangle (S (pr).reduced (S (0.6f)), S (12.0f), S (1.2f));
            }
            else if (drumBgStyle == 2) drumHalo (g, bd, kInsR, kAccent);   // B : halo léger derrière la bande
        }
        const auto band = S (insBand());
        for (int l = 3; l >= 1; --l)
        {
            g.setColour (col.withAlpha (0.05f));
            g.fillRoundedRectangle (band.expanded (S (3.0f * (float) l)), S (6.0f + 3.0f * (float) l));
        }
        g.setColour (col.withAlpha (0.10f));
        g.fillRoundedRectangle (band, S (6.0f));
        g.setColour (col.withAlpha (0.55f));
        g.drawRoundedRectangle (band, S (6.0f), S (0.9f));
        const float cx = insBand().getCentreX();
        Graphics::ScopedSaveState clipTop (g);
        g.reduceClipRegion (S (Rectangle<float> (0.0f, kRowY + kRowH + 4.0f, kSW, kSH)).toNearestInt());
        for (int i = 0; i < (int) ch.size(); ++i)
        {
            const float d = (float) i - insertDrumPos, ad = std::abs (d);
            if (ad > 4.6f) continue;
            const float a = d * kInsStep * MathConstants<float>::pi / 180.0f;
            float op = jmax (0.06f, 1.0f - ad * 0.24f);
            if (i == hoverInsert && ad > 0.5f) op = jmin (1.0f, op + 0.25f);
            const bool sel = ad < 0.5f;
            const auto cy = S (Point<float> (cx, insY (d)));
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::scale (1.0f - ad * 0.08f, (1.0f - ad * 0.08f) * std::cos (a), cy.x, cy.y));
            g.setFont (sel ? font (tfBold, 18.3f) : font (tfMedium, 16.7f));
            g.setColour ((sel ? Colours::white : kText).withAlpha (op));
            g.drawText (ch[(size_t) i].second, Rectangle<float> (cy.x - S (insNamesW() * 0.5f + 20.0f), cy.y - S (13.65f), S (insNamesW() + 40.0f), S (27.3f)), Justification::centred, false);
        }
        const auto hintF = font (tfBold, 11.6f).withExtraKerningFactor (0.14f);
        const String hintT = CharPointer_UTF8 ("SCROLL  \xc2\xb7  CLICK TO CHOOSE");
        const float hx = cx - (13.0f + 6.0f + unitsOf (hintF, hintT)) * 0.5f, hy = kInsMidY + kInsR + 33.0f;
        const auto mr = S (Rectangle<float> (hx, hy, 13.0f, 19.0f));
        g.setColour (Colour (0xff9aa4b4));
        g.drawRoundedRectangle (mr, mr.getWidth() * 0.5f, S (1.2f));
        g.setColour (col);
        g.fillRoundedRectangle (S (Rectangle<float> (hx + 5.3f, hy + 3.6f, 2.4f, 5.4f)), S (1.2f));
        g.setFont (hintF);
        g.setColour (Colour (0xff9aa4b4));
        g.drawText (hintT, S (Rectangle<float> (hx + 19.0f, hy, 220.0f, 19.0f)), Justification::centredLeft);
    }
    int chipCount (int c)
    {
        int n = 0;
        for (auto& f : eng.installedList())
        {
            const auto type = FxCat::typeOf (f.name).toUpperCase();
            if (type.endsWith ("I")) continue;
            switch (c)
            {
                case 1: if (! favs.contains (f.name)) continue; break;
                case 2: if (! FxCat::isOwnVendor (FxCat::vendor (f.name))) continue; break;
                case 3: if (type != "VST3") continue; break;
                case 4: if (type != "VST" && type != "VST2") continue; break;
                case 5: if (type != "CLAP") continue; break;
                case 6: if (type != "JS") continue; break;
                case 7: if (type != "AU") continue; break;
                default: break;
            }
            ++n;
        }
        return n;
    }
    float chipWidth (int c) const
    {
        const int cnt = const_cast<PedalView*> (this)->chipCount (c);
        const float tw = unitsOf (font (tfMedium, 15.5f), kChips[c]);
        return tw + (cnt > 0 ? 6.0f + unitsOf (font (tfMedium, 12.0f), String (cnt)) : 0.0f);   // onglet : texte + nombre
    }

    // indices de défilement d'une liste à 2 colonnes : fondu, pastille « ▼ MORE », barre à droite, « 1 - 24 of 27 »
    Rectangle<float> morePill;
    // units : colonnes (ou lignes) en tout, visible : combien on en voit, scroll : la première visible
    void drawListCues (Graphics& g, int units, int visible, float scroll, int shownFrom, int shownTo, int total, const String& noun)
    {
        morePill = {};
        const float top = itemRect (0).getY(), bottom = itemRect (kListRows - 1).getBottom();
        if (units > visible)
        {
            const float tx = kSW - 9.0f, th = bottom - top;
            g.setColour (Colour (0xff161a21));
            g.fillRoundedRectangle (S (Rectangle<float> (tx, top, 4.0f, th)), S (2.0f));
            const float thumb = jmax (24.0f, th * (float) visible / (float) units), y = top + (th - thumb) * jlimit (0.0f, 1.0f, scroll / (float) jmax (1, units - visible));
            g.setColour (Colour (0xff4a5363));
            g.fillRoundedRectangle (S (Rectangle<float> (tx, y, 4.0f, thumb)), S (2.0f));
        }
        const float cueY = jmax (598.0f, bottom + 6.0f);
        if ((float) units > scroll + (float) visible + 0.01f)          // encore des lignes plus bas : flèche bleue seule (cliquable)
        {
            morePill = Rectangle<float> (60.0f, 20.0f).withCentre ({ kSW * 0.5f, cueY + 10.0f });
            const auto c = morePill.getCentre();
            Path tri;
            tri.addTriangle (S (c + Point<float> (-6.0f, -3.5f)), S (c + Point<float> (6.0f, -3.5f)), S (c + Point<float> (0.0f, 4.0f)));
            g.setColour (hoverMore ? kAccent.brighter (0.3f) : kAccent);
            g.fillPath (tri);
        }
        if (total > 0 && units > visible)
        {
            g.setFont (font (tfRegular, 14.0f));
            g.setColour (kDim);
            g.drawText (String (shownFrom) + " - " + String (shownTo) + " of " + String (total) + " " + noun, S (Rectangle<float> (16.0f, cueY, 1032.0f, 20.0f)), Justification::centredRight);
        }
    }
    bool hoverMore = false;

    void drawBrowserList (Graphics& g)
    {
        const auto items = filtered();
        morePill = {};
        if (items.isEmpty())
        {
            g.setFont (font (tfMedium, 18.0f));
            g.setColour (kDim);
            g.drawText (eng.installedList().isEmpty() ? "REAPER did not give its plug-in list (REAPER 7 needed)"
                        : chip == kChipFavourites && favs.isEmpty() ? CharPointer_UTF8 ("No favorite yet : click the \xe2\x98\x86 at the end of a plug-in line")
                        : CharPointer_UTF8 ("No plug-in found"),
                        S (Rectangle<float> (16.0f, 230.0f, 1032.0f, 40.0f)), Justification::centred);
            return;
        }
        const auto lines = browserLines (items);
        const float pitch = itemRect (1).getY() - itemRect (0).getY();
        const float top = itemRect (0).getY(), bottom = itemRect (kListRows - 1).getBottom();
        const int base = (int) std::floor (listPos);
        const float frac = listPos - (float) base;
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (S (Rectangle<float> (0.0f, top - 2.0f, kSW, bottom - top + 4.0f)).toNearestInt());
            for (int k = 0; k <= kListRows; ++k)
            {
                const int l = base + k;
                if (! isPositiveAndBelow (l, (int) lines.size())) continue;
                const auto& ln = lines[(size_t) l];
                const float dy = ((float) k - frac) * pitch - (itemRect (jmin (k, kListRows - 1)).getY() - top);
                if (ln.header())
                {
                    // titre de type sur toute la largeur : nom coloré, nombre, filet
                    const auto ir = itemRect (jmin (k, kListRows - 1)).translated (0.0f, dy).withRight (itemRect (kListRows).getRight());
                    const auto col = FxCat::colour (ln.kind);
                    const auto f = font (tfBold, 13.5f).withExtraKerningFactor (0.14f);
                    const String t = String (FxCat::label (ln.kind)).toUpperCase();
                    g.setFont (f);
                    g.setColour (col);
                    const float tw = unitsOf (f, t);
                    g.drawText (t, S (Rectangle<float> (ir.getX() + 2.0f, ir.getY() + 4.0f, tw + 6.0f, ir.getHeight() - 4.0f)), Justification::centredLeft, false);
                    g.setColour (kDim.withAlpha (0.7f));
                    g.drawText (String (ln.count), S (Rectangle<float> (ir.getX() + tw + 12.0f, ir.getY() + 4.0f, 30.0f, ir.getHeight() - 4.0f)), Justification::centredLeft, false);
                    g.setColour (Colour (0xff232933));
                    g.fillRect (S (Rectangle<float> (ir.getX() + tw + 40.0f, ir.getCentreY() + 2.0f, ir.getRight() - (ir.getX() + tw + 40.0f), 1.0f)));
                    continue;
                }
                for (int c = 0; c < 2; ++c)
                {
                    const int i = c == 0 ? ln.a : ln.b;
                    if (i < 0) continue;
                    const int v = k < kListRows && frac < 0.01f ? k + c * kListRows : -1;      // case visible (survol), hors glissement
                    drawBrowserItem (g, items.getReference (i), itemRect (jmin (k, kListRows - 1) + c * kListRows).translated (0.0f, dy), v);
                }
            }
        }
        // compteur « a - b of N » et flèche s'il reste des lignes plus bas
        int before = 0, shown = 0;
        for (int l = 0; l < (int) lines.size(); ++l)
        {
            const int n = (lines[(size_t) l].a >= 0 ? 1 : 0) + (lines[(size_t) l].b >= 0 ? 1 : 0);
            if (l < listScroll) before += n; else if (l < listScroll + kListRows) shown += n;
        }
        drawListCues (g, (int) lines.size(), kListRows, listPos, before + 1, before + shown, items.size(), "plug-ins");
    }

    void drawBrowserItem (Graphics& g, const InstalledFx& f, Rectangle<float> unitRect, int v)
    {
        const auto ir = S (unitRect);
        const bool hov = v >= 0 && hoverItem == v;
        const int order = pickedIndex (f);
        g.setColour (order >= 0 ? kAccent.withAlpha (0.20f) : (hov ? kScrPanel2.brighter (0.12f) : kScrPanel));
        g.fillRoundedRectangle (ir, S (6.0f));
        if (order >= 0)
        {
            g.setColour (kAccent);
            g.drawRoundedRectangle (ir.reduced (S (0.6f)), S (6.0f), S (1.4f));
            const auto badge = Rectangle<float> (S (24.0f), S (24.0f)).withCentre ({ ir.getRight() - S (216.0f), ir.getCentreY() });
            g.fillEllipse (badge);
            g.setFont (font (tfBold, 14.0f));
            g.setColour (kScrBg);
            g.drawText (String (order + 1), badge, Justification::centred);
        }
        const int kind = FxCat::classify (f.name);
        g.setColour (FxCat::colour (kind));
        g.fillRoundedRectangle (ir.withWidth (S (5.0f)), S (2.5f));
        FxCat::drawIcon (g, kind, Rectangle<float> (S (22.0f), S (22.0f)).withCentre ({ ir.getX() + S (24.0f), ir.getCentreY() }), FxCat::colour (kind).brighter (0.3f), S (1.8f), FxCat::iconOf (f.name));
        g.setFont (font (tfMedium, 17.0f));
        g.setColour (kText);
        g.drawText (FxCat::shortName (f.name), ir.withTrimmedLeft (S (44.0f)).withTrimmedRight (S (234.0f)), Justification::centredLeft, true);
        g.setFont (font (tfRegular, 13.5f));
        g.setColour (kDim);
        const auto vend = FxCat::vendor (f.name);
        g.drawText (FxCat::typeOf (f.name) + (vend.isNotEmpty() ? "  " + vend : String()), ir.withTrimmedRight (S (40.0f)).removeFromRight (S (166.0f)),
                    Justification::centredRight, true);
        const auto star = starPath (S (starRect (unitRect).withSizeKeepingCentre (17.0f, 17.0f)));   // étoile des favoris
        if (favs.contains (f.name)) { g.setColour (Colour (0xffffd23f)); g.fillPath (star); }
        else { g.setColour (kDim.withAlpha (0.8f)); g.strokePath (star, PathStrokeType (S (1.2f), PathStrokeType::curved)); }
    }

    void addFromBrowser (const InstalledFx& f)
    {
        const int at = insertPos;
        closeOverlay();
        eng.addFx (f, at);
    }

    int pickedIndex (const InstalledFx& f) const
    {
        for (int k = 0; k < picked.size(); ++k) if (picked.getReference (k).name == f.name) return k;
        return -1;
    }
    void togglePick (const InstalledFx& f)
    {
        const int k = pickedIndex (f);
        if (k >= 0) picked.remove (k); else picked.add (f);
        askRepaint (screenPx());
    }
    // tous les effets choisis, dans l'ordre, à la suite, en une fois
    void addPicked()
    {
        if (picked.isEmpty()) return;
        const auto list = picked;
        const int at = insertPos;
        closeOverlay();
        eng.addFxList (list, at);
    }

    // paramètres du bloc choisi
    const FxInfo* paramFx() const
    {
        for (auto& f : eng.chain()) if (f.guid == paramGuid) return &f;
        return nullptr;
    }
    int paramFxIndex() const
    {
        for (int i = 0; i < eng.chain().size(); ++i) if (eng.chain().getReference (i).guid == paramGuid) return i;
        return -1;
    }
    Rectangle<float> paramCell (int k) const { return { 16.0f + 520.0f * (float) (k % 2), 134.0f + 112.0f * (float) (k / 2), 512.0f, 100.0f }; }
    Rectangle<float> paramBar (int k) const { auto c = paramCell (k); return { c.getX() + 14.0f, c.getY() + 62.0f, c.getWidth() - 28.0f, 22.0f }; }

    // lit les paramètres affichés (au plus 8) ; renvoie true si quelque chose a changé
    String paramPresetName;            // preset du plugin affiché (vide : réglages libres)
    bool pollParams (bool force)
    {
        const int i = paramFxIndex();
        auto& h = eng.chainHost();
        if (i < 0) { if (overlay == Overlay::params) closeOverlay(); return false; }
        {
            String pn = (userCur >= 0 && plistGuid == paramGuid && isPositiveAndBelow (userCur, (int) upresets.size()))
                            ? upresets[(size_t) userCur].name : h.presetName (i);
            if (pn != paramPresetName) { paramPresetName = pn; force = true; }
        }
        const auto key = paramGuid + (midiView ? "/midi" : "/main") + "/" + String (h.numParams (i));
        if (key != paramListKey)
        {
            // tout ce qui concerne le MIDI (canal, learn, CC émulés...) est rangé derrière le bouton MIDI
            paramList.clearQuick();
            const int n = h.numParams (i);
            for (int p = 0; p < n; ++p)
                if (h.paramName (i, p).containsIgnoreCase ("MIDI") == midiView) paramList.add (p);
            paramListKey = key;
        }
        paramCount = paramList.size();
        const int pages = jmax (1, (paramCount + 7) / 8);
        paramPage = jlimit (0, pages - 1, paramPage);
        params.clearQuick();
        String sig;
        for (int k = 0; k < 8; ++k)
        {
            const int idx = paramPage * 8 + k;
            if (idx >= paramCount) break;
            const int p = paramList[idx];
            ParamView pv { p, h.paramName (i, p), h.paramText (i, p), h.getParam (i, p), h.paramSteps (i, p) };
            sig << pv.name << pv.text << String (pv.value, 4) << "|";
            params.add (pv);
        }
        sig << paramCount << paramPage;
        if (! force && sig == drawnParamSig) return false;
        drawnParamSig = sig;
        return true;
    }

    void drawParams (Graphics& g)
    {
        const auto* f = paramFx();
        if (f == nullptr) return;
        const int kind = FxCat::classify (f->original);
        FxCat::drawIcon (g, kind, S (Rectangle<float> (16.0f, 86.0f, 32.0f, 32.0f)), FxCat::colour (kind).brighter (0.3f), S (2.0f), FxCat::iconOf (f->original));
        g.setFont (font (tfBold, 21.0f));
        g.setColour (kText);
        const auto title = FxCat::shortName (f->name);
        const auto titleR = Rectangle<float> (58.0f, 84.0f, 336.0f, 36.0f);
        g.drawText (title, S (titleR), Justification::centredLeft, true);
        if (paramPresetName.isNotEmpty())          // preset choisi : son nom à côté du nom du plugin
        {
            const float tw = jmin (titleR.getWidth() * 0.6f, GlyphArrangement::getStringWidth (font (tfBold, 21.0f), title) / (L::kG * s));
            const auto pr = titleR.withTrimmedLeft (tw + 12.0f);
            g.setColour (kFaint);
            g.fillRect (S (Rectangle<float> (pr.getX() - 6.5f, pr.getCentreY() - 9.0f, 1.0f, 18.0f)));
            g.setFont (font (tfMedium, 17.0f));
            g.setColour (FxCat::colour (kind).brighter (0.45f));
            g.drawText (paramPresetName, S (pr), Justification::centredLeft, true);
        }
        if (presetMode) { drawPresets (g, kind); return; }
        const int pages = jmax (1, (paramCount + 7) / 8);
        g.setFont (font (tfBold, 15.0f));
        g.setColour (kDim);
        g.drawText (String (paramPage + 1) + " / " + String (pages), S (Rectangle<float> (796.0f, 84.0f, 74.0f, 36.0f)), Justification::centred);
        if (params.isEmpty())
        {
            g.setFont (font (tfMedium, 18.0f));
            g.drawText (midiView ? "No MIDI parameter" : "This plug-in has no parameter", S (Rectangle<float> (16.0f, 230.0f, 1032.0f, 40.0f)), Justification::centred);
            return;
        }
        const auto col = FxCat::colour (kind);
        for (int k = 0; k < params.size(); ++k)
        {
            const auto& pv = params.getReference (k);
            const auto cell = S (paramCell (k));
            g.setColour (hoverParam == k || paramDrag == k ? kScrPanel2.brighter (0.08f) : kScrPanel);
            g.fillRoundedRectangle (cell, S (8.0f));
            // nom du paramètre : grand et lisible ; valeur telle que le plugin l'affiche
            g.setFont (font (tfMedium, 23.0f));
            g.setColour (kText);
            g.drawText (pv.name, cell.reduced (S (14.0f), 0.0f).withY (cell.getY() + S (8.0f)).withHeight (S (38.0f)).withTrimmedRight (S (170.0f)), Justification::centredLeft, true);
            g.setFont (font (tfBold, 21.0f));
            g.setColour (col.brighter (0.4f));
            g.drawText (pv.text, cell.reduced (S (14.0f), 0.0f).withY (cell.getY() + S (8.0f)).withHeight (S (38.0f)).removeFromRight (S (200.0f)), Justification::centredRight, true);
            const auto bar = S (paramBar (k));
            const float v = (float) jlimit (0.0, 1.0, pv.value);
            g.setColour (kScrBg);
            g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
            if (pv.steps >= 2)
            {
                // paramètre à crans (modes, interrupteurs) : une case par position, la position choisie allumée
                const int n = pv.steps;
                const int cur = roundToInt (v * (float) (n - 1));
                const float gap = n <= 24 ? S (4.0f) : 0.0f;
                const float w = (bar.getWidth() - gap * (float) (n - 1)) / (float) n;
                for (int st = 0; st < n; ++st)
                {
                    const auto seg = Rectangle<float> (bar.getX() + (w + gap) * (float) st, bar.getY(), w, bar.getHeight());
                    g.setColour (st == cur ? col : (st < cur && n > 4 ? col.withAlpha (0.35f) : kScrPanel2.brighter (0.15f)));
                    g.fillRoundedRectangle (seg, jmin (seg.getHeight(), seg.getWidth()) * 0.35f);
                }
            }
            else
            {
                g.setColour (col.withAlpha (0.85f));
                g.fillRoundedRectangle (bar.withWidth (jmax (bar.getHeight(), bar.getWidth() * v)), bar.getHeight() * 0.5f);
            }
        }
    }

    // ---- presets du plugin (usine / utilisateur, ceux de la liste de REAPER)
    void openPresets (bool reprobe)
    {
        const int i = paramFxIndex();
        if (i < 0) return;
        presetMode = true;
        if (reprobe || plistGuid != paramGuid)
        {
            MouseCursor::showWaitCursor();
            plist = eng.chainHost().presets (i, true);
            MouseCursor::hideWaitCursor();
            if (plistGuid != paramGuid) userCur = -1;
            plistGuid = paramGuid;
            presetScroll = 0;
        }
        else plist.current = eng.chainHost().presets (i, false).current;
        upPlugin = eng.chain().getReference (i).original;
        upresets = UserFxPresets::load (upPlugin);
        askRepaint (screenPx());
    }

    void loadPluginPreset (int k)
    {
        const int i = paramFxIndex();
        if (i < 0 || ! isPositiveAndBelow (k, plist.names.size())) return;
        auto& h = eng.chainHost();
        h.beginUndo();
        const bool ok = h.loadPreset (i, plist.byName ? -1 : k, plist.names[k]);
        h.endUndo ("preset " + plist.names[k]);
        if (ok) { userCur = -1; plist.current = plist.byName ? k : h.presets (i, false).current; eng.say (FxCat::shortName (eng.chain().getReference (i).name) + " : " + plist.names[k]); }
        else eng.say ("REAPER could not load " + plist.names[k]);
        pollParams (true);
        askRepaint (screenPx());
    }

    int presetTotal() const { return (int) upresets.size() + plist.names.size(); }
    void clickPresetItem (int k)          // k : rang dans la liste affichée (presets utilisateur d'Octopus d'abord)
    {
        const int nu = (int) upresets.size();
        if (k < nu) loadUserPreset (k);
        else loadPluginPreset (k - nu);
    }

    void loadUserPreset (int u)
    {
        const int i = paramFxIndex();
        if (i < 0 || ! isPositiveAndBelow (u, (int) upresets.size())) return;
        auto& h = eng.chainHost();
        const auto& p = upresets[(size_t) u];
        h.beginUndo();
        const bool ok = h.setFxState (i, p.key, p.data);
        h.endUndo ("preset " + p.name);
        if (ok) { userCur = u; plist.current = -1; eng.say (FxCat::shortName (eng.chain().getReference (i).name) + " : " + p.name); }
        else eng.say ("Could not load " + p.name);
        pollParams (true);
        askRepaint (screenPx());
    }

    void startPresetSave (int renameIndex = -1)
    {
        const int i = paramFxIndex();
        if (i < 0) return;
        if (presetEdIndex >= -1 && renameIndex == -1) { commitPresetName(); return; }   // le bouton devient OK pendant la saisie
        presetEdIndex = renameIndex;
        String n;
        if (renameIndex >= 0) n = upresets[(size_t) renameIndex].name;
        else
        {
            n = userCur >= 0 ? upresets[(size_t) userCur].name : eng.chainHost().presetName (i);
            if (n.isEmpty()) n = "My preset " + String ((int) upresets.size() + 1);
        }
        layoutEditors();
        presetNameEd.setText (n, dontSendNotification);
        presetNameEd.setVisible (true);
        presetNameEd.grabKeyboardFocus();
        presetNameEd.selectAll();
        askRepaint (screenPx());
    }

    void cancelPresetName()
    {
        presetEdIndex = -2;
        presetNameEd.setVisible (false);
        askRepaint (screenPx());
    }

    void commitPresetName()
    {
        if (presetEdIndex < -1) return;
        const int mode = presetEdIndex;
        const auto name = presetNameEd.getText().trim();
        cancelPresetName();
        if (name.isEmpty()) return;
        if (mode >= 0)                                             // renommer
        {
            const int other = UserFxPresets::indexOf (upresets, name);
            if (other >= 0 && other != mode) { eng.say ("A user preset is already named " + name); return; }
            upresets[(size_t) mode].name = name;
            UserFxPresets::save (upPlugin, upresets);
            return;
        }
        const int existing = UserFxPresets::indexOf (upresets, name);
        if (existing < 0) { storeUserPreset (-1, name); return; }
        PopupMenu m;                                               // jamais d'écrasement sans confirmation
        menuHeader (m, MenuStyle::Badge::ofUi (MenuStyle::icoSave), "\"" + name + "\" already exists", "User preset");
        m.addItem (1, "Replace it with the current settings");
        m.addItem (2, "Cancel");
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, existing, name] (int r) { if (sp != nullptr && r == 1) sp->storeUserPreset (existing, name); });
    }

    void storeUserPreset (int index, const String& name)      // index -1 : nouveau
    {
        const int i = paramFxIndex();
        if (i < 0) return;
        UserFxPreset p { name, {}, {} };
        if (! eng.chainHost().getFxState (i, p.key, p.data)) { eng.say ("REAPER did not give the settings of this plug-in"); return; }
        if (index >= 0) upresets[(size_t) index] = p;
        else
        {
            upresets.push_back (p);
            std::stable_sort (upresets.begin(), upresets.end(), [] (const UserFxPreset& a, const UserFxPreset& b) { return a.name.compareNatural (b.name) < 0; });
        }
        if (! UserFxPresets::save (upPlugin, upresets)) { eng.say ("Could not write " + UserFxPresets::fileFor (upPlugin).getFullPathName()); return; }
        userCur = UserFxPresets::indexOf (upresets, name);
        plist.current = -1;
        eng.say ("User preset saved : " + name);
        askRepaint (screenPx());
    }

    void userPresetMenu (int u)
    {
        if (! isPositiveAndBelow (u, (int) upresets.size())) return;
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofUi (MenuStyle::icoPreset), upresets[(size_t) u].name, "User preset");
        m.addItem (1, "Load");
        m.addItem (2, "Replace with the current settings");
        m.addItem (3, "Rename");
        m.addSeparator();
        m.addItem (4, "Delete");
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, u] (int r)
        {
            if (sp == nullptr || ! isPositiveAndBelow (u, (int) sp->upresets.size())) return;
            if (r == 1) sp->loadUserPreset (u);
            else if (r == 2) sp->storeUserPreset (u, sp->upresets[(size_t) u].name);
            else if (r == 3) sp->startPresetSave (u);
            else if (r == 4)
            {
                sp->eng.say ("User preset deleted : " + sp->upresets[(size_t) u].name);
                sp->upresets.erase (sp->upresets.begin() + u);
                if (sp->userCur == u) sp->userCur = -1; else if (sp->userCur > u) --sp->userCur;
                UserFxPresets::save (sp->upPlugin, sp->upresets);
                sp->askRepaint (sp->screenPx());
            }
        });
    }

    void stepPluginPreset (int dir)
    {
        const int i = paramFxIndex();
        if (i < 0) return;
        auto& h = eng.chainHost();
        h.beginUndo();
        h.stepPreset (i, dir);
        h.endUndo ("preset");
        plist.current = h.presets (i, false).current;
        userCur = -1;
        eng.say (FxCat::shortName (eng.chain().getReference (i).name) + " : " + h.presetName (i));
        pollParams (true);
        askRepaint (screenPx());
    }

    void drawPresets (Graphics& g, int kind)
    {
        const auto col = FxCat::colour (kind);
        const int i = paramFxIndex();
        g.setFont (font (tfBold, 15.0f));
        g.setColour (kDim);
        g.drawText (plist.count > 0 ? String (plist.current >= 0 ? plist.current + 1 : 0) + " / " + String (plist.count) : String ("-"),
                    S (Rectangle<float> (796.0f, 84.0f, 74.0f, 36.0f)), Justification::centred);
        // preset actif
        g.setFont (font (tfRegular, 15.0f));
        g.setColour (kDim);
        g.drawText ("CURRENT", S (Rectangle<float> (16.0f, 132.0f, 90.0f, 36.0f)), Justification::centredLeft);
        g.setFont (font (tfMedium, 21.0f));
        g.setColour (col.brighter (0.4f));
        auto curName = i >= 0 ? eng.chainHost().presetName (i) : String();
        if (userCur >= 0 && isPositiveAndBelow (userCur, (int) upresets.size())) curName = upresets[(size_t) userCur].name + "  (user)";
        if (presetEdIndex < -1)
            g.drawText (curName.isNotEmpty() ? curName : String ("(modified / no preset)"), S (Rectangle<float> (104.0f, 132.0f, 540.0f, 36.0f)), Justification::centredLeft, true);
        const int nu = (int) upresets.size();
        if (presetTotal() == 0)
        {
            g.setFont (font (tfMedium, 18.0f));
            g.setColour (kDim);
            g.drawText (plist.count == 0 ? "This plug-in has no preset in REAPER" : "Preset names not available for this plug-in : use the arrows",
                        S (Rectangle<float> (16.0f, 260.0f, 1032.0f, 40.0f)), Justification::centred);
            return;
        }
        for (int v = 0; v < kListRows * 2; ++v)
        {
            const int k = presetScroll * kListRows + v;
            if (k >= presetTotal()) break;
            const auto ir = S (itemRect (v));
            const bool mine = k < nu;
            const int rk = k - nu;
            const bool cur = mine ? k == userCur : (! plist.byName && rk == plist.current && userCur < 0);
            g.setColour (cur ? col.withAlpha (0.22f) : (hoverItem == v ? kScrPanel2.brighter (0.12f) : kScrPanel));
            g.fillRoundedRectangle (ir, S (6.0f));
            if (cur) { g.setColour (col); g.drawRoundedRectangle (ir.reduced (S (0.6f)), S (6.0f), S (1.4f)); }
            g.setFont (font (tfRegular, 13.5f));
            g.setColour (kDim);
            g.drawText (mine ? String (CharPointer_UTF8 ("\xe2\x98\x85")) : String (rk + 1), ir.withWidth (S (40.0f)), Justification::centred);
            g.setFont (font (tfMedium, 17.0f));
            g.setColour (kText);
            g.drawText (mine ? upresets[(size_t) k].name : plist.names[rk], ir.withTrimmedLeft (S (44.0f)).withTrimmedRight (S (96.0f)), Justification::centredLeft, true);
            g.setFont (font (tfBold, 12.5f));
            const bool user = mine || plist.user[rk];
            g.setColour (user ? kAmber : kDim);
            g.drawText (user ? "USER" : "FACTORY", ir.withTrimmedRight (S (12.0f)).removeFromRight (S (84.0f)), Justification::centredRight);
        }
        if (plist.names.isEmpty() && plist.count > 0)
        {
            g.setFont (font (tfRegular, 14.0f));
            g.setColour (kDim);
            g.drawText ("REAPER presets of this plug-in : use the arrows", S (Rectangle<float> (16.0f, 598.0f, 600.0f, 20.0f)), Justification::centredLeft);
        }
        const int rows = (presetTotal() + kListRows - 1) / kListRows;
        const int shownTo = jmin (presetTotal(), (presetScroll + 2) * kListRows);
        drawListCues (g, rows, 2, (float) presetScroll, presetScroll * kListRows + 1, shownTo, presetTotal(), "presets");
    }

    void setParamFromX (int k, float x)
    {
        if (! isPositiveAndBelow (k, params.size())) return;
        const auto bar = paramBar (k);
        double v = jlimit (0.0, 1.0, (double) ((x - bar.getX()) / bar.getWidth()));
        const int i = paramFxIndex();
        if (i < 0) return;
        const auto& pv = params.getReference (k);
        if (pv.steps >= 2)
        {
            // à crans : la case sous la souris, jamais une valeur intermédiaire
            const int n = pv.steps;
            v = (double) jlimit (0, n - 1, (int) std::floor (v * n)) / (double) (n - 1);
            if (std::abs (v - pv.value) < 1.0e-6) return;
        }
        eng.chainHost().setParam (i, pv.index, v);
        if (pollParams (false)) askRepaint (screenPx());
    }

    // banks
    void drawBanks (Graphics& g)
    {
        g.drawText ("BANKS", S (Rectangle<float> (16.0f, 84.0f, 200.0f, 36.0f)), Justification::centredLeft);
        g.setFont (font (tfRegular, 16.0f));
        g.setColour (kDim);
        g.drawText ("16 banks x 6 presets  -  footswitches A-F recall the presets of the bank shown (PRESET mode)",
                    S (Rectangle<float> (130.0f, 84.0f, 860.0f, 36.0f)), Justification::centredLeft, true);
        for (int sl = 0; sl < OctopusEngine::kSlots; ++sl)
        {
            const int idx = eng.bank() * OctopusEngine::kSlots + sl;
            const auto c = S (cardRect (sl));
            const bool isCur = eng.current() == idx;
            g.setColour (isCur ? kPresetLed.withAlpha (0.12f) : kScrPanel);
            g.fillRoundedRectangle (c, S (8.0f));
            g.setColour (isCur ? kPresetLed : kFaint.withAlpha (0.7f));
            g.drawRoundedRectangle (c.reduced (S (0.6f)), S (8.0f), S (isCur ? 1.8f : 1.0f));
            const auto lab = Rectangle<float> (c.getX() + S (10.0f), c.getY() + S (7.5f), S (58.0f), S (34.0f));
            g.setColour (isCur ? kText : kScrPanel2.brighter (0.2f));
            g.fillRoundedRectangle (lab, S (6.0f));
            g.setFont (font (tfBold, 19.0f));
            g.setColour (isCur ? kScrBg : kText);
            g.drawText (OctopusEngine::label (idx), lab.translated (0.0f, S (1.0f)), Justification::centred);
            if (renameSlot != sl)
            {
                const auto& p = eng.preset (idx);
                g.setFont (font (tfMedium, 18.0f));
                g.setColour (p.saved ? kText : kDim);
                g.drawText (eng.displayName (idx), Rectangle<float> (c.getX() + S (80.0f), c.getY(), S (250.0f), c.getHeight() * 0.62f).translated (0.0f, S (4.0f)), Justification::centredLeft, true);
                g.setFont (font (tfRegular, 13.5f));
                g.setColour (kDim);
                const String info = p.saved ? String (p.entries.size()) + (p.entries.size() == 1 ? " effect" : " effects") : String ("free slot");
                g.drawText (info, Rectangle<float> (c.getX() + S (80.0f), c.getY() + c.getHeight() * 0.55f, S (250.0f), c.getHeight() * 0.38f), Justification::centredLeft, true);
            }
        }
    }

    // points sous le numéro de chaque bank : un par preset enregistré (dessinés après les cases, sinon cachés par elles)
    void drawBankDots (Graphics& g)
    {
        for (int b = 0; b < OctopusEngine::kBanks; ++b)
        {
            // autant de points que de presets enregistrés dans la bank (aucun pour une bank vide), centrés
            const auto br = bankRect (b);
            int n = 0;
            for (int sl = 0; sl < OctopusEngine::kSlots; ++sl) n += eng.preset (b * OctopusEngine::kSlots + sl).saved ? 1 : 0;
            g.setColour (eng.bank() == b ? kPresetLed.withAlpha (0.9f) : kDim.withAlpha (0.85f));
            for (int d = 0; d < n; ++d)
                g.fillEllipse (S (Rectangle<float> (5.0f, 5.0f).withCentre ({ br.getCentreX() + 7.0f * ((float) d - (float) (n - 1) * 0.5f), br.getBottom() - 10.0f })));
        }
    }

    void startRename (int slot)
    {
        renameSlot = slot;
        layoutEditors();
        renameEd.setText (eng.preset (eng.bank() * OctopusEngine::kSlots + slot).name, dontSendNotification);
        renameEd.setVisible (true);
        renameEd.grabKeyboardFocus();
        askRepaint (screenPx());
    }

    void commitRename()
    {
        if (renameSlot < 0) return;
        const int idx = eng.bank() * OctopusEngine::kSlots + renameSlot;
        renameSlot = -1;
        renameEd.setVisible (false);
        eng.rename (idx, renameEd.getText());
        askRepaint (screenPx());
    }

    void exportBanks()
    {
        chooser = std::make_unique<FileChooser> ("Export Octopus banks", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Octopus banks.octobank"), "*.octobank");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                              [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == File()) return;
            f = f.withFileExtension ("octobank");
            MemoryBlock mb;
            {
                MemoryOutputStream mo (mb, false);
                GZIPCompressorOutputStream gz (mo, 6);
                eng.banksTree().writeToStream (gz);
            }
            if (f.replaceWithData (mb.getData(), mb.getSize())) eng.say ("Banks exported : " + f.getFileName());
            else eng.say ("Could not write " + f.getFileName());
        });
    }

    void importBanks()
    {
        chooser = std::make_unique<FileChooser> ("Import Octopus banks (replaces all 64 presets)", File::getSpecialLocation (File::userDocumentsDirectory), "*.octobank");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            MemoryBlock mb;
            f.loadFileAsData (mb);
            MemoryInputStream mi (mb, false);
            GZIPDecompressorInputStream gz (mi);
            if (! eng.importBanks (ValueTree::readFromStream (gz))) eng.say ("Not an Octopus banks file");
        });
    }

    // ------------------------------------------------------------ accordeur : l'écran de CHROMA dans Octopus
    // Quand un bloc CHROMA est allumé, Octopus devient l'accordeur : même moteur (TunerEngine de CHROMA) appliqué au son
    // qui traverse Octopus, même écran (dessin repris de CHROMA, police IBM Plex Mono), réglages lus dans CHROMA
    // (mode, La de référence, accordage). À droite : MUTE / MODE (paramètres de CHROMA) et EXIT (éteint le bloc CHROMA).
    static constexpr float kTW = 828.0f, kTH = 594.5f;              // repère de l'écran de CHROMA
    bool tunerShown() const { return eng.isReady() && proc.chroma().on; }
    // écran de CHROMA un peu réduit, centré dans la partie gauche ; colonne de boutons à droite
    static constexpr float kTunerColX = 800.0f;
    float tunerScale() const { return 0.84f * kSH / kTH; }
    Rectangle<float> tunerRect() const
    {
        const float w = kTW * tunerScale(), h = kTH * tunerScale();
        return { (kTunerColX - w) * 0.5f, (kSH - h) * 0.5f, w, h };
    }
    // ---- écran de l'accordeur (07/11/2026) : ligne d'infos centrée, note, mètre fin qui s'efface aux bouts,
    // écart en cents, boutons en mots sur 4 colonnes. Repère de l'écran (1064 x 651), centre en x = kSW / 2.
    static constexpr float kTx0 = 170.4f, kTw = 723.2f;             // mètre : -50 .. +50 cents, centré sur l'écran
    // ligne du haut (v1.17.1) : MODE · ACCORDAGE · A 440 · MUTE, centrée, et une croix en haut à droite pour quitter
    std::vector<Btn> tunerButtons() const
    {
        const auto t = tunerTop();
        std::vector<Btn> v;
        v.push_back ({ t.mute, bTunerMute, "MUTE", proc.chroma().muted });
        v.push_back ({ tunerCloseRect(), bTunerExit, {} });
        return v;
    }
    static Rectangle<float> tunerCloseRect() { return { kSW - 50.0f, 37.0f, 28.0f, 28.0f }; }
    Font monoS (float units) const { return Font (FontOptions (tfMono).withHeight (units * L::kG * s)); }
    Font tunerTopFont() const { return monoS (21.0f).withExtraKerningFactor (0.22f); }
    String tunerModeText() const
    {
        static const char* names[3] = { "MONO", "POLY", "STROBE" };
        const auto& c = proc.chroma();
        const auto sn = proc.tunerSnapshot();
        const bool autoMono = c.mode == 1 && sn.monoOffTuning && ! sn.anyStringActive;
        return autoMono ? String ("POLY>MONO") : String (names[jlimit (0, 2, c.mode)]);
    }
    // ligne d'infos : zones cliquables (mode, accordage, La de référence, mute) ; dots : centres des points séparateurs
    struct TunerTop { Rectangle<float> mode, name, ref, mute; float dots[3]; };
    TunerTop tunerTop() const
    {
        const auto& c = proc.chroma();
        const auto f = tunerTopFont();
        const float trail = 21.0f * 0.22f * 0.6f;                         // espace d'approche ajouté après la dernière lettre
        auto vis = [&] (const String& t) { return unitsOf (f, t) - trail; };
        const float wm = vis ("STROBE"), wn = vis (c.tuningName.toUpperCase()), wr = vis ("A " + String (c.ref)), wu = vis ("MUTE");
        const float gap = 60.0f, y = 38.0f, h = 26.0f;                    // point au milieu de chaque écart
        float x = kSW * 0.5f - (wm + wn + wr + wu + 3.0f * gap) * 0.5f;
        TunerTop t;
        t.mode = { x, y, wm, h };            x += wm + gap; t.dots[0] = x - gap * 0.5f;
        t.name = { x, y, wn, h };            x += wn + gap; t.dots[1] = x - gap * 0.5f;
        t.ref  = { x, y, wr, h };            x += wr + gap; t.dots[2] = x - gap * 0.5f;
        t.mute = { x, y, wu, h };
        return t;
    }
    Rectangle<float> tuningBox() const { return { 414.0f - 165.0f, 16.0f, 330.0f, 66.0f }; }   // repère de CHROMA

    void referenceMenu()
    {
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofFx (FxCat::tuner, FxCat::iconFamily), "Reference A", "CHROMA");
        for (int hz = 430; hz <= 450; ++hz) m.addItem (hz, "A = " + String (hz) + " Hz", true, proc.chroma().ref == hz);
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp] (int r) { if (sp != nullptr && r >= 430) sp->proc.chromaSetReference (r); });
    }

    // presets d'accordage : ceux de CHROMA (favoris, usine par catégorie, perso)
    void tuningMenu()
    {
        auto& lib = proc.tuningLibrary();
        lib.reload();
        const auto cur = proc.chroma().tuningKey;
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofFx (FxCat::tuner, FxCat::iconFamily), "Tunings", "CHROMA");
        m.addItem (1, proc.chroma().internal ? "Default tuning (E standard)" : "Use the tuning set in CHROMA", proc.tuningOverridden(), ! proc.tuningOverridden());
        m.addSeparator();
        // chaque sous-menu : nom de l'accordage | notes, les notes alignées en colonne sur la première
        const auto fonts = menuFonts();
        auto fill = [&] (PopupMenu& sub, const std::vector<int>& idx)
        {
            float col = 0.0f;
            for (int i : idx) col = jmax (col, MenuStyle::TwoColItem::leftWidth (fonts, String::fromUTF8 (tuna::displayName (lib.get (i), false).c_str())));
            col += 26.0f;
            for (int i : idx)
            {
                const auto& t = lib.get (i);
                const auto nm = String::fromUTF8 (tuna::displayName (t, false).c_str());
                sub.addCustomItem (1000 + i, std::make_unique<MenuStyle::TwoColItem> (fonts, nm, String (tuna::notesToString (t.notes)), col, lib.keyAt (i) == cur), nullptr, nm);
            }
        };
        const auto fav = lib.favouriteIndices();
        if (! fav.empty())
        {
            PopupMenu f;
            fill (f, std::vector<int> (fav.begin(), fav.end()));
            m.addSubMenu (CharPointer_UTF8 ("\xe2\x98\x85 Favorites"), f);
        }
        StringArray cats;
        std::vector<std::vector<int>> byCat;
        for (int i = 0; i < lib.size(); ++i)
        {
            const auto cat = String::fromUTF8 (tuna::displayCategory (lib.get (i).category, false).c_str());
            int k = cats.indexOf (cat);
            if (k < 0) { cats.add (cat); byCat.emplace_back(); k = cats.size() - 1; }
            byCat[(size_t) k].push_back (i);
        }
        std::vector<PopupMenu> menus (byCat.size());
        for (size_t k = 0; k < byCat.size(); ++k) fill (menus[k], byCat[k]);
        for (int k = 0; k < cats.size(); ++k) m.addSubMenu (cats[k], menus[(size_t) k], true, nullptr, false);
        SafePointer<PedalView> sp (this);
        if (captureOnly) { captured = m; return; }
        styled (m).showMenuAsync (PopupMenu::Options(), [sp] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            sp->proc.chromaSelectTuning (r == 1 ? -1 : r - 1000);
            sp->eng.say ("Tuning : " + sp->proc.chroma().tuningName);
        });
    }
    Font mono (float h) const { return Font (FontOptions (tfMono).withHeight (h)); }

    struct TC   // couleurs de l'écran de CHROMA
    {
        static Colour orange()   { return Colour (0xffff7a3a); }
        static Colour amber()    { return Colour (0xffffb23a); }
        static Colour amberDim() { return Colour (0xff3a2808); }
        static Colour green()    { return Colour (0xff5dff8a); }
        static Colour greenDim() { return Colour (0xff12351f); }
        static Colour grey()     { return Colour (0xffc9ced3); }
    };

    static String sign1 (float v)
    {
        const float r = std::round (v * 10.0f) / 10.0f;
        return (r > 0 ? "+" : (r < 0 ? "-" : "")) + String (std::abs (r), 1);
    }

    static void segment (Graphics& g, Rectangle<float> r, Colour c, bool lit)
    {
        if (lit)
        {
            g.setColour (c.withAlpha (0.10f)); g.fillRoundedRectangle (r.expanded (7, 6), 6.0f);
            g.setColour (c.withAlpha (0.22f)); g.fillRoundedRectangle (r.expanded (3.5f, 3.0f), 4.0f);
            g.setColour (c); g.fillRoundedRectangle (r, 2.5f);
            g.setColour (Colours::white.withAlpha (0.35f)); g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.3f, r.getHeight() * 0.2f), 1.5f);
        }
        else { g.setColour (c); g.fillRoundedRectangle (r, 2.5f); }
    }

    void drawTuner (Graphics& g)
    {
        const auto& c = proc.chroma();
        const auto sn = proc.tunerSnapshot();
        g.setColour (Colour (0xff08090c));
        g.fillRect (S (Rectangle<float> (0.0f, 0.0f, kSW, kSH)));
        const bool autoMono = c.mode == 1 && sn.monoOffTuning && ! sn.anyStringActive;
        // ligne d'infos : MODE · ACCORDAGE · A 440 · MUTE, centrée (mode : tambour ; accordage, La : menus ; MUTE : coupe le son)
        {
            const auto t = tunerTop();
            const auto f = tunerTopFont();
            const bool green = c.mode == 1 && ! autoMono && sn.allInTune;
            const auto grey = Colour (0xff9aa3b2);
            g.setFont (f);
            g.setColour (grey);
            for (float d : t.dots) g.drawText (String (CharPointer_UTF8 ("\xc2\xb7")), S (Rectangle<float> (d - 10.0f, t.name.getY(), 20.0f, t.name.getHeight())), Justification::centred);
            const bool hm = hoverTunerTop == 1 || modeOpen, hr = hoverTunerTop == 3;
            g.setColour (hm ? Colours::white : grey);
            g.drawText (tunerModeText(), S (t.mode.withWidth (t.mode.getWidth() + 12.0f).withX (t.mode.getCentreX() - (t.mode.getWidth() + 12.0f) * 0.5f)), Justification::centred);
            g.setColour (hr ? Colours::white : grey);
            g.drawText ("A " + String (c.ref), S (t.ref.expanded (6.0f, 0.0f)), Justification::centred);
            const bool hu = hoverBtn == bTunerMute;
            g.setColour (c.muted ? Colour (0xffff4d4d) : (hu ? Colours::white : grey));
            g.drawText ("MUTE", S (t.mute.expanded (6.0f, 0.0f)), Justification::centred);
            g.setColour (green ? TC::green() : (hoverTunerTop == 2 ? TC::amber().brighter (0.3f) : TC::amber()));
            g.drawText (c.tuningName.toUpperCase(), S (t.name.expanded (6.0f, 0.0f)), Justification::centred);
            // croix : quitter l'accordeur
            const auto xr = tunerCloseRect().reduced (8.0f);
            Path x;
            x.startNewSubPath (S (xr.getTopLeft())); x.lineTo (S (xr.getBottomRight()));
            x.startNewSubPath (S (xr.getTopRight())); x.lineTo (S (xr.getBottomLeft()));
            g.setColour (hoverBtn == bTunerExit ? Colours::white : grey);
            g.strokePath (x, PathStrokeType (S (2.0f), PathStrokeType::curved, PathStrokeType::rounded));
        }
        if (c.mode == 0 || autoMono) tunerMono (g, sn);
        else if (c.mode == 1)        tunerPoly (g, sn);
        else
        {
            Graphics::ScopedSaveState ss (g);       // stroboscope : dessin d'origine, centré sous la ligne d'infos
            const float sc = 0.8f * kSH / kTH, w = kTW * sc;
            const auto o = S (Point<float> ((kSW - w) * 0.5f, 18.0f));
            g.addTransform (AffineTransform::scale (sc * L::kG * s).translated (o.x, o.y));
            tunerStrobe (g, sn);
        }
        if (modeOpen) drawModeDrum (g);
    }

    // ---- tambour du mode de CHROMA (MONO / POLY / STROBE), comme celui de Semitones, sous le mode
    bool modeOpen = false; int modeTarget = 0, hoverMode = -1, hoverTunerTop = 0; float modePos = 0.0f;
    static constexpr float kModeTopBottom = 38.0f + 26.0f;                       // bas de la ligne MODE · ACCORDAGE (tunerTop)
    static constexpr float kModeMidY = (kModeTopBottom + kDrumGap + kDrumBandH * 0.5f), kModeR = kDrumR, kModeStep = kDrumStep;   // comme Semitones, même écart sous le mode
    float modeY (float d) const { return kModeMidY + kModeR * std::sin (jlimit (-5.5f, 5.5f, d) * kModeStep * MathConstants<float>::pi / 180.0f); }
    Rectangle<float> modeBand() const { const float cx = tunerTop().mode.getCentreX(); return { cx - 95.0f, kModeMidY - kDrumBandH * 0.5f, 190.0f, kDrumBandH }; }
    void openModeDrum() { modeTarget = jlimit (0, 2, proc.chroma().mode); modePos = (float) modeTarget; hoverMode = -1; modeOpen = true; askRepaint (screenPx()); }
    void closeModeDrum() { if (! modeOpen) return; modeOpen = false; hoverMode = -1; askRepaint (screenPx()); }
    void chooseMode (int m)
    {
        for (int i = 0; i < 3 && proc.chroma().mode != m; ++i) proc.chromaNextMode();
        modeTarget = m;
        askRepaint (screenPx());
    }
    int modeItemAt (Point<float> p) const
    {
        const auto band = modeBand();
        if (p.x < band.getX() || p.x > band.getRight()) return -1;
        int best = -1; float bd = 1.0e9f;                               // comme Semitones : la ligne la plus proche
        for (int i = 0; i < 3; ++i)
        {
            const float d = (float) i - modePos;
            const float h = 13.5f * std::cos (d * kModeStep * MathConstants<float>::pi / 180.0f);
            const float dy = std::abs (p.y - modeY (d));
            if (dy <= h && dy < bd) { bd = dy; best = i; }
        }
        return best;
    }
    void drawModeDrum (Graphics& g)
    {
        static const char* names[3] = { "MONO", "POLY", "STROBE" };
        const auto col = TC::amber();
        g.setColour (Colour (0xff05070b).withAlpha (0.985f));                 // fond presque opaque + halo (lisibilité)
        g.fillRect (S (Rectangle<float> (0.0f, 72.0f, kSW, kSH - 72.0f)));
        drumHalo (g, modeBand(), kModeR, col);
        const auto band = S (modeBand());
        for (int l = 3; l >= 1; --l)
        {
            g.setColour (col.withAlpha (0.05f));
            g.fillRoundedRectangle (band.expanded (S (3.0f * (float) l)), S (6.0f + 3.0f * (float) l));
        }
        g.setColour (col.withAlpha (0.10f));
        g.fillRoundedRectangle (band, S (6.0f));
        g.setColour (col.withAlpha (0.55f));
        g.drawRoundedRectangle (band, S (6.0f), S (0.9f));
        const float cx = modeBand().getCentreX();
        Graphics::ScopedSaveState clipTop (g);
        g.reduceClipRegion (S (Rectangle<float> (0.0f, kModeTopBottom + 4.0f, kSW, kSH)).toNearestInt());
        for (int i = 0; i < 3; ++i)
        {
            const float d = (float) i - modePos, ad = std::abs (d);
            const float a = d * kModeStep * MathConstants<float>::pi / 180.0f;
            float op = jmax (0.06f, 1.0f - ad * 0.24f);
            if (i == hoverMode && ad > 0.5f) op = jmin (1.0f, op + 0.25f);
            const bool sel = ad < 0.5f;
            const auto cy = S (Point<float> (cx, modeY (d)));
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::scale (1.0f - ad * 0.08f, (1.0f - ad * 0.08f) * std::cos (a), cy.x, cy.y));
            g.setFont (monoS (sel ? 19.0f : 17.5f).withExtraKerningFactor (0.22f));
            g.setColour ((sel ? col : Colour (0xffc7cdd6)).withAlpha (op));
            g.drawText (names[i], Rectangle<float> (cy.x - S (95.0f), cy.y - S (13.65f), S (190.0f), S (27.3f)), Justification::centred);
        }
        // aide : souris + texte, comme le tambour de Semitones
        const auto hintF = font (tfBold, 11.6f).withExtraKerningFactor (0.14f);
        const String hintT = CharPointer_UTF8 ("SCROLL  \xc2\xb7  CLICK TO CHOOSE");
        const float hx = cx - (13.0f + 6.0f + unitsOf (hintF, hintT)) * 0.5f, hy = kModeMidY + kModeR + 33.0f;
        const auto mr = S (Rectangle<float> (hx, hy, 13.0f, 19.0f));
        g.setColour (Colour (0xff9aa4b4));
        g.drawRoundedRectangle (mr, mr.getWidth() * 0.5f, S (1.2f));
        g.setColour (col);
        g.fillRoundedRectangle (S (Rectangle<float> (hx + 5.3f, hy + 3.6f, 2.4f, 5.4f)), S (1.2f));
        g.setFont (hintF);
        g.setColour (Colour (0xff9aa4b4));
        g.drawText (hintT, S (Rectangle<float> (hx + 19.0f, hy, 220.0f, 19.0f)), Justification::centredLeft);
    }

    // mètre fin : traits blancs (un par cent, moyens tous les 5, grands tous les 10) qui s'effacent vers les bouts,
    // barre blanche au centre, aiguille fine qui grandit en approchant du centre et en prend la taille à 0
    void fineMeter (Graphics& g, float y, float hh, bool labels, bool centre, bool needle, float cents, Colour needleCol, float alpha)
    {
        const float px1 = jmax (S (0.9f), 1.0f) / S (1.0f);          // trait d'au moins 1 pixel
        auto X = [] (float c) { return kTx0 + (c + 50.0f) * kTw / 100.0f; };
        for (int c = -50; c <= 50; ++c)
        {
            const bool maj = c % 10 == 0, mid = c % 5 == 0;
            const float fade = jmax (0.05f, 1.0f - std::pow (std::abs ((float) c) / 50.0f, 1.8f));
            const float h = hh * (maj ? 1.0f : mid ? 0.65f : 0.35f);
            g.setColour (Colours::white.withAlpha ((maj ? 0.85f : mid ? 0.6f : 0.38f) * fade));
            g.fillRect (S (Rectangle<float> (X ((float) c) - px1 * 0.5f, y + (hh - h) * 0.5f, px1, h)));
            if (labels && maj)
            {
                g.setFont (monoS (14.0f));
                g.setColour (Colour (0xffa0a8b6).withAlpha (jmax (0.15f, fade)));
                g.drawText ((c > 0 ? "+" : "") + String (c), S (Rectangle<float> (X ((float) c) - 25.0f, y + hh + 22.0f, 50.0f, 16.0f)), Justification::centred);
            }
        }
        const float ce = hh * 0.25f;                                  // la barre du centre dépasse les grands traits
        const auto bar = Rectangle<float> (X (0.0f) - 1.8f, y - ce, 3.6f, hh + 2.0f * ce);
        if (centre)
        {
            g.setColour (Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (S (bar.expanded (2.5f, 2.5f)), S (3.0f));
            g.setColour (Colours::white.withAlpha (0.95f));
            g.fillRoundedRectangle (S (bar), S (1.8f));
        }
        if (needle)
        {
            const float k = jmax (0.0f, 1.0f - std::abs (cents) / 3.0f);
            const float ext = ce * (0.375f + 0.625f * k), w = 1.8f + 1.8f * k;
            const auto r = Rectangle<float> (X (jlimit (-50.0f, 50.0f, cents)) - w * 0.5f, y - ext, w, hh + 2.0f * ext);
            g.setColour (needleCol.withAlpha (0.25f * alpha));
            g.fillRoundedRectangle (S (r.expanded (2.5f, 2.0f)), S (2.5f));
            g.setColour (needleCol.withAlpha (alpha));
            g.fillRoundedRectangle (S (r), S (w * 0.5f));
        }
    }

    void tunerPoly (Graphics& g, const tuna::Snapshot& sn)
    {
        const int n = jmax (1, sn.numStrings);
        const float pitch = jmin (68.7f, 412.0f / (float) n), hh = jmin (27.1f, pitch * 0.4f);
        // bloc centré : même écart entre la ligne du haut (MODE · ACCORDAGE) et le haut des cordes qu'entre -50 / 0 / +50 et le bas de l'écran
        const float topLine = 38.0f + 26.0f, labelsGap = 22.0f, labelsH = 18.0f;
        const float y0 = (topLine + kSH - pitch * ((float) n + 0.5f) + hh * 0.5f - labelsGap - labelsH) * 0.5f;
        for (int row = 0; row < n; ++row)
        {
            const int idx = n - 1 - row;   // corde la plus aiguë en haut
            const auto& st = sn.strings[idx];
            const float cy = y0 + pitch * ((float) row + 0.5f);
            float alpha = 1.0f;
            if (st.measured) alpha = st.active ? 1.0f : jmap (jlimit (0.4f, 6.0f, st.age), 0.4f, 6.0f, 1.0f, 0.45f);
            const bool inTune = st.measured && std::abs (st.cents) <= (float) tuna::TunerEngine::kPolyInTune;
            const auto col = st.measured ? (inTune ? TC::green() : TC::amber()).withAlpha (alpha) : Colour (0xff4a5060);
            {   // nom de la corde : police et taille de la ligne du haut (IBM Plex Mono 21), en gras (contour ajouté)
                const auto nf = tunerTopFont();
                GlyphArrangement ga;
                ga.addLineOfText (nf, String (tuna::noteName (st.midi)), S (Point<float> (72.3f, 0.0f)).x, S (Point<float> (0.0f, cy)).y + nf.getAscent() * 0.36f);
                Path np; ga.createPath (np);
                g.setColour (col);
                g.fillPath (np);
                g.strokePath (np, PathStrokeType (S (0.9f), PathStrokeType::curved, PathStrokeType::rounded));
            }
            fineMeter (g, cy - hh * 0.5f, hh, false, st.measured, st.measured, st.cents, inTune ? TC::green() : TC::amber(), alpha);
            if (st.measured)
            {
                g.setFont (monoS (18.8f));
                g.setColour (col);
                g.drawText (sign1 (st.cents), S (Rectangle<float> (900.0f, cy - 12.0f, 91.6f, 24.0f)), Justification::centredRight);   // alignés sur le point
            }
        }
        g.setFont (monoS (15.3f));
        g.setColour (Colour (0xff9aa3b2));
        const float ly = y0 + pitch * (float) n + labelsGap;
        g.drawText ("-50", S (Rectangle<float> (kTx0 - 30.0f, ly, 60.0f, 18.0f)), Justification::centred);
        g.drawText ("0", S (Rectangle<float> (kTx0 + kTw * 0.5f - 30.0f, ly, 60.0f, 18.0f)), Justification::centred);
        g.drawText ("+50", S (Rectangle<float> (kTx0 + kTw - 30.0f, ly, 60.0f, 18.0f)), Justification::centred);
    }

    void tunerMono (Graphics& g, const tuna::Snapshot& sn)
    {
        const bool has = sn.monoMidi >= 0 && (sn.monoActive || sn.monoAge < 2.0f);
        const float alpha = ! has ? 0.3f : (sn.monoActive ? 1.0f : jmap (jlimit (0.25f, 2.0f, sn.monoAge), 0.25f, 2.0f, 1.0f, 0.4f));
        const float c = has ? sn.monoCents : 0.0f;
        const bool inTune = has && sn.monoPrecise && std::abs (c) <= (float) tuna::TunerEngine::kMonoInTune;
        const auto state = inTune ? TC::green() : TC::amber();
        const auto col = state.withAlpha (alpha);
        // note : lettre en Poppins Bold, altération en exposant, octave en indice ; le groupe est centré sur l'écran
        const String pc = has ? String (tuna::pitchClassName (sn.monoMidi)) : String ("--");
        const String letter = has ? pc.substring (0, 1) : pc, accd = has ? pc.substring (1) : String();
        const String oct = has ? String (tuna::octaveOf (sn.monoMidi)) : String();
        const auto big = font (tfBold, 189.8f), small = font (tfBold, 63.3f);
        const float wL = GlyphArrangement::getStringWidth (big, letter);
        const float wS = jmax (GlyphArrangement::getStringWidth (small, accd), GlyphArrangement::getStringWidth (small, oct));
        const float baseline = S (Point<float> (0.0f, 223.8f)).y, capTop = S (Point<float> (0.0f, 129.0f)).y;
        const float x = S (Point<float> (kSW * 0.5f, 0.0f)).x - (wL + wS) * 0.5f;
        Path p;
        {
            GlyphArrangement ga;
            ga.addLineOfText (big, letter, x, baseline);
            if (oct.isNotEmpty()) ga.addLineOfText (small, oct, x + wL, baseline);
            if (accd.isNotEmpty()) ga.addLineOfText (small, accd, x + wL, capTop + small.getAscent() * 0.72f);
            ga.createPath (p);
        }
        if (has) DropShadow (state.withAlpha (0.30f * alpha), (int) S (22.0f), {}).drawForPath (g, p);
        g.setColour (col);
        g.fillPath (p);
        fineMeter (g, 318.0f, 57.9f, true, true, has, c, state, alpha);
        g.setFont (monoS (37.6f));
        g.setColour (col);
        g.drawText (has ? ((sn.monoPrecise ? String() : String ("~")) + sign1 (c) + " c") : String ("-- c"), S (Rectangle<float> (kSW * 0.5f - 200.0f, 424.0f, 400.0f, 42.0f)), Justification::centred);
        String info;
        if (has)
        {
            info = String (sn.monoHz, sn.monoHz < 100 ? 2 : 1) + " Hz";
            for (int i = 0; i < sn.numStrings; ++i)
                if (sn.strings[i].midi == sn.monoMidi) { info += String (CharPointer_UTF8 ("   \xc2\xb7   STRING ")) + String (sn.numStrings - i); break; }
        }
        g.setFont (monoS (17.6f).withExtraKerningFactor (0.13f));
        g.setColour (Colour (0xff8b93a3).withAlpha (jmax (0.4f, alpha)));
        g.drawText (info, S (Rectangle<float> (kSW * 0.5f - 300.0f, 466.0f, 600.0f, 22.0f)), Justification::centred);
    }

    void tunerStrobe (Graphics& g, const tuna::Snapshot& sn)
    {
        const bool has = sn.monoMidi >= 0 && (sn.monoActive || sn.monoAge < 2.0f);
        const float alpha = ! has ? 0.3f : (sn.monoActive ? 1.0f : 0.45f);
        const float c = has ? sn.monoCents : 0.0f;
        const bool inTune = has && sn.monoPrecise && std::abs (c) <= (float) tuna::TunerEngine::kMonoInTune;
        const auto col = (inTune ? TC::green() : TC::amber()).withAlpha (alpha);
        g.setFont (mono (120.0f));
        g.setColour (col);
        g.drawText (has ? String (tuna::noteName (sn.monoMidi)) : String ("--"), Rectangle<float> (40, 104, 420, 150), Justification::centredLeft);
        g.setFont (mono (58.0f));
        g.drawText (has ? sign1 (c) + " c" : String ("-- c"), Rectangle<float> (kTW - 400, 120, 372, 96), Justification::centredRight);
        g.setFont (mono (28.0f));
        g.setColour (TC::grey().withAlpha (0.75f * jmax (0.4f, alpha)));
        if (has) g.drawText (String (sn.monoHz, sn.monoHz < 100 ? 3 : 2) + " Hz", Rectangle<float> (kTW - 400, 206, 372, 50), Justification::centredRight);
        const Rectangle<float> area (44, 284, kTW - 88, 230);
        g.setColour (Colour (0xff0c0f0d));
        g.fillRoundedRectangle (area, 6.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (area.toNearestInt());
            // résolution doublée : bandes deux fois plus fines
            const float bandH[3] = { 86.0f, 66.0f, 50.0f }, period[3] = { 48.0f, 24.0f, 12.0f };
            float y = area.getY() + 10.0f;
            for (int b = 0; b < 3; ++b)
            {
                const float P = period[b];
                float off = std::fmod (strobePhase * period[0] * (float) (1 << b), P);
                if (off < 0) off += P;
                for (float x = area.getX() - P + off; x < area.getRight(); x += P)
                {
                    g.setColour (col.withAlpha (alpha * (0.78f - 0.14f * b)));
                    g.fillRect (Rectangle<float> (x, y, P * 0.5f, bandH[b]));
                }
                y += bandH[b] + 9.0f;
            }
            g.setColour (Colours::black.withAlpha (0.55f));
            g.fillRect (area.getCentreX() - 1.25f, area.getY(), 2.5f, area.getHeight());
            g.setColour (Colours::white.withAlpha (0.35f));
            g.fillRect (area.getCentreX() - 0.5f, area.getY(), 1.0f, area.getHeight());
        }
        g.setFont (mono (26.0f));
        g.setColour (TC::grey().withAlpha (0.7f));
        g.drawText ("< FLAT", Rectangle<float> (44, 530, 200, 40), Justification::centredLeft);
        g.drawText ("SHARP >", Rectangle<float> (kTW - 244, 530, 200, 40), Justification::centredRight);
    }

    // ------------------------------------------------------------ souris
    void updateHover (Point<float> p)
    {
        const int hb = buttonAt (p);
        int hs = -1, hi = -1, hf = -1, hp = -1;
        if (overlay == Overlay::none) { hs = eng.isReady() ? slotAt (p) : -1; hf = footerAt (p); }
        else if (overlay == Overlay::browser)
        {
            for (int v = 0; v < kListRows * 2; ++v) if (itemRect (v).contains (p)) { hi = v; break; }
            if (hi >= 0 && browserItemAt (hi) < 0) hi = -1;
            if (insertOpen) hi = -1;                              // tambour de l'emplacement ouvert : pas de survol ni de bulle
        }
        else if (overlay == Overlay::params && presetMode)
        {
            for (int v = 0; v < kListRows * 2; ++v) if (itemRect (v).contains (p)) { hi = v; break; }
            if (hi >= 0 && presetScroll * kListRows + hi >= presetTotal()) hi = -1;
        }
        else if (overlay == Overlay::params)
            for (int k = 0; k < params.size(); ++k) if (paramCell (k).contains (p)) { hp = k; break; }
        hoverPos = p;
        int hla = -1, hnk = -1;
        if (overlay == Overlay::none && eng.isReady())
        {
            hla = laneAddAt (p);
            bool m = false; const int ng = nodeAt (p, m);
            hnk = ng >= 0 ? ng * 2 + (m ? 1 : 0) : -1;
            int lg = -1, ll = -1;
            if (hnk < 0 && laneLetterAt (p, lg, ll)) hnk = 1000 + lg * 4 + ll;   // survol d'une lettre de voie
        }
        if (hla != hoverLaneAdd || hnk != hoverNodeKey) { hoverLaneAdd = hla; hoverNodeKey = hnk; askRepaint (screenPx()); }
        if (hb != hoverBtn || hs != hoverSlot || hi != hoverItem || hf != hoverFooter || hp != hoverParam)
        {
            if (hi != hoverItem) { hoverItemMs = Time::getMillisecondCounterHiRes(); tipShown = false; }   // la bulle attend 0,8 s
            hoverBtn = hb; hoverSlot = hs; hoverItem = hi; hoverFooter = hf; hoverParam = hp;
            askRepaint (screenPx());
        }
    }

    int fsAt (Point<float> unitPos) const
    {
        for (int f = 0; f < 4; ++f)
            if (unitPos.getDistanceFrom ({ L::fsX[f], L::fsY }) <= L::fsRadius) return f;
        return -1;
    }

    void mouseMove (const MouseEvent& e) override
    {
        if (semiOpen)
        {
            const int hd = drumItemAt (toLocal (e.position));
            if (hd != hoverDrum) { hoverDrum = hd; askRepaint (screenPx()); }
        }
        {
            const bool hm2 = overlay == Overlay::browser && morePill.contains (toLocal (e.position));
            if (hm2 != hoverMore) { hoverMore = hm2; askRepaint (screenPx()); }
        }
        if (insertOpen)
        {
            const int hi = insertItemAt (toLocal (e.position));
            if (hi != hoverInsert) { hoverInsert = hi; askRepaint (screenPx()); }
        }
        if (modeOpen)
        {
            const int hm = modeItemAt (toLocal (e.position));
            if (hm != hoverMode) { hoverMode = hm; askRepaint (screenPx()); }
        }
        if (tunerShown())                                    // survol du mode, de l'accordage et du La
        {
            const auto lp = toLocal (e.position);
            const auto t = tunerTop();
            const int ht = t.mode.expanded (8.0f, 6.0f).contains (lp) ? 1 : t.name.expanded (8.0f, 6.0f).contains (lp) ? 2 : t.ref.expanded (8.0f, 6.0f).contains (lp) ? 3 : 0;
            if (ht != hoverTunerTop) { hoverTunerTop = ht; askRepaint (screenPx()); }
        }
        updateHover (toLocal (e.position));
        const int hc = comboAt (e.position / s);
        if (hc != hoverCombo && hc >= 0) eng.say (eng.comboTitle (hc) + "   (click : run, right-click : set)", 3000.0);
        if (hc != hoverCombo) { hoverCombo = hc; askRepaint (r (Rectangle<float> (0.0f, L::fsY - 30.0f, (float) L::kW, 60.0f)).toNearestInt()); }
    }
    void mouseExit (const MouseEvent&) override { updateHover ({ -100.0f, -100.0f }); }

    void mouseDown (const MouseEvent& e) override
    {
        const auto unit = e.position / s;
        const auto p = toLocal (e.position);
        downSlot = -1; dragging = false; paramDrag = -1;
        if (renameSlot >= 0) commitRename();

        // doubles appuis : clic sur la sérigraphie = l'action, clic droit = réglage
        if (const int k = comboAt (unit); k >= 0)
        {
            if (e.mods.isPopupMenu()) comboMenu (k);
            else if (OctopusEngine::keyOfCombo (k) < 0) eng.toggleTuner();     // clic sur le symbole : accordeur on / off
            else eng.triggerCombo (k);
            return;
        }

        // lettres A-D au-dessus des footswitchs : clic droit = menu de leur footswitch (Color & Glow...)
        if (e.mods.isPopupMenu())
            for (int lf = 0; lf < 4; ++lf)
                if (Rectangle<float> (40.0f, 40.0f).withCentre (ledPos (lf)).contains (unit)) { footswitchMenu (lf); return; }

        // footswitchs
        const int f = fsAt (unit);
        if (f >= 0)
        {
            if (e.mods.isPopupMenu()) { footswitchMenu (f); return; }
            pressedFs = f;
            if (e.mods.isShiftDown())                      // souris : Maj + clic = double appui avec le voisin de droite (ou de gauche)
            {
                const int k = (f < 3 && eng.comboAction (f) != OctopusEngine::comboNone) ? f : f - 1;
                if (k >= 0 && eng.comboAction (k) != OctopusEngine::comboNone) eng.triggerCombo (k);
            }
            else eng.pressFs (f);                          // comme Echoflex : action dès l'appui (double appui : le premier est annulé)
            askRepaint (r (Rectangle<float> (L::fsSprite, L::fsSprite).withCentre ({ L::fsX[f], L::fsY })).toNearestInt());
            askRepaint (screenPx());
            return;
        }

        if (! L::glass.contains (unit)) { closeSemiDrum(); if (e.mods.isPopupMenu()) pedalMenu(); return; }

        // tambour de l'emplacement ouvert : clic sur une ligne = la choisir, sur la pastille = fermer, ailleurs = fermer
        if (insertOpen)
        {
            if (! insertWhereRect().contains (p))
            {
                const int it = insertItemAt (p);
                if (it >= 0 && ! e.mods.isPopupMenu()) chooseInsert (it);
            }
            closeInsertDrum();
            return;
        }
        // tambour du mode de CHROMA ouvert : clic sur un mode = le choisir, ailleurs = fermer
        if (modeOpen)
        {
            const int it = modeItemAt (p);
            if (it >= 0 && ! e.mods.isPopupMenu()) chooseMode (it);
            closeModeDrum();
            return;
        }
        // tambour de Semitones ouvert : clic sur un preset = le choisir, ailleurs = fermer
        if (semiOpen && ! tunerShown())
        {
            if (semiRect().contains (p) && e.mods.isPopupMenu()) { closeSemiDrum(); semiMenu(); return; }
            if (semiRect().contains (p)) { closeSemiDrum(); return; }
            const int it = drumItemAt (p);
            if (it >= 0 && ! e.mods.isPopupMenu()) chooseSemi (it);
            closeSemiDrum();
            return;
        }

        const int b = buttonAt (p);
        if (e.mods.isPopupMenu() && b == bChroma) { chromaMenu(); return; }
        if (e.mods.isPopupMenu() && b == bSemi) { semiMenu(); return; }
        if (b != bNone && ! e.mods.isPopupMenu()) { clickButton (b); return; }
        if (tunerShown())
        {
            if (e.mods.isPopupMenu()) { pedalMenu(); return; }
            const auto t = tunerTop();
            if (t.name.expanded (8.0f, 6.0f).contains (p)) tuningMenu();          // clic sur le nom de l'accordage
            else if (t.ref.expanded (8.0f, 6.0f).contains (p)) referenceMenu();     // clic sur « A 440 » : La de référence
            else if (t.mode.expanded (8.0f, 6.0f).contains (p)) openModeDrum();     // clic sur le mode : tambour
            return;
        }
        if (headerNameRect().contains (p) && ! e.mods.isPopupMenu() && eng.isReady()) { startHeaderRename(); return; }
        if (badgeRect().contains (p) && eng.current() >= 0) { presetColourMenu (eng.current()); return; }
        if (overlay == Overlay::none && ! e.mods.isPopupMenu() && maxFirstRow() > 0)          // flèches bleues : une rangée plus bas / plus haut
        {
            if (firstRow < maxFirstRow() && moreDownArrow().expanded (6.0f).contains (p)) { ++firstRow; askRepaint (screenPx()); return; }
            if (firstRow > 0 && moreUpArrow().expanded (6.0f).contains (p)) { --firstRow; askRepaint (screenPx()); return; }
        }
        if (overlay == Overlay::none && e.mods.isPopupMenu() && eng.isReady() && recMarkAt (p)) { recColourMenu(); return; }   // effet record : couleur & glow

        if (overlay == Overlay::browser)
        {
            // clic : choisir / retirer (numérotés dans l'ordre) ; double-clic : ajouter tout de suite ; clic droit : rien
            if (e.mods.isPopupMenu()) return;
            if (morePill.contains (p)) { listScroll = jlimit (0, browserMaxScroll(), listScroll + kListRows - 2); askRepaint (screenPx()); return; }
            for (int v = 0; v < kListRows * 2; ++v)
                if (itemRect (v).contains (p))
                {
                    const auto items = filtered();
                    const int i = browserItemAt (v);
                    if (i < 0 || i >= items.size()) return;
                    const auto& fx = items.getReference (i);
                    if (starRect (itemRect (v)).expanded (3.0f).contains (p)) { toggleFavourite (fx); return; }
                    if (e.getNumberOfClicks() >= 2) { if (pickedIndex (fx) < 0) picked.add (fx); addPicked(); }
                    else togglePick (fx);
                    return;
                }
            return;
        }
        if (overlay == Overlay::params && presetMode)
        {
            if (morePill.contains (p)) { presetScroll = jlimit (0, jmax (0, (presetTotal() + kListRows - 1) / kListRows - 2), presetScroll + 2); askRepaint (screenPx()); return; }
            for (int v = 0; v < kListRows * 2; ++v)
                if (itemRect (v).contains (p))
                {
                    const int k = presetScroll * kListRows + v;
                    if (k >= presetTotal()) return;
                    if (e.mods.isPopupMenu()) { if (k < (int) upresets.size()) userPresetMenu (k); return; }
                    clickPresetItem (k);
                    return;
                }
            return;
        }
        if (overlay == Overlay::params)
        {
            for (int k = 0; k < params.size(); ++k)
                if (paramCell (k).contains (p)) { paramDrag = k; setParamFromX (k, p.x); return; }
            return;
        }
        if (overlay == Overlay::banks)
        {
            for (int sl = 0; sl < OctopusEngine::kSlots; ++sl)
                if (cardRect (sl).contains (p) && e.getNumberOfClicks() >= 2 && eng.preset (eng.bank() * 4 + sl).saved)
                    eng.load (eng.bank() * 4 + sl);
            return;
        }

        if (! eng.isReady()) return;
        const int ft = footerAt (p);
        if (ft >= 0)
        {
            if (e.mods.isPopupMenu()) footswitchMenu (ft);
            else eng.press (ft);
            return;
        }
        // voies parallèles : mixeur ouvert, points Y / M, case « + » d'une voie vide, groupe replié
        if (mixGroup >= 0)
        {
            if (mixerRect().contains (p)) { mixerDown (p, e.getNumberOfClicks() >= 2); return; }
            mixGroup = -1; askRepaint (screenPx());
            bool m0 = false;
            if (nodeAt (p, m0) >= 0 && m0) return;              // clic sur M : le mixeur se ferme seulement
        }
        {
            int lg = -1, ll = -1;                                // lettre d'une voie : comme le footswitch A <-> B
            if (! e.mods.isPopupMenu() && laneLetterAt (p, lg, ll))
            {
                if (eng.soloLane (lg) == ll) eng.nextLane (lg); else eng.selectLane (lg, ll);
                askRepaint (screenPx());
                return;
            }
            bool isM = false;
            const int ng = nodeAt (p, isM);
            if (ng >= 0)
            {
                if (e.mods.isPopupMenu()) groupMenu (ng);
                else if (isM) { mixGroup = ng; askRepaint (screenPx()); }
                else eng.toggleFold (ng);
                return;
            }
            const int la = laneAddAt (p);
            if (la >= 0)
            {
                const auto cell = gCells[(size_t) la];
                if (e.mods.isPopupMenu()) groupMenu (cell.group);
                else openBrowser (eng.laneEndBlock (cell.group, cell.lane));
                return;
            }
        }
        const int slot = slotAt (p);
        const int n = eng.blocks().size();
        if (isPositiveAndBelow (slot, n) && eng.roleAt (eng.blocks()[slot]) == Routing::split)      // groupe replié
        {
            const int gi = eng.groupAt (eng.blocks()[slot]);
            if (e.mods.isPopupMenu()) groupMenu (gi); else eng.toggleFold (gi);
            return;
        }
        if (e.mods.isPopupMenu())
        {
            if (slot >= 0 && slot < n) blockMenu (slot);
            else gridMenu (slot >= 0 ? slot : n);
            return;
        }
        if (slot == n) { openBrowser (n); return; }
        if (slot >= 0 && slot < n) blockDown (slot, p, Time::getMillisecondCounterHiRes());
    }

public:
    // ---- clic / double-clic sur un bloc (fonctions séparées des événements souris pour être testées avec des temps donnés)
    void blockDown (int slot, Point<float> p, double now)
    {
        if (! isPositiveAndBelow (slot, eng.blocks().size())) return;
        {
            const auto guid = eng.chain().getReference (eng.blocks()[slot]).guid;
            // double-clic détecté par Octopus (pas par le système) : même bloc, moins de 650 ms, souris presque immobile
            if (guid == lastClickGuid && now - lastClickMs < 650.0 && p.getDistanceFrom (lastClickPos) < 16.0f)
            {
                pendingToggle = -1;                         // double-clic : fenêtre du plugin, jamais de bascule
                ++doubleClicks;
                if (toggledGuid == guid && now - toggledAtMs < 900.0)
                {
                    // double-clic lent : la bascule du premier clic est déjà passée -> on la défait
                    const int b = eng.blockOf (guid);
                    if (b >= 0) eng.toggle (eng.blocks()[b]);
                }
                toggledGuid = {}; lastClickGuid = {};
                downSlot = -1;
                const int b = eng.blockOf (guid);
                if (b >= 0) eng.focusWindow (eng.blocks()[b]);   // ouvre la fenêtre du plugin (ou la ramène au premier plan)
                return;
            }
            lastClickGuid = guid; lastClickMs = now; lastClickPos = p;
            downSlot = slot;
            downPos = p;
        }
    }

    void blockUp (int slot, double now)
    {
        if (isPositiveAndBelow (slot, eng.blocks().size()) && eng.chain().getReference (eng.blocks()[slot]).guid == lastClickGuid)
        {
            // clic simple : ON / OFF, appliqué 400 ms après le relâchement si aucun second clic n'est venu
            pendingToggle = eng.blocks()[slot];
            pendingToggleAt = now + 400.0;
        }
    }

    void runPendingToggle (double now)
    {
        if (pendingToggle >= 0 && now >= pendingToggleAt)
        {
            const int i = pendingToggle;
            pendingToggle = -1;
            if (isPositiveAndBelow (i, eng.chain().size()))
            {
                toggledGuid = eng.chain().getReference (i).guid;
                toggledAtMs = now;
                eng.toggle (i);
            }
        }
    }
    bool paramsOpen() const { return overlay == Overlay::params; }
    void toggleMidiView() { clickButton (bParamMidi); }
    void closeAll() { closeOverlay(); }
    Point<float> blockCentre (int slot) const { return slotCentre (slot); }

private:

    void mouseDrag (const MouseEvent& e) override
    {
        const auto p = toLocal (e.position);
        if (overlay == Overlay::params && paramDrag >= 0) { setParamFromX (paramDrag, p.x); return; }
        if (mixGroup >= 0 && mixDragLane >= 0) { mixerDrag (p); return; }
        if (downSlot < 0) return;
        if (! dragging && p.getDistanceFrom (downPos) > 7.0f) dragging = true;
        if (dragging)
        {
            dragPos = p;
            dropPos = dropAt (p);
            // défilement automatique près des bords de la grille
            if (p.y < kGridTop && firstRow > 0) --firstRow;
            else if (p.y > kFooterLine - 10.0f && firstRow < maxFirstRow()) ++firstRow;
            askRepaint (screenPx());
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (pressedFs >= 0)
        {
            const int f = pressedFs;
            pressedFs = -1;
            askRepaint (r (Rectangle<float> (L::fsSprite, L::fsSprite).withCentre ({ L::fsX[f], L::fsY })).toNearestInt());
        }
        paramDrag = -1;
        mixDragLane = -1; mixDragWhat = -1;
        if (downSlot >= 0)
        {
            const int slot = downSlot;
            downSlot = -1;
            if (dragging)
            {
                dragging = false;
                const int from = slot, to = dropPos;
                const int splitAt = dropSplitSlot, pathOf = dropPathGroup;
                dropPos = -1; dropSplitSlot = -1; dropPathGroup = -1;
                if (splitAt >= 0) eng.split (eng.blocks()[splitAt], eng.blocks()[from]);          // posé sous un bloc : split
                else if (pathOf >= 0) eng.addPath (pathOf, eng.blocks()[from]);                     // sous la dernière voie : voie de plus
                else if (to >= 0 && to != from && to != from + 1)
                    eng.moveBlock (eng.blocks()[from], to > from ? to - 1 : to);
                askRepaint (screenPx());
                return;
            }
            blockUp (slot, Time::getMillisecondCounterHiRes());
        }
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const auto p = toLocal (e.position);
        const int dir = w.deltaY < 0 ? 1 : (w.deltaY > 0 ? -1 : 0);
        if (dir == 0 || ! Rectangle<float> (0, 0, kSW, kSH).contains (p)) return;
        if (insertOpen)
        {
            insertTarget = jlimit (0, (int) insertChoices().size() - 1, insertTarget + dir);
            hoverInsert = -1;
            askRepaint (screenPx());
            return;
        }
        if (modeOpen)
        {
            modeTarget = jlimit (0, 2, modeTarget + dir);
            hoverMode = -1;
            askRepaint (screenPx());
            return;
        }
        if (semiOpen)
        {
            semiTarget = jlimit (0, semi::kCount - 1, semiTarget + dir);
            hoverDrum = -1;
            askRepaint (screenPx());
            return;
        }
        if (overlay == Overlay::browser)
        {
            listScroll = jlimit (0, browserMaxScroll(), listScroll + dir);
        }
        else if (overlay == Overlay::params && presetMode)
        {
            const int rows = (presetTotal() + kListRows - 1) / kListRows;
            presetScroll = jlimit (0, jmax (0, rows - 2), presetScroll + dir);
        }
        else if (overlay == Overlay::params)
        {
            const int pages = jmax (1, (paramCount + 7) / 8);
            paramPage = jlimit (0, pages - 1, paramPage + dir);
            pollParams (true);
        }
        else if (overlay == Overlay::none) firstRow = jlimit (0, maxFirstRow(), firstRow + dir);
        updateHover (p);
        askRepaint (screenPx());
    }

    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::escapeKey && insertOpen) { closeInsertDrum(); return true; }
        if (k == KeyPress::returnKey && insertOpen) { chooseInsert (insertTarget); closeInsertDrum(); return true; }
        if (k == KeyPress::escapeKey && modeOpen) { closeModeDrum(); return true; }
        if (k == KeyPress::returnKey && modeOpen) { chooseMode (modeTarget); closeModeDrum(); return true; }
        if (k == KeyPress::escapeKey && semiOpen) { closeSemiDrum(); return true; }
        if (k == KeyPress::returnKey && semiOpen) { chooseSemi (semiTarget); closeSemiDrum(); return true; }
        if (k == KeyPress::escapeKey && overlay != Overlay::none) { closeOverlay(); return true; }
        if (k == KeyPress::escapeKey && mixGroup >= 0) { mixGroup = -1; askRepaint (screenPx()); return true; }
        if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && overlay == Overlay::none
            && isPositiveAndBelow (hoverSlot, eng.blocks().size()))
        {
            eng.remove (eng.blocks()[hoverSlot]);
            return true;
        }
        return false;
    }

    void clickButton (int b)
    {
        switch (b)
        {
            case bBankPrev:   eng.bankStep (-1); break;
            case bBankNext:   eng.bankStep (1); break;
            case bModePreset: eng.setMode (OctopusEngine::presetMode); break;
            case bModeStomp:  eng.setMode (OctopusEngine::stompMode); break;
            case bSave:       saveMenu(); break;
            case bBanks:      if (overlay == Overlay::banks) closeOverlay(); else openBanks(); break;
            case bClose:      closeOverlay(); break;
            case bReload:     eng.installedList (true); listScroll = 0; listPos = 0.0f; break;
            case bParamOnOff: { const int i = paramFxIndex(); if (i >= 0) eng.toggle (i); break; }
            case bParamWindow:{ const int i = paramFxIndex(); if (const auto* f = paramFx()) eng.openWindow (i, ! f->open); break; }
            case bPagePrev:   if (presetMode) stepPluginPreset (-1); else { paramPage = jmax (0, paramPage - 1); pollParams (true); } break;
            case bPageNext:   if (presetMode) stepPluginPreset (1); else { ++paramPage; pollParams (true); } break;
            case bPresets:    if (presetMode) { presetMode = false; cancelPresetName(); } else openPresets (false); break;
            case bParamMidi:  presetMode = false; cancelPresetName(); midiView = ! midiView; paramPage = 0; pollParams (true); break;
            case bPresetRescan: openPresets (true); break;
            case bAddPicked:  addPicked(); break;
            case bClearPicked: picked.clear(); break;
            case bInsertWhere: if (insertOpen) closeInsertDrum(); else openInsertDrum(); break;
            case bTunerTunings: tuningMenu(); break;
            case bPresetSave: startPresetSave (-1); break;
            case bTunerMute:  proc.chromaSetMute (! proc.chroma().muted); break;
            case bTunerMode:  proc.chromaNextMode(); break;
            case bTunerExit:  proc.chromaExit(); break;
            case bChroma:     closeSemiDrum(); eng.toggleTuner(); break;      // mini CHROMA : ouvre l'accordeur
            case bSemi:       if (semiOpen) closeSemiDrum(); else openSemiDrum(); break;
            case bExport:     exportBanks(); break;
            case bImport:     importBanks(); break;
            case bPc:         eng.setProgramChange (! eng.programChange); break;
            default:
                if (b >= bChip && b < bChip + kNumChips) setChip (b - bChip);
                if (b == bSortAZ || b == bSortType) { browserByType = b == bSortType; listScroll = 0; listPos = 0.0f; OctoSettings::setGlowOf ("browserByType", browserByType ? 1 : 0); askRepaint (screenPx()); }
                else if (b == bFsColour + OctopusEngine::kFs) tunerColourMenu (S (colourBtnRect (OctopusEngine::kFs)));
                else if (b >= bFsColour && b < bFsColour + OctopusEngine::kFs) colourMenu (b - bFsColour);
                else if (b >= bBank && b < bBank + OctopusEngine::kBanks) eng.setBank (b - bBank);
                else if (b >= bCard && b < bCard + 100)
                {
                    const int sl = (b - bCard) / 10, a = (b - bCard) % 10;
                    const int idx = eng.bank() * OctopusEngine::kSlots + sl;
                    if (a == cLoad) eng.load (idx);
                    else if (a == cSave) confirmSave (idx);
                    else if (a == cRename) startRename (sl);
                    else if (a == cCopy) { eng.copy (idx); eng.say ("Copied " + OctopusEngine::label (idx)); }
                    else if (a == cPaste) eng.paste (idx);
                    else if (a == cClear) eng.clear (idx);
                }
                break;
        }
        askRepaint (screenPx());
    }

    // enregistrement : jamais d'écrasement silencieux d'un preset existant

    // nuancier (couleurs rapides + teinte / saturation / luminosité), ouvert dans la fenêtre d'Octopus, au-dessus de 'near' (unités)
    void openPicker (const String& title, Colour start, Rectangle<float> near, bool withDefault,
                     std::function<void (Colour)> apply, std::function<void()> toDefault = {},
                     int glowPc = -1, std::function<void (int)> glowApply = {})
    {
        auto* top = findParentComponentOfClass<OctopusEditor>();
        Component* parent = top != nullptr ? (Component*) top : (Component*) this;
        auto picker = std::make_unique<OctoColourPicker> (title, start, jlimit (0.85f, 1.6f, s * 1.25f), withDefault);
        SafePointer<PedalView> sp (this);
        picker->onChange = [sp, apply] (Colour c) { if (sp != nullptr) { apply (c); sp->askRepaint (sp->getLocalBounds()); } };
        picker->onDefault = [sp, toDefault] { if (sp != nullptr && toDefault) { toDefault(); sp->askRepaint (sp->getLocalBounds()); } };
        // GLOW : opacité du halo seulement (lettres et sérigraphie, ou symbole de l'accordeur)
        picker->setGlow (glowPc >= 0 ? glowPc : haloPc);
        picker->onGlow = [sp, glowApply] (int pc) { if (sp != nullptr) { if (glowApply) glowApply (pc); else sp->setHalo (pc, false); } };
        const auto area = parent->getLocalArea (this, near.toNearestInt());
        auto* layer = new OctoPickerLayer (std::move (picker), area, parent->getLocalBounds());   // se détruit en se fermant
        parent->addAndMakeVisible (layer);
        layer->toFront (true);
    }

    // couleur d'un preset (clic sur l'étiquette 1A... en haut) : étiquette, lettre, anneau et case de son footswitch
    void presetColourMenu (int idx)
    {
        openPicker ("Preset " + OctopusEngine::label (idx) + " colour", Colour (eng.presetColour (idx)),
                    S (badgeRect()), true,
                    [this, idx] (Colour c) { eng.setPresetColour (idx, c.getARGB()); },
                    [this, idx] { eng.setPresetColour (idx, 0); });
    }

    // symbole de l'accordeur (B+C) : sa couleur et l'opacité de son halo, communes à toutes les fenêtres
    juce::uint32 tunerCol = OctoSettings::tunerColour();
    int tunerHaloPc = OctoSettings::tunerHalo();
    Colour tunerColour() const { return tunerCol != 0 ? Colour (tunerCol) : TC::green(); }
    void tunerColourMenu (Rectangle<float> near)
    {
        openPicker ("Tuner symbol colour", tunerColour(), near, true,
                    [this] (Colour c) { tunerCol = c.getARGB(); OctoSettings::setTuner (tunerCol, tunerHaloPc); },
                    [this] { tunerCol = 0; OctoSettings::setTuner (0, tunerHaloPc); },
                    haloPc,                                       // GLOW : le Letter glow commun (lettres, marquages et accordeur)
                    [this] (int pc) { setHalo (pc, false); });
    }

    // couleur d'un footswitch (lettre lumineuse et case de l'écran, en mode PRESET)
    void colourMenu (int f, Rectangle<float> near = {})
    {
        openPicker ("Footswitch " + fsLetter (f) + " colour", Colour (eng.fsColour (f)),
                    near.isEmpty() ? S (colourBtnRect (f)) : near, true,
                    [this, f] (Colour c) { eng.setFsColour (f, c.getARGB()); },
                    [this, f] { eng.setFsColour (f, OctopusEngine::kDefaultFsColour); });
    }

    void confirmSave (int idx)
    {
        if (! eng.preset (idx).saved) { eng.save (idx); return; }
        PopupMenu m;
        menuHeader (m, MenuStyle::Badge::ofUi (MenuStyle::icoSave), "Replace " + OctopusEngine::label (idx) + " ?", eng.displayName (idx));
        m.addItem (1, "Yes, save the current chain in " + OctopusEngine::label (idx));
        m.addItem (2, "Cancel");
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, idx] (int r) { if (sp != nullptr && r == 1) sp->eng.save (idx); });
    }

    void saveMenu()
    {
        PopupMenu m;
        const int cur = eng.current();
        menuHeader (m, MenuStyle::Badge::ofUi (MenuStyle::icoSave), "Save the current chain", "Bank " + String (eng.bank() + 1).paddedLeft ('0', 2));
        if (cur >= 0) m.addItem (1, "Save " + OctopusEngine::label (cur) + "  " + eng.displayName (cur));
        PopupMenu to;
        for (int b = 0; b < OctopusEngine::kBanks; ++b)
        {
            PopupMenu bm;
            for (int sl = 0; sl < OctopusEngine::kSlots; ++sl)
            {
                const int idx = b * OctopusEngine::kSlots + sl;
                bm.addItem (100 + idx, OctopusEngine::label (idx) + "  " + eng.displayName (idx), true, idx == cur);
            }
            to.addSubMenu ("Bank " + String (b + 1), bm, true, nullptr, b == eng.bank());
        }
        m.addSubMenu ("Save as...", to);
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, cur] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            if (r == 1) sp->eng.save (cur);
            else if (r >= 100) sp->confirmSave (r - 100);
        });
    }

    // ------------------------------------------------------------ menus (clic droit)
    // ------------------------------------------------------------ voies parallèles : menu d'un groupe, mixeur
    void groupMenu (int g)
    {
        if (! isPositiveAndBelow (g, (int) eng.groups().size())) return;
        const auto& G = eng.groups()[(size_t) g];
        const String guid = G.guid;
        PopupMenu m, assign;
        menuHeader (m, MenuStyle::Badge::ofGroup (Colour (0xffffb547)), "Parallel paths", eng.blockName (G.split).replace (" || ", String (CharPointer_UTF8 (" \xe2\x80\x96 "))));
        m.addItem (1, "Mix the paths (level, pan, phase)...");
        m.addItem (2, G.on ? "Bypass the group (dry signal)" : "Enable the group");
        m.addItem (3, eng.isFolded (g) ? "Unfold" : "Fold into one block");
        m.addSeparator();
        m.addItem (4, "Add path " + String::charToString ((juce_wchar) ('A' + (int) G.lanes.size())), (int) G.lanes.size() < Routing::kMaxLanes);
        for (int l = 0; l < (int) G.lanes.size(); ++l)
            m.addItem (10 + l, "Remove path " + String::charToString ((juce_wchar) ('A' + l)) + (G.lanes[(size_t) l].isEmpty() ? String() : " (and its effects)"));
        m.addItem (5, "Merge the paths in series");
        m.addSeparator();
        for (int k = 0; k < OctopusEngine::kKeys; ++k)
        {
            if (k == 4) assign.addSeparator();
            assign.addItem (20 + k, "Footswitch " + fsLetter (k) + (k == 4 ? "  (A+B)" : k == 5 ? "  (C+D)" : ""), true, eng.isAssigned (k, guid));
        }
        m.addSubMenu ("Assign the group to footswitch (STOMP)", assign);
        PopupMenu swap;
        const String sk = OctopusEngine::swapKey (guid);
        for (int k = 0; k < OctopusEngine::kKeys; ++k)
        {
            if (k == 4) swap.addSeparator();
            swap.addItem (30 + k, "Footswitch " + fsLetter (k) + (k == 4 ? "  (A+B)" : k == 5 ? "  (C+D)" : ""), true, eng.isAssigned (k, sk));
        }
        m.addSubMenu ("Switch path A / B with footswitch (STOMP)", swap);
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, guid] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            auto& e = sp->eng;
            int gi = -1;
            for (int k = 0; k < (int) e.groups().size(); ++k) if (e.groups()[(size_t) k].guid == guid) gi = k;
            if (gi < 0) return;
            switch (r)
            {
                case 1: sp->mixGroup = gi; sp->askRepaint (sp->screenPx()); break;
                case 2: e.toggle (e.groups()[(size_t) gi].split); break;
                case 3: e.toggleFold (gi); break;
                case 4: e.addPath (gi); break;
                case 5: e.unsplit (gi); break;
                default:
                    if (r >= 10 && r < 10 + Routing::kMaxLanes) e.removePath (gi, r - 10);
                    else if (r >= 20 && r < 20 + OctopusEngine::kKeys) e.toggleAssign (r - 20, guid);
                    else if (r >= 30 && r < 30 + OctopusEngine::kKeys) e.toggleAssign (r - 30, OctopusEngine::swapKey (guid));
                    break;
            }
        });
    }

    // mixeur : sous le point M (ou au-dessus s'il n'y a pas la place)
    static constexpr float kMixW = 392.0f, kMixRowH = 38.0f;
    Rectangle<float> mixerRect() const
    {
        if (! isPositiveAndBelow (mixGroup, (int) eng.groups().size())) return {};
        const int n = (int) eng.groups()[(size_t) mixGroup].lanes.size();
        const float h = 46.0f + kMixRowH * (float) n + 8.0f;
        Point<float> m { kSW * 0.5f, kGridTop + 20.0f };
        float below = m.y + 40.0f;
        if (auto* gg = gridGroup (mixGroup))
        {
            if (gg->folded) { const auto* cl = cellOfSlot (eng.blocks().indexOf (eng.groups()[(size_t) mixGroup].split)); if (cl != nullptr) m = cellCentre (*cl); below = m.y + 62.0f; }
            else { m = nodeM (*gg); below = m.y + kLanePitch * (float) (gg->lanes - 1) + 62.0f; }
        }
        float x = jlimit (12.0f, kSW - kMixW - 12.0f, m.x - kMixW * 0.5f);
        float y = below;
        if (y + h > kFooterLine - 4.0f) y = jmax (kGridTop - 4.0f, m.y - 30.0f - h);
        return { x, y, kMixW, h };
    }
    Rectangle<float> mixRow (int l) const { const auto r = mixerRect(); return { r.getX(), r.getY() + 42.0f + kMixRowH * (float) l, r.getWidth(), kMixRowH }; }
    // zones d'une ligne : 0 niveau, 1 pan, 2 phase, 3 mute, 4 valeur (dB) ; 5 bouton fermer (ligne -1)
    Rectangle<float> mixZone (int l, int what) const
    {
        const auto r = mixRow (l);
        const float y = r.getCentreY();
        switch (what)
        {
            case 0: return { r.getX() + 132.0f, y - 9.0f, 104.0f, 18.0f };
            case 4: return { r.getX() + 240.0f, y - 9.0f, 50.0f, 18.0f };
            case 1: return { r.getX() + 294.0f, y - 9.0f, 42.0f, 18.0f };
            case 2: return { r.getX() + 342.0f, y - 10.0f, 20.0f, 20.0f };
            case 3: return { r.getX() + 366.0f, y - 10.0f, 20.0f, 20.0f };
            default: break;
        }
        const auto m = mixerRect();
        return { m.getRight() - 30.0f, m.getY() + 8.0f, 22.0f, 22.0f };
    }

    void drawMixer (Graphics& g)
    {
        if (! isPositiveAndBelow (mixGroup, (int) eng.groups().size())) { mixGroup = -1; return; }
        const auto& G = eng.groups()[(size_t) mixGroup];
        const auto r = S (mixerRect());
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0.0f, S (6.0f)).expanded (S (4.0f)), S (14.0f));
        g.setColour (kScrPanel);
        g.fillRoundedRectangle (r, S (10.0f));
        g.setColour (Colour (0xff2a303a));
        g.drawRoundedRectangle (r, S (10.0f), S (1.0f));
        const auto m = mixerRect();
        g.setFont (font (tfBold, 13.0f).withExtraKerningFactor (0.14f));
        g.setColour (kDim);
        g.drawText ("MERGE  -  MIX", S (Rectangle<float> (m.getX() + 16.0f, m.getY() + 10.0f, 200.0f, 20.0f)), Justification::centredLeft);
        {
            const auto cz = S (mixZone (-1, 5));
            g.setColour (kDim);
            g.drawLine (cz.getX() + cz.getWidth() * 0.28f, cz.getY() + cz.getHeight() * 0.28f, cz.getRight() - cz.getWidth() * 0.28f, cz.getBottom() - cz.getHeight() * 0.28f, S (1.6f));
            g.drawLine (cz.getRight() - cz.getWidth() * 0.28f, cz.getY() + cz.getHeight() * 0.28f, cz.getX() + cz.getWidth() * 0.28f, cz.getBottom() - cz.getHeight() * 0.28f, S (1.6f));
        }
        const auto& fx = eng.chain();
        for (int l = 0; l < (int) G.lanes.size(); ++l)
        {
            const auto row = mixRow (l);
            const bool empty = G.lanes[(size_t) l].isEmpty();
            const double lev = eng.mixValue (mixGroup, l, Routing::mixLevel), pan = eng.mixValue (mixGroup, l, Routing::mixPan);
            const bool ph = eng.mixValue (mixGroup, l, Routing::mixPhase) > 0.5, mu = eng.mixValue (mixGroup, l, Routing::mixMute) > 0.5;
            const auto act = (empty || mu) ? kDim : kAmber;
            // lettre + nom du premier bloc
            const auto pr = S (Rectangle<float> (row.getX() + 14.0f, row.getCentreY() - 8.0f, 16.0f, 16.0f));
            g.setColour (kAmber.withAlpha (0.16f)); g.fillRoundedRectangle (pr, S (3.0f));
            g.setColour (kAmber); g.setFont (font (tfBold, 12.0f));
            g.drawText (String::charToString ((juce_wchar) ('A' + l)), pr, Justification::centred);
            g.setColour (empty ? kDim : kText); g.setFont (font (tfMedium, 15.0f));
            g.drawText (empty ? String ("(empty)") : FxCat::shortName (fx.getReference (G.lanes[(size_t) l][0]).name),
                        S (Rectangle<float> (row.getX() + 36.0f, row.getY(), 94.0f, row.getHeight())), Justification::centredLeft, true);
            // niveau
            const auto lz = mixZone (l, 0);
            const float t = (float) jlimit (0.0, 1.0, (lev + 60.0) / 72.0);
            const auto track = S (Rectangle<float> (lz.getX(), lz.getCentreY() - 2.0f, lz.getWidth(), 4.0f));
            g.setColour (Colour (0xff2a303a)); g.fillRoundedRectangle (track, S (2.0f));
            g.setColour (act); g.fillRoundedRectangle (track.withWidth (track.getWidth() * t), S (2.0f));
            g.setColour (kFaint); g.fillRect (S (Rectangle<float> (lz.getX() + lz.getWidth() * (60.0f / 72.0f) - 0.5f, lz.getCentreY() - 6.0f, 1.0f, 12.0f)));   // 0 dB
            g.setColour (kText); g.fillEllipse (S (Rectangle<float> (12.0f, 12.0f).withCentre ({ lz.getX() + lz.getWidth() * t, lz.getCentreY() })));
            g.setColour (kDim); g.setFont (font (tfBold, 13.0f));
            g.drawText (lev <= -59.9 ? String ("-inf") : (lev > 0.04 ? "+" : "") + String (lev, 1) + " dB", S (mixZone (l, 4)), Justification::centredRight);
            // pan
            const auto pz = mixZone (l, 1);
            const float tp = (float) ((pan + 100.0) / 200.0);
            g.setColour (Colour (0xff2a303a)); g.fillRoundedRectangle (S (Rectangle<float> (pz.getX(), pz.getCentreY() - 2.0f, pz.getWidth(), 4.0f)), S (2.0f));
            g.setColour (kFaint); g.fillRect (S (Rectangle<float> (pz.getCentreX() - 0.5f, pz.getCentreY() - 6.0f, 1.0f, 12.0f)));
            g.setColour (act); g.fillEllipse (S (Rectangle<float> (10.0f, 10.0f).withCentre ({ pz.getX() + pz.getWidth() * tp, pz.getCentreY() })));
            g.setColour (kDim); g.setFont (font (tfBold, 10.0f));
            const int pi = roundToInt (pan);
            g.drawText (pi == 0 ? String ("C") : (pi < 0 ? "L" : "R") + String (std::abs (pi)), S (pz.translated (0.0f, 12.0f)), Justification::centred);
            // phase / mute
            auto toggleBox = [&] (Rectangle<float> z, const String& txt, bool on, Colour onCol)
            {
                const auto zz = S (z);
                g.setColour (on ? onCol.withAlpha (0.18f) : Colours::transparentBlack); g.fillRoundedRectangle (zz, S (4.0f));
                g.setColour (on ? onCol : kFaint); g.drawRoundedRectangle (zz, S (4.0f), S (1.0f));
                g.setColour (on ? onCol : kDim); g.setFont (font (tfBold, 12.0f));
                g.drawText (txt, zz, Justification::centred);
            };
            toggleBox (mixZone (l, 2), CharPointer_UTF8 ("\xc3\x98"), ph, kAmber);
            toggleBox (mixZone (l, 3), "M", mu, Colour (0xffff4d4d));
        }
    }

    void mixerSet (Point<float> p)
    {
        const int l = mixDragLane;
        if (l < 0) return;
        if (mixDragWhat == 0)
        {
            const auto z = mixZone (l, 0);
            double v = -60.0 + 72.0 * jlimit (0.0, 1.0, (double) ((p.x - z.getX()) / z.getWidth()));
            if (std::abs (v) < 0.8) v = 0.0;                               // aimanté sur 0 dB
            eng.setMix (mixGroup, l, Routing::mixLevel, v);
        }
        else if (mixDragWhat == 1)
        {
            const auto z = mixZone (l, 1);
            double v = -100.0 + 200.0 * jlimit (0.0, 1.0, (double) ((p.x - z.getX()) / z.getWidth()));
            if (std::abs (v) < 8.0) v = 0.0;                               // aimanté au centre
            eng.setMix (mixGroup, l, Routing::mixPan, v);
        }
        askRepaint (screenPx());
    }
    void mixerDown (Point<float> p, bool dbl = false)
    {
        if (mixZone (-1, 5).expanded (4.0f).contains (p)) { mixGroup = -1; askRepaint (screenPx()); return; }
        const int n = isPositiveAndBelow (mixGroup, (int) eng.groups().size()) ? (int) eng.groups()[(size_t) mixGroup].lanes.size() : 0;
        for (int l = 0; l < n; ++l)
        {
            // double clic sur le curseur : niveau à 0 dB (gain unité), pan au centre
            if (dbl && mixZone (l, 0).expanded (6.0f, 8.0f).contains (p)) { mixDragLane = -1; eng.setMix (mixGroup, l, Routing::mixLevel, 0.0); askRepaint (screenPx()); return; }
            if (dbl && mixZone (l, 1).expanded (4.0f, 8.0f).contains (p)) { mixDragLane = -1; eng.setMix (mixGroup, l, Routing::mixPan, 0.0); askRepaint (screenPx()); return; }
            if (mixZone (l, 0).expanded (6.0f, 8.0f).contains (p)) { mixDragLane = l; mixDragWhat = 0; mixerSet (p); return; }
            if (mixZone (l, 1).expanded (4.0f, 8.0f).contains (p)) { mixDragLane = l; mixDragWhat = 1; mixerSet (p); return; }
            if (mixZone (l, 4).contains (p)) { eng.setMix (mixGroup, l, Routing::mixLevel, 0.0); askRepaint (screenPx()); return; }
            for (int w : { 2, 3 })
                if (mixZone (l, w).expanded (3.0f).contains (p))
                {
                    const int what = w == 2 ? Routing::mixPhase : Routing::mixMute;
                    eng.setMix (mixGroup, l, what, eng.mixValue (mixGroup, l, what) > 0.5 ? 0.0 : 1.0);
                    askRepaint (screenPx());
                    return;
                }
        }
    }
    void mixerDrag (Point<float> p) { mixerSet (p); }

public:
    void openMixer (int g) { mixGroup = g; askRepaint (screenPx()); }
    void unfoldAllForTest() {}
private:

    void blockMenu (int slot)
    {
        const int i = eng.blocks()[slot];
        const auto f = eng.chain().getReference (i);
        PopupMenu m, assign, moveM;
        menuHeader (m, fxBadge (i), FxCat::shortName (f.name), String (FxCat::label (eng.blockKind (i))) + String (CharPointer_UTF8 (" \xc2\xb7 ")) + (f.enabled ? "ON" : "OFF"));
        m.addItem (1, f.enabled ? "Bypass (OFF)" : "Enable (ON)");
        {   // mode de bypass juste sous Bypass, puis un séparateur
            bool inv = false;
            const bool hasPedal = eng.pedalBypassParam (i, inv) >= 0;
            const bool pedal = eng.usesPedalBypass (i);
            PopupMenu bm;
            bm.addItem (30, hasPedal ? "Pedal bypass (delay / reverb trails go on)" : "Pedal bypass (not available for this plug-in)", hasPedal, pedal);
            bm.addItem (31, "REAPER bypass (plug-in stops, saves CPU)", true, ! pedal);
            m.addSubMenu ("Bypass mode", bm);
        }
        m.addSeparator();
        m.addItem (2, "Parameters on screen");
        m.addItem (3, f.open ? "Close plug-in window" : "Open plug-in window");
        m.addItem (24, "Focus plug-in window");                       // la fenêtre du VST au premier plan
        m.addSeparator();
        for (int k = 0; k < OctopusEngine::kKeys; ++k)
        {
            if (k == 4) assign.addSeparator();
            assign.addItem (10 + k, "Footswitch " + fsLetter (k) + (k == 4 ? "  (A+B)" : k == 5 ? "  (C+D)" : ""), true, eng.isAssigned (k, f.guid));
        }
        m.addSubMenu ("Assign to footswitch (STOMP)", assign);
        m.addSeparator();                                                      // séparateur au-dessus de Split here
        const int gOf = eng.groupAt (i);
        if (gOf < 0) m.addItem (4, "Split here (parallel paths)");
        else
        {
            const int ln = eng.laneAt (i);
            m.addItem (8, "Mix the paths...");
            m.addItem (9, "Remove path " + String::charToString ((juce_wchar) ('A' + jmax (0, ln))) + " (and its effects)");
        }
        moveM.addItem (20, "Move left", slot > 0);
        moveM.addItem (21, "Move right", slot < eng.blocks().size() - 1);
        moveM.addItem (22, "Move to start", slot > 0);
        moveM.addItem (23, "Move to end", slot < eng.blocks().size() - 1);
        m.addSubMenu ("Move", moveM);
        m.addItem (5, "Insert an effect before");
        m.addItem (6, "Insert an effect after");
        m.addSeparator();
        m.addItem (7, "Remove " + FxCat::shortName (f.name));
        SafePointer<PedalView> sp (this);
        const String guid = f.guid;
        if (captureOnly) { captured = m; return; }
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, slot, guid] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            auto& e = sp->eng;
            const int b = e.blockOf (guid);
            if (b < 0) return;
            const int idx = e.blocks()[b];
            const int n = e.blocks().size();
            switch (r)
            {
                case 1: e.toggle (idx); break;
                case 2: sp->openParams (idx); break;
                case 3: e.openWindow (idx, ! e.chain().getReference (idx).open); break;
                case 24: e.focusWindow (idx); break;
                case 4: e.split (idx); break;
                case 8: if (e.groupAt (idx) >= 0) { sp->mixGroup = e.groupAt (idx); sp->askRepaint (sp->screenPx()); } break;
                case 9: if (e.groupAt (idx) >= 0) e.removePath (e.groupAt (idx), jmax (0, e.laneAt (idx))); break;
                case 5: sp->openBrowser (b); break;
                case 6: sp->openBrowser (b + 1); break;
                case 7: e.remove (idx); break;
                case 20: e.moveBlock (idx, b - 1); break;
                case 21: e.moveBlock (idx, b + 1); break;
                case 22: e.moveBlock (idx, 0); break;
                case 23: e.moveBlock (idx, n - 1); break;
                case 30: e.setBypassPref (e.chain().getReference (idx).original, OctopusEngine::bypassPedal); break;
                case 31: e.setBypassPref (e.chain().getReference (idx).original, OctopusEngine::bypassReaper); break;
                default: if (r >= 10 && r < 10 + OctopusEngine::kKeys) e.toggleAssign (r - 10, guid); break;
            }
            juce::ignoreUnused (slot);
        });
    }

    void gridMenu (int blockPos)
    {
        PopupMenu m;
        m.addItem (1, "Add an effect");
        m.addItem (2, "Save the chain...");
        m.addItem (3, "Banks...");
        addSizeMenu (m);
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, blockPos] (int r)
        {
            if (sp == nullptr) return;
            if (r == 1) sp->openBrowser (blockPos);
            else if (r == 2) sp->saveMenu();
            else if (r == 3) sp->openBanks();
            else sp->handleSize (r);
        });
    }

    void addLearnItems (PopupMenu& m, int target)
    {
        const int c = eng.ccOf (target);
        m.addItem (500 + target, "MIDI Learn : " + OctopusEngine::targetName (target) + " (" + (c >= 0 ? "CC " + String (c) : String ("none")) + ")",
                   true, eng.learnTarget == target);
        m.addItem (520 + target, "Clear MIDI", c >= 0);
        m.addItem (540 + target, "MIDI footswitch latching (127 / 0 alternately)", true, eng.latching (target));
    }

    void footswitchMenu (int f)
    {
        PopupMenu m, assign;
        const int idx = eng.bank() * OctopusEngine::kSlots + f;
        // en-tête : pastille de la lettre (couleur du footswitch) + mode et bank
        menuHeader (m, MenuStyle::Badge::ofLetter (fsLetter (f), f < 4 ? Colour (eng.fsColour (f)) : MenuStyle::kAccent),
                    "Footswitch " + fsLetter (f) + (f == 4 ? "  (A+B)" : f == 5 ? "  (C+D)" : ""),
                    String (eng.mode() == OctopusEngine::stompMode ? "STOMP" : "PRESET") + String (CharPointer_UTF8 (" \xc2\xb7 bank ")) + String (eng.bank() + 1).paddedLeft ('0', 2));
        m.addItem (1, "Save the current chain in " + OctopusEngine::label (idx), eng.isReady());
        m.addItem (2, "Load " + OctopusEngine::label (idx), eng.preset (idx).saved && eng.isReady());
        m.addSeparator();
        // STOMP : grille de tuiles des blocs, on coche / décoche plusieurs blocs sans que le menu se ferme
        SafePointer<PedalView> spg (this);
        assign.addCustomItem (9002, std::make_unique<MenuStyle::BlockGrid> (eng, menuFonts(), f, [spg] { if (spg != nullptr) spg->askRepaint (spg->getLocalBounds()); }),
                              nullptr, "Blocks");
        int nAssigned = 0;
        for (auto& g : eng.stompKeys (f)) if (eng.blockOf (g) >= 0) ++nAssigned;
        m.addSubMenu ("STOMP : blocks switched by " + fsLetter (f) + (nAssigned > 0 ? "  (" + String (nAssigned) + ")" : String()), assign);
        m.addItem (3, "STOMP : clear assignment", true);
        m.addSeparator();
        if (f < 4) addLearnItems (m, f);
        else                                             // E / F : les CC de A+B / C+D reçus ensemble, rien à apprendre
        {
            const int f0 = f == 4 ? 0 : 2;
            const auto ccTxt = [] (int c) { return c >= 0 ? "CC " + String (c) : String ("not learned"); };
            m.addItem (390, "MIDI : " + fsLetter (f) + " = " + fsLetter (f0) + " + " + fsLetter (f0 + 1) + " pressed together ("
                              + ccTxt (eng.ccOf (f0)) + " + " + ccTxt (eng.ccOf (f0 + 1)) + ")", false);
        }
        midiSubmenu (m);
        m.addSeparator();
        m.addItem (395, "Color & Glow");                // couleur du footswitch et opacité du halo (pas de taille du plugin ici)
        const auto near = fsMenuNear (f);
        SafePointer<PedalView> sp (this);
        if (captureOnly) { captured = m; return; }
        styled (m).showMenuAsync (PopupMenu::Options(), [sp, f, idx, near] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            auto& e = sp->eng;
            if (r == 395) { sp->colourMenu (f, near); return; }
            if (r == 1) sp->confirmSave (idx);
            else if (r == 2) e.load (idx);
            else if (r == 3) e.clearAssign (f);
            else sp->handleMidi (r);
        });
    }
    // outil de rendu : un vrai menu clic droit (0-5 = footswitch A-F ouvert sur STOMP, 100 + n = bloc n ouvert sur « Assign »)
    bool captureOnly = false;
    PopupMenu captured;
    Image menuPreview (int which)
    {
        captureOnly = true; captured = {};
        if (which == 200) tuningMenu(); else if (which < 100) footswitchMenu (which); else blockMenu (which - 100);
        captureOnly = false;
        PopupMenu main = captured; captured = {};
        const String openPrefix = which == 200 ? "" : which < 100 ? "STOMP : blocks" : "Assign";

        struct Row { const PopupMenu::Item* it; int h; };
        const int bd = menuLnf.getPopupMenuBorderSize();
        auto layout = [&] (const PopupMenu& m, int& w, int& h)
        {
            std::vector<Row> rows; w = 0; h = 0;
            PopupMenu::MenuItemIterator iter (m);
            while (iter.next())
            {
                const auto& it = iter.getItem();
                int iw = 0, ih = 0;
                if (it.customComponent != nullptr) it.customComponent->getIdealSize (iw, ih);
                else if (it.isSectionHeader) { iw = 200; ih = 32; }
                else menuLnf.getIdealPopupMenuItemSize (it.text, it.isSeparator, 32, iw, ih);
                rows.push_back ({ &it, ih }); w = jmax (w, iw); h += ih;
            }
            w += 2 * bd; h += 2 * bd;
            return rows;
        };
        auto draw = [&] (Graphics& g, const std::vector<Row>& rows, int w, int h, int& openY) -> const PopupMenu*
        {
            menuLnf.drawPopupMenuBackground (g, w, h);
            int y = bd; const PopupMenu* open = nullptr;
            for (auto& r : rows)
            {
                const auto& it = *r.it;
                const Rectangle<int> a (bd, y, w - 2 * bd, r.h);
                if (it.customComponent != nullptr)
                {
                    it.customComponent->setSize (a.getWidth(), a.getHeight());
                    Graphics::ScopedSaveState ss (g);
                    g.addTransform (AffineTransform::translation ((float) a.getX(), (float) a.getY()));
                    it.customComponent->paintEntireComponent (g, false);
                }
                else if (it.isSectionHeader) menuLnf.drawPopupMenuSectionHeader (g, a, it.text);
                else
                {
                    const bool hi = it.subMenu != nullptr && open == nullptr && it.text.startsWith (openPrefix);
                    if (hi) { open = it.subMenu.get(); openY = y; }
                    menuLnf.drawPopupMenuItem (g, a, it.isSeparator, it.isEnabled, hi, it.isTicked, it.subMenu != nullptr, it.text, {}, nullptr, nullptr);
                }
                y += r.h;
            }
            return open;
        };
        int mw, mh; const auto rows = layout (main, mw, mh);
        // sous-menu ouvert (mesuré d'avance pour la taille de l'image)
        int sw = 0, sh = 0, openY = 0; std::vector<Row> srows; const PopupMenu* sub = nullptr;
        {
            int y = bd;
            for (auto& r : rows) { if (r.it->subMenu != nullptr && r.it->text.startsWith (openPrefix)) { sub = r.it->subMenu.get(); openY = y; break; } y += r.h; }
            if (sub != nullptr) srows = layout (*sub, sw, sh);
        }
        const float sc = 2.0f;
        const int gx = mw + 4, gy = jmax (0, openY - bd);
        const int W = mw + (sub != nullptr ? sw + 4 : 0) + 20, H = jmax (mh, gy + sh) + 20;
        Image img (Image::ARGB, roundToInt (W * sc), roundToInt (H * sc), true);
        Graphics g (img);
        g.fillAll (Colour (0xff08090c));
        g.addTransform (AffineTransform::scale (sc).translated (10.0f * sc, 10.0f * sc));
        int dummy = 0;
        draw (g, rows, mw, mh, dummy);
        if (sub != nullptr) { g.addTransform (AffineTransform::translation ((float) gx, (float) gy)); draw (g, srows, sw, sh, dummy); }
        return img;
    }
    // où ouvrir le nuancier d'un footswitch : près de son footswitch (A-D), de son marquage (E / F)
    Rectangle<float> fsMenuNear (int f) const
    {
        if (f >= 4) return r (comboRect (f == 4 ? 0 : 2));
        return r (Rectangle<float> (L::fsSprite, L::fsSprite).withCentre ({ L::fsX[f], L::fsY }));
    }

    void midiSubmenu (PopupMenu& m)
    {
        PopupMenu midi;
        for (int t = 0; t < OctopusEngine::fsE; ++t)                  // E / F : CC de A+B / C+D, pas de learn à part
        {
            const int c = eng.ccOf (t);
            midi.addItem (500 + t, "Learn : " + OctopusEngine::targetName (t) + " (" + (c >= 0 ? "CC " + String (c) : String ("none")) + ")", true, eng.learnTarget == t);
        }
        midi.addSeparator();
        if (eng.learnTarget >= 0) midi.addItem (600, "Cancel MIDI learn");
        midi.addItem (601, "Program Change 1-64 = presets 1A-16D", true, eng.programChange);
        midi.addItem (602, "Reset MIDI (all unassigned)");
        midi.addSeparator();
        midi.addItem (603, "Parameters fsA-fsD, bankDown, bankUp, mode can also use REAPER's MIDI learn", false);
        m.addSeparator();
        m.addSubMenu ("MIDI", midi);
    }

    bool handleMidi (int r)
    {
        if (r >= 500 && r < 500 + OctopusEngine::numTargets)
        {
            eng.learnTarget = eng.learnTarget == r - 500 ? -1 : r - 500;
            if (eng.learnTarget >= 0) eng.say ("MIDI learn : press the MIDI footswitch for " + OctopusEngine::targetName (eng.learnTarget), 8000.0);
            return true;
        }
        if (r >= 520 && r < 520 + OctopusEngine::numTargets) { eng.clearCc (r - 520); return true; }
        if (r >= 540 && r < 540 + OctopusEngine::numTargets) { eng.setLatching (r - 540, ! eng.latching (r - 540)); return true; }
        if (r == 600) { eng.learnTarget = -1; return true; }
        if (r == 601) { eng.setProgramChange (! eng.programChange); return true; }
        if (r == 602) { eng.resetMidi(); return true; }
        return false;
    }

    void pedalMenu()
    {
        PopupMenu m;
        midiSubmenu (m);
        addSizeMenu (m);
        SafePointer<PedalView> sp (this);
        styled (m).showMenuAsync (PopupMenu::Options(), [sp] (int r) { if (sp != nullptr && ! sp->handleMidi (r)) sp->handleSize (r); });
    }

    void addSizeMenu (PopupMenu& m)
    {
        auto* ed = findParentComponentOfClass<OctopusEditor>();
        const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
        const int def = OctoSettings::defaultSize();
        PopupMenu sub, defM;
        for (int i = 1; i < L::kNumSizes; ++i)            // 60 / 75 / 100 % (50 % retire : minimum 60 %)
        {
            sub.addItem (910 + i, String (L::kSizeNames[i]) + (i == def ? "  (default)" : ""), true, cur == i);
            defM.addItem (920 + i, L::kSizeNames[i], true, i == def);
        }
        sub.addSeparator();
        sub.addSubMenu ("Default size (new windows)", defM);
        m.addSeparator();
        m.addSubMenu ("Plugin size", sub);
        PopupMenu glowM;                                  // opacité du halo des lettres et du symbole de l'accordeur
        for (int i = 0; i < kNumHalo; ++i)
            glowM.addItem (930 + i, kHalo[i] == 0 ? String ("Off (0 %)") : String (kHalo[i]) + " %" + (kHalo[i] == 100 ? "  (default)" : ""), true, haloPc == kHalo[i]);
        m.addSubMenu ("Letter glow (halo opacity)", glowM);
    }
    static constexpr int kNumHalo = 9;
    static constexpr int kHalo[kNumHalo] = { 0, 25, 50, 75, 100, 125, 150, 175, 200 };
    int haloPc = OctoSettings::haloPercent();
    void setHalo (int pc, bool tell = true)
    {
        haloPc = jlimit (0, 200, pc);
        OctoSettings::setHaloPercent (haloPc);
        if (tell) { OctoSettings::flush(); eng.say ("Letter glow : " + String (haloPc) + " %"); }
        askRepaint (getLocalBounds());
    }

    void handleSize (int r)
    {
        auto* ed = findParentComponentOfClass<OctopusEditor>();
        if (ed == nullptr) return;
        if (r >= 930 && r < 930 + kNumHalo) { setHalo (kHalo[r - 930]); return; }
        if (r >= 910 && r < 910 + L::kNumSizes) ed->setScaleIndex (r - 910);
        else if (r >= 920 && r < 920 + L::kNumSizes)
        {
            OctoSettings::setDefaultSize (r - 920);       // taille par défaut de toutes les fenêtres d'Octopus
            ed->setScaleIndex (-1);                       // cette fenêtre suit la nouvelle taille par défaut
            eng.say ("Default size : " + String (L::kSizeNames[r - 920]));
        }
    }

    // ------------------------------------------------------------ timer (30 Hz)
    void timerCallback() override
    {
        const double now = Time::getMillisecondCounterHiRes();
        auto* ed = findParentComponentOfClass<OctopusEditor>();
        if (ed != nullptr) ed->checkRenderer();
        if (overlay == Overlay::browser && hoverItem >= 0 && ! tipShown && now - hoverItemMs >= kTipDelayMs)
        {
            tipShown = true;                                   // 0,8 s sur la même ligne : la bulle apparaît
            askRepaint (screenPx());
        }

        if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
        else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
        {
            lastScaleCheckMs = now;
            const float sc = physScale(), ap = (float) Component::getApproximateScaleFactorForComponent (this);
            if (std::abs (sc - dispScale) > 0.01f || std::abs (ap - approxScale) > 0.01f) { dispScale = sc; approxScale = ap; rebuildCaches(); }
        }

        if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
        {
            repaintAskedMs = 0.0;
            if (ed != nullptr) ed->resetRenderer();
            repaint();
        }

        // clic simple sur un bloc : bascule ON / OFF une fois le délai du double-clic passé
        runPendingToggle (now);
        tickHeaderWidgets();

        // piste armée : IN / OUT / REC clignotent (0,5 s allumés, 0,5 s éteints)
        if (recPreview < 0)
        {
            const bool on = eng.recState() != 2 || std::fmod (now, 1000.0) < 500.0;     // clignote pendant l'enregistrement
            if (on != recBlinkOn) { recBlinkOn = on; askRepaint (screenPx()); }
            if (drawnRec >= 0 && recShown() != drawnRec) askRepaint (screenPx());   // fin de l'aperçu du réglage
        }

        // accordeur : l'écran de CHROMA s'anime en continu tant que le bloc CHROMA est allumé
        if (tunerShown())
        {
            if (! drawnTuner) { drawnTuner = true; if (overlay != Overlay::none) closeOverlay(); }
            const auto sn = proc.tunerSnapshot();
            strobePhase += (1.0f / 30.0f) * jlimit (-6.0f, 6.0f, sn.monoCents * 0.25f);
            if (std::abs (strobePhase) > 1.0e4f) strobePhase = 0.0f;
            askRepaint (screenPx());
        }
        else if (drawnTuner) { drawnTuner = false; askRepaint (screenPx()); }

        // écran : redessiné seulement quand l'état affiché change
        if (eng.generation() != drawnGen)
        {
            drawnGen = eng.generation();
            if (overlay == Overlay::params) pollParams (true);
            firstRow = jlimit (0, maxFirstRow(), firstRow);
            askRepaint (screenPx());
        }
        else if (overlay == Overlay::params && ++pollTick % 3 == 0 && paramDrag < 0 && pollParams (false))
            askRepaint (screenPx());

        // LED des footswitchs : fondu (0.45 à l'allumage, 0.28 à l'extinction, comme Echoflex), couleur selon le mode
        for (int f = 0; f < kLeds; ++f)
        {
            bool lit = false;
            Colour col;
            if (f == kTunerLed) { lit = eng.isReady() && eng.tunerOn(); col = tunerColour(); }
            else
            {
                lit = eng.isReady() && eng.fsLit (f);
                col = Colour (eng.presetColour (eng.bank() * OctopusEngine::kSlots + f));
                if (eng.mode() == OctopusEngine::stompMode) { const int k = eng.fsKind (f); col = k >= 0 ? FxCat::colour (k) : col; }
            }
            if (lit || ledLevel[f] < 0.01f) ledColour[f] = col;                     // pendant l'extinction, la couleur reste
            const float target = lit ? 1.0f : 0.0f;
            const float before = ledLevel[f];
            ledLevel[f] += (target - ledLevel[f]) * (target > ledLevel[f] ? 0.45f : 0.28f);
            if (std::abs (ledLevel[f] - target) < 0.004f) ledLevel[f] = target;
            if (std::abs (ledLevel[f] - before) > 0.002f || ledColour[f] != drawnLedColour[f])
            {
                drawnLedColour[f] = ledColour[f];
                askRepaint (r (glowRect (f)).toNearestInt().expanded (2));
                if (f < 4) askRepaint (r (Rectangle<float> (L::fsSprite, L::fsSprite).withCentre ({ L::fsX[f], L::fsY })).toNearestInt());
            }
        }
    }
    Colour drawnLedColour[kLeds];

    friend class OctopusEditor;
};

//==============================================================================
OctopusEditor::OctopusEditor (OctopusProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    view = std::make_unique<PedalView> (proc);
    addAndMakeVisible (*view);
    setResizable (true, true);
    const int minW = L::kMinW;                                                    // jamais plus petit que 60 %
    const int maxW = L::kMaxW;
    setResizeLimits (minW, (int) std::lround ((double) minW * kH / kW), maxW, (int) std::lround ((double) maxW * kH / kW));
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    hostLock.setMinimumSize (minW, (int) std::lround ((double) minW * kH / kW));
    hostLock.setMaximumSize (maxW, (int) std::lround ((double) maxW * kH / kW));
    setOpaque (true);
    setScaleIndex (jlimit (-1, L::kNumSizes - 1, proc.uiScaleIdx.load()));
    view->initCaches();
}

OctopusEditor::~OctopusEditor()
{
    for (int i = getNumChildComponents(); --i >= 0;)              // nuancier resté ouvert
        if (auto* l = dynamic_cast<OctoPickerLayer*> (getChildComponent (i))) delete l;
}

//==============================================================================
// Anti-gel v2 (NINE HANDS | DSP) :
//  1. moteur de rendu logiciel forcé sur Windows (Direct2D peut figer la fenêtre dans certains hôtes),
//     appliqué à chaque nouvelle fenêtre hôte (peer) et revérifié toutes les secondes, jamais en boucle ;
//  2. aucun calcul lourd dans paint() : les caches d'images (fond, footswitchs, éclairage des LED) sont refaits
//     par le timer seulement, après stabilisation de la taille ou quand l'échelle d'affichage change (au plus toutes les 250 ms) ;
//  3. chien de garde : si un redessin demandé n'est pas arrivé au bout de 1,5 s alors que la fenêtre est
//     visible, le moteur de rendu est réappliqué et toute la fenêtre est redessinée ;
//  4. côté audio : aucun appel à l'hôte ni allocation depuis le thread audio (voir PluginProcessor).
void OctopusEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if ((void*) peer != lastPeer) { lastPeer = (void*) peer; repaint(); }
    lastRendererCheckMs = Time::getMillisecondCounterHiRes();
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw)
            peer->setCurrentRenderingEngine (sw);
    }
   #endif
}

void OctopusEditor::checkRenderer()
{
    if ((void*) getPeer() != lastPeer || Time::getMillisecondCounterHiRes() - lastRendererCheckMs > 1000.0)
        useSoftwareRenderer();
    trackHostWindow();
}

// verrou de la fenetre du DAW (WindowMinSize) : rattache a chaque nouvelle fenetre hote
void OctopusEditor::trackHostWindow()
{
    if (auto* peer = getPeer())
    {
        void* handle = peer->getNativeHandle();
        if (handle != nullptr && ! hostLock.isAttachedTo (handle))
        {
            juce::Component::SafePointer<juce::Component> top (&peer->getComponent());
            hostLock.attach (handle, [top] { return top != nullptr ? top->getWidth() : 0; });
            hostLock.enforceNow();
        }
        if (getWidth() < L::kMinW - 2) hostLock.enforceNow();
    }
    else hostLock.detach();
}

void OctopusEditor::resetRenderer()
{
    lastPeer = nullptr;
    useSoftwareRenderer();
    repaint();
}

void OctopusEditor::paint (Graphics& g) { g.fillAll (Colours::black); }
void OctopusEditor::parentHierarchyChanged() { useSoftwareRenderer(); trackHostWindow(); }
void OctopusEditor::visibilityChanged()      { useSoftwareRenderer(); if (isShowing()) repaint(); }

void OctopusEditor::resized()
{
    view->setBounds (getLocalBounds());   // jamais de setSize ici (boucle avec l'hôte à 125 / 150 %)
    for (auto* c : getChildren())                                  // la taille change : le nuancier se ferme
        if (auto* l = dynamic_cast<OctoPickerLayer*> (c); l != nullptr && l->getBounds() != getLocalBounds() && l->isVisible()) l->dismiss();
}

int OctopusEditor::getScaleIndex() const
{
    for (int i = 0; i < L::kNumSizes; ++i) if (std::abs (getWidth() - L::kWidths[i]) <= 3) return i;
    return -1;
}

void OctopusEditor::setScaleIndex (int idx)
{
    idx = jlimit (-1, L::kNumSizes - 1, idx);
    if (idx == 0) idx = 1;                                  // ancien 50 % : minimum 60 %
    proc.uiScaleIdx = idx;                                  // -1 : suit la taille par défaut choisie dans le menu
    const int w = L::kWidths[idx < 0 ? OctoSettings::defaultSize() : idx];
    setSize (w, (int) std::lround ((double) w * kH / kW));
    hostLock.enforceNow();
}

void OctopusEditor::showScreen (int which, int arg)
{
    switch (which)
    {
        case 1: view->openBrowser (arg); break;
        case 2: view->openParams (arg); break;
        case 3: view->openBanks(); break;
        case 4: view->openParams (arg); view->showPresets(); break;
        case 5: view->presetColourMenu (arg); break;
        case 6: view->openBanks(); view->colourMenu (arg); break;
        case 7: view->setHalo (arg); break;
        case 8: view->openBanks(); view->tunerColourMenu (view->S (PedalView::colourBtnRect (OctopusEngine::kFs))); break;
        case 9: view->openSemiDrum(); break;                                      // tambour de Semitones
        case 10: view->semiTarget = jlimit (0, semi::kCount - 1, view->semiTarget + arg); break;   // molette
        case 17: view->browserByType = arg != 0; view->listScroll = 0; view->listPos = 0.0f; view->askRepaint (view->screenPx()); break;   // rendus : tri du navigateur
        case 18: view->listScroll = jlimit (0, view->browserMaxScroll(), arg); view->listPos = (float) view->listScroll; view->askRepaint (view->screenPx()); break;   // rendus : défilement du navigateur
        case 15: view->openMixer (arg); break;   // rendus : mixeur d'un groupe (-1 : fermé)
        case 13: view->recGlowPc = arg; view->askRepaint (view->screenPx()); break;   // rendus : glow de l'effet record
        case 14: view->recColourMenu(); break;
        case 21: if (arg != 0) view->startHeaderRename(); else view->headerNameEd.setVisible (false); break;   // rendus : renommer le preset (en-tête)
        case 22: view->drumBgStyle = arg; view->askRepaint (view->screenPx()); break;   // rendus : fond du tambour (essais)
        case 20: if (arg != 0) view->openInsertDrum(); else view->closeInsertDrum(); break;   // rendus : tambour de l'emplacement
        case 19: if (arg != 0) view->openModeDrum(); else view->closeModeDrum(); break;   // rendus : tambour du mode de CHROMA
        case 12: view->recPreview = arg == 0 ? -1 : (arg == 1 ? 1 : 2); view->recBlinkOn = arg != 2; view->askRepaint (view->screenPx()); break;   // rendus : 1 = armée (REC fixe), 2 = enregistrement (REC éteint), 3 = enregistrement (REC allumé)
        case 11: view->openPicker ("Semitones colour", view->semiColour(), view->S (view->semiRect()), true, [] (Colour) {}); break;
        default: view->closeSemiDrum(); view->closeOverlay(); break;
    }
}

void OctopusEditor::screenSearch (const String& t, int chip) { view->setChip (chip); view->setSearch (t); }
void OctopusEditor::screenPick (int visibleIndex) { view->pickVisible (visibleIndex); }
void OctopusEditor::screenFavourite (int visibleIndex)
{
    const auto items = view->filtered();
    const int i = view->browserItemAt (visibleIndex);
    if (isPositiveAndBelow (i, items.size())) view->toggleFavourite (items.getReference (i));
}

void OctopusEditor::screenParamsMidi() { view->toggleMidiView(); }

// test du clic / double-clic : temps en ms (appui, relâchement) pour chaque clic, puis attente ; renvoie true si un double-clic est reconnu
bool OctopusEditor::testBlockClicks (int slot, const Array<double>& downUp, double waitMs)
{
    view->closeAll();
    const int before = view->doubleClicks;
    const auto c = view->blockCentre (slot);
    static double t0 = 1.0e9;
    t0 += 1.0e5;                       // chaque série de clics bien après la précédente
    for (int k = 0; k + 1 < downUp.size(); k += 2)
    {
        view->runPendingToggle (t0 + downUp[k]);
        view->blockDown (slot, c, t0 + downUp[k]);
        if (view->doubleClicks != before) break;
        view->blockUp (slot, t0 + downUp[k + 1]);
    }
    view->runPendingToggle (t0 + downUp.getLast() + waitMs);
    return view->doubleClicks != before;               // double-clic reconnu (il ouvre la fenetre du plugin)
}
void OctopusEditor::screenHover (Point<float> unitPos) { view->simulateHover (unitPos); }

int OctopusEditor::screenUserPreset (int op, const String& arg)
{
    auto& v = *view;
    switch (op)
    {
        case 0: v.startPresetSave (-1); v.presetNameEd.setText (arg, dontSendNotification); break;
        case 1: if (v.presetEdIndex < -1) v.startPresetSave (-1); v.presetNameEd.setText (arg, dontSendNotification); v.commitPresetName(); break;
        case 2: v.loadUserPreset (arg.getIntValue()); break;
        case 3: v.cancelPresetName(); v.upresets.clear(); v.userCur = -1; UserFxPresets::save (v.upPlugin, v.upresets); break;
        default: break;
    }
    v.askRepaint (v.screenPx());
    return (int) v.upresets.size();
}
void OctopusEditor::screenTuningMenu() { view->tuningMenu(); }
juce::Image OctopusEditor::menuPreview (int f) { return view->menuPreview (f); }
