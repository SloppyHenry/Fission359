# AP-1.8 Eigenschaften-Panel, Zahlenfeld, Farbe, Kontextleiste

**Größe:** M, Teile a–d · **Phasendatei:** AP-1.8

## Ziel
Rechts steht immer, **was gewählt ist** und **was man daran ändern kann**; Seltenes steckt hinter „▸ Mehr“. Jede Zahl im Programm
läuft über **ein** Zahlenfeld, das Formeln, Einheiten, Komma und Punkt versteht, mit Pfeiltasten zählt, sich durch Ziehen am Namen
ändern lässt und Fehler verständlich erklärt („‚Breitee‘ kenne ich nicht. Meintest du ‚Breite‘?“). Jeder Körper bekommt eine Farbe aus
einer Palette oder frei gewählt. Neben der Auswahl in der 3D-Ansicht erscheint eine **Kontextleiste** mit den 4–6 wahrscheinlichsten
Aktionen.

## Voraussetzungen
AP-1.1 (erledigt). Sinnvoll nach AP-1.7 (dann hat die Kontextleiste Vereinen/Ausschneiden) und AP-1.6e (Duplizieren, Aufs Bett).

## Vorher lesen
- [Handbuch](handbuch.md) §5, §7, §8; `docs/02-ux-konzept.md` §1, §2.1, §2.3, §12; `docs/08-gestaltung.md` (Panel, Zahlenfeld,
  Abschnitte, Farben); Mockup `docs/mockups/oberflaeche.html` (rechtes Panel, Kontextleiste ansehen)
- `apps/fission3d/qml/NumberField.qml`, `RightPanel.qml`, `Section.qml`
- `src/ui/include/fission/ui/numbers.h` (Formelauswertung für Felder), `src/core/include/fission/core/expression.h` (`ExpressionError`)
- `src/core/include/fission/core/commands.h` (`makeSetBodyColor` gibt es schon)

## Ausgangslage im Code
- `NumberField.qml` kann Formeln, Einheiten, Komma/Punkt, Achsen-Buchstaben, `invalid` (roter Rand), `pendingEdit` für ⌘Z.
  Es kann **nicht**: Pfeiltasten, Ziehen am Namen, Fehlertext mit Vorschlag.
- `RightPanel.qml` zeigt Abschnitte „Maße“, „Lage“, „Drehung“, Wörter (Text, Schrift) und eine feste Farbreihe (vier Farben, nur Anzeige?
  prüfen, ob sie schon wirkt: `grep -n "setBodyColor" -r apps src`).
- Fehler eines Ausdrucks liefert der Core als `ExpressionError` (Art + Stelle im Text).

## Festlegungen
- Regel 2: überall dieselbe Komponente. Kein `TextField` für Zahlen außerhalb von `NumberField`.
- Pfeiltasten: ↑/↓ ±1, mit ⇧ ±10, mit ⌥ ±0,1 (Phasendatei). In der Einheit des Felds (mm bzw. Grad).
- Fehler werden inline unter dem Feld erklärt, kein Dialog (Regel 1, 5).

## Haltepunkte (am Anfang fragen)
1. **Pfeiltaste auf einer Formel** (`Breite/2`): Empfehlung: Die Formel wird zu `(Breite/2) + 1` (bleibt Formel), Anzeige des Werts aktualisiert.
   Alternative: wird zur Zahl. (Empfehlung schützt Regel 3 „alles bleibt änderbar“.)
2. **Farbpalette:** Empfehlung: 10 Vorgaben, gut unterscheidbar und farbenblind-tauglich (UX §13), plus „Eigene Farbe…“ (Qt-Farbwähler,
   nicht modal, als Popover). Die Vorgaben schlägt die Umsetzung vor, der Product Owner bestätigt per Bild.
3. **Inhalt der Kontextleiste** bei Körper / Fläche / Kante / mehreren Körpern (Vorschlag in Teil d). Bestätigen lassen.

## Teil 1.8a – Zahlenfeld vervollständigen
1. Pfeiltasten in `NumberField.qml` (`Keys.onUpPressed` usw.), Schrittweite nach Modifikator. Ergebnis über denselben Weg wie Tippen
   (`textCommitted`), damit Undo und Formeln gleich behandelt werden. Formel-Regel nach Haltepunkt 1: neue Hilfsfunktion im Core
   `std::string adjustedExpression(const std::string& expression, double delta)` (Vorbild `core::scaledExpression` in `scaling.h`;
   fasst `(X) + 1` und `+1` zusammen: `(X) + 2`, und beim Zurück auf 0 wieder `X`). Unit-Tests dafür.
2. **Ziehen am Namen** (Label): `DragHandler` auf dem Label, horizontal; 1 Pixel = 0,1 mm (mit ⇧ ×10, mit ⌥ ×0,1); während des Ziehens
   ist es **eine** Transaktion (`DocumentModel.beginChange(feldname)` … `commitChange()`), Esc bricht ab. Zeiger `Qt.SizeHorCursor`.
3. **Fehlertext mit Vorschlag:** Neue Funktion in `Numbers` (C++): `Q_INVOKABLE QVariantMap explain(const QString& text)` →
   `{ kind, start, end, unknownName, suggestion }`. Vorschlag: der bekannte Name (benannte Maße des Dokuments und Feldnamen des Schritts)
   mit dem kleinsten Levenshtein-Abstand ≤ 2. Texte in QML: „‚%1‘ kenne ich nicht. Meintest du ‚%2‘?“ (en: “I don't know ‘%1’. Did you
   mean ‘%2’?”), ohne Vorschlag „‚%1‘ kenne ich nicht.“, weitere Arten (Klammer fehlt, Division durch null, Einheit passt nicht: „Hier
   gehört eine Länge hin, kein Winkel.“). Die fehlerhafte Stelle im Feld unterstreichen. Ein Klick auf den Vorschlag ersetzt das Wort.
