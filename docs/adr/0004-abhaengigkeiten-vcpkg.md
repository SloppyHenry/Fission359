# ADR-0004: Abhängigkeiten – vcpkg, Qt-Bezug und PlaneGCS

- **Status:** angenommen
- **Datum:** 2026-10-12
- **Arbeitspaket:** AP-0.2

## Kontext
Alle Kernbibliotheken (OpenCASCADE, Manifold, Eigen, lib3mf, …) sollen auf macOS (arm64) und Linux
(x86_64) reproduzierbar verfügbar sein, ohne manuelle Schritte außer der Toolchain. Rahmenbedingungen:

- Ziel-Plattformen laut AP-0.2: macOS und **Ubuntu 24.04**. Dort liefert apt nur Qt 6.4; die
  Architektur verlangt Qt ≥ 6.8.
- Einsteiger sollen mit `scripts/setup` und `scripts/run` starten können. Ein erster Lauf von
  mehreren Stunden wäre eine Hürde.
- PlaneGCS ist nicht in vcpkg und im FreeCAD-Quelltext an FreeCAD-Interna gekoppelt.
- Nur GPL-3-kompatible Lizenzen (Architektur §8).

## Optionen

### Qt-Bezug
1. **Immer Qt aus vcpkg:** überall dieselbe Version · jeder baut Qt beim ersten Mal selbst (lang).
2. **Hybrid:** Qt vom System, wenn ≥ 6.8 vorhanden, sonst aus vcpkg; CI immer vcpkg-Qt ·
   kurzer erster Lauf auf Mac und aktuellen Linux-Distributionen · Qt kann lokal und in der CI
   leicht abweichen.
3. **Qt immer vom System:** scheitert auf Ubuntu 24.04.

### PlaneGCS
1. **Aus FreeCAD-Release-Tag übernehmen** (`third_party/planegcs`), Quelle unverändert, kleine
   Ersatzdateien für FreeCAD-Interna, Boost.Graph aus vcpkg.
2. Wie 1, aber Boost.Graph herauspatchen · eine Abhängigkeit weniger, Updates mühsamer.
3. Veralteten Standalone-Fork nutzen · verpasst Jahre an Solver-Korrekturen.

## Entscheidung
- **Qt: Option 2 (Hybrid).** `FISSION_QT_SOURCE=auto` (Standard) nimmt ein System-Qt ≥ 6.8, sonst
  baut vcpkg Qt (Manifest-Feature `qt`). `vcpkg` erzwingt vcpkg-Qt (für die CI und für
  reproduzierbare Fehlersuche), `system` verlangt System-Qt.
- **PlaneGCS: Option 1.** Quelle aus FreeCAD 1.1.4 (Commit `4fd3bf32…`), byte-identisch, dazu
  `shim/` und `SketcherGlobal.h` als Ersatz. Herkunft und Update-Anleitung in
  `third_party/planegcs/README.md`.
- **OpenCASCADE 8.0.1** aus der vcpkg-Baseline (Architektur verlangt ≥ 7.9). Rückfall auf 7.9.x per
  `overrides` in `vcpkg.json`, falls sich eine Inkompatibilität zeigt.
- **vcpkg** im Manifest-Modus, Baseline in `vcpkg.json` (`builtin-baseline`). `scripts/setup`
  klont vcpkg nach `.vcpkg/` (nicht im Repo, kein Submodul) und setzt es auf genau diesen Stand.
- **Eigene Triplets** (`cmake/vcpkg-triplets/`): dynamische Bibliotheken, nur Release-Builds.
  Debug-Builds von Fission359 linken die Release-Bibliotheken; das halbiert Bauzeit und Plattenbedarf.
- **Binär-Cache:** vcpkg-Standard (`~/.cache/vcpkg/archives`, lokal). Die CI-Anbindung folgt in AP-0.3.
- **Schichten:** Welche Schicht welche Bibliothek linken darf, steht in `cmake/FissionLayers.cmake`
  (z. B. OpenCASCADE `TK*` nur `fission_geometry_occt`). Der Build prüft das beim Konfigurieren.
- **Lizenzen:** `scripts/tools/licenses.py` löst die Abhängigkeiten aus `vcpkg.json` und den Ports
  ohne Bauen auf (beide Plattformen), prüft jede Lizenz gegen die Erlaubtliste und erzeugt das
  Inventar in `THIRD_PARTY_NOTICES.md`. Ports ohne Lizenzfeld stehen mit Begründung in
  `third_party/inventory.json`.

## Konsequenzen
- **Einfacher:** Ein frischer Klon baut mit `scripts/setup` und `scripts/run`; neue Bibliotheken sind
  eine Zeile in `vcpkg.json` plus ein Lizenz-Check.
- **Schwerer:**
  - Der erste Lauf baut alle Bibliotheken (ohne Qt rund 15–20 Minuten auf einem M-Mac, mit vcpkg-Qt
    deutlich länger; auf kleinen Rechnern Stunden).
  - Im Hybrid-Betrieb auf Linux können gleichnamige Bibliotheken doppelt vorkommen (System-Qt nutzt
    z. B. die System-`libfreetype`, OpenCASCADE die aus vcpkg). Der Lader nimmt dann die zuerst
    geladene. Das ist ein Absturzrisiko, kein Rechenfehler-Risiko.
- **Folge-Arbeiten:** CI mit `FISSION_QT_SOURCE=vcpkg` und Binär-Cache (AP-0.3); OCCT-Module nach der
  Viewport-Entscheidung verkleinern (AP-0.4); Paketierung bündelt die dynamischen Bibliotheken
  (AP-0.7).
- **Revisionskriterium:** Tritt im Hybrid-Betrieb ein Fehler auf, der nur dort vorkommt, wird `auto`
  auf Linux zu `vcpkg` geändert. Wird PlaneGCS in vcpkg aufgenommen, ersetzen wir `third_party/planegcs`.

## Ergänzung (2026-10-14, AP-0.8): Qt-Übersetzungswerkzeuge
Für Übersetzungen mit Qt Linguist (`qsTr` + `.ts`-Dateien, Deutsch und Englisch) kommt `qttools` dazu
(nur `lrelease`/`lupdate` zur Bauzeit, nichts davon wird ausgeliefert): Feature `linguist` in `vcpkg.json`,
Homebrew `qttools`, apt `qt6-tools-dev`, `qt6-tools-dev-tools`, `qt6-l10n-tools`. Das Feature zieht in
vcpkg `qtbase[widgets]` nach, ändert damit den Hash von Qt und aller Qt-Pakete: Der Zwischenspeicher der CI
wird einmal komplett neu gefüllt.

## Belege
- Bauzeiten, Testläufe und Plattformen: Abschlussvermerk AP-0.2 in
  docs/arbeitspakete/phase-0-fundament.md.
- Port-Lizenzen und Versionen: `THIRD_PARTY_NOTICES.md` (generiert).
