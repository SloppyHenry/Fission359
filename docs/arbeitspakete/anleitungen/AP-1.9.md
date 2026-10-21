# AP-1.9 Rezept-Panel (Version 1)

**Größe:** M, Teile a–d · **Phasendatei:** AP-1.9

## Ziel
Links steht das Rezept als lesbare Sätze („① Quader 50 × 20 × 4 mm“), mit Symbol und Status (ok, braucht Hilfe, geht nicht).
Überfahren hebt die Geometrie hervor, Klick öffnet die Bearbeitung mit Griffen. Schritte lassen sich ausblenden, umbenennen und
löschen (mit Hinweis auf abhängige Schritte). Ein Zeitmarker „jetzt“ lässt sich zurückziehen: Das Modell zeigt den früheren Stand,
neue Schritte kommen an dieser Stelle hinein. Darunter die Körperliste mit Sichtbarkeit, Farbe, Name, Typ-Badge.

## Voraussetzungen
AP-1.2 (erledigt), AP-1.8 erledigt.

## Vorher lesen
- [Handbuch](handbuch.md) §5, §7, §8; `docs/02-ux-konzept.md` §4 (Rezept), §9; `docs/08-gestaltung.md`; Mockup (linke Leiste)
- `apps/fission3d/qml/LeftPanel.qml`, `TreeRow.qml`; `DocumentModel::recipe` (Aufbau der Liste), `selectStep`, `stepOfBody`
- `src/core/include/fission/core/document.h` (`dependentsOf`, `removeFeature`, `setFeatureSuppressed`, `setFeatureLabel`),
  `commands.h` (`makeRemoveFeature`, `makeSetFeatureSuppressed`, `makeSetFeatureLabel` gibt es)

## Ausgangslage im Code
- `DocumentModel.recipe` liefert `{ index, type, state, values, problem }`; QML baut daraus Sätze. Die Liste zeigt schon Schritte und
  wählt per Klick.
- Löschen, Ausblenden, Umbenennen gibt es als Commands, aber ohne Bedienung. Menü Bearbeiten ▸ Löschen ist aus.
- Es gibt keinen Zeitmarker; die Neuberechnung rechnet immer alle Schritte.

## Festlegungen
- Keine modalen Dialoge (Regel 1). Rückfragen erscheinen als Karte **im Panel** (inline), mit Knöpfen.
- „Ausblenden“ statt Löschen ist der empfohlene Weg zum Ausprobieren (UX §4).
- Löschen eines Körpers in der Ansicht (Entf/⌫) = Löschen des Schritts, der ihn macht (und bei kombinierten Körpern: Haltepunkt 3).

## Haltepunkte (am Anfang fragen)
1. **Zeitmarker im Dokument?** Empfehlung: ja, die Stelle des Markers wird mit der Datei gespeichert (wie Fusion), Verschieben ist ein
   Undo-Schritt („Zeitmarker verschoben“). Alternative: nur Ansicht, nicht gespeichert (einfacher, aber beim Öffnen springt das Modell).
2. **Löschen mit Abhängigen:** Empfehlung: Karte „Schritt ③ wird von ④ und ⑤ benutzt. Wenn du ihn löschst, brauchen sie Hilfe.
   [Trotzdem löschen] [Nur ausblenden] [Abbrechen]“. Alternative: Abhängige mitlöschen (gefährlich, verworfen).
3. **Löschen eines kombinierten Körpers** (nach AP-1.7): Empfehlung: löscht den `combine`-Schritt **und** alle Formen, die nur für
   ihn da sind, in einem Undo-Schritt; die Karte zählt sie auf.

## Teil 1.9a – Sätze, Symbole, Status
1. Satz je Typ in QML aus `describe`-Schlüssel und Werten. Eine Tabelle statt verstreuter `if`s: `RecipeSentences.js` (neue Datei,
   in `FISSION_QML_FILES`) mit `function sentence(entry)`; jeder Feature-Typ ein Eintrag (Quader, Zylinder, alle Formen, Text,
   `combine`, `split`). Unbekannter Typ → „Schritt ⑤ (unbekannte Art)“ statt leer.
2. Symbol je Typ (vorhandene Icons in `icons.js`, fehlende als SVG-Pfad ergänzen, Stil des Mockups).
3. Status-Badge: ok (kein Badge), `warning` ⚠ gelb „braucht Hilfe“, `fails` ✕ rot. **Nie nur Farbe** (UX §14): Symbol + Text im Tooltip.
   Ausgeblendete Schritte grau mit „(ausgeblendet)“.
4. Ein Klick auf das Badge öffnet die Erklärung inline unter dem Schritt (Text aus den vorhandenen Fehlertexten; der Katalog kommt
   mit AP-1.12/2.14).
5. Test: jeder registrierte Feature-Typ hat einen Satz (Prüfung in `check_translations.py` nicht nötig; ein QML-Test oder ein
   C++-Test, der `FeatureRegistry::ids()` gegen eine Liste in `RecipeSentences.js` prüft – einfach: Datei lesen und Ids suchen).