4. Unit-Tests (`tests/unit/ui/test_numbers.cpp`): Vorschlag für „Breitee“ = „Breite“; kein Vorschlag bei Abstand 3; Einheitenfehler.
5. Audit: `grep -rn "TextField\|TextInput" apps/fission3d/qml` – jede Zahleneingabe außerhalb `NumberField` umstellen
   (Akzeptanzkriterium 1). Ausnahme: `SnapControl.qml` benutzt hoffentlich schon `NumberField`; prüfen.

## Teil 1.8b – Panel nach Auswahl
1. Zustände (in `RightPanel.qml` sauber getrennt, je ein Abschnitt-Satz):
   - **Nichts gewählt:** Hinweis (gibt es).
   - **Ein Körper (oder Teil davon):** Kopf mit Name (umbenennbar, Command `makeSetBodyName`), Typ-Badge, Farbe; Abschnitte Maße, Lage,
     Drehung, Wörter; „▸ Mehr“ klappt Seltenes auf (z. B. „Durchgehend“, Eckenradius bei Quadern, Werkzeug-Körper behalten).
   - **Mehrere Körper:** Anzahl, gemeinsame Farbe (setzt alle in einem Undo-Schritt), Ausrichten/Vereinen-Knöpfe (falls gebaut).
   - **Fläche/Kante gewählt:** Name des Körpers und was gewählt ist („Fläche von Quader 1“), dazu die Werte des Körpers wie oben.
2. Welche Werte „selten“ sind, sagt der Feature-Typ: `ParamSpec` um `bool advanced = false` erweitern; Grundformen setzen es für
   Eckenradius? **Nein**, Eckenradius ist Ablauf A/D wichtig – nur wirklich Seltenes (z. B. `through`, Text-Zeilenabstand). Liste im PR nennen.
3. Tests: `test_document_model.cpp` – `stepValues` trägt `advanced`; Umbenennen ist ein Undo-Schritt.

## Teil 1.8c – Farbe
1. Palette (Haltepunkt 2) als Liste in `Theme`/`theme_tokens.cpp` (nicht in QML fest), Kontrast gegen Hell und Dunkel geprüft
   (`test_theme_tokens.cpp` erweitern).
2. Klick auf eine Farbe = `DocumentModel.setBodyColor(index, farbe)` → `makeSetBodyColor` (Command gibt es). Mehrere gewählte Körper
   = eine Transaktion.
3. „Eigene Farbe…“: `ColorDialog` aus `QtQuick.Dialogs` **nicht modal** öffnen oder eigener kleiner Farbkreis; bestätigen per Klick.
4. Farbe ist kein Geometrie-Wechsel: `documentChanged(false)` (keine Neuberechnung).
5. Tests: Farbe setzen, Undo, Szene hat neue Farbe ohne neues Netz (`meshVersion` unverändert).

## Teil 1.8d – Kontextleiste
1. Schwebende Leiste neben dem gewählten Element in der 3D-Ansicht (Position aus der Projektion des Hüllquaders, Ausweichen mit
   `clearplace.js`), Gestaltung nach Mockup.
2. Vorschlag Inhalt (Haltepunkt 3):
   - Körper: Als Loch · Farbe · Duplizieren · Aufs Bett legen · Löschen (wenn AP-1.9 gebaut) · Mehr ▾
   - mehrere Körper: Vereinen · Ausschneiden · Ausrichten · Farbe
   - Fläche: Auf Fläche schauen · Skizze darauf (ab AP-2.1) · Loch (ab AP-2.7) · Farbe (ab AP-3.4)
   - Kante: Abrunden (ab AP-2.5)
   Nicht gebaute Aktionen werden **weggelassen**, nicht ausgegraut.
3. Jede Aktion ruft dieselbe Funktion wie das Menü (keine zweite Logik). Tooltip-Karten aus dem Werkzeug-Katalog.
4. Die Leiste verschwindet beim Ziehen und bei Esc, kommt nach dem Loslassen wieder.

## Tests
Siehe Teile. Zusätzlich Fenster-Test `app_number_field_arrows`: Quader, Feld Breite fokussieren (neue `--simulate`-Aktion
`focus:width` nötig), `key:up`, `key:shift+up`, `near:width,31` bei Start 20.

## Abnahme
- Akzeptanzkriterien: ein Zahlenfeld überall (Audit im PR belegen), Erklärung mit Vorschlag.
- So probierst du es aus: Quader wählen; in „Breite“ `Breitee*2` tippen → Erklärung mit Vorschlag, Klick auf den Vorschlag repariert.
  ↑ in „Höhe“ → +1, mit ⇧ +10. Am Wort „Tiefe“ ziehen → Wert ändert sich, ⌘Z nimmt den ganzen Zug zurück. Farbe Rot wählen.
  Die Leiste neben dem Quader zeigt Als Loch, Farbe, Duplizieren …

## Stolperfallen
- Pfeiltasten im Feld dürfen den Körper nicht verschieben (die Ansicht nutzt Pfeiltasten zum Verschieben, AP-1.6a): Ereignis im Feld
  annehmen (`event.accepted = true`).
- Ziehen am Namen und `pendingEdit` (getippter, nicht bestätigter Text) vertragen sich nicht: Ziehen beginnt erst, wenn kein Text offen ist.

## Nicht tun
- Keine Maße-Tabelle (AP-2.9), keine Autovervollständigung (AP-2.9).
- Keine Farbe je Fläche (AP-3.4).
