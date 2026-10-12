# Phase 0 – Fundament

**Ziel:** Ein Gerüst, auf dem sich zügig bauen lässt, und die vier großen technischen Risiken sind früh geklärt: Viewport-Technik, Kernel-Robustheit, Topological Naming, Signieren und Paketieren.

**Meilenstein-Demo:** Ein installierbares Fission3D (signiertes `.dmg` und AppImage), das ein Fenster mit einem drehbaren, schön beleuchteten 3D-Würfel auf einem Druckbett zeigt.

---

### AP-0.1 Repository, Build-System und Konventionen
**Status:** Review · **Größe:** S · **Abhängigkeiten:** –

**Ziel:** Ein sauberes Monorepo, das sich auf beiden Plattformen mit einem Befehl bauen lässt.

**Umfang:**
- Git-Repo, `.gitignore`, `.gitattributes` (LFS für Testmodelle > 1 MB)
- CMake ≥ 3.28, Ninja, `CMakePresets.json` (debug, release, asan; je macOS/Linux)
- Verzeichnisstruktur gemäß [Architektur §9](../03-architektur.md#9-projektstruktur)
- CMake-Targets je Schicht (`fission_core`, `fission_geometry`, `fission_render` …) mit erzwungener Abhängigkeitsrichtung (Core linkt kein Qt)
- `.clang-format`, `.clang-tidy`, `.editorconfig`; Code-Konventionen in `CONTRIBUTING.md` (Englisch im Code, Deutsch/Englisch in UI-Texten)
- `CLAUDE.md` im Root mit Verweis auf [05-master-prompt.md](../05-master-prompt.md)
- ADR-Vorlage nutzen ([adr/0000-vorlage.md](../adr/0000-vorlage.md)); ADR-0001 „Tech-Stack“ anlegen
- **Open-Source-Grundausstattung:** `LICENSE` (GPL-3.0-or-later), SPDX-Header-Prüfung in der CI, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` (Contributor Covenant), Issue- und PR-Vorlagen (Fehlerbericht, Funktionswunsch)
- **Ein-Befehl-Skripte für Nicht-Programmierer:** `scripts/setup` (prüft und installiert Voraussetzungen bzw. sagt genau, was fehlt), `scripts/run` (bauen und starten), `scripts/test` (alle Tests mit verständlicher Zusammenfassung)

**Akzeptanzkriterien:**
- [x] `cmake --preset macos-debug && cmake --build --preset macos-debug` (bzw. `linux-debug`) baut ein leeres Qt-Fenster
- [x] Ein Verstoß gegen die Schichtregel (Core inkludiert Qt) führt zu einem Build-Fehler
- [x] README im Root erklärt Build in ≤ 10 Schritten
- [ ] Auf einem frischen Mac genügen `scripts/setup` und `scripts/run`, um die App zu starten, ohne C++-Kenntnisse
  *(auf dem Entwicklungs-Mac und per frischem Klon geprüft, aber noch nicht auf einem Mac ohne Xcode/Homebrew – siehe Abschlussvermerk)*

#### Abschlussvermerk (2026-10-11)

**Umgesetzt**
- Git-Repo (`main`), `.gitignore`, `.gitattributes` (LFS nach Dateiendung unter `tests/fixtures/`), `.editorconfig`,
  `LICENSE` (GPL-3.0-Volltext), `CLAUDE.md` (bindet den Master-Prompt per `@docs/05-master-prompt.md` ein).
- `CMakeLists.txt` (≥ 3.28), `CMakePresets.json` mit `macos-`/`linux-` × `debug`/`release`/`asan`; Warnungen sind Fehler.
- Ordnerstruktur nach Architektur §9; je Schicht ein CMake-Ziel mit Allow-List in `cmake/FissionLayers.cmake`.
- Schichtregel dreifach abgesichert: (1) Konfigurieren bricht bei falscher Link-Abhängigkeit ab,
  (2) Compiler lehnt Qt-Includes in `core` ab (Test `layering_rejects_qt_in_core` mit Kontrolltest),
  (3) `scripts/tools/check_layers.py` prüft Qt und Kernel-Typen (OCCT, Manifold, PlaneGCS) im Quelltext.
- Leeres QML-Fenster `fission3d` mit Entwickler-Option `--smoke-test <datei.png>` (rendert einen Frame, speichert ihn, beendet).
- `.clang-format`, `.clang-tidy`, SPDX-Prüfung `scripts/tools/check_spdx.py`, Catch2-Unit-Test für `fission_core`.
- `scripts/setup`, `scripts/run`, `scripts/test` (Deutsch, mit Rückfrage vor Installationen; getestet mit bash 3.2).
- `README.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), Issue-/PR-Vorlagen,
  [ADR-0001](../adr/0001-tech-stack.md), `THIRD_PARTY_NOTICES.md`, [docs/testumgebung.md](../testumgebung.md).

