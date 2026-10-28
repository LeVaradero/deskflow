# Placement par écran — Plan d'implémentation

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Permettre de placer un client (le laptop) dans les coordonnées du bureau du serveur
Windows. Le passage de la souris se fait alors par les bords de chaque écran physique, et plus
seulement par ceux du rectangle global.

**Architecture:** Un module de géométrie pure (`server/Placement.*`) contient tous les calculs et
il est testé unitairement. `Config` lit et stocke une nouvelle section `placements`.
`MSWindowsComputer` expose la liste des écrans via `IPrimaryComputer`. `Server` consulte le
placement avant la logique `links` existante, dans `onMouseMovePrimary` et `onMouseMoveSecondary`.
Rien ne change quand il n'y a aucun placement.

**Tech Stack:** C++20, Qt 6 (QtTest pour les tests), Win32 (`EnumDisplayMonitors`), CMake, GitHub
Actions.

**Spec:** `docs/superpowers/specs/2026-10-28-placement-par-ecran-design.md`

## Global Constraints

- Base : tag `v1.27.0` de deskflow/deskflow. Fork : `LeVaradero/deskflow`.
- Compilation uniquement via GitHub Actions. Elle se fait avec `CMAKE_COMPILE_WARNING_AS_ERROR=ON` :
  aucun warning n'est toléré.
- Le code commun (`src/lib/server`, `src/lib/deskflow`) doit compiler sur toutes les plateformes.
  Le code Win32 reste dans `src/lib/platform/MSWindows*`.
- Sans section `placements`, le comportement est strictement identique à la 1.27.0 : aucun chemin
  de code existant ne change.
- Pas de changement du protocole réseau, et pas de changement du GUI.
- Chaque nouveau fichier commence par l'en-tête SPDX du projet (copier celui de
  `ServerConfigTests.h`, avec la licence `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`).
- Style : celui du fichier modifié (clang-format du dépôt, 2 espaces, accolade de fonction à la
  ligne).
- Valeurs de référence (setup réel) : G9 `(-1292, -1440, 5120, 1440)`, 21:9 `(0, 0, 2560, 1080)`,
  placement `LAPTOPDUFLEX = Corsair(-1292, 0, 1292, 1080)`.

## Review Focus

1. **Rebond immédiat à l'entrée sur le serveur.** Si le point d'entrée tombe dans la zone de saut,
   le curseur repart aussitôt vers le laptop. Il doit être décalé de `zoneSize` vers l'intérieur.
   Testé dans Task 1 (`serverEntryIsInsetByZone`).
2. **Bord partagé entre deux écrans du serveur.** Bas du G9 entre X = 0 et 2560 : il ne doit
   jamais déclencher de bascule, Windows gère. Testé dans Task 1 (`noExitWhereAnotherMonitorIsAdjacent`).
3. **Coin du débord (X = -1, Y = -1 sur le G9).** Le curseur est à la fois au bas et à gauche du G9.
   Seul le bas mène au laptop, et le bord gauche ne doit rien faire. Testé dans Task 1
   (`cornerPicksSideThatHitsPlacement`).
4. **Fraction exactement 1.0 (dernier pixel).** Le point d'entrée doit rester dans les bornes du
   client ou de l'écran (`w - 1` / `h - 1`), sans déborder d'un pixel. Testé dans Task 1
   (`entryClampedToLastPixel`).
5. **Placement qui vise un autre écran que le serveur, ou client inconnu.** Pas de crash. Le
   placement est ignoré avec un warning, ou c'est une erreur de config, selon la spec. Testé dans
   Task 2 (`placementUnknownComputerRejected`) ; l'avertissement runtime est dans Task 4.

---

### Task 1 : Module de géométrie `Placement`

