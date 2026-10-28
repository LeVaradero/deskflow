# Deskflow : placer un client à côté d'un écran précis du serveur

Date : 2026-10-28. Base : Deskflow **v1.27.0** (tag `v1.27.0`).

## Pourquoi

Le serveur Deskflow (PC « Corsair », Windows 11) a deux écrans qui ne forment pas
un rectangle :

| Écran | Position Windows (pixels physiques) | Taille |
|---|---|---|
| Samsung G9 32:9 (haut) | X = -1292, Y = -1440 | 5120 × 1440 |
| 21:9 (bas, écran principal) | X = 0, Y = 0 | 2560 × 1080 |

Le 21:9 est centré sous le G9 (centres à X = 1268 et X = 1280). Il reste donc un trou
de 1292 px de large sous chaque débord du G9.

Physiquement, le laptop client (« LAPTOPDUFLEX », Windows) est posé **dans le trou de
gauche** : sous le débord gauche du G9 et à gauche du 21:9, à la même hauteur que le
21:9.

```
X: -1292           0                        2560          3828
   ┌──────────────────────────────────────────────────────┐ Y=-1440
   │                        G9                            │
   └──────────────────────────────────────────────────────┘ Y=0
   ┌─────────────┐┌─────────────────────────┐
   │   LAPTOP    ││          21:9           │   (vide)
   └─────────────┘└─────────────────────────┘               Y=1080
```

Deskflow ne connaît que le **rectangle englobant** du bureau du serveur
(`MSWindowsComputer::updateComputerShape`, qui lit `SM_*VIRTUALSCREEN`). Il ne bascule
vers un client que si le curseur touche un bord de ce rectangle
(`Server::onMouseMovePrimary`, `Server.cpp:1663`). Résultat aujourd'hui, avec le laptop
lié au bord gauche de Corsair :

- laptop → Corsair : ça marche à peu près. Le curseur arrive dans la zone vide et
  Windows le recolle quelque part.
- **21:9 → laptop : impossible.** Le curseur bute à X = 0, qui n'est pas un bord du
  rectangle global.
- Le seul passage est le bord gauche du G9, qui ne correspond à rien physiquement.

Cette limite est connue depuis longtemps sans avoir été corrigée (issues #4993, #568,
#4453 fermées, #6257 ouverte depuis 2018). Le cas #4993 est quasiment identique.

## Ce qu'on veut

On veut pouvoir **placer un client dans les coordonnées du bureau du serveur**, comme
dans les paramètres d'affichage Windows. Les passages deviennent alors « logiques »
selon la vraie géométrie.

### Objectifs

1. **21:9 → gauche → laptop** : le curseur arrive sur le bord droit du laptop, à la
   même hauteur relative (proportionnel : haut ↔ haut, milieu ↔ milieu, bas ↔ bas).
2. **Laptop → droite → 21:9** : réciproque exacte, à la même hauteur relative.
3. **Bas du débord gauche du G9 → descendre → laptop** : arrivée sur le bord haut du
   laptop, à la même position horizontale relative.
4. **Laptop → monter → G9** : réciproque exacte, sur le débord gauche du G9.
5. Le bord gauche du G9 ne mène plus au laptop. C'est un choix de config (on retire le
   lien `left`) et ça doit rester réversible sans recompiler.
6. La disposition Windows reste **strictement inchangée**.
7. Seul Corsair (le serveur) a besoin de la version modifiée. Le laptop garde Deskflow
   1.27.0 officiel.

### Non-objectifs

- Aucune modification de l'interface graphique (grille drag & drop). Le placement se
  fait uniquement dans le fichier de config externe, écrit à la main.
- Pas de support macOS/Linux côté serveur. Seul le serveur Windows est visé. Le code
  commun ne doit pas casser les autres plateformes, puisque la CI compile tout.
- Pas de proposition de la modif au projet officiel pour l'instant (possible plus tard).
- On ne gère pas les clients à plusieurs écrans : le laptop est traité comme un seul
  rectangle.