**Wie getestet**
- macOS 27 arm64 (AppleClang 21, Qt 6.11.2): Presets `debug` und `asan`, `scripts/test` → 7 von 7 Tests grün.
- Ubuntu 26.04 x86_64 (GCC 15.2, Qt 6.10.2, per SSH): Presets `debug`, `release`, `asan` → je 7 von 7 grün.
- CMake 3.28.6 (Mindestversion) auf Linux: Konfigurieren, Bauen, Tests grün.
- Absichtliche Verstöße vorgeführt: `#include <QString>` in `core` → Compiler-Fehler; `Qt6::Core` an `fission_core`
  gelinkt → Konfigurationsfehler.
- Fenster-Screenshots (macOS und Linux, Software-Rendering, ohne Bildschirm) sind erzeugt und geprüft.

**So probierst du es aus**
1. Terminal im Projektordner öffnen.
2. `scripts/setup` ausführen. Es zeigt, was vorhanden ist und was fehlt (Antwort `J` zum Installieren).
3. `scripts/run` ausführen. Nach dem Bauen öffnet sich ein dunkles Fenster mit dem Text „Fission3D“.
4. Fenster schließen, dann `scripts/test` ausführen. Am Ende steht „Alles in Ordnung: 7 von 7 Tests bestanden.“
5. Optional: In `src/core/src/version.cpp` eine Zeile `#include <QString>` einfügen und `scripts/run` starten.
   Das Bauen muss mit „'QString' file not found“ scheitern. Die Zeile danach wieder entfernen.

**Bekannte Einschränkungen und Annahmen**
- **Frischer Mac nicht nachgewiesen:** Der Entwicklungs-Mac hat bereits Xcode und Homebrew. `scripts/setup` ist hier
  nur im Zustand „alles installiert“ und „einzelne Pakete fehlen“ geprüft (auf Linux mit Voll-Installation).
- **CI nicht angebunden:** Die SPDX-Prüfung ist als Skript und CTest-Test vorhanden; der GitHub-Workflow folgt
  in AP-0.3. Die Definition of Done verlangt grüne CI auf beiden Plattformen; das ist bis dahin offen.
- **Linux-Qt:** Läuft mit apt-Qt 6.10 auf Ubuntu 26.04. Ubuntu 24.04 liefert nur Qt 6.4; dort braucht es
  vcpkg (AP-0.2). `scripts/setup` meldet das und bricht kontrolliert ab. Fedora wird von `scripts/setup`
  noch nicht automatisch unterstützt.
- **Fenstertest ohne GPU:** `app_opens_window` nutzt Software-Rendering. Echte Grafik folgt mit AP-0.4/AP-0.8.
- **LFS:** `.gitattributes` kann nicht nach Größe filtern, daher Regeln nach Dateiendung.
- **Kontakt im Code of Conduct:** vorläufig GitHubs privates Melden (Security-Tab, „Report a vulnerability“,
  in den Repo-Einstellungen aktiviert). Eine eigene Projektadresse ersetzt das, sobald sie existiert.
- Mac-Entwickler-Werkzeuge `clang-format`, `ccache`, `llvm` (für clang-tidy) wurden per Homebrew installiert.

