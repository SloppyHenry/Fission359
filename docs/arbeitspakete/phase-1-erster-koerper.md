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
**Status:** in Arbeit (1.6a, 1.6b, 1.6c1 erledigt, 1.6c2 und 1.6d im Review) · **Größe:** L · **Abhängigkeiten:** AP-1.4
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
- [ ] Jede Transformation steht als Rezept-Eintrag im Verlauf und ist editierbar

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
| **c2) Fangen bei Maß-Griffen und Ablegen** | Review (2026-10-20) | dieselben Punkte und Marken für die Maß-Griffe (`HandleController`), den Eck-Griff (`ScaleController`) und das Ablegen (`PlacementController`); gemeinsamer ⇧-Schalter `ui::snappingSuppressed()`; Marken des Ablegens in `HandleSource::placement` |
| **d) Abstandsmaße** | Review (2026-10-20) | `interaction/distances.h` (Rechnung: nächstes Hindernis je Richtung, Bettrand, „Abstand setzen“); `ui::DistanceController` + `DistanceLayer.qml`: Bänder im Werkzeug „Verschieben“ bei gewähltem Körper, Zahl anklicken und tippen = ein Verschieben (Undo-Schritt „Körper verschoben“); `--simulate "distance:left,5"`; Akzeptanzkriterium 1 als Test `app_distance_five_from_edge` |
| **e) Rest** | offen | Ausrichten mehrerer Körper; „Aufs Bett legen“; „Auf Fläche legen“ für vorhandene Körper (nutzt `seatOnFace`); Kopieren, Einfügen, Duplizieren (⌘D) mit Versatz |

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
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.5
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
- [ ] Alle Kombinationen aus dem Kernel-Spike (AP-0.5) liefern gültige Körper oder eine verständliche Meldung
- [ ] Ändert man ein Loch nachträglich (Größe, Position), wird es korrekt neu ausgeschnitten
- [ ] Ablauf D, Schritte 2–4, läuft automatisiert grün; es gibt keinen Schritt „Gruppieren“

---

### AP-1.8 Eigenschaften-Panel und Werteingabe
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1
**Bauanleitung:** [anleitungen/AP-1.8.md](anleitungen/AP-1.8.md)

**Umfang:**
- Kontextabhängiges Panel: Was ist ausgewählt, welche Werte sind änderbar; „▸ Mehr“ für Seltenes
- Zahlenfeld-Komponente: Ausdrücke, Einheiten, Komma/Punkt, Pfeil hoch/runter (±1, mit ⇧ ±10, mit ⌥ ±0,1), Ziehen am Label ändert den Wert, Fehleranzeige inline
- Farbwahl je Körper (Palette + frei)
- Kontextleiste an der Auswahl in der 3D-Ansicht (die 4–6 wahrscheinlichsten Aktionen)

**Akzeptanzkriterien:**
- [ ] Zahlenfeld-Komponente ist einheitlich überall verwendet (keine Sonderfelder)
- [ ] Ungültige Eingabe wird erklärt („‚Breitee‘ kenne ich nicht. Meintest du ‚Breite‘?“)

---

### AP-1.9 Rezept-Panel (Version 1)
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.2, AP-1.8
**Bauanleitung:** [anleitungen/AP-1.9.md](anleitungen/AP-1.9.md)

**Umfang:**
- Liste der Features als Sätze (aus `describe()`), Symbol, Status-Badge
- Hover hebt Geometrie hervor; Klick öffnet die Bearbeitung mit Griffen
- Ausblenden, Löschen (mit Hinweis auf abhängige Schritte), Umbenennen
- Zeitmarker „jetzt“ verschiebbar (Rollback); neue Features werden an der Markerposition eingefügt
- Körper-Liste darunter: Sichtbarkeit, Farbe, Name, Typ-Badge

**Akzeptanzkriterien:**
- [ ] Ein Schritt in der Mitte des Rezepts lässt sich ändern, alles danach wird korrekt neu berechnet
- [ ] Löschen eines Schritts mit Abhängigen fragt mit klarer Erklärung nach

---

### AP-1.10 Dateien: Speichern, Laden, Autosave, Import und Export
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1
**Bauanleitung:** [anleitungen/AP-1.10.md](anleitungen/AP-1.10.md)

