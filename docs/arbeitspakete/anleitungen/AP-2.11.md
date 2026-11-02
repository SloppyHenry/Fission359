# AP-2.11 Messen und Analysieren

**Größe:** M · **Phasendatei:** AP-2.11

## Ziel
**Messen**: Abstand (Punkt/Kante/Fläche, kürzester Abstand), Winkel, Radius/Durchmesser, Länge, Fläche; Ergebnis kopierbar. **Körper-Infos**:
Volumen, Oberfläche, Hüllquader, **Gewicht und Filamentlänge** nach Material (PLA, PETG, ABS, ASA, TPU; Dichte änderbar). **Schnittansicht**:
eine Ebene durch das Modell ziehen, Schnittfläche schraffiert. Messwerte stimmen auf 1e-6 mit analytischen Werten.

## Voraussetzungen
AP-1.4 (Auswahl).

## Vorher lesen
`docs/02-ux-konzept.md` §3 (Gruppe Prüfen), §12; `docs/03-architektur.md` §2 (`src/analysis`); `SolidReport` in `geo/solid.h`.

## Haltepunkte
1. **Materialtabelle**: Empfehlung: Dichten PLA 1,24, PETG 1,27, ABS 1,04, ASA 1,07, TPU 1,21 g/cm³; Filament 1,75 mm (2,85 wählbar);
   Hinweis, dass die echte Masse von Füllung und Wänden abhängt (Volumen ist ein Maximalwert). Bestätigen, wie das angezeigt wird (z. B. „bis zu 12 g“).
2. **Schnittansicht:** nur Anzeige (Renderer-Clipping) oder echter Schnitt? Empfehlung: Anzeige per Clipping-Ebene im Shader + Deckfläche aus
   dem exakten Schnitt (`BRepAlgoAPI_Section`) für die Schraffur; ändert das Modell nicht.

## Schritte
1. GAL: `MeasureResult measure(const MeasureQuery&)` mit Elementen (Punkt, Kante, Fläche per Name) → kürzester Abstand
   (`BRepExtrema_DistShapeShape`), Winkel (zwischen Kanten/Flächen-Normalen), Radius (Kreis/Zylinder), Länge (`GProp`), Fläche. Golden je Art.
2. `src/analysis`: Masse/Filament aus Volumen und Material (rein, Tests).
3. Werkzeug „Messen“ (Gruppe Prüfen): ein bis zwei Elemente anklicken, Ergebnis als Maßlinie in der Ansicht und als Zeile in einer kleinen Karte
   (mit Kopieren-Knopf, Text „12,50 mm“); ⇧-Klick misst weiter. Esc beendet. Regel 10 („messen“, „abstand“, „lineal“, „measure“, „distance“).
4. Körper-Infos im Panel (▸ Mehr bei gewähltem Körper): Volumen, Oberfläche, Hüllquader, Material-Auswahl, Gewicht, Filamentlänge
   (Länge = Volumen / Querschnitt des Filaments).
5. Schnittansicht: Ansicht ▸ Schnitt; Ebene mit Pfeil-Griff verschieben (X/Y/Z wählbar), Schraffur auf der Schnittfläche. Renderer: Clip-Ebene
   als Uniform, Deckel als zusätzliches Netz.

## Tests
Golden (1e-6): Abstand zweier paralleler Quaderflächen, Winkel 90°/45° (Keil), Radius Zylinder, Länge Kreiskante (2πr), Fläche; Masse PLA eines
Würfels 20 mm = 8 cm³ × 1,24 = 9,92 g; Schnitt durch Quader mit Loch → Deckfläche nach Formel.

## Abnahme
So probierst du es aus: Messen, zwei Flächen anklicken → Abstand; Kopieren. Körper wählen ▸ Mehr → Gewicht in PETG. Ansicht ▸ Schnitt, Ebene ziehen.

## Nicht tun
Kein Wandstärken-Heatmap (Druckcheck AP-3.2).