**Files:**
- Create: `src/lib/base/Rect.h` (header-only, à ajouter à la liste de `src/lib/base/CMakeLists.txt` à côté de `DirectionTypes.h`)
- Create: `src/lib/server/Placement.h`, `src/lib/server/Placement.cpp`
- Modify: `src/lib/server/CMakeLists.txt` (ajouter `Placement.cpp` / `Placement.h` à `add_library(server ...)`)
- Test: `src/unittests/server/PlacementTests.h`, `src/unittests/server/PlacementTests.cpp`,
  `src/unittests/server/CMakeLists.txt` (nouveau `create_test(NAME PlacementTests ...)`, mêmes
  `DEPENDS`/`LIBS` que `ServerConfigTests`)

**Interfaces:**
- Consumes : `Direction` (`base/DirectionTypes.h`).
- Produces, dans `base/Rect.h` (namespace `deskflow`, utilisé aussi par Task 3) :

```cpp
struct Rect {
  int32_t x = 0, y = 0, w = 0, h = 0;
  bool contains(int32_t px, int32_t py) const { return px >= x && px < x + w && py >= y && py < y + h; }
  bool operator==(const Rect &) const = default;
};
struct Point { int32_t x = 0; int32_t y = 0; bool operator==(const Point &) const = default; };
```

- Produces, dans `server/Placement.h` (namespace `deskflow::server::placement`, avec
  `using deskflow::Rect; using deskflow::Point;`) :

```cpp
struct Placement {
  std::string client;     // nom canonique du client placé
  std::string reference;  // nom de l'écran (serveur) dont on utilise les coordonnées
  Rect area;              // rectangle occupé, en pixels du bureau de `reference`
  bool operator==(const Placement &) const = default;
};
struct ServerExit {
  std::string client;   // client visé
  Direction travel;     // sens du mouvement (Left = on sort par la gauche)
  float fraction;       // position relative [0,1) le long du bord d'entrée du client
};

// Serveur -> client. monitors = écrans du serveur ; (x,y) = curseur ; zoneSize >= 1.
std::optional<ServerExit> findServerExit(const std::vector<Rect> &monitors,
    const std::vector<Placement> &placements, int32_t x, int32_t y, int32_t zoneSize);

// Point d'arrivée sur le client, collé au bord opposé au sens `travel`.
Point clientEntryPoint(const Rect &clientShape, Direction travel, float fraction);

// Position relative [0,1) du curseur (x,y) le long du bord qu'il franchit dans le sens `travel`.
float edgeFraction(const Rect &shape, Direction travel, int32_t x, int32_t y);

// Client placé -> serveur. Renvoie le point d'arrivée sur un écran du serveur (décalé de zoneSize
// vers l'intérieur), ou nullopt si le point juste au-delà de `area` n'est sur aucun écran.
std::optional<Point> findServerEntry(const std::vector<Rect> &monitors, const Rect &area,
    Direction travel, float fraction, int32_t zoneSize);

// Parse "<client> = <reference>(<x>, <y>, <w>, <h>)". Lève std::invalid_argument (message en
// anglais) si la syntaxe est fausse, si w <= 0 ou si h <= 0. Espaces/tabulations tolérés partout.
Placement parsePlacement(const std::string &line);
```

Règles que le code doit respecter. Ce sont elles qui fixent l'algorithme :
- `findServerExit` : trouver l'écran qui contient (x,y) ; s'il n'y en a aucun, renvoyer nullopt.
  Tester les côtés dans l'ordre Left, Right, Top, Bottom. Un côté est candidat si le curseur est
  à moins de `zoneSize` px de ce bord de l'écran. Point sondé juste au-delà du bord : Left
  `(m.x-1, y)`, Right `(m.x+m.w, y)`, Top `(x, m.y-1)`, Bottom `(x, m.y+m.h)`. Si ce point est sur
  un écran, le côté est ignoré. Sinon, le premier placement dont `area.contains(point)` gagne.
  `fraction` = `(y - area.y) / area.h` pour Left/Right, `(x - area.x) / area.w` pour Top/Bottom,
  bornée à `[0, 1)`.