**Umfang:**
- `.fission`-Format gemäß [Architektur §7](../03-architektur.md#7-dateiformat-fission) inkl. Vorschaubild
- Atomisches Speichern, Autosave, Wiederherstellung nach Absturz („Wir haben deine Arbeit von 14:32 gerettet.“)
- Zuletzt geöffnete Dateien; Dateizuordnung auf macOS und Linux
- Export: STL (binär), 3MF (Körper als Objekte, mit Farben), Tessellierungsqualität wählbar
- Import: STL und 3MF als freie Körper (Netz)

**Akzeptanzkriterien:**
- [ ] Roundtrip-Tests: Speichern → Laden ergibt ein identisches Dokument und identische Geometrie
- [ ] Ein simulierter Absturz während des Speicherns hinterlässt nie eine kaputte Datei
- [ ] Exportiertes 3MF öffnet sich korrekt in Bambu Studio, OrcaSlicer und PrusaSlicer (manuell geprüft, Screenshots im Abschlussvermerk)

---

### AP-1.11 App-Shell, Einstellungen, Sprachen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.8
**Bauanleitung:** [anleitungen/AP-1.11.md](anleitungen/AP-1.11.md)

**Umfang:**
- Native Menüs (macOS-Menüleiste, Linux-Fenstermenü), Tastenkürzel-Konzept (plattformgerecht)
- Erststart-Assistent: Sprache, Drucker (Profile: Bambu A1/A1 mini/P1S/X1C, Prusa MK4/Core One/Mini, Creality Ender-3 V3/K1, Voron, eigenes), Maus-Stil
- Einstellungen: Einheit, Nachkommastellen, Maus-Stil, Thema (hell/dunkel/System), UI-Skalierung, Autosave
- i18n-Infrastruktur (Qt Linguist), Deutsch und Englisch vollständig
- Startbildschirm (Version 1): Neu, Zuletzt geöffnet, Beispiele

**Akzeptanzkriterien:**
- [ ] Sprachwechsel ohne Neustart
- [ ] Keine hartkodierten UI-Texte (Prüfskript in der CI)

**Stand nach AP-0.8 (2026-10-14):**
- *Schon da:* Fenster nach Mockup, Menüs (Mac: Systemmenüleiste, Linux: im Fenster), Deutsch und Englisch mit Umschalten zur Laufzeit (`--language`), `qsTr` mit Übersetzungstest in der CI, Hell/Dunkel und Akzentfarbe (Theme), Über-Fenster, Protokolldatei.
- *Offen:* Erststart-Assistent, Einstellungen (Einheit, Nachkommastellen, Maus-Stil, UI-Skalierung, Autosave), Druckerprofile, Startbildschirm, Tastenkürzel-Konzept, **„Neu“ und mehrere Dokument-Tabs verdrahten** (aus AP-1.1 hierher verschoben; das Dokumentmodell kann mehrere Dokumente).

---

### AP-1.12 Hinweis-System und Befehlssuche
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.11
**Bauanleitung:** [anleitungen/AP-1.12.md](anleitungen/AP-1.12.md)

**Umfang:**
- Hinweiszeile: Jedes Werkzeug und jeder Zustand liefert einen Satz („Klicke auf eine Fläche …“)
- Tooltip-Karten (Titel, Satz, Tastenkürzel, Platz für Animation/GIF/Video-Loop)
- Befehlssuche ⌘K mit unscharfer Suche und Synonym-Liste je Werkzeug (DE und EN)
- Meldungskatalog-Grundgerüst (Schlüssel → Text mit Platzhaltern → Aktionen); wird in AP-2.14 ausgebaut

**Akzeptanzkriterien:**
- [ ] „runde ecken“, „bohren“, „hohl“ und „namen“ finden das richtige Werkzeug
- [ ] Jedes Werkzeug aus Phase 1 hat Hinweis, Tooltip-Karte und Synonyme

---

### AP-1.13 Nutzertest 1
**Status:** offen · **Größe:** S · **Abhängigkeiten:** alle APs der Phase 1
**Bauanleitung:** [anleitungen/AP-1.13.md](anleitungen/AP-1.13.md)

**Umfang:**
- 5 Personen ohne CAD-Erfahrung (davon 2 Tinkercad-Nutzer): Aufgabe „Schlüsselanhänger mit deinem Namen“, Think-Aloud, Bildschirmaufnahme, SUS-Fragebogen
- Auswertung: Stolperstellen nach Schwere sortiert → neue Tickets oder AP-Ergänzungen

**Akzeptanzkriterien:**
- [ ] Bericht `docs/nutzertests/2026-xx-phase1.md`
- [ ] ≥ 4 von 5 schaffen die Aufgabe in ≤ 10 min; sonst werden die Top-3-Probleme vor Phase 2 behoben
