#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "FxCategory.h"
#include "OctopusEngine.h"
#include "Routing.h"

// Menus clic droit d'Octopus (v1.17.0, design « grille de blocs ») : fond sombre arrondi, icônes devant les commandes,
// coche bleue, en-tête du footswitch (pastille de la lettre), et pour STOMP une grille de tuiles de blocs qui reste ouverte.
namespace MenuStyle
{
    using namespace juce;

    const Colour kBg (0xff13171d), kBorder (0xff262c36), kHover (0xff1c222b), kSep (0xff20252e),
                 kText (0xffeef1f5), kDim (0xff8d96a4), kFaint (0xff3a414d), kAccent (0xff4fc3f7), kTileBg (0xff0e1115), kTileLine (0xff2a303a);

    struct Fonts { Typeface::Ptr reg, med, bold; };
    inline Font fnt (const Typeface::Ptr& tf, float h) { return Font (FontOptions (tf).withHeight (h)); }

    // petites icônes des commandes, dessinées sur une grille de 16 (choisies d'après le texte de la commande)
    enum Ico { icoNone = 0, icoSave, icoLoad, icoStomp, icoClear, icoMidi, icoLatch, icoColour, icoAdd, icoBanks, icoSize, icoParams,
               icoWindow, icoSplit, icoMove, icoRemove, icoBypass, icoMix, icoSwap, icoPreset, icoGlow, icoPedal };

    inline int iconFor (const String& t)
    {
        const auto s = t.toLowerCase();
        if (s.startsWith ("save")) return icoSave;
        if (s.startsWith ("load")) return icoLoad;
        if (s.contains ("clear") || s.startsWith ("reset") || s.startsWith ("cancel")) return icoClear;
        if (s.startsWith ("stomp") || s.startsWith ("assign") || s.startsWith ("footswitch")) return icoStomp;
        if (s.contains ("latching")) return icoLatch;
        if (s.startsWith ("midi") || s.startsWith ("learn") || s.startsWith ("program change")) return icoMidi;
        if (s.startsWith ("glow")) return icoGlow;
        if (s.contains ("color") || s.contains ("colour") || s.contains ("glow")) return icoColour;
        if (s.startsWith ("add") || s.startsWith ("insert")) return icoAdd;
        if (s.startsWith ("bank")) return icoBanks;
        if (s.contains ("size") || s.endsWith ("%")) return icoSize;
        if (s.startsWith ("parameters") || s.startsWith ("edit")) return icoParams;
        if (s.contains ("window")) return icoWindow;
        if (s.startsWith ("split") || s.contains ("path c") || s.contains ("parallel")) return icoSplit;
        if (s.startsWith ("move")) return icoMove;
        if (s.startsWith ("remove") || s.startsWith ("delete") || s.startsWith ("unsplit")) return icoRemove;
        if (s.contains ("bypass") || s.startsWith ("turn") || s.startsWith ("on ") || s.startsWith ("switch")) return icoBypass;
        if (s.startsWith ("mixer") || s.startsWith ("mix")) return icoMix;
        if (s.contains ("a <-> b") || s.contains ("swap")) return icoSwap;
        if (s.startsWith ("preset") || s.startsWith ("rename") || s.startsWith ("user preset")) return icoPreset;
        return icoNone;
    }

