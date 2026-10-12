# ADR-0001: Tech-Stack

- **Status:** angenommen
- **Datum:** 2026-10-11
- **Arbeitspaket:** AP-0.1

## Kontext
Fission3D soll ein installierbares 3D-Konstruktionsprogramm für macOS (Apple Silicon) und Linux
werden: so einfach wie Tinkercad, so präzise wie Fusion (docs/01-vision.md). Rahmenbedingungen:
bewusst keine Browser-App, keine Cloud-Pflicht, Open Source unter GPL-3.0-or-later,
Performance-Budgets laut docs/03-architektur.md §12. Der Product Owner programmiert nicht selbst.
Alle relevanten freien Geometrie-Bibliotheken sind C++.

## Optionen
1. **C++20 + Qt 6/QML + OCCT + Manifold + PlaneGCS (CMake/Ninja/vcpkg):** keine Binding-Schicht,
   reife Bibliotheken, volle Kontrolle über Rendering · mehr Build-Aufwand, C++-Disziplin nötig.
2. **Electron/Tauri + Three.js:** schnelle UI-Entwicklung · widerspricht „keine Browser-App“,
   schwache WebGL-Leistung unter Linux, Geometrie-Kern liefe doppelt nativ daneben.
3. **Rust (wgpu + egui/Slint):** modern und speichersicher · OCCT-Bindings unreif, große Teile per FFI.
4. **Swift/SwiftUI:** gute macOS-Integration · kein Linux.

## Entscheidung
Wir wählen **Option 1**:

| Bereich | Wahl |
|---|---|
| Sprache | C++20 |
| UI | Qt 6 (≥ 6.8) mit Qt Quick/QML, dynamisch gelinkt (LGPL-3.0) |
| Präziser Kern | OpenCASCADE (OCCT) ≥ 7.9 |
| Netz/Freiform | Manifold ≥ 3.x |
| Skizzen-Solver | PlaneGCS |
| Build | CMake ≥ 3.28 + Ninja, CMakePresets; vcpkg im Manifest-Modus ab AP-0.2 |
| Tests | Catch2 v3 |
| Lizenz | GPL-3.0-or-later, SPDX-Header in jeder Datei |

Zusätzlich für AP-0.1 festgelegt:

- **Ein CMake-Ziel je Schicht** (`fission_core`, `fission_geometry` = GAL, `fission_geometry_occt|mesh|sketch`,
  `fission_features`, `fission_io`, `fission_analysis`, `fission_services`, `fission_render`,
  `fission_interaction`, `fission_ui`). Erlaubte Abhängigkeiten stehen als Allow-List in
  `cmake/FissionLayers.cmake`; ein Verstoß bricht schon das Konfigurieren ab. Zusätzlich lehnt der
  Compiler Qt in `core` ab (Test `layering_rejects_qt_in_core`) und `scripts/tools/check_layers.py`
  prüft Includes im Quelltext.
- **Kernel-Implementierungen** (`occt`, `mesh`, `sketch`) implementieren nur die GAL und werden
  ausschließlich von der App (`apps/fission3d`) zusammengeführt. Keine andere Schicht kann sie linken.
- **Bezug der Bibliotheken:** in AP-0.1 vorläufig aus Systempaketen, seit AP-0.2 per vcpkg
  (Qt hybrid: System-Qt ≥ 6.8, sonst vcpkg). Siehe [ADR-0004](0004-abhaengigkeiten-vcpkg.md).
- **Rendering** (eigener Renderer auf Qt RHI oder OCCT AIS) bleibt offen und wird per ADR-0002
  nach dem Spike AP-0.4 entschieden.

## Konsequenzen
- Einfacher: eine Sprache vom Kernel bis zur Oberfläche, testbarer Core ohne UI, Kernel-Wechsel über die GAL möglich.
- Schwerer: C++-Build und Abhängigkeits-Management (vcpkg, OCCT-Build) brauchen Sorgfalt; Qt Quick und
  C++ müssen sauber getrennt bleiben.
- Folge-Arbeiten: vcpkg-Manifest und Smoke-Tests (AP-0.2), CI (AP-0.3), Viewport-Entscheidung (AP-0.4).
- Revisionskriterium: Kann der Spike AP-0.4 oder AP-0.5 die Performance-Budgets bzw. die geforderte
  Robustheit mit diesem Stack nicht erreichen, wird diese ADR überprüft.

## Belege
- docs/03-architektur.md §1 (Leitentscheidungen und verworfene Alternativen), §2 (Schichtenmodell)
- AP-0.1: Build und Tests auf macOS 27 arm64 (AppleClang 21, Qt 6.11.2) und Ubuntu 26.04 x86_64
  (GCC 15.2, Qt 6.10.2); zusätzlich CMake 3.28.6 auf Linux. Details im Abschlussvermerk in
  docs/arbeitspakete/phase-0-fundament.md.
