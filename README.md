# OCTOPUS (NINE HANDS | DSP)

Plugin VST3 (Windows / macOS) **pour REAPER** : un écran façon multi-effet (inspiré du Quad Cortex mini) qui pilote
la chaîne d'effets de la piste où il est inséré. Pédale sans jacks qui occupe toute la fenêtre (tailles 60 %, 75 %,
100 % ; taille par défaut au choix dans le menu, 60 % au départ), écran sur presque toute la façade, 4 footswitchs hexagonaux inox surmontés de leur lettre A-D rétroéclairée.

<p align="center">
  <a href="https://github.com/Nine-Hands-Dsp/Octopus/releases/latest/download/Octopus-VST3-Windows.zip"><img alt="Télécharger pour Windows" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-Windows-0078D6?style=for-the-badge"></a>
  <a href="https://github.com/Nine-Hands-Dsp/Octopus/releases/latest/download/Octopus-VST3-macOS.zip"><img alt="Télécharger pour macOS" src="https://img.shields.io/badge/%E2%AC%87%20T%C3%A9l%C3%A9charger-macOS-111111?style=for-the-badge&logo=apple&logoColor=white"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Octopus/releases/latest/download/Octopus-Notice.pdf"><img alt="Télécharger la notice (PDF)" src="https://img.shields.io/badge/%F0%9F%93%96%20Notice-PDF-919496?style=for-the-badge"></a>
  <br>
  <a href="https://github.com/Nine-Hands-Dsp/Octopus/releases/latest"><img alt="Dernière version" src="https://img.shields.io/github/v/release/Nine-Hands-Dsp/Octopus?label=derni%C3%A8re%20version&color=919496"></a>
