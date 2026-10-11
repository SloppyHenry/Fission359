# Architektur

> Status: **Vorschlag**. Die mit (Spike) markierten Punkte werden in Phase 0 durch Prototypen bestätigt und per ADR festgehalten.

## 1. Leitentscheidungen

| Entscheidung | Wahl | Begründung |
|---|---|---|
| App-Typ | **Native Desktop-App**, kein Browser, keine Web-UI | Ausdrücklicher Wunsch; volle GPU-Leistung; offline; native Menüs und Trackpad-Gesten |
| Sprache | **C++20** | Alle relevanten Geometrie-Bibliotheken (OCCT, Manifold, PlaneGCS) sind C++. Keine Binding-Schicht, maximale Performance. |
| UI-Framework | **Qt 6 (LTS, ≥ 6.8) mit Qt Quick/QML** | Plattformübergreifend (macOS, Linux, später Windows); modernes, animiertes UI möglich; reife Barrierefreiheit und i18n; LGPL |
| Geometrie-Kern (präzise) | **OpenCASCADE Technology (OCCT) ≥ 7.9** | Einziger reifer, freier B-Rep-Kern: Booleans, Abrunden, Aushöhlen, Offset, STEP. Grundlage von FreeCAD, KiCad 3D, CadQuery |
| Geometrie (Netz/Freiform) | **Manifold ≥ 3.x** | Extrem robuste, schnelle Netz-Booleans; Level-Set/SDF-Unterstützung; auch in OpenSCAD genutzt |
| Skizzen-Solver | **PlaneGCS** (aus FreeCAD) | Bewährter 2D-Constraint-Solver, LGPL; liefert Freiheitsgrade und Konfliktdiagnose |
| Rendering | **Eigener Renderer auf Qt RHI** (Metal auf macOS, Vulkan/OpenGL auf Linux) (Spike) | Volle Kontrolle über Optik, die für Einsteigerfreundlichkeit zentral ist; zukunftssicher auf macOS (Metal). Fallback: OCCT AIS/V3d |
| Build | **CMake + Ninja + vcpkg (Manifest-Modus)**, CMakePresets | Reproduzierbare Abhängigkeiten auf beiden Plattformen |
| Plattformen | macOS 14+ (Apple Silicon), Linux x86_64 (Ubuntu 22.04+, Fedora 40+, Wayland und X11) | Intel-Macs werden von Apple nicht mehr unterstützt; Linux-arm64 optional später |
| Projektlizenz | **Open Source, GPL-3.0-or-later** (Vorschlag) | Schützt davor, dass jemand das Projekt geschlossen weiterverkauft; kompatibel mit allen geplanten Abhängigkeiten (LGPL, Apache-2.0, MIT, BSD) |

### Verworfene Alternativen
- **Electron/Tauri + Three.js:** Widerspricht dem Wunsch „keine Browser-App“. WebKitGTK unter Linux hat schwache WebGL-Performance. Der B-Rep-Kern läge trotzdem nativ daneben, was doppelte Komplexität bedeutet.
- **Rust (wgpu + egui/Slint):** Modern und sicher, aber OCCT-Bindings sind unreif. Man müsste große Teile per FFI anbinden. Kann für einzelne Module später neu bewertet werden.
- **Swift/SwiftUI:** Kein Linux.
- **Parasolid/ACIS (kommerzielle Kerne, wie Fusion/Shapr3D):** Lizenzkosten im fünf- bis sechsstelligen Bereich pro Jahr und mit einem Open-Source-Projekt unter GPL nicht vereinbar. Die Kernel-Abstraktion (GAL, siehe unten) hält trotzdem einen Wechsel auf einen anderen freien Kernel offen.

---

## 2. Schichtenmodell

```
┌──────────────────────────── App / UI  (Qt Quick, QML) ─────────────────────────┐
│ Fenster, Werkzeugleisten, Rezept, Eigenschaften, Hinweise, Missionen, Dialoge   │
├──────────────────────────── Interaction  (C++) ────────────────────────────────┤
│ Werkzeug-Zustandsautomaten · Griffe/Gizmos · Fangen · Auswahl · Eingabe-Mapping │
├──────────── Viewport / Renderer (Qt RHI) ────────────┬──── App-Services ────────┤
│ Szene, Shader, Picking (ID-Buffer), Overlays, Kamera │ Autosave, Settings, i18n, │
│                                                      │ Logging, Updates, Slicer  │
├──────────────────────────── Core  (C++20, OHNE Qt) ────────────────────────────┤
│ Dokument · Feature-Graph · Recompute-Engine · Parameter & Ausdrücke · Einheiten │
│ Commands & Undo · Persistente Referenzen · Serialisierung · Meldungskatalog     │
├──────────────────────── Geometry Abstraction Layer (GAL) ──────────────────────┤
│  occt/ (B-Rep)  │  mesh/ (Manifold, SDF)  │  sketch/ (PlaneGCS)  │  io/ (Formate) │
└────────────────────────────────────────────────────────────────────────────────┘
```