**Vorgeschlagene Folge-Arbeiten**
- AP-0.2: vcpkg-Manifest; Qt und Catch2 darüber beziehen, `THIRD_PARTY_NOTICES.md` automatisch erzeugen.
- AP-0.3: CI inkl. SPDX-, Format- und Layer-Prüfung; Ubuntu 24.04 als CI-Plattform.
- Sobald ein Mac ohne Xcode/Homebrew verfügbar ist: Frisch-Mac-Test von `scripts/setup`.

---

### AP-0.2 Abhängigkeiten
**Status:** in Arbeit · **Größe:** M · **Abhängigkeiten:** AP-0.1

**Ziel:** Alle Kernbibliotheken sind reproduzierbar verfügbar.

**Umfang:**
- `vcpkg.json` (Manifest-Modus) mit Baseline: Qt 6 (qtbase, qtdeclarative, qtshadertools, qtsvg), OCCT, Manifold, Eigen, lib3mf, nlohmann-json, spdlog, minizip-ng, catch2, stb, nanosvg
- PlaneGCS: aus FreeCAD extrahieren (eigener Ordner `third_party/planegcs`, Herkunft und Commit dokumentiert) oder einen gepflegten Standalone-Fork prüfen
- Binary-Caching für vcpkg (lokal und CI); OCCT-Build-Optionen minimieren (nur benötigte Module)
- `THIRD_PARTY_NOTICES.md` mit Lizenzinventar; Skript, das die Lizenzen aus vcpkg ausliest
- Mini-Smoke-Test je Bibliothek (z. B. OCCT: Box erzeugen und Volumen prüfen; Manifold: Würfel minus Kugel)

**Akzeptanzkriterien:**
- [ ] Frischer Clone baut auf macOS und Ubuntu 24.04 ohne manuelle Schritte (außer Toolchain)
  *(macOS: nachgewiesen aus frischem Klon mit leerem Zwischenspeicher; Debian 13 nachgewiesen; Ubuntu 24.04: siehe Abschlussvermerk)*
- [x] Alle Smoke-Tests grün
- [ ] Alle Lizenzen im Inventar sind GPL-3.0-kompatibel (Prüfskript in der CI)
  *(Prüfung und Test vorhanden und grün; die CI-Anbindung folgt in AP-0.3)*

#### Abschlussvermerk (2026-10-12, Stand vor Abschluss des Ubuntu-24.04-Laufs)

**Umgesetzt**
- `vcpkg.json` (Manifest-Modus, feste Baseline `e456309491…` vom 2026-10-10) mit OpenCASCADE 8.0.1, Manifold 3.5.4,
  Eigen 5.0.1, lib3mf 2.5.0, nlohmann-json, spdlog, minizip-ng, stb, nanosvg, Boost.Graph (für PlaneGCS), Catch2 und
  dem Feature `qt` (qtbase, qtdeclarative, qtshadertools, qtsvg).
- Qt hybrid: System-Qt ≥ 6.8 wird genutzt, sonst baut vcpkg Qt (`FISSION_QT_SOURCE=auto|system|vcpkg`).
  `scripts/setup` erkennt auf Linux selbst, welcher Weg gilt, und installiert die passenden Systempakete.
- Eigene vcpkg-Triplets (dynamische Bibliotheken, nur Release). vcpkg liegt in `.vcpkg/` (nicht im Repo) und wird von
  `scripts/setup` auf den festen Stand gesetzt.
- PlaneGCS aus FreeCAD 1.1.4 (Commit `4fd3bf32…`) nach `third_party/planegcs`, Quellen byte-identisch, Ersatzdateien
  für FreeCAD-Interna, Herkunft und Update-Anleitung in der README dort.
- Welche Schicht welche Bibliothek linken darf (z. B. OpenCASCADE nur `fission_geometry_occt`), steht in
  `cmake/FissionLayers.cmake` und wird beim Konfigurieren geprüft.
