# OCTOPUS (NINE HANDS | DSP) - guide MIDI et CC, à part de la notice : commandes MIDI, réglage d'un pédalier
# (M-Vave Chocolate Plus avec CubeSuite), MIDI learn, doubles appuis A+B / B+C / C+D, tests. Anglais puis français.
# Usage : python3 tools/midi_guide.py <dossier des captures (design/renders)> <fichier PDF de sortie>
import os, sys
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image as RLImage, Table, TableStyle,
                                PageBreak, KeepTogether, NextPageTemplate)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHOTS, OUT = sys.argv[1], sys.argv[2]
VERSION = "1.12.0"
TMP = os.path.join(os.path.dirname(os.path.abspath(OUT)), "midi_guide_tmp"); os.makedirs(TMP, exist_ok=True)
for n, f in (("Pop", "Poppins-Regular"), ("PopM", "Poppins-Medium"), ("PopB", "Poppins-Bold")):
    pdfmetrics.registerFont(TTFont(n, os.path.join(ROOT, "fonts", f + ".ttf")))
ACC = colors.HexColor("#1E9BD0"); DARK = colors.HexColor("#1A1C22"); GREY = colors.HexColor("#646B76"); LIGHT = colors.HexColor("#EAF4FA")
W = A4[0] - 36 * mm
FNT = ImageFont.truetype(os.path.join(ROOT, "fonts", "Poppins-Bold.ttf"), 24)

def badge(dr, x, y, n, r=17):
    dr.ellipse((x - r, y - r, x + r, y + r), fill=(30, 155, 208), outline=(255, 255, 255), width=3)
    dr.text((x, y + 1), str(n), font=FNT, fill="white", anchor="mm")

# ---------- images ----------
# 1. CubeSuite (réglage de la pédale) : repères numérotés
cs = Image.open(os.path.join(ROOT, "design", "cubesuite_advanced.png")).convert("RGB")
d = ImageDraw.Draw(cs)
for n, x, y in ((1, 300, 268), (2, 1052, 130), (3, 612, 219), (4, 645, 601)):
    badge(d, x - 28 if n != 2 else x - 26, y, n)
cs_path = os.path.join(TMP, "cubesuite.png"); cs.save(cs_path)

# 2. footswitchs et doubles appuis (A+B = CC 26, B+C = CHROMA, C+D = BANK +)
def crop_units(name, box, out):
    im = Image.open(os.path.join(SHOTS, name)).convert("RGB")
    k = im.width / 1249.0
    im.crop(tuple(int(v * k) for v in box)).save(os.path.join(TMP, out))
    return os.path.join(TMP, out)
combos = Image.open(crop_units("octopus_combos.png", (81, 770, 1171, 934), "combos.png")); d = ImageDraw.Draw(combos)
kk = combos.width / 1090.0
for n, ux in ((1, 338.5), (2, 624.4), (3, 906.4)):
    badge(d, int((ux - 81) * kk), int(18 * kk), n, r=int(17 * kk / 1.4))
combos_path = os.path.join(TMP, "combos_marked.png"); combos.save(combos_path)
full_path = crop_units("octopus_combos.png", (0, 0, 1249, 934), "full.png")
tuner_path = crop_units("octopus_tuner_builtin.png", (36, 67, 1213, 788), "tuner.png")

def img(path, width):
    im = Image.open(path); return RLImage(path, width=width, height=width * im.height / im.width)