    inline void drawIco (Graphics& g, int ico, Rectangle<float> r, Colour c)
    {
        if (ico == icoNone) return;
        const float k = r.getWidth() / 16.0f;
        auto P = [&] (float x, float y) { return Point<float> (r.getX() + x * k, r.getY() + y * k); };
        Path p;
        auto line = [&] (float x1, float y1, float x2, float y2) { p.startNewSubPath (P (x1, y1)); p.lineTo (P (x2, y2)); };
        auto circ = [&] (float cx, float cy, float rad) { p.addEllipse (P (cx - rad, cy - rad).x, P (cx - rad, cy - rad).y, rad * 2 * k, rad * 2 * k); };
        auto rect = [&] (float x, float y, float w, float h, float rr) { p.addRoundedRectangle (P (x, y).x, P (x, y).y, w * k, h * k, rr * k); };
        bool fillDots = false;
        switch (ico)
        {
            case icoSave:   p.startNewSubPath (P (2.5f, 2.5f)); p.lineTo (P (11, 2.5f)); p.lineTo (P (13.5f, 5)); p.lineTo (P (13.5f, 13.5f)); p.lineTo (P (2.5f, 13.5f)); p.closeSubPath();
                            p.startNewSubPath (P (5, 2.5f)); p.lineTo (P (5, 6)); p.lineTo (P (10, 6)); p.lineTo (P (10, 2.5f)); break;
            case icoLoad:   line (8, 2, 8, 10); line (4.5f, 6.5f, 8, 10); line (11.5f, 6.5f, 8, 10); line (3, 13.5f, 13, 13.5f); break;
            case icoStomp:  circ (8, 8, 5.5f); circ (8, 8, 2.0f); break;
            case icoClear:  line (4, 4, 12, 12); line (12, 4, 4, 12); break;
            case icoMidi:   circ (8, 8, 6.0f); fillDots = true; break;
            case icoLatch:  p.startNewSubPath (P (2, 11)); p.lineTo (P (5, 11)); p.lineTo (P (5, 5)); p.lineTo (P (8, 5)); p.lineTo (P (8, 11)); p.lineTo (P (11, 11)); p.lineTo (P (11, 5)); p.lineTo (P (14, 5)); break;
            case icoColour: circ (8, 8, 6.0f); break;
            case icoAdd:    line (8, 3, 8, 13); line (3, 8, 13, 8); break;
            case icoBanks:  rect (2.5f, 3, 11, 3, 1); rect (2.5f, 7, 11, 3, 1); rect (2.5f, 11, 11, 3, 1); break;
            case icoSize:   line (3, 3, 13, 13); line (3, 3, 3, 7); line (3, 3, 7, 3); line (13, 13, 13, 9); line (13, 13, 9, 13); break;
            case icoParams: line (3, 4, 13, 4); line (3, 8, 13, 8); line (3, 12, 13, 12); circ (6, 4, 1.4f); circ (10, 8, 1.4f); circ (5, 12, 1.4f); break;
            case icoWindow: rect (2, 3, 12, 10, 1.5f); line (2, 6, 14, 6); break;
            case icoSplit:  line (2, 8, 5, 8); line (11, 8, 14, 8); p.startNewSubPath (P (5, 8)); p.quadraticTo (P (8, 3), P (11, 8)); p.startNewSubPath (P (5, 8)); p.quadraticTo (P (8, 13), P (11, 8)); break;
            case icoMove:   line (8, 2, 8, 14); line (5, 5, 8, 2); line (11, 5, 8, 2); line (5, 11, 8, 14); line (11, 11, 8, 14); break;
            case icoRemove: line (3, 4.5f, 13, 4.5f); p.startNewSubPath (P (4.5f, 4.5f)); p.lineTo (P (5.5f, 13.5f)); p.lineTo (P (10.5f, 13.5f)); p.lineTo (P (11.5f, 4.5f)); line (6.5f, 2.5f, 9.5f, 2.5f); break;
            case icoBypass: p.addCentredArc (P (8, 8.5f).x, P (8, 8.5f).y, 5.5f * k, 5.5f * k, 0.0f, 0.75f, MathConstants<float>::twoPi - 0.75f, true); line (8, 2, 8, 8); break;
            case icoMix:    line (4, 2.5f, 4, 13.5f); line (8, 2.5f, 8, 13.5f); line (12, 2.5f, 12, 13.5f); rect (2.5f, 9, 3, 2, 0.5f); rect (6.5f, 5, 3, 2, 0.5f); rect (10.5f, 10, 3, 2, 0.5f); break;
            case icoSwap:   line (3, 5.5f, 13, 5.5f); line (10, 2.5f, 13, 5.5f); line (13, 10.5f, 3, 10.5f); line (6, 13.5f, 3, 10.5f); break;
            case icoPreset: rect (3, 2.5f, 10, 11, 1.5f); line (5.5f, 6, 10.5f, 6); line (5.5f, 9, 9, 9); break;
            case icoGlow:   circ (8, 8, 2.6f);
                            for (int i = 0; i < 8; ++i) { const float a = (float) i * MathConstants<float>::pi / 4.0f;
                                                          line (8 + 4.4f * std::cos (a), 8 + 4.4f * std::sin (a), 8 + 6.3f * std::cos (a), 8 + 6.3f * std::sin (a)); }
                            break;
            case icoPedal:  rect (3.5f, 1.5f, 9, 13, 1.8f); circ (8, 5.5f, 1.6f); circ (8, 11, 1.8f); break;
            default: break;
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (1.5f * k, PathStrokeType::curved, PathStrokeType::rounded));
        if (ico == icoColour)
        {
            Path h; h.addPieSegment (P (2, 2).x, P (2, 2).y, 12 * k, 12 * k, MathConstants<float>::pi, MathConstants<float>::twoPi, 0.0f);
            g.fillPath (h);
        }
        if (fillDots)
            for (auto d : { Point<float> (5, 7), Point<float> (11, 7), Point<float> (6.3f, 10.3f), Point<float> (9.7f, 10.3f), Point<float> (8, 4.7f) })
                g.fillEllipse (Rectangle<float> (1.8f * k, 1.8f * k).withCentre (P (d.x, d.y)));
    }

