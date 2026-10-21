# AP-1.11 App-Shell, Einstellungen, Sprachen, mehrere Dokumente

**Größe:** M (eher L), Teile a–f · **Phasendatei:** AP-1.11

## Ziel
Das Programm merkt sich, was man einstellt (Sprache, Drucker, Maus-Stil, Design, Akzentfarbe, Griff-Art, Raster), fragt es beim
ersten Start freundlich ab und zeigt beim Start einen einfachen Startbildschirm (Neu, Zuletzt geöffnet, Beispiele). „Neu“ öffnet ein
weiteres Dokument in einem eigenen Tab. Die Sprache lässt sich ohne Neustart wechseln.

## Voraussetzungen
AP-0.8 (Review, gemergt), AP-1.10 erledigt (Zuletzt geöffnet, Öffnen).

## Vorher lesen
- [Handbuch](handbuch.md) §3, §7, §8; `docs/02-ux-konzept.md` §2.2 (Griff-Art), §10, §11 (erster Start), §13 (Akzentfarbe, Hell/Dunkel),
  §14 (UI-Skalierung); `docs/06-rueckfragen.md` R17–R21; `docs/08-gestaltung.md` (Einstellungen, Startseite, Tabs); Mockup (Einstellungen,
  Startseite, Dokument-Tabs)
- `src/ui/include/fission/ui/theme.h`, `language.h`, `app_options.h`; `apps/fission3d/qml/DocTabs.qml`, `Main.qml` (Menüs),
  `apps/fission3d/src/main.cpp` (Kommandozeile, Singletons)
- `src/ui/include/fission/ui/printer_catalog.h` (Druckerliste bis AP-3.1)

## Ausgangslage im Code
- Sprache: `Language`-Singleton, Umschalten zur Laufzeit geht (`--language`). Theme: Hell/Dunkel, Akzentfarbe ableitbar (`Theme`).
- Nichts wird gespeichert: Maus-Stil, Drucker, Raster-Genauigkeit, „Raster beim Ziehen“, Weiche Schatten sind nach dem Neustart weg.
- Es gibt **ein** `DocumentModel` als Singleton; alle Controller hängen an ihm. `DocTabs.qml` zeigt einen Tab.
- Menü Datei ▸ Neu ist aus.

## Festlegungen
- Einstellungen sind Programm-Einstellungen, **kein** Undo-Schritt und nicht in der Datei (Drucker siehe AP-3.1).
- Griff-Art: „Achsen“ (Standard) und „Klassisch“ (R19); beide lösen dieselben Commands aus.
- Akzentfarbe frei, mit Hinweis bei Ähnlichkeit zu Rot/Gelb/Grün (R18). Schrift auf Akzent ≥ 4,5 : 1 (AA).
- UI-Skalierung 80–200 % (UX §14), kleinste Schrift 12 px bei 100 % (R21).

## Haltepunkte (am Anfang fragen)
1. **Mehrere Dokumente in diesem AP?** Empfehlung: ja, aber schlicht: Tabs oben (gibt es im Mockup), je Tab ein eigenes `DocumentModel`;
   Schließen eines ungespeicherten Tabs fragt (Haltepunkt 4 aus AP-1.10). Alternative: eigenes AP danach.
2. **Griff-Art „Klassisch“ jetzt bauen?** Empfehlung: ja als eigener Teil (f), weil R19 beide verspricht und AP-1.5 sie hierher verschoben hat.
   Sie braucht eigene Darstellung (weiße Eck-Quadrate, Hebe-Pfeil, Maßkästchen wie Tinkercad) – Mockup vorher zeigen lassen.
3. **Erststart-Assistent: Missionen?** „Bist du neu hier? [Mission starten]“ (UX §11) braucht AP-5.1. Empfehlung: Knopf erst mit AP-5.1;
   bis dahin endet der Assistent mit „Los geht's“.

