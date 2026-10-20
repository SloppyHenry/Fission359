# Handbuch für Umsetzer

Wie man in Fission3D die wiederkehrenden Dinge richtig macht. Jede Bauanleitung verweist hierher („Handbuch §n“).
Stand: 2026-10-20 (nach AP-1.6c1). Wenn der Code anders aussieht als hier beschrieben, gilt der Code; korrigiere dann dieses
Handbuch im selben PR.

---

## §1 Werkzeuge und Befehle

| Zweck | Befehl |
|---|---|
| Voraussetzungen prüfen/einrichten | `scripts/setup` (`--check` nur prüfen) |
| Bauen und starten (für den Product Owner immer Release) | `scripts/run --release` |
| Alle Tests mit Zusammenfassung | `scripts/test` |
| Konfigurieren / bauen / testen direkt | `cmake --preset macos-debug` · `cmake --build --preset macos-debug` · `ctest --preset macos-debug` (Linux: `linux-…`; weitere Presets: `-release`, `-asan`) |
| Nur bestimmte Tests | `ctest --preset macos-debug -R <Name-Teil>` oder `build/macos-debug/tests/unit/fission_unit_tests "[snap]"` (Catch2-Tags) |
| Formatieren prüfen / reparieren | `python3 scripts/tools/check_format.py` / `--fix` (mit clang-format **21.1.8**, siehe §12) |
| SPDX-Köpfe, Schichten, Lizenzen | `python3 scripts/tools/check_spdx.py`, `check_layers.py`, `licenses.py check` |
| Übersetzungen aktualisieren | `cmake --build build/<preset> --target update_translations`, dann §8 |
| Testabdeckung des Core | `scripts/coverage` (Mindestwert 85 %) |

Auf kleinen oder geteilten Rechnern: `FISSION_JOBS=2 nice -n 19 scripts/test`.

## §2 Arbeitskopie, Branch, Commits, PR

1. Nie im Hauptordner arbeiten (dort laufen evtl. andere Sitzungen). Eigene Arbeitskopie:
   ```sh
   git -C ~/Fission3D fetch
   git -C ~/Fission3D worktree add ../Fission3D-<ap> -b <branch> origin/main
   git -C ../Fission3D-<ap> branch --show-current   # muss <branch> sein
   ```
   Branch-Namen: `ap-1.6c2-fangen-griffe`, `ap-1.7a-ausschneiden` usw. Die Arbeitskopie nutzt die Bibliotheken des Hauptordners
   automatisch (`.vcpkg`), das erste Konfigurieren dauert trotzdem einige Minuten.
2. Ein Commit je Schritt der Anleitung. Nachricht englisch: `AP-1.6c2: Snap size handles on points of other bodies`.
   Am Ende jeder Commit-Nachricht die Zeile, die die Sitzung für Commits vorgibt (Co-Authored-By), falls vorhanden.
3. Je Teilpaket **ein PR** gegen `main`. PR-Beschreibung (Deutsch, Alltagssprache): Was kann man jetzt? · So probierst du es aus
   (3–6 Schritte, beginnend mit `scripts/run --release` **aus welchem Ordner**) · Wie getestet · Annahmen · Bekannte Einschränkungen.
4. Gemergt wird erst, wenn die CI grün ist **und** der Product Owner es ausprobiert und freigegeben hat. Danach Arbeitskopie
   entfernen: `git -C ~/Fission3D worktree remove ../Fission3D-<ap>` und `git branch -d <branch>`.

## §3 Wo was hingehört (Schichten)

