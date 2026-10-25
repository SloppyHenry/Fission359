# AP-1.10 Dateien: Speichern, Laden, Autosave, Wiederherstellen, Export (Import: Haltepunkt)

**Größe:** M, Teile a–e · **Phasendatei:** AP-1.10

## Ziel
Ein Modell lässt sich als `.fission` speichern und genau so wieder öffnen. Speichern kann nie eine kaputte Datei hinterlassen.
Alle 60 s und nach jedem 20. Befehl wird automatisch gesichert; nach einem Absturz bietet das Programm an: „Wir haben deine Arbeit von
14:32 gerettet.“ Zuletzt geöffnete Dateien stehen im Menü, `.fission` öffnet sich per Doppelklick. Export als STL (binär) und 3MF
(Körper als Objekte, mit Farben), Qualität wählbar.

## Voraussetzungen
AP-1.1 (erledigt). Sinnvoll nach AP-1.7 und AP-1.9 (dann sind `combine`, `split` und der Zeitmarker schon im Dokument).

## Vorher lesen
- [Handbuch](handbuch.md) §3, §5, §10; `CLAUDE.md` (neues Dateiformat-Feld → Roundtrip- **und** Migrationstest)
- `docs/03-architektur.md` §7 (Dateiformat), §6 (Export-Toleranzen: Sehne 0,01 mm, Winkel 5°), §8 (lib3mf, minizip-ng, nlohmann/json)
- `src/core/include/fission/core/document.h` (alles, was gespeichert werden muss), `history.h` (`markClean`, `isClean`)
- `cmake/FissionLayers.cmake` (wer darf `fission_io` benutzen)

## Ausgangslage im Code
- `src/io` ist leer (nur `layer.cpp`). `fission_io` darf lib3mf, minizip-ng, nanosvg, nlohmann_json benutzen.
- `fission_ui` darf `fission_io` **noch nicht** benutzen: in `FissionLayers.cmake` bei `FISSION_ALLOWED_DEPS_fission_ui` ergänzen
  (das ist keine neue Abhängigkeit nach außen, nur eine Schicht-Freigabe; im PR erwähnen).
- Menü Datei hat Neu, Öffnen, Speichern, Speichern unter, Export, Schließen (alle aus).
- `History::markClean/isClean` gibt es schon (für „ungespeichert“).
- MIME-Typ `application/x-fission3d` und macOS-Dokumenttyp sind in `packaging/` angelegt (AP-0.7).

## Aus AP-1.9 und AP-1.8 übernommen (in der Datei zu speichern)
- **Zeitmarker:** `Document::rollback()` (Anzahl der zählenden Schritte, leer = alle) gehört in `document.json` als `rollback` (Zahl; fehlt = alle).
  Beim Laden: Wert ≥ Schrittzahl = alle. Roundtrip- und Migrationstest (Datei ohne das Feld öffnet wie bisher).
- **Farbpalette des Projekts:** Die Palette (Standardfarben, angepasst, plus eigene Farben) liegt bisher nur in `DocumentModel::palette` und geht beim
  Beenden verloren. Empfehlung: nur die **Abweichungen** speichern (`settings.palette`: angepasste Standardfarben nach Schlüssel, eigene Farben in
  Reihenfolge), ein neues Projekt startet weiter mit den Standardfarben. Als Haltepunkt 5 bestätigen lassen.

## Festlegungen
- Format nach Architektur §7: ZIP mit `manifest.json`, `document.json`, `assets/`, `cache/` (optional), `preview.png`.
  Quelle der Wahrheit ist `document.json`. `formatVersion` = **1**.
- Atomisches Speichern: in `<datei>.tmp-<zufall>` im selben Ordner schreiben, `fsync`, dann umbenennen.
- Export-Netz: Sehnenfehler 0,01 mm, Winkel 5° als „Normal“.

## Haltepunkte (am Anfang fragen)
1. **Import STL/3MF:** Die Phasendatei verlangt Import als „freie Körper“. Freie Körper (Netze) gibt es erst mit AP-4.1. Empfehlung:
   Import **nach AP-4.1 verschieben** und in der Phasendatei vermerken. Alternative: Netz in einen präzisen Körper aus Dreiecken
   wandeln (OCCT „Sewing“) – langsam bei großen Dateien, Ergebnis nicht schön editierbar.
2. **Schriften in der Datei:** Empfehlung: mitgelieferte Schriften (IBM Plex, OFL) **nicht** einbetten (sind überall da); Systemschriften
   **nicht** einbetten (Lizenz unklar), nur den Namen speichern; fehlt die Schrift beim Öffnen, meldet der Text-Schritt das (gibt es schon).
3. **Wiederherstellungsordner:** Empfehlung: `QStandardPaths::AppDataLocation/recovery/` (macOS `~/Library/Application Support/Fission3D/recovery`,
   Linux `~/.local/share/Fission3D/recovery`), höchstens 10 Dateien, älteste zuerst löschen.