## Teil 1.9b – Überfahren, Klicken, Ausblenden, Umbenennen, Löschen
1. **Überfahren** einer Zeile: Körper des Schritts in der Ansicht hervorheben (wie Hover in der 3D-Ansicht; `SelectionController`
   hat eine Hover-Darstellung – eine Funktion `hoverBody(bodyIndex)` ergänzen).
2. **Klicken:** wählt den Schritt und seinen Körper (gibt es); die Griffe des Schritts erscheinen.
3. **Rechtsklick / Kontextmenü der Zeile** und Tasten: Ausblenden (H?) – Taste mit Haltepunkt klären, Empfehlung keine eigene Taste,
   nur Menü; Umbenennen (Doppelklick auf den Namen oder F2, Inline-Feld, `makeSetFeatureLabel`); Löschen (Entf/⌫, `makeRemoveFeature`).
4. Löschen mit Abhängigen: `Document::dependentsOf` → Karte (Haltepunkt 2). Ohne Abhängige: sofort löschen, Undo möglich.
5. Menü Bearbeiten ▸ Löschen aktivieren (wirkt auf gewählte Körper bzw. den gewählten Schritt); ⌘X (Ausschneiden) = Kopieren (1.6e) +
   Löschen, jetzt aktivieren.
6. Tests: Löschen + Undo stellt Schritt mit gleicher Nummer und gleichen Körpern her (`sameContentAs`); Löschen mit Abhängigen erzeugt
   bei „Trotzdem“ Status `inputMissing` bei den Abhängigen; Ausblenden lässt den Körper verschwinden und Undo bringt ihn zurück.

## Teil 1.9c – Zeitmarker
1. Core: `Document` bekommt `std::optional<std::size_t> rollback` (Anzahl der wirksamen Schritte; leer = alle), mit Setter und
   Command `makeSetRollback`. **Dateiformat:** wird in AP-1.10 gespeichert (Haltepunkt 1); falls AP-1.10 schon fertig ist, Feld,
   Roundtrip- und Migrationstest ergänzen (Handbuch/CLAUDE.md: neues Dateiformat-Feld).
2. Neuberechnung: Schritte ab `rollback` werden nicht berechnet (Status `pending`/`off`, Körper unsichtbar). Ergebnis-Cache bleibt.
3. `History::addFeature` bzw. `DocumentModel` fügt neue Schritte an der Marker-Stelle ein (`position`), danach rückt der Marker eins weiter.
4. QML: Linie „── jetzt ──“ zwischen den Zeilen, ziehbar (DragHandler, Einrasten zwischen Zeilen), Doppelklick setzt ans Ende.
   Schritte nach dem Marker blass.
5. Tests: Marker vor Schritt 3 von 5 → nur 2 berechnet; neuer Schritt landet an Stelle 3; Undo stellt alles her.

## Teil 1.9d – Körperliste
- Unter dem Rezept (gibt es teilweise): Zeile je **nicht verbrauchtem** Körper (AP-1.7) mit Auge, Farbpunkt (Klick → Palette aus 1.8c),
  Name (umbenennbar), Badge „Loch“ bzw. später „frei geformt“. Löcher als leerer Umriss (gibt es).
- Tests: Auge-Undo (gibt es), Umbenennen-Undo.

## Abnahme
- Akzeptanzkriterien: Schritt in der Mitte ändern → alles danach korrekt neu (Test mit drei Schritten und Zähler am Kernel);
  Löschen mit Abhängigen fragt inline mit Erklärung.
- So probierst du es aus: Quader, Zylinder als Loch, Text ablegen. Im Rezept über Zeilen fahren: Körper leuchten. Den Quader-Schritt
  löschen: Karte erklärt, dass das Loch ihn braucht. „Nur ausblenden“. Zeitmarker über den Text-Schritt ziehen: Text verschwindet;
  einen Kegel ablegen: er erscheint vor dem Text im Rezept. Marker ans Ende.

## Stolperfallen
- **Nummern:** Die Feature-Nummer (`F5`) wird nie wiederverwendet; nach dem Löschen von F3 lauten die Nummern F1, F2, F4. Zeigt das
  Rezept diese Nummern, entstehen Lücken. Prüfe, was `LeftPanel.qml` heute anzeigt. Empfehlung (als Haltepunkt bestätigen lassen):
  im Rezept die **Position** (①, ②, ③) zeigen, die Feature-Nummer bleibt intern (Namen, Verweise).
- Ausblenden ändert keine Geometrie-Werte, aber die Neuberechnung: `documentChanged(true)`.

## Nicht tun
- Kein Umsortieren per Ziehen (UX §4 erlaubt es, aber nicht im Umfang 1.9; Folgearbeit in [pflege.md](pflege.md)).
- Keine Gruppen im Rezept (später).
- Keine Mini-Vorschaubilder je Schritt (später).