- `clientEntryPoint` : travel Left → `x = s.x + s.w - 1` ; Right → `x = s.x` ; Top →
  `y = s.y + s.h - 1` ; Bottom → `y = s.y`. L'autre coordonnée vaut
  `s.y + floor(fraction * s.h)` (ou en x), bornée à `s.y + s.h - 1`.
- `edgeFraction` : `(y - s.y) / s.h` pour Left/Right, `(x - s.x) / s.w` pour Top/Bottom, bornée à
  `[0, 1)`.
- `findServerEntry` : point sondé, travel Right `(area.x+area.w, area.y + floor(f*area.h))`,
  Left `(area.x-1, …)`, Top `(area.x + floor(f*area.w), area.y-1)`, Bottom `(…, area.y+area.h)`.
  S'il est sur un écran `m`, on le décale de `zoneSize` vers l'intérieur de `m` dans le sens du
  mouvement, puis on le borne à `m`.

- [ ] **Step 1 : écrire les tests qui échouent** (`PlacementTests`, QtTest, slots privés). Valeurs
  du setup réel : `G9 {-1292,-1440,5120,1440}`, `W21 {0,0,2560,1080}`,
  `P {"LAPTOPDUFLEX","Corsair",{-1292,0,1292,1080}}`, `zone = 1`, `laptop {0,0,1920,1080}`.
  - `exitLeftFromUltrawide` : `findServerExit({G9,W21},{P},0,540,1)` → client `LAPTOPDUFLEX`,
    `travel == Left`, `fraction ≈ 0.5` (`qFuzzyCompare` à 1e-3 près).
  - `exitDownFromG9Overhang` : `(-646,-1)` → `travel == Bottom`, `fraction ≈ 0.5`.
  - `noExitWhereAnotherMonitorIsAdjacent` : `(1000,-1)` → nullopt.
  - `noExitIntoEmptyRightGap` : `(3000,-1)` → nullopt.
  - `noExitFromG9LeftEdge` : `(-1292,-700)` → nullopt.
  - `cornerPicksSideThatHitsPlacement` : `(-1292,-1)` → `travel == Bottom`, `fraction ≈ 0.0`.
  - `noExitAwayFromEdges` : `(1280,540)` → nullopt.
  - `noPlacementsMeansNoExit` : `findServerExit({G9,W21},{},0,540,1)` → nullopt.
  - `clientEntryFromLeft` : `clientEntryPoint(laptop, Left, 0.5f)` → `Point{1919,540}`.
  - `clientEntryFromBottom` : `clientEntryPoint(laptop, Bottom, 0.25f)` → `Point{480,0}`.
  - `entryClampedToLastPixel` : `clientEntryPoint(laptop, Left, 0.99999f)` → `y == 1079` ;
    `findServerEntry({G9,W21},P.area,Right,0.99999f,1)` → `y == 1079`.
  - `serverEntryRightToUltrawide` : `findServerEntry({G9,W21},P.area,Right,0.5f,1)` →
    `Point{1,540}`.
  - `serverEntryIsInsetByZone` : même appel avec `zone = 5` → `Point{5,540}`.
  - `serverEntryUpToG9` : `findServerEntry({G9,W21},P.area,Top,0.5f,1)` → `Point{-646,-2}`.
  - `serverEntryLeftGoesNowhere` : `findServerEntry({G9,W21},P.area,Left,0.5f,1)` → nullopt ;
    idem pour `Bottom`.
  - `edgeFractionHalf` : `edgeFraction(laptop, Right, 1920, 540)` ≈ 0.5.
  - `parseValid` : `parsePlacement("\tLAPTOPDUFLEX = Corsair(-1292, 0, 1292, 1080)")` → `P`.
  - `parseRejects` : `QVERIFY_THROWS_EXCEPTION(std::invalid_argument, …)` pour
    `"A = B(1,2,3)"`, `"A = B(0,0,0,10)"`, `"A = B(0,0,10,-1)"`, `"A B(0,0,1,1)"`,
    `"A = B(x,0,1,1)"`, `"= B(0,0,1,1)"`.