    inline void drawCheck (Graphics& g, Rectangle<float> r, Colour c, float w)
    {
        Path p;
        p.startNewSubPath (r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.52f);
        p.lineTo (r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.76f);
        p.lineTo (r.getX() + r.getWidth() * 0.84f, r.getY() + r.getHeight() * 0.26f);
        g.setColour (c);
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // ------------------------------------------------------------------ apparence de tous les menus
    struct MenuLnf : public LookAndFeel_V4
    {
        Fonts f;
        MenuLnf()
        {
            setColour (PopupMenu::backgroundColourId, Colours::transparentBlack);       // fenêtre transparente : coins arrondis
            setColour (PopupMenu::textColourId, kText);
            setColour (PopupMenu::headerTextColourId, kDim);
            setColour (PopupMenu::highlightedBackgroundColourId, kHover);
            setColour (PopupMenu::highlightedTextColourId, Colours::white);
        }
        Font getPopupMenuFont() override { return fnt (f.med, 15.0f); }
        // champs de texte d'Octopus : fond et cadre aux coins arrondis, comme le reste de l'interface
        void fillTextEditorBackground (Graphics& g, int w, int h, TextEditor& ed) override
        {
            const auto bg = ed.getProperties()["roundBg"];
            if (bg.isVoid()) { LookAndFeel_V4::fillTextEditorBackground (g, w, h, ed); return; }
            g.setColour (Colour ((uint32) (int) bg));
            g.fillRoundedRectangle (Rectangle<float> ((float) w, (float) h), jmin (10.0f, (float) h * 0.2f));
        }
        void drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& ed) override
        {
            if (ed.getProperties()["roundBg"].isVoid()) { LookAndFeel_V4::drawTextEditorOutline (g, w, h, ed); return; }
            if (! ed.isEnabled()) return;
            const bool foc = ed.hasKeyboardFocus (true) && ! ed.isReadOnly();
            const float t = foc ? 1.6f : 1.1f;
            g.setColour (ed.findColour (foc ? TextEditor::focusedOutlineColourId : TextEditor::outlineColourId));
            g.drawRoundedRectangle (Rectangle<float> ((float) w, (float) h).reduced (t * 0.5f), jmin (10.0f, (float) h * 0.2f), t);
        }
        int getPopupMenuBorderSize() override { return 7; }
        void drawPopupMenuBackground (Graphics& g, int w, int h) override
        {
            const auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            g.setColour (kBg);
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (kBorder);
            g.drawRoundedRectangle (r, 10.0f, 1.0f);
        }
        void getIdealPopupMenuItemSize (const String& text, bool isSeparator, int standardHeight, int& w, int& h) override
        {
            if (isSeparator) { w = 50; h = 11; return; }
            h = standardHeight > 0 ? standardHeight : 32;
            w = (int) std::ceil (GlyphArrangement::getStringWidth (getPopupMenuFont(), text)) + 34 + 40;
        }
        void drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& name) override
        {
            g.setFont (fnt (f.bold, 11.5f).withExtraKerningFactor (0.14f));
            g.setColour (kDim);
            g.drawFittedText (name.toUpperCase(), area.withTrimmedLeft (14).withTrimmedRight (10).withTrimmedTop (6), Justification::centredLeft, 1);
        }
        void drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                bool hasSubMenu, const String& text, const String& shortcut, const Drawable* icon, const Colour* textColour) override
        {
            ignoreUnused (shortcut, icon);
            const auto a = area.toFloat();
            if (isSeparator)
            {
                g.setColour (kSep);
                g.fillRect (a.withSizeKeepingCentre (a.getWidth() - 20.0f, 1.0f));
                return;
            }
            const bool hi = isHighlighted && isActive;
            if (hi)
            {
                g.setColour (kHover);
                g.fillRoundedRectangle (a.reduced (5.0f, 1.0f), 7.0f);
            }
            const auto ib = Rectangle<float> (15.0f, 15.0f).withCentre ({ a.getX() + 22.0f, a.getCentreY() });
            if (isTicked) drawCheck (g, ib.expanded (1.0f), kAccent, 2.0f);
            else drawIco (g, iconFor (text), ib, ! isActive ? kFaint : hi ? kAccent : kDim);
            auto tr = a.withTrimmedLeft (38.0f).withTrimmedRight (hasSubMenu ? 30.0f : 14.0f);
            g.setFont (getPopupMenuFont());
            g.setColour (textColour != nullptr ? *textColour : ! isActive ? kDim.withAlpha (0.55f) : hi ? Colours::white : kText);
            g.drawFittedText (text, tr.toNearestInt(), Justification::centredLeft, 1, 0.9f);
            if (hasSubMenu)                                                           // ›
            {
                const float cx = a.getRight() - 18.0f, cy = a.getCentreY();
                Path p; p.startNewSubPath (cx - 2.5f, cy - 5.0f); p.lineTo (cx + 2.5f, cy); p.lineTo (cx - 2.5f, cy + 5.0f);
                g.setColour (hi ? Colours::white : kDim);
                g.strokePath (p, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
            }
        }
    };

    // ------------------------------------------------------------------ en-tête des menus : pastille + titre + sous-titre
    // pastille : une lettre (footswitch), l'icône d'une famille d'effets, un groupe (voies parallèles) ou une icône de commande
    struct Badge
    {
        enum Type { letter, fx, group, ui } type = ui;
        String text; int kind = -1, icon = FxCat::iconFamily, ico = icoNone; Colour col { kAccent };
        static Badge ofLetter (String l, Colour c) { Badge b; b.type = letter; b.text = l; b.col = c; return b; }
        static Badge ofFx (int kind, int icon) { Badge b; b.type = fx; b.kind = kind; b.icon = icon; b.col = kind >= 0 ? FxCat::colour (kind) : kAccent; return b; }
        static Badge ofGroup (Colour c) { Badge b; b.type = group; b.col = c; return b; }
        static Badge ofUi (int ico, Colour c = kAccent) { Badge b; b.type = ui; b.ico = ico; b.col = c; return b; }
    };

    inline void drawGroupIcon (Graphics& g, Rectangle<float> ic, Colour c, float w)
    {
        Path p; const float k = ic.getWidth() / 24.0f; auto P = [&] (float x, float y) { return Point<float> (ic.getX() + x * k, ic.getY() + y * k); };
        p.addEllipse (Rectangle<float> (4.0f * k, 4.0f * k).withCentre (P (5, 12))); p.addEllipse (Rectangle<float> (4.0f * k, 4.0f * k).withCentre (P (19, 12)));
        p.startNewSubPath (P (7, 12)); p.lineTo (P (10, 12)); p.startNewSubPath (P (14, 12)); p.lineTo (P (17, 12));
        p.startNewSubPath (P (10, 12)); p.cubicTo (P (10, 7), P (14, 7), P (14, 12)); p.startNewSubPath (P (10, 12)); p.cubicTo (P (10, 17), P (14, 17), P (14, 12));
        g.setColour (c);
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }

    struct Header : public PopupMenu::CustomComponent
    {
        Fonts f; Badge badge; String title, sub;
        Header (Fonts fo, Badge b, String t, String s) : CustomComponent (false), f (fo), badge (b), title (t), sub (s) {}
        void getIdealSize (int& w, int& h) override
        {
            w = jmax (240, (int) std::ceil (jmax (GlyphArrangement::getStringWidth (fnt (f.bold, 15.5f), title), GlyphArrangement::getStringWidth (fnt (f.reg, 12.5f), sub))) + 72);
            h = 52;
        }
        void paint (Graphics& g) override
        {
            const auto b = Rectangle<float> (30.0f, 30.0f).withPosition (12.0f, 10.0f);
            if (badge.type == Badge::letter)                                  // pastille pleine de la couleur
            {
                g.setColour (badge.col);
                g.fillRoundedRectangle (b, 8.0f);
                g.setColour (badge.col.getPerceivedBrightness() > 0.55f ? Colour (0xff06121a) : Colours::white);
                g.setFont (fnt (f.bold, 17.0f));
                g.drawText (badge.text, b, Justification::centred, false);
            }
            else                                                               // pastille teintée, icône de la couleur
            {
                g.setColour (badge.col.withAlpha (0.16f));
                g.fillRoundedRectangle (b, 8.0f);
                g.setColour (badge.col.withAlpha (0.7f));
                g.drawRoundedRectangle (b.reduced (0.6f), 8.0f, 1.2f);
                const auto ic = b.reduced (6.0f);
                if (badge.type == Badge::fx) FxCat::drawIcon (g, jmax (0, badge.kind), ic, badge.col, 1.7f, badge.icon);
                else if (badge.type == Badge::group) drawGroupIcon (g, ic, badge.col, 1.7f);
                else drawIco (g, badge.ico, ic.reduced (1.0f), badge.col);
            }
            g.setColour (kText);
            g.setFont (fnt (f.bold, 15.5f));
            g.drawText (title, Rectangle<float> (52.0f, sub.isEmpty() ? 15.0f : 8.0f, (float) getWidth() - 60.0f, 19.0f), Justification::centredLeft, true);
            if (sub.isNotEmpty())
            {
                g.setColour (kDim);
                g.setFont (fnt (f.reg, 12.5f));
                g.drawText (sub, Rectangle<float> (52.0f, 27.0f, (float) getWidth() - 60.0f, 16.0f), Justification::centredLeft, true);
            }
        }
    };

    // ------------------------------------------------------------------ STOMP : grille de tuiles (le menu reste ouvert)
    struct BlockGrid : public PopupMenu::CustomComponent
    {
        struct Tile { String key, name; int kind = -1, icon = FxCat::iconFamily; bool group = false; };
        OctopusEngine& eng; Fonts f; int fs; std::function<void()> changed;
        static constexpr int kCols = 3, kGap = 5, kPad = 8, kHead = 24, kFoot = 32, kW = 282;
        static constexpr int kTileH = (kW - 2 * kPad - (kCols - 1) * kGap) / kCols;   // v1.18.5 : cases carrées (hauteur = largeur)
        int hover = -1; bool hoverClear = false;

        BlockGrid (OctopusEngine& e, Fonts fo, int f0, std::function<void()> onChange)
            : CustomComponent (false), eng (e), f (fo), fs (f0), changed (std::move (onChange)) {}

        // blocs de la chaîne ; les repères des voies parallèles sont cachés, le Split devient la tuile du groupe (A <-> B)
        std::vector<Tile> tiles() const
        {
            std::vector<Tile> v;
            for (int b : eng.blocks())
            {
                const int role = eng.roleAt (b);
                const auto& fx = eng.chain().getReference (b);
                if (role == Routing::split && eng.groupAt (b) >= 0)
                {
                    Tile t; t.key = OctopusEngine::swapKey (fx.guid); t.name = eng.blockName (b).replace (" || ", CharPointer_UTF8 (" \xe2\x80\x96 "));
                    t.kind = eng.blockKind (b); t.group = true; v.push_back (t);
                    continue;
                }
                if (role != Routing::none) continue;
                Tile t; t.key = fx.guid; t.name = FxCat::shortName (fx.name); t.kind = eng.blockKind (b); t.icon = FxCat::iconOf (fx.name);
                v.push_back (t);
            }
            return v;
        }
        int rows (int n) const { return jmax (1, (n + kCols - 1) / kCols); }
        void getIdealSize (int& w, int& h) override
        {
            const int n = (int) tiles().size();
            w = kW;
            h = kPad + kHead + rows (n) * (kTileH + kGap) - kGap + kFoot;
        }
        Rectangle<float> tileRect (int i) const
        {
            const float tw = ((float) getWidth() - 2.0f * kPad - (kCols - 1) * kGap) / kCols;
            return { kPad + (float) (i % kCols) * (tw + kGap), (float) (kPad + kHead + (i / kCols) * (kTileH + kGap)), tw, (float) kTileH };
        }
        Rectangle<float> clearRect() const { return { (float) getWidth() - kPad - 64.0f, (float) getHeight() - kFoot + 4.0f, 64.0f, (float) kFoot - 8.0f }; }

        void paint (Graphics& g) override
        {
            const auto t = tiles();
            g.setFont (fnt (f.bold, 11.5f).withExtraKerningFactor (0.14f));
            g.setColour (kDim);
            g.drawText ("TAP THE BLOCKS FOR " + String::charToString ((juce_wchar) ('A' + fs)), Rectangle<float> (kPad + 2.0f, (float) kPad - 2.0f, (float) getWidth(), 18.0f), Justification::centredLeft, false);
            int sel = 0;
            if (t.empty())
            {
                g.setFont (fnt (f.med, 13.5f));
                g.drawText ("No block in the chain", tileRect (0).withWidth ((float) getWidth() - 2.0f * kPad), Justification::centred, false);
            }
            for (int i = 0; i < (int) t.size(); ++i)
            {
                const auto r = tileRect (i);
                const bool on = eng.isAssigned (fs, t[(size_t) i].key);
                sel += on ? 1 : 0;
                const auto c = t[(size_t) i].kind >= 0 ? FxCat::colour (t[(size_t) i].kind) : kAccent;
                g.setColour (on ? c.withAlpha (0.15f) : (hover == i ? Colour (0xff151a21) : kTileBg));
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (on ? c : (hover == i ? kFaint.brighter (0.2f) : kTileLine));
                g.drawRoundedRectangle (r.reduced (0.75f), 8.0f, 1.4f);
                const auto ic = Rectangle<float> (24.0f, 24.0f).withCentre ({ r.getCentreX(), r.getCentreY() - 11.0f });   // icône + nom centrés dans la case carrée
                if (t[(size_t) i].group)                                                // groupe : deux voies entre deux points
                {
                    drawGroupIcon (g, ic, c, 2.0f);
                }
                else FxCat::drawIcon (g, jmax (0, t[(size_t) i].kind), ic, c, 2.0f, t[(size_t) i].icon);
                g.setFont (fnt (f.med, 12.5f));
                g.setColour (on ? Colours::white : kDim);
                g.drawFittedText (t[(size_t) i].name + (t[(size_t) i].group ? String (CharPointer_UTF8 ("  \xe2\x87\x84")) : String()),
                                  Rectangle<float> (r.getX() + 3.0f, r.getCentreY() + 5.0f, r.getWidth() - 6.0f, 18.0f).toNearestInt(), Justification::centred, 1, 0.8f);
                if (on)                                                                  // pastille cochée
                {
                    const auto bd = Rectangle<float> (13.0f, 13.0f).withPosition (r.getRight() - 17.0f, r.getY() + 4.0f);
                    g.setColour (c);
                    g.fillEllipse (bd);
                    drawCheck (g, bd.reduced (2.5f), Colour (0xff06121a), 1.8f);
                }
            }
            // pied : nombre choisi + CLEAR (sans filet)
            g.setFont (fnt (f.med, 12.5f));
            g.setColour (kDim);
            g.drawText (String (sel) + " selected", Rectangle<float> (kPad + 2.0f, (float) getHeight() - kFoot + 2.0f, 160.0f, (float) kFoot - 2.0f), Justification::centredLeft, false);
            g.setFont (fnt (f.bold, 12.5f).withExtraKerningFactor (0.1f));
            g.setColour (sel > 0 ? (hoverClear ? kAccent.brighter (0.25f) : kAccent) : kFaint);
            g.drawText ("CLEAR", clearRect(), Justification::centredRight, false);
        }
        int tileAt (Point<float> p) const
        {
            const int n = (int) tiles().size();
            for (int i = 0; i < n; ++i) if (tileRect (i).contains (p)) return i;
            return -1;
        }
        void mouseMove (const MouseEvent& e) override
        {
            const int h = tileAt (e.position);
            const bool hc = clearRect().contains (e.position);
            if (h != hover || hc != hoverClear) { hover = h; hoverClear = hc; repaint(); }
        }
        void mouseExit (const MouseEvent&) override { hover = -1; hoverClear = false; repaint(); }
        void mouseUp (const MouseEvent& e) override
        {
            if (clearRect().contains (e.position)) { eng.clearAssign (fs); }
            else
            {
                const int i = tileAt (e.position);
                if (i < 0) return;
                eng.toggleAssign (fs, tiles()[(size_t) i].key);
            }
            repaint();
            if (changed) changed();
        }
    };

    // ------------------------------------------------------------------ ligne à deux colonnes (accordages : nom | notes alignées)
    struct TwoColItem : public PopupMenu::CustomComponent
    {
        Fonts f; String left, right; float colX; bool ticked;
        TwoColItem (Fonts fo, String l, String r, float cx, bool t) : CustomComponent (true), f (fo), left (l), right (r), colX (cx), ticked (t) {}
        static float leftWidth (const Fonts& fo, const String& t) { return GlyphArrangement::getStringWidth (fnt (fo.med, 15.0f), t); }
        void getIdealSize (int& w, int& h) override
        {
            w = (int) std::ceil (38.0f + colX + GlyphArrangement::getStringWidth (fnt (f.reg, 14.0f), right) + 22.0f);
            h = 32;
        }
        void paint (Graphics& g) override
        {
            const auto a = getLocalBounds().toFloat();
            const bool hi = isItemHighlighted();
            if (hi)
            {
                g.setColour (kHover);
                g.fillRoundedRectangle (a.reduced (5.0f, 1.0f), 7.0f);
            }
            if (ticked) drawCheck (g, Rectangle<float> (17.0f, 17.0f).withCentre ({ 22.0f, a.getCentreY() }), kAccent, 2.0f);
            g.setFont (fnt (f.med, 15.0f));
            g.setColour (hi ? Colours::white : kText);
            g.drawText (left, Rectangle<float> (38.0f, 0.0f, colX, a.getHeight()), Justification::centredLeft, true);
            g.setFont (fnt (f.reg, 14.0f));
            g.setColour (hi ? kText : kDim);
            g.drawText (right, Rectangle<float> (38.0f + colX, 0.0f, a.getWidth() - 38.0f - colX - 8.0f, a.getHeight()), Justification::centredLeft, true);
        }
    };
}
