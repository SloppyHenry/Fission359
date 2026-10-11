# Phase 1 – Der erste Körper

**Ziel:** Alles, was man für ein erstes Erfolgserlebnis braucht: Formen platzieren, exakt bemaßen, bewegen, kombinieren, rückgängig machen, speichern, exportieren. Am Ende fühlt sich Fission3D schon besser an als Tinkercad.

**Meilenstein M1 (Demo):** Ablauf A „Schlüsselanhänger“ aus dem [UX-Konzept](../02-ux-konzept.md#15-beispielabläufe-referenz-für-tests) mit flachem Text (Prägen auf gekrümmte Flächen folgt in Phase 4) → STL/3MF-Export → druckbar.

---

### AP-1.1 Dokumentmodell und Feature-Graph
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-0.5, AP-0.6

**Umfang:**
- `Document`, `Feature`, `Value`, `Body`, `Parameter` gemäß [Architektur §3](../03-architektur.md#3-core-datenmodell)
- Feature-Typ-Registry (Plugin-Schnittstelle: `validate`, `execute`, `describe`, `handles`)
- Recompute-Engine: Dirty-Propagation, Ergebnis-Cache, asynchroner Worker mit Abbruch, Status je Feature
- Ausdrucksparser mit Einheiten, Komma/Punkt, Fehlerpositionen und Zykluserkennung
- Signale/Events für die UI (Feature geändert, Recompute gestartet/fertig, Status geändert)

**Akzeptanzkriterien:**
- [ ] Core ist ohne Qt kompilierbar und hat ≥ 85 % Testabdeckung
- [ ] Eine Parameteränderung rechnet nur betroffene Features neu (per Test nachgewiesen)
- [ ] Eine laufende Neuberechnung bricht bei einer neuen Änderung innerhalb von ≤ 50 ms ab
- [ ] `"25,5 + 1 in"` ergibt `50,9 mm`; `"a = b, b = a"` liefert einen verständlichen Zyklusfehler

---

### AP-1.2 Commands und Undo/Redo
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1

**Umfang:**
- Command-Basisklasse, Transaktionen (zusammenfassen beim Ziehen), unbegrenzter Undo-Stack
- Standard-Commands: Feature hinzufügen, entfernen, Parameter setzen, ausblenden, Körper umbenennen, Farbe ändern
- Menü- und Tastenkürzel: ⌘Z / ⇧⌘Z bzw. Strg+Z / Strg+Y / Strg+⇧+Z
- Undo-Historie als Liste einsehbar (Bearbeiten → Verlauf)

**Akzeptanzkriterien:**
- [ ] Property-basierter Test: 1000 zufällige Commands, danach komplett rückgängig, ergibt den Ausgangszustand
- [ ] Ein Zieh-Vorgang erzeugt genau einen Undo-Schritt

---

### AP-1.3 Viewport und Navigation
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-0.4, AP-1.1

**Umfang:**
- Rendering aller Körper (Tessellierung mit Face/Edge-IDs, inkrementell nur geänderte Körper)
- Optik gemäß [UX §13](../02-ux-konzept.md#13-visuelle-gestaltung): matte Kunststoff-Optik, SSAO, Bettschatten, Kantenlinien
- Druckbett mit Raster in der Größe des aktiven Druckerprofils
- Kamera: Orbit, Pan, Zoom zum Zeiger; Perspektive/Ortho; Standardansichten; „Alles zeigen“; animierte Übergänge
- ViewCube (klickbar: Flächen, Kanten, Ecken)
- Maus-Voreinstellungen (Fission, Fusion, Tinkercad, Blender, SketchUp) und Trackpad-Gesten auf macOS
- Doppelklick auf Fläche = senkrecht draufschauen

- **Adaptive Darstellungsfeinheit:** Präzise Körper werden beim Hineinzoomen feiner tesselliert (im Hintergrund, ohne Ruckeln), damit Rundungen nie facettiert aussehen ([UX §7.3](../02-ux-konzept.md#73-niedrige-auflösung))

**Akzeptanzkriterien:**
- [ ] 60 fps bei 2 Mio. Dreiecken auf Referenz-Hardware (M1, Intel Iris Xe/AMD iGPU)
- [ ] Ein Loch Ø 3 mm zeigt bei bildschirmfüllendem Zoom keine sichtbaren Facetten (Screenshot-Test)
- [ ] Alle Voreinstellungen sind in einem Test-Skript durchgespielt
- [ ] Trackpad-Navigation fühlt sich auf macOS „wie Shapr3D/Fusion“ an (subjektiver Check durch 3 Personen)

---

### AP-1.4 Auswahl und Hover
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.3

**Umfang:**
- Picking von Körper, Fläche, Kante und Ecke über ID-Buffer; Hover-Hervorhebung
- Klick-Logik: Einfachklick wählt Fläche/Kante (je nachdem, was näher ist), Doppelklick wählt den Körper; ⇧ erweitert die Auswahl, Rahmenauswahl
- Auswahlfilter (nur Körper, nur Flächen …), abhängig vom aktiven Werkzeug
- Auswahl-Modell im Core (stabil über Neuberechnung via persistente Referenzen)

**Akzeptanzkriterien:**
- [ ] Hover-Feedback < 16 ms
- [ ] Nach einer Parameteränderung bleibt die Auswahl erhalten (wenn die Geometrie noch existiert)

---

### AP-1.5 Grundformen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1, AP-1.4

**Umfang:**
- Quader (mit optionalem Eckenradius), Zylinder, Kugel, Kegel, Torus, Röhre, Prisma (n-eckig), Pyramide, Halbkugel, Keil, Stern (2D-Stern hochgezogen), Text (flach, System- und mitgelieferte Schriften)
- Formen-Galerie (große Kacheln mit 3D-Vorschau); Drag-and-drop in die Ansicht
- **Auf Fläche legen:** Die Form landet auf der Fläche unter dem Zeiger; diese wird zur Arbeitsebene
- Maß-Griffe (Tinkercad-artig) je Form; Wertblase; Tippen ersetzt Ziehen; `Tab` wechselt das Maß
- **„Als Loch“-Schalter** je Form (wirkt in AP-1.7)

- **Griffe ändern Parameter, nie die Skalierung** ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern)). Ist ein Körper aus einer Grundform entstanden, zeigen seine Griffe auch später die Maße dieser Grundform.

**Akzeptanzkriterien:**
- [ ] Jede Form lässt sich per Drag-and-drop platzieren und über Griffe oder Tastatur exakt bemaßen
- [ ] Quader mit Eckenradius 5 mm auf doppelte Breite gezogen → Radius bleibt exakt 5 mm (Golden-Test; Ablauf D, Schritt 1)
- [ ] Text: Umlaute, ß, Emojis werden sauber abgelehnt („Diese Schrift hat kein Zeichen für …“)
- [ ] Golden-Tests: Volumen jeder Form = analytischer Wert (rel. 1e-6)

---

### AP-1.6 Transformieren: Gizmo, Fangen, Abstandsmaße
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.4

**Umfang:**
- Gizmo: Verschieben (Achsen und Ebenen), Drehen (Ringe), **proportional** Vergrößern/Verkleinern (Eck-Griff mit ⇧ oder Prozentangabe)
- **Kein ungleichmäßiges Skalieren über das Gizmo.** Breiter/höher machen geht über die Maß-Griffe der Grundform (AP-1.5) bzw. ab Phase 2 über Strecken (AP-2.4). Echtes Verzerren nur über Ändern ▸ Mehr ▸ „Verzerren“, mit Warnhinweis ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern))
- Fangen: adaptives Raster, Ecken, Kantenmitten, Kreismittelpunkte, Flächenmitten, 15°-Winkel; ⇧ = aus
- **Intelligente Abstandsmaße** zu den nächsten Kanten anderer Körper und zum Bett, anklickbar und editierbar
- Ausrichten-Werkzeug (mehrere Körper bündig oder mittig)
- „Aufs Bett legen“ (Körper fällt auf z = 0), „Auf Fläche legen“
- Kopieren/Einfügen/Duplizieren (⌘D) mit Versatz

**Akzeptanzkriterien:**
- [ ] Eine Form lässt sich per Abstandsmaß exakt „5 mm vom Rand“ platzieren
- [ ] Jede Transformation steht als Rezept-Eintrag im Verlauf und ist editierbar

---

### AP-1.7 Kombinieren
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.5

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

**Umfang:**
- Native Menüs (macOS-Menüleiste, Linux-Fenstermenü), Tastenkürzel-Konzept (plattformgerecht)
- Erststart-Assistent: Sprache, Drucker (Profile: Bambu A1/A1 mini/P1S/X1C, Prusa MK4/Core One/Mini, Creality Ender-3 V3/K1, Voron, eigenes), Maus-Stil
- Einstellungen: Einheit, Nachkommastellen, Maus-Stil, Thema (hell/dunkel/System), UI-Skalierung, Autosave
- i18n-Infrastruktur (Qt Linguist), Deutsch und Englisch vollständig
- Startbildschirm (Version 1): Neu, Zuletzt geöffnet, Beispiele

**Akzeptanzkriterien:**
- [ ] Sprachwechsel ohne Neustart
- [ ] Keine hartkodierten UI-Texte (Prüfskript in der CI)

---

### AP-1.12 Hinweis-System und Befehlssuche
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.11

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

**Umfang:**
- 5 Personen ohne CAD-Erfahrung (davon 2 Tinkercad-Nutzer): Aufgabe „Schlüsselanhänger mit deinem Namen“, Think-Aloud, Bildschirmaufnahme, SUS-Fragebogen
- Auswertung: Stolperstellen nach Schwere sortiert → neue Tickets oder AP-Ergänzungen

**Akzeptanzkriterien:**
- [ ] Bericht `docs/nutzertests/2026-xx-phase1.md`
- [ ] ≥ 4 von 5 schaffen die Aufgabe in ≤ 10 min; sonst werden die Top-3-Probleme vor Phase 2 behoben