- 10 Smoke-Tests (je Bibliothek einer), Lizenzprüfung `scripts/tools/licenses.py` (löst die Abhängigkeiten ohne
  Bauen für macOS und Linux auf, 129 Komponenten) mit drei Tests (Lizenzen ok, Inventar aktuell, verbotene Lizenz
  wird abgelehnt) und generiertem `THIRD_PARTY_NOTICES.md`.
- Die App zeigt `Fission3D 0.0.1 (OpenCASCADE 8.0.1)` (`--version`, auch ohne Bildschirm) und lädt OpenCASCADE damit
  beim Start wirklich.
- `scripts/tools/docker-ubuntu-check`: Baut und testet den Stand in einem sauberen `ubuntu:24.04`-Container.
- [ADR-0004](../adr/0004-abhaengigkeiten-vcpkg.md); ADR-0001, README, CONTRIBUTING und `docs/testumgebung.md` angepasst.

**Wie getestet**
- macOS 27 arm64 (System-Qt 6.11.2): frischer `git clone`, leerer Zwischenspeicher, `scripts/setup --yes` (21 s) und
  `scripts/test`: Bibliotheken bauen und Tests dauern zusammen **15 Minuten**, 21 bzw. später 23 von 23 Tests grün.
  Warmer Lauf mit Zwischenspeicher: Bibliotheken kommen in unter 1 Sekunde zurück.
- Debian 13 x86_64 (System-Qt 6.8.2, 2 Kerne, niedrige Priorität): erster Lauf **rund 80 Minuten** für die
  Bibliotheken (davon OpenCASCADE ca. 60), 23 von 23 Tests grün.
- Dabei gefundene und behobene Fehler: fehlendes `pkg-config` (macOS), fehlende autoconf-Pakete (Linux),
  GCC-Warnungen in den PlaneGCS-Headern (jetzt System-Header), `--version` ohne Bildschirm,
  Linker ließ OpenCASCADE weg (jetzt echter Aufruf).

**So probierst du es aus**
1. Terminal im Projektordner, `scripts/setup` ausführen (installiert nach Rückfrage fehlende Pakete und richtet vcpkg ein).
2. `scripts/test` ausführen. Beim ersten Mal baut es die Bibliotheken (15–30 Minuten, Fortschritt in
   `build/macos-debug.log`); am Ende steht „Alles in Ordnung: 23 von 23 Tests bestanden“.
3. `scripts/run -- --version` ausführen. Es erscheint `Fission3D 0.0.1 (OpenCASCADE 8.0.1)`.
4. Optional: In `vcpkg.json` ist die Bibliotheksliste. In `THIRD_PARTY_NOTICES.md` stehen alle Lizenzen.
5. Optional (nur mit Docker): `scripts/tools/docker-ubuntu-check` prüft den Stand in einem Ubuntu-24.04-Container.

**Bekannte Einschränkungen und Annahmen**
- **Ubuntu 24.04** ist über den Container geprüft (Ergebnis siehe unten); Qt wird dort aus vcpkg gebaut und braucht
  auf einem kleinen Rechner Stunden. Das passiert nur einmal je Zwischenspeicher.
- **Binär-Cache:** Lokal der vcpkg-Standard (`~/.cache/vcpkg/archives`). Die Anbindung für die CI folgt in AP-0.3.
- **OCCT-Module:** Der Port schaltet Draw und Dokumentation bereits ab; Visualisierung, DataExchange und
  ApplicationFramework bleiben an, weil der Viewport-Spike (AP-0.4, Prototyp B) und STEP-Farben sie brauchen.
  Gelinkt werden nur drei Module. Weiter verkleinern nach AP-0.4.
- **Mischbetrieb auf Linux:** System-Qt und OpenCASCADE aus vcpkg laden zusammen (nachgewiesen mit `ldd`). Gleichnamige
  Bibliotheken (freetype, libpng, zlib) werden vom Lader nur einmal geladen. Heute ohne Folgen, weil die gelinkten
  OCCT-Module sie nicht brauchen; sobald Schriften oder die OCCT-Visualisierung hinzukommen, in der CI mit
  `FISSION_QT_SOURCE=vcpkg` gegenprüfen.