| Ordner | Inhalt | Darf benutzen |
|---|---|---|
| `src/core` | Dokument, Features-Schnittstelle, Commands, History, Ausdrücke, Neuberechnung | **kein Qt**, keine Kernel-Typen; nur `fission_geometry` (GAL) |
| `src/geometry/gal` | Schnittstelle `geo::GeometryKernel`, `Solid`, `TriangleMesh`, Formen-Specs, reine Mathematik (`rotation.h`, `seat.h`) | nichts |
| `src/geometry/occt` | OpenCASCADE-Umsetzung der GAL (einziger Ort für `TopoDS_*`, `BRep*`) | OCCT |
| `src/geometry/mesh` | Manifold-/SDF-Umsetzung (ab Phase 4) | Manifold |
| `src/geometry/sketch` | PlaneGCS-Anbindung (ab AP-2.2) | PlaneGCS, Eigen |
| `src/features` | Feature-Typen (Box, Zylinder, …, später Extrude, Fillet) | core, GAL |
| `src/io` | Dateiformate (`.fission`, 3MF, STL, STEP) | core, GAL, lib3mf, minizip, json |
| `src/analysis` | Messen, Druckcheck | core, GAL |
| `src/interaction` | reine Zieh-/Fang-Mathematik ohne Fenster (`drag.h`, `snap.h`) | core, GAL, render |
| `src/render` | RHI-Renderer, Picking, Kamera, Griff-Grafik | core, GAL, Qt Gui |
| `src/ui` | `DocumentModel`, Controller (Griffe, Gizmo, Drehen, Größe, Ablegen, Auswahl), `Viewport3D`, Theme | alles darunter, Qt Quick |
| `src/services` | Protokoll, Startzeit, später Einstellungen, Autosave | core, Qt Core |
| `apps/fission3d` | `main.cpp` (Verdrahtung, `--simulate`), QML-Oberfläche, Übersetzungen, Schriften | alles |

Die erlaubten Abhängigkeiten stehen in `cmake/FissionLayers.cmake`; ein Verstoß bricht den Build. Neue Quelldateien müssen in die
`SOURCES`-Liste der jeweiligen `CMakeLists.txt` (es gibt kein automatisches Einsammeln). Neue QML-Dateien in `FISSION_QML_FILES`
in `apps/fission3d/CMakeLists.txt`. Neue Testdateien in `tests/unit/CMakeLists.txt`.

## §4 Rezept: neuer Feature-Typ (ein neuer Rezept-Schritt)

Vorbild: `src/features/src/cylinder.cpp` (einfach), `shapes.cpp` (mehrere), `text.cpp` (mit Wörtern).

1. Klasse in `src/features/src/<name>.cpp`, abgeleitet von `core::FeatureType` (`src/core/include/fission/core/feature_type.h`):
   `id()` (englisch, klein, z. B. `"extrude"`), `version()` = 1, `parameters()` (Werte mit Standard; Lage mit `withPlace(...)`,
   Drehung mit `turnParameters()`), `validate()` (nur billige Prüfungen, gibt `FeatureIssue` mit Code zurück),
   `execute()` (ruft nur `context.kernel` auf; benennt **jede** Fläche, Namens-Vertrag ADR-0003), `describe()` (Schlüssel +
   Werte, **kein Text**), `handles()`.
2. In `src/features/src/builtin.cpp` registrieren; Datei in `src/features/CMakeLists.txt` eintragen.
3. Braucht der Typ eine neue Kernel-Fähigkeit: §6.
4. Oberfläche: Satz im Rezept (`describe`-Schlüssel) und Feldnamen in QML übersetzen (§8). Wo die Sätze gebaut werden:
   `grep -rn '"cylinder"' apps/fission3d/qml` zeigt die Stellen, an denen ein neuer Typ ergänzt werden muss (Rezeptsatz, Feldnamen,
   Formen-Galerie, Symbol).
5. Tests: Golden-Test (§10) für die Geometrie, Unit-Test für `validate`, `describe`, `handles`.
6. Fehler eines Schritts sind `FeatureIssueCode`s. Ein neuer Code braucht einen Text in der Oberfläche (bis AP-1.12/2.14 dort,
   wo die vorhandenen Codes übersetzt werden: `grep -rn "parameterOutOfRange" apps/fission3d/qml`).

## §5 Rezept: neue Änderung am Dokument (Command)

Regel 3: Die Oberfläche ändert das Dokument **nur** über `core::History::execute(command)`.

1. Fehlt eine Änderungsfunktion im `core::Document` (`document.h`), dort ergänzen (gibt `bool` zurück: false = nichts geändert
   oder ungültig).