- [ ] **Step 2 : vérifier que ça échoue.** Il n'y a pas de toolchain locale : l'échec attendu,
  c'est que la CI ne compile pas tant que `Placement.h` n'existe pas. Fusionner les étapes 1 à 4
  en un seul commit si la CI est le seul moyen d'exécuter les tests (voir Task 5).

- [ ] **Step 3 : implémenter `Placement.h/.cpp`** selon les signatures et les règles ci-dessus.
  Utiliser `std::floor` sur des `double` pour les conversions, et `std::clamp` pour les bornes.

- [ ] **Step 4 : les tests passent** (en CI : job `windows-2022-x64`, étape `Tests`,
  `PlacementTests` PASS).

- [ ] **Step 5 : commit** `feat(server): add per-monitor placement geometry`.

### Task 2 : Section `placements` dans `Config`

**Files:**
- Modify: `src/lib/server/Config.h`, `src/lib/server/Config.cpp` (`readSection` l. 491-533,
  `operator==` l. 395, `operator<<` l. 1255)
- Test: `src/unittests/server/ServerConfigTests.h/.cpp`

**Interfaces:**
- Consumes : `placement::Placement`, `placement::parsePlacement` (Task 1).
- Produces (dans `Config`) :
  - `bool addPlacement(const placement::Placement &p);` : renvoie false si `p.client` ou
    `p.reference` n'est pas un écran connu (`isComputer`), ou si `p.client` a déjà un placement.
    Les noms sont convertis en noms canoniques (`getCanonicalName`) avant stockage.
  - `const std::vector<placement::Placement> &getPlacements() const;`
  - `private: void readSectionPlacements(ConfigReadContext &);` +
    `std::vector<placement::Placement> m_placements;`

- [ ] **Step 1 : tests qui échouent.**
  - `placementEquality` : deux `Config` avec `computerA`/`computerB` ; `a.addPlacement({"computerB","computerA",{-10,0,10,10}})` → `QVERIFY(a != b)` ; même ajout sur `b` → `QVERIFY(a == b)`.
  - `placementUnknownComputerRejected` : `QVERIFY(!a.addPlacement({"nope","computerA",{0,0,1,1}}))` et `QVERIFY(!a.addPlacement({"computerB","nope",{0,0,1,1}}))`.
  - `placementDuplicateRejected` : le deuxième `addPlacement` pour `computerB` → false.
  - `placementSerialized` : `std::ostringstream os; os << a;` → contient `"section: placements"` et `"computerB = computerA(-10,0,10,10)"`.