**Regeln:**
1. **Core kennt kein Qt** und keine konkreten Kernel-Typen in öffentlichen Headern. Er spricht nur mit der GAL. Dadurch ist er testbar ohne UI und offen für einen Kernel-Wechsel.
2. **Die GAL kapselt OCCT, Manifold und PlaneGCS.** Typen wie `TopoDS_Shape` erscheinen nicht außerhalb von `geometry/occt/`.
3. **Die UI ändert das Dokument nur über Commands.** Kein direkter Schreibzugriff, sonst funktioniert Undo nicht.
4. **Lange Berechnungen laufen nie im UI-Thread.**
5. **Abhängigkeiten zeigen nur nach unten.** Die Einhaltung wird per CMake-Target-Struktur erzwungen.

---

## 3. Core: Datenmodell

### 3.1 Dokument
```
Document
 ├─ meta           (Format-Version, App-Version, Titel, Erstellt/Geändert)
 ├─ settings       (Einheit, Druckerprofil, Toleranzen, Anzeige)
 ├─ parameters[]   (benannte Maße: Name, Ausdruck, Einheit, Kommentar)
 ├─ features[]     (gerichteter azyklischer Graph, Reihenfolge = Rezept)
 ├─ bodies[]       (Ergebnisse: id, Name, Farbe, Sichtbarkeit, Typ präzise|frei)
 └─ assets[]       (eingebettete Fonts, SVGs, Bilder, importierte Netze)
```

### 3.2 Feature
```
Feature
 ├─ id             (UUID, stabil)
 ├─ type           ("box", "extrude", "fillet", "smooth_union", …)
 ├─ typeVersion    (für Migrationen des einzelnen Feature-Typs)
 ├─ label          (optional, vom Nutzer)
 ├─ params         { name → Value }   Value = { expression: "Breite/2", resolved: 25.0, unit: mm }
 ├─ refs           { name → PersistentRef[] }   Bezüge auf Geometrie anderer Features
 ├─ suppressed     (ausgeblendet)
 ├─ status         ok | warning(Meldung) | error(Meldung)
 └─ outputs        (Körper-IDs, die erzeugt oder geändert werden)
```

- **Feature-Typen sind Plugins** mit einer festen Schnittstelle: `validate()`, `execute(inputs, params, ctx) → Result`, `describe() → Rezept-Satz`, `handles() → Griff-Definitionen`. So lassen sich neue Werkzeuge sauber ergänzen (siehe AP-1.1).
- **Werte sind immer Ausdrücke.** `40` ist der einfachste Ausdruck. Der Ausdrucksparser kennt Einheiten, Grundrechenarten, `min`, `max`, `round`, `sin`, `cos` und `sqrt`, akzeptiert Komma und Punkt als Dezimaltrenner und meldet Zyklen.

### 3.3 Recompute-Engine
- Topologische Sortierung des Feature-Graphen; geänderte Features und alle nachfolgenden werden als *dirty* markiert.
- **Ergebnis-Cache je Feature:** Schlüssel = Hash aus (Feature-Parametern + Hashes der Eingangs-Ergebnisse). Ein Undo kostet dadurch oft keine Neuberechnung.
- **Asynchron** in einem Worker-Thread, mit Abbruch-Token. Eine neue Änderung bricht die laufende Berechnung ab.
- **Interaktive Vorschau** während des Ziehens: Nur das aktive Feature wird auf Basis des gecachten Vorgängers berechnet. Bei teuren Operationen erscheint zuerst eine Netz-Näherung.
- **Fehler sind lokal:** Ein gescheitertes Feature bekommt `status=error`. Nachfolgende Features rechnen mit dem letzten gültigen Ergebnis weiter, wenn möglich.

### 3.4 Persistente Referenzen (Topological Naming)
Das klassische Problem parametrischer CAD-Programme: Ein Feature verweist auf „Fläche 7“. Nach einer Änderung weiter oben heißt die Fläche plötzlich „Fläche 9“, und das Abrunden landet an der falschen Kante. Unsere Strategie:

