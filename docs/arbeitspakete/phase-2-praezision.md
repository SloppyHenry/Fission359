# Phase 2 – Präzision

**Ziel:** Fission359 wird zum echten Konstruktionsprogramm: Skizzen mit Maßen und Regeln, Ziehen und Drehen, Abrunden, Aushöhlen, Bohrungen für echte Schrauben, benannte Maße. Alles bleibt über das Rezept änderbar.

**Meilenstein M2 (Demo):** Ablauf B „Wandhalterung“ aus dem [UX-Konzept](../02-ux-konzept.md#b-wandhalterung-ziel--20-min-für-tinkercad-umsteiger) komplett, inkl. nachträglicher Maßänderung, bei der alles korrekt bleibt. STEP-Export öffnet sich in Fusion und FreeCAD mit exakter Geometrie.

---

### AP-2.1 Skizzen-Modus: Zeichnen
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.4, AP-1.6
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
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.2
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
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.4, AP-2.10 (Basis)
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
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.10 (Basis)
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
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-2.4
**Bauanleitung:** [anleitungen/AP-2.6.md](anleitungen/AP-2.6.md)

**Umfang:**
- Aushöhlen: Wandstärke innen/außen, offene Flächen wählen (z. B. Deckel weg), unterschiedliche Stärken je Fläche (▸ Mehr)
- Schräge (Draft) an Flächen; Anwendungsbeispiel „Formen, die sich leicht lösen“

**Akzeptanzkriterien:**
- [ ] Aushöhlen eines abgerundeten Quaders mit offener Oberseite funktioniert (klassischer Problemfall)

---

### AP-2.7 Bohrungen, Gewinde und Schraubverbindungen
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.3
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
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-2.3
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
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.8
**Bauanleitung:** [anleitungen/AP-2.9.md](anleitungen/AP-2.9.md)

**Umfang:**
- Maße-Tabelle (Name, Ausdruck, Ergebnis, Einheit, Kommentar, „verwendet in“)
- Erstellen direkt aus einem Zahlenfeld: Rechtsklick → „Als benanntes Maß speichern“
- Autovervollständigung in allen Zahlenfeldern, Umbenennen aktualisiert alle Ausdrücke
- Schieberegler-Ansicht (min/max/Schritt je Maß) als **Konfigurator**

**Akzeptanzkriterien:**
- [ ] Ablauf B Schritt 7 (Breite ändern) funktioniert vollständig
- [ ] Umbenennen eines Maßes, das in 10 Ausdrücken steckt, ist ein einziger Undo-Schritt

---

### AP-2.10 Persistente Referenzen (produktiv) und Reparatur-UI
**Status:** offen · **Größe:** XL · **Abhängigkeiten:** AP-0.6, AP-1.1
**Bauanleitung:** [anleitungen/AP-2.10.md](anleitungen/AP-2.10.md)

> Vor dem Start in Teilpakete zerlegen (Naming für jede Feature-Art, Signatur-Matching, Reparatur-UI).

**Umfang:**
- Umsetzung des in ADR-0003 gewählten Ansatzes für **alle** Feature-Typen
- Statusanzeige „braucht Hilfe“ mit Geist der alten Geometrie; Neu-Auswahl mit einem Klick
- Testsuite mit ≥ 40 Änderungsszenarien (wächst mit jedem neuen Feature-Typ)

**Akzeptanzkriterien:**
- [ ] 0 stille Fehlzuordnungen in der Testsuite
- [ ] ≥ 90 % der Szenarien lösen sich automatisch korrekt auf

---

### AP-2.11 Messen und Analysieren
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.4
**Bauanleitung:** [anleitungen/AP-2.11.md](anleitungen/AP-2.11.md)

**Umfang:**
- Messen: Abstand (Punkt/Kante/Fläche, min. Abstand), Winkel, Radius/Durchmesser, Länge, Fläche; Ergebnis kopierbar
- Körper-Infos: Volumen, Oberfläche, Bounding Box, **Gewicht und Filamentlänge** nach Material (PLA, PETG, ABS, ASA, TPU; Dichte editierbar)
- **Schnittansicht:** Ebene ziehen, Schnittfläche schraffiert

**Akzeptanzkriterien:**
- [ ] Messwerte stimmen mit analytischen Werten überein (1e-6)

---

### AP-2.12 Hilfsgeometrie
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-2.1
**Bauanleitung:** [anleitungen/AP-2.12.md](anleitungen/AP-2.12.md)

**Umfang:**
- Hilfsebenen (versetzt, gewinkelt, durch 3 Punkte, tangential, Mittelebene), Hilfsachsen, Hilfspunkte
- Dezent dargestellt, im Rezept sichtbar, ein-/ausblendbar

---

### AP-2.13 STEP und weitere Formate
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.10
**Bauanleitung:** [anleitungen/AP-2.13.md](anleitungen/AP-2.13.md)

**Umfang:**
- STEP AP214/AP242 Import und Export inkl. Farben und Namen (OCCT XCAF); IGES Import
- OBJ-Export; DXF/SVG-Export einer Skizze oder Fläche (für Lasercutter)
- Importierte STEP-Körper sind präzise (aber ohne Verlauf) → mit direktem Bearbeiten (AP-2.4) änderbar

**Akzeptanzkriterien:**
- [ ] Roundtrip Fission → STEP → Fusion/FreeCAD → STEP → Fission ohne Geometrieverlust (Volumen, Flächenzahl)

---

### AP-2.14 Meldungskatalog und Fehler-UX
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.12, AP-0.5
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
