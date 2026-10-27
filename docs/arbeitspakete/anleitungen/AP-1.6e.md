# AP-1.6e Ausrichten, Aufs Bett legen, Auf Fläche legen, Kopieren und Duplizieren

**Größe:** M (vier kleine Teile, ein PR je Teil möglich) · **Branch:** `ap-1.6e-rest` · **Phasendatei:** AP-1.6, Teil e

## Ziel
- **Ausrichten:** Mehrere Körper wählen, dann mit Punkten am gemeinsamen Rahmen bündig links/mittig/rechts (ebenso vorne/hinten,
  unten/oben) ausrichten, wie in Tinkercad.
- **Aufs Bett legen:** Ein schwebender oder im Bett steckender Körper fällt mit einem Befehl genau auf z = 0.
- **Auf Fläche legen:** Einen vorhandenen Körper mit der Unterseite auf eine Fläche eines anderen legen (auch Wände, Schrägen).
- **Kopieren, Einfügen, Duplizieren (⌘D):** Körper innerhalb des Programms vervielfältigen, mit Versatz.
Jede dieser Aktionen ist **ein** Undo-Schritt und ändert nur Werte von Rezept-Schritten (ADR-0010), kein neuer Rezept-Eintrag
außer bei Einfügen/Duplizieren (dort neue Schritte).

## Voraussetzungen
AP-1.6d erledigt.

## Vorher lesen
- [Handbuch](handbuch.md) §5, §7, §9; `docs/adr/0010-lage-im-rezept.md` (ganz)
- `src/geometry/gal/include/fission/geo/seat.h` (`turnOntoFace`, `seatOnFace`), `DocumentModel::placeOnFace` (`document_model.cpp`)
- `src/core/include/fission/core/selection.h`, `src/ui/include/fission/ui/selection_controller.h` (mehrere gewählte Körper)
- `apps/fission359/qml/Main.qml`: Liste `menus` (Einträge `cut`, `copy`, `paste`, `delete` sind schon da, aber aus) und die Stelle,
  die Menü-`id`s ausführt (`grep -n '"hole"' apps/fission359/qml/*.qml`)
- `src/core/include/fission/core/history.h` (Transaktionen)

## Ausgangslage im Code
- `DocumentModel::beginMove/moveCurrent` bewegt nur den **gewählten Schritt** (`currentStep`). Für mehrere Körper fehlt eine
  Funktion, die den Schritt eines beliebigen Körpers bewegt.
- `DocumentModel::currentBodyBounds()` liefert den exakten Hüllquader des gewählten Körpers.
- `placeOnFace(point, normal)` setzt Lage und Winkel des Schritts der Form, die gerade **abgelegt** wird.
- Ein internes Kopieren gibt es nicht. `History::addFeature(draft)` legt einen Schritt an; `defaultDraft(type)` macht einen Entwurf.

## Festlegungen
- Kopieren/Einfügen nur innerhalb des Programms; die System-Zwischenablage mit dem Dateiformat kommt mit AP-1.10.
- Drehen bleibt um die Körpermitte; nach „Aufs Bett legen“ ändert sich nur z.
- Körper, deren Schritt keine Lage hat (später: Ergebnisse von Kombinieren), werden beim Ausrichten übersprungen, mit Hinweis.

## Haltepunkte (am Anfang fragen)
1. **Versatz beim Duplizieren/Einfügen:** Empfehlung: die Kopie liegt um **10 mm in X und 10 mm in Y** versetzt (sichtbar, leicht
   wiederzufinden); jedes weitere Einfügen derselben Zwischenablage versetzt um weitere 10/10 mm. Alternative: an der gleichen
   Stelle (Tinkercad), dann sieht man die Kopie nicht.
2. **Taste für „Aufs Bett legen“:** Empfehlung: **D** (wie Tinkercad), im Menü Körper. Prüfen, dass D nicht schon belegt ist
   (`grep -n "Qt.Key_D" apps/fission359/qml`).
3. **Auf Fläche legen bei vorhandenen Körpern:** Empfehlung: Menü Körper ▸ „Auf Fläche legen“, danach sagt die Hinweiszeile
   „Klicke auf die Fläche, auf die der Körper soll“, ein Klick legt ihn ab, Esc bricht ab. Alternative: Körper mit gedrückter ⌥
   ziehen. (Empfehlung ist einfacher zu entdecken.)