1. **Generierte Namen:** Jedes Feature benennt die Flächen und Kanten, die es erzeugt, semantisch, z. B. `F12/extrude/side[sketchEdge:E3]`, `F12/extrude/cap:top`.
2. **Namensweitergabe durch Operationen:** über OCCTs Historie (`BRepBuilderAPI_MakeShape::Generated/Modified/IsDeleted`, `BRepTools_History`), auch durch Booleans hindurch.
3. **Geometrische Signatur als Rückfallebene:** Jede Referenz speichert zusätzlich Typ, Schwerpunkt, Normale, Fläche/Länge und Nachbarschaft. Wenn der Name nicht eindeutig auflösbar ist, wird nach Ähnlichkeit gesucht, mit Schwellwert.
4. **Ehrlich bleiben:** Ist die Zuordnung unsicher, bekommt das Feature den Status „braucht Hilfe“. Die UI zeigt die alte Position als Geist und bittet um eine neue Auswahl. Es wird **niemals still falsch zugeordnet**.
5. **Testsuite** mit typischen Änderungsszenarien (Maß ändern, Feature davor einfügen, Skizze ändern), siehe AP-0.6 und AP-2.10.

### 3.5 Commands und Undo
- Jede Änderung ist ein `Command` mit `apply()` und `revert()` auf **Dokumentebene** (Parameter, Feature hinzufügen/entfernen …), nicht auf Geometrie-Ebene. Die Geometrie wird neu berechnet oder kommt aus dem Cache.
- **Transaktionen** fassen zusammen: Ein Zieh-Vorgang mit 200 Zwischenwerten ergibt einen Undo-Schritt.
- Undo-Stack unbegrenzt (nur durch Speicher begrenzt). Er wird nicht in der Datei gespeichert; der Versionsverlauf übernimmt das (AP-5.7).

---

## 4. Geometry Abstraction Layer (GAL)

```cpp
// Skizze der Schnittstelle (vereinfacht)
namespace fission::geo {
  class Solid;      // präziser Körper (B-Rep), intern TopoDS_Shape
  class Mesh;       // freier Körper, intern manifold::Manifold
  class Field;      // implizite Form (SDF), wird bei Bedarf zu Mesh

  struct OpResult { std::variant<Solid, Mesh> body; NamingMap names; std::vector<Diagnostic> diagnostics; };

  OpResult extrude(const Profile&, const ExtrudeParams&);
  OpResult fillet(const Solid&, std::span<const EdgeRef>, double radius);
  OpResult boolean(BoolOp, const Body& a, const Body& b);   // mischt Solid/Mesh automatisch
  Mesh     tessellate(const Solid&, TessParams);             // inkl. Face-/Edge-IDs je Dreieck
  Mesh     smoothUnion(std::span<const Body>, double k);     // SDF-basiert
  ...
}
```

- **Diagnostics** sind strukturierte Fehler (Code, betroffene Elemente, Zahlen wie „max. möglicher Radius“). Der Meldungskatalog macht daraus verständliche Texte. Rohe OCCT-Fehlermeldungen erreichen den Nutzer nie.
- **Gemischte Booleans:** Präzise und freie Körper werden automatisch über Tessellierung in Manifold kombiniert. Das Ergebnis ist frei.
- **SDF für Deko:** Signierte Distanzfelder aus Netzen (BVH + Winding Number oder OpenVDB, entschieden im Spike in AP-4.2) → glatte Operationen → Netz via Manifold `LevelSet`/Dual Contouring.
- **Validierung:** Jedes präzise Ergebnis durchläuft `BRepCheck_Analyzer`. Ungültige Ergebnisse werden als Fehler behandelt, nicht weitergereicht.

---

## 5. Rendering (Spike in AP-0.4)

- **Tessellierung je Fläche,** mit Face-ID pro Dreieck; Kanten als Polylinien mit Edge-ID; Ecken als Punkte.
- **Picking über ID-Buffer:** Ein Offscreen-Render-Pass schreibt IDs. Hover wird mit 60 Hz ausgewertet.
- **Optik:** einfache PBR (matter Kunststoff), SSAO, weicher Schatten aufs Druckbett, Silhouetten- und Kantenlinien, 4× MSAA, Auswahl als Kontur und Aufhellung.
- **Overlays** für Griffe, Gizmos, Maße und Wertblasen: Griffe werden in 3D gerendert (Tiefe korrekt), Texte und Eingabefelder als QML-Overlay.
- **Budget:** 60 fps bei 2 Mio. Dreiecken auf einem M1 bzw. integrierter Intel/AMD-GPU (Linux).
- **Fallback-Option:** OCCT AIS/V3d in Qt eingebettet (schneller zu B-Rep-Picking, aber OpenGL und weniger Kontrolle über die Optik). Die Entscheidung fällt per ADR nach dem Spike.

