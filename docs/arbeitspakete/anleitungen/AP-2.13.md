# AP-2.13 STEP und weitere Formate

**Größe:** M · **Phasendatei:** AP-2.13

## Ziel
STEP (AP214/AP242) importieren und exportieren mit Farben und Namen; IGES importieren; OBJ exportieren; Skizze oder Fläche als DXF/SVG exportieren
(Lasercutter). Importierte STEP-Körper sind präzise, aber ohne Verlauf; man ändert sie mit direktem Bearbeiten (AP-2.4). Roundtrip
Fission → STEP → Fusion/FreeCAD → STEP → Fission ohne Geometrieverlust.

## Voraussetzungen
AP-1.10 (Dateien), sinnvoll AP-2.4.

## Vorher lesen
`docs/03-architektur.md` §7, §8; `cmake/FissionDependencies.cmake` (`FISSION_OCCT_EXTRA_TARGETS` enthält DataExchange; bisher nur für Tests
gelinkt), `tests/geometry/test_io.cpp` (AP-0.5: STEP-Roundtrip mit Farben).

## Haltepunkte
1. **Import-Körper im Rezept:** Empfehlung: Feature-Typ `importSolid` mit der eingebetteten STEP-Datei in `assets/` des `.fission` (Architektur §7),
   Lage-Werte (ADR-0010) für Verschieben, Flächennamen `F<n>/import/face<i>` nach Reihenfolge der Datei. Ein erneuter Import derselben Datei
   ersetzt nicht automatisch (später „Aktualisieren“).
2. **Einheit beim Import:** STEP trägt Einheiten → umrechnen; IGES ebenso; bei fehlender Einheit fragen? Empfehlung: Annahme mm + Hinweis mit
   Ein-Klick „war in Zoll“ (×25,4).

## Schritte
1. `fission_geometry_occt` linkt die DataExchange-Module (`FissionDependencies.cmake`/`occt/CMakeLists.txt`); Paketgröße vorher/nachher im PR.
2. GAL: `ImportResult importStep(bytes)`, `importIges(bytes)` → Liste von Körpern mit Name, Farbe; `std::string exportStep(bodies with names/colours)`.
   OCCT `STEPCAFControl_Reader/Writer` (XCAF für Farben/Namen), `IGESCAFControl_Reader`. Ergebnis prüfen (`BRepCheck_Analyzer`), ungültig →
   `ShapeFix_Shape` versuchen **und** Hinweis „Datei wurde repariert“ (nie still).
3. `importSolid`-Feature (Haltepunkt 1), Datei → Assets (AP-1.10 erweitern: `assets/` lesen/schreiben + Roundtrip-Test).
4. Export-Seite (AP-1.10/AP-3.8) um STEP, OBJ ergänzen; freie Körper (später) als Netz mit Hinweis (UX §6).
5. DXF/SVG einer Skizze oder ebenen Fläche: Kurven in 2D (Linien, Bögen, Splines → SVG-Pfade; DXF R12/R2000 einfache Entitäten). Eigener kleiner
   Schreiber in `src/io` (kein neues Paket), Test: Rechteck mit Loch → erwartete Elemente.
6. Menü Datei ▸ Importieren…, Drag-and-drop von Dateien ins Fenster.

## Tests
- Roundtrip intern: Export → Import → Volumen, Flächenzahl gleich (1e-6), Farben und Namen gleich.
- Fremddateien: 5 echte STEP-Dateien (frei lizenziert, z. B. aus FreeCAD-Testdaten; Quelle/Lizenz in `tests/fixtures/step/README.md`) importieren,
  gültig, Volumen plausibel.
- Akzeptanz Roundtrip über Fusion/FreeCAD: manuell (Product Owner oder FreeCAD per Kommandozeile `FreeCADCmd` auf dem Testrechner); Werte im PR.

## Abnahme
So probierst du es aus: Datei ▸ Importieren ▸ eine STEP-Datei → Körper mit Farben; Loch mit Fläche ziehen (AP-2.4) vergrößern; als STEP exportieren,
in FreeCAD öffnen.

## Nicht tun
Keine STL-Import (Netze, AP-4.1). Keine Baugruppen-Struktur aus STEP übernehmen (flach importieren, Hinweis bei Baugruppen).
