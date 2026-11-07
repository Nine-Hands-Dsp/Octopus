# OCTOPUS (NINE HANDS | DSP)

Plugin VST3 (Windows / macOS) **pour REAPER** : un écran façon multi-effet (inspiré du Quad Cortex mini) qui pilote
la chaîne d'effets de la piste où il est inséré. Pédale sans jacks qui occupe toute la fenêtre (tailles 50 %, 60 %, 75 %,
100 % ; taille par défaut au choix dans le menu, 60 % au départ), écran sur presque toute la façade, 4 footswitchs hexagonaux inox surmontés de leur lettre A-D rétroéclairée.

## Ce que fait Octopus
- **L'écran montre la chaîne de la piste** (tous les effets sauf Octopus), en blocs colorés par famille
  (accordeur, ampli, baffle, drive, dynamique, EQ, wah / filtre, delay, reverb, modulation, pitch, spécial, utilitaire en blanc),
  reliés de IN à OUT. Ce que l'on fait dans REAPER apparaît aussi à l'écran (relecture ~7 fois par seconde).
- **Clic** sur un bloc : ON / OFF. **Double-clic** : paramètres à l'écran (8 par page, glisser pour régler, bouton WINDOW
  pour la fenêtre du plugin). **Glisser** : déplacer. **Clic droit** : ouvrir la fenêtre, assigner à un footswitch,
  traitement parallèle, déplacer, insérer avant / après, supprimer. Touche Suppr : supprime le bloc survolé.
- **Ajouter** (case « + ») : liste des plugins installés fournie par REAPER, recherche et filtres
  (NINE HANDS | DSP, VST3, VST, CLAP, JS, AU ; instruments masqués). Plusieurs plugins se choisissent à la suite (numérotés dans
  l'ordre des clics) puis ADD les insère tous dans la piste, dans cet ordre, en un seul point d'annulation.
- **Presets de chaque plugin** (bouton PRESETS du panneau des paramètres) : presets d'usine et utilisateur de REAPER, numérotés,
  chargés d'un clic, flèches précédent / suivant. Pour lire les noms, chaque preset est chargé un instant puis l'état exact
  du plugin est rétabli (vst_chunk / clap_chunk). **SAVE USER** : presets utilisateur gardés par Octopus pour chaque plugin
  (`NINE HANDS | DSP/Octopus/Plugin presets/<plugin>.xml` dans le dossier de données utilisateur, état exact du plugin), en tête de
  liste avec une étoile ; clic droit : charger, remplacer, renommer, supprimer.
- **Accordeur** : quand un bloc CHROMA est allumé, Octopus affiche l'écran de CHROMA (même dessin) et fait tourner le même
  moteur (TunerEngine de CHROMA) sur le son qui le traverse ; mode, La de référence et accordage lus dans CHROMA
  (paramètres + état du plugin) ; écran un peu réduit, boutons TUNINGS, MUTE, MODE, EXIT. TUNINGS (ou clic sur le nom de
  l'accordage) : presets d'accordage de CHROMA (favoris, usine par instrument, accordages perso de `library.xml`) ; le choix est
  gardé par Octopus avec le projet et s'efface si l'accordage est changé dans CHROMA lui-même.
- **Banks / presets** : 16 banks x 4 presets (1A à 16D). Un preset garde la chaîne complète (plugins, ordre, réglages,
  ON / OFF, assignations STOMP), au format de REAPER. SAVE (jamais d'écrasement sans confirmation), BANKS (charger,
  enregistrer, renommer, copier / coller, effacer, exporter / importer un fichier `.octobank`, couleur de chaque footswitch A-D en mode PRESET). En-tête : PRESET, STOMP,
  SAVE, BANKS ; clic sur l'étiquette du preset (1A...) : couleur propre à ce preset.
- **Rappel sans coupure inutile** : si le preset a la même suite de plugins que la chaîne actuelle, seuls les blocs dont les
  réglages diffèrent sont rechargés, les autres ne font que passer ON / OFF (changement quasi instantané). Sinon la chaîne est
  remplacée. Octopus n'est jamais recréé ni déplacé. Un plugin désinstallé est signalé, les autres sont placés correctement.
- **Footswitchs A-D** : mode **PRESET** (rappellent les presets A-D de la bank affichée, lettre dans la couleur choisie dans BANKS, bleu par défaut) ou **STOMP**
  (chacun bascule un ou plusieurs blocs, lettre de la couleur du bloc). Fonctionnement d'Echoflex (action à l'appui, bouton
  assombri), seule la lettre s'allume, en fondu.
- **MIDI** (M-Vave Chocolate Plus) : MIDI learn au clic droit (footswitchs A-D, bank -, bank +, mode), momentané ou à
  accrochage (127 / 0), **Program Change 1-64 = presets 1A-16D**. Les paramètres `fsA`..`fsD`, `bankDown`, `bankUp`, `mode`
  acceptent aussi le MIDI learn de REAPER. Le MIDI continue vers les effets suivants.
- Annulation : chaque action est un point d'annulation de REAPER (« Octopus: ... »).
- Le son traverse Octopus sans modification (latence nulle).

## Comment ça marche
REAPER donne aux plugins VST3 une interface propre (`IReaperHostApplication`, SDK officiel de REAPER,
`reaper_vst3_interfaces.h`) : la piste qui héberge le plugin et toute l'API de REAPER (les fonctions de ReaScript :
`TrackFX_AddByName`, `TrackFX_Delete`, `TrackFX_SetEnabled`, `TrackFX_CopyToTrack`, `GetTrackStateChunk`...).
Octopus se retrouve dans la chaîne grâce à un jeton propre à chaque instance (paramètre « Octopus ID »), même s'il est
renommé ou s'il y a plusieurs Octopus. Toutes les actions se font sur le thread de messages (timer à 30 Hz du processeur,
fenêtre ouverte ou fermée) ; le thread audio ne fait que lire le MIDI dans une file sans verrou.
Les presets sont recréés par REAPER lui-même : une piste temporaire reçoit les effets (SetTrackStateChunk), qui sont
ensuite déplacés à leur place (TrackFX_CopyToTrack), puis la piste temporaire est supprimée (sans rafraîchissement
intermédiaire, un seul point d'annulation).

Dans un autre DAW, Octopus reste inactif et l'écran l'indique. Il pilote la chaîne normale de la piste ou ses effets
d'entrée (selon l'endroit où il est inséré), pas les effets de prise. REAPER 6.37 ou plus récent ; liste des plugins
installés : REAPER 7.