## Teil 1.11a – Einstellungen speichern
1. `src/services/include/fission/services/settings.h`: kleine Klasse über `QSettings` mit **benannten, typisierten** Schlüsseln (keine
   freien Strings im ganzen Code): `language`, `theme` (system|light|dark), `accentColor`, `uiScale`, `mouseStyle`, `handleStyle`
   (axes|classic), `printerId`, `snapStep` (0 = Auto), `snapGridShown`, `softShadows`, `units` (mm|in), `decimals` (0–4), `autosave`
   (an/aus, Minuten), `firstRunDone`. Standardwerte an einer Stelle.
2. Beim Start in `main.cpp` lesen und an die Objekte geben (Language, Theme, Viewport, DocumentModel.setPrinter, …); bei Änderung
   schreiben (Signale verbinden).
3. Tests (`tests/unit/services/test_settings.cpp`, `QSettings` mit eigenem Pfad im Test): Standardwerte, Schreiben/Lesen, unbekannter
   Wert (z. B. Theme „pink“) → Standard.

## Teil 1.11b – Einstellungen-Seite
1. Menü „Einstellungen…“ (gibt es, `role: preferences`) öffnet eine Seite im Fenster (kein modaler Dialog nötig; Gestaltung nach Mockup):
   Bereiche Allgemein (Sprache, Einheit, Nachkommastellen, Autosave), Aussehen (Design, Akzentfarbe mit Vorschlägen + eigene,
   UI-Skalierung), Bedienung (Maus-Stil, Griff-Art), Drucker (Auswahl aus der Liste; Profile kommen mit AP-3.1).
2. Akzentfarbe: Vorschau der abgeleiteten Töne, Hinweistext bei Rot/Gelb/Grün-Nähe (Funktion in `theme_tokens.cpp`, mit Test).
3. UI-Skalierung: Faktor auf alle `Theme`-Größen (Schrift, Abstände) – **nicht** über `QT_SCALE_FACTOR` (bräuchte Neustart). Prüfen, dass
   Layouts bei 200 % nicht überlaufen (Bildschirmfotos 80/100/150/200 % in den PR).
4. Einheit „in“: Anzeige in Zoll, Eingabe ohne Einheit = Zoll (`DocumentSettings::defaultUnitMillimetres` gibt es; ist aber eine
   **Dokument**-Einstellung). Haltepunkt, falls unklar: Empfehlung, Programm-Einstellung „Einheit für neue Dokumente“, im Dokument gespeichert.

## Teil 1.11c – Erster Start
1. Wenn `firstRunDone` fehlt: Assistent in 3 Schritten über der leeren Ansicht (Karte, nicht modal blockierend): Sprache → Drucker
   (Liste mit Bildern/Namen, „Anderer Drucker“) → Maus-Stil (mit kleiner Erklärung je Stil). Danach `firstRunDone = true`.
2. Überspringen jederzeit („Später“), dann Standardwerte.
3. Fenster-Test mit leerem Einstellungspfad (`--settings-dir` als neue Kommandozeilen-Option für Tests): Assistent erscheint; mit
   `firstRunDone` nicht.

## Teil 1.11d – Startbildschirm Version 1
1. Beim Start ohne Datei-Argument: Startseite (Mockup) mit „Neu“, „Zuletzt geöffnet“ (große Kacheln mit `preview.png` aus der Datei,
   AP-1.10) und „Beispiele“ (Ordner `resources/examples/`, zunächst 3 Dateien, im Paket mitgeliefert: Schlüsselanhänger, Box, Stern aus
   vorhandenen Formen).
2. Die volle Startseite „Was möchtest du bauen?“ kommt mit AP-5.4; diese Seite so bauen, dass AP-5.4 sie erweitert (eigene QML-Datei
   `StartPage.qml`).

