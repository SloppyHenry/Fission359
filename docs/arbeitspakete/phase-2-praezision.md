# Phase 2 – Präzision

**Ziel:** Fission359 wird zum echten Konstruktionsprogramm: Skizzen mit Maßen und Regeln, Ziehen und Drehen, Abrunden, Aushöhlen, Bohrungen für echte Schrauben, benannte Maße. Alles bleibt über das Rezept änderbar.

**Meilenstein M2 (Demo):** Ablauf B „Wandhalterung“ aus dem [UX-Konzept](../02-ux-konzept.md#b-wandhalterung-ziel--20-min-für-tinkercad-umsteiger) komplett, inkl. nachträglicher Maßänderung, bei der alles korrekt bleibt. STEP-Export öffnet sich in Fusion und FreeCAD mit exakter Geometrie.

---

### AP-2.1 Skizzen-Modus: Zeichnen
**Status:** in Arbeit (2.1a Datenmodell, 2.1b Skizzen-Modus gebaut) · **Größe:** L · **Abhängigkeiten:** AP-1.4, AP-1.6, AP-1.10, AP-2.10 (Teile a–c)
**Bauanleitung:** [anleitungen/AP-2.1.md](anleitungen/AP-2.1.md)

**Umfang:**
- Skizze starten: Fläche, Grundebene oder Hilfsebene anklicken → animierte Draufsicht, Skizzenraster
- Werkzeuge: Linie (mit Bogen-Übergang per Ziehen), Rechteck (2-Punkt, Mitte), Kreis (Mitte, 3-Punkt), Bogen, Langloch, Polygon, Spline, Punkt, Hilfslinie (gestrichelt, nicht profilbildend)
- Bearbeiten: Trimmen (per Überstreichen), Verlängern, Versetzen, Abrunden/Abschrägen von Skizzen-Ecken, Spiegeln, Muster
- Kanten von Körpern projizieren/übernehmen
- **Automatische Profilerkennung:** geschlossene Bereiche werden schattiert und sind auswählbar (auch überlappende Bereiche einzeln)

**Akzeptanzkriterien:**
- [ ] Ein L-Profil mit Rundung lässt sich ohne Regel-Kenntnisse in < 1 min zeichnen
- [ ] Bereichserkennung funktioniert bei überlappenden Kreisen und Rechtecken (Golden-Tests)

**Vermerk 2026-11-06 (Teil 2.1a, Datenmodell und Befehle):**
- Ergebnis: Schritt-Typ `sketch` (kein Körper) mit `Feature::sketch`; `SketchPlane` (`toWorld`/`toPlane`), Elemente Punkt, Linie, Kreis, Bogen, Spline mit stabilen Ids je Art
  (`P1`, `L3`, `C2`, `A1`, `S1`); `RecomputeResult::sketches`; Befehle Element hinzufügen/entfernen, Punkte verschieben (ein Zug = ein Undo-Schritt), Hilfslinie an/aus; Dateiformat **3**
  (Migration 1→2→3, Fixtures `v1/sketch.fission` und `v3/sketch.fission`); Rezept-Satz „Skizze“.
- Festlegungen: [ADR-0013](../adr/0013-skizzen-im-dokument.md) und AP-2.1.md „Festlegungen (2026-11-06)“ (alle Elemente sind Punkte; Ebene aus einer Fläche rechnet erst 2.1b).
- Offene Reste: Teile c–g (Zeichnen, Spline, Bearbeiten, Kanten, Bereiche); die Reste dieses Vermerks (Texte der neuen Befehle, Meldungstext für `basePlane`/`sketch`) sind mit 2.1b erledigt.

**Vermerk 2026-11-06 (Teil 2.1b, Skizzen-Modus):**
- Ergebnis: Knopf „Skizze“ (Palette, Befehlssuche mit Synonymen, Hinweis, Tooltip) wählt die Ebene: Klick auf die Grundebene (z = 0) oder auf eine ebene Körperfläche
  (Schritt mit Verweis `plane` und dem Körper als Eingang; die Ebene wird aus der Fläche gerechnet und folgt dem Körper, `GeometryKernel::flatFace`, `core::planeOnFace`);
  gekrümmte Fläche → Hinweis statt Schritt. Im Modus: Ansicht dreht animiert auf die Ebene, andere Körper blass (ca. 35 %), Skizzenraster, Punkt-Marker und
  gestrichelte Hilfslinien (`interaction::planePoint`, `sketchSnap`), Knopf „Fertig“ und Esc, leere Skizze wird per Command wieder entfernt, Doppelklick im Rezept öffnet sie erneut.
- Festlegungen: AP-2.1.md „Festlegungen“ 7–12. Ein `sketch`-Schritt zählt nicht als Verbraucher des Körpers (`Document::consumerOf`).
- Offene Reste: Zeichenwerkzeuge (2.1c); Fenster-Tests `app_sketch_ground` und `app_sketch_face` liefen bisher nur in der CI.

---

### AP-2.2 Skizzen-Regeln und Maße (Constraint-Solver)
**Status:** offen · **Größe:** XL · **Abhängigkeiten:** AP-2.1
**Bauanleitung:** [anleitungen/AP-2.2.md](anleitungen/AP-2.2.md)

> Vor dem Start in Teilpakete zerlegen (Solver-Anbindung, Auto-Regeln, Maße, Diagnose-UI).

**Umfang:**
- PlaneGCS-Anbindung hinter der GAL: Regeln Koinzident, Horizontal, Vertikal, Parallel, Senkrecht, Tangential, Gleich, Symmetrisch, Mittelpunkt, Fixiert, Punkt-auf-Kurve, konzentrisch
- Maße: Länge, Abstand (horizontal/vertikal/schräg), Radius/Durchmesser, Winkel; Maße akzeptieren Ausdrücke und benannte Maße
- **Auto-Regeln beim Zeichnen** (mit Vorschau-Symbol, abschaltbar per ⌥/Alt)
- Freiheitsgrade: Anzeige „kann sich noch in N Richtungen bewegen“; Ziehen an freier Geometrie zeigt die verbleibende Bewegung
- **Konfliktdiagnose:** widersprüchliche oder überflüssige Regeln identifizieren und in Klartext mit Ein-Klick-Lösungen erklären
- Live-Lösen beim Ziehen (≥ 30 Hz bei 200 Elementen)

**Akzeptanzkriterien:**
- [ ] Nicht vollständig bestimmte Skizzen sind gültig und werden nie als Fehler dargestellt
- [ ] Jeder Konflikt nennt die beteiligten Regeln und bietet ≥ 1 Lösung an
- [ ] Solver-Tests: 50 Referenzskizzen konvergieren deterministisch

---

### AP-2.3 Aus Skizzen werden Körper
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.2, AP-2.10, AP-1.7
**Bauanleitung:** [anleitungen/AP-2.3.md](anleitungen/AP-2.3.md)

**Umfang:**
- **Ziehen** (Extrude): Distanz, bis Fläche, symmetrisch, durch alles; Verjüngungswinkel; Modus Neu/Vereinen/Ausschneiden/Überlappung **automatisch vorgeschlagen** (Richtung in den Körper = Ausschneiden), manuell änderbar
- **Drehkörper** (Revolve): Achse wählen (Skizzenlinie, Kante, Hauptachse), Winkel
- **Entlang Pfad** (Sweep): Profil + Pfad, Ausrichtung
- **Übergang** (Loft): zwei oder mehr Profile, optional Führungskurven
- Direkter Pfeil-Griff auf Skizzenflächen (ohne Werkzeugwahl)

**Akzeptanzkriterien:**
- [ ] Golden-Tests für alle Modi
- [ ] Ändert man die Skizze, aktualisiert sich der Körper und abhängige Features behalten ihre Bezüge (Naming-Tests)

---

### AP-2.4 Direktes Bearbeiten von Flächen
**Status:** in Arbeit (Teil 1 in main seit 2026-11-06, PR #98; Teile a–e gebaut: Fläche ziehen, intelligentes Strecken, Fläche drehen, entfernen, versetzen, mit Befehlen und Pfeil; Tests aus dem Kern und der Oberfläche; Bogen-Griff zum Drehen, Fenster-Test, Körper ohne Verlauf offen) · **Größe:** L · **Abhängigkeiten:** AP-1.4, AP-2.10 (Basis)
**Bauanleitung:** [anleitungen/AP-2.4.md](anleitungen/AP-2.4.md)

**Umfang:**
- **Fläche ziehen** (Push/Pull) an beliebigen ebenen und zylindrischen Flächen von Körpern (OCCT `BRepOffset` mit Flächen-Offset bzw. `BRepFeat`)
- **Intelligentes Strecken** kombinierter Körper ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern)): Körper an einer Ebene teilen, eine Hälfte verschieben, Lücke mit dem Querschnitt füllen. Rundungen, Löcher und Text bleiben unverändert. Die Schnittebene wird automatisch an eine Stelle ohne Details gelegt und ist verschiebbar
- **Fläche verschieben/drehen** mit Gizmo
- **Fläche entfernen** (Defeaturing: Rundungen, Löcher, Fasen „wegheilen“; `BRepAlgoAPI_Defeaturing`)
- **Fläche versetzen** (Offset), z. B. für Passungen
- Funktioniert auch bei importierten STEP-Körpern ohne Verlauf