2. Command in `src/core/src/commands.cpp` (Fabrikfunktion in `commands.h`, z. B. `makeSetBodyColor`). Er merkt sich den alten
   Zustand für `revert` und beschreibt sich mit `CommandDescription` (Schlüssel + Werte, kein Text).
3. Test in `tests/unit/core/test_history.cpp`: apply → revert ergibt `sameContentAs` den Ausgangszustand; der Zufallstest
   (1000 Befehle) bekommt den neuen Befehl in seine Auswahl.
4. In `DocumentModel` (`src/ui`) eine `Q_INVOKABLE`-Funktion, die den Command ausführt und danach `documentChanged()` aufruft.
5. Für Ziehen: `beginChange(field)` … viele Änderungen … `commitChange()`; Esc = `cancelChange()`. Ein Zug ist **ein** Undo-Schritt.
6. Den Text für „Letzte Aktionen“ und das Bearbeiten-Menü in QML ergänzen (Schlüssel aus `describe`): `grep -rn "setBodyHole" apps/fission3d/qml`.

## §6 Rezept: neue Fähigkeit des Geometrie-Kerns

1. Virtuelle Funktion in `geo::GeometryKernel` (`src/geometry/gal/include/fission/geo/kernel.h`) mit Doku-Kommentar: Einheiten
   (mm, Grad), welche Flächen-Rollen das Ergebnis hat, welche `GeometryError`-Codes sie wirft. Nur GAL-Typen in der Signatur.
2. Umsetzung in `src/geometry/occt/src/` (neue Datei oder `solids.cpp`), eingehängt in `occt/src/kernel.cpp`. Jedes Ergebnis mit
   `BRepCheck_Analyzer` prüfen (Hilfen in `occt_build.h`); ungültig → `GeometryError(kernelResultInvalid)`, nie durchreichen.
3. Fehlt ein OCCT-Modul (z. B. `TKOffset`, `TKFillet` stecken in `ModelingAlgorithms` und sind schon gelinkt; `DataExchange` für
   STEP nicht): in `src/geometry/occt/CMakeLists.txt` ergänzen, nicht global.
4. Golden-Tests in `tests/unit/geometry/` (§10). Der Kern muss mehrere Aufrufe gleichzeitig vertragen (keine statischen Variablen).

## §7 Rezept: Oberfläche (QML) und Controller

- C++-Logik für die Oberfläche lebt in Controllern unter `src/ui` (Vorbilder: `scale_controller.cpp`, `rotate_controller.cpp`).
  Sie werden in `apps/fission3d/src/main.cpp` als Singleton angemeldet:
  `qmlRegisterSingletonInstance("Fission3D.Ui", 1, 0, "<Name>", &instanz);` und in QML mit `import Fission3D.Ui` benutzt.
- Griffe, Pfeile, Marken zeichnet der Renderer (`render::HandleGraphics`), tiefenrichtig. QML liefert nur Beschriftungen,
  Eingabefelder und unsichtbare Anfassflächen. Vorbild: `ScaleLayer.qml` + `ScaleController`.
- Farben und Größen nur aus `Theme` (`src/ui/include/fission/ui/theme.h`), nie feste Werte in QML. Gestaltung: `docs/08-gestaltung.md`,
  Vorlage `docs/mockups/oberflaeche.html` (im Browser öffnen und vergleichen).
- Jeder Text über `qsTr("English source")` (§8). Fester Text ohne `qsTr` lässt den Test `translations_complete` scheitern.
- **Regel 10:** Ein neues Werkzeug bekommt einen Eintrag im Katalog (`apps/fission3d/qml/ToolCatalog.qml` für Paletten-Werkzeuge,
  `ShapeCatalog.qml` für Formen): `name`, `tip` (Tooltip-Satz), `hint` (Hinweiszeile), `synonyms` (de und en, Alltagswörter).
