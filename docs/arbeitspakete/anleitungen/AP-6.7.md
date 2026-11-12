# AP-6.7 Browser-Version

**Größe:** XL (Teilpakete a–d) · **Phasendatei:** AP-6.7 · **ADR:** [0014](../../adr/0014-browser-version.md)

## Ziel
Wer Fission359 ausprobieren will, öffnet eine Internetadresse und konstruiert sofort im Browser, ohne Installation und ohne Konto.
Es ist dasselbe Programm wie auf dem Desktop: gleiche Bedienung, gleiche Ergebnisse, alles rechnet auf dem eigenen Rechner. Dateien
werden heruntergeladen und wieder hochgeladen; nichts geht verloren, auch nicht beim Schließen des Tabs.

## Voraussetzungen
Keine fachlichen. Wird vorgezogen (Festlegung 1) und läuft vor den übrigen Phase-6-APs.

## Vorher lesen
ADR-0014 (ganz), ADR-0002 (Renderer), ADR-0004 (vcpkg), ADR-0005 (CI), `docs/03-architektur.md` §2 (Schichten) und §5 (Threads),
`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`, `.github/workflows/ci.yml`, `src/render/CMakeLists.txt` (Shader),
`src/services/` (Autosave, Wiederherstellung, Einstellungen), `src/ui/src/file_controller.cpp`, `export_controller.cpp`, `import_controller.cpp`.

## Ausgangslage im Code
- Neuberechnung und Bereichserkennung laufen in eigenen Threads (`core/recompute.h`, `ui/sketch_region_worker.h`).
- Dateizugriffe: `ui/file_controller`, `ui/export_controller`, `ui/import_controller`, `services/recovery_store`, `services/autosave`.
- Renderer: `render/scene_renderer.cpp` auf QRhi, Shader über `qt_add_shaders` in `src/render/CMakeLists.txt`.
- Qt kommt in der CI aus vcpkg (Feature `qt`) oder vom System.

## Festlegungen
1. **Reihenfolge (2026-11-10):** 6.7a–d direkt nach AP-2.2a, danach weiter mit AP-2.2b. Ab 6.7d muss jeder PR auch im Web-Job grün sein.
2. **Technik:** wie ADR-0014 (Qt for WebAssembly *multithread*, vcpkg-Triplet `wasm32-emscripten` für die übrigen Abhängigkeiten,
   Emscripten-Version passend zur Qt-Version, WebGL 2, coi-serviceworker, GitHub Pages).
3. **Ein Preset** `web-release` in `CMakePresets.json` und ein Skript `scripts/web` (bauen, lokal mit den nötigen Headern ausliefern:
   `scripts/web serve` öffnet `http://localhost:8359`). Der lokale Server ist ein kleines Python-Skript mit COOP/COEP-Headern.
4. **Speicher:** `-sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=4GB`, Thread-Pool fest (`-sPTHREAD_POOL_SIZE=4`, Neuberechnung braucht ≤ 4).
5. **Im Browser ausgeblendet** (nicht halb angeboten): „Zuletzt geöffnet“, „Im Finder/Ordner zeigen“, Datei-Verknüpfung, Updates,
   Absturzberichte, „An Slicer senden“ als Programmstart (stattdessen Download der Druckdatei).
6. **Hinweis im Programm:** Im Browser zeigt der Startbildschirm einmal eine Karte „Du nutzt die Browser-Version: Speichern lädt die
   Datei herunter, Öffnen holt sie von deinem Rechner. Deine Arbeit wird zusätzlich im Browser gesichert.“ (DE/EN, Glossarbegriffe).
7. **Tests im Browser:** Playwright (nur CI, Apache-2.0) mit Chromium headless. Kein Playwright lokal auf dem Mac nötig.
8. **Testrechner B (Docker):** darf für die Browser-Version genutzt werden: ein eigener Container `fission3d-web` liefert den letzten
   Web-Build aus `main` im lokalen Netz aus (Port 8359, mit COOP/COEP), und Tests (Playwright, Fenster-Tests unter Xvfb) dürfen dort in
   eigenen `fission3d-*`-Containern laufen (`nice -n 19`, höchstens 2 Jobs). Übersetzt wird in der CI (der Rechner hat wenig Speicher);
   der Build kommt als CI-Artefakt. Andere Container auf dem Rechner werden nie angefasst. Zugang und Name stehen nicht im Repo.
9. **Nicht Teil der Browser-Version 1:** Datei-Import über Ziehen aus dem Finder (folgt, wenn Qt es sauber kann), Mehrfenster.

## Haltepunkte
Alle entschieden (Festlegungen 1–8). Neue Fragen: anhalten und berichten.

## Teil 6.7a – Kern in WebAssembly
1. Emscripten-Werkzeugkette in der CI (`emsdk`, Version fest), vcpkg-Triplet `wasm32-emscripten` (ggf. Overlay-Triplet in
   `cmake/triplets/` mit `-pthread`), Preset `web-core` ohne Qt: baut `core`, `geometry` (OCCT, Manifold, PlaneGCS), `features`, `io`,
   `analysis` und deren Unit-Tests.
   **lib3mf** ist im vcpkg-Port nicht für Emscripten freigegeben (`supports` nennt nur Windows/Linux/macOS): Overlay-Port unter
   `cmake/ports/lib3mf/` mit erweitertem `supports` und nötigen Patches. 3MF-Export ist für den Druck zentral; scheitert der Bau,
   anhalten und berichten, nicht weglassen.