- **Eigen 5.0.1** funktioniert mit PlaneGCS (Smoke-Test grün); die FreeCAD-Version selbst nutzt Eigen 3.4.
- **Boost.Graph** ist neu in der Bibliotheksliste (nur Header, BSL-1.0), nötig für PlaneGCS.
- **Vendored PlaneGCS:** Solver-Logging von FreeCAD ist stumm geschaltet, Warnungen gehen nach stderr.
- **Linux-Testrechner A** war während AP-0.2 nicht erreichbar; alle Linux-Läufe liefen auf Linux-Testrechner B (4 Kerne, mit anderen
  Diensten geteilt, deshalb auf 2 Kerne und niedrige Priorität begrenzt).

**Vorgeschlagene Folge-Arbeiten**
- AP-0.3: CI mit `FISSION_QT_SOURCE=vcpkg`, Binär-Cache und den Lizenz-Tests; clang-tidy auf geänderten Dateien.
- AP-0.4: OCCT-Module nach der Viewport-Entscheidung verkleinern.
- Windows-Triplet und Paketierung (dynamische Bibliotheken bündeln) bei AP-0.7.

---

### AP-0.3 CI-Pipeline
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.2

**Umfang:**
- GitHub Actions (oder Forgejo/GitLab, siehe Rückfragen): Jobs `macos-arm64` und `ubuntu-24.04` × `debug-asan`, `release`
- Caching (vcpkg-Binärcache, ccache/sccache)
- Schritte: Format-Check, clang-tidy (nur geänderte Dateien), Build, Tests, Artefakt-Upload
- Status-Badges im README

**Akzeptanzkriterien:**
- [ ] Pull Requests zeigen Build- und Test-Status beider Plattformen
- [ ] Ein warmer CI-Lauf dauert < 15 Minuten

---

### AP-0.4 Spike: Viewport-Technologie
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-0.2

**Ziel:** Fundierte Entscheidung zwischen eigenem Renderer auf **Qt RHI** und **OCCT AIS/V3d**.

**Umfang:**
- Prototyp A: Qt Quick + RHI-Renderer; tesselliertes OCCT-Modell mit Face-IDs; ID-Buffer-Picking; einfache PBR + SSAO + Schatten; Orbit/Pan/Zoom inkl. Trackpad
- Prototyp B: OCCT AIS in Qt Quick eingebettet; gleiche Funktionen
- Messen: fps bei 0,5 / 2 / 5 Mio. Dreiecken, Picking-Latenz, Optik (Screenshots), Aufwand, Verhalten auf macOS (Metal vs. OpenGL) und Linux (Wayland/X11)

**Akzeptanzkriterien:**
- [ ] ADR-0002 „Viewport-Technologie“ mit Messwerten, Screenshots und Entscheidung
- [ ] Der gewählte Prototyp läuft auf beiden Plattformen und ist Ausgangsbasis für AP-1.3

---

### AP-0.5 Spike: Kernel-Robustheit
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.2

**Ziel:** Wissen, wo OCCT stark und wo es zickig ist, bevor die Features darauf gebaut werden.

**Umfang:**
- Testkatalog mit ca. 30 typischen Maker-Fällen: Booleans mit koplanaren Flächen, Abrunden an Ecken mit 3 Kanten, Abrunden nach Booleans, Aushöhlen mit Rundungen, Text-Booleans, Gewinde-Helix-Sweep, STEP-Roundtrip
- Für jeden Fall: Erfolg, Laufzeit, Gültigkeit (`BRepCheck_Analyzer`), Fallbacks (z. B. `BOPAlgo` Fuzzy-Wert, `ShapeFix`)
- Manifold-Gegenprobe für Booleans auf Netzbasis
- Tessellierungsqualität und -dauer für Export (0,01 mm Sehnenfehler)