- Lange Rechnungen nie im UI-Thread (Regel 9): über die Neuberechnung (`RecomputeEngine`) oder einen `QThreadPool` mit Ergebnis
  per `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
- C++-Objekte, die eine `Q_INVOKABLE`-Funktion an QML zurückgibt, gehören sonst JavaScript: Gib ihnen einen QObject-Elternteil,
  sonst stürzt das Programm beim Beenden ab.
- `contentItem: Icon {...}` in einem `AbstractButton` sitzt oben links: in ein `Item` packen und zentrieren.

## §8 Texte und Übersetzungen

1. Quelltext englisch in `qsTr("...")`. Begriffe nur aus dem Glossar (`docs/02-ux-konzept.md` §12), englische Begriffe aus der
   EN-Spalte.
2. `cmake --build build/<preset> --target update_translations` (braucht Homebrew- oder apt-Qt, nicht das vcpkg-Qt).
3. In `apps/fission3d/i18n/fission3d_de.ts` jede neue `<message>` mit `type="unfinished"` übersetzen: `<translation>` füllen und
   `type="unfinished"` entfernen. In `fission3d_en.ts` die englische Quelle als Übersetzung eintragen. Von Hand oder mit einem
   kleinen Skript (Muster `<translation type="unfinished">[^<]*</translation>`; nie über Zeilen hinweg ersetzen).
4. `ctest --preset <preset> -R translations` muss grün sein.
5. Deutsch: Alltagssprache, Du-Form, kurze Sätze, keine Anglizismen, wo das Glossar ein deutsches Wort hat.

## §9 Fenster-Tests mit `--simulate`

Das echte Programm spielt Eingaben ab und macht ein Bild. Registriert in `tests/CMakeLists.txt`, Vorbild `app_snap_catch`:
```cmake
add_test(NAME app_<name>
  COMMAND ${_fission_gui_runner} $<TARGET_FILE:fission3d>
          --smoke-test "${CMAKE_BINARY_DIR}/window-<name>.png" --language en --size 1280x800
          --camera=0,30,160 --simulate "put:box;type:23.3,enter;gizmo:x,48,0,8;near:x,-21.65")
set_tests_properties(app_<name> PROPERTIES
  ENVIRONMENT "${_fission_gui_env};${_fission_lsan_env}" TIMEOUT 90 RUN_SERIAL TRUE)
