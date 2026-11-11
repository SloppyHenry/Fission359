# ADR-0014: Browser-Version mit Qt for WebAssembly

- **Status:** angenommen
- **Datum:** 2026-11-10
- **Arbeitspaket:** AP-6.7

## Kontext
Fission359 war als reine Desktop-App geplant (ADR-0001, Vision „Bewusst nicht im Umfang“). Der Maintainer wünscht zusätzlich eine
**Browser-Version**: ohne Installation ausprobieren, z. B. auf fremden Rechnern oder in der Schule. Die Produktprinzipien bleiben:
kein Konto, keine Cloud-Pflicht, nichts geht verloren, präzise Ergebnisse. Die Desktop-Version bleibt die Hauptversion.

Rahmen: C++20-Kern mit OCCT, Manifold, PlaneGCS; Oberfläche Qt Quick; eigener Renderer auf Qt RHI (ADR-0002); Neuberechnung im
Hintergrund-Thread (Regel 9).

## Optionen
1. **Qt for WebAssembly, dieselbe Codebasis.** Der ganze Code (Kern, Oberfläche, Renderer) wird mit Emscripten nach WebAssembly
   übersetzt und läuft im Browser. Qt RHI nutzt dort WebGL 2 (OpenGL-ES-Pfad). · Eine Codebasis, gleiche Bedienung, gleiche
   Ergebnisse, rechnet lokal. · Großer Download (geschätzt 30–60 MB, komprimiert weniger), 32-bit-Speicher (max. 4 GB), Threads nur
   mit „Cross-Origin-Isolation“, Dateizugriff nur über Browser-Dialoge.
2. **Eigene Web-Oberfläche (z. B. Three.js) + Kern als WebAssembly.** · Kleinere, „webigere“ Oberfläche. · Zweite Oberfläche, die
   doppelt gepflegt werden muss und auseinanderläuft; widerspricht „überall dieselbe Zahlenfeld-Komponente“.
3. **Rechnen auf einem Server, Bild/Netz zum Browser streamen.** · Kleiner Download. · Server, Kosten, Konto-Fragen, offline unmöglich;
   widerspricht „keine Cloud-Pflicht“.

## Entscheidung
Wir wählen **Option 1**, weil sie als einzige ohne zweite Oberfläche und ohne Server auskommt. Im Einzelnen:

- **Qt:** offizielle Qt-Binärpakete für WebAssembly in der Variante *multithread* (gleiche Qt-Version wie auf dem Desktop, ≥ 6.8),
  in der CI über `aqtinstall`. Die übrigen Abhängigkeiten baut vcpkg mit dem Triplet `wasm32-emscripten`. Emscripten-Version =
  die von Qt für diese Version vorgegebene.
- **Threads:** Emscripten-pthreads. Dafür braucht die Seite die Header `Cross-Origin-Opener-Policy: same-origin` und
  `Cross-Origin-Embedder-Policy: require-corp`. GitHub Pages setzt sie nicht; deshalb liefert die Seite `coi-serviceworker` (MIT) mit,
  der sie nachträglich setzt. Ohne Isolation (alter Browser) erscheint eine verständliche Startseite statt eines Absturzes.
- **Rendering:** WebGL 2 über Qt RHI. Alle Shader bekommen eine GLSL-ES-300-Variante (`qt_add_shaders … GLSL "300es,330"` o. ä.).
  Teure Effekte dürfen im Browser abgeschaltet sein, wenn Messungen das verlangen; Geometrie und Genauigkeit nie.
- **Dateien:** Öffnen über den Datei-Dialog des Browsers, Speichern und Export als Download, Autosave und Wiederherstellung im
  Browser-Speicher (IndexedDB über das Emscripten-Dateisystem). Alles hinter `src/services/` (Architektur §2), damit der Rest des Codes
  nichts davon weiß. „Zuletzt geöffnet“ entfällt im Browser.
- **Hosting:** GitHub Pages des Repositorys, gebaut von der CI aus `main`. Kein eigener Server, keine Domain.
- **Lizenzen:** Qt wird unter GPL-3 genutzt (statisches Linken in WebAssembly ist mit der Projektlizenz vereinbar). Emscripten (MIT/
  NCSA) ist nur Werkzeug. `coi-serviceworker` (MIT) kommt in `THIRD_PARTY_NOTICES.md`. Playwright (Apache-2.0) nur für CI-Tests.

## Konsequenzen
- Jeder neue Code muss auch unter Emscripten bauen: kein `fork`, keine Prozesse, keine plattformspezifischen Systemaufrufe außerhalb
  von `src/services/`. Die CI bekommt einen Job „Web“, der ab AP-6.7d Pflicht ist.
- Desktop-Funktionen ohne Browser-Gegenstück (Datei-Verknüpfung, „Im Finder zeigen“, Updates, Absturzberichte) werden im Browser
  ausgeblendet, nicht halb angeboten.
- Schriften für das Text-Werkzeug müssen mitgeliefert werden (im Browser gibt es keine Systemschriften).
- Folgearbeit: AP-6.7a–d (Phasendatei 6).
- **Revisionskriterium:** Lässt sich OCCT unter Emscripten nicht mit vertretbarem Aufwand bauen, oder bricht eine Referenz-Konstruktion
  (Ablauf D) im Browser wegen Speicher ab, wird die Browser-Version zurückgestellt und neu bewertet (z. B. Option 2 nur zum Ansehen).

## Belege
- Qt for WebAssembly: https://doc.qt.io/qt-6/wasm.html (Multithreading, Dateidialoge `QFileDialog::getOpenFileContent`/`saveFileContent`).
- OCCT unter Emscripten: offizielle Unterstützung seit 7.x (`BUILD_LIBRARY_TYPE=Static`, Emscripten-Toolchain), Projekt opencascade.js.
- Manifold hat einen eigenen WebAssembly-Build (manifold-3d auf npm).
- coi-serviceworker: https://github.com/gzuidhof/coi-serviceworker