4. **Ungespeichert schließen:** Ein Rückfrage-Dialog „Änderungen speichern?“ ist kein Geometrie-Dialog; Empfehlung: Standard-Dialog des
   Systems erlaubt. Bestätigen lassen.

## Teil 1.10a – Dokument ↔ JSON
1. `src/core/include/fission/core/serialize.h` + `src/core/src/serialize.cpp` (Core darf nlohmann_json):
   `std::string toJson(const Document&)` und `LoadResult fromJson(const std::string&)` mit `struct LoadResult { std::optional<Document>
   document; std::vector<LoadProblem> problems; }`. **Jedes** Feld des Dokuments: Titel, Einstellungen, Parameter (Name, Ausdruck, Art,
   Kommentar), Schritte (Id, Nummer, Typ, Typ-Version, Label, Werte mit Ausdruck und Art, Wörter, Verweise mit Aussehen, Eingänge,
   Ausgänge, ausgeblendet), Körper (Id, Name, Farbe, sichtbar, Loch, Art, createdBy), `lastFeatureNumber`, Zeitmarker (falls 1.9 fertig).
2. Ids als Text (UUID). Zahlen nie als Zahl speichern, wenn sie als Ausdruck getippt wurden: `expression` ist ein String.
3. Unbekannte Felder beim Laden: ignorieren und in `problems` vermerken (Datei aus neuerer Version), nie abstürzen.
4. Fehlerhafte Datei (kaputtes JSON, fehlende Pflichtfelder): `document` leer, `problems` sagt was (Code, kein Text).
5. **Migrationen:** `fromJson` liest `formatVersion`; eine Tabelle `migrations[v] : json → json` hebt Schritt für Schritt auf die
   aktuelle Version. Für v1 leer, aber der Mechanismus wird getestet (Testdatei mit erfundenem `formatVersion: 0` und einer Migration
   nur im Test).
6. Tests (`tests/unit/core/test_serialize.cpp`): Roundtrip für ein Dokument mit **allen** Feature-Typen, Parametern mit Formeln, Texten,
   Löchern, Farben, Verweisen → `sameContentAs`; kaputte Dateien (leer, abgeschnitten, falscher Typ) → Problem statt Absturz;
   Property-Test: 200 zufällige Dokumente aus dem Zufallsgenerator von `test_history.cpp` → Roundtrip gleich.
   Coverage des Core bleibt ≥ 85 %.

## Teil 1.10b – Container und atomisches Speichern
1. `src/io/include/fission/io/fission_file.h`: `SaveResult save(const Document&, const std::filesystem::path&, const SaveExtras&)`
   und `OpenResult open(const std::filesystem::path&)`. `SaveExtras` = Vorschaubild (PNG-Bytes), App-Version. Mit minizip-ng schreiben.
2. `manifest.json`: `{ formatVersion, appVersion, created, modified, checksums: { "document.json": "sha256:…" } }`. Beim Öffnen
   Prüfsumme prüfen; falsch → Problem „Datei beschädigt“ (Code), Angebot „Trotzdem öffnen“ wenn JSON lesbar.
3. Atomisch: Tempdatei im Zielordner, vollständig schreiben, schließen, `fsync` (Datei und Ordner), umbenennen
   (`std::filesystem::rename`; unter macOS/Linux atomar innerhalb eines Dateisystems). Fehler zwischendurch → Tempdatei löschen,
   Original unberührt.
4. Testbarkeit: eine Fehler-Einspritzung (`SaveOptions::failAfterBytes` nur für Tests) simuliert den Absturz.
5. Tests (`tests/unit/io/test_fission_file.cpp`, neuer Ordner, CMake: `fission_io` in die Testbibliotheken): Speichern → Öffnen gleich;
   Abbruch nach der Hälfte → alte Datei unverändert und lesbar, keine Tempdatei übrig; beschädigte Prüfsumme erkannt;
   **Geometrie-Roundtrip:** nach dem Öffnen neu berechnen → Volumen jedes Körpers gleich (1e-9 relativ).
6. Alt-Dateien für Migrationstests: `tests/fixtures/files/v1/*.fission` (eine Datei je Feature-Typ), ab jetzt **nie mehr ändern**;
   jeder spätere Formatwechsel muss sie laden können.

## Teil 1.10c – Oberfläche: Neu, Öffnen, Speichern, Zuletzt
1. `DocumentModel`: `Q_INVOKABLE bool saveTo(QUrl)`, `openFrom(QUrl)`, `newDocument()`; Eigenschaften `filePath`, `modified`
   (aus `!history.isClean()`), Titel im Fenster „Name.fission •“ bei Ungespeichertem.
   **Laden ersetzt das Dokument** über `History::reset` (Undo-Liste wird geleert; das ist so gewollt).
2. Speichern läuft **nicht** im UI-Thread, wenn das Vorschaubild gerendert wird: Bild auf dem UI-Thread holen (schnell), Schreiben im
   Hintergrund (`QtConcurrent` oder `QThreadPool`), danach `markClean` – aber nur, wenn seitdem nichts geändert wurde (Revisionsnummer
   vergleichen).