**Akzeptanzkriterien:**
- [ ] Ein Loch in einem importierten STEP-Teil lässt sich per Ziehen am Lochzylinder vergrößern
- [ ] Ein Loch lässt sich per „Fläche entfernen“ schließen
- [ ] Ablauf D, Schritt 5 (Strecken eines kombinierten Körpers, Loch bleibt rund) läuft automatisiert grün

---

### AP-2.5 Abrunden und Abschrägen
**Status:** in Arbeit (Teil 1 in main seit 2026-11-06, PR #97; Kernel, Schritte, Meldung, Befehle, Griff am Schritt und Hover-Knopf gebaut; Tests aus AP-0.5, Namens-Szenarien, Fenster-Test offen) · **Größe:** L · **Abhängigkeiten:** AP-2.10 (Basis)
**Bauanleitung:** [anleitungen/AP-2.5.md](anleitungen/AP-2.5.md)

**Umfang:**
- Abrunden (konstanter Radius; variabler Radius als „▸ Mehr“), Abschrägen (Abstand, Abstand+Winkel, zwei Abstände)
- **Kanten-Griff direkt in der Ansicht** (Viertelkreis-Symbol beim Hover)
- Kettenauswahl: tangential verbundene Kanten automatisch mitnehmen; „alle Kanten einer Fläche“, „alle senkrechten Kanten“
- **Größtmöglicher Radius:** Bei Fehlschlag per Bisektion den maximal möglichen Wert ermitteln und als Lösung anbieten

**Akzeptanzkriterien:**
- [ ] Fehlschläge zeigen immer den größtmöglichen Wert als Ein-Klick-Lösung
- [ ] 20 typische Fälle aus AP-0.5 laufen stabil

---

### AP-2.6 Aushöhlen und Schräge
**Status:** in Arbeit (Teil 1 in main seit 2026-11-06, PR #98; Kern mit Plausibilitätsprüfung, Schritte, Befehle, Meldungen gebaut; Rezept-Neubau des Innenkörpers, Wand je Fläche, Winkel-Griff, Fenster-Test offen) · **Größe:** M · **Abhängigkeiten:** AP-2.4, AP-2.10
**Bauanleitung:** [anleitungen/AP-2.6.md](anleitungen/AP-2.6.md)

**Umfang:**
- Aushöhlen: Wandstärke innen/außen, offene Flächen wählen (z. B. Deckel weg), unterschiedliche Stärken je Fläche (▸ Mehr)
- Schräge (Draft) an Flächen; Anwendungsbeispiel „Formen, die sich leicht lösen“

**Akzeptanzkriterien:**
- [ ] Aushöhlen eines abgerundeten Quaders mit offener Oberseite funktioniert (klassischer Problemfall)

---

### AP-2.7 Bohrungen, Gewinde und Schraubverbindungen
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.3, AP-2.10, AP-1.6
**Bauanleitung:** [anleitungen/AP-2.7.md](anleitungen/AP-2.7.md)

**Umfang:**
- **Bohrungs-Assistent** mit Alltagsauswahl: „Für Schraube M3 … M10“, „Holzschraube 3–6 mm“, Durchgangsloch, Senkkopf, Zylinderkopf (Senkung), Kernloch für Gewinde, **Einschmelzmutter** (heat-set insert, gängige Größen), **Mutternfalle** (Sechskant-Tasche, auch seitlich einschiebbar)
- Normtabellen (ISO 273, ISO 10642, DIN 7991, ISO 4032) als Datendateien in `resources/`
- **Echte Gewinde** (metrisch, Bolzen und Mutter, innen und außen) als Geometrie mit Druckspiel-Option; alternativ „kosmetisch“ für die Anzeige
- Lochkorrektur für den 3D-Druck aus dem Druckerprofil (z. B. +0,2 mm) mit Erklärung
- Platzierung per Klick auf Fläche; Abstandsmaße zu Kanten; mehrere Löcher in einem Schritt

**Akzeptanzkriterien:**
- [ ] Gedruckte Testteile (M3-Durchgang, M3-Einschmelzmutter, M8-Gewinde Bolzen+Mutter) passen auf Bambu P1S und Prusa MK4 mit Standardprofil (Fotos im Abschlussvermerk)
- [ ] Golden-Tests für Lochgeometrie gegen Normtabellen

---

### AP-2.8 Muster und Spiegeln
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-2.3, AP-2.10, AP-2.12, AP-1.7
**Bauanleitung:** [anleitungen/AP-2.8.md](anleitungen/AP-2.8.md)

**Umfang:**
- Reihe (1D/2D, Anzahl + Abstand oder Gesamtlänge), Kreis (Anzahl, Winkel, Achse), entlang Pfad, Spiegeln (Ebene, Fläche)
- Muster von Features (z. B. Löcher) **und** von Körpern
- Einzelne Instanzen auslassen (anklicken)

**Akzeptanzkriterien:**
- [ ] Ändert man das Ausgangs-Feature, ändern sich alle Instanzen
- [ ] 100 Instanzen in < 1 s

---

### AP-2.9 Benannte Maße und Konfigurator
**Status:** erledigt (2026-11-06, PR #95) · **Größe:** M · **Abhängigkeiten:** AP-1.8
**Bauanleitung:** [anleitungen/erledigt/AP-2.9.md](anleitungen/erledigt/AP-2.9.md)

**Umfang:**
- Maße-Tabelle (Name, Ausdruck, Ergebnis, Einheit, Kommentar, „verwendet in“)
- Erstellen direkt aus einem Zahlenfeld: Rechtsklick → „Als benanntes Maß speichern“
- Autovervollständigung in allen Zahlenfeldern, Umbenennen aktualisiert alle Ausdrücke
- Schieberegler-Ansicht (min/max/Schritt je Maß) als **Konfigurator**

**Akzeptanzkriterien:**
- [x] Ablauf B Schritt 7 (Breite ändern) funktioniert vollständig (Unit-Test am Dokumentmodell; Ausprobieren im Programm steht aus)
- [x] Umbenennen eines Maßes, das in 10 Ausdrücken steckt, ist ein einziger Undo-Schritt (Test `rename everywhere: ten formulas at once`)

**Abschlussvermerk 2026-11-05:**
- Ergebnis: Bereich „Maße“ unter dem Rezept (⌘M, Ansicht ▸ Benannte Maße) mit Tabelle (Name, Formel, Ergebnis, „Benutzt in“ mit Klick auf den Schritt, Kommentar, Fehler je Zeile mit Zyklus-Pfad) und Regler-Ansicht
  (Ziehen = ein Undo-Schritt, Live-Vorschau); Umbenennen schreibt alle Formeln in Schritten und Maßen um (`makeRenameMeasureEverywhere`, nur ganze Namen);
  Rechtsklick am Zahlenfeld „Als benanntes Maß speichern“; Autovervollständigung im Zahlenfeld (`Numbers::completions`).
- Festlegungen: Dateiformat **2** (neues Feld `slider` {min, max, step} je Maß, sonst null; Migration 1→2 gebaut, alte Dateien öffnen weiter), Bereich unter dem Rezept.
  Maßnamen bestehen aus Buchstaben, Ziffern und `_` (Kollision: `Breite2`, nicht „Breite 2“, weil Formeln keine Leerzeichen in Namen lesen).
- Offene Reste: Tabelle und Regler sind nur per Unit-Tests belegt, nicht im Fenster angesehen; Maße vom Typ „Winkel“ nehmen im Regler Grad ohne Einheitenzusatz.

---

### AP-2.10 Persistente Referenzen (produktiv) und Reparatur-UI
**Status:** erledigt (2026-11-06, PR #91) · **Größe:** XL · **Abhängigkeiten:** AP-0.6, AP-1.1, AP-1.7, AP-1.10
**Bauanleitung:** [anleitungen/erledigt/AP-2.10.md](anleitungen/erledigt/AP-2.10.md)

> Vor dem Start in Teilpakete zerlegen (Naming für jede Feature-Art, Signatur-Matching, Reparatur-UI).

**Umfang:**
- Umsetzung des in ADR-0003 gewählten Ansatzes für **alle** Feature-Typen
- Statusanzeige „braucht Hilfe“ mit Geist der alten Geometrie; Neu-Auswahl mit einem Klick
- Testsuite mit ≥ 40 Änderungsszenarien (wächst mit jedem neuen Feature-Typ)

**Akzeptanzkriterien:**
- [x] 0 stille Fehlzuordnungen in der Testsuite (550 beurteilte Verweise, davon 70 Szenarien und 480 aus der Zufallsprobe; `scripts/test --release`, Mac)
- [x] ≥ 90 % der Szenarien lösen sich automatisch korrekt auf (99 %; 4 zu Recht um Hilfe gebeten, 1 unnötig)

**Abschlussvermerk 2026-11-03 (Teile a–e, Gesamtlauf 774 von 774 grün auf dem Mac):**
- Ergebnis: Kernel beschreibt Flächen und Kanten (`geo::ElementSignature`), Booleans melden die Namens-Historie, Körper merken verbrauchte
  Elemente; `core::resolve` (strenger Modus, Schwellen benannt); Einbau in die Neuberechnung (Warnung `referenceNeedsHelp` mit Grund, letztes
  gültiges Ergebnis bleibt); Heilen ohne Undo-Schritt; `makeRef`, `makeSetFeatureRefs`; Produkt-Suite (70 Szenarien, Zufallsprobe, drei
  Baustein-Gegenproben, Zeile in `scripts/test`); Reparatur-Oberfläche mit Geist und „Neu wählen“.
- Festlegungen: [ADR-0003](../adr/0003-persistente-referenzen.md) Nachtrag 2026-11-03 (Namen geteilter Flächen, Koordinaten je Schritt,
  Position wird nicht herausgerechnet). Heilen ohne Undo-Schritt (Entscheidung 2026-11-03).
- Messwert: Auflösung von 49 Verweisen in einem Lauf ohne Kernel-Aufruf 0,64 ms (Release, Mac; Budget 50 ms).
- Offene Reste: Szenarien mit Abrunden, Bohren, Skizzen (mit AP-2.5, 2.7, 2.3, in deren Anleitungen eingetragen); Fenster-Test der
  Reparatur (sichtbar erst mit AP-2.5); Lauf auf Linux steht aus (CI); die Aussehen eines Körpers werden je Lauf einmal berechnet, nicht über
  Läufe hinweg gespeichert. Details: [anleitungen/erledigt/AP-2.10.md](anleitungen/erledigt/AP-2.10.md), Abschnitte „Stand“.

---

### AP-2.11 Messen und Analysieren
**Status:** in Arbeit (Messen und Eckdaten in main seit 2026-11-06, PR #94; Schnittansicht offen) · **Größe:** M · **Abhängigkeiten:** AP-1.4
**Bauanleitung:** [anleitungen/AP-2.11.md](anleitungen/AP-2.11.md)

**Umfang:**
- Messen: Abstand (Punkt/Kante/Fläche, min. Abstand), Winkel, Radius/Durchmesser, Länge, Fläche; Ergebnis kopierbar
- Körper-Infos: Volumen, Oberfläche, Bounding Box, **Gewicht und Filamentlänge** nach Material (PLA, PETG, ABS, ASA, TPU; Dichte editierbar)
- **Schnittansicht:** Ebene ziehen, Schnittfläche schraffiert

**Akzeptanzkriterien:**
- [x] Messwerte stimmen mit analytischen Werten überein (1e-6): Golden-Tests für Abstand, Winkel (90°, 45° am Keil), Länge, Radius, Fläche; Gewicht PLA 20-mm-Würfel 9,92 g
- [ ] Schnittansicht (Ebene ziehen, Schnittfläche schraffiert): **nicht gebaut**

**Abschlussvermerk 2026-11-05 (ohne Schnittansicht):**
- Ergebnis: Kernel misst Ecke, Kante, Fläche und je zwei davon (`measureElements`: Position, Länge, Fläche, Radius, kürzester Abstand, Winkel). Werkzeug „Messen“ (Prüfen ▸ Messen, Ansicht ▸ Messen):
  Auswahl von ein bis zwei Elementen, Maßlinie mit Pfeilen und Zahl in der Ansicht, Karte mit Zeilen und „Kopieren“ (Hauptzahl, z. B. „12,50 mm“), Esc beendet.
  Eckdaten im Panel bei einem gewählten Körper: Volumen, Oberfläche, Maße, Material (PLA, PETG, ABS, ASA, TPU), Filament 1,75/2,85 mm, Dichte änderbar,
  „Gewicht bis zu … g“ und Filamentlänge „bis zu … m“ (Maximalwert, Hinweis auf Füllung und Wände).
- Festlegungen: Gewicht wird als Höchstwert angezeigt; Schnittansicht soll nur Anzeige sein (Clip-Ebene, Deckfläche aus dem exakten Schnitt), nicht das Modell ändern.
- Offene Reste: **Schnittansicht** (Clip-Ebene in allen Zeichen-Durchläufen des Renderers: Körper, Schatten, Umgebungsverdeckung, Auswahl; Deckfläche mit Schraffur); Messen mit Körpern als Ganzes und Punkt im Raum;
  Materialwahl wird nicht mit dem Projekt gespeichert.

---

### AP-2.12 Hilfsgeometrie
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-2.1, AP-2.10
**Bauanleitung:** [anleitungen/AP-2.12.md](anleitungen/AP-2.12.md)

**Umfang:**
- Hilfsebenen (versetzt, gewinkelt, durch 3 Punkte, tangential, Mittelebene), Hilfsachsen, Hilfspunkte
- Dezent dargestellt, im Rezept sichtbar, ein-/ausblendbar

---

### AP-2.13 STEP und weitere Formate
**Status:** erledigt (2026-11-06, PR #93) · **Größe:** M · **Abhängigkeiten:** AP-1.10
**Bauanleitung:** [anleitungen/erledigt/AP-2.13.md](anleitungen/erledigt/AP-2.13.md)

**Umfang:**
- STEP AP214/AP242 Import und Export inkl. Farben und Namen (OCCT XCAF); IGES Import
- OBJ-Export; DXF/SVG-Export einer Skizze oder Fläche (für Lasercutter)
- Importierte STEP-Körper sind präzise (aber ohne Verlauf) → mit direktem Bearbeiten (AP-2.4) änderbar

**Akzeptanzkriterien:**
- [x] Roundtrip Fission → STEP → Fission ohne Geometrieverlust (Volumen, Fläche, Flächenzahl, Namen, Farben), auch mit 6 STEP-Dateien anderer Programme
- [ ] Roundtrip über Fusion/FreeCAD: manuell, steht noch aus (Maintainer)

**Abschlussvermerk 2026-11-05:**
- Ergebnis: `.fission` mit Ordner `assets/` (Prüfsummen), Schritt „Import“ je Körper (ein Undo-Schritt), STEP/IGES lesen, STEP schreiben (Farben,
  Namen), OBJ (mit `.mtl`), SVG und DXF vom Umriss einer ebenen Fläche, Datei ▸ Importieren…, Ablegen auf das Fenster, Export-Karte um OBJ, STEP,
  SVG, DXF erweitert.
- Festlegungen: [ADR-0012](../adr/0012-eingebettete-dateien-und-import.md). Annahmen: Schritt-Typ heißt `import` (Flächennamen `F<n>/import/face<i>`);
  Import-Körper nur verschiebbar (x, y, z); SVG/DXF-Ansicht von außen auf die Fläche.
- Offene Reste: „Aktualisieren“ einer eingebetteten Datei; Drehen/Größe eines Import-Körpers über `place` und AP-2.4; reine Flächenmodelle aus IGES;
  Reparatur-Hinweis nur durch Fremddateien mit Fehlern prüfbar (kein Testfall in der Sammlung); Roundtrip über Fusion/FreeCAD.



---

### AP-2.14 Meldungskatalog und Fehler-UX
**Status:** in Arbeit (Teil 1 in main seit 2026-11-06, PR #96; Grundlage gebaut, Rest hängt an AP-2.4/2.5) · **Größe:** M · **Abhängigkeiten:** AP-1.12, AP-0.5
**Bauanleitung:** [anleitungen/AP-2.14.md](anleitungen/AP-2.14.md)

**Umfang:**
- Katalog: strukturierter Diagnose-Code → Titel, Erklärung (mit Platzhaltern für Zahlen), Lösungsaktionen
- Abbildung typischer OCCT-Fehlerbilder (aus AP-0.5) auf verständliche Ursachen
- Darstellung: Rezept-Badge, Inline-Karte an der Stelle in 3D, Geistergeometrie, keine modalen Dialoge
- Fallback-Meldung für Unbekanntes mit „Details für Fehlerbericht kopieren“

**Akzeptanzkriterien:**
- [ ] Alle Fehlercodes aus Phase 1 und 2 haben Katalogeinträge (CI-Prüfung: kein Code ohne Text)
- [ ] Kein roher Kernel-Text erreicht die UI

---

### AP-2.15 Nutzertest 2
**Status:** offen · **Größe:** S · **Abhängigkeiten:** alle APs der Phase 2
**Bauanleitung:** [anleitungen/AP-2.15.md](anleitungen/AP-2.15.md)

**Umfang:**
- 5 Personen (2 Neulinge nach Mission/Anleitung, 2 Tinkercad-Umsteiger, 1 Fusion-Nutzer): Ablauf B und eine freie Aufgabe „Ersatzteil nach Foto mit Maßen“
- SUS, Zeitmessung, Stolperstellen

**Akzeptanzkriterien:**
- [ ] Bericht in `docs/nutzertests/`; Ablauf B von ≥ 3 Tinkercad-Umsteigern in ≤ 20 min geschafft
