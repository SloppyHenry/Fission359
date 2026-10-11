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
- **Kontaktadresse im Code of Conduct** ist ein markierter `TODO`, bis eine Projektadresse feststeht.
- Mac-Entwickler-Werkzeuge `clang-format`, `ccache`, `llvm` (für clang-tidy) wurden per Homebrew installiert.

**Vorgeschlagene Folge-Arbeiten**
- AP-0.2: vcpkg-Manifest; Qt und Catch2 darüber beziehen, `THIRD_PARTY_NOTICES.md` automatisch erzeugen.
- AP-0.3: CI inkl. SPDX-, Format- und Layer-Prüfung; Ubuntu 24.04 als CI-Plattform.
- Sobald ein Mac ohne Xcode/Homebrew verfügbar ist: Frisch-Mac-Test von `scripts/setup`.

---

### AP-0.2 Abhängigkeiten
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.1

**Ziel:** Alle Kernbibliotheken sind reproduzierbar verfügbar.

**Umfang:**
- `vcpkg.json` (Manifest-Modus) mit Baseline: Qt 6 (qtbase, qtdeclarative, qtshadertools, qtsvg), OCCT, Manifold, Eigen, lib3mf, nlohmann-json, spdlog, minizip-ng, catch2, stb, nanosvg
- PlaneGCS: aus FreeCAD extrahieren (eigener Ordner `third_party/planegcs`, Herkunft und Commit dokumentiert) oder einen gepflegten Standalone-Fork prüfen
- Binary-Caching für vcpkg (lokal und CI); OCCT-Build-Optionen minimieren (nur benötigte Module)
- `THIRD_PARTY_NOTICES.md` mit Lizenzinventar; Skript, das die Lizenzen aus vcpkg ausliest
- Mini-Smoke-Test je Bibliothek (z. B. OCCT: Box erzeugen und Volumen prüfen; Manifold: Würfel minus Kugel)

**Akzeptanzkriterien:**
- [ ] Frischer Clone baut auf macOS und Ubuntu 24.04 ohne manuelle Schritte (außer Toolchain)
- [ ] Alle Smoke-Tests grün
- [ ] Alle Lizenzen im Inventar sind GPL-3.0-kompatibel (Prüfskript in der CI)

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