---

## 6. Präzision und Toleranzen

- **Interne Einheit:** Millimeter, `double`. OCCT-Toleranz `Precision::Confusion()` = 1e-7 mm.
- **Anzeige:** standardmäßig 2 Nachkommastellen (einstellbar). Eingaben werden mit voller Genauigkeit übernommen; `12,345` bleibt `12,345`.
- **Einheiten:** mm (Standard), cm, m, in, ft. Gemischte Eingabe in Ausdrücken ist erlaubt.
- **Anzeige-Tessellierung:** adaptiv und blickabhängig. Beim Hineinzoomen wird im Hintergrund feiner tesselliert, damit Rundungen nie facettiert aussehen. Es gibt keinen „Seiten“-Parameter für runde Formen.
- **Export-Tessellierung:** Sehnenfehler standardmäßig 0,01 mm, Winkeltoleranz 0,5°. Beides ist im Export-Dialog mit Vorschau der Dreiecksanzahl einstellbar.
- **Druck-Toleranzen** (im Druckerprofil): Standard-Spiel 0,2 mm für Steckverbindungen, Lochkorrektur (+0,1 bis 0,2 mm für FDM), Mindestwandstärke = 2 × Düsendurchmesser.

---

## 7. Dateiformat `.fission`

ZIP-Container, menschenlesbar wo möglich:

```
projekt.fission
 ├─ manifest.json        Format-Version, App-Version, Checksummen
 ├─ document.json        Parameter, Features, Referenzen, Körper-Metadaten
 ├─ assets/              eingebettete Schriften, SVGs, Bilder, importierte Netze (.bin / .3mf)
 ├─ cache/               optional: BREP- und Netz-Cache für schnelles Öffnen
 └─ preview.png          Vorschaubild (für Dateimanager und Startbildschirm)
```

- **Quelle der Wahrheit** ist `document.json`. Der Cache darf fehlen oder veraltet sein.
- **Versionierung:** `formatVersion` als Ganzzahl. Für jede Erhöhung gibt es eine Migrationsfunktion, getestet mit archivierten Altdateien.
- **Atomisches Speichern:** in eine temporäre Datei schreiben, dann umbenennen. Autosave alle 60 s und nach jedem 20. Command in ein Wiederherstellungsverzeichnis.
- Import und Export: **3MF** (lib3mf, inkl. Farben/Objekte), **STL** (ASCII/Binär), **STEP** (OCCT XCAF, inkl. Farben), **OBJ**, **SVG** und **DXF** (für Skizzen), Bilder (PNG/JPG) für Reliefs.

---

## 8. Abhängigkeiten und Lizenzen

| Bibliothek | Zweck | Lizenz |
|---|---|---|
| Qt 6 | UI, Rendering-Abstraktion (RHI), Plattform | LGPL-3.0 (dynamisch gelinkt) |
| OpenCASCADE (OCCT) | B-Rep-Kern, STEP, Schriften → Konturen | LGPL-2.1 + OCCT-Ausnahme |
| Manifold | Netz-Booleans, Level-Sets | Apache-2.0 |
| PlaneGCS (+ Eigen) | Skizzen-Constraints | LGPL-2.1+ (Eigen: MPL-2.0) |
| lib3mf | 3MF Lesen/Schreiben | BSD-2-Clause |
| nanosvg | SVG-Import | zlib |
| FreeType | Schriften (über OCCT) | FreeType-Lizenz |
| stb_image | Bilder laden | Public Domain / MIT |
| nlohmann/json | JSON | MIT |
| minizip-ng | ZIP-Container | zlib |
| spdlog / fmt | Logging | MIT |
| Catch2 v3 | Tests | BSL-1.0 |
| Embree 4 *(optional)* | Strahlabfragen für Wandstärken-Analyse | Apache-2.0 |
| Sparkle 2 | Auto-Update macOS | MIT |
| Crashpad *(opt-in)* | Absturzberichte | Apache-2.0 |

**Lizenzregel:** Das Projekt ist Open Source unter **GPL-3.0-or-later**. Erlaubt sind alle **GPL-3-kompatiblen** Lizenzen, bevorzugt permissive (MIT, BSD, Apache-2.0, zlib) und LGPL. **Nicht erlaubt** sind GPL-2.0-only (inkompatibel mit GPL-3), proprietäre und „nicht-kommerziell“-Lizenzen. Jede neue Abhängigkeit wird in `THIRD_PARTY_NOTICES.md` und im Über-Dialog aufgeführt. Jede Quelldatei trägt einen SPDX-Header (`// SPDX-License-Identifier: GPL-3.0-or-later`).

