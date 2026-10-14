# Prompt: AP-0.8 App-Gerüst mit Würfel

**Verwendung:** In einer neuen Sitzung im Fission3D-Repository einfügen. Der Prompt enthält den Stand, die
bereits getroffenen Entscheidungen und den freigegebenen Plan. Entscheidungen, die du anders treffen willst,
vor dem Absenden im Text ändern.

---

## Prompt

```markdown
# Auftrag: AP-0.8 App-Gerüst mit Würfel (Meilenstein-Demo von Phase 0)

Halte dich an CLAUDE.md (Master-Prompt). Lies zuerst `docs/arbeitspakete/phase-0-fundament.md` (AP-0.8 und die
Abschlussvermerke von AP-0.4 und AP-0.7), `docs/08-gestaltung.md` (**verbindlich**), `docs/mockups/oberflaeche.html`,
`docs/02-ux-konzept.md` (§1, §2, §12 Glossar, §14), `docs/adr/0002-viewport-technologie.md` und
`docs/adr/0006-paketierung.md`.

## Stand (14.10.2026)
- AP-0.1 bis 0.3, 0.5 und 0.6 stehen auf **Review** bzw. sind gemergt; AP-0.4 (Viewport = Qt RHI, ADR-0002) und
  AP-0.7 (Pakete: `.dmg`, AppImage, Flatpak, CI-Job „Package“, ADR-0006) sind **erledigt**. `main` ist auf dem Stand
  von PR #7 (Eigen-Warnung unter GCC 14/15 behoben).
- **`main` ist geschützt:** nur per Pull Request, alle CI-Prüfungen grün, keine Force-Pushes. Merge-Commit, kein
  Squash. GitHub „Keep my email addresses private“ ist an.
- **Parallele Sitzungen:** Arbeite in einem eigenen Worktree (`git worktree add ../Fission3D-ap08 -b
  ap-0.8-app-geruest origin/main`), nie im Haupt-Arbeitsverzeichnis. Es existiert bereits ein fremder Worktree
  `Fission3D-docs-mcp`; nicht anfassen. Build-Zwischenspeicher teilen: `VCPKG_ROOT=/Users/henryroth/Fission3D/.vcpkg`.
- **Testrechner:** `maintux` (Ubuntu 26.04, GCC 15, 32 Kerne, SSH-Alias, freie Hand) ist der Linux-Rechner;
  `secondtux` ist zurzeit aus. `vierstein` (Debian 13, schwach) nur für Containertests, dort kein `rsync`.
  Auf dem Mac gibt es eine **saubere macOS-14-VM** (`tart run fission3d-mac14`, siehe `docs/testumgebung.md`) für
  Installationstests. Pakete von 26.04 laufen nicht auf 24.04; verteilbare Pakete kommen nur aus der CI.
  `scripts/package` baut lokal das `.dmg` bzw. AppImage (nur Entwicklungsnachweis).
- Mein Projektgedächtnis (`project-state`, `linux-test-machine`) hat den Rest.

## Entscheidungen des Product Owners
- **Übersetzungen: Qt-Linguist (`qsTr` + `.ts`-Dateien), Deutsch und Englisch.** Dafür kommt `qttools` in die
  Bibliotheken (vcpkg-Feature `qt`, Homebrew `qttools`, apt `qt6-tools-dev`/`qt6-l10n-tools` in `scripts/setup`).
  **Das ändert den Zwischenspeicher-Schlüssel:** die CI baut danach einmal alle Bibliotheken neu (rund 75 Minuten je
  System; Pull Requests speichern den Zwischenspeicher nicht, `main` tut es). Mache diese Änderung deshalb als
  **eigenen kleinen PR zuerst** (Schritt 0), lass ihn mergen, damit `main` den Speicher füllt, und baue erst danach
  die eigentliche Oberfläche.
- **Menüs: echte Menüleiste auf dem Mac.** Auf macOS stehen die Menüs (Datei, Bearbeiten, Ansicht, Körper,
  Fenster, Hilfe, dazu App-Menü mit Über, Einstellungen ⌘, und Beenden) in der **System-Menüleiste**; im Fenster
  bleiben Logo, Befehlssuche, Zahnrad und Druckerprofil. Unter Linux sind die Menüs im Fenster wie im Mockup.
- **Umsetzung 1:1 nach dem Mockup. Keine Kompromisse** bei Anordnung, Farben, Schrift, Maßen, Abständen,
  Radien, Zuständen und Beschriftungen laut `docs/08-gestaltung.md`; einzige Ausnahme sind die Menüs auf dem
  Mac (siehe oben). Die Texte im Mockup sind Beispiele: UI-Texte nur mit Begriffen aus dem Glossar (UX §12),
  immer Deutsch **und** Englisch.
- **Wenn dir etwas Wesentliches auffällt** (ein Designfehler, der Einsteigern schadet, ein Widerspruch zwischen
  Mockup, 08-gestaltung und UX-Konzept, etwas Unumsetzbares): **setze nichts still anders um.** Melde es mit
  höchstens drei Optionen, Empfehlung und Folgen. Kleinigkeiten setzt du wie im Mockup um.

## Annahmen (gelten, solange nicht widersprochen)
- Farben und Maße aus den Tokens in 08-gestaltung §2/§4, **dunkel und hell** definiert, dunkel als Standard; ein
  Umschalter folgt mit den Einstellungen. Akzentfarbe zunächst fest (Blau), aber so gebaut, dass sie ableitbar ist.
- Schrift: IBM Plex Sans und Mono werden mitgeliefert (SIL OFL 1.1) und in `THIRD_PARTY_NOTICES.md` eingetragen;
  Ersatzschrift laut 08-gestaltung §3.
- Navigation laut UX §2: Maus rechte Taste dreht, mittlere verschiebt, Rad zoomt zum Zeiger; Trackpad zwei
  Finger drehen, Shift + zwei Finger verschieben, Pinch zoomt.
- Der Würfel entsteht über den Kern (OpenCASCADE), nicht im Renderer: kleine Mesh-Schnittstelle in der
  Geometrie-Abstraktion, Tesselierung in `src/geometry/occt`. Kernel-Typen verlassen `src/geometry/<impl>/` nicht.
- Startzeit = Prozessstart bis erstes gezeichnetes Bild, im Log ausgegeben, gemessen auf dem Mac und auf maintux.
- Nicht in diesem AP: Auswahl, Griffe, Werkzeuge, Rezept-Inhalt, SSAO. Die Leisten sind Platzhalter mit echtem
  Aussehen und Platzhaltertexten aus dem Glossar.

## Plan (vorgestellt, freigegeben)
0. **Eigener PR:** `qttools` einbinden (`vcpkg.json`, `scripts/setup`, CI-Voraussetzungen), mergen, Zwischenspeicher
   füllen lassen.
1. **Würfel als Netz:** Mesh-Schnittstelle, OCCT-Würfel tesselliert; Golden-Test (12 Dreiecke, Volumen, Hülle,
   BRepCheck gültig).
2. **Renderer** in `src/render` aus `spikes/viewport/a_rhi`: Druckbett (5/10 mm Raster, Achsen rot/grün,
   Ursprungspunkt), Beleuchtung, Kantenlinien, Kamera mit Maus und Trackpad (`QQuickRhiItem`). Schichtregeln beachten
   (`cmake/FissionLayers.cmake`); Layer-Tests bleiben grün.
3. **Fenster-Gerüst in QML:** Theme-Tokens, Anordnung nach 08-gestaltung §1 (Menüleiste 40 px, Dokument-Tabs 52 px,
   linke Leiste 248 px mit Körper/Bibliothek und Rezept, rechte Leiste 300 px mit Objekt/Drucker, Statuszeile
   28 px mit Hinweiszeile), schwebende 3D-Elemente (Werkzeugpalette, Darstellungsleiste, Ansichtswürfel,
   Navigationsknöpfe, Fokus-Modus ⇧F), Zahlenfeld-Komponente als Grundbaustein. Auf dem Mac native Menüleiste.
4. **Logging** (spdlog, Datei im Nutzerverzeichnis mit Rotation), **Startzeit-Messung**, **Über-Fenster** mit
   Version, Qt, OpenCASCADE, Grafik-Backend.
5. **Tests und Nachweise:** Screenshot des Fensters (Würfel sichtbar), Startzeit-Test, Sprachtest (keine
   hartkodierten Strings, DE und EN vollständig), Screenshot-Vergleich je Hauptansicht (dunkel und hell) gegen das
   Mockup nach 08-gestaltung §8, Kontrasttest der Token-Paare.
6. **Pakete prüfen:** `.dmg` und AppImage aus der CI starten das neue Fenster (Mac-VM und Linux-Container).

**Erfolgsmaß:** Meilenstein-Demo von Phase 0 (installierbares `.dmg` und AppImage zeigen ein Fenster mit drehbarem,
beleuchtetem Würfel auf dem Druckbett), Startzeit unter 3 s, Abweichungen zum Mockup nur die Mac-Menüs und
gemeldete Punkte.

## Arbeitsweise
- Fasse in fünf Zeilen dein Verständnis zusammen und **starte dann mit Schritt 0**; der Plan ist freigegeben.
  Halte an und frage, wenn sich etwas ergibt, das den Plan, die Architektur oder das Design berührt.
- Kleine, lauffähige Schritte, nach jedem Schritt bauen und testen; Commits `AP-0.8: …`. Übernahme in `main` per
  Pull Request mit grüner CI; mergen darfst du, wenn alle Prüfungen grün sind und der Product Owner zugestimmt hat.
- Status am Ende ehrlich setzen („Review“, nicht „erledigt“), Abschlussvermerk mit „So probierst du es aus“,
  Screenshots als Nachweis. Was nicht belegt ist, benennst du als offen.
- Rückmeldung kurz und in Alltagssprache; Fachbegriffe über ihre Wirkung erklären.
```