### Anti-objectifs (échec même si « ça marche »)

- **Sans section de placement, le comportement doit rester identique à Deskflow
  officiel**, au bit près.
- Le curseur ne doit jamais rester coincé, ni atterrir à un endroit incohérent avec le
  schéma ci-dessus.
- Aucune régression sur le presse-papier, les raccourcis, les liens classiques
  (`section: links`) ou le verrouillage sur un écran.
- Ne jamais demander de toucher aux paramètres d'affichage Windows.

## Comportement attendu

### Config

Nouvelle section dans le fichier de config serveur externe (format texte Synergy/Deskflow
habituel). Syntaxe proposée :

```
section: placements
	LAPTOPDUFLEX = Corsair(-1292, 0, 1292, 1080)
end
```

Ça signifie : le client `LAPTOPDUFLEX` occupe virtuellement le rectangle
(x, y, largeur, hauteur) dans le système de coordonnées du bureau de `Corsair`, en
pixels, avec les mêmes coordonnées que Windows. Ce rectangle n'a pas besoin d'avoir la
résolution réelle du laptop : la conversion est proportionnelle à la taille que le
client annonce à la connexion.

Config cible complète pour ce setup (le lien gauche de Corsair disparaît, volontairement) :

*(En 1.27, la section `screens` du fichier est ignorée : la liste des machines vient des
réglages du GUI, où `Corsair` et `LAPTOPDUFLEX` sont déjà déclarés.)*

```
section: placements
	LAPTOPDUFLEX = Corsair(-1292, 0, 1292, 1080)
end

section: options
end
```

### Règles de bascule