- [ ] **Step 2 : ils échouent** (compilation : `addPlacement` n'existe pas).
- [ ] **Step 3 : implémenter.**
  - Dans `readSection`, brancher `"placements"` sur `readSectionPlacements`.
  - `readSectionPlacements` lit jusqu'à `end`. Chaque ligne passe par `parsePlacement`
    (`std::invalid_argument` → `ServerConfigReadException(s, e.what())`), puis par
    `addPlacement`. Un refus donne `ServerConfigReadException(s, "unknown computer name or
    duplicate placement \"%{1}\"", line)`. Une fin de flux sans `end` donne `"unexpected end of
    placements section"`.
  - `operator==` compare aussi `m_placements`.
  - `operator<<` écrit la section après `links`, uniquement si elle n'est pas vide, au format
    `\t<client> = <reference>(<x>,<y>,<w>,<h>)`.
- [ ] **Step 4 : les tests passent** (`ServerConfigTests` PASS, anciens tests compris).
- [ ] **Step 5 : commit** `feat(server): parse placements config section`.

### Task 3 : Liste des écrans du serveur (Windows)

**Files:**
- Modify: `src/lib/deskflow/IPrimaryComputer.h` (à côté de `getJumpZoneSize`, l. 158)
- Modify: `src/lib/deskflow/Computer.h/.cpp`, `src/lib/server/PrimaryClient.h/.cpp` (transmission
  de l'appel, sur le modèle de `getJumpZoneSize`)
- Modify: `src/lib/platform/MSWindowsComputer.h/.cpp` (`updateComputerShape`, l. 1431)

**Interfaces:**
- Produces :
  - `IPrimaryComputer` : `virtual std::vector<deskflow::Rect> getMonitorRects() const { return {}; }`
    (inclut `base/Rect.h` de Task 1). Ce n'est **pas** une méthode virtuelle pure :
    macOS/Linux/Wayland n'ont rien à faire.
  - `Computer::getMonitorRects() const` : renvoie `{}` si `!m_isPrimary`, sinon
    `m_computer->getMonitorRects()`.
  - `PrimaryClient::getMonitorRects() const` : renvoie `m_computer->getMonitorRects()`.
  - `MSWindowsComputer::getMonitorRects() const override` : renvoie `m_monitorRects`, rempli dans
    `updateComputerShape()` via `EnumDisplayMonitors(nullptr, nullptr, cb, …)` avec
    `MONITORINFO::rcMonitor`. Ajouter un `LOG_DEBUG("monitor %d: %d,%d %dx%d", …)` par écran.

- [ ] **Step 1 : implémenter** les quatre points ci-dessus. Il n'y a pas de test unitaire,
  puisque c'est de l'API Win32. La vérification se fait en Task 6 : le log en niveau `Debug`
  doit afficher `monitor 0: -1292,-1440 5120x1440` et `monitor 1: 0,0 2560x1080`, dans un ordre
  quelconque.
- [ ] **Step 2 : la CI compile** sur toutes les plateformes, si elles sont activées, ou au moins
  sur Windows.
- [ ] **Step 3 : commit** `feat(platform): expose monitor rectangles on Windows primary`.

### Task 4 : Câblage dans `Server`

**Files:**
- Modify: `src/lib/server/Server.h/.cpp` : `onMouseMovePrimary` (l. 1611), `onMouseMoveSecondary`
  (l. 1708), `setConfig` (l. 205)

**Interfaces:**
- Consumes : Task 1 (fonctions), Task 2 (`getPlacements`), Task 3 (`PrimaryClient::getMonitorRects`).
- Produces : `private: const placement::Placement *findPlacementFor(const std::string &client) const;`.
  Elle ne renvoie que les placements dont `reference == getName(m_primaryClient)`.

- [ ] **Step 1 : `onMouseMovePrimary`.** Juste après le calcul de `xc/yc` (l. 1653) et **avant**
  le test `dirh/dirv` : si `m_config->getPlacements()` n'est pas vide, appeler
  `findServerExit(m_primaryClient->getMonitorRects(), placementsDuServeur, x, y, zoneSize)`.
  En cas de résultat, chercher `hit.client` dans `m_clients` (`std::map<std::string,
  BaseClientProxy *>`, indexé par nom). S'il est présent (donc connecté), récupérer sa forme avec
  `dst->getShape(dx, dy, dw, dh)` et calculer `p = clientEntryPoint(Rect{dx, dy, dw, dh},
  hit.travel, hit.fraction)`, puis `if (isSwitchOkay(dst, hit.travel, p.x, p.y, xc, yc)) {
  switchComputer(dst, p.x, p.y, false); return true; }`. Dans tous les autres cas, continuer sur le
  code existant sans rien changer.
- [ ] **Step 2 : `onMouseMoveSecondary`.** Dans le `do { … }`, une fois `dir` déterminé et avant
  `mapToNeighbor` (l. 1838) : si `findPlacementFor(getName(m_active))` renvoie un placement,
  calculer `f = edgeFraction(Rect{ax, ay, aw, ah}, dir, xc, yc)` puis
  `e = findServerEntry(m_primaryClient->getMonitorRects(), pl->area, dir, f,
  m_primaryClient->getJumpZoneSize())`. Si `e` existe et que
  `isSwitchOkay(m_primaryClient, dir, e->x, e->y, xc, yc)`, alors `newComputer = m_primaryClient;
  m_x = e->x; m_y = e->y;` et sortir du `do` avec `jump = true`. Sinon, garder le chemin
  existant.
- [ ] **Step 3 : avertissements dans `setConfig`.** Pour chaque placement dont la `reference`
  n'est pas le serveur : `LOG_WARN("placement for \"%s\" ignored: \"%s\" is not this server", …)`.
  Pour chaque placement dont `area` chevauche un rectangle de `getMonitorRects()` :
  `LOG_WARN("placement for \"%s\" overlaps a monitor", …)`.
- [ ] **Step 4 : la CI compile et tous les tests passent.**
- [ ] **Step 5 : commit** `feat(server): switch through per-monitor edges using placements`.

### Task 5 : Fork, CI réduite, installeur

**Prérequis utilisateur :** le fork `LeVaradero/deskflow` existe. Il doit le créer lui-même,
l'action a été refusée par le filtre d'autorisations.

**Files:**
- Modify: `.github/workflows/continuous-integration.yml` (**sur le fork uniquement**)

- [ ] **Step 1 :** limiter la matrice du job de build à `windows-2022-x64`, retirer
  `needs: lint-clang` du job de build et désactiver les jobs `unix`/`flatpak`/doc. Commit
  `ci(fork): build windows x64 only`.
- [ ] **Step 2 :** activer Actions sur le fork
  (`gh api -X PUT repos/LeVaradero/deskflow/actions/permissions -F enabled=true -f allowed_actions=all`).
- [ ] **Step 3 :** configurer l'identité git du clone avec l'adresse noreply GitHub de
  l'utilisateur (pas son e-mail perso), ajouter le remote `fork`, puis
  `git push fork HEAD:refs/heads/master --force`. Le master du fork est remplacé par v1.27.0 +
  nos commits : c'est voulu, puisque la CI ne tourne que sur `master`.
- [ ] **Step 4 :** suivre le run (`gh run watch`). Attendu : job `windows-2022-x64` vert, étape
  `Tests` avec `PlacementTests` et `ServerConfigTests` PASS. En cas d'échec, lire le log
  (`gh run view --log-failed`), corriger, puis re-pousser.
- [ ] **Step 5 :** télécharger l'artefact `package-deskflow-windows-2022-x64`
  (`gh run download`) dans le scratchpad. Vérifier qu'il contient un `.msi` ou un `.exe`.

### Task 6 : Installation et test sur Corsair (avec l'utilisateur)

- [ ] **Step 1 :** sauvegarder `C:\ProgramData\Deskflow\deskflow-server.conf` et
  `%APPDATA%\Deskflow\Deskflow.conf` (copies `.bak` à côté).
- [ ] **Step 2 :** l'utilisateur installe le paquet sur Corsair uniquement.
- [ ] **Step 3 :** écrire dans `deskflow-server.conf` (admin) :

```
section: placements
	LAPTOPDUFLEX = Corsair(-1292, 0, 1292, 1080)
end

section: options
end
```

  Dans `Deskflow.conf`, mettre `externalConfig=true` et `enableUpdateCheck=false` (Deskflow fermé),
  et passer temporairement `[log] level=Debug`.
- [ ] **Step 4 :** démarrer Deskflow. Le log doit montrer les deux lignes `monitor …` (Task 3), et
  aucun warning de placement.
- [ ] **Step 5 :** l'utilisateur teste chaque ligne du tableau « Résultat attendu » de la spec.
  Il note les écarts, qu'on corrige en repassant par Task 4/5.
- [ ] **Step 6 :** remettre `[log] level=Info`. Laisser à l'utilisateur un mémo de retour arrière :
  réinstaller la 1.27.0 officielle et restaurer les `.bak`.