## Schritte

### Teil 1 – Aufs Bett legen
1. `DocumentModel`: `Q_INVOKABLE bool dropToBed()` – für jeden gewählten Körper (Liste aus `Selection`) den exakten Hüllquader
   holen; `z` des Schritts um `−min.z` ändern. Alles in **einer** Transaktion (`beginChange("dropToBed")` … `commitChange()`).
   Körper ohne Lage überspringen. Gibt false, wenn nichts zu tun war (schon auf dem Bett: |min.z| < 1e-6).
   Dafür eine allgemeine Hilfsfunktion `bool moveBodyBy(const core::BodyId&, std::array<double,3> delta)` bauen (findet den
   Schritt, der den Körper macht, schreibt **nur** geänderte Lage-Werte, Formeln in unveränderten Werten bleiben; Vorbild
   `moveCurrent`). Teil 2 benutzt sie auch.
2. Menü Körper: Eintrag `{ id: "dropToBed", text: qsTr("Drop to bed"), shortcut: "D" }`, aktiv, wenn etwas gewählt ist.
3. Text für „Letzte Aktionen“: „Auf das Bett gelegt“ (de) / „Dropped to bed“ (en).
4. Tests (`test_document_model.cpp`): gekippter Würfel (rotX 45) mit z = 0 steckt im Bett → danach min.z = 0 (±1e-6), x und y
   unverändert, Drehung unverändert; ein Undo-Schritt; ein schwebender Körper (z = 30) fällt auf 0; zwei gewählte Körper in einem
   Schritt.

### Teil 2 – Ausrichten
1. Reine Rechnung in `src/interaction/include/fission/interaction/align.h`:
   `std::vector<Vec3> alignOffsets(const std::vector<Box3>& boxes, int axis, AlignTo to)` mit `AlignTo { min, centre, max }`:
   Verschiebung je Körper, so dass alle die gleiche min/Mitte/max auf dieser Achse haben, bezogen auf den **gemeinsamen** Rahmen
   (min = kleinstes min aller, max = größtes max, Mitte = Mitte des Rahmens).
2. Controller `ui::AlignController` (Vorbild `ScaleController`): sichtbar, wenn ≥ 2 Körper gewählt sind **und** das Werkzeug
   „Verschieben“ an ist (oder über Menü Körper ▸ „Ausrichten“ eingeschaltet, Haltepunkt bei Unklarheit). Zeichnet um den
   gemeinsamen Rahmen je Achse drei Punkte (min, Mitte, max) auf der dem Betrachter zugewandten Seite, wie Tinkercad. Überfahren
   zeigt eine gestrichelte Vorschau-Linie; Klick richtet aus (eine Transaktion, `moveBodyBy` je Körper).
3. QML `AlignLayer.qml` mit Anfassflächen, eingetragen in `FISSION_QML_FILES`, eingebunden in `Main.qml`.
4. Regel 10: Eintrag in `ToolCatalog.qml` (oder als Menübefehl mit Tooltip-Karte und Synonymen: „ausrichten“, „bündig“,
   „zentrieren“, „align“, „centre“, „flush“).
5. Tests: `alignOffsets` für drei Quader (min, Mitte, max je Achse); Controller: ein Klick = ein Undo-Schritt, Formel in unbeteiligter
   Achse bleibt.

### Teil 3 – Auf Fläche legen (vorhandener Körper)
1. `DocumentModel::seatOnFace(const core::BodyId&, geo::Point3d point, geo::Point3d normal)`: Rechnung wie `placeOnFace`, aber für
   den Schritt dieses Körpers und in einer Transaktion. Gemeinsamen Teil von `placeOnFace` herausziehen, nicht kopieren.
2. Ablauf nach Haltepunkt 3: Modus „Fläche wählen“ im `SelectionController` oder ein kleiner eigener Zustand; der Klick nutzt
   `Viewport3D::landingAt(x, y, eigenerKörper, z, onFaces=true)`.
3. Tests: Quader auf die rechte Wand eines anderen Quaders legen → Unterseite berührt die Wand (Abstand < 1e-6 mm, wie die
   `[placement]`-Tests), Drehung Y = 90 (bzw. die von `seatOnFace` erwartete), ein Undo-Schritt.