**Akzeptanzkriterien:**
- [ ] Bericht `docs/spikes/kernel-robustheit.md` mit Ergebnistabelle und empfohlenen Standard-Einstellungen
- [ ] Liste „bekannte Problemfälle“ als Grundlage für den Meldungskatalog (AP-2.14)
- [ ] Testfälle landen als Golden-Tests in `tests/geometry/`

---

### AP-0.6 Spike: Persistente Referenzen (Topological Naming)
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-0.5

**Ziel:** Ein tragfähiges Konzept nachweisen, *bevor* das Feature-System gebaut wird. Das ist das größte Architekturrisiko.

**Umfang:**
- Prototyp gemäß [Architektur §3.4](../03-architektur.md#34-persistente-referenzen-topological-naming): generierte Namen für Extrude und Box, Weitergabe durch Booleans und Fillet via OCCT-Historie, geometrische Signatur als Fallback
- Szenarien: Quader → Kante abrunden → Quaderhöhe ändern; Skizze → Extrude → Fläche bohren → Skizze um ein Segment erweitern; Feature davor einfügen
- Vergleich mit OCAF/TNaming (OCCT-eigen) und der FreeCAD-1.0-Lösung

**Akzeptanzkriterien:**
- [ ] Alle Szenarien behalten die richtige Referenz oder melden ehrlich „braucht Hilfe“, ohne stille Fehlzuordnung
- [ ] ADR-0003 „Persistente Referenzen“ mit gewähltem Ansatz

---

### AP-0.7 Paketierung: Hallo-Welt
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.3

**Ziel:** Signieren, Notarisieren und Paketieren früh klären, nicht erst vor dem Release.

**Umfang:**
- macOS: `macdeployqt`, Bundle-ID, `Info.plist`, Entitlements (Hardened Runtime), Codesign, `notarytool`, Staple, `.dmg` mit Hintergrund und Applications-Link
- Linux: AppImage via `linuxdeploy` + Qt-Plugin; Flatpak-Manifest (Runtime `org.kde.Platform` 6.x); `.desktop`, Icon, MIME-Typ
- CI-Job „package“ für Tags; Secrets-Handling dokumentiert

> **Hinweis:** Es gibt aktuell nur ein **kostenloses** Apple-Developer-Konto. Developer-ID-Signatur und Notarisierung sind damit nicht möglich. Vorgehen:
> 1. **Jetzt:** Ad-hoc-Signatur (`codesign -s -`), damit die App auf Apple Silicon überhaupt startet. Eine Installationsanleitung erklärt den Gatekeeper-Umweg („Rechtsklick → Öffnen“ bzw. Systemeinstellungen → Datenschutz & Sicherheit → „Trotzdem öffnen“).
> 2. **Später** (bezahltes Konto, 99 €/Jahr, spätestens vor einer öffentlichen Beta): Die Pipeline für Developer-ID und Notarisierung ist vorbereitet und wird per CI-Secret aktiviert.

**Akzeptanzkriterien:**
- [ ] `.dmg` mit ad-hoc-signierter App startet auf einem Mac nach dem dokumentierten Gatekeeper-Umweg
- [ ] Der Notarisierungs-Schritt existiert in der CI, ist ohne Secrets übersprungen und dokumentiert
- [ ] AppImage startet auf Ubuntu 22.04/24.04 und Fedora (Wayland und X11)
- [ ] Flatpak baut lokal und startet

---

### AP-0.8 App-Gerüst mit Würfel
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-0.4, AP-0.7

**Umfang:**
- Hauptfenster mit Platzhalter-Panels (Rezept, Eigenschaften, Hinweiszeile) gemäß UX-Layout
- Viewport aus dem gewählten Spike, Druckbett-Raster, OCCT-Würfel, Kamera-Navigation
- Logging (spdlog) in Datei im Nutzerverzeichnis; Versions-Info im Über-Dialog

**Akzeptanzkriterien:**
- [ ] Meilenstein-Demo erfüllt (siehe oben)
- [ ] Startzeit < 3 s gemessen und im Log ausgegeben
