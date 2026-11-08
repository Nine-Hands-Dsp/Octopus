# OCTOPUS (NINE HANDS | DSP) - notice PDF bilingue (anglais puis français), illustrée avec le rendu réel du plugin.
# Usage : python3 tools/manual.py <dossier des captures (design/renders)> <fichier PDF de sortie>
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_CENTER
from reportlab.platypus import (BaseDocTemplate, PageTemplate, NextPageTemplate, Frame, Paragraph, Spacer, Image as RLImage,
                                Table, TableStyle, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS = sys.argv[1]
OUT = sys.argv[2]
VERSION = "1.13.2"
TMP = os.path.join(os.path.dirname(os.path.abspath(OUT)), "manual_tmp")
os.makedirs(TMP, exist_ok=True)

for n, f in (("Pop", "Poppins-Regular"), ("PopL", "Poppins-Light"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(ROOT, "fonts", f + ".ttf")))

ACC = colors.HexColor("#1E9BD0"); DARK = colors.HexColor("#1A1C22"); GREY = colors.HexColor("#646B76"); LIGHT = colors.HexColor("#EAF4FA")

# ---------- capture annotée (repère 1249 x 934 du plugin) ----------
src = Image.open(os.path.join(SHOTS, "octopus_100pc.png")).convert("RGB")
k = src.height / 934.0
marks = [(n, x, y) for n, x, y in ((1, 75.7, 62.5), (2, 201.8, 62.5), (3, 700.6, 62.5), (4, 891.9, 62.5), (5, 1050.1, 62.5), (6, 1151.8, 62.5), (7, 160.2, 174.5), (8, 624.8, 370.2), (9, 1175.5, 394.6), (10, 180.4, 703.0), (11, 482.5, 868.3), (12, 465.0, 808.5), (13, 425.2, 62.5), (14, 534.7, 62.5))]   # repère 1249 x 934 (cadre du 07/11)
ann = src.copy(); dr = ImageDraw.Draw(ann)
fnt = ImageFont.truetype(os.path.join(ROOT, "fonts", "Poppins-Bold.ttf"), 26)
for n, x, y in marks:
    cx, cy, r = x * k, y * k, 20
    dr.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(255, 255, 255), outline=(30, 155, 208), width=5)
    dr.text((cx, cy + 1), str(n), font=fnt, fill=(10, 80, 120), anchor="mm")
annotated = os.path.join(TMP, "annotated.png"); ann.save(annotated)

# gros plans de l'écran (vitre : 36, 67, 1177 x 720)
def screen_crop(name, out):
    im = Image.open(os.path.join(SHOTS, name)).convert("RGB")
    kk = im.height / 934.0
    im.crop((int(36 * kk), int(67 * kk), int(1213 * kk), int(788 * kk))).save(os.path.join(TMP, out))
    return os.path.join(TMP, out)
scr_stomp = screen_crop ("octopus_stomp.png", "stomp.png")
scr_add = screen_crop ("octopus_add_effect.png", "add.png")
scr_params = screen_crop ("octopus_params.png", "params.png")
scr_banks = screen_crop ("octopus_banks.png", "banks.png")
scr_edit = screen_crop ("octopus_edited_hover.png", "edited.png")
scr_presets = screen_crop ("octopus_presets_user.png", "presets.png")
scr_tmono = screen_crop ("octopus_tuner_builtin.png", "tuner_mono.png")
scr_tpoly = screen_crop ("octopus_tuner_tuning.png", "tuner_poly.png")
scr_semi = screen_crop ("octopus_semitones_drum.png", "semitones.png")
scr_split = screen_crop ("octopus_split.png", "split.png")
scr_mix = screen_crop ("octopus_split_mixer.png", "split_mixer.png")

# couverture : pédale détourée (alpha de background.png : boîtier + jacks) sur fond noir, reflet au sol, halo bleu très doux
shot = Image.open(os.path.join(SHOTS, "octopus_100pc.png")).convert("RGB")
msk = Image.open(os.path.join(ROOT, "background.png")).split()[3].resize(shot.size, Image.LANCZOS)
cut = Image.new("RGBA", shot.size, (0, 0, 0, 0)); cut.paste(shot, (0, 0), msk)
cut = cut.crop(cut.getbbox())
cw, ch = cut.size
pad = int(cw * 0.10)
canvas = Image.new("RGBA", (cw + 2 * pad, int(ch * 1.30) + pad), (0, 0, 0, 255))
yy, xx = np.mgrid[0:canvas.height, 0:canvas.width].astype(np.float64)
gcx, gcy = canvas.width / 2, pad + ch * 0.45
glow = np.exp(-(((xx - gcx) / (cw * 0.48)) ** 2 + ((yy - gcy) / (ch * 0.42)) ** 2)) * 38
edge = np.clip(np.minimum.reduce([xx, canvas.width - 1 - xx, yy, canvas.height - 1 - yy]) / (pad * 0.9), 0, 1)
glow *= edge ** 2
halo = np.dstack([glow * 0.18, glow * 0.62, glow * 1.0, np.full_like(glow, 255)]).astype(np.uint8)
canvas = Image.alpha_composite(canvas, Image.fromarray(halo, "RGBA"))
refl = cut.transpose(Image.FLIP_TOP_BOTTOM)
ra = np.array(refl).astype(np.float64)
fade = np.clip(1 - np.arange(refl.height) / (refl.height * 0.22), 0, 1) ** 1.6 * 0.22
ra[..., 3] *= fade[:, None]
canvas.alpha_composite(Image.fromarray(ra.astype(np.uint8), "RGBA"), (pad, pad + ch + 6))
canvas.alpha_composite(cut, (pad, pad))
canvas = canvas.crop((0, 0, canvas.width, pad + int(ch * 1.20)))
cover = os.path.join(TMP, "cover_cutout.png"); canvas.convert("RGB").save(cover)

def img(path, width):
    im = Image.open(path); h = width * im.height / im.width
    return RLImage(path, width=width, height=h)