```
Vorhandene Aktionen (Doku in `apps/fission3d/src/main.cpp`, Funktion `playActions`, und `fission3d --help`):
`hover:x,y` · `click:x,y` · `shiftclick:x,y` · `doubleclick:x,y` · `frame:x1,y1,x2,y2` · `drop:form,x,y[,alt]` · `drophole:…` ·
`put:form` · `puthole:form` · `type:50,tab,20,enter` · `set:feld,wert` · `grab:parameter,dx,dy,schritte` · `point:parameter` ·
`gizmo:teil,dx,dy,schritte[,halten]` · `ring:achse,…` · `corner:i,dx,dy,schritte[,halten]` · `drag:…` · `tool:select|move|rotate|size` ·
`key:…` · `grid:0.1` · `snap:off|on` · Prüfungen `expect:feld,min` · `least:…` · `near:feld,wert` (warten die Neuberechnung ab und
beenden das Programm mit Fehler, wenn sie nicht stimmen).
Eine **neue Aktion** kommt in `playActions` in `main.cpp` (gleiches Muster: Zeit `delay += 350`, dann `QTimer::singleShot`), plus
eine Zeile in der `--simulate`-Hilfe. Fenster-Tests, die die Maus ziehen, bekommen `RUN_SERIAL TRUE` (macOS stiehlt sonst den Fokus).
Ein Test, der scheitern **soll** (Gegenprobe), bekommt `WILL_FAIL TRUE` (Vorbild `app_snap_off_does_not_catch`).
Bild für den PR: `build/<preset>/window-<name>.png`, oder manuell
`build/macos-release/apps/fission3d/fission3d.app/Contents/MacOS/fission3d --smoke-test /tmp/x.png --simulate "..."`.

## §10 Tests

- **Unit-Tests** (Catch2) in `tests/unit/<schicht>/test_<thema>.cpp`, Tags in eckigen Klammern (`[snap]`). Testnamen sind Sätze,
  die das Verhalten beschreiben: `"snap: a size handle catches on the face of another box"`.
- **Golden-Tests** für jede Geometrie-Operation: Volumen und Fläche gegen die **Formel** (nicht gegen den Kernel), relative
  Toleranz 1e-6; Hüllquader; Anzahl Flächen/Kanten/Ecken; `valid == true`; Flächen-Rollen vollständig. Vorbild
  `tests/unit/features/test_primitives.cpp`.
- **Oberfläche ohne Fenster:** Controller und `DocumentModel` lassen sich mit `QCoreApplication`/`QGuiApplication` testen (Vorbild
  `tests/unit/ui/test_scale.cpp`). `QGuiApplication::queryKeyboardModifiers()` stürzt dort ab: über eine Abfrage schützen, die
  prüft, ob eine `QGuiApplication` läuft (Vorbild `GizmoController::snappingOff`).
- **Renderer ohne GPU:** `test_scene_renderer.cpp` nutzt die Null-Schnittstelle von Qt RHI.
- **Undo:** Jede neue Nutzeraktion: ein Test „ein Zug = ein Schritt“, „Undo stellt den Ausgangszustand her“.
- Bekannt wackelig: „pick on the graphics card: 2 million triangles“ (unter Last), `app_start_time` (erster Start nach dem Bauen).
  Einmal neu laufen lassen; bleibt es rot, ist es ein echter Fehler.

## §11 Wo Entscheidungen hingehören

- Neue Abhängigkeit, neues Muster, Abweichung von der Architektur → ADR in `docs/adr/` (Vorlage `0000-vorlage.md`), vorher Haltepunkt.
- Entscheidung des Product Owners → Festlegungen in der Phasendatei und ggf. `docs/06-rueckfragen.md`.
- Gestaltung → `docs/08-gestaltung.md` (bei Abweichung vom Mockup dort vermerken).

## §12 Stolperfallen (gesammelt)

- **clang-format-Version:** Die CI nutzt 21.1.8; Homebrew hat neuere, die fremde Dateien umformatieren. Eigene Umgebung:
  `python3 -m venv /tmp/cf && /tmp/cf/bin/pip install clang-format==21.1.8`, dann
  `CLANG_FORMAT=/tmp/cf/bin/clang-format python3 scripts/tools/check_format.py --fix`.
- **macOS `sed -i`** braucht `''`; zsh braucht Anführungszeichen um Muster (`--include='*.h'`). Für mehrzeilige Änderungen Python.
- **Ziffern 1–6 sind Ansichten.** Maße tippen beginnt nur direkt nach dem Ablegen oder während eines Zugs.
- **Debug ist langsam** (OCCT): dem Product Owner immer `scripts/run --release` nennen und sagen, aus welchem Ordner.
- **Bildschirmfotos:** nie den ganzen Bildschirm fotografieren oder Klicks an den Desktop schicken. Eigene Bilder über
  `--smoke-test` + `--simulate` oder `screencapture -l <Fenster-ID>`.
- **Werte mit Formeln:** Wer einen Wert setzt, der sich nicht ändert, überschreibt sonst die getippte Formel. Nur geänderte Werte
  schreiben (Vorbild `DocumentModel::moveCurrent`).
- **Zug, der nicht endet:** Verschwinden die Griffe mitten im Zug (Neuberechnung), muss der gehaltene Griff bleiben, sonst bleibt
  die Transaktion offen und Undo geht nicht mehr (siehe `HandleController::refresh`).
- **Achse zeigt auf den Betrachter:** Ziehen entlang einer Achse, die fast in den Bildschirm zeigt, nie räumlich, sondern über den
  Bildschirm (`interaction::AxisDrag::screenOf`), sonst rast der Wert davon.
- **Fangpunkte aus dem Anzeige-Netz** haben Rauschen von etwa 1e-5 mm; `interaction::cleanedCoordinate` glättet es.
- **Linux-CI:** Was auf dem Mac zufällig geht (Rundung, Schriften), scheitert dort. Grenzfälle (Kante genau im Blick, fehlende
  Schrift) immer mit eigenem Test absichern.
