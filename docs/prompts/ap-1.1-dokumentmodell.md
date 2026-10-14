# Prompt: AP-1.1 Dokumentmodell und Feature-Graph

**Verwendung:** In einer neuen Sitzung im Fission3D-Repository einfügen. Der Prompt enthält den Stand, die
bereits getroffenen Entscheidungen und einen Planvorschlag. Entscheidungen, die du anders treffen willst, vor dem
Absenden im Text ändern.

---

## Prompt

```markdown
# Auftrag: AP-1.1 Dokumentmodell und Feature-Graph (Beginn von Phase 1)

Halte dich an CLAUDE.md (Master-Prompt). Lies zuerst `docs/arbeitspakete/phase-1-erster-koerper.md` (AP-1.1 bis
AP-1.3 und die Meilenstein-Beschreibung), `docs/03-architektur.md` (§2 Schichten, §3 Datenmodell, §4 GAL, §6
Präzision), `docs/adr/0003-persistente-referenzen.md` samt `docs/spikes/persistente-referenzen.md`, die
Abschlussvermerke von AP-0.5, AP-0.6 und AP-0.8 in `docs/arbeitspakete/phase-0-fundament.md` und
`docs/adr/0007-oberflaeche-qt-quick.md`.

## Stand (14.10.2026)
- **Phase 0 ist fachlich durch:** AP-0.1 bis 0.8 sind gemergt, AP-0.8 steht auf **Review**, bis der Product Owner die
  Mac-Version ausprobiert hat. `main` ist auf Stand PR #12.
- **Vorhanden, darauf baust du auf:**
  - `src/geometry/gal`: `Solid` (undurchsichtiger Körper), `TriangleMesh` (Flächen-ID je Dreieck, Kanten als Linienzüge),
    `SolidReport`, `GeometryError`.
  - `src/geometry/occt`: `makeBox`, `makeCylinder`, `measure`, `tessellate` (jedes Ergebnis mit `BRepCheck_Analyzer`
    geprüft).
  - `src/core`: **Ausdrucksauswertung** `fission::core::evaluateExpression` (`+ − * /`, Klammern, mm/cm/in, benannte
    Maße, Komma und Punkt). Sie ist der Ausgangspunkt für den Ausdrucksparser von AP-1.1, es fehlen Funktionen
    (`min`, `max`, `round`, `sin`, `cos`, `sqrt`), Fehlerpositionen, Abhängigkeiten zwischen benannten Maßen und
    Zykluserkennung.
  - `src/render` und `src/ui`: Renderer, 3D-Ansicht, Fenster nach Mockup, Theme-Tokens, Zahlenfeld. Die Oberfläche zeigt
    bisher einen festen Demo-Würfel (`makeDemoScene` in `apps/fission3d/src/main.cpp`).
  - Naming-Prototyp aus AP-0.6 liegt in `tests/naming/` (Szenarien, Zufallstests, OCAF-Vergleich); ADR-0003 ist
    angenommen (nur strenger Modus, eigene Namensschicht über der Kernel-Historie).
- **`main` ist geschützt:** nur per Pull Request, alle CI-Prüfungen grün, keine Force-Pushes, Merge-Commit. Die CI
  braucht bei warmem Zwischenspeicher rund 15 Minuten. Jede Änderung an `vcpkg.json` füllt den Speicher neu (ca. 75 min
  je System): solche Änderungen als eigenen kleinen PR zuerst.
- **Parallele Sitzungen:** Arbeite in einem eigenen Worktree (`git worktree add ../Fission3D-ap11 -b ap-1.1-dokumentmodell
  origin/main`), nie im Haupt-Arbeitsverzeichnis. Fremde Worktrees (`Fission3D-docs-mcp`, `Fission3D-prompt08`) nicht
  anfassen. Bibliotheken teilen: `VCPKG_ROOT=/Users/henryroth/Fission3D/.vcpkg`.
- **Werkzeuge:** Auf dem Mac ist Homebrew-Qt 6.12 inklusive `qttools` installiert (`scripts/setup --check` zeigt es).
  `scripts/test` baut und testet alles in rund 2 Minuten. Formatierung mit **clang-format 21.1.8** wie die CI (die lokale
  Version 23 formatiert anders: per `pip install clang-format==21.1.8` in einem venv, dann
  `CLANG_FORMAT=<pfad> python3 scripts/tools/check_format.py --fix`).
- **Testrechner:** `maintux` (Ubuntu 26.04, GCC 15, 32 Kerne, SSH-Alias, freie Hand). Mit
  `FISSION_QT_SOURCE=vcpkg cmake --preset linux-release -B build/vcpkg-qt` baust du dort mit demselben Qt wie die CI;
  Fenster-Tests brauchen `xvfb-run`. `secondtux` ist aus, `vierstein` nur für Containertests.
- Mein Projektgedächtnis (`project-state`, `linux-test-machine`) hat den Rest.

## Entscheidungen des Product Owners (gelten)
- **Design-System gilt** (08-gestaltung.md); Mindest-Schriftgröße 12 px bei 100 % (R21). Texte nur aus dem Glossar,
  immer Deutsch **und** Englisch (`qsTr`, englische Quelltexte, `.ts`-Dateien; `ctest -R translations` prüft das).
- **Die Dokument-Tab-Leiste bleibt**; mehrere offene Dokumente sind dem Product Owner wichtig, plane das
  Dokumentmodell so, dass es mehrere gleichzeitig geben kann (kein globaler Zustand).
- Übernahme in `main` per Pull Request mit grüner CI; mergen darfst du, wenn alle Prüfungen grün sind und der
  Product Owner zugestimmt hat.

## Annahmen (gelten, solange nicht widersprochen)
- **Zerlegung:** AP-1.1 ist L. Schlage in deinem Plan eine Aufteilung in Teilpakete vor (Vorschlag: a) Wertetypen und
  Ausdrucksparser, b) Dokument, Feature, Körper, Parameter mit Feature-Typ-Registry, c) Recompute-Engine mit
  Dirty-Propagation, Cache, Worker und Abbruch, d) Anbindung an die Oberfläche: der Demo-Würfel wird ein echtes
  `box`-Feature im Dokument). Trage sie in die Phasendatei ein, bevor du anfängst.
- **Erste Feature-Typen:** nur `box` und `cylinder` (liefert `makeBox`/`makeCylinder` schon), damit der Weg
  Dokument → Neuberechnung → Körper → Anzeige durchgängig belegt ist. Weitere Typen gehören zu AP-1.5.
- **Persistente Referenzen:** die Schnittstelle (`PersistentRef`, Namens-Vertrag je Feature) gehört in dieses AP, die
  vollständige Umsetzung der Namensschicht in AP-2.10. Halte `core` frei von Qt und von Kernel-Typen.
- **Nicht in diesem AP:** Commands und Undo (AP-1.2), Auswahl (AP-1.4), Speichern (AP-1.10), Rezept-Panel (AP-1.9).
- **Überschneidungen melden:** AP-1.3 (Viewport) und AP-1.11 (App-Shell) sind durch AP-0.8 teilweise erledigt
  (Kamera, Ansichtswürfel, Bett, Raster, Fenster, Sprachen). Trage in der Phasendatei ein, was davon schon steht und was
  offen bleibt.

## Erfolgsmaß
Akzeptanzkriterien von AP-1.1: Core ohne Qt kompilierbar, ≥ 85 % Testabdeckung; eine Parameteränderung rechnet nur
betroffene Features neu (Test); eine laufende Neuberechnung bricht in ≤ 50 ms ab; `"25,5 + 1 in"` ergibt `50,9 mm`,
`"a = b, b = a"` liefert einen verständlichen Zyklusfehler. Dazu sichtbar: Ein im Dokument angelegtes `box`-Feature
erscheint in der 3D-Ansicht und im Rezept-Platzhalter, ein geänderter Parameter bewegt den Körper.

## Arbeitsweise
- Fasse in fünf Zeilen dein Verständnis zusammen, liste offene Fragen, schreibe den Plan mit der Zerlegung und
  **warte auf mein OK**, bevor du mit der Umsetzung beginnst.
- Kleine, lauffähige Schritte, nach jedem Schritt bauen und testen; Commits `AP-1.1: …`.
- Wenn dir etwas Wesentliches auffällt (Widerspruch zwischen Architektur und UX-Konzept, etwas Unumsetzbares,
  ein Designfehler, der Einsteigern schadet): **setze nichts still anders um**, melde es mit höchstens drei Optionen,
  Empfehlung und Folgen.
- Status am Ende ehrlich setzen („Review“, nicht „erledigt“), Abschlussvermerk mit „So probierst du es aus“. Was nicht
  belegt ist, benennst du als offen.
- Rückmeldung kurz und in Alltagssprache; Fachbegriffe über ihre Wirkung erklären.
```