# ---------- styles ----------
st = {
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=20, leading=24, textColor=ACC, spaceAfter=8, spaceBefore=4, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=13, leading=17, textColor=DARK, spaceAfter=4, spaceBefore=10, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
    "cover": ParagraphStyle("cover", fontName="PopL", fontSize=40, leading=46, textColor=colors.white, alignment=TA_CENTER),
    "coversub": ParagraphStyle("coversub", fontName="Pop", fontSize=12, leading=16, textColor=colors.HexColor("#B4BCC6"), alignment=TA_CENTER),
    "coverlang": ParagraphStyle("coverlang", fontName="PopM", fontSize=8.5, leading=11, textColor=colors.HexColor("#4FC3F7")),
}
def P(t, s="p"): return Paragraph(t, st[s])
def bullets(items): return [P("&bull;&nbsp;&nbsp;" + i) for i in items]
def table(rows, widths, header=True):
    data = [[P(c, "cellb" if (header and r == 0) else "cell") for c in row] for r, row in enumerate(rows)]
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LINEBELOW", (0, 0), (-1, -1), 0.3, colors.HexColor("#D5DDE5")),
             ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]
    if header: style += [("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, 0), 0.8, ACC)]
    t.setStyle(TableStyle(style)); return t

# ---------- textes (conformes au code livré : PluginEditor.cpp, OctopusEngine.cpp, PluginProcessor.cpp) ----------
T = {
"en": dict(
 lang="ENGLISH", sub="FX chain controller for REAPER &nbsp;·&nbsp; VST3 for Windows and macOS", ver="User manual &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Introduction",
 intro=["<b>OCTOPUS</b> is not an effect: it is a <b>control screen for the FX chain of a REAPER track</b>, laid out like the screen of a modern floor modeler (in the spirit of the Quad Cortex mini). Insert it on your guitar track and its screen shows every effect of that track as a coloured block, from IN to OUT.",
        "Whatever you do on the Octopus screen happens <b>at the same time in the track's FX chain</b>: switching a block on or off, adding an effect, removing it, moving it, changing its parameters. And whatever you do directly in REAPER appears on the Octopus screen a moment later.",
        "On top of that, Octopus stores <b>16 banks of 6 presets</b> (1A to 16F) - each preset is a complete chain with its plug-ins, their order and all their settings - and gives you <b>6 footswitches</b> (A-D, plus E = A+B and F = C+D) to recall presets (PRESET mode) or to switch groups of blocks (STOMP mode), with the mouse or a MIDI foot controller such as the M-Vave Chocolate Plus. When the CHROMA tuner block is switched on, Octopus shows CHROMA's screen and becomes the tuner.",
        "The sound passes through Octopus untouched (zero latency), unless you choose a transposition with Semitones. Octopus works <b>in REAPER only</b>: REAPER gives VST3 plug-ins access to their track and to its API, which other DAWs do not. In another DAW the screen simply says so."],
 feat_t="Features at a glance",
 feat=["<b>Live view of the track's FX chain</b>: every insert of the track as a coloured block (family, icon, ON / OFF, open window, parallel, offline), 7 blocks per row, 3 rows on screen, scrolling beyond.",
       "<b>Parallel paths</b>: split the chain into 2 or 3 paths (two amps, amp + IR pairs...) with a mixer per path (level, pan, phase, mute).",
       "<b>Full control from the screen</b>: switch on / off, add one or several effects at once, remove, move by drag and drop, insert before / after, process in parallel, open the plug-in window.",
       "<b>Parameters on screen</b>: 8 per page, large names, smooth sliders for continuous values and notched sliders for modes and switches, MIDI parameters behind a MIDI button, BYPASS next to the close button.",
       "<b>Plug-in presets</b>: factory and REAPER user presets of each plug-in, plus your own user presets saved by Octopus (SAVE USER), with load, replace, rename and delete.",
       "<b>96 chain presets</b> (16 banks x 6) saved in the project, with fast switching when only settings differ, copy / paste, rename, export / import of the banks.",
       "<b>6 footswitches</b> A-F in PRESET or STOMP mode (E = A+B, F = C+D), each with its backlit letter or marking in the colour you choose (colour picker with halo opacity), or the colour of its blocks in STOMP mode.",
       "<b>Built-in tuner</b>: switch a CHROMA block on and Octopus becomes the tuner (MONO, POLY, STROBE), with CHROMA's tuning presets.",
       "<b>Semitones built in</b>: transposition from -12 to +12 semitones at the input (21 presets on a vertical drum: drop tunings, capo, octaves, harmonies), global or saved with a preset; <b>mini CHROMA</b> in the header showing the note that comes out, green when in tune, red when not.",
       "<b>MIDI foot controller</b>: MIDI learn of every footswitch, bank and mode command, momentary or latching, Program Change 1-64 for presets A-D of each bank.",
       "<b>Safe by design</b>: every action is one REAPER undo point, the sound passes through untouched (except a Semitones transposition), no host call from the audio thread, window in 4 sizes (50 to 100 %) with a default size of your choice."],
 more_t="Menus and details",
 more=[("Right-click a block", "<b>Bypass / Enable</b> - <b>Parameters on screen</b> - <b>Open / Close plug-in window</b> (the floating window of REAPER) - <b>Assign to footswitch (STOMP) A-D</b>: tick the footswitches that switch this block - <b>Split here (parallel paths)</b> (see Parallel paths); in a group: <b>Mix the paths</b>, <b>Remove path</b> - <b>Move</b>: left, right, to the start, to the end - <b>Insert an effect before / after</b>: opens ADD AN EFFECT at that place - <b>Remove</b>."),
       ("Bypass mode", "Switching a block OFF (click, footswitch in STOMP mode, preset recall) uses, when the plug-in has one, <b>its own on / off switch</b> (pedal bypass): the plug-in keeps running, so delay repeats and reverb tails die away naturally, like on a real pedalboard, without a click, and the pedal shows OFF in its own window. A plug-in without such a switch (for example CHROMA or most JS) is bypassed by <b>REAPER</b>: it stops completely and uses no CPU. Right-click a block &gt; <b>Bypass mode</b> to choose <i>Pedal bypass</i> or <i>REAPER bypass</i> for that plug-in (saved with the project). Presets remember the on / off state; with the pedal bypass, recalling a preset only sets the switch, the plug-in is not reloaded."),
       ("Right-click the screen", "On an empty part of the screen: <b>Add an effect</b>, <b>Save the chain...</b>, <b>Banks...</b>, the <b>MIDI</b> menu (learn, latching, reset) and <b>Plugin size</b>. Right-click outside the screen (on the pedal) gives the same MIDI and size menus."),
       ("Header", "From left to right: the preset label (1A ... 16F, or -- for a chain that is not a saved preset), its name with <b>EDITED</b> when the chain differs from it, the <b>status line</b> (track name, last action, warnings such as a missing plug-in), the <b>BANK</b> arrows, <b>PRESET</b> and <b>STOMP</b>, <b>SAVE</b> and <b>BANKS</b>."),
       ("Long chains", "Beyond 21 effects, the mouse wheel scrolls the rows; the rows shown appear at the bottom right (for example 1-3 / 4) with small arrows. When you drag a block near the top or bottom edge, the screen scrolls by itself."),
       ("Which chain is shown", "Octopus controls the chain it is inserted in: the track FX chain, or the record input FX chain if you insert it there. It finds itself in the chain with a token of its own (parameter <i>Octopus ID</i>), so you can rename it, move it or use several Octopus on different tracks. It never removes or moves itself."),
       ("Missing plug-ins", "If a preset contains a plug-in that is not installed on this computer, the status line names it and the other effects are loaded in their right place, in the right order.")],
 flow_t="9. Step by step",
 flow=[("Build your first preset", "1. Insert Octopus first on the guitar track. 2. Click <b>+</b>, click the plug-ins you want in the order of the chain (for example CHROMA, NGATE, Crycat, Doorn, Ampire, Impulse, ECHOFLEX, Holyverb), then <b>ADD</b>. 3. Set each block (double-click). 4. <b>SAVE</b> &gt; Save as... &gt; Bank 1 &gt; 1A. 5. Switch a few blocks on or off, change a delay time, and save it as 1B: switching between 1A and 1B is now almost instant."),
       ("Prepare a live set", "Give one bank to each song (or one preset per part of a song). Rename the presets in BANKS so that the footswitch cells show readable names, choose a colour for each footswitch in BANKS &gt; FOOTSWITCH COLOURS, and keep a copy of everything with EXPORT...."),
       ("Play with a MIDI pedalboard", "In REAPER, enable the MIDI input of the track and arm it (or turn monitoring on). Right-click footswitch A &gt; MIDI Learn and press the first switch of your pedalboard; do the same for B, C, D and, if you have more switches, for Bank down / up and the mode. With a Program Change pedalboard, nothing to learn: PC 1-64 recall presets A-D of each bank directly (1A-1D, 2A-2D...).")],
 inst_t="2. Installation",
 inst=[("Windows", "Copy <b>Octopus.vst3</b> (the whole folder) into <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Copy <b>Octopus.vst3</b> into <b>/Library/Audio/Plug-Ins/VST3</b> (or ~/Library/Audio/Plug-Ins/VST3). The build is signed ad hoc: if macOS blocks it, open System Settings &gt; Privacy &amp; Security and allow it, or run <i>xattr -cr</i> on the file."),
       ("REAPER", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Octopus appears as <b>VST3: Octopus (NINE HANDS | DSP)</b>. Insert it on the track you want to control, ideally in first position. REAPER 6.37 or newer is needed; the list of installed plug-ins used by the ADD screen needs REAPER 7.")],
 ov_t="3. Overview",
 ov_rows=[["#", "Element", "Role"],
   ["1", "Preset", "Preset currently loaded (1A ... 16F), in its colour, or -- for a live chain. Click it to choose the colour of this preset."],
   ["2", "Name / status line", "Preset name (click it to rename the preset; EDITED when the chain no longer matches it); below: the track name, or the last action and messages."],
   ["3", "BANK", "Bank shown by the footswitches (arrows: previous / next bank)."],
   ["4", "PRESET / STOMP", "Mode of the footswitches."],
   ["5", "SAVE", "Save the current chain in a preset."],
   ["6", "BANKS", "Opens the banks panel (load, save, rename, copy, export, footswitch colours...)."],
   ["7", "Block", "One effect of the track. Click: ON / OFF. Double-click: parameters. Drag: move. Right-click: menu."],
   ["8", "+ Add", "Adds an effect (list of the plug-ins installed in REAPER)."],
   ["9", "IN / OUT", "Signal flow of the track, from left to right, row after row. Red when the track is record-armed (blinking) or recording (steady), with REC under IN. Right-click: colour and glow."],
   ["10", "Footswitch cells", "A-F: what each footswitch does in the current mode (preset or blocks). A click works like the footswitch; right-click: its menu."],
   ["11", "Footswitches A-D", "Recall presets A-D of the bank shown (PRESET) or switch their blocks (STOMP); A+B = E, C+D = F."],
   ["12", "Letter A-D and ring", "Backlit letter and light ring inside the footswitch (around the button): lit for the loaded preset (in the colour chosen for that footswitch, blue by default) or for blocks that are ON (colour of the block). Between the footswitches: E, the tuner symbol and F, lit the same way."],
   ["13", "Mini CHROMA", "Note that comes out of Octopus (transposed by Semitones), live: green ring = in tune, red = out of tune. Click: tuner. Right-click: colours and glow."],
   ["14", "SEMITONES", "Transposition (ring) and Semitones preset. Click: drum of the 21 presets (wheel, click to choose). Right-click: save with the loaded preset, colour and glow."],
   ],
 ctl_t="4. The screen in detail",
 ctl=[("Blocks", "Each effect of the track is a block whose colour and icon show its family, guessed from the plug-in name: tuner, amp, cab / IR, drive, dynamics, EQ, wah / filter, delay, reverb, modulation, pitch, special, utility. A lit block with a coloured corner dot is ON; a grey block is bypassed. Small marks show a plug-in window that is open (square, top left), processing in parallel with the previous effect (||) and an offline plug-in (red line). The utility family is shown in white (never grey, so it cannot be mistaken for a block that is off). Octopus itself is not shown. Seven blocks per row and three rows on screen, with the same space between blocks across and down; with more effects, scroll with the mouse wheel (the row count appears bottom right)."),
      ("Mouse", "<b>Click</b> a block: ON / OFF (applied 0.4 s after you release it). <b>Double-click</b> (two presses on the same block within 0.65 s) never switches the effect: if a slow double-click arrives after the first click has already switched it, Octopus switches it back. Double-click: parameters on screen. <b>Drag</b> a block: a blue bar shows where it will go, release to move it (the screen scrolls when you drag near its top or bottom edge). <b>Right-click</b>: Bypass / Enable, Parameters on screen, Open / Close plug-in window, Assign to footswitch (STOMP) A-D, Process in parallel with previous, Move (left, right, to start, to end), Insert an effect before / after, Remove. The <b>Delete</b> key removes the block under the mouse. Every action is an undo point in REAPER (Edit &gt; Undo: <i>Octopus: ...</i>)."),
      ("Adding an effect", "Click the <b>+</b> block (or Insert before / after). The ADD AN EFFECT panel lists the plug-ins installed in REAPER (instruments hidden). Type in the search field and filter with <b>All, Favorites, NINE HANDS | DSP, VST3, VST, CLAP, JS, AU</b>. The <b>star</b> at the end of each line adds the plug-in to your favorites (yellow star) or removes it (empty star); <b>Favorites</b> shows only them. Favorites are shared by all your projects. <b>Click the plug-ins one after another</b>: each one is numbered in the order of your clicks (click again to remove it, CLEAR to start over), then press <b>ADD</b> (or Enter): they are all inserted in the track, in that order, where shown at the top (before a block or at the end), as a single undo point. A double-click adds right away; a right-click does nothing. Hover a <b>NINE HANDS | DSP</b> plug-in: after 0.8 s a bubble shows its family and a short description. RESCAN re-reads REAPER's list after installing new plug-ins; the mouse wheel scrolls the list."),
      ("Parameters", "The parameters panel shows 8 parameters per page with their name in large letters and the value as the plug-in displays it. Click or drag on a bar to set it; the wheel or the arrows change page. A continuous parameter moves smoothly; a parameter with positions (delay or amp modes, switches, MIDI channel...) is shown as a row of notches and moves <b>notch by notch</b>, never in between. Everything about MIDI (MIDI channel, MIDI learn, controllers...) is hidden: the <b>MIDI</b> button shows those parameters only, press it again to come back. <b>WINDOW</b> opens or closes the plug-in's own window; <b>BYPASS</b>, next to the close cross, switches the effect off (lit while bypassed). Values follow the plug-in live (also when you turn its knobs). Close with X or Escape."),
      ("Plug-in presets", "When a preset of the plug-in is loaded, its name is shown next to the plug-in name at the top of the parameters panel. In the parameters panel, <b>PRESETS</b> lists the presets of the plug-in as REAPER knows them: its <b>FACTORY</b> presets and your <b>USER</b> presets (saved in REAPER), numbered, with the current one highlighted and shown at the top. Click a preset to load it; the arrows load the previous / next one. To read the names, Octopus briefly loads each preset and then restores the plug-in exactly as it was (settings and displayed preset): do it while not playing. RELOAD LIST reads the list again after you save new presets. PRESETS again returns to the parameters. <b>SAVE USER</b> saves the current settings of the plug-in as a user preset of your own: type a name and press OK (or Enter; Escape cancels). These presets are kept by Octopus for that plug-in (in every project, file <i>NINE HANDS | DSP/Octopus/Plugin presets</i> in your user application data folder), appear at the top of the list with a star and the USER tag, and load with a click. A name already used is never replaced without confirmation. Right-click one of them: Load, Replace with the current settings, Rename, Delete. (REAPER does not let a plug-in add presets to its own preset menu, so these presets are listed in Octopus only.)"),
      ("Recording", "When the track that holds Octopus is <b>record-armed</b> in REAPER, <b>IN</b>, <b>OUT</b>, their dots and <b>REC</b> (under IN) turn red and <b>blink</b>; while REAPER <b>records</b>, they stay <b>steady</b>. Only this track counts: arming or recording another track changes nothing. Right-click IN, OUT or REC: colour of this indicator and its <b>glow</b> (0 to 200 %), shared by all your projects.")],
 scr_cap_add="ADD AN EFFECT panel, filtered on NINE HANDS | DSP plug-ins.",
 scr_cap_par="Parameters of a block.",
 scr_cap_pre="Presets of a block: your user presets saved by Octopus (star) on top, then REAPER's factory and user presets; SAVE USER, naming a new preset.",
 tun_t="Tuner (CHROMA)",
 tun=["<b>Built-in tuner</b>: send the MIDI command <b>Tuner on / off</b> (right-click the tuner symbol between B and C &gt; MIDI Learn, then press a switch of your pedalboard that is not one of A-D, for example a long press) or click the tuner symbol, and Octopus becomes a tuner with CHROMA's own screen and engine, <b>without any CHROMA plug-in in the chain</b>. While tuning, any footswitch (or the tuner command again, or EXIT) closes the tuner; the preset does not change. TUNINGS, MUTE (Octopus silences its output while you tune), MODE (MONO, POLY, STROBE) and the reference A (click <i>A=440</i>, 430 to 450 Hz) are saved with the project. The display has a fine resolution: 1.25 cent per mark in MONO, 2 cents in POLY.",
      "When a <b>CHROMA</b> block is switched on (click, footswitch in STOMP mode, MIDI...), <b>Octopus becomes the tuner</b>: its screen shows CHROMA's own screen, with the same tuning engine applied to the guitar that goes through Octopus. Mode (MONO, POLY, STROBE), reference A and tuning are read from CHROMA (the tuning chosen in CHROMA, for example G standard on a 7-string, gives the strings of the POLY view and the STRING number in MONO).",
      "The tuner screen is centred: at the top the mode, the tuning and the reference A; then the note, a fine meter from -50 to +50 cents whose marks fade towards the ends (white bar at the centre, the needle grows as it gets close and turns green, the size of the centre bar, when the note is in tune) and the offset in cents; in POLY, one fine meter per string, with the offset of each string on the right. The buttons are words at the bottom. <b>TUNINGS</b> (or a click on the tuning name in the screen) opens CHROMA's tuning presets: your favourites, the factory tunings by instrument (guitar 6, 7, 8 strings, bass...) and your own tunings made in CHROMA. The chosen tuning is used by Octopus right away and saved with the project; <i>Use the tuning set in CHROMA</i> goes back to CHROMA's own tuning, and changing the tuning in CHROMA itself does the same. At the bottom: <b>MUTE</b> mutes the output through CHROMA (red while muted), the mode button (<b>MONO</b>, <b>POLY</b> or <b>STROBE</b>) switches CHROMA to its next mode (or click the mode name at the top), a click on <b>A 440</b> sets the reference, <b>EXIT</b> switches the CHROMA block off and brings the chain back. Switching CHROMA off any other way does the same. Tip: keep Octopus first on the track so that it sees the guitar before any effect."],
 tun_cap="Octopus as a tuner: built-in tuner (MIDI command) in MONO (left), and POLY with the Drop D tuning chosen with TUNINGS (right).",
 semi_t="Semitones (transposition) and mini CHROMA",
 semi=["<b>Semitones</b>: the transposer of the Semitones pedal (NINE HANDS | DSP) is built into Octopus. The <b>SEMITONES</b> button, left of the bank, shows the transposition in a ring (arc from -12 to +12 semitones) and the name of the preset. Click it: the 21 Semitones presets turn on a vertical drum (Standard, Eb / D / C# / C / B / Bb Standard, Baritone A, Octave down, Octave doubler, Sub octave, Power fifth, Capo 1 / 2 / 3 / 5 / 7, Fourth and Fifth harmony, Octave up, 12-string). Scroll with the mouse wheel and click a preset to choose it; click elsewhere or press Esc to close without changing anything.",
       "The transposition is applied <b>at the input of Octopus</b>, before the whole chain, like a pitch pedal placed first, with no added latency. On <i>Standard (0)</i> the sound comes out exactly as it went in.",
       "It is <b>global</b>: changing preset does not change it. To keep a transposition with a preset, right-click SEMITONES &gt; <b>Save with preset 1A</b> (the loaded preset): that preset recalls its transposition each time it loads, and a choice made on the drum while it is loaded updates it. Untick to make the transposition global again. Right-click &gt; <b>Color &amp; Glow</b>: colour of the ring and of the drum, and opacity of their halo.",
       "<b>Mini CHROMA</b>: left of SEMITONES, a ring shows <b>the note that comes out of Octopus</b>, live: the note you play, transposed by Semitones (play E with D Standard: it shows D). The ring is <b>green when the note is in tune</b> (within 1 cent, as on CHROMA) and <b>red when it is not</b>, with red <b>chevrons</b> on its right: pointing <b>down</b> when you are below the note shown, <b>up</b> when you are above, one chevron when you are slightly off, two from 10 cents (they disappear when the note is in tune or when nothing sounds); grey with a dash when nothing is playing. Click it to open the tuner (the big tuner always shows the note actually played, to tune the guitar). Right-click: <b>Color &amp; Glow : in tune</b> and <b>Color &amp; Glow : out of tune</b>, with one glow for both. It listens while the Octopus window is open."],
 semi_cap="The SEMITONES drum open (D Standard selected); left of the SEMITONES button, the mini CHROMA.",

 par_t="Parallel paths (split / merge)",
 par=["Run <b>two or three paths side by side</b> and mix them, to stack two amps, an amp and a clean DI, or two amp + IR pairs. On the screen a group starts at a <b>Y</b> point and ends at an <b>M</b> point; its paths A, B, C are drawn one under the other, with blocks of the same size, and its row grows to make room.",
      "<b>Create a split</b>: right-click a block &gt; <b>Split here</b> (the block stays on path A, path B is empty with its <b>+</b> box), or <b>drag a block just under another one</b> (an orange SPLIT box shows where it lands, on path B). Dragging a block under the last path of a group (NEW PATH box) adds path C. Click the <b>+</b> of an empty path to add effects to it; inside a group, blocks are moved from one path to another, or out of the group, by drag and drop.",
      "<b>Click M</b>: the mixer of the group - level (-60 to +12 dB, click the value: 0 dB), pan, <b>phase</b> (stacked amps with their IRs can cancel each other: flip one) and mute of each path. <b>Click Y</b>: the group folds into one block (|| 2 PATHS), a click unfolds it. <b>Right-click Y or M</b>: mix, bypass the group (dry signal), fold, add / remove a path, merge the paths in series, assign the group to a footswitch (STOMP), or <b>switch path A / B with a footswitch</b>: each press lets only one path play (A, then B, then C...), the others are muted in the mixer with a 10 ms fade, no click; the footswitch is lit on B. A group never splits across two rows; a long chain scrolls.",
      "How it works in REAPER: Octopus places four small JS effects of its own around the paths (Nine Hands Split, Path B, Path C, Merge, installed in REAPER's Effects folder, NINE HANDS/Routing) and gives the track 8 channels; every plug-in stays on channels 1/2. Presets keep the groups and their mix. REAPER does not realign the paths when a plug-in of one path adds latency: prefer zero-latency amps and IRs in parallel paths."],
 par_cap="Two paths (amp + IR on each) between Y and M; right, the mixer opened by a click on M.",
 pre_t="5. Banks and presets",
 pre=["A preset is a <b>complete FX chain</b>: plug-ins, order, every setting, ON / OFF states, parallel processing and the STOMP assignments of the footswitches. It is stored in REAPER's own format, so every plug-in (VST, VST3, CLAP, JS, AU) comes back exactly as saved. Octopus keeps 96 presets (16 banks x 6: 1A to 16F) <b>inside the REAPER project</b>.",
      "<b>SAVE</b>: saves the loaded preset, or <i>Save as...</i> a slot chosen by bank. An existing preset is never replaced without confirmation. You can also right-click a footswitch &gt; <i>Save the current chain in ...</i>.",
      "<b>Recall</b>: footswitch (PRESET mode), click on a footswitch cell, LOAD in the banks panel, double-click a preset card, or MIDI Program Change. <b>Fast switching</b>: when the preset uses the same plug-ins in the same order as the current chain, Octopus only reloads the blocks whose settings differ and simply switches the others ON / OFF: changing between presets of one rig is almost instant. Otherwise the whole chain is replaced (Octopus itself is never touched). A plug-in that is no longer installed is reported on the status line; the other effects are loaded in their right place.",
      "<b>Empty preset</b>: recalling a preset that has never been saved (footswitch, footswitch cell, LOAD...) gives an <b>empty screen as at start-up</b>: every effect of the chain is removed (Octopus stays), the preset label shows that slot and you can build a new chain, then SAVE it there. REAPER's undo brings the previous chain back.",
      "<b>Preset colour</b>: click the preset label at the top left (1A...) and choose its colour in the colour picker (DEFAULT = colour of its footswitch). The colour picker shows 11 quick colours, a <b>MY COLOURS</b> row (click an empty dashed slot to save the current colour there, click a saved colour to use it, right-click to remove it; your colours are shared by all your projects) and three sliders: <b>HUE</b>, <b>SATURATION</b> and <b>BRIGHTNESS</b>, plus <b>GLOW (HALO OPACITY)</b> for the halo only. The colour changes live while you move them; the code (#RRGGBB) is shown in the preview. <b>DEFAULT</b> goes back to the default colour, <b>OK</b> (or a click beside the picker) closes it. The colour belongs to that preset only: label, letter, light ring and footswitch cell take it whenever the preset is in the bank shown (PRESET mode). It is saved with the project and in .octobank files.",
      "<b>Rename</b>: click the preset name at the top of the screen, type the new name, Enter (Escape cancels). RENAME in the BANKS panel does the same for any preset.",
      "<b>EDITED</b> appears next to the preset name when the chain has changed since it was loaded or saved; pressing the footswitch of that preset again restores it.",
      "<b>BANKS panel</b>: the 16 banks on the left (one dot per saved preset, none for an empty bank), the 6 presets (A-F) of the selected bank on the right with <b>LOAD, SAVE, RENAME, COPY, PASTE, CLEAR</b>. <b>EXPORT...</b> writes all banks to an <b>.octobank</b> file, <b>IMPORT...</b> replaces the 96 presets with a file (files of older versions keep their letters) (to share banks between projects). <b>PROGRAM CHANGE = PRESETS</b> switches MIDI Program Change on or off. <b>COLOURS AND GLOW</b>: A to F open the colour picker of each footswitch in PRESET mode (letter, ring, marking and screen cell; DEFAULT = blue; saved with the project). <b>TUNER</b> opens the picker of the tuner symbol: its colour (green by default); its GLOW is the common letter glow."],
 scr_cap_banks="BANKS panel.",
 fs_t="6. Footswitches, letters and modes",
 fs=["<b>PRESET mode</b>: footswitches A to F recall presets A-F of the bank shown in the header (E with A+B, F with C+D) (arrows or MIDI Bank down / up change the bank shown; the bank is loaded when you press a footswitch). The letter of the loaded preset lights up in the colour of its footswitch (blue by default, chosen in BANKS &gt; COLOURS AND GLOW).",
     "<b>STOMP mode</b>: each footswitch switches one or several blocks (right-click a block &gt; Assign to footswitch, or right-click a footswitch &gt; STOMP : blocks switched by...). If one of its blocks is ON, a press switches them all OFF; otherwise all ON. The footswitch cell shows the family and names of its blocks, its letter takes the colour of the first block and lights up when one of them is ON. Assignments are saved with the presets.",
     "Like on Echoflex, a footswitch acts as soon as it is pressed, its button darkens while it is held. Above it, the letter A, B, C or D is backlit: light grey when off, the letter itself lights up in colour, fading in and out, together with the light ring around the button.",
     "<b>Double presses</b>: pressing two neighbouring footswitches together (<b>A+B = E, C+D = F</b>) gives two more footswitches, printed on the pedal between the two footswitches: <b>E</b> and <b>F</b>. B+C does nothing: the tuner symbol between B and C is controlled by its own MIDI learn. A+B is footswitch <b>E</b> and C+D is footswitch <b>F</b>, exactly like A-D. Right-click the tuner symbol: <i>MIDI Learn : Tuner on / off</i>, <i>Clear MIDI</i>, latching, and <i>Color &amp; Glow</i>. A click on a marking runs it with the mouse. Every footswitch acts at once, without waiting: if its neighbour follows within 0.25 s, that first press is undone and the double press acts instead (in PRESET mode the preset of the double press is loaded). See the separate <b>MIDI and CC guide</b>.",
     "<b>Footswitches E and F</b>: E (A+B) and F (C+D) are footswitches like A-D, with their own cells on the screen and the same right-click menu: in PRESET mode they recall presets E and F of the bank (1E, 1F...), in STOMP mode they switch their blocks (right-click a block &gt; Assign to footswitch &gt; E / F). Their assignments are saved with each preset. <b>By MIDI</b>, nothing more to learn: E is the CC of A and the CC of B received together, F those of C and D (learn A, B, C and D on their footswitches; the E / F menu shows the CCs used). A CC already learned on a footswitch is never taken by another command.",
     "<b>Markings</b>: between A and B, between C and D, and between B and C, the pedal is printed with <b>] E [</b>, <b>] F [</b> and the tuner symbol. Each marking is a light: light grey when off, the E / F marking lights up like the letters A-D (colour of its preset in PRESET mode, of its first block in STOMP mode), and the tuner symbol lights up while the tuner is on (its side lines never light). <b>Glow</b>: the halo around the letters, the markings and the tuner symbol is set with the <b>GLOW (HALO OPACITY)</b> slider of the colour picker (right-click a footswitch &gt; Color &amp; Glow, 0 to 200 %, 100 % by default); it only changes the light around them, not their colour. The tuner symbol has its own colour (BANKS &gt; TUNER, or right-click the symbol &gt; Color &amp; Glow) and the same glow as the letters. These settings are shared by all your projects.",
     "Right-click a footswitch: save / load the preset of that footswitch, STOMP assignments, MIDI learn of the footswitch, latching mode, MIDI menu and <b>Color &amp; Glow</b> (colour picker of that footswitch with its GLOW slider). The plug-in size is in the right-click menu outside the footswitches."],
 stomp_cap="STOMP mode: footswitch A switches NGATE + Crycat; the letters take the colour of their blocks.",
 midi_t="7. MIDI",
 midi=["Octopus listens to the MIDI of its track (in REAPER: MIDI input on the track, record arm or monitoring on). The MIDI then continues to the following effects (for example your expression pedal for Crycat or Wharry)."],
 midi_hdr=["Command", "Default", "How"],
 midi_rows=[["Footswitch A, B, C, D", "none", "MIDI learn (right-click the footswitch &gt; MIDI Learn, then press the controller's switch)"],
            ["Bank down / Bank up", "none", "Right-click &gt; MIDI &gt; Learn"], ["PRESET / STOMP mode", "none", "Right-click &gt; MIDI &gt; Learn"],
            ["Tuner on / off", "none", "Right-click the tuner symbol &gt; MIDI Learn"], ["Presets A-D of each bank", "Program Change 1-64 (PC 1-4 = 1A-1D, PC 5-8 = 2A-2D...)", "On by default (PROGRAM CHANGE = PRESETS); E / F: double presses"],
            ["Footswitch E, F", "CC of A + B, C + D", "Automatic: the CCs learned on the two footswitches, received together"]],
 midi2=["<b>Momentary or latching</b>: a CC of 64 or more is a press. Momentary (default): an action when the switch goes ON (127 on press, 0 on release). Latching (right-click &gt; <i>MIDI footswitch latching</i>): for switches that alternate 127 / 0, every change is a press.",
        "A CC number drives only one command (learning it again moves it). Right-click &gt; MIDI &gt; <i>Reset MIDI</i> clears every assignment. Assignments are saved with the project.",
        "The plug-in parameters <b>Footswitch A-D, Footswitch E and F, Bank down, Bank up, PRESET / STOMP mode, Tuner on / off</b> can also be driven by REAPER's own MIDI learn or automation, with the same momentary / latching rule. The <b>Octopus ID</b> parameter is internal (it lets each Octopus find itself in the chain): never automate it.",
        "<b>M-Vave Chocolate Plus</b>: in Program Change mode its 4 switches recall presets directly (bank 1 = PC 1-4 = 1A-1D...). In CC mode, learn its switches on footswitches A-D and use STOMP / PRESET as on a hardware modeler."],
 win_t="8. Window",
 win=["The pedal fills the whole window (no jacks on the sides). Right-click outside the screen (or in the menus of the screen) &gt; <b>Plugin size</b>: <b>50 %</b>, <b>60 %</b>, <b>75 %</b> and <b>100 %</b>; the default size is marked <i>(default)</i>. <b>Default size (new windows)</b> chooses the size at which every Octopus window opens (60 % until you change it; the choice is common to all your projects and the current window takes it at once). You can also drag the bottom-right corner (proportions are kept), down to 50 %. A size chosen for one Octopus is saved with the project.",
      "The <b>status line</b> under the preset name shows the track name, or the last action (preset loaded, effect added, MIDI learn...)."],
 tips_t="10. Tips",
 tips=["Insert Octopus <b>first</b> on the guitar track; your tuner (CHROMA), noise gate, pedals, Ampire, Impulse and ambiences follow. Octopus passes the sound unchanged, so its position does not change the tone (with a Semitones transposition, everything after it is transposed, as with a pitch pedal placed first).",
       "For a live rig, build one chain with all the effects you need and save variations of it (blocks ON / OFF, different settings): switching between these presets is almost instant because only what differs is reloaded.",
       "Use STOMP mode inside a song to switch drive, delay or reverb groups, and PRESET mode between songs.",
       "Keep a backup of your banks with EXPORT... and IMPORT them in a new project.",
       "Use REAPER's undo freely: every Octopus action, including a preset recall, is a single undo point."],
 tech_t="11. Specifications",
 tech=[["Format", "VST3 (Windows 64-bit, macOS universal)"], ["Version", VERSION], ["Host", "REAPER 6.37 or newer (installed plug-in list: REAPER 7); inactive in other DAWs"],
       ["Audio", "Mono or stereo, passed through unchanged (except the Semitones transposition, -12 to +12 semitones at the input), 0 samples latency"], ["Chain", "Track FX chain or record input FX chain of the track where Octopus is inserted"],
       ["Presets", "16 banks x 6 presets (A-F), complete chains in REAPER's format, saved in the project, .octobank export / import; factory / user presets of each plug-in"],
       ["Tuner", "CHROMA screen and engine (MONO, POLY, STROBE) when a CHROMA block is on; mode, reference and tuning read from CHROMA; TUNINGS menu with CHROMA's tuning presets"],
       ["MIDI", "MIDI learn of footswitches A-D, bank down / up, mode; momentary or latching; Program Change 1-64; all channels; MIDI passed to the following effects"],
       ["Window", "50 %, 60 %, 75 %, 100 %; default size to choose (60 % at first), minimum 50 %; anti-freeze v2 on Windows (display caches rebuilt by a timer, watchdog, no host call from the audio thread)"]]),
"fr": dict(
 lang="FRANÇAIS", sub="Contrôleur de chaîne d'effets pour REAPER &nbsp;·&nbsp; VST3 pour Windows et macOS", ver="Notice d'utilisation &nbsp;·&nbsp; version " + VERSION,
 intro_t="1. Présentation",
 intro=["<b>OCTOPUS</b> n'est pas un effet : c'est un <b>écran de commande de la chaîne d'effets d'une piste REAPER</b>, présenté comme l'écran d'un multi-effet moderne au sol (dans l'esprit du Quad Cortex mini). Insérez-le sur votre piste guitare : son écran affiche chaque effet de cette piste sous forme de bloc coloré, de IN à OUT.",
        "Tout ce que vous faites sur l'écran d'Octopus se fait <b>en même temps dans la chaîne d'effets de la piste</b> : allumer ou couper un bloc, ajouter un effet, le supprimer, le déplacer, régler ses paramètres. Et ce que vous faites directement dans REAPER apparaît un instant plus tard sur l'écran d'Octopus.",
        "Octopus garde en plus <b>16 banks de 6 presets</b> (1A à 16F) - chaque preset est une chaîne complète avec ses plugins, leur ordre et tous leurs réglages - et vous donne <b>6 footswitchs</b> (A-D, plus E = A+B et F = C+D) pour rappeler les presets (mode PRESET) ou basculer des groupes de blocs (mode STOMP), à la souris ou avec un pédalier MIDI comme la M-Vave Chocolate Plus. Quand le bloc accordeur CHROMA est allumé, Octopus affiche l'écran de CHROMA et devient l'accordeur.",
        "Le son traverse Octopus sans modification (latence nulle), sauf si vous choisissez une transposition avec Semitones. Octopus fonctionne <b>uniquement dans REAPER</b> : REAPER donne aux plugins VST3 l'accès à leur piste et à son API, ce que les autres DAW ne font pas. Dans un autre DAW, l'écran l'indique simplement."],
 feat_t="Fonctions en bref",
 feat=["<b>Vue en direct de la chaîne d'effets de la piste</b> : chaque insert sous forme de bloc coloré (famille, icône, ON / OFF, fenêtre ouverte, parallèle, hors ligne), 7 blocs par rangée, 3 rangées à l'écran, défilement au-delà.",
       "<b>Voies parallèles</b> : la chaîne se sépare en 2 ou 3 voies (deux amplis, paires ampli + IR...) avec un mixeur par voie (niveau, pan, phase, mute).",
       "<b>Tout se pilote depuis l'écran</b> : allumer / couper, ajouter un ou plusieurs effets d'un coup, supprimer, déplacer par glisser-déposer, insérer avant / après, traiter en parallèle, ouvrir la fenêtre du plugin.",
       "<b>Paramètres à l'écran</b> : 8 par page, noms en grand, curseurs continus pour les valeurs libres et curseurs à crans pour les modes et interrupteurs, paramètres MIDI derrière un bouton MIDI, BYPASS à côté de la croix.",
       "<b>Presets des plugins</b> : presets d'usine et utilisateur de REAPER pour chaque plugin, plus vos propres presets utilisateur enregistrés par Octopus (SAVE USER), avec chargement, remplacement, renommage et suppression.",
       "<b>96 presets de chaîne</b> (16 banks x 6) enregistrés dans le projet, changement rapide quand seuls les réglages diffèrent, copier / coller, renommer, export / import des banks.",
       "<b>6 footswitchs</b> A-F en mode PRESET ou STOMP (E = A+B, F = C+D), chacun avec sa lettre rétroéclairée ou son marquage dans la couleur choisie (nuancier avec opacité du halo), ou celle de ses blocs en mode STOMP.",
       "<b>Accordeur intégré</b> : allumez un bloc CHROMA et Octopus devient l'accordeur (MONO, POLY, STROBE), avec les presets d'accordage de CHROMA.",
       "<b>Semitones intégré</b> : transposition de -12 à +12 demi-tons en entrée (21 presets sur un tambour vertical : accordages bas, capo, octaves, harmonies), globale ou enregistrée avec un preset ; <b>mini CHROMA</b> dans l'en-tête, qui affiche la note qui sort, en vert quand elle est juste, en rouge sinon.",
       "<b>Pédalier MIDI</b> : MIDI learn de chaque footswitch, des commandes de bank et du mode, momentané ou à accrochage, Program Change 1-64 pour les presets A-D de chaque bank.",
       "<b>Sûr par conception</b> : chaque action est un seul point d'annulation de REAPER, le son passe sans modification (sauf transposition de Semitones), aucun appel à l'hôte depuis le thread audio, fenêtre en 4 tailles (50 à 100 %) avec une taille par défaut à votre choix."],
 more_t="Menus et détails",
 more=[("Clic droit sur un bloc", "<b>Bypass / Enable</b> - <b>Parameters on screen</b> (paramètres à l'écran) - <b>Open / Close plug-in window</b> (fenêtre flottante de REAPER) - <b>Assign to footswitch (STOMP) A-D</b> : cochez les footswitchs qui basculent ce bloc - <b>Split here (parallel paths)</b> (voir Voies parallèles) ; dans un groupe : <b>Mix the paths</b>, <b>Remove path</b> - <b>Move</b> : à gauche, à droite, au début, à la fin - <b>Insert an effect before / after</b> : ouvre ADD AN EFFECT à cet endroit - <b>Remove</b> (supprimer)."),
       ("Mode de bypass", "Couper un bloc (clic, footswitch en mode STOMP, rappel de preset) passe, quand le plugin en a un, par <b>son propre interrupteur</b> (bypass de la pédale) : le plugin continue de tourner, les répétitions d'un delay et la queue d'une reverb s'éteignent naturellement comme sur un vrai pedalboard, sans clic, et la pédale s'affiche éteinte dans sa propre fenêtre. Un plugin sans interrupteur (par exemple CHROMA ou la plupart des JS) est coupé par <b>REAPER</b> : il s'arrête complètement et ne consomme plus de processeur. Clic droit sur un bloc &gt; <b>Bypass mode</b> pour choisir <i>Pedal bypass</i> ou <i>REAPER bypass</i> pour ce plugin (enregistré avec le projet). Les presets gardent l'état ON / OFF ; avec le bypass de la pédale, le rappel d'un preset ne fait que régler l'interrupteur, le plugin n'est pas rechargé."),
       ("Clic droit sur l'écran", "Sur une partie vide de l'écran : <b>Add an effect</b>, <b>Save the chain...</b>, <b>Banks...</b>, le menu <b>MIDI</b> (learn, accrochage, remise à zéro) et <b>Plugin size</b>. Un clic droit hors de l'écran (sur la pédale) donne les mêmes menus MIDI et taille."),
       ("En-tête", "De gauche à droite : l'étiquette du preset (1A ... 16F, ou -- pour une chaîne qui n'est pas un preset enregistré), son nom avec <b>EDITED</b> quand la chaîne en diffère, la <b>ligne d'état</b> (nom de la piste, dernière action, avertissements comme un plugin manquant), les flèches <b>BANK</b>, <b>PRESET</b> et <b>STOMP</b>, <b>SAVE</b> et <b>BANKS</b>."),
       ("Longues chaînes", "Au-delà de 21 effets, la molette fait défiler les rangées ; les rangées affichées s'indiquent en bas à droite (par exemple 1-3 / 4) avec de petites flèches. Quand vous glissez un bloc près du bord haut ou bas, l'écran défile tout seul."),
       ("Quelle chaîne est affichée", "Octopus pilote la chaîne dans laquelle il est inséré : la chaîne d'effets de la piste, ou la chaîne d'effets d'entrée (enregistrement) si vous l'y insérez. Il se retrouve dans la chaîne grâce à un jeton qui lui est propre (paramètre <i>Octopus ID</i>) : vous pouvez le renommer, le déplacer ou utiliser plusieurs Octopus sur des pistes différentes. Il ne se supprime ni ne se déplace jamais lui-même."),
       ("Plugins manquants", "Si un preset contient un plugin qui n'est pas installé sur cet ordinateur, la ligne d'état le nomme et les autres effets sont chargés à leur place, dans le bon ordre.")],
 flow_t="9. Pas à pas",
 flow=[("Créer votre premier preset", "1. Insérez Octopus en premier sur la piste guitare. 2. Cliquez sur <b>+</b>, cliquez les plugins voulus dans l'ordre de la chaîne (par exemple CHROMA, NGATE, Crycat, Doorn, Ampire, Impulse, ECHOFLEX, Holyverb), puis <b>ADD</b>. 3. Réglez chaque bloc (double-clic). 4. <b>SAVE</b> &gt; Save as... &gt; Bank 1 &gt; 1A. 5. Allumez ou coupez quelques blocs, changez un temps de delay et enregistrez en 1B : passer de 1A à 1B est maintenant quasi instantané."),
       ("Préparer un concert", "Donnez une bank à chaque morceau (ou un preset par partie de morceau). Renommez les presets dans BANKS pour que les cases des footswitchs affichent des noms lisibles, choisissez une couleur par footswitch dans BANKS &gt; FOOTSWITCH COLOURS, et gardez une copie de tout avec EXPORT...."),
       ("Jouer avec un pédalier MIDI", "Dans REAPER, activez l'entrée MIDI de la piste et armez-la (ou activez le monitoring). Clic droit sur le footswitch A &gt; MIDI Learn, puis appuyez sur le premier commutateur du pédalier ; faites de même pour B, C, D et, si vous avez plus de commutateurs, pour Bank down / up et le mode. Avec un pédalier en Program Change, rien à apprendre : PC 1-64 rappellent directement les presets A-D de chaque bank (1A-1D, 2A-2D...).")],
 inst_t="2. Installation",
 inst=[("Windows", "Copiez <b>Octopus.vst3</b> (tout le dossier) dans <b>C:\\Program Files\\Common Files\\VST3</b>."),
       ("macOS", "Copiez <b>Octopus.vst3</b> dans <b>/Library/Audio/Plug-Ins/VST3</b> (ou ~/Library/Audio/Plug-Ins/VST3). Le plugin est signé localement : si macOS le bloque, autorisez-le dans Réglages Système &gt; Confidentialité et sécurité, ou lancez <i>xattr -cr</i> sur le fichier."),
       ("REAPER", "Options &gt; Preferences &gt; Plug-ins &gt; VST &gt; <b>Re-scan</b>. Octopus apparaît sous le nom <b>VST3: Octopus (NINE HANDS | DSP)</b>. Insérez-le sur la piste à piloter, de préférence en première position. Il faut REAPER 6.37 ou plus récent ; la liste des plugins installés utilisée par l'écran d'ajout demande REAPER 7.")],
 ov_t="3. Vue d'ensemble",
 ov_rows=[["#", "Élément", "Rôle"],
   ["1", "Preset", "Preset chargé (1A ... 16F), dans sa couleur, ou -- pour une chaîne libre. Cliquez dessus pour choisir la couleur de ce preset."],
   ["2", "Nom / ligne d'état", "Nom du preset (cliquez dessus pour le renommer ; EDITED quand la chaîne ne lui correspond plus) ; dessous : le nom de la piste, ou la dernière action et les messages."],
   ["3", "BANK", "Bank affichée sur les footswitchs (flèches : bank précédente / suivante)."],
   ["4", "PRESET / STOMP", "Mode des footswitchs."],
   ["5", "SAVE", "Enregistre la chaîne actuelle dans un preset."],
   ["6", "BANKS", "Ouvre le panneau des banks (charger, enregistrer, renommer, copier, exporter, couleurs des footswitchs...)."],
   ["7", "Bloc", "Un effet de la piste. Clic : ON / OFF. Double-clic : paramètres. Glisser : déplacer. Clic droit : menu."],
   ["8", "+ Add", "Ajoute un effet (liste des plugins installés dans REAPER)."],
   ["9", "IN / OUT", "Trajet du signal de la piste, de gauche à droite, rangée après rangée. En rouge quand la piste est armée (clignote) ou en enregistrement (fixe), avec REC sous IN. Clic droit : couleur et glow."],
   ["10", "Cases des footswitchs", "A-F : ce que fait chaque footswitch dans le mode actuel (preset ou blocs). Un clic agit comme le footswitch ; clic droit : son menu."],
   ["11", "Footswitchs A-D", "Rappellent les presets A-D de la bank affichée (PRESET) ou basculent leurs blocs (STOMP) ; A+B = E, C+D = F."],
   ["12", "Lettre A-D et anneau", "Lettre rétroéclairée et anneau lumineux dans le footswitch (autour du bouton) : allumés pour le preset chargé (dans la couleur choisie pour ce footswitch, bleu par défaut) ou pour les blocs allumés (couleur du bloc). Entre les footswitchs : E, le symbole de l'accordeur et F, éclairés de la même façon."],
   ["13", "Mini CHROMA", "Note qui sort d'Octopus (transposée par Semitones), en direct : anneau vert = juste, rouge = faux. Clic : accordeur. Clic droit : couleurs et halo."],
   ["14", "SEMITONES", "Transposition (anneau) et preset de Semitones. Clic : tambour des 21 presets (molette, clic pour choisir). Clic droit : enregistrer avec le preset chargé, couleur et halo."],
   ],
 ctl_t="4. L'écran en détail",
 ctl=[("Blocs", "Chaque effet de la piste est un bloc dont la couleur et l'icône indiquent la famille, devinée d'après le nom du plugin : accordeur, ampli, baffle / IR, drive, dynamique, EQ, wah / filtre, delay, reverb, modulation, pitch, spécial, utilitaire. Un bloc coloré avec son point allumé est ON ; un bloc gris est contourné. De petits repères signalent une fenêtre de plugin ouverte (carré en haut à gauche), un traitement en parallèle avec l'effet précédent (||) et un plugin hors ligne (trait rouge). La famille utilitaire est en blanc (jamais en gris, pour ne pas la confondre avec un bloc éteint). Octopus lui-même n'est pas affiché. Sept blocs par rangée et trois rangées à l'écran, avec le même espace entre les blocs en largeur et en hauteur ; au-delà, faites défiler avec la molette (le nombre de rangées s'affiche en bas à droite)."),
      ("Souris", "<b>Clic</b> sur un bloc : ON / OFF (appliqué 0,4 s après le relâchement). Le <b>double-clic</b> (deux appuis sur le même bloc en moins de 0,65 s) ne bascule jamais l'effet : si un double-clic lent arrive après la bascule du premier clic, Octopus la défait. <b>Double-clic</b> : paramètres à l'écran. <b>Glisser</b> un bloc : une barre bleue montre où il ira, relâchez pour le déplacer (l'écran défile si vous glissez près de son bord haut ou bas). <b>Clic droit</b> : Bypass / Enable, Parameters on screen, Open / Close plug-in window, Assign to footswitch (STOMP) A-D, Process in parallel with previous, Move (gauche, droite, au début, à la fin), Insert an effect before / after, Remove. La touche <b>Suppr</b> supprime le bloc sous la souris. Chaque action est un point d'annulation dans REAPER (Edit &gt; Undo : <i>Octopus: ...</i>)."),
      ("Ajouter un effet", "Cliquez sur le bloc <b>+</b> (ou Insert before / after). Le panneau ADD AN EFFECT liste les plugins installés dans REAPER (instruments masqués). Tapez dans le champ de recherche et filtrez avec <b>All, Favorites, NINE HANDS | DSP, VST3, VST, CLAP, JS, AU</b>. L'<b>étoile</b> en bout de ligne ajoute le plugin à vos favoris (étoile jaune) ou l'en retire (étoile vide) ; <b>Favorites</b> n'affiche qu'eux. Les favoris sont communs à tous vos projets. <b>Cliquez les plugins les uns après les autres</b> : chacun est numéroté dans l'ordre de vos clics (nouveau clic pour le retirer, CLEAR pour tout recommencer), puis appuyez sur <b>ADD</b> (ou Entrée) : ils sont tous insérés dans la piste, dans cet ordre, à l'endroit indiqué en haut (devant un bloc ou à la fin), en un seul point d'annulation. Un double-clic ajoute tout de suite ; un clic droit ne fait rien. Au survol d'un plugin <b>NINE HANDS | DSP</b>, une bulle affiche au bout de 0,8 s sa famille et une courte description. RESCAN relit la liste de REAPER après l'installation de nouveaux plugins ; la molette fait défiler la liste."),
      ("Paramètres", "Le panneau des paramètres montre 8 paramètres par page avec leur nom en grand et leur valeur telle que le plugin l'affiche. Cliquez ou glissez sur une barre pour régler ; la molette ou les flèches changent de page. Un paramètre continu se règle en douceur ; un paramètre à positions (modes d'un delay ou d'un ampli, interrupteurs, canal MIDI...) s'affiche en une rangée de crans et se règle <b>cran par cran</b>, jamais entre deux. Tout ce qui concerne le MIDI (canal MIDI, MIDI learn, contrôleurs...) est caché : le bouton <b>MIDI</b> affiche seulement ces paramètres, un nouvel appui revient aux autres. <b>WINDOW</b> ouvre ou ferme la fenêtre du plugin lui-même ; <b>BYPASS</b>, à côté de la croix de fermeture, coupe l'effet (allumé tant qu'il est contourné). Les valeurs suivent le plugin en direct (même quand vous tournez ses boutons). Fermez avec X ou Échap."),
      ("Presets du plugin", "Quand un preset du plugin est chargé, son nom s'affiche à côté du nom du plugin en haut du panneau des paramètres. Dans le panneau des paramètres, <b>PRESETS</b> liste les presets du plugin tels que REAPER les connaît : ses presets d'usine (<b>FACTORY</b>) et vos presets utilisateur (<b>USER</b>, enregistrés dans REAPER), numérotés, le preset actif surligné et rappelé en haut. Cliquez sur un preset pour le charger ; les flèches chargent le précédent / le suivant. Pour lire les noms, Octopus charge un instant chaque preset puis rétablit le plugin exactement comme il était (réglages et preset affiché) : faites-le hors lecture. RELOAD LIST relit la liste après l'enregistrement de nouveaux presets. Un nouveau clic sur PRESETS revient aux paramètres. <b>SAVE USER</b> enregistre les réglages actuels du plugin comme preset utilisateur à vous : tapez un nom puis OK (ou Entrée ; Échap annule). Ces presets sont gardés par Octopus pour ce plugin (dans tous les projets, fichier <i>NINE HANDS | DSP/Octopus/Plugin presets</i> du dossier de données utilisateur), s'affichent en tête de liste avec une étoile et l'étiquette USER, et se chargent d'un clic. Un nom déjà utilisé n'est jamais remplacé sans confirmation. Clic droit sur l'un d'eux : Load, Replace with the current settings, Rename, Delete. (REAPER ne permet pas d'ajouter un preset au menu de presets du plugin lui-même : ces presets sont listés dans Octopus seulement.)"),
      ("Enregistrement", "Quand la piste qui contient Octopus est <b>armée</b> dans REAPER, <b>IN</b>, <b>OUT</b>, leurs points et <b>REC</b> (sous IN) passent en rouge et <b>clignotent</b> ; pendant que REAPER <b>enregistre</b>, ils restent <b>fixes</b>. Seule cette piste compte : armer ou enregistrer une autre piste ne change rien. Clic droit sur IN, OUT ou REC : couleur de ce voyant et son <b>glow</b> (0 à 200 %), communs à tous vos projets.")],
 scr_cap_add="Panneau ADD AN EFFECT, filtré sur les plugins NINE HANDS | DSP.",
 scr_cap_par="Paramètres d'un bloc.",
 scr_cap_pre="Presets d'un bloc : vos presets enregistrés par Octopus (étoile) en tête, puis les presets d'usine et utilisateur de REAPER ; SAVE USER, saisie du nom d'un nouveau preset.",
 tun_t="Accordeur (CHROMA)",
 tun=["<b>Accordeur intégré</b> : envoyez la commande MIDI <b>Tuner on / off</b> (clic droit sur le symbole de l'accordeur entre B et C &gt; MIDI Learn, puis appui sur un commutateur de votre pédalier qui n'est pas A-D, par exemple un appui long) ou cliquez sur le symbole, et Octopus devient l'accordeur, avec l'écran et le moteur de CHROMA, <b>sans aucun plugin CHROMA dans la chaîne</b>. Pendant l'accordage, n'importe quel footswitch (ou la commande de l'accordeur de nouveau, ou EXIT) ferme l'accordeur ; le preset ne change pas. TUNINGS, MUTE (Octopus coupe sa sortie pendant l'accordage), MODE (MONO, POLY, STROBE) et le La de référence (clic sur <i>A=440</i>, de 430 à 450 Hz) sont enregistrés avec le projet. L'affichage est fin : 1,25 cent par graduation en MONO, 2 cents en POLY.",
      "Quand un bloc <b>CHROMA</b> est allumé (clic, footswitch en mode STOMP, MIDI...), <b>Octopus devient l'accordeur</b> : son écran affiche l'écran de CHROMA lui-même, avec le même moteur d'accordage appliqué à la guitare qui traverse Octopus. Le mode (MONO, POLY, STROBE), le La de référence et l'accordage sont lus dans CHROMA (l'accordage choisi dans CHROMA, par exemple G standard sur une 7 cordes, donne les cordes de la vue POLY et le numéro de corde STRING en MONO).",
      "L'écran de l'accordeur est centré : en haut le mode, l'accordage et le La de référence ; puis la note, un mètre fin de -50 à +50 cents dont les traits s'effacent vers les bouts (barre blanche au centre ; l'aiguille grandit en s'en approchant et devient verte, de la taille de la barre du centre, quand la note est juste) et l'écart en cents ; en POLY, un mètre fin par corde, avec l'écart de chaque corde à droite. Les boutons sont des mots en bas. <b>TUNINGS</b> (ou un clic sur le nom de l'accordage dans l'écran) ouvre les presets d'accordage de CHROMA : vos favoris, les accordages d'usine par instrument (guitare 6, 7, 8 cordes, basse...) et vos accordages personnels créés dans CHROMA. L'accordage choisi est utilisé tout de suite par Octopus et enregistré avec le projet ; <i>Use the tuning set in CHROMA</i> revient à l'accordage de CHROMA, et changer l'accordage dans CHROMA lui-même fait de même. En bas : <b>MUTE</b> coupe la sortie par CHROMA (en rouge tant que c'est coupé), le bouton du mode (<b>MONO</b>, <b>POLY</b> ou <b>STROBE</b>) passe CHROMA au mode suivant (ou cliquez sur le nom du mode en haut), un clic sur <b>A 440</b> règle le La de référence, <b>EXIT</b> éteint le bloc CHROMA et ramène la chaîne. Éteindre CHROMA autrement fait la même chose. Conseil : gardez Octopus en premier sur la piste pour qu'il entende la guitare avant tout effet."],
 tun_cap="Octopus en accordeur : accordeur intégré (commande MIDI) en MONO (à gauche), et en POLY avec l'accordage Drop D choisi par TUNINGS (à droite).",
 semi_t="Semitones (transposition) et mini CHROMA",
 semi=["<b>Semitones</b> : le transpositeur de la pédale Semitones (NINE HANDS | DSP) est intégré à Octopus. Le bouton <b>SEMITONES</b>, à gauche de la bank, affiche la transposition dans un anneau (arc de -12 à +12 demi-tons) et le nom du preset. Cliquez dessus : les 21 presets de Semitones tournent sur un tambour vertical (Standard, Eb / D / C# / C / B / Bb Standard, Baritone A, Octave down, Octave doubler, Sub octave, Power fifth, Capo 1 / 2 / 3 / 5 / 7, Fourth et Fifth harmony, Octave up, 12-string). Faites défiler avec la molette et cliquez un preset pour le choisir ; un clic ailleurs ou Échap ferme sans rien changer.",
       "La transposition se fait <b>à l'entrée d'Octopus</b>, avant toute la chaîne, comme une pédale de transposition placée en premier, sans latence ajoutée. Sur <i>Standard (0)</i>, le son ressort exactement comme il est entré.",
       "Elle est <b>globale</b> : changer de preset ne la change pas. Pour garder une transposition avec un preset, clic droit sur SEMITONES &gt; <b>Save with preset 1A</b> (le preset chargé) : ce preset rappelle sa transposition à chaque chargement, et un choix fait sur le tambour pendant qu'il est chargé la met à jour. Décochez pour que la transposition redevienne globale. Clic droit &gt; <b>Color &amp; Glow</b> : couleur de l'anneau et du tambour, et opacité de leur halo.",
       "<b>Mini CHROMA</b> : à gauche de SEMITONES, un anneau affiche <b>la note qui sort d'Octopus</b>, en direct : la note jouée, transposée par Semitones (un Mi joué avec D Standard affiche D). L'anneau est <b>vert quand la note est juste</b> (à 1 cent près, comme sur CHROMA) et <b>rouge quand elle ne l'est pas</b>, avec à sa droite des <b>chevrons</b> rouges : vers le <b>bas</b> quand vous êtes en dessous de la note affichée, vers le <b>haut</b> quand vous êtes au-dessus, un chevron quand c'est un peu faux, deux à partir de 10 cents (ils disparaissent quand la note est juste ou quand rien ne sonne) ; gris avec un tiret quand rien ne joue. Un clic ouvre l'accordeur (le grand accordeur affiche toujours la note réellement jouée, pour accorder la guitare). Clic droit : <b>Color &amp; Glow : in tune</b> et <b>Color &amp; Glow : out of tune</b>, avec un halo commun aux deux. Il écoute tant que la fenêtre d'Octopus est ouverte."],
 semi_cap="Le tambour de SEMITONES ouvert (D Standard choisi) ; à gauche du bouton SEMITONES, le mini CHROMA.",

 par_t="Voies parallèles (split / merge)",
 par=["Faites passer le son par <b>deux ou trois voies côte à côte</b> et mélangez-les, pour empiler deux amplis, un ampli et un son clair (DI), ou deux paires ampli + IR. À l'écran, un groupe commence à un point <b>Y</b> et finit à un point <b>M</b> ; ses voies A, B, C sont dessinées l'une sous l'autre, avec des blocs de même taille, et sa rangée s'agrandit pour leur faire de la place.",
      "<b>Créer un split</b> : clic droit sur un bloc &gt; <b>Split here</b> (le bloc reste en voie A, la voie B est vide avec sa case <b>+</b>), ou <b>glissez un bloc juste sous un autre</b> (une case orange SPLIT montre où il se pose, en voie B). Glisser un bloc sous la dernière voie d'un groupe (case NEW PATH) ajoute la voie C. Cliquez sur le <b>+</b> d'une voie vide pour y ajouter des effets ; dans un groupe, les blocs se déplacent d'une voie à l'autre, ou hors du groupe, par glisser-déposer.",
      "<b>Clic sur M</b> : le mixeur du groupe - niveau (-60 à +12 dB, clic sur la valeur : 0 dB), pan, <b>phase</b> (deux amplis empilés avec leurs IR peuvent s'annuler : inversez-en un) et mute de chaque voie. <b>Clic sur Y</b> : le groupe se replie en un seul bloc (|| 2 PATHS), un clic le déplie. <b>Clic droit sur Y ou M</b> : mixer, couper le groupe (signal sec), replier, ajouter / retirer une voie, remettre les voies en série, assigner le groupe à un footswitch (STOMP), ou <b>passer de la voie A à la voie B au footswitch</b> : chaque appui ne laisse sonner qu'une voie (A, puis B, puis C...), les autres sont coupées dans le mixeur avec un fondu de 10 ms, sans clic ; le footswitch est allumé sur B. Un groupe ne se coupe jamais entre deux rangées ; une chaîne longue défile.",
      "Dans REAPER : Octopus place autour des voies quatre petits effets JS à lui (Nine Hands Split, Path B, Path C, Merge, installés dans le dossier Effects de REAPER, NINE HANDS/Routing) et passe la piste à 8 canaux ; tous les plugins restent sur les canaux 1/2. Les presets gardent les groupes et leur mixage. REAPER ne recale pas les voies quand un plugin d'une voie ajoute de la latence : préférez des amplis et des IR sans latence dans les voies parallèles."],
 par_cap="Deux voies (ampli + IR sur chacune) entre Y et M ; à droite, le mixeur ouvert par un clic sur M.",
 pre_t="5. Banks et presets",
 pre=["Un preset est une <b>chaîne d'effets complète</b> : plugins, ordre, tous les réglages, états ON / OFF, traitement parallèle et assignations STOMP des footswitchs. Il est gardé au format de REAPER lui-même : chaque plugin (VST, VST3, CLAP, JS, AU) revient exactement comme il a été enregistré. Octopus garde 96 presets (16 banks x 6 : 1A à 16F) <b>dans le projet REAPER</b>.",
      "<b>SAVE</b> : enregistre le preset chargé, ou <i>Save as...</i> dans un emplacement choisi par bank. Un preset existant n'est jamais remplacé sans confirmation. Vous pouvez aussi faire un clic droit sur un footswitch &gt; <i>Save the current chain in ...</i>.",
      "<b>Rappel</b> : footswitch (mode PRESET), clic sur une case de footswitch, LOAD dans le panneau des banks, double-clic sur une carte de preset, ou Program Change MIDI. <b>Changement rapide</b> : si le preset utilise les mêmes plugins dans le même ordre que la chaîne actuelle, Octopus ne recharge que les blocs dont les réglages diffèrent et se contente d'allumer ou couper les autres : passer d'un preset à l'autre d'un même rig est quasi instantané. Sinon toute la chaîne est remplacée (Octopus lui-même n'est jamais touché). Un plugin qui n'est plus installé est signalé sur la ligne d'état ; les autres effets sont chargés à leur place.",
      "<b>Preset vide</b> : rappeler un preset jamais enregistré (footswitch, case de footswitch, LOAD...) donne un <b>écran vide comme au démarrage</b> : tous les effets de la chaîne sont retirés (Octopus reste), l'étiquette affiche cet emplacement et vous pouvez construire une nouvelle chaîne puis l'y enregistrer avec SAVE. L'annulation de REAPER ramène la chaîne précédente.",
      "<b>Couleur du preset</b> : cliquez sur l'étiquette du preset en haut à gauche (1A...) et choisissez sa couleur dans le nuancier (DEFAULT = couleur de son footswitch). Le nuancier propose 11 couleurs rapides, une rangée <b>MY COLOURS</b> (clic sur un emplacement vide en pointillés pour y enregistrer la couleur en cours, clic sur une couleur enregistrée pour l'utiliser, clic droit pour l'effacer ; vos couleurs servent dans tous vos projets) et trois curseurs : <b>HUE</b> (teinte), <b>SATURATION</b> et <b>BRIGHTNESS</b> (luminosité), plus <b>GLOW (HALO OPACITY)</b> pour le halo seul. La couleur change en direct pendant que vous les déplacez ; son code (#RRVVBB) s'affiche dans l'aperçu. <b>DEFAULT</b> revient à la couleur par défaut, <b>OK</b> (ou un clic à côté du nuancier) le ferme. La couleur n'appartient qu'à ce preset : étiquette, lettre, anneau lumineux et case du footswitch la prennent quand ce preset est dans la bank affichée (mode PRESET). Elle est enregistrée avec le projet et dans les fichiers .octobank.",
      "<b>Renommer</b> : cliquez sur le nom du preset en haut de l'écran, tapez le nouveau nom, Entrée (Échap annule). RENAME dans le panneau BANKS fait la même chose pour n'importe quel preset.",
      "<b>EDITED</b> s'affiche à côté du nom quand la chaîne a changé depuis le chargement ou l'enregistrement du preset ; appuyer de nouveau sur le footswitch de ce preset le rétablit.",
      "<b>Panneau BANKS</b> : les 16 banks à gauche (un point par preset enregistré, aucun pour une bank vide), les 6 presets (A-F) de la bank choisie à droite avec <b>LOAD, SAVE, RENAME, COPY, PASTE, CLEAR</b>. <b>EXPORT...</b> écrit toutes les banks dans un fichier <b>.octobank</b>, <b>IMPORT...</b> remplace les 96 presets par ceux d'un fichier (les fichiers des versions précédentes gardent leurs lettres) (pour partager des banks entre projets). <b>PROGRAM CHANGE = PRESETS</b> active ou coupe les Program Change MIDI. <b>COLOURS AND GLOW</b> : A à F ouvrent le nuancier de chaque footswitch en mode PRESET (lettre, anneau, marquage et case de l'écran ; DEFAULT = bleu ; enregistré avec le projet). <b>TUNER</b> ouvre le nuancier du symbole de l'accordeur : sa couleur (vert par défaut) ; son GLOW est le halo commun des lettres."],
 scr_cap_banks="Panneau BANKS.",
 fs_t="6. Footswitchs, lettres et modes",
 fs=["<b>Mode PRESET</b> : les footswitchs A à F rappellent les presets A-F de la bank affichée en haut de l'écran (E avec A+B, F avec C+D) (les flèches ou les commandes MIDI Bank down / up changent la bank affichée ; elle est chargée quand vous appuyez sur un footswitch). La lettre du preset chargé s'allume dans la couleur de son footswitch (bleu par défaut, à choisir dans BANKS &gt; COLOURS AND GLOW).",
     "<b>Mode STOMP</b> : chaque footswitch bascule un ou plusieurs blocs (clic droit sur un bloc &gt; Assign to footswitch, ou clic droit sur un footswitch &gt; STOMP : blocks switched by...). Si l'un de ses blocs est ON, un appui les coupe tous ; sinon il les allume tous. La case du footswitch affiche la famille et les noms de ses blocs, sa lettre prend la couleur du premier bloc et s'allume quand l'un d'eux est ON. Les assignations sont enregistrées avec les presets.",
     "Comme sur Echoflex, un footswitch agit dès l'appui, son bouton s'assombrit tant qu'on appuie. Au-dessus, la lettre A, B, C ou D est rétroéclairée : gris clair quand elle est éteinte, c'est la lettre elle-même qui s'allume en couleur (avec l'anneau lumineux autour du bouton), en fondu.",
     "<b>Doubles appuis</b> : appuyer sur deux footswitchs voisins ensemble (<b>A+B = E, C+D = F</b>) donne deux footswitchs de plus, sérigraphiés entre les deux footswitchs : <b>E</b> et <b>F</b>. B+C ne fait rien : le symbole de l'accordeur entre B et C se commande par son propre MIDI learn. A+B est le footswitch <b>E</b> et C+D le footswitch <b>F</b>, exactement comme A-D. Clic droit sur le symbole de l'accordeur : <i>MIDI Learn : Tuner on / off</i>, <i>Clear MIDI</i>, accrochage et <i>Color &amp; Glow</i>. Un clic sur un marquage l'exécute à la souris. Chaque footswitch agit tout de suite, sans attente : si son voisin suit dans les 0,25 s, ce premier appui est annulé et c'est le double appui qui agit (en mode PRESET, le preset du double appui est chargé). Voir le <b>guide MIDI et CC</b> à part.",
     "<b>Footswitchs E et F</b> : E (A+B) et F (C+D) sont des footswitchs comme A-D, avec leurs propres cases à l'écran et le même menu au clic droit : en mode PRESET ils rappellent les presets E et F de la bank (1E, 1F...), en mode STOMP ils basculent leurs blocs (clic droit sur un bloc &gt; Assign to footswitch &gt; E / F). Leurs assignations sont enregistrées avec chaque preset. <b>En MIDI</b>, rien de plus à apprendre : E correspond au CC de A et au CC de B reçus ensemble, F à ceux de C et D (apprenez A, B, C et D sur leurs footswitchs ; le menu de E / F affiche les CC utilisés). Un CC déjà appris sur un footswitch n'est jamais pris par une autre commande.",
     "<b>Marquages</b> : entre A et B, entre C et D, et entre B et C, la pédale porte <b>] E [</b>, <b>] F [</b> et le symbole de l'accordeur. Chaque marquage est un voyant : gris clair éteint, le marquage E / F s'allume comme les lettres A-D (couleur de son preset en mode PRESET, de son premier bloc en mode STOMP), et le symbole de l'accordeur s'allume tant que l'accordeur est actif (ses traits latéraux ne s'allument jamais). <b>Halo</b> : la lumière autour des lettres, des marquages et du symbole de l'accordeur se règle avec le curseur <b>GLOW (HALO OPACITY)</b> du nuancier (clic droit sur un footswitch &gt; Color &amp; Glow, 0 à 200 %, 100 % par défaut) ; il ne change que la lumière autour, pas leur couleur. Le symbole de l'accordeur a sa propre couleur (BANKS &gt; TUNER, ou clic droit sur le symbole &gt; Color &amp; Glow) et le même halo que les lettres. Ces réglages valent pour tous vos projets.",
     "Clic droit sur un footswitch : enregistrer / charger le preset de ce footswitch, assignations STOMP, MIDI learn du footswitch, mode à accrochage, menu MIDI et <b>Color &amp; Glow</b> (nuancier de ce footswitch avec son curseur GLOW). La taille du plugin est dans le menu du clic droit en dehors des footswitchs."],
 stomp_cap="Mode STOMP : le footswitch A bascule NGATE + Crycat ; les lettres prennent la couleur de leurs blocs.",
 midi_t="7. MIDI",
 midi=["Octopus écoute le MIDI de sa piste (dans REAPER : entrée MIDI sur la piste, armement ou monitoring actif). Le MIDI continue ensuite vers les effets suivants (par exemple votre pédale d'expression pour Crycat ou Wharry)."],
 midi_hdr=["Commande", "Par défaut", "Comment"],
 midi_rows=[["Footswitch A, B, C, D", "aucun", "MIDI learn (clic droit sur le footswitch &gt; MIDI Learn, puis appui sur le commutateur du contrôleur)"],
            ["Bank down / Bank up", "aucun", "Clic droit &gt; MIDI &gt; Learn"], ["Mode PRESET / STOMP", "aucun", "Clic droit &gt; MIDI &gt; Learn"],
            ["Accordeur on / off", "aucun", "Clic droit sur le symbole de l'accordeur &gt; MIDI Learn"], ["Presets A-D de chaque bank", "Program Change 1-64 (PC 1-4 = 1A-1D, PC 5-8 = 2A-2D...)", "Actif par défaut (PROGRAM CHANGE = PRESETS) ; E / F : doubles appuis"],
            ["Footswitch E, F, B+C", "CC de A + B, C + D, B + C", "Automatique : les CC appris sur les deux footswitchs, reçus ensemble"]],
 midi2=["<b>Momentané ou à accrochage</b> : un CC à 64 ou plus compte comme un appui. Momentané (par défaut) : action quand le commutateur passe à ON (127 à l'appui, 0 au relâchement). À accrochage (clic droit &gt; <i>MIDI footswitch latching</i>) : pour les commutateurs qui alternent 127 / 0, chaque changement est un appui.",
        "Un numéro de CC ne pilote qu'une commande (l'apprendre de nouveau le déplace). Clic droit &gt; MIDI &gt; <i>Reset MIDI</i> efface toutes les assignations. Les assignations sont enregistrées avec le projet.",
        "Les paramètres du plugin <b>Footswitch A-D, Footswitch E et F, Bank down, Bank up, PRESET / STOMP mode, Tuner on / off</b> peuvent aussi être pilotés par le MIDI learn ou l'automation de REAPER, avec la même règle momentané / accrochage. Le paramètre <b>Octopus ID</b> est interne (il permet à chaque Octopus de se retrouver dans la chaîne) : ne l'automatisez jamais.",
        "<b>M-Vave Chocolate Plus</b> : en mode Program Change, ses 4 commutateurs rappellent directement les presets (bank 1 = PC 1-4 = 1A-1D...). En mode CC, apprenez ses commutateurs sur les footswitchs A-D et utilisez STOMP / PRESET comme sur un multi-effet matériel."],
 win_t="8. Fenêtre",
 win=["La pédale occupe toute la fenêtre (pas de jacks sur les côtés). Clic droit hors de l'écran (ou dans les menus de l'écran) &gt; <b>Plugin size</b> : <b>50 %</b>, <b>60 %</b>, <b>75 %</b> et <b>100 %</b> ; la taille par défaut porte la mention <i>(default)</i>. <b>Default size (new windows)</b> choisit la taille à laquelle s'ouvrent toutes les fenêtres d'Octopus (60 % tant que vous ne la changez pas ; ce choix vaut pour tous vos projets et la fenêtre actuelle le prend tout de suite). On peut aussi tirer le coin en bas à droite (proportions conservées), jusqu'à 50 %. Une taille choisie pour un Octopus est mémorisée avec le projet.",
      "La <b>ligne d'état</b>, sous le nom du preset, affiche le nom de la piste ou la dernière action (preset chargé, effet ajouté, MIDI learn...)."],
 tips_t="10. Conseils d'utilisation",
 tips=["Insérez Octopus <b>en premier</b> sur la piste guitare ; votre accordeur (CHROMA), le noise gate, les pédales, Ampire, Impulse et les ambiances suivent. Octopus laisse passer le son tel quel : sa position ne change pas le son (avec une transposition de Semitones, tout ce qui suit est transposé, comme avec une pédale de transposition placée en premier).",
       "Pour la scène, construisez une chaîne avec tous les effets dont vous avez besoin et enregistrez-en des variantes (blocs ON / OFF, réglages différents) : passer d'un de ces presets à l'autre est quasi instantané car seul ce qui diffère est rechargé.",
       "Utilisez le mode STOMP dans un morceau pour basculer des groupes drive, delay ou reverb, et le mode PRESET entre les morceaux.",
       "Gardez une copie de vos banks avec EXPORT... et importez-les dans un nouveau projet avec IMPORT....",
       "L'annulation de REAPER reste votre amie : chaque action d'Octopus, rappel de preset compris, est un seul point d'annulation."],
 tech_t="11. Caractéristiques techniques",
 tech=[["Format", "VST3 (Windows 64 bits, macOS universel)"], ["Version", VERSION], ["Hôte", "REAPER 6.37 ou plus récent (liste des plugins installés : REAPER 7) ; inactif dans les autres DAW"],
       ["Audio", "Mono ou stéréo, transmis sans modification (sauf la transposition de Semitones, -12 à +12 demi-tons en entrée), latence de 0 échantillon"], ["Chaîne", "Chaîne d'effets ou effets d'entrée (enregistrement) de la piste où Octopus est inséré"],
       ["Presets", "16 banks x 6 presets (A-F), chaînes complètes au format de REAPER, enregistrés dans le projet, export / import .octobank ; presets usine / utilisateur de chaque plugin"],
       ["Accordeur", "écran et moteur de CHROMA (MONO, POLY, STROBE) quand un bloc CHROMA est allumé ; mode, référence et accordage lus dans CHROMA ; menu TUNINGS avec les presets d'accordage de CHROMA"],
       ["MIDI", "MIDI learn des footswitchs A-D, bank - / +, mode ; momentané ou à accrochage ; Program Change 1-64 ; tous canaux ; MIDI transmis aux effets suivants"],
       ["Fenêtre", "50 %, 60 %, 75 %, 100 % ; taille par défaut au choix (60 % au départ), minimum 50 % ; anti-gel v2 sous Windows (caches d'affichage refaits par un timer, chien de garde, aucun appel à l'hôte depuis le thread audio)"]]),
}

W = A4[0] - 36 * mm
def captioned(path, width, cap):
    return KeepTogether([Spacer(1, 2 * mm), img(path, width), Spacer(1, 1 * mm), P(cap, "small")])

from reportlab.platypus.flowables import Flowable

class Cover(Flowable):
    """ Première page : pédale centrée sur la page ; en bas, logo OCTOPUS, sous-titre, version et petit logo NINE HANDS | DSP. """
    def __init__(self, t): super().__init__(); self.t = t
    def wrap(self, aw, ah): self.aw, self.ah = aw, ah - 1; return aw, ah - 1
    def draw(self):
        c = self.canv; aw = self.aw
        ox, oy = doc.leftMargin, doc.bottomMargin                      # origine du cadre sur la page
        c.setFont("PopM", 8.5); c.setFillColor(colors.HexColor("#4FC3F7"))
        c.drawString(0, self.ah - 6 * mm, self.t["lang"].replace("&nbsp;", " "))
        # pédale : son centre (hors halo et reflet) au centre de la page
        k = aw / cover_size[0]
        hp = cover_size[1] * k
        ped_cy_from_top = cover_pedal_cy * k
        page_cy = A4[1] / 2 - oy
        c.drawImage(cover, 0, page_cy + ped_cy_from_top - hp, aw, hp)
        # bloc du bas (remonté depuis le bas du cadre)
        y = 2 * mm
        sw = 30 * mm; sh = sw * sw_ratio
        c.drawImage(os.path.join(ROOT, "design", "logo_ninehands.png"), (aw - sw) / 2, y, sw, sh, mask="auto")
        y += sh + 9 * mm
        c.setFont("Pop", 10.5); c.setFillColor(colors.HexColor("#B4BCC6"))
        c.drawCentredString(aw / 2, y, self.t["ver"].replace("&nbsp;", " "))
        y += 6 * mm
        c.drawCentredString(aw / 2, y, self.t["sub"].replace("&nbsp;", " "))
        y += 8 * mm
        lw = 80 * mm; lh = lw * oct_ratio
        c.drawImage(os.path.join(ROOT, "design", "logo_octopus_v003.png"), (aw - lw) / 2, y, lw, lh, mask="auto")

cover_size = Image.open(cover).size
cover_pedal_cy = pad + ch / 2.0                                          # centre de la pédale dans l'image de couverture (px)
_sw = Image.open(os.path.join(ROOT, "design", "logo_ninehands.png")); sw_ratio = _sw.height / _sw.width
_ol = Image.open(os.path.join(ROOT, "design", "logo_octopus_v003.png")); oct_ratio = _ol.height / _ol.width

def section(lang):
    t = T[lang]; f = []
    f.append(NextPageTemplate("p"))
    f.append(Cover(t))
    f.append(PageBreak())
    f.append(P(t["intro_t"], "h1")); f += [P(x) for x in t["intro"]]
    f.append(P(t["feat_t"], "h2")); f += bullets(t["feat"])
    f.append(P(t["inst_t"], "h1"))
    for h, x in t["inst"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(KeepTogether([P(t["ov_t"], "h1"), img(annotated, W * 0.80), Spacer(1, 3 * mm),
                           table(t["ov_rows"], [10 * mm, 38 * mm, W - 48 * mm])]))
    f.append(PageBreak())
    f.append(P(t["ctl_t"], "h1"))
    for h, x in t["ctl"]: f.append(P(h, "h2")); f.append(P(x))
    pair = Table([[img(scr_add, W * 0.48), img(scr_params, W * 0.48)], [P(t["scr_cap_add"], "small"), P(t["scr_cap_par"], "small")]],
                 colWidths=[W * 0.5, W * 0.5])
    pair.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    f.append(KeepTogether([Spacer(1, 3 * mm), pair]))
    f.append(captioned(scr_presets, W * 0.48, t["scr_cap_pre"]))
    f.append(P(t["more_t"], "h2"))
    for h, x in t["more"]: f.append(P("<b>" + h + "</b>", "p")); f.append(P(x))
    f.append(P(t["tun_t"], "h2")); f += [P(x) for x in t["tun"]]
    tpair = Table([[img(scr_tmono, W * 0.48), img(scr_tpoly, W * 0.48)]], colWidths=[W * 0.5, W * 0.5])
    tpair.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    f.append(KeepTogether([Spacer(1, 2 * mm), tpair, P(t["tun_cap"], "small")]))
    f.append(P(t["semi_t"], "h2")); f += [P(x) for x in t["semi"]]
    f.append(captioned(scr_semi, W * 0.62, t["semi_cap"]))
    f.append(P(t["par_t"], "h2")); f += [P(x) for x in t["par"]]
    ppair = Table([[img(scr_split, W * 0.48), img(scr_mix, W * 0.48)]], colWidths=[W * 0.5, W * 0.5])
    ppair.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0)]))
    f.append(KeepTogether([Spacer(1, 2 * mm), ppair, P(t["par_cap"], "small")]))
    f.append(PageBreak())
    f.append(P(t["pre_t"], "h1")); f += [P(x) for x in t["pre"]]
    f.append(captioned(scr_banks, W * 0.62, t["scr_cap_banks"]))
    f.append(P(t["fs_t"], "h1")); f += bullets(t["fs"])
    f.append(captioned(scr_stomp, W * 0.62, t["stomp_cap"]))
    f.append(P(t["midi_t"], "h1")); f += [P(x) for x in t["midi"]]
    f.append(Spacer(1, 2 * mm)); f.append(table([t["midi_hdr"]] + t["midi_rows"], [40 * mm, 32 * mm, W - 72 * mm])); f.append(Spacer(1, 3 * mm))
    f += bullets(t["midi2"])
    f.append(P(t["win_t"], "h1")); f += [P(x) for x in t["win"]]
    f.append(P(t["flow_t"], "h1"))
    for h, x in t["flow"]: f.append(P(h, "h2")); f.append(P(x))
    f.append(P(t["tips_t"], "h1")); f += bullets(t["tips"])
    f.append(KeepTogether([P(t["tech_t"], "h1"), table(t["tech"], [30 * mm, W - 30 * mm], header=False)]))
    f.append(PageBreak())
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Octopus (NINE HANDS | DSP) · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="OCTOPUS - Manual / Notice v" + VERSION, author="NINE HANDS | DSP")
def cover_bg(c, doc):
    c.saveState(); c.setFillColor(colors.black); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 3 * mm, A4[0], 3 * mm, stroke=0, fill=1); c.restoreState()
doc.addPageTemplates([PageTemplate(id="cover", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="fc")], onPage=cover_bg),
                      PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
en = section("en"); fr = section("fr")
en[-1:] = [NextPageTemplate("cover"), PageBreak()]
story = en + fr[:-1]
doc.build(story)
print(OUT)