st = {
    "title": ParagraphStyle("title", fontName="PopB", fontSize=26, leading=30, textColor=DARK),
    "sub": ParagraphStyle("sub", fontName="Pop", fontSize=11, leading=15, textColor=GREY),
    "h1": ParagraphStyle("h1", fontName="PopB", fontSize=17, leading=21, textColor=ACC, spaceAfter=6, spaceBefore=8, keepWithNext=1),
    "h2": ParagraphStyle("h2", fontName="PopM", fontSize=12, leading=16, textColor=DARK, spaceAfter=3, spaceBefore=8, keepWithNext=1),
    "p": ParagraphStyle("p", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, spaceAfter=4),
    "step": ParagraphStyle("step", fontName="Pop", fontSize=9.6, leading=14, textColor=DARK, leftIndent=14, firstLineIndent=-14, spaceAfter=3),
    "small": ParagraphStyle("small", fontName="Pop", fontSize=8.2, leading=11.5, textColor=GREY, spaceAfter=6),
    "cell": ParagraphStyle("cell", fontName="Pop", fontSize=8.4, leading=11, textColor=DARK),
    "cellb": ParagraphStyle("cellb", fontName="PopM", fontSize=8.4, leading=11, textColor=DARK),
}
def P(t, s="p"): return Paragraph(t, st[s])
def steps(items): return [P("<b>%d.</b>&nbsp;&nbsp;%s" % (i + 1, x), "step") for i, x in enumerate(items)]
def table(rows, widths):
    t = Table([[P(c, "cellb" if r == 0 else "cell") for c in row] for r, row in enumerate(rows)], colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, 0), LIGHT), ("LINEBELOW", (0, 0), (-1, -1), 0.4, colors.HexColor("#D5DCE3")),
                           ("VALIGN", (0, 0), (-1, -1), "TOP"), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]))
    return t

