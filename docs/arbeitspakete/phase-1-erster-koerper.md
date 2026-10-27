# Phase 1 – Der erste Körper

**Ziel:** Alles, was man für ein erstes Erfolgserlebnis braucht: Formen platzieren, exakt bemaßen, bewegen, kombinieren, rückgängig machen, speichern, exportieren. Am Ende fühlt sich Fission3D schon besser an als Tinkercad.

**Meilenstein M1 (Demo):** Ablauf A „Schlüsselanhänger“ aus dem [UX-Konzept](../02-ux-konzept.md#15-beispielabläufe-referenz-für-tests) mit flachem Text (Prägen auf gekrümmte Flächen folgt in Phase 4) → STL/3MF-Export → druckbar.

Bei erledigten APs stehen hier nur Ergebnis, weiter geltende Festlegungen und offene Reste. Die ausführlichen Abschlussvermerke
(Messwerte, Testlisten, Ausprobier-Anleitungen) stehen im Git-Verlauf (vor 2026-10-20).

---

### AP-1.1 Dokumentmodell und Feature-Graph
**Status:** erledigt (2026-10-14, PR #14–#18) · **Größe:** L · **Abhängigkeiten:** AP-0.5, AP-0.6

**Ergebnis:** Formelparser (Fehlerstellen, `min max round sin cos sqrt`, Einheiten `mm cm m in " ft °`), benannte Maße mit Zyklus-Pfad;
`Document` als kopierbarer Wert ohne globalen Zustand; `FeatureType` (`validate`, `execute`, `describe`, `handles`) mit Registry;
`GeometryKernel` in der GAL (ADR-0008); Neuberechnung mit Ergebnis-Cache, Worker und eigenem `CancelToken`; `DocumentModel` (Qt).
Core-Abdeckung ≥ 94 % (`scripts/coverage`).

**Festlegungen:**
- `describe()` und Statusmeldungen liefern Schlüssel und Werte, nie fertigen Text (Regel 4).
- Ein scheiterndes Feature behält sein letztes gültiges Ergebnis (als veraltet markiert).
- Lage einer Form = Mitte der Grundfläche (x, y) und Unterkante (z). Funktionsargumente trennt `;` oder „Komma + Leerzeichen“.

**Offen:** Maße-Tabelle in der Oberfläche; „Neu“ und mehrere Tabs (AP-1.11); Abbruch einzelner Kernel-Operationen mit AP-1.7 neu messen.

---

### AP-1.2 Commands und Undo/Redo
**Status:** erledigt (2026-10-15, PR #19) · **Größe:** M · **Abhängigkeiten:** AP-1.1

**Ergebnis:** `Command`, `History` (besitzt das Dokument, die UI bekommt nur `const&`), Transaktionen, Befehle als Fabriken in
`commands.h`. `DocumentModel::beginChange/commitChange/cancelChange` für Zieh-Vorgänge. Liste **„Letzte Aktionen“** (Bearbeiten-Menü).

**Festlegungen:**
- Der Name „Letzte Aktionen“ ist vom Product Owner gewählt; „Verlauf“ meint das Rezept.
- Neue Bedienelemente erweitern `DocumentModel` und nutzen `History::execute`, nie das Dokument direkt.
- Auswahl, Kamera, Panels sind keine Undo-Schritte. Auge an/aus ist einer.

---

### AP-1.3 Viewport und Navigation
**Status:** erledigt auf dem Mac (2026-10-16, PR #20) · **Größe:** L · **Abhängigkeiten:** AP-0.4, AP-1.1

**Ergebnis:** Maus-Stile (Fission3D, Fusion, Tinkercad, Blender, SketchUp; `render/navigation.h`), Trackpad und Pinch, Ansichtswürfel mit
Kanten/Ecken, eigene GPU-Puffer je Körper, adaptive Feinheit (5 Stufen, `render/detail.h`, Loch Ø 3 mm: 0,03 px Abweichung), SSAO
„Weiche Schatten“ mit Selbst-Abschaltung, Druckerliste (`ui/printer_catalog.h`) für die Bettgröße, `--benchmark` (M1: 2 Mio. Dreiecke
8,0 ms je Bild).

**Offen:**
- Kriterium 60 fps auf Linux-iGPU nicht gemessen; SSAO nur auf Metal angesehen.
- Bettmaße der Druckerliste aus dem Gedächtnis, vor Veröffentlichung gegen Datenblätter prüfen.
- Maus-Stil und Drucker werden nicht gemerkt (AP-1.11).

---

### AP-1.4 Auswahl und Hover
**Status:** erledigt (2026-10-16, PR #21) · **Größe:** M · **Abhängigkeiten:** AP-1.3

**Ergebnis:** ID-Bild-Picking (15×15 um den Zeiger, `SceneRenderer`), `core::ElementNames`/`core::Selection`, `SelectionController`;
Klick, ⇧-Klick, Rahmen, Filter „Wählen: Alles | Körper | Flächen | Kanten | Ecken“; Auswahl bleibt über Neuberechnungen
(ADR-0009). Entwickler-Option `--simulate "hover:…;click:…;frame:…"` mit `--smoke-test`.

**Festlegungen:**
- **Doppelklick wählt den Körper.** „Senkrecht draufschauen“ ist Ansicht ▸ Auf Fläche schauen (**L**) und Smart Zoom.
- Elemente heißen nach ihren Flächen (Kante = zwei Flächen, Ecke = Flächen am Punkt); was nicht sicher wiedergefunden wird, wird
  losgelassen (Regel 6).

**Offen:** Hover-Leuchten kommt 1–2 Bilder nach der Bewegung (Arbeit 2,0 ms); falls zu träge: CPU-BVH für Hover. Rechtsklick-Menü fehlt.
Namensweitergabe für umbauende Features mit AP-2.10. Höchstens 1023 Körper wählbar.

---

### AP-1.5 Grundformen
**Status:** erledigt (2026-10-17, PR #22–#26) · **Größe:** M · **Abhängigkeiten:** AP-1.1, AP-1.4

**Ergebnis:** Quader (mit Eckenradius), Zylinder, Kugel, Halbkugel, Kegel(stumpf), Torus, Röhre, Prisma, Pyramide, Keil, Stern, Text
(IBM Plex mitgeliefert + Systemschriften, fett/kursiv). Galerie mit echten Vorschaubildern, Ablegen per Ziehen oder Klick, „Als Loch“
als Geisterform. Maß-Griffe „mit Tiefe“ (vom Renderer gezeichnet, `render::HandleGraphics`; QML nur Beschriftungen und Anfassflächen),
Wertblase, Tippen ersetzt Ziehen, `Tab`, Esc. Ablauf D Schritt 1 automatisch grün.

**Festlegungen:**
- Griff-Art nur „Achsen“; „Klassisch“ mit den Einstellungen (AP-1.11).
- Umlaute/ß gehen, wenn die Schrift sie hat; nur fehlende Zeichen werden abgelehnt. Nie stille Ersatzschrift oder Ersatzschnitt.
- Formen stehen mit der Unterseite auf z, zentriert in (x, y). Eckenradius ≤ halbe kürzere Seite, sonst Fehler am Schritt.
- Keil: vorne (−Y) Höhe 0. Prisma und Stern: 3–64 Ecken. Flächen-Namen `top`, `bottom`, `side`, `side1…`, `cornerFrontLeft` …
- Ziffern 1–6 sind Ansichten; Tippen von Maßen startet nur nach dem Ablegen oder während eines Zugs.

**Offen:** Ausschneiden (AP-1.7); Schrift in der Datei (AP-1.10); Seiten/Zacken ohne Griff.

---

### AP-1.6 Transformieren: Gizmo, Fangen, Abstandsmaße
**Status:** erledigt (2026-10-21, vom Product Owner geprüft) · **Größe:** L · **Abhängigkeiten:** AP-1.4
**Bauanleitungen:** [1.6c2](anleitungen/AP-1.6c2.md) · [1.6d](anleitungen/AP-1.6d.md) · [1.6e](anleitungen/AP-1.6e.md) · Pflege und Folgearbeiten: [pflege.md](anleitungen/pflege.md)

**Umfang:**
- Gizmo: Verschieben (Achsen und Ebenen), Drehen (Ringe), **proportional** Vergrößern/Verkleinern (Eck-Griff oder Prozentangabe; der Eck-Griff ist immer gleichmäßig)
- **Kein ungleichmäßiges Skalieren über das Gizmo.** Breiter/höher machen geht über die Maß-Griffe der Grundform (AP-1.5) bzw. ab Phase 2 über Strecken (AP-2.4). Echtes Verzerren nur über Ändern ▸ Mehr ▸ „Verzerren“, mit Warnhinweis ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern))
- Fangen: adaptives Raster, Ecken, Kantenmitten, Kreismittelpunkte, Flächenmitten, 15°-Winkel; ⇧ = aus
- **Intelligente Abstandsmaße** zu den nächsten Kanten anderer Körper und zum Bett, anklickbar und editierbar
- Ausrichten-Werkzeug (mehrere Körper bündig oder mittig)
- „Aufs Bett legen“ (Körper fällt auf z = 0), „Auf Fläche legen“
- Kopieren/Einfügen/Duplizieren (⌘D) mit Versatz

**Akzeptanzkriterien:**
- [x] Eine Form lässt sich per Abstandsmaß exakt „5 mm vom Rand“ platzieren (Test `app_distance_five_from_edge`, Review 2026-10-20)
- [x] Jede Transformation steht als Rezept-Eintrag im Verlauf und ist editierbar (Verschieben, Drehen, Vergrößern, Fangen, Abstandsmaß, Ausrichten, Aufs Bett/Auf Fläche legen, Duplizieren: je ein Eintrag in „Letzte Aktionen“; die Lage steht als Werte im Panel des Schritts, ADR-0010; Review 2026-10-20)

#### Zerlegung (2026-10-18, vom Product Owner bestätigt)
Je Teilpaket ein Pull Request mit grüner CI. Keine neuen Abhängigkeiten. Gizmo, Ringe, Fang-Marken und Abstandsmaße zeichnet die
3D-Ansicht tiefenrichtig (`render::HandleGraphics`); in QML liegen nur Beschriftungen und Anfassflächen.

| Teil | Status | Inhalt |
|---|---|---|
| **a) Verschieben** | erledigt (PR #27, #28) | ADR-0010; Werkzeuge der Palette wirken; Gizmo mit Pfeilen, Ebenen-Quadraten, Mittelpunkt; Körper selbst ziehen verschiebt ihn; Pfeiltasten (⇧ = 10 mm), Bild↑/↓ |
| **b1) Drehen** | erledigt (PR #29) | `rotX/rotY/rotZ` an jeder Form, `GeometryKernel::rotate`, Werkzeug „Drehen“ mit Ringen, 15°-Schritte, Tippen in Grad |
| **b2) Gleichmäßig vergrößern** | erledigt (PR #31) | Eck-Quadrate im Werkzeug „Größe“, Prozent an der Wertblase, alle Längen im gleichen Verhältnis (`core::scaledExpression`), `GeometryKernel::exactBounds` |
| **b3) Ablegen an Wänden** | erledigt (PR #33) | Unterseite auf jede Fläche (`geo/seat.h`), ⌥ = aufrecht |
| **c1) Fangen beim Verschieben** | erledigt (PR #34) | `interaction/snap.h`, `ui::SnapSession` im `GizmoController`; Fang-Marken und Ausrichtungslinie; ⇧ aus; Raster auf dem Bett (Ansicht ▸ „Raster beim Ziehen“); Genauigkeit in der Statuszeile (Auto, Schieberegler 0,01–10 mm, Zahlenfeld) |
| **c2) Fangen bei Maß-Griffen und Ablegen** | erledigt (2026-10-21) | dieselben Punkte und Marken für die Maß-Griffe (`HandleController`), den Eck-Griff (`ScaleController`) und das Ablegen (`PlacementController`); gemeinsamer ⇧-Schalter `ui::snappingSuppressed()`; Marken des Ablegens in `HandleSource::placement` |
| **d) Abstandsmaße** | erledigt (2026-10-21) | `interaction/distances.h` (Rechnung: nächstes Hindernis je Richtung, Bettrand, „Abstand setzen“); `ui::DistanceController` + `DistanceLayer.qml`: Bänder im Werkzeug „Verschieben“ bei gewähltem Körper, Zahl anklicken und tippen = ein Verschieben (Undo-Schritt „Körper verschoben“); `--simulate "distance:left,5"`; Akzeptanzkriterium 1 als Test `app_distance_five_from_edge` |
| **e) Rest** | erledigt (2026-10-21) | Ausrichten (`interaction/align.h`, `ui::AlignController`, 9 Punkte am Rahmen mehrerer gewählter Körper, Vorschau-Ebene); Aufs Bett legen (Taste **D**, Menü Körper); Auf Fläche legen für vorhandene Körper (Menü Körper, Klick auf die Fläche, `PlacementController::seatChosen`); Kopieren, Einfügen, Duplizieren (⌘C/⌘V/⌘D, Versatz 10 mm in X und Y je Einfügen); alles über `DocumentModel::moveBodiesBy` bzw. `pasteBodies`, ein Undo-Schritt je Aktion |

#### Festlegungen (Product Owner)
- **Lage im Rezept** ([ADR-0010](../adr/0010-lage-im-rezept.md)): Verschieben, Drehen und Vergrößern ändern die Werte des Schritts, der
  den Körper macht; ein eigener Schritt „Verschoben“ nur, wo ein Schritt mehrere Körper speist. Ein Zug = ein Eintrag in „Letzte
  Aktionen“ („Quader verschoben/gedreht/auf 150 % vergrößert“), kein neuer Rezept-Eintrag. Unberührte Werte (auch Formeln) bleiben.
- **Griffe nach Werkzeug:** Verschieben (Standard) = Gizmo und Maßbänder ohne Punkte; Größe = Punkte und Eck-Quadrate; Drehen = Ringe;
  Wählen = keine. Den Körper selbst ziehen verschiebt ihn in jedem Werkzeug außer Wählen; der Rahmen entsteht dann nur auf leerem Grund.
- **Drehen:** drei Winkel, erst X, dann Y, dann Z, um die festen Bett-Achsen und die Mitte der Hülle der ungedrehten Form; Ringe
  stehen in Bett-Richtung. Nach dem Drehen bleibt die Mitte (Körper kann schweben oder im Bett stecken; „Aufs Bett legen“ kommt mit e).
- **Gleichmäßig vergrößern:** alle Längen wachsen (auch Rundung, Wand, Schriftgröße), Anzahlen und Winkel nicht. Zahlen × Faktor,
  Formeln bleiben als `(Formel) * Faktor`. Die gegenüberliegende untere Ecke des Kastens in Bett-Richtung bleibt stehen (wie Tinkercad).
- **Ablegen an Wänden:** automatisch an jeder Fläche, ⌥ = aufrecht, Arbeitsebene bleibt ein Höhenwert. Ergebnis sind gewöhnliche Zahlen
  (Lage, Winkel in 0,1°), keine Bindung an die Fläche.
- **Abstandsmaße (d):** Kante zu Kante (Hüllquader, gedrehte Körper zählen mit ihrem Hüllquader), nur in Bett-Richtung (±X, ±Y), je Richtung das nächste
  Hindernis mit überlappendem Grundriss, sonst der Bettrand (steht der Körper über dem Rand, kein Band); Löcher zählen nicht. Sichtbar im Werkzeug
  „Verschieben“ bei gewähltem Körper, auch ohne Ziehen. Ein Maß verschiebt nur und hält den Abstand nicht fest (kein Bezug).
- **Rest (e):** Alle Aktionen ändern nur die Lage-Werte (x, y, z, Winkel) der Schritte, Formeln in unberührten Werten bleiben; Schritte, die mehrere
  Körper erzeugen oder Eingänge haben (ab AP-1.7), werden übersprungen bzw. sind nicht kopierbar. Kopie/Einfügen nur im Programm (System-Zwischenablage
  mit AP-1.10); jedes weitere Einfügen derselben Zwischenablage rückt um weitere 10 mm; Duplizieren ersetzt die Zwischenablage (Treppe).
  Ausrichten: Punkte am Rahmen aller gewählten Körper, bei Werkzeug „Verschieben“; Kopieren/Löschen/Ausschneiden: Ausschneiden erst mit AP-1.9.
- **Fangen:** Der gezogene Körper fängt mit Ecken, Kantenmitten und Flächenmitten seines achsparallelen Hüllquaders an Punkten anderer
  Körper und des Betts, Achse für Achse; Punkt-Fang hat Vorrang. Reichweite 10 px. Getippte Zahlen werden nie gefangen.
  - *Maß-Griffe (c2):* Die bewegte Seite des Hüllquaders (9 Punkte) fängt entlang einer Bett-Achse; nur bei achsparalleler Griffrichtung
    ohne Steigung (Kugel-Radius, gedrehte Körper, Details wie Eckenradius haben nur das Raster). ⇧ = Fangen und Raster aus.
  - *Eck-Griff (c2):* Fang vor ganzen Prozent; der Faktor ist dann krumm (137,4 %, Blase mit einer Nachkommastelle). ⇧ = nur Fangen aus,
    der Körper bleibt gleichmäßig und der Faktor rastet auf ganze Prozent (Regel 11).
  - *Ablegen (c2):* Die Unterseite (9 Punkte) fängt auf Bett und waagerechten Oberseiten; an Wänden und Schrägen nur das Raster entlang
    der Fläche. Beim ersten Platz kennt die Form ihre Größe noch nicht (nur Raster), ab der zweiten Bewegung fängt sie. ⇧ = frei.
- **Kopieren/Einfügen** nur innerhalb des Programms (System-Zwischenablage mit dem Dateiformat, AP-1.10).
- Abstandsmaß von Kante zu Kante; Gizmo-Achsen immer in Bett-Richtung; Zoomen per Tastatur gehört nicht zu AP-1.6.

#### Bekannte Einschränkungen (Stand nach c1)
- **Fangpunkte aus dem Anzeige-Netz** (auf 0,0001 mm bereinigt); exakte Kernel-Werte wären eine Folge-Arbeit. Runde Flächen haben
  keinen Fangpunkt.
- Raster nur auf dem Bett gezeigt. „Raster beim Ziehen“, Fang-Genauigkeit, Maus-Stil werden nicht gemerkt (AP-1.11).
- Die 15°-Schritte beim Drehen rasten relativ zum Startwert; der Eck-Griff rastet auf ganze Prozent.
- **Gedrehte Körper:** Maß-Punkte und Bänder sind nicht mitgedreht gezeichnet (Folge-Arbeit seit b1).
- **Maße tippen direkt nach dem Ablegen an einer Wand** schiebt die Form von der Wand (Mitführen der Lage: eigener Mechanismus, Idee für e).
- Gewölbte Flächen: Richtung aus dem Dreieck unter dem Zeiger, Ablegen kann leicht ruckeln. ⌥ unter Linux noch nicht von Hand geprüft.
- Bild↑/Bild↓ fehlen auf Mac-Laptops (Fn+↑/↓).

---

### AP-1.7 Kombinieren
**Status:** erledigt (2026-10-25, vom Product Owner geprüft) · **Größe:** M · **Abhängigkeiten:** AP-1.5
**Bauanleitung:** [anleitungen/AP-1.7.md](anleitungen/AP-1.7.md)

**Umfang:**
- Vereinen, Ausschneiden, Überlappung behalten, Teilen (an Ebene oder Fläche)
- **Löcher ohne Gruppieren** gemäß [UX §7.1](../02-ux-konzept.md#71-ausschneiden-ist-umständlich):
  - „Als Loch“-Formen schneiden **live** aus, sobald sie einen Körper berühren
  - Ziel = Körper, auf dem das Loch abgelegt wurde; Etikett „schneidet aus: …“ am Loch, per Klick änderbar (auch mehrere Ziele)
  - Das Loch bleibt als Geisterform auswählbar, verschiebbar und in der Größe änderbar
  - Tiefe „durchgehend“ als Standard (bleibt durchgehend, wenn das Ziel dicker wird)
- Option „Werkzeug-Körper behalten“ beim Werkzeug Ausschneiden
- Fehlerfälle (nicht überlappend, koplanar, ungültig) mit Meldungen aus dem Katalog

**Akzeptanzkriterien:**
- [x] Alle Kombinationen aus dem Kernel-Spike (AP-0.5) liefern gültige Körper oder eine verständliche Meldung
- [x] Ändert man ein Loch nachträglich (Größe, Position), wird es korrekt neu ausgeschnitten
- [x] Ablauf D, Schritte 2–4, läuft automatisiert grün; es gibt keinen Schritt „Gruppieren“

**Abschlussvermerk 2026-10-21 (wartet auf Test durch den Product Owner):**
- Ergebnis: Vereinen, Ausschneiden, Überlappung behalten (Menü Körper und Palette „Kombinieren“), Teilen an einer Ebene (X, Y, Z; Pfeil-Griff
  im Werkzeug Größe), Live-Löcher ohne Gruppieren mit Etikett „Schneidet aus: …“ (Ziele per Klick änderbar), „Geht ganz durch“ (Standard),
  Ausschneider optional als Körper behalten. Kernel-Booleans exakt mit Flächennamen; die Kombinationen des Kernel-Spikes laufen über die GAL.
- Festlegungen: [ADR-0011](../adr/0011-kombinieren-und-loecher.md) samt Ergänzung (ein Schritt „combine“, Löcher pflegt `reconcileCuts`, kombinierte
  Körper wandern samt Löchern, jeder Teil eines Teilens bekommt einen Schritt „place“, Körpername bleibt, Symbol dahinter).
- Nachtrag 2026-10-21: halbtransparente Schnittebene, Drehen und Größe ändern kombinierter Körper (alle Teile gemeinsam), Löcher schneiden
  schon beim Ziehen live (alle 150 ms im selben Schritt), „Bündig machen“ (Fuzzy 0,001 mm) als Schalter am Schritt.
- Offene Reste: Teilen nur an Achsenebenen (an einer Fläche: Phase 2, AP-2.12); Teile eines geteilten Körpers lassen sich noch nicht drehen oder
  vergrößern; die Prüfung der Löcher rechnet im Oberflächen-Thread (kalt 26 ms, warm 4 ms, kleine Modelle); Löschen eines Lochs mit AP-1.9; Prüfung im Hintergrund und Drehen/Vergrößern von Teilen siehe [pflege.md](anleitungen/pflege.md) P13, P14; Teilen an einer Fläche mit AP-2.12.

---

### AP-1.8 Eigenschaften-Panel und Werteingabe
**Status:** erledigt (2026-10-25, vom Product Owner geprüft, PR #44, #47) · **Größe:** M · **Abhängigkeiten:** AP-1.1
**Bauanleitung:** [anleitungen/AP-1.8.md](anleitungen/AP-1.8.md)

**Umfang:**
- Kontextabhängiges Panel: Was ist ausgewählt, welche Werte sind änderbar; „▸ Mehr“ für Seltenes
- Zahlenfeld-Komponente: Ausdrücke, Einheiten, Komma/Punkt, Pfeil hoch/runter (±1, mit ⇧ ±10, mit ⌥ ±0,1), Ziehen am Label ändert den Wert, Fehleranzeige inline
- Farbwahl je Körper (Palette + frei)
- Kontextleiste an der Auswahl in der 3D-Ansicht (die 4–6 wahrscheinlichsten Aktionen)

**Akzeptanzkriterien:**
- [x] Zahlenfeld-Komponente ist einheitlich überall verwendet (keine Sonderfelder) – Panel: `NumberField.qml`; 3D-Ansicht: alle sechs Wertblasen/Felder über `ValueInput.qml` (2026-10-27, Pflege T3)
- [x] Ungültige Eingabe wird erklärt („‚Breitee‘ kenne ich nicht. Meintest du ‚Breite‘?“) – Panel und Blasen teilen `ValueExplanation.qml`; Nachweis Blase: Fenster-Test `app_bubble_wrong_input` (2026-10-27)

**Abschlussvermerk (2026-10-25, Teile b–d):**
- Panel nach Auswahl (Körper, mehrere, Fläche/Kante), umbenennbarer Körpername, „Mehr“ für Seltenes (`ParamSpec::advanced`).
- Farbe: 10 Filamentfarben (`Theme.filaments`) plus „Eigene Farbe…“ als Popover; mehrere Körper = ein Undo-Schritt, keine Neuberechnung.
- Kontextleiste neben der Auswahl: Körper (Als Loch/Teil · Farbe · Duplizieren · Aufs Bett), mehrere (Vereinen · Ausschneiden · Farbe · Aufs Bett),
  Fläche (Auf Fläche schauen). Nicht gebaute Aktionen fehlen.
- Offene Reste: „Mehr ▾“ der Kontextleiste (noch nichts Passendes), Kante ohne Aktionen (Abrunden ab AP-2.5), Ausrichten bei mehreren Körpern,
  Farbe je Fläche (AP-3.4).

---

### AP-1.9 Rezept-Panel (Version 1)
**Status:** erledigt (2026-10-26, vom Product Owner geprüft, PR #49) · **Größe:** M · **Abhängigkeiten:** AP-1.2, AP-1.8
**Bauanleitung:** [anleitungen/AP-1.9.md](anleitungen/AP-1.9.md)

**Umfang:**
- Liste der Features als Sätze (aus `describe()`), Symbol, Status-Badge
- Hover hebt Geometrie hervor; Klick öffnet die Bearbeitung mit Griffen
- Ausblenden, Löschen (mit Hinweis auf abhängige Schritte), Umbenennen
- Zeitmarker „jetzt“ verschiebbar (Rollback); neue Features werden an der Markerposition eingefügt
- Körper-Liste darunter: Sichtbarkeit, Farbe, Name, Typ-Badge

**Akzeptanzkriterien:**
- [x] Ein Schritt in der Mitte des Rezepts lässt sich ändern, alles danach wird korrekt neu berechnet (Kerntest „a change recomputes only what it affects“; Zeitmarker-Tests)
- [x] Löschen eines Schritts mit Abhängigen fragt mit klarer Erklärung nach (Karte im Panel, Tests `deletionOfStep`/`deleteSteps`, Bild im PR)

**Abschlussvermerk (2026-10-25, Teile a–d):**
- Rezeptzeilen mit Position (①), Symbol je Art, Satz, Name, Badge (ausgeblendet / braucht Hilfe / geht nicht, nie nur Farbe); Klick auf das Badge erklärt inline. Überfahren hebt den Körper hervor (`Viewport3D::lightUpBodies`).
- Umbenennen (Doppelklick, F2, Menü), Ausblenden, Löschen; Rechtsklick-Menü `RowMenu`. Löschen mit Abhängigen oder mit mehr als einem Schritt fragt in einer Karte im Panel (Trotzdem löschen / Nur ausblenden / Abbrechen). Entf/⌫ und ⌘X wirken auf die gewählten Körper; ein Ergebnis aus Kombinieren nimmt die Formen mit, die nur dafür da sind (`deletionOfBodies`); ein Loch nimmt seinen Schnitt im selben Undo-Schritt mit.
- Zeitmarker: `Document::rollback()` (Anzahl zählender Schritte), `makeSetRollback`; Schritte dahinter werden nicht berechnet (Status „pending“, ihre Körper fehlen). Neue Schritte landen am Marker, der Marker rückt dahinter; Hinzufügen und Entfernen halten ihn an seiner Stelle (Zufallstest in `test_history.cpp`). Ziehen mit Einrasten zwischen Zeilen, Doppelklick = zurück ans Ende.
- Körperliste: Doppelklick benennt um, Klick auf den Farbpunkt öffnet die Farben, Badge „Loch“, Überfahren hebt hervor.
- Festlegungen: Haltepunkte wie empfohlen (Zeitmarker kommt in die Datei, Karte statt Dialog, kombinierte Körper nehmen ihre Formen mit); im Rezept zählt die Position, nicht die Feature-Nummer. `RecipeSentences.js` wurde nicht angelegt: die Sätze bleiben in `Main.qml` (`stepText`), unbekannte Arten zeigen „Schritt N (unbekannte Art)“.
- Offene Reste: Zeitmarker und Projektpalette werden erst mit AP-1.10 gespeichert (siehe dort); Umsortieren per Ziehen = P9; die Fenster-Tests `app_ring_turn` und `app_snap_corner_handle` fielen einmal unter Last durch (einzeln grün). Der wacklige `app_flow_d_hole` war ein Wettlauf im Testrahmen (Prüfung vor dem Loslassen des Lochs), behoben in PR #51.

---

### AP-1.10 Dateien: Speichern, Laden, Autosave, Import und Export
**Status:** erledigt (2026-10-27, gemergt mit PR #53–#58; Slicer-Prüfung des 3MF durch den Product Owner offen) · **Größe:** M · **Abhängigkeiten:** AP-1.1
**Bauanleitung:** [anleitungen/AP-1.10.md](anleitungen/AP-1.10.md)

**Umfang:**
- `.fission`-Format gemäß [Architektur §7](../03-architektur.md#7-dateiformat-fission) inkl. Vorschaubild
- Atomisches Speichern, Autosave, Wiederherstellung nach Absturz („Wir haben deine Arbeit von 14:32 gerettet.“)
- Zuletzt geöffnete Dateien; Dateizuordnung auf macOS und Linux
- Export: STL (binär), 3MF (Körper als Objekte, mit Farben), Tessellierungsqualität wählbar
- ~~Import: STL und 3MF als freie Körper (Netz)~~ → verschoben nach AP-4.1 (Entscheidung 2026-10-25: freie Körper gibt es erst dort)

**Akzeptanzkriterien:**
- [x] Roundtrip-Tests: Speichern → Laden ergibt ein identisches Dokument und identische Geometrie
- [x] Ein simulierter Absturz während des Speicherns hinterlässt nie eine kaputte Datei
- [ ] Exportiertes 3MF öffnet sich korrekt in Bambu Studio, OrcaSlicer und PrusaSlicer (manuell geprüft, Screenshots im Abschlussvermerk) (offen: der Product Owner prüft das mit einem echten Slicer; die Tests lesen die Datei mit lib3mf wieder ein)

**Abschlussvermerk (2026-10-26, Teile a–e):**
- `.fission` = ZIP mit `manifest.json` (Version, Zeiten, SHA-256), `document.json` (Quelle der Wahrheit, `formatVersion` 1) und `preview.png`. Alle Inhalte des Dokuments inklusive Zeitmarker und Projektpalette (`settings.palette`) werden gespeichert; Ausdrücke bleiben Text. Unbekannte Felder und neuere Versionen werden gemeldet, nie ein Absturz; Migrationstabelle ist getestet (erfundene Version 0). Alt-Dateien `tests/fixtures/files/v1/` sind ab jetzt unveränderlich.
- Speichern atomar (Temp-Datei, Flush, Umbenennen) und im Hintergrund; „gespeichert“ nur, wenn sich währenddessen nichts geändert hat. Fenstertitel „Name.fission •“, Zuletzt geöffnet (flache Liste im Datei-Menü, 10), Öffnen per Argument und macOS-Doppelklick.
- Autosave alle 60 s und nach 20 Schritten in `AppDataLocation/recovery` (höchstens 10 Dateien); Karte „Wir haben deine Arbeit von … gerettet“ nach einem Absturz. Projekt mit Datei wird beim Beenden/Neu/Öffnen automatisch gespeichert; ein nie gespeichertes Projekt bleibt als Sicherung liegen und wird beim nächsten Start angeboten.
- Export STL (binär) und 3MF (ein Objekt je Körper, Name, Farbe), Qualität Grob/Normal/Fein, im Hintergrund und abbrechbar; keine Löcher, keine verbrauchten oder versteckten Körper, nichts bei Körpern mit Rezept-Fehler. Lochkante ≤ 0,01 mm (Ablauf D Schritt 6, `test_export.cpp`).
- System-Zwischenablage mit dem Dateiformat (aus AP-1.6): kopierte Körper liegen als `application/x-fission3d` bereit.
- Festlegungen: Schriften werden nicht eingebettet (nur der Name); eine Datei aus einer neueren Version wird geöffnet, aber nur unter neuem Namen gespeichert; Palettenänderungen markieren „geändert“, haben aber kein Undo.
- Offene Reste: 3MF in Bambu Studio / OrcaSlicer / PrusaSlicer von Hand öffnen (Product Owner); „Zuletzt geöffnet“ als Untermenü statt flacher Liste; Dateizuordnung unter Linux/macOS ist aus AP-0.7 vorhanden, aber nicht von Hand geprüft; Kernel-Abdeckung ≥ 85 % nicht neu gemessen; mehrere Dokumente (Tabs) = AP-1.11.

---

### AP-1.11 App-Shell, Einstellungen, Sprachen
**Status:** erledigt (2026-10-27, gemergt mit PR #59–#64) · **Größe:** M · **Abhängigkeiten:** AP-0.8
**Bauanleitung:** [anleitungen/AP-1.11.md](anleitungen/AP-1.11.md)

**Umfang:**
- Native Menüs (macOS-Menüleiste, Linux-Fenstermenü), Tastenkürzel-Konzept (plattformgerecht)
- Erststart-Assistent: Sprache, Drucker (Profile: Bambu A1/A1 mini/P1S/X1C, Prusa MK4/Core One/Mini, Creality Ender-3 V3/K1, Voron, eigenes), Maus-Stil
- Einstellungen: Einheit, Nachkommastellen, Maus-Stil, Thema (hell/dunkel/System), UI-Skalierung, Autosave
- i18n-Infrastruktur (Qt Linguist), Deutsch und Englisch vollständig
- Startbildschirm (Version 1): Neu, Zuletzt geöffnet, Beispiele

**Akzeptanzkriterien:**
- [x] Sprachwechsel ohne Neustart (Ende-zu-Ende-Test `expecttr`, Einstellungen-Seite)
- [x] Keine hartkodierten UI-Texte (Prüfskript `translations_complete` in der CI)

**Stand nach AP-0.8 (2026-10-14):**
- *Schon da:* Fenster nach Mockup, Menüs (Mac: Systemmenüleiste, Linux: im Fenster), Deutsch und Englisch mit Umschalten zur Laufzeit (`--language`), `qsTr` mit Übersetzungstest in der CI, Hell/Dunkel und Akzentfarbe (Theme), Über-Fenster, Protokolldatei.
- *Offen:* Erststart-Assistent, Einstellungen (Einheit, Nachkommastellen, Maus-Stil, UI-Skalierung, Autosave), Druckerprofile, Startbildschirm, Tastenkürzel-Konzept, **„Neu“ und mehrere Dokument-Tabs verdrahten** (aus AP-1.1 hierher verschoben; das Dokumentmodell kann mehrere Dokumente).

**Abschlussvermerk (2026-10-27, Teile a–f; autonom gearbeitet, Haltepunkte nach Empfehlung):**
- Einstellungen (`services::Settings`, `ui::Preferences`): Sprache, Design, Akzent (mit Hinweis bei Rot/Gelb/Grün), Oberflächen-Skalierung 80–200 % ohne Neustart, Maus-Stil, Griff-Art, Drucker, Raster, weiche Schatten, Einheit für neue Projekte, Nachkommastellen (Obergrenze der Anzeige), Autosave (ein/aus, Abstand). Seite ⌘, im Fenster, kein Dialog. Kommandozeile gewinnt, ohne gespeichert zu werden.
- Erststart-Assistent (Sprache → Drucker → Maus) als Karte, „Später“ jederzeit; endet mit „Los geht's“ (Mission erst mit AP-5.1).
- Startseite (`StartPage.qml`): Neu, Zuletzt geöffnet (mit Vorschaubild), 3 Beispiele (Resource, öffnen als neues Projekt).
- Mehrere Dokumente: Tabs mit je eigenem Dokument, Undo-Liste, Datei, Palette und Sicherungsdatei; `DocumentModel` bleibt das eine Objekt und tauscht das Dokument (`exchange`). Ein unberührtes Projekt wird von Neu/Öffnen ersetzt.
- Griff-Art „Klassisch“ (erste Stufe): weiße Quadrate, Maßband und -kästchen erst beim Zeigen; dieselben Werte und Commands (Test `app_handle_drag_classic`).
- Festlegungen: Haltepunkt 1 ja (Tabs, schlicht), 2 nur erste Stufe von Klassisch, 3 Knopf „Mission“ erst mit AP-5.1.
- Offene Reste: Klassisch: Eck-Griffe, die Breite und Tiefe zugleich ändern, und der schwarze Hebe-Pfeil (Lage z) fehlen; Kamera je Dokument merken; Einheit „Zoll“ gilt nur für neue Projekte (Anzeige der Statuszeile bleibt „mm“); Bildschirmfotos 80/100/150/200 % nur von mir angesehen, nicht in den PRs; Tastenkürzel-Konzept (Ctrl+Tab zum Wechseln der Tabs) fehlt.

---

### AP-1.12 Hinweis-System und Befehlssuche
**Status:** erledigt (2026-10-27, gemergt mit PR #65) · **Größe:** M · **Abhängigkeiten:** AP-1.11
**Bauanleitung:** [anleitungen/AP-1.12.md](anleitungen/AP-1.12.md)

**Umfang:**
- Hinweiszeile: Jedes Werkzeug und jeder Zustand liefert einen Satz („Klicke auf eine Fläche …“)
- Tooltip-Karten (Titel, Satz, Tastenkürzel, Platz für Animation/GIF/Video-Loop)
- Befehlssuche ⌘K mit unscharfer Suche und Synonym-Liste je Werkzeug (DE und EN)
- Meldungskatalog-Grundgerüst (Schlüssel → Text mit Platzhaltern → Aktionen); wird in AP-2.14 ausgebaut

**Akzeptanzkriterien:**
- [x] „runde ecken“, „bohren“, „hohl“ und „namen“ finden das richtige Werkzeug (Unit-Test der Bewertung und Ende-zu-Ende-Test über den echten Katalog; Abrunden, Bohrung und Aushöhlen stehen als „kommt bald“ im Katalog)
- [x] Jedes Werkzeug aus Phase 1 hat Hinweis, Tooltip-Karte und Synonyme (Test `commands_complete`, `scripts/tools/check_commands.py`)

**Abschlussvermerk (2026-10-27, Teile a–d; autonom, Haltepunkte nach Empfehlung):**
- Befehlskatalog `CommandCatalog.qml` (52 Einträge: Werkzeuge, Formen, Menübefehle, „kommt bald“) führt die Wörter von `ToolCatalog`/`ShapeCatalog` zusammen; Haltepunkt 2 abweichend: QML statt JS (lupdate liest `qsTr` aus beiden, QML ist einfacher einzubinden).
- Befehlssuche (⌘K, Suchfeld): eigene Bewertung in C++ (`command_search.cpp`: ganzes Wort > Wortanfang > enthalten > ein Tippfehler, Umlaute und ß normalisiert, deutsche und englische Wörter zugleich); Ergebnisliste mit Kürzel und Grund bei nicht möglichen Befehlen; ↑/↓, Enter, Esc.
- Hinweiszeile: ein Ort (`currentHint` in `Main.qml`) mit fester Rangfolge; im leeren Projekt sagt sie, wie man anfängt. `TooltipCard.qml` (Titel, Satz, Kürzel, Platz für Animation, 0,8 s) für Werkzeuge und Formen.
- Meldungskatalog `MessageCatalog.qml`: alle 15 `FeatureIssueCode`s, Text je Code und Lösungsaktionen „Wert ändern“ und „Diesen Schritt ausblenden“ als Knöpfe unter der Erklärung am Rezept-Schritt; Test `messages_complete`.
- Offene Reste: Tooltip-Karten für Menübefehle der Kontextleiste (nur Werkzeuge und Formen); die Fehlerkarte an der Stelle im 3D-Bild (nur inline am Rezept-Schritt); „Wert ändern“ wählt den Schritt, fokussiert aber das Feld nicht; Kürzel in Tooltip der Werkzeuge leer, solange sie keine haben.

---

### AP-1.13 Nutzertest 1
**Status:** offen (Vorbereitung fertig: [Leitfaden](../nutzertests/2026-xx-phase1-leitfaden.md) und [Auswertungsvorlage](../nutzertests/2026-xx-phase1.md); die fünf Sitzungen macht der Product Owner) · **Größe:** S · **Abhängigkeiten:** alle APs der Phase 1
**Bauanleitung:** [anleitungen/AP-1.13.md](anleitungen/AP-1.13.md)

**Umfang:**
- 5 Personen ohne CAD-Erfahrung (davon 2 Tinkercad-Nutzer): Aufgabe „Schlüsselanhänger mit deinem Namen“, Think-Aloud, Bildschirmaufnahme, SUS-Fragebogen
- Auswertung: Stolperstellen nach Schwere sortiert → neue Tickets oder AP-Ergänzungen

**Akzeptanzkriterien:**
- [ ] Bericht `docs/nutzertests/2026-xx-phase1.md`
- [ ] ≥ 4 von 5 schaffen die Aufgabe in ≤ 10 min; sonst werden die Top-3-Probleme vor Phase 2 behoben
