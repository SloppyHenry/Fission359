# Phase 0 – Fundament

**Ziel:** Ein Gerüst, auf dem sich zügig bauen lässt; die großen technischen Risiken (Viewport, Kernel-Robustheit, Topological Naming, Paketieren) sind früh geklärt.

**Meilenstein-Demo:** Installierbares Fission359 (`.dmg`, AppImage) mit drehbarem, beleuchtetem 3D-Würfel auf einem Druckbett.

Ausführliche Abschlussvermerke dieser Phase stehen im Git-Verlauf (vor 2026-10-20).

---

### AP-0.1 Repository, Build-System und Konventionen
**Status:** Review · **Größe:** S · **Abhängigkeiten:** –

**Ergebnis:** CMake-Presets `macos-|linux-` × `debug|release|asan` (Warnungen = Fehler), Schicht-Ziele mit Allow-List in
`cmake/FissionLayers.cmake` (Core ohne Qt, dreifach geprüft), `.clang-format`/`.clang-tidy`, SPDX-Prüfung, Open-Source-Grundausstattung,
`scripts/setup|run|test`, ADR-0001.

**Offen:** Kriterium „frischer Mac ohne Xcode/Homebrew genügt `scripts/setup` + `scripts/run`“ ist nicht nachgewiesen.

---

### AP-0.2 Abhängigkeiten
**Status:** erledigt (2026-10-20, vom Product Owner bestätigt) · **Größe:** M · **Abhängigkeiten:** AP-0.1

**Ergebnis:** `vcpkg.json` mit fester Baseline (OCCT 8.0.1, Manifold, Eigen 5, lib3mf, nlohmann-json, spdlog, minizip-ng, stb, nanosvg,
Boost.Graph, Catch2, Feature `qt`). Qt hybrid (`FISSION_QT_SOURCE=auto|system|vcpkg`), Host- = Ziel-Triplet (sonst baut vcpkg Qt
doppelt). PlaneGCS aus FreeCAD 1.1.4 in `third_party/planegcs`. Lizenzprüfung `scripts/tools/licenses.py` erzeugt
`THIRD_PARTY_NOTICES.md`. Ubuntu-24.04-Check im Container: `scripts/tools/docker-ubuntu-check`. Siehe ADR-0004.

**Offen:** OCCT-Module weiter verkleinern; bei Schriften/OCCT-Visualisierung Mischbetrieb System-Qt + vcpkg-Bibliotheken in der CI mit
`FISSION_QT_SOURCE=vcpkg` gegenprüfen.

---

### AP-0.3 CI-Pipeline
**Status:** erledigt (2026-10-20, vom Product Owner bestätigt) · **Größe:** M · **Abhängigkeiten:** AP-0.2

**Ergebnis:** `.github/workflows/ci.yml`: Checks (clang-format 21, SPDX, Schichten, Lizenzen) → Libraries (Cache füllen) → Build (macOS
arm64 + Ubuntu 24.04, `debug-asan` + `release`). Cache nur auf `main`, wöchentlicher Lauf hält ihn warm. Kalt ca. 2 h, warm ca. 3½ min.
clang-tidy nur beratend. Siehe ADR-0005.

---

### AP-0.4 Spike: Viewport-Technologie
**Status:** erledigt (2026-10-13) · **Größe:** L · **Abhängigkeiten:** AP-0.2

**Ergebnis:** ADR-0002 angenommen: eigener Renderer auf **Qt RHI** (Prototyp A). Leistung trennte die Optionen nicht; A ist
OpenGL-unabhängig, headless testbar und gibt volle Kontrolle über die Optik. Prototypen in `spikes/viewport/`
(`-DFISSION_BUILD_SPIKES=ON`, `scripts/spike`).

---

### AP-0.5 Spike: Kernel-Robustheit
**Status:** erledigt (2026-10-20, vom Product Owner bestätigt) · **Größe:** M · **Abhängigkeiten:** AP-0.2

**Ergebnis:** 42 Konstruktionsfälle als Golden-Tests in `tests/geometry/`, gleiche Ergebnisse auf Mac und Linux. Befunde:
(1) Aushöhlen gerundeter Quader liefert still falsche Körper; Gültigkeit allein reicht als Prüfung nicht. (2) Fast bündige Flächen
brauchen einen Fuzzy-Wert. (3) Export-Winkeltoleranz 5° statt 0,5° (R16).

**Offen:** Große Modelle, fremde STEP-Dateien, variabler Abrundungsradius, Mehr-Thread-Booleans ungeprüft. Für AP-2.6 eine allgemeine
Plausibilitätsprüfung (Volumen, Flächen-/Körperzahl) einplanen.

---

### AP-0.6 Spike: Persistente Referenzen (Topological Naming)
**Status:** erledigt (2026-10-20, vom Product Owner bestätigt) · **Größe:** L · **Abhängigkeiten:** AP-0.5

**Ergebnis:** Prototyp in `tests/naming/`; ADR-0003 angenommen (nur strikter Modus, eigene Namensschicht auf der Kernel-Historie, kein
OCAF). 0 stille Fehlzuordnungen bei 1 494 Zufallsverweisen. Das Gedächtnis für verbrauchte Elemente ist nötig (ohne es 8 stille Fehler);
die großzügige Variante „⚠ bitte prüfen“ wird nicht gebaut.

**Offen (für AP-2.10):** Fasen, Aushöhlen, Muster, Spiegeln, Text, Booleans mehrerer Körper ungeprüft; Zufallsprobe mit den echten
Features wiederholen.

---

### AP-0.7 Paketierung: Hallo-Welt
**Status:** erledigt (2026-10-14) · **Größe:** M · **Abhängigkeiten:** AP-0.3

**Ergebnis:** ID `io.github.sloppyhenry.Fission359`. `.dmg` ad hoc signiert (Gatekeeper-Umweg, vom Product Owner auf sauberem macOS 14
geprüft), Notarisierung vorbereitet (aktiv mit Secrets, bezahltes Konto). AppImage und Flatpak (verpackt das fertige Programm, nicht
Flathub-tauglich). `scripts/package`, CI-Job „Package“. Ubuntu 22.04 entfällt (Backlog B-15). Siehe ADR-0006, [paketierung.md](../paketierung.md).

**Offen:** echtes Icon (Platzhalter), Quellbau-Flatpak vor AP-6.1, Paketgröße, `.fission` per Doppelklick öffnen.

---

### AP-0.8 App-Gerüst mit Würfel
**Status:** erledigt (2026-10-20, vom Product Owner bestätigt) · **Größe:** S · **Abhängigkeiten:** AP-0.4, AP-0.7

**Ergebnis:** Fenster nach Mockup und [08-gestaltung.md](../08-gestaltung.md); Menüs auf dem Mac in der System-Menüleiste, unter Linux im
Fenster. RHI-Renderer (Bett, Schatten, Kanten, Kamera, Ansichtswürfel), Theme-Tokens in C++, `NumberField.qml`, `qsTr` mit
Deutsch/Englisch, spdlog-Protokoll, Startzeit < 3 s, Über-Fenster. Siehe ADR-0007.

**Festlegungen:** Abweichungen vom Mockup nur, wo das Mockup sonst Unwahres zeigt (kein Lotse ohne Missionen, kein „Automatisch
gespeichert“ ohne Autosave); ganze Pixel statt 12,5/14,5 px. Design-System schlägt UX §14 bei der Mindestschriftgröße.