T = {
"en": dict(
 lang="ENGLISH", title="OCTOPUS - MIDI and CC guide", sub="Footswitches, MIDI pedalboard and double presses, step by step  ·  Octopus " + VERSION,
 intro="Octopus can be driven entirely by a MIDI foot controller. This guide shows, step by step, how to set up a pedalboard (example: <b>M-Vave Chocolate Plus</b> with its CubeSuite editor), how to assign it in Octopus with <b>MIDI learn</b>, and how to use the <b>double presses</b> <b>E</b> (A+B) and <b>F</b> (C+D), and the <b>tuner</b> command: E and F are two more footswitches (presets 1E-16F, or blocks in STOMP mode). No CC is assigned by default: everything is learned.",
 t1="1. The MIDI commands of Octopus",
 cmd_hdr=["Command", "MIDI message", "How to assign it"],
 cmd=[["Footswitch A, B, C, D", "a CC of your choice", "Right-click the footswitch &gt; MIDI Learn, then press the pedal switch"],
      ["Bank down / Bank up", "a CC of your choice", "Right-click &gt; MIDI &gt; Learn : Bank down / Bank up"],
      ["PRESET / STOMP mode", "a CC of your choice", "Right-click &gt; MIDI &gt; Learn : PRESET / STOMP mode"],
      ["Tuner on / off", "a CC of your choice", "Right-click the tuner symbol &gt; MIDI Learn"],
      ["Presets A-D of each bank", "Program Change 1-64 (PC 1-4 = 1A-1D, PC 5-8 = 2A-2D...)", "Nothing to learn (PROGRAM CHANGE = PRESETS in BANKS)"],
      ["E (A+B), F (C+D)", "CC of A + CC of B together (E), C + D (F)", "Automatic once A-D are learned"]],
 rule="<b>Rule for incoming CCs</b>: a value of 64 or more is a press. <i>Momentary</i> (default): one action each time the switch goes to 127. <i>Latching</i> (right-click &gt; MIDI footswitch latching): for switches that alternate 127 / 0, every change is a press. One CC drives one command only; all assignments are saved with the REAPER project.",
 t2="2. Set up the pedalboard (M-Vave Chocolate Plus, CubeSuite)",
 cs_cap="CubeSuite: 1 Advanced custom mode, 2 footswitch selector, 3 Single step mode, 4 messages of the switch (A Bank).",
 s2=["Connect the Chocolate Plus by USB and open <b>CubeSuite</b>.",
     "In <b>Mode selection</b>, choose <b>Advanced custom mode</b> (mark 1).",
     "In <b>Foot Switch Additional options</b>, select <b>Foot Switch [A]</b> (mark 2).",
     "Choose <b>Mode: Single step (single Bank information)</b> (mark 3): the message is sent <b>as soon as the switch is pressed</b>, which double presses need.",
     "In <b>A Bank</b>, <b>Remove All</b>, then <b>Add</b> a <b>CC</b> message, channel 1, with its own number and value 127 (mark 4). Suggested numbers: A = CC 20, B = CC 21, C = CC 22, D = CC 23. You may add the same CC with value 0 after it.",
     "Repeat for <b>Foot Switch [B]</b>, <b>[C]</b> and <b>[D]</b>, each with its own CC number, then send the settings to the pedal as CubeSuite asks.",
     "Avoid <i>Step short or step long</i> for A-D if you want double presses: in that mode the short message is only sent when you release the switch. It is however a good way to get a <b>long press</b> (see section 6)."],
 t3="3. Bring the MIDI to Octopus in REAPER",
 s3=["<b>Options &gt; Preferences &gt; Audio &gt; MIDI Inputs</b>: right-click the Chocolate &gt; <b>Enable input</b>. (Bluetooth on Windows: pair it, then use MIDIberry + loopMIDI if REAPER does not list it.)",
     "On the guitar track, with Octopus first in the FX chain: record arm, input <b>MIDI &gt; Chocolate &gt; All channels</b>, monitoring on, record mode <i>Record: disable (input monitoring only)</i>.",
     "Octopus now receives the pedal; the MIDI also continues to the effects after it."],
 t4="4. Assign the footswitches (MIDI learn)",
 s4=["Right-click footswitch <b>A</b> on Octopus &gt; <b>MIDI Learn</b>. The status line asks you to press the switch.",
     "Press switch A of the pedal: the status line confirms <i>Footswitch A = CC 20</i>.",
     "Do the same for B, C and D. Bank down / up, PRESET / STOMP mode and Tuner on / off are learned the same way from the <b>MIDI</b> menu (right-click outside the screen).",
     "<i>Reset MIDI</i> in the same menu clears every assignment."],
 t5="5. Double presses E (A+B), F (C+D) and the tuner",
 comb_cap="The three double presses printed on the pedal: 1 E = A+B (footswitch E: preset 1E... or its blocks), 2 tuner symbol (MIDI learn, B+C does nothing), 3 F = C+D (footswitch F).",
 s5=["Press two <b>neighbouring</b> switches together (less than 0.25 s apart): Octopus runs the action of the pair instead of the two footswitches.",
     "<b>Tuner</b>: right-click the tuner symbol between B and C &gt; <i>MIDI Learn : Tuner on / off</i>, then press a switch of the pedalboard that is not one of A-D (for example the long press of a switch). A click on the symbol also opens / closes the tuner. <i>Color &amp; Glow</i> sets the colour and halo of the tuner symbol.",
     "<b>E</b> (A+B) and <b>F</b> (C+D) are two more footswitches, exactly like A-D (same right-click menu): in PRESET mode they recall presets E and F of the bank, in STOMP mode they switch their blocks; the tuner symbol lights up while tuning. Hover the marking to see its action on the status line.",
     "<b>Nothing to learn for E and F</b>: E is the CC of A and the CC of B received together, F those of C and D. Learn A, B, C and D (step 4) and the double presses work at once; the E / F menus show the CCs used. Learning <i>Tuner on / off</i> with the CC of a footswitch is refused, so that footswitch keeps its CC.",
     "With the mouse: click the marking (or Shift + click a footswitch). Every footswitch acts at once, without waiting: if its neighbour follows within 0.25 s, the first press is undone and the pair acts instead."],
 tun_cap="Tuner command: Octopus becomes the tuner (CHROMA screen and engine), without a CHROMA plug-in in the chain.",
 t6="6. Check and solve",
 s6=["<b>Check what the pedal sends</b>: arm a track with the pedal as input, record a few seconds while pressing the switches, open the MIDI item and choose <b>View &gt; Mode: Event List</b>. A double press must show the two CCs at the same time (a few milliseconds apart).",
     "<b>Only one CC for a double press</b>: the pedal does not send both. Use a <b>long press</b> instead: in CubeSuite, set the switch to <i>Step short or step long</i> with a second CC in <b>B Bank</b> (for example CC 26), and learn that CC on <i>Tuner on / off</i> or any command.",
     "<b>Double press not recognised in Program Change mode</b>: Program Change loads presets directly. Use CC messages for double presses.",
     "<b>Nothing happens</b>: check the MIDI input of the track, the record arm / monitoring, and that the status line shows the CC when you press a switch."]),
"fr": dict(
 lang="FRANÇAIS", title="OCTOPUS - guide MIDI et CC", sub="Footswitchs, pédalier MIDI et doubles appuis, pas à pas  ·  Octopus " + VERSION,
 intro="Octopus peut être entièrement piloté par un pédalier MIDI. Ce guide montre pas à pas comment régler un pédalier (exemple : <b>M-Vave Chocolate Plus</b> avec son logiciel CubeSuite), comment l'affecter dans Octopus par <b>MIDI learn</b>, et comment utiliser les <b>doubles appuis</b> <b>E</b> (A+B) et <b>F</b> (C+D), et la commande de l'<b>accordeur</b> : E et F sont deux footswitchs de plus (presets 1E-16F, ou blocs en mode STOMP). Aucun CC n'est affecté par défaut : tout s'apprend.",
 t1="1. Les commandes MIDI d'Octopus",
 cmd_hdr=["Commande", "Message MIDI", "Comment l'affecter"],
 cmd=[["Footswitch A, B, C, D", "un CC au choix", "Clic droit sur le footswitch &gt; MIDI Learn, puis appui sur le commutateur du pédalier"],
      ["Bank - / Bank +", "un CC au choix", "Clic droit &gt; MIDI &gt; Learn : Bank down / Bank up"],
      ["Mode PRESET / STOMP", "un CC au choix", "Clic droit &gt; MIDI &gt; Learn : PRESET / STOMP mode"],
      ["Accordeur on / off", "un CC au choix", "Clic droit sur le symbole de l'accordeur &gt; MIDI Learn"],
      ["Presets A-D de chaque bank", "Program Change 1-64 (PC 1-4 = 1A-1D, PC 5-8 = 2A-2D...)", "Rien à apprendre (PROGRAM CHANGE = PRESETS dans BANKS)"],
      ["E (A+B), F (C+D)", "CC de A + CC de B ensemble (E), C + D (F)", "Automatique dès que A-D sont appris"]],
 rule="<b>Règle des CC reçus</b> : une valeur de 64 ou plus compte comme un appui. <i>Momentané</i> (par défaut) : une action à chaque passage du commutateur à 127. <i>À accrochage</i> (clic droit &gt; MIDI footswitch latching) : pour les commutateurs qui alternent 127 / 0, chaque changement est un appui. Un CC ne pilote qu'une commande ; toutes les affectations sont enregistrées avec le projet REAPER.",
 t2="2. Régler le pédalier (M-Vave Chocolate Plus, CubeSuite)",
 cs_cap="CubeSuite : 1 Advanced custom mode, 2 choix du commutateur, 3 mode Single step, 4 messages du commutateur (A Bank).",
 s2=["Branchez la Chocolate Plus en USB et ouvrez <b>CubeSuite</b>.",
     "Dans <b>Mode selection</b>, choisissez <b>Advanced custom mode</b> (repère 1).",
     "Dans <b>Foot Switch Additional options</b>, sélectionnez <b>Foot Switch [A]</b> (repère 2).",
     "Choisissez <b>Mode: Single step (single Bank information)</b> (repère 3) : le message part <b>dès l'appui</b>, ce que demandent les doubles appuis.",
     "Dans <b>A Bank</b>, <b>Remove All</b>, puis <b>Add</b> un message <b>CC</b>, canal 1, avec son propre numéro et la valeur 127 (repère 4). Numéros conseillés : A = CC 20, B = CC 21, C = CC 22, D = CC 23. Vous pouvez ajouter ensuite le même CC à la valeur 0.",
     "Recommencez pour <b>Foot Switch [B]</b>, <b>[C]</b> et <b>[D]</b>, chacun avec son numéro de CC, puis envoyez les réglages à la pédale comme CubeSuite le demande.",
     "Évitez <i>Step short or step long</i> pour A-D si vous voulez les doubles appuis : dans ce mode, le message court ne part qu'au relâchement. C'est en revanche un bon moyen d'obtenir un <b>appui long</b> (voir section 6)."],
 t3="3. Amener le MIDI à Octopus dans REAPER",
 s3=["<b>Options &gt; Preferences &gt; Audio &gt; MIDI Inputs</b> : clic droit sur la Chocolate &gt; <b>Enable input</b>. (Bluetooth sous Windows : appairez-la, puis utilisez MIDIberry + loopMIDI si REAPER ne la liste pas.)",
     "Sur la piste guitare, Octopus en premier dans la chaîne : piste armée, entrée <b>MIDI &gt; Chocolate &gt; All channels</b>, monitoring actif, mode d'enregistrement <i>Record: disable (input monitoring only)</i>.",
     "Octopus reçoit maintenant le pédalier ; le MIDI continue aussi vers les effets placés après lui."],
 t4="4. Affecter les footswitchs (MIDI learn)",
 s4=["Clic droit sur le footswitch <b>A</b> d'Octopus &gt; <b>MIDI Learn</b>. La ligne d'état demande d'appuyer sur le commutateur.",
     "Appuyez sur le commutateur A du pédalier : la ligne d'état confirme <i>Footswitch A = CC 20</i>.",
     "Faites de même pour B, C et D. Bank - / +, mode PRESET / STOMP et accordeur s'apprennent de la même façon depuis le menu <b>MIDI</b> (clic droit hors de l'écran).",
     "<i>Reset MIDI</i>, dans le même menu, efface toutes les affectations."],
 t5="5. Doubles appuis E (A+B), F (C+D) et accordeur",
 comb_cap="Les trois doubles appuis sérigraphiés sur la pédale : 1 E = A+B (footswitch E : preset 1E... ou ses blocs), 2 symbole de l'accordeur (MIDI learn, B+C ne fait rien), 3 F = C+D (footswitch F).",
 s5=["Appuyez sur deux commutateurs <b>voisins</b> ensemble (à moins de 0,25 s d'écart) : Octopus exécute l'action de la paire au lieu des deux footswitchs.",
     "<b>Accordeur</b> : clic droit sur le symbole de l'accordeur entre B et C &gt; <i>MIDI Learn : Tuner on / off</i>, puis appui sur un commutateur du pédalier qui n'est pas A-D (par exemple l'appui long d'un commutateur). Un clic sur le symbole ouvre / ferme aussi l'accordeur. <i>Color &amp; Glow</i> règle la couleur et le halo du symbole de l'accordeur.",
     "<b>E</b> (A+B) et <b>F</b> (C+D) sont deux footswitchs de plus, exactement comme A-D (même menu au clic droit) : en mode PRESET ils rappellent les presets E et F de la bank, en mode STOMP ils basculent leurs blocs ; le symbole de l'accordeur s'allume pendant l'accordage. Survolez le marquage pour voir son action sur la ligne d'état.",
     "<b>Rien à apprendre pour E et F</b> : E correspond au CC de A et au CC de B reçus ensemble, F à ceux de C et D. Apprenez A, B, C et D (étape 4) et les doubles appuis fonctionnent tout de suite ; les menus de E / F affichent les CC utilisés. Apprendre <i>Tuner on / off</i> avec le CC d'un footswitch est refusé : ce footswitch garde son CC.",
     "À la souris : clic sur le marquage (ou Maj + clic sur un footswitch). Chaque footswitch agit tout de suite, sans attente : si son voisin suit dans les 0,25 s, le premier appui est annulé et c'est la paire qui agit."],
 tun_cap="Commande de l'accordeur : Octopus devient l'accordeur (écran et moteur de CHROMA), sans plugin CHROMA dans la chaîne.",
 t6="6. Vérifier et dépanner",
 s6=["<b>Voir ce qu'envoie le pédalier</b> : armez une piste avec le pédalier en entrée, enregistrez quelques secondes en appuyant sur les commutateurs, ouvrez l'item MIDI et choisissez <b>View &gt; Mode: Event List</b>. Un double appui doit montrer les deux CC au même instant (quelques millisecondes d'écart).",
     "<b>Un seul CC pour un double appui</b> : le pédalier n'envoie pas les deux. Utilisez plutôt un <b>appui long</b> : dans CubeSuite, réglez le commutateur sur <i>Step short or step long</i> avec un second CC dans <b>B Bank</b> (par exemple CC 26), et apprenez ce CC sur <i>Tuner on / off</i> ou une autre commande.",
     "<b>Double appui non reconnu en mode Program Change</b> : le Program Change charge directement les presets. Utilisez des CC pour les doubles appuis.",
     "<b>Rien ne se passe</b> : vérifiez l'entrée MIDI de la piste, l'armement / le monitoring, et que la ligne d'état affiche le CC quand vous appuyez sur un commutateur."]),
}