### Teil 4 – Kopieren, Einfügen, Duplizieren
1. Zwischenablage im `DocumentModel` (nicht global, gehört zum Fenster): `std::vector<core::FeatureDraft> m_clipboard`.
2. `Q_INVOKABLE bool copySelection()`: für jeden gewählten Körper den Schritt, der ihn macht; nur Schritte **ohne Eingänge**
   (`inputs` leer, also Grundformen und Text). Entwurf daraus bauen: Typ, Version, alle `params` (als getippt), `texts`, `label`;
   Körpereigenschaften (Farbe, „Als Loch“, Name) mitnehmen (eigene Struktur neben dem Entwurf). Schritte mit Eingängen: nicht
   kopierbar → Hinweis „Das geht erst mit … (AP-1.7)“ und false.
3. `Q_INVOKABLE bool paste()`: in **einer** Transaktion je Entwurf `addFeature`, Lage-Werte um den Versatz (Haltepunkt 1)
   erhöhen (eine Formel `x = Breite/2` wird zu `(Breite/2) + 10`; Hilfe: `core::scaledExpression` als Vorbild, eigene kleine
   Funktion `offsetExpression`), danach Farbe/Loch/Name per Command setzen. Die neuen Körper werden gewählt.
4. `Q_INVOKABLE bool duplicate()` = `copySelection()` + `paste()` (ändert die Zwischenablage nicht, wenn Haltepunkt das so will:
   Empfehlung, Tinkercad-Verhalten, sie **wird** zur Zwischenablage).
5. Menü: `copy`, `paste` aktivieren; neuer Eintrag `{ id: "duplicate", text: qsTr("Duplicate"), shortcut: primaryKey + "D" }`;
   `cut` = kopieren + Schritt löschen erst, wenn Löschen existiert (AP-1.9), bis dahin aus lassen.
6. **Zahlenfeld mit Fokus:** ⌘C/⌘V/⌘D gehören dem Text, wenn ein Eingabefeld Fokus hat (gleiches Muster wie ⌘Z mit `pendingEdit`,
   siehe `NumberField.qml`). Test dafür.
7. Texte für „Letzte Aktionen“: „{n} Körper eingefügt“ / „{n} bodies pasted“ (Pluralformen über `qsTr("…", "", n)`).
8. Tests: Kopieren + Einfügen eines Quaders mit Formel in der Breite → neuer Schritt mit derselben Formel, Lage um 10/10 versetzt,
   ein Undo-Schritt nimmt alle eingefügten zurück; „Als Loch“ bleibt Loch; zweimal Einfügen versetzt um 20/20.

### Teil 5 – Abschluss AP-1.6
- Phasendatei: Kriterium 2 („Jede Transformation steht als Rezept-Eintrag im Verlauf und ist editierbar“) prüfen und abhaken
  (nach ADR-0010 heißt das: die Lage im Panel des Schritts ist änderbar).
- AP-1.6 auf `Review`, nach Freigabe `erledigt`.

## Abnahme
So probierst du es aus: drei Formen aufs Bett, alle mit Rahmen wählen, an einem Ausrichten-Punkt „links“ klicken → alle linksbündig;
⌘Z. Einen Körper drehen, dass er im Bett steckt, **D** → liegt auf dem Bett. Quader wählen, Körper ▸ Auf Fläche legen, auf die
Wand eines anderen klicken. Quader wählen, ⌘D → Kopie 10 mm daneben; ⌘C, ⌘V → noch eine.

## Stolperfallen
- `moveBodyBy` muss den Schritt über den Körper finden (`stepOfBody`), nicht über `currentStep`.
- Mehrere Körper eines Schritts (später bei Kombinieren): ausrichten verschiebt dann den ganzen Schritt; bis AP-1.7 hat jeder
  Schritt genau einen Körper, trotzdem prüfen und überspringen statt falsch verschieben.
- Hüllquader während einer laufenden Neuberechnung sind veraltet: `currentBodyBounds` liefert dann nichts. Befehle in diesem
  Moment ausgrauen oder kurz warten (`calculating`).

## Nicht tun
- Kein Verteilen mit gleichen Abständen (nicht im Umfang).
- Kein Kopieren über die System-Zwischenablage (AP-1.10).
- Keine Gruppen.