## Anti-gel v2
JUCE 8.0.15, fenêtre opaque, moteur de rendu logiciel sous Windows (réappliqué si la fenêtre hôte change), aucun calcul
lourd dans `paint()` (caches du fond, des footswitchs et de l'éclairage des LED refaits par le timer), chien de garde
(redessin non servi en 1,5 s : moteur réappliqué), aucun appel à l'hôte ni allocation dans le thread audio.

## Visuel
`python3 tools/make_frame.py` génère `background.png` à partir du visuel fourni (`design/frame_photo.jpg` : marquages effacés, vitre vidée, 148 colonnes retirées pour les proportions de l'écran, logo NINE HANDS | DSP net) ; `python3 tools/make_art.py` génère l'ancien fond dessiné (`design/background_procedural.png`) (boîtier acier brossé anthracite sans jacks, lunette et vitre, logos NINE HANDS | DSP et OCTOPUS), `fs_off.png` / `fs_on.png` (footswitch inox, photo réelle détourée `design/footswitch_inox_source.png`,
ombres selon la lumière en haut à gauche). Positions communes : `Layout.h`.

## Build et vérifications
- GitHub Actions (`.github/workflows/build.yml`) : artefacts `Octopus-VST3-Windows` et `Octopus-macOS-VST3`, et job `tests`.
- `tools/reaper_sim` : le pont REAPER testé contre une API de REAPER simulée (états de piste au format RPP) : 36 tests
  (presets, rappel partiel, Octopus au milieu de la chaîne, effets d'entrée, plugin manquant, deux Octopus...).
- `tools/render` : fenêtre compilée sous Linux sur une chaîne simulée, tests du moteur et captures (`design/renders/`).
- `tools/host_check` : charge le VST3 compilé comme un hôte (paramètres, son inchangé, état, fenêtre).

v1.0.0 : première version.
v1.0.1 : footswitchs tournés de 30° (un plat de l'écrou en haut), LED au-dessus de chaque footswitch, lettre à gauche ; éclairage des LED qui retombe à zéro avant le bord du calque.
v1.0.2 : les LED sont remplacées par les lettres A, B, C, D rétroéclairées au-dessus des footswitchs (gravées quand elles sont éteintes, lumineuses dans la couleur du mode quand elles sont allumées).
v1.0.3 : lettres moins lumineuses ; seul le tracé de la lettre s'allume (plus de tache de lumière sur la façade).
v1.0.4 : lettres A-D un peu plus lumineuses.
v1.0.5 : très léger halo autour des lettres allumées.
v1.0.6 : halo des lettres allumées accentué (dégradé progressif qui épouse la lettre).
v1.1.0 : taille par défaut = ancienne taille 100 % (affichée 60 %), 75 % et 100 % plus grands ; jacks retirés et pédale agrandie dans la fenêtre ;
  accordeur CHROMA dans Octopus ; ajout de plusieurs effets d'un coup ; presets usine / utilisateur de chaque plugin ; famille utilitaire en blanc.
v1.1.1 : double-clic fiable (détecté par Octopus, la bascule d'un double-clic lent est défaite) ; noms des paramètres plus grands ;
  paramètres à crans réglés cran par cran ; paramètres MIDI derrière le bouton MIDI ; BYPASS à côté de la croix.
v1.1.2 : presets utilisateur des plugins (SAVE USER) ; en-tête PRESET / STOMP séparés, SAVE après BANKS ; logo NINE HANDS | DSP plus petit
  et centré entre le haut de la pédale et l'écran ; couleur des footswitchs A-D au choix (BANKS) ; accordeur plus compact et menu
  TUNINGS avec les presets d'accordage de CHROMA.
v1.1.3 : trois rangées de blocs, même espace entre les blocs en largeur et en hauteur ; cases des footswitchs décollées du bas de
  l'écran ; logo NINE HANDS | DSP encore réduit ; taille 50 % et choix de la taille par défaut (réglage commun, `settings.xml`).
v1.1.4 : cases des footswitchs à égale distance entre elles et des bords de l'écran.
v1.1.5 : ligne de liaison entre deux rangées centrée entre le nom des blocs et la rangée suivante ; grille centrée, lignes de retour
  de même longueur à gauche et à droite.
v1.1.6 : bloc utilitaire allumé en blanc plein (icône sombre), bien distinct d'un bloc éteint ; ligne de liaison entre les rangées un peu remontée.
v1.1.7 : blanc du bloc utilitaire allumé 5 % moins lumineux.
v1.1.8 : nouveau logo OCTOPUS (pieuvre, « fx chain controller ») en haut à droite, en symétrie du logo NINE HANDS | DSP.
v1.1.9 : logo OCTOPUS agrandi (54 unités de haut).
v1.2.0 : nouveaux footswitchs inox (photo fournie, écrou plat en haut) ; blanc du bloc utilitaire allumé encore 5 % plus transparent.
v1.2.1 : logo OCTOPUS dessiné par le plugin à la taille exacte (net à toutes les tailles) ; footswitchs remontés, anneau lumineux autour de chaque footswitch (diffuseur gris éteint, couleur du footswitch allumé, avec halo, comme la Phase 90).
v1.2.2 : logo OCTOPUS avec une marge transparente autour du dessin (plus rien de rogné en bas à la mise à l'échelle), redessiné à la bonne taille pendant un redimensionnement.
v1.2.3 : anneau lumineux placé dans le footswitch, autour du bouton qui s'enfonce (diffuseur gris éteint) ; écrou à sa taille d'origine.
v1.2.4 : lettres A-D abaissées, à égale distance de l'écran et des footswitchs.
v1.2.5 : étiquette du preset (1A...) dans la couleur de son footswitch ; clic sur le nom du preset en haut pour le renommer ; preset vide = écran vide comme au démarrage (chaîne vidée, un point d'annulation).
v1.2.6 : nouveau boîtier d'après le design fourni : acier brossé anthracite au brossage fin, liserés usinés clairs sur le pourtour et autour de l'écran.
v1.2.7 : texture d'acier brossé plus réaliste (tools/brushed.py : lignes de brossage multi-échelles sur toute la largeur, rares rayures, reflet anisotrope en bandes verticales douces).
v1.3.0 : boîtier repris du visuel fourni (photo, footswitchs compris), écran du plugin dans son ouverture, lettres A-D et anneaux lumineux dessinés par le plugin sur les footswitchs de la photo ; repère 1267 x 981 (fenêtre un peu plus haute).
v1.3.1 : photo du boîtier redressée (prise un peu en plongée) : côtés de la pédale parfaitement verticaux, aussi larges en haut qu'en bas.
v1.3.2 : anneau lumineux calé sur le bord du chapeau de chaque footswitch (centre et rayon mesurés un par un sur la photo).
v1.3.3 : anneau lumineux qui suit la forme de chaque bouton (chapeau + flanc vu en perspective, contour relevé sur chaque footswitch).
v1.3.4 : retour aux footswitchs inox des versions précédentes (vus de dessus, anneau lumineux dans la gorge autour du bouton) ; footswitchs de la photo effacés du boîtier.
v1.3.5 : aluminium brossé du boîtier assombri (x 0,78).
v1.3.6 : façade en aluminium brossé entièrement générée (tools/brushed_alu.py : stries de longueurs variées en fondu, rayures fines, grain, reflets anisotropes) : plus aucune tache de retouche ; arête du boîtier usinée avec reflet en haut à gauche ; lunette de l'écran de la photo.
v1.3.7 : bas de la pédale raccourci (repère 1267 x 944, fenêtre 60 % = 993 x 740) ; lettres et footswitchs remontés.
v1.3.8 : lettres A-D centrées entre l'écran et les footswitchs.
v1.3.9 : lettres A-D alignées sur le centre de leur footswitch.
v1.4.0 : couleur propre à chaque preset (clic sur l'étiquette en haut à gauche) ; en-tête PRESET, STOMP, SAVE, BANKS ; aluminium plus sombre ; cadre de l'écran redessiné, régulier tout autour (plus de grain à droite).
v1.5.0 : plugins favoris dans la fenêtre d'ajout (bouton Favorites, petite étoile vide / jaune en bout de ligne, liste commune à tous les projets) ; bypass de la pédale (son propre interrupteur : les queues de delay / reverb continuent) quand le plugin en a un, sinon bypass de REAPER, au choix par plugin (clic droit > Bypass mode) ; rappel de preset sans rechargement avec le bypass de la pédale.
v1.5.1 : accordeur CHROMA en résolution doublée : graduations et aiguille plus fines (MONO 1,25 cent, POLY 2 cents, STROBE bandes deux fois plus fines).
v1.5.2 : Strong Pickle reconnu comme fuzz / distorsion : bloc orange avec le symbole de l'onde écrêtée (au lieu de la prise des effets inconnus).
v1.6.0 : nom du preset du plugin à côté de son nom dans le panneau des paramètres ; accordeur intégré (B+C, sans plugin CHROMA : MUTE, MODE, TUNINGS, La 430-450 Hz) ; doubles appuis E (A+B, CC 26 par défaut), B+C (CHROMA) et F (C+D, CC 28 par défaut) réglables (accordeur, bank -, bank +, mode, ou envoi d'un CC 0-127 aux effets suivants), sérigraphiés entre les footswitchs ; commande MIDI « Tuner on / off » ; guide MIDI et CC à part (docs/Octopus_MIDI_Guide_EN-FR.pdf, tools/midi_guide.py).
v1.7.0 : footswitchs E (A+B) et F (C+D) complets : cases E et F à l'écran, effets à basculer (modes PRESET et STOMP, enregistrés avec les presets), MIDI learn et paramètres Footswitch E / F ; sérigraphie dessinée ] E [, ] F [ et symbole de l'accordeur, éclairés comme des LED (couleur des effets, vert pendant l'accordage) ; nuancier (couleurs rapides, rangée MY COLOURS, teinte, saturation, luminosité) pour les couleurs des presets et des footswitchs.
v1.7.1 : MIDI de E, F et B+C automatique : E = CC de A + CC de B reçus ensemble, F = C + D, B+C = B + C (plus de learn à part qui prenait le CC de A) ; un learn de l'accordeur avec le CC d'un footswitch est refusé ; les CC pris à A / B par un learn de la 1.7.0 leur sont rendus.
v1.7.2 : délai des doubles appuis élargi de 0,14 s à 0,25 s (A+B, B+C, C+D reconnus même quand les deux appuis ou les deux CC n'arrivent pas tout à fait ensemble).
v1.8.0 : E (A+B) et F (C+D) deviennent des footswitchs comme A-D (même menu, presets E / F en mode PRESET, blocs en STOMP) : 6 presets par bank (1A-16F, 96 presets ; anciens projets et .octobank convertis en gardant leurs lettres ; Program Change 1-64 = A-D de chaque bank) ; panneau BANKS à 6 lignes, cases des banks alignées sur 1A-1F, couleurs A-F + TUNER ; curseur GLOW (opacité du halo seulement) dans le nuancier et menu Letter glow ; couleur et halo propres au symbole de l'accordeur.
v1.8.1 : clic droit des footswitchs A-F : « Color & Glow... » (nuancier du footswitch et halo) à la place de Plugin size et Letter glow ; menu B+C : « Color & Glow... » à la place de Test now et Tuner symbol colour and glow. ; footswitchs sans attente : chaque appui agit tout de suite, et si le voisin suit dans les 0,25 s le premier appui est annulé au profit du double appui ; clic droit sur les lettres A-D = menu de leur footswitch.
v1.8.2 : B+C (accordeur) aussi facile à faire que A+B et C+D : le délai du double appui part de la fin du premier appui (un preset long à charger ne le raccourcit plus) ; menus « Color & Glow » sans points de suspension ; envoi de CC retiré du menu de B+C.
v1.8.3 : même marge de 3 tout autour de la pédale (repère 1249 x 934) ; B+C mesuré à l'heure d'arrivée des CC (reconnu même après un chargement long) ; menu B+C : Nothing, Tuner, PRESET / STOMP mode ; fenêtre d'ajout : clic droit sans effet, bulle de description au survol des plugins NINE HANDS | DSP ; BANKS : un point par preset enregistré sous chaque bank, lettres et TUNER recentrés.
v1.8.4 : B+C (« G ») comme A+B / C+D : l'accordeur s'ouvre tout de suite, n'importe quel footswitch le ferme et ramène le preset d'avant ; le Letter glow s'applique aussi au symbole de l'accordeur ; bulle de description des plugins NINE HANDS | DSP au bout de 0,8 s de survol.
v1.8.5 : B+C n'ouvre plus l'accordeur : il se commande par MIDI learn (clic droit sur le symbole de l'accordeur : MIDI Learn, Clear MIDI, latching, Color & Glow ; clic sur le symbole : on / off) ; bouton STOMP en orange quand le mode STOMP est sélectionné.
v1.8.6 : logo NINE HANDS | DSP 30 % plus petit sur la pédale.
v1.9.0 : Semitones intégré (transposition -12 à +12 en entrée, bouton à gauche de la bank, tambour vertical des 21 presets, globale ou enregistrée avec un preset, couleur et halo) ; mini CHROMA dans l'en-tête (note qui sort, anneau vert juste / rouge faux, couleurs et halo).
v1.9.1 : tambour de Semitones centré sous le bouton SEMITONES, valeurs et noms bien dans l'encadré de sélection.
v1.10.0 : nouveau cadre (plan du 07/11) : façade en alu brossé noir, écran plus grand (1177 x 720), logos OCTOPUS et NINE HANDS | DSP, footswitchs hexagonaux inox, lettres et marquages ] E [ / ] F [ / accordeur repris des SVG fournis ; icône de lampe et famille ampli pour Anode ; mot SEMITONES plus grand.
v1.10.1 : lettres A-D à mi-distance entre l'écran et les footswitchs ; logo OCTOPUS réduit et centré entre le bord de la pédale et l'écran ; NINE HANDS | DSP aligné sur le bas du logo OCTOPUS.
v1.10.2 : cases A-F de l'écran à espacement égal entre elles et avec les bords de l'écran (côtés et bas).