## Teil 1.11e – Mehrere Dokumente (nach Haltepunkt 1)
1. `ui::DocumentSet` (neu): hält mehrere `DocumentModel` und den aktuellen. Eigenschaft `current`, Liste `documents` (Titel, ungespeichert).
2. Die Controller (Selection, Placement, Handles, Gizmo, Rotate, Scale, …) bekommen eine Funktion `setModel(DocumentModel*)`; beim
   Tab-Wechsel werden sie umgehängt, Auswahl und laufende Züge vorher beendet. **Prüfe jeden Controller** auf gespeicherte Zeiger und
   Verbindungen (`connect`), die umgehängt werden müssen.
3. QML spricht statt `DocumentModel` künftig `Documents.current` an – oder einfacher: das Singleton `DocumentModel` bleibt ein
   **Stellvertreter**, der an das aktuelle Modell weiterleitet. Empfehlung: Umhängen der Singletons vermeiden, stattdessen ein Objekt
   `DocumentModel` behalten und ihm `History` + Ergebnis austauschbar machen (`swapDocument`). Diese Wahl im PR begründen.
4. Tabs (`DocTabs.qml`): Klick wechselt, × schließt (Rückfrage bei Ungespeichertem), „+“ = Neu. Kamera je Dokument merken.
5. Tests: zwei Dokumente, Änderung im einen erscheint nicht im anderen; Undo wirkt nur im aktuellen; Wechsel während eines Zugs bricht
   ihn sauber ab (kein offener `beginChange`).

## Teil 1.11f – Griff-Art „Klassisch“ (nach Haltepunkt 2)
1. Mockup zeigen lassen (das Mockup `oberflaeche.html` hat beide Arten; Bild in den PR).
2. `HandleController` bekommt `style` (axes|classic). Klassisch: weiße Eck-Quadrate an den unteren Ecken (Breite/Tiefe gleichzeitig wie
   Tinkercad? **Nein** – Regel 11: Eck-Griffe in Klassisch ändern **Breite und Tiefe** als zwei Parameter, nie Skalierung), ein weißes
   Quadrat oben Mitte für die Höhe, ein schwarzer Hebe-Pfeil (Lage z), Maßkästchen statt Bänder. Gleiche Commands, gleiche Tests.
   Diese Abbildung (welches Quadrat welche Parameter ändert) als Tabelle in den ADR-Nachtrag oder die Phasendatei.
3. Ablauf D Schritt 1 auch mit Klassisch automatisiert (UX: „Ablauf D wird mit beiden Griff-Arten geprüft“).

## Tests
- Akzeptanzkriterien: Sprachwechsel ohne Neustart (Fenster-Test: `--simulate` neue Aktion `language:de`, Prüfung, dass ein bekannter Text
  danach deutsch ist, z. B. über eine Eigenschaft); `translations_complete` grün (läuft in der CI).
- Siehe Teile.

## Abnahme
So probierst du es aus: Einstellungsordner löschen (Pfad im PR nennen), starten: Assistent fragt Sprache, Drucker, Maus. Einstellungen
öffnen, Akzent Grün → Hinweis. Skalierung 150 %. Programm neu starten: alles gemerkt. ⌘N: zweiter Tab.

## Stolperfallen
- `QSettings` auf macOS schreibt nach `~/Library/Preferences/io.github.sloppyhenry.Fission3D.plist`: `QCoreApplication::setOrganizationDomain`
  und `setApplicationName` müssen vor dem ersten Zugriff gesetzt sein (prüfen in `main.cpp`).
- Tests dürfen nie die echten Einstellungen des Entwicklers ändern: immer eigener Pfad.
- Sprachwechsel: QML-Texte erneuern sich über `qsTr` + `retranslate`; Texte, die in C++ zusammengesetzt werden, nicht (deshalb liefert C++
  nur Schlüssel, Regel 4).

## Nicht tun
- Keine Druckerprofile mit Toleranzen (AP-3.1). Keine Tastenkürzel-Belegung durch Nutzer (AP-5.6). Keine Missionen (AP-5.1).