**Du serveur vers un client placé.** Le serveur doit connaître la liste de ses écrans
(sous Windows : `EnumDisplayMonitors`, mise à jour quand l'affichage change). Quand le
curseur est sur un écran du serveur, dans la zone de saut (`getJumpZoneSize`) d'un bord
de **cet écran** :

- si, juste au-delà de ce bord, il y a un autre écran du serveur, rien ne change
  (Windows déplace le curseur normalement) ;
- sinon, si le point juste au-delà tombe dans le rectangle d'un client placé, on
  bascule vers ce client. Le point d'entrée est la position du curseur convertie
  proportionnellement dans le rectangle placé, puis ramenée à la résolution réelle du
  client et collée au bord opposé (bord droit du laptop si on sort par la gauche, bord
  haut si on sort par le bas, etc.) ;
- sinon, on retombe sur la logique actuelle (bords du rectangle global +
  `section: links`).

**D'un client placé vers le serveur.** Quand le curseur touche un bord du client
placé :

- on convertit sa position en un point du bureau serveur juste à l'extérieur du
  rectangle placé (par exemple bord droit à la hauteur h → `(x+largeur, y + h × hauteur
  / hauteurClient)`) ;
- si ce point tombe sur un écran du serveur, on bascule sur le serveur à ce point ;
- sinon (exemple : bord gauche ou bas du laptop), on retombe sur la logique actuelle.
  Avec la config cible, il ne se passe alors rien.

**Priorité.** Le placement est évalué avant `links`. Les liens classiques continuent de
marcher pour tout ce que le placement ne couvre pas.

**Raccourcis clavier directionnels** (`switchInDirection`). Ils continuent de suivre
uniquement `links`, sans changement. Le placement ne concerne que la souris.
*Décision de Claude : l'utilisateur n'a aucun raccourci configuré (`hotkeys\size=0`).*

**Garde-fous existants.** Les bascules par placement passent par la même vérification
`isSwitchOkay` que les bascules classiques, sans modification : le verrouillage sur un
écran bloque toujours, et le délai, le double-tap et les coins s'appliquent comme
aujourd'hui (les coins restent calculés sur le rectangle global).
*Décision de Claude : l'utilisateur n'utilise aucun de ces garde-fous (tous désactivés
dans sa config), donc on garde le comportement le plus simple.*

**Validation de la config.** Une section `placements` invalide (écran de référence ou
client qui n'est pas une machine connue, client placé deux fois, rectangle de taille nulle ou
négative, syntaxe incorrecte) est une erreur de config, au même titre que les autres :
le serveur refuse la config avec un message clair dans le log. Un rectangle placé qui
chevauche un vrai écran du serveur ou un autre placement produit seulement un
avertissement dans le log.
*Décision de Claude : une faute de frappe doit se voir tout de suite plutôt que de
retomber silencieusement sur `links`.*

**Retour sur le serveur.** Quand le curseur revient sur le serveur, il est placé une
largeur de zone de saut à l'intérieur de l'écran, pour ne pas repartir aussitôt vers le
client.

### Résultat attendu pour ce setup

| Départ | Geste | Arrivée |
|---|---|---|
| 21:9, bord gauche, hauteur `y` | pousser à gauche | laptop, bord droit, même hauteur relative |
| laptop, bord droit | pousser à droite | 21:9, X = 0, même hauteur relative |
| G9, bas, X entre -1292 et 0 | pousser vers le bas | laptop, bord haut, même position relative |
| laptop, bord haut | pousser vers le haut | G9, bas du débord gauche, même position relative |
| G9, bas, X entre 0 et 2560 | descendre | 21:9 (comportement Windows normal) |
| G9, bas, X entre 2560 et 3828 | descendre | rien (trou de droite vide) |
| G9, bord gauche | pousser à gauche | rien |
| laptop, bord gauche ou bas | pousser | rien |

## Contraintes

- Fork GitHub `LeVaradero/deskflow`, à partir du tag `v1.27.0`.
- Compilation **uniquement via GitHub Actions** (`continuous-integration.yml`). Rien ne
  se compile sur le PC. La CI tourne sur un push vers `master` ou sur une pull request.
  Elle compile avec `CMAKE_COMPILE_WARNING_AS_ERROR=ON` et sur toutes les plateformes,
  donc le code commun doit compiler partout sans warning.
- L'artefact à récupérer est le paquet `windows-2022-x64` (installeur).
- Le protocole réseau ne change pas : le client 1.27.0 officiel doit fonctionner tel quel.
- Les coordonnées sont en pixels physiques (process DPI-aware). Les deux écrans sont à
  100 %, mais le code ne doit pas le supposer en dur.
- Issue amont #10030 à garder en tête : le garde-fou contre les « faux » mouvements est
  mal calibré quand l'écran principal n'est pas centré dans le bureau virtuel. C'est
  notre cas, donc il faut vérifier que les nouvelles bascules ne le déclenchent pas.

## Laissé au constructeur

- Noms de classes et de fonctions, découpage du code, endroit exact où stocker la liste
  des écrans (interface plateforme, `PrimaryClient`…).
- Tests unitaires : recommandés sur la géométrie pure (détection de bord par écran,
  conversions proportionnelles dans les deux sens, retombée sur `links`).
- Stratégie de branche dans le fork, tant que la CI produit l'installeur Windows.
- Numéro de version ou suffixe affiché, si ça aide à distinguer la version modifiée.
- Mode du hook Windows et gestion des zones de saut quand Corsair n'a plus aucun lien.
- Correction éventuelle liée à #10030, à condition qu'elle ne s'active que si une
  section `placements` existe.
- Liste d'écrans vide sur macOS/Linux, et sérialisation de la section dans la config.

## Mise en service (côté utilisateur)

1. Sauvegarder `C:\ProgramData\Deskflow\deskflow-server.conf` et
   `%APPDATA%\Deskflow\Deskflow.conf`.
2. Installer l'installeur produit par la CI sur Corsair uniquement.
3. Écrire la config cible dans `C:\ProgramData\Deskflow\deskflow-server.conf`
   (droits admin).
4. Dans Deskflow, cocher l'utilisation de la config externe et désactiver la
   vérification de mises à jour, pour qu'une version officielle n'écrase pas la version
   modifiée.
5. Tester chaque ligne du tableau « Résultat attendu ».

Retour arrière : réinstaller Deskflow 1.27.0 officiel et restaurer les sauvegardes.