</p>

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
v1.11.0 : nouvel écran d'accordeur (ligne d'infos centrée, note plus petite, mètre fin qui s'efface aux bouts, aiguille verte de la taille de la barre centrale quand c'est juste, POLY avec un mètre fin par corde, boutons en mots en bas) ; en-tête à écarts égaux ; arête arrondie du boîtier plus discrète ; logos descendus.
v1.11.1 : piste armée dans REAPER : IN, OUT et REC en rouge (clignotent quand la piste est armée, fixes pendant l'enregistrement ; seule la piste d'Octopus compte), couleur et glow réglables (clic droit sur IN / OUT / REC) ; les réglages de couleur et de glow ne sont écrits qu'une fois, à la fermeture du nuancier.
v1.12.0 : voies parallèles (split / merge) : 2 ou 3 voies côte à côte (empiler deux amplis, ampli + IR...), créées par clic droit « Split here » ou en glissant un bloc sous un autre ; points Y / M, mixeur par voie (niveau, pan, phase, mute) en cliquant sur M, groupe repliable (clic sur Y), coupable et assignable à un footswitch ; quatre petits effets JS NINE HANDS installés par Octopus, piste à 8 canaux.
v1.12.1 : mini CHROMA : chevron rouge à sa gauche, vers le bas sous la note affichée, vers le haut au-dessus ; il disparaît quand la note est juste ou sans son.
v1.12.2 : mini CHROMA : chevrons dans la case, à droite de l'anneau : un chevron quand la note est un peu fausse, deux à partir de 10 cents (vers le bas : trop bas, vers le haut : trop haut).
v1.13.0 : en-tête redessiné, sans cadres (badge carré, mini CHROMA et Semitones posés sur le fond ; séparateurs fins, ‹ BANK 01 ›, PRESET / STOMP soulignés, SAVE et BANKS avec icône) ; footswitch « A / B » d'un groupe de voies parallèles : chaque appui ne laisse sonner qu'une voie (fondu de 10 ms).
v1.13.1 : bandeau du haut moins haut (environ 10 px), la chaîne de blocs remonte d'autant.
v1.14.0 : fenêtre d'ajout : tri A-Z ou par type (titres colorés), nombre de plugins sur chaque filtre, indices quand la liste continue (fondu, MORE cliquable, barre de défilement, « 1 - 24 of N »), aussi dans la liste des presets ; icône de Darkbass (tête de basse, famille DRIVE) ; mini CHROMA : chevrons vers le haut quand la note est trop basse, vers le bas quand elle est trop haute.
v1.15.0 : fenêtre d'ajout : nombres des filtres sans pastille ; en A-Z la liste défile vers le bas ligne à ligne (molette, MORE) comme en TYPE ; descriptions au survol pour Ampire, Anode, Darkbass, Impulse et Screamer.
v1.16.0 : fenêtre d'ajout redessinée : emplacement cliquable à côté du titre (au début, avant un bloc, à la fin), tri A-Z / TYPE en interrupteur, « N selected ✕ » pour vider la sélection, ADD plein, rescan en icône, recherche pleine largeur avec loupe, filtres en onglets soulignés ; cases de même hauteur et même écart.
v1.17.0 : menus clic droit redessinés (fond sombre arrondi, icônes, en-tête à pastille sur tous les menus) ; STOMP d'un footswitch en grille de blocs qui reste ouverte (plusieurs blocs cochés d'affilée, groupe en une tuile A <-> B, « N selected » + CLEAR) ; bandeau 5 plus bas, textes recentrés, BANK et flèches à la taille des autres, PRESET bleu / STOMP orange sans soulignement, badge plus petit ; REC seul en rouge sous IN (fixe quand la piste est armée, clignote pendant l'enregistrement) ; écran noir uni.
v1.18.67 : taille minimale 60 % (50 % retiré du menu Plugin size), fenêtre du DAW verrouillée (jamais sous 60 %) ; nouvelles versions des pédales prises en compte (menus clic droit au tronc commun) : Doorn 4.1.0, Peachy 2.1.0, Screamer 1.4.0, Strong Pickle 1.6.0, Darkbass 1.2.0, Anode 2.1.0, Ring Think 3.1.0, Small Tone 2.1.0, Phase 90 2.1.0, Fog Z 2.1.0, Fantomine 2.1.0, FreakOut 1.4.0, Holyverb 1.7.0, Pipeline 1.7.0, Ngate 2.1.0, Wack'd 2.1.0, Echoflex 2.1.0, Eight Echo 2.1.0, Arc 3.2.0, Crycat 2.1.0, Wharry 2.1.0, Semitones 2.1.0, Synthetika 1.3.0, Textural 2.1.0, Freeze Shimmer 1.5.0, FM-9 1.4.0, Ampire 1.2.0, Impulse 1.6.0, Spindeck 1.1.0, Chroma 2.1.0.
v1.18.69 : FM-9 v1.4.1 pris en compte (description : horloge réglée automatiquement pour chaque firmware, surveillance en direct).
v1.18.70 : Freeze Shimmer v1.5.2 (halo du footswitch qui pulse en freeze, LED MODE blanche en page A et bleue en page B).
v1.18.68 : Freeze Shimmer v1.5.1 (le halo du footswitch pulse pendant le freeze).
v1.18.66 : FM-9 v1.3.19 pris en compte (description : catégorie Updates et tri du catalogue).
v1.18.65 : FM-9 v1.3.18 pris en compte (description : horloge jusqu'à 960 MHz, lumières des touches).
v1.18.63 : FM-9 v1.3.17 pris en compte.
v1.18.62 : FM-9 v1.3.16 pris en compte (description).
v1.18.61 : FM-9 v1.3.15 pris en compte.
v1.18.60 : FM-9 v1.3.14 pris en compte.
v1.18.59 : FM-9 v1.3.13 (catalogue en galerie, 262 firmwares compatibles) pris en compte.
v1.18.58 : FM-9 v1.3.12 (catalogue sans blocage) pris en compte.
v1.18.57 : FM-9 v1.3.11 (canal MIDI envoyé au firmware) pris en compte.
v1.18.56 : FM-9 v1.3.10 pris en compte.
v1.18.55 : FM-9 v1.3.10 pris en compte.
v1.18.54 : Impulse v1.5.5 pris en compte (description : bibliothèque d'IR et synchronisation TONE3000).
v1.18.53 : FM-9 v1.3.9 pris en compte.
v1.18.52 : FM-9 v1.3.8 pris en compte.
v1.18.51 : recherche de la fenêtre d'ajout : revenir dans le champ ne sélectionne plus tout le texte (la touche suivante l'effaçait), écran redessiné quand la recherche prend ou perd le clavier.
v1.18.50 : FM-9 v1.3.7 pris en compte.
v1.18.49 : Peachy v2.0.1 (halo des LED corrigé) pris en compte.
v1.18.48 : FM-9 v1.3.6 pris en compte.
v1.18.47 : FM-9 v1.3.5 (MIDI Learn de OCT- / OCT+) pris en compte.
v1.18.46 : ARC v3.1.2.
v1.18.45 : FM-9 v1.3.4 (MIDI Learn de tous les potards, configurations MIDI, écran interactif) pris en compte, description mise à jour.
v1.18.44 : Peachy v2.0.0 (refonte : potards animés, LED rouges, écran de presets) prise en compte, description mise à jour.
v1.18.43 : Small Tone v2.0.1 (chorus plus présent) pris en compte.
v1.18.42 : Darkbass v1.1.5 (footswitch BYPASS lu dans le bon sens : la tuile suit la pédale).
v1.18.41 : ARC v3.1.1.
v1.18.40 : FM-9 v1.3.3 (potards sans point indicateur) pris en compte.
v1.18.39 : FM-9 v1.3.2 (logements noirs, réglage Lights + Glow des lumières) pris en compte.
v1.18.38 : FM-9 v1.3.1 (nouveau design de la façade, nouveaux potards) pris en compte.
v1.18.37 : FM-9 v1.3.0 (firmwares matériels de la FM-1 dans un émulateur de la puce, catalogue de 300 firmwares) : description mise à jour.
v1.18.36 : Synthetika v1.2.1 (justesse avec plusieurs cordes qui sonnent, notes graves stables).
v1.18.35 : FM-9 v1.2.0 (skins aux coloris de la FM-1, couleurs à la volée, profil KeyStep 37, export NKS) : description mise à jour.
v1.18.34 : Synthetika v1.2.0 (suivi des arpèges et des attaques fortes, sortie sans saturation ni crachotement).
v1.18.33 : FM-9 v1.1.0 (firmwares de la FM-1 : Hortator fourni, installation depuis GitHub) : description mise à jour.
v1.18.32 : FM-9 v1.0.0 (nouveau synthé FM d'après la M-VAVE FM-1, famille SPECIAL) : description dans la fenêtre d'ajout d'effets, plugin ajouté à la liste de démonstration.
v1.18.31 : Synthetika v1.1.1 (corrections audio : bypass exact, entrée mono, modulation sans clic, distorsion suréchantillonnée).
v1.18.30 : Synthetika v1.1.0 (moteur audio refait : attaques nettes, bends et slides suivis, basse et 7 / 8 cordes dès 28 Hz).
v1.18.29 : Synthetika v1.0.1 (tailles de fenêtre 100 / 125 / 150 / 200 %).
v1.18.28 : Synthetika v1.0.0 (nouveau synthé guitare, famille SPECIAL) : description dans la fenêtre d'ajout d'effets, plugin ajouté à la liste de démonstration.
v1.18.27 : ARC v3.1.0 (écran de corde « flux d'archet »).
v1.18.0 : CHROMA : barre du bas retirée, ligne du haut MODE · ACCORDAGE · A 440 · MUTE et croix pour quitter, tambour pour le mode, menu des accordages aux notes alignées en colonne ; banks : COLOURS AND GLOW aligné sur les tuiles ; fenêtre d'ajout : onglets sans soulignement, défilement vertical fluide en A-Z et en TYPE, plus de fondu ni de texte MORE (flèche bleue seule), tambour pour l'emplacement, pastille et cases du titre au même contour, flèche rescan plus petite.
v1.18.1 : fenêtre d'ajout : tant que le tambour de l'emplacement est ouvert, plus de survol des lignes ni de bulle de description.
v1.18.2 : tambours (Semitones, mode de CHROMA, emplacement) plus près de leur bouton, centrés dessus, fond presque opaque et léger halo pour la lisibilité ; grille STOMP en cases plus petites (texte et icône inchangés) ; champs de texte aux coins arrondis.
v1.18.3 : tambours du mode de CHROMA et de l'emplacement avec le même cylindre et le même fonctionnement que Semitones, bande de sélection toujours à la même distance de son bouton ; accordeur POLY centré (même écart en haut et en bas), noms de cordes en IBM Plex Mono gras.
v1.18.4 : mixeur d'un groupe : double clic sur le rond du niveau = 0 dB (gain unité), sur le pan = centre.
v1.18.5 : grille STOMP en cases carrées ; menu d'un bloc : Focus plug-in window (fenêtre du VST au premier plan), Bypass mode sous Bypass, séparateurs revus.
v1.18.6 : Echoflex v2.0.0 reconnu sous son nom actuel « Echoflex » (chaîne de démonstration, tests) ; bulle de description mise à jour (AGE, tap tempo, synchro DAW).
v1.18.7 : Ngate v2.0.0 reconnu sous son nom actuel « Ngate » (chaîne de démonstration, tests) ; bulle de description mise à jour (LEARN, SOFT / HARD, BASS).
v1.18.8 : Holyverb v1.6.0 (nouvelle pédale graphite à la cathédrale dorée) ; bulle de description mise à jour (8 modes, PRE DELAY, TAILS, pédale d'expression).
v1.18.9 : Eight Echo v2.0.1 (nouveau visuel plein cadre, barre de presets) ; bulle de description mise à jour (11 modes, looper 40 s, synchro DAW, 7 mémoires).
v1.18.10 : Small Tone v2.0.0 (pédale entièrement refaite, réglage Glow) ; bulle de description mise à jour (RATE, DIN. / DEPTH, synchro tempo sur la LED STATUS).
v1.18.11 : voies parallèles : un clic sur la petite lettre A / B / C d'une voie agit comme le footswitch A <-> B (cette voie seule ; sur la voie qui sonne, la suivante).
v1.18.12 : Textural v2.0.0 (pédale refaite NINE HANDS | DSP, potards animés, barre de presets, réglage Glow) ; bulle de description mise à jour (modes Delay, Auto Freeze, Layers, REVERSE, PITCH).
v1.18.13 : Wack'd v2.0.0 (pédale refaite NINE HANDS | DSP, potards animés, menus Octopus, réglage Glow) ; bulle de description mise à jour (MODE, KRAZY Wack'd).
v1.18.14 : Phase 90 v2.0.0 (pédale refaite NINE HANDS | DSP, potard animé, LED SYNC, réglage Glow) ; bulle de description mise à jour (SCRIPT / BLOCK, synchro tempo sur la LED SYNC).
v1.18.15 : Freeze Shimmer v1.4.0 (pédale refaite NINE HANDS | DSP, givre réaliste, potards animés sur deux pages, menus Octopus, réglage Glow) ; bulle de description mise à jour (freeze au footswitch, shimmer, scènes, page B).
v1.18.16 : Freeze Shimmer v1.4.1 (LED centrale = effet actif / bypass, halo du footswitch = freeze).
v1.18.24 : ARC v3.0.0 (pédale entièrement refaite, format double) : bulle de description mise à jour.
v1.18.26 : Doorn v4.0.0 (nouveau design v018, potards animés avec flou, réglage Glow) : bulle de description mise à jour.
v1.18.25 : Crycat v2.0.0 (nouvelle façade NINE HANDS | DSP) : bulle de description mise à jour.
v1.18.23 : Wharry v2.0.0 (nouvelle façade NINE HANDS | DSP) : bulle de description mise à jour.
v1.18.22 : Ring Think v3.0.3 (image 2350 x 1800).
v1.18.21 : Ring Think v3.0.2 (nouveaux footswitchs).
v1.18.20 : Ring Think v3.0.1 (visuel haute définition, net jusqu'à la taille maximale).
v1.18.19 : Ring Think v3.0.0 (pédale refaite NINE HANDS | DSP au format double 2350 x 1800, potards animés, LED et réglage Glow, pastille presets / ligne d'état, menus Octopus) ; bulle de description mise à jour.
v1.18.18 : Fantomine v2.0.0 (pédale refaite NINE HANDS | DSP au format double haute résolution, néon, potards animés, barre de presets, menus Octopus, réglage Glow) ; bulle de description mise à jour.
v1.18.17 : Fog Z v2.0.0 (pédale refaite NINE HANDS | DSP en haute résolution, gros bouton PRESET animé, barre de presets, menus Octopus, réglage Glow) ; bulle de description mise à jour.