def section(lang):
    t = T[lang]; f = []
    f.append(P(t["lang"], "small"))
    f.append(P(t["title"], "title")); f.append(P(t["sub"], "sub")); f.append(Spacer(1, 5 * mm))
    f.append(img(full_path, W * 0.78)); f.append(Spacer(1, 4 * mm))
    f.append(P(t["intro"]))
    f.append(P(t["t1"], "h1")); f.append(table([t["cmd_hdr"]] + t["cmd"], [38 * mm, 46 * mm, W - 84 * mm])); f.append(Spacer(1, 2 * mm))
    f.append(P(t["rule"]))
    f.append(PageBreak())
    f.append(P(t["t2"], "h1")); f.append(img(cs_path, W)); f.append(P(t["cs_cap"], "small")); f += steps(t["s2"])
    f.append(P(t["t3"], "h1")); f += steps(t["s3"])
    f.append(P(t["t4"], "h1")); f += steps(t["s4"])
    f.append(KeepTogether([P(t["t5"], "h1"), img(combos_path, W), P(t["comb_cap"], "small")])); f += steps(t["s5"])
    f.append(KeepTogether([Spacer(1, 2 * mm), img(tuner_path, W * 0.55), P(t["tun_cap"], "small")]))
    f.append(P(t["t6"], "h1")); f += steps(t["s6"])
    f.append(PageBreak())
    return f

def deco(c, doc):
    c.saveState()
    c.setFillColor(DARK); c.rect(0, A4[1] - 6 * mm, A4[0], 6 * mm, stroke=0, fill=1)
    c.setFillColor(ACC); c.rect(0, A4[1] - 6.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Pop", 7.5); c.setFillColor(GREY)
    c.drawString(18 * mm, 10 * mm, "Octopus (NINE HANDS | DSP) · MIDI and CC guide · v" + VERSION)
    c.drawRightString(A4[0] - 18 * mm, 10 * mm, str(doc.page))
    c.restoreState()

doc = BaseDocTemplate(OUT, pagesize=A4, leftMargin=18 * mm, rightMargin=18 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                      title="OCTOPUS - MIDI and CC guide v" + VERSION, author="NINE HANDS | DSP")
doc.addPageTemplates([PageTemplate(id="p", frames=[Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="f")], onPage=deco)])
story = section("en") + section("fr")
doc.build(story[:-1])
print(OUT)