3. Menü Datei aktivieren; Dialoge mit `FileDialog` aus `QtQuick.Dialogs` (nativ). Kürzel gibt es schon im Menü.
4. Zuletzt geöffnet: `src/services` (`QSettings`, Liste der letzten 10 Pfade); Menü Datei ▸ „Zuletzt geöffnet ▸“.
5. Öffnen per Doppelklick/Argument: `fission3d <datei.fission>`; macOS: `QFileOpenEvent` in `main.cpp` behandeln.
6. Mehrere Dokumente/Tabs: **nicht** hier (AP-1.11). Öffnen ersetzt das aktuelle Dokument (mit Rückfrage bei Ungespeichertem, Haltepunkt 4).
7. Tests: `test_document_model.cpp` – speichern, ändern, `modified` true, öffnen, gleich wie gespeichert; Titel-Eigenschaft.

## Teil 1.10d – Autosave und Wiederherstellen
1. `src/services/include/fission/services/autosave.h`: Timer 60 s + Zähler (nach jedem 20. Befehl, Signal `historyChanged`), schreibt im
   Hintergrund nach Haltepunkt 3 (`<sitzungs-id>.fission` + kleine `<id>.json` mit Originalpfad und Zeit).
2. Beim Start: liegen Wiederherstellungsdateien einer **nicht sauber beendeten** Sitzung da (Marker-Datei beim Start anlegen, beim
   sauberen Beenden löschen), zeigt das Programm eine Karte (kein modaler Dialog): „Wir haben deine Arbeit von 14:32 gerettet.
   [Wiederherstellen] [Verwerfen]“.
3. Tests: Autosave schreibt nach 20 Befehlen; Wiederherstellen öffnet das gerettete Dokument; sauberes Beenden löscht die Dateien.

## Teil 1.10e – Export STL und 3MF
1. `src/io/include/fission/io/export.h`: `exportStl(const std::vector<MeshForExport>&, path)` (binär, mm) und `export3mf(...)` (lib3mf;
   ein Objekt je Körper, Name, Farbe als `BaseMaterial`/Farbgruppe). `MeshForExport` = Netz aus `GeometryKernel::tessellate` mit
   Export-Parametern (`TessellationParams{0.01, 5°}`, „Fein“ 0,005/3°, „Grob“ 0,05/10°) + Name + Farbe.
2. Tessellieren im Hintergrund (Regel 9), Fortschritt und Abbrechen.
3. Löcher/verbrauchte Körper werden **nicht** exportiert; nur sichtbare Ergebniskörper.
4. Menü Datei ▸ Export…: kleine Seite mit Format (3MF, STL), Qualität, „Nur gewählte Körper“; die volle Export-Seite „Wofür ist die Datei?“
   (R20) kommt mit AP-3.8.
5. Tests: Export → Wiedereinlesen (lib3mf bzw. eigener STL-Leser im Test) → Volumen des Netzes ≈ exaktes Volumen (relativ 1e-3),
   Anzahl Objekte, Farben im 3MF; **Ablauf D Schritt 6**: Abweichung der Lochkante vom Kreis ≤ 0,01 mm (Netzpunkte am Loch messen).
6. Manuelle Prüfung (Akzeptanzkriterium 3): 3MF in Bambu Studio, OrcaSlicer, PrusaSlicer öffnen; Bildschirmfotos in den PR. Wer das
   nicht selbst kann: als offenen Punkt für den Product Owner in den PR schreiben.

## Abnahme
- Akzeptanzkriterien 1–3 (Roundtrip, Absturz-Simulation, Slicer) nachweisbar.
- So probierst du es aus: Modell bauen, ⌘S, Namen wählen. Programm schließen, Datei doppelklicken: alles wie vorher, Rezept änderbar.
  Etwas ändern, Programm hart beenden (Aktivitätsanzeige/`kill -9`), neu starten: Karte „gerettet“. Export ▸ 3MF, im Slicer öffnen.

## Stolperfallen
- Ids müssen beim Laden **dieselben** bleiben (Verweise, Undo-Logik, Namen `F5/…`). `IdSource` darf beim Laden keine neuen vergeben;
  `lastFeatureNumber` mitspeichern.
- Komma/Punkt: Ausdrücke sind Text, nie über `std::to_string` speichern (Gebietsschema!). Zahlen im JSON mit nlohmann (gebietsschema-frei).
- ZIP unter Windows-Pfaden ist egal (kein Windows), aber Dateinamen im ZIP immer mit `/`.
- `fsync` des Ordners: unter macOS `fcntl(F_FULLFSYNC)` für die Datei.

## Nicht tun
- Kein Import von STL/3MF ohne Entscheidung (Haltepunkt 1).
- Kein Speichern des Undo-Stapels (Architektur §3.5).
- Kein `cache/` mit BREP in Version 1 (optional laut Architektur; später für schnelles Öffnen, wenn das Budget „Datei öffnen < 1 s“ reißt).