2. Unit-Tests laufen unter Node (`node --experimental-wasm-threads` bzw. aktuelle Node-Version) über CTest (`CMAKE_CROSSCOMPILING_EMULATOR`).
3. CI-Job „Web“ (noch **nicht** Pflicht), mit Zwischenspeicher für vcpkg-Binärpakete wie die übrigen Jobs (ADR-0005).
4. Ergebnis: Liste der Tests, die unter WebAssembly scheitern, mit Ursache. Tests werden nicht abgeschaltet; was im Browser
   grundsätzlich nicht geht (z. B. Systemschriften), bekommt einen klar begründeten `#ifdef __EMSCRIPTEN__`-Zweig mit eigenem Test.
5. Bericht: Größe der Wasm-Dateien, Bauzeit, Laufzeit von Ablauf-D-Golden-Tests im Vergleich zum Desktop.

## Teil 6.7b – Programm im Browser
1. Qt für WebAssembly (aqtinstall, `wasm_multithread`, plus passende Host-Qt-Version) in der CI; Preset `web-release` für die ganze App.
2. Shader: GLSL-ES-300-Varianten; Renderer prüft, dass alle Pässe auf WebGL 2 laufen (Fallbacks dokumentieren).
   Stand 2026-11-11: `qt_add_shaders` erzeugt `GLSL "100es,120,150,300es"` (alle 18 Shader); der Lauf auf WebGL 2 folgt mit der App.
3. Plattformzweige nur in `src/services/` (z. B. `services/platform.h`: `isBrowser()`), Festlegung 5 umsetzen.
4. `scripts/web` und lokaler Server (Festlegung 3). Startseite-HTML (`packaging/web/index.html`): Ladebalken, Hinweis bei fehlender
   Cross-Origin-Isolation oder fehlendem WebGL 2 (DE/EN, je nach Browsersprache).
5. Playwright-Test im Web-Job: Seite lädt, App meldet „bereit“ (Konsolenzeile wie beim Smoke-Test), ein Würfel wird über
   `--simulate`-Schritte (als URL-Parameter durchgereicht) platziert, ein Bildschirmfoto wird als CI-Artefakt abgelegt.

## Teil 6.7c – Dateien und „Nichts geht verloren“ im Browser
1. Öffnen (`.fission`, STL, 3MF, STEP, SVG, Bilder) über `QFileDialog::getOpenFileContent`; Speichern und Export über `saveFileContent`
   (Download). Alles hinter einer Schnittstelle in `src/services/` (z. B. `FileAccess`), Desktop-Umsetzung unverändert.
2. Autosave und Wiederherstellung im Browser-Speicher (IDBFS unter `/persist`, `FS.syncfs` nach jedem Autosave); nach Neuladen des
   Tabs bietet das Programm die Wiederherstellung wie auf dem Desktop an.
3. Einstellungen im Browser-Speicher (QSettings auf dem persistenten Pfad).
4. Hinweiskarte (Festlegung 6), Texte DE/EN.
5. Playwright-Tests: speichern → Download enthält gültige `.fission`; Tab neu laden → Wiederherstellung angeboten; Datei hochladen → Körper da.

## Teil 6.7d – Veröffentlichung
0. Vorschau auf Testrechner B (Festlegung 8): `scripts/web deploy-preview <host>` holt das CI-Artefakt und startet/ersetzt den Container
   `fission3d-web` (Caddy oder Python mit COOP/COEP). Der Host kommt als Argument, nie aus dem Repo.
1. Workflow-Schritt: bei Push auf `main` Web-Build nach GitHub Pages (`actions/deploy-pages`), mit `coi-serviceworker.js`, komprimierten
   Dateien und Versionsnummer + Commit im Programm sichtbar („Über“).
2. Web-Job wird Pflicht-Check für PRs.
3. README und `docs/`: Abschnitt „Im Browser ausprobieren“ mit Link, Browser-Anforderungen (aktuelles Chrome, Edge, Firefox, Safari ≥ 17).
4. `THIRD_PARTY_NOTICES.md`: coi-serviceworker; Hinweis Qt unter GPL-3 in der Web-Ausgabe.

## Tests
- 6.7a: alle Unit-Tests des Kerns unter Node grün (Ausnahmen begründet), Ablauf-D-Golden-Werte identisch zum Desktop (1e-9).
- 6.7b: Playwright-Smoke-Test (laden, bereit, Würfel, Bildschirmfoto).
- 6.7c: Download-, Hochlade- und Wiederherstellungstests (Playwright).
- 6.7d: Pages-Deployment liefert die Seite mit funktionierender Isolation (Playwright gegen die Pages-Adresse nach dem Deploy).

## Abnahme
- So probierst du es aus: Adresse öffnen → Startbildschirm → Würfel ablegen, Loch hineinziehen → Speichern (Download) → Tab schließen
  und wieder öffnen → Wiederherstellung angeboten → Datei wieder öffnen.

## Stolperfallen
- Qt-Wasm-Binärpakete brauchen genau ihre Emscripten-Version; falsche Version = kryptische Linkfehler.
- Ohne COOP/COEP gibt es kein `SharedArrayBuffer` → Threads starten nicht. Lokal immer über `scripts/web serve`, nie `file://`.
- Blockierendes Warten im Hauptthread (`QThread::wait`, `QEventLoop::exec` ohne Asyncify) friert den Browser ein; Wartestellen in
  Tests und beim Beenden prüfen.
- OCCT braucht viel Speicher beim Linken; in der CI `-O2` statt `-O3` falls nötig, und `-flto` nur, wenn gemessen.
- `std::filesystem` funktioniert nur im Emscripten-Dateisystem; echte Pfade des Nutzers gibt es nicht.

## Nicht tun
- Keine zweite Oberfläche, keine Server-Berechnung, kein Konto, keine Analyse-/Tracking-Skripte auf der Seite.
- Keine Funktion im Browser halb anbieten (Festlegung 5). Desktop-Verhalten nicht verschlechtern.