---

## 9. Projektstruktur

```
fission3d/
├─ CMakeLists.txt · CMakePresets.json · vcpkg.json · .clang-format · .clang-tidy
├─ CLAUDE.md                     ← Master-Prompt (Kopie/Verweis auf docs/05)
├─ apps/fission3d/               main.cpp, QML-Einstieg, App-Icon
├─ src/
│  ├─ core/                      Dokument, Features, Recompute, Ausdrücke, Commands, Serialisierung
│  ├─ geometry/
│  │  ├─ gal/                    öffentliche Schnittstelle
│  │  ├─ occt/                   OCCT-Implementierung
│  │  ├─ mesh/                   Manifold, SDF
│  │  └─ sketch/                 PlaneGCS-Anbindung
│  ├─ features/                  ein Unterordner je Feature-Typ (box/, extrude/, fillet/ …)
│  ├─ io/                        3MF, STL, STEP, OBJ, SVG, DXF, Bilder
│  ├─ analysis/                  Druckcheck, Messen, Masse
│  ├─ render/                    RHI-Renderer, Picking, Overlays
│  ├─ interaction/               Werkzeuge, Griffe, Fangen, Auswahl
│  ├─ ui/                        QML-Komponenten + C++-Backends (Models)
│  └─ services/                  Autosave, Settings, i18n, Updates, Slicer-Erkennung
├─ resources/
│  ├─ i18n/ (de, en) · icons/ · fonts/ · printers/ (Druckerprofile, JSON)
│  ├─ missions/ · templates/ · bausteine/ · messages/ (Meldungskatalog)
├─ tests/
│  ├─ unit/ · geometry/ (Golden-Tests) · naming/ · io/ · ui/ · perf/
│  └─ fixtures/                  Testmodelle, Altdateien für Migrationen
├─ packaging/
│  ├─ macos/ (Info.plist, entitlements, dmg) · linux/ (AppImage, Flatpak, .desktop, MIME)
└─ docs/
```

---

## 10. Qualitätssicherung

- **Unit-Tests** (Catch2) für Core, Ausdrücke, Commands und Serialisierung.
- **Geometrie-Golden-Tests:** Für jedes Feature Referenzmodelle mit erwarteten Werten (Volumen, Oberfläche, Bounding Box, Flächen- und Kantenzahl, `BRepCheck` gültig). Toleranz 1e-6 relativ.
- **Naming-Tests:** Szenarien „Maß ändern → Bezug muss stabil bleiben“.
- **IO-Roundtrip-Tests:** speichern → laden → identisch; Export → Re-Import → Volumen gleich.
- **Fuzzing** (libFuzzer) für Ausdrucksparser, Datei-Lader, SVG und 3MF.
- **UI-Tests:** Qt Quick Test für Komponenten; skriptbare End-to-End-Szenarien für die drei Abnahmeabläufe aus dem UX-Konzept.
- **Performance-Benchmarks** in der CI mit Schwellwerten (Regression > 15 % lässt die CI fehlschlagen).
- **Sanitizer** (ASan/UBSan) im Debug-CI-Lauf; Warnungen als Fehler.
- **Nutzertests** mit echten Einsteigern an jedem Phasenende (je 5 Personen, Think-Aloud, SUS-Fragebogen).

---

## 11. Plattform-Besonderheiten

**macOS**
- Apple Silicon (arm64), macOS 14+; Metal über Qt RHI
- Native Menüleiste, ⌘-Kürzel, Trackpad-Gesten (`QNativeGestureEvent`), Vollbild, „Bewegung reduzieren“
- Code-Signatur (Developer ID) mit Hardened Runtime, **Notarisierung**, `.dmg`
- Dateizuordnung `.fission`, später QuickLook-Vorschau-Plugin

**Linux**
- x86_64; Vulkan, sonst OpenGL; Wayland und X11
- **AppImage** (sofort startbar) und **Flatpak** (Flathub); optional `.deb`/`.rpm` später
- `.desktop`-Datei, MIME-Typ `application/x-fission3d`, Icons in allen Größen, Thumbnailer

---

## 12. Performance-Budgets

| Vorgang | Budget |
|---|---|
| Kaltstart bis bedienbar | < 3 s |
| Viewport | 60 fps bei 2 Mio. Dreiecken |
| Hover/Picking-Feedback | < 16 ms |
| Griff-Vorschau beim Ziehen (einfaches Feature) | < 50 ms |
| Neuberechnung typisches Modell (≤ 50 Features) | < 300 ms |
| Datei öffnen (mit Cache) | < 1 s |
| UI-Thread blockiert | nie > 100 ms |
