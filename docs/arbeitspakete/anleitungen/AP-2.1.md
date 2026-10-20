# AP-2.1 Skizzen-Modus: Zeichnen

**Größe:** L, zerlegt in a–g · **Phasendatei:** AP-2.1

## Ziel
„Skizze“ wählen, auf eine Fläche oder die Grundebene klicken: Die Ansicht dreht sich animiert senkrecht darauf, ein Skizzenraster erscheint.
Man zeichnet mit Linie, Rechteck, Kreis, Bogen, Langloch, Polygon, Spline, Punkt und Hilfslinie; bearbeitet mit Trimmen, Verlängern,
Versetzen, Ecken runden/abschrägen, Spiegeln, Muster; übernimmt Kanten von Körpern. **Geschlossene Bereiche werden automatisch erkannt**,
schattiert und sind einzeln wählbar, auch bei Überlappungen. Ein L-Profil mit Rundung gelingt ohne Regel-Kenntnisse in unter einer Minute.
(Regeln und Maße kommen mit AP-2.2; hier sind Punkte frei.)

## Voraussetzungen
AP-1.4, AP-1.6 (erledigt), AP-1.10 (Skizzen müssen gespeichert werden), AP-2.10 Teil a–c (Skizze auf einer Fläche ist ein Verweis).

## Vorher lesen
- `docs/02-ux-konzept.md` §5 (Skizzieren für Einsteiger, verbindlich), §2.2, §12 (Skizze, Hilfsebene, Regel, Maß), §15 B (Ablauf)
- `docs/03-architektur.md` §2, §4 (GAL: `sketch/`), `docs/adr/0003` (Verweise), `docs/08-gestaltung.md`, Mockup (Skizzen-Werkzeuge, falls
  vorhanden; sonst Haltepunkt)
- `src/ui/src/viewport_3d.cpp` (`lookAtFace`, Kamera-Übergänge), `src/render/include/fission/render/scene_renderer.h` (Linien zeichnen,
  `HandleGraphics`), `src/interaction/include/fission/interaction/snap.h`

## Ausgangslage im Code
- `src/geometry/sketch` ist leer (nur `layer.cpp`), PlaneGCS liegt in `third_party/planegcs` (wird in AP-2.2 angebunden).
- Feature-Daten kennen nur Werte (`params`), Wörter (`texts`), Verweise (`refs`), Eingänge. Für eine Skizze (viele Punkte, Kurven, später
  Regeln) gibt es keine Struktur.
- Die Ansicht kann senkrecht auf eine Fläche schauen (`lookAtFace`), Linien und Punkte in 3D zeichnen (Griff-Grafik).

## Haltepunkte (Teil a, vor jedem Code)
1. **ADR-0011 bzw. nächste freie Nummer: „Skizzen im Dokument“.** Empfohlener Entwurf:
   - Feature-Typ `sketch` mit neuem Feld `Feature::sketch` (Struktur `SketchData` im Core): Ebene (`refs["plane"]` = Fläche, oder Wort
     `basePlane` = xy|xz|yz, plus Lage/Drehung der Ebene), Elemente (Punkt, Linie, Kreis, Bogen, Ellipse? nein, Spline) mit **stabilen Ids**
     (`P1`, `L3`, `C2` – nie wiederverwendet, wie Feature-Nummern), Koordinaten als **Zahlen in mm in Ebenen-Koordinaten**, Kennzeichen
     „Hilfslinie“. Regeln/Maße (AP-2.2) als eigene Liste mit Ids; Maß-Werte sind **Ausdrücke** (`Value`).
   - Die Skizze erzeugt **keinen Körper** (`newBodies = 0`); ihr Ergebnis sind Kurven und Bereiche, die AP-2.3 benutzt. Dafür braucht die
     Neuberechnung ein Ergebnis „Skizze“ neben Körpern (`RecomputeResult::sketches`).
   - Namens-Vertrag: Bereiche und Kanten der Skizze heißen nach den Element-Ids (`F7/sketch/L3`), damit Körper aus Skizzen stabile
     Flächennamen bekommen (`F8/extrude/side[L3]`, ADR-0003 Punkt 1).
   Alternativen im ADR nennen (Skizze als Wörter-JSON in `texts` – verworfen, kein Undo je Element).
2. **Skizzen-Werkzeuge im Mockup?** Wenn das Mockup keine Skizzen-Oberfläche zeigt: erst ein Mockup (HTML wie `docs/mockups/oberflaeche.html`)
   bauen und freigeben lassen (Gestaltung ist verbindlich, R17).
3. **Bedienung „Skizze beenden“:** Empfehlung: Knopf „Fertig“ oben in der Ansicht + Esc (zweimal: erst Werkzeug, dann Skizze).

## Teil 2.1a – Datenmodell und Commands
1. `SketchData` in `src/core/include/fission/core/sketch.h`: `struct SketchPoint { std::string id; double u, v; bool construction; }`,
   `struct SketchCurve { std::string id; CurveKind kind; std::vector<std::string> points; /* Kreis: Mitte + Radius-Wert */ double radius;
   bool construction; std::vector<std::pair<double,double>> splinePoles; }` – genaue Form im ADR festlegen. Ids zählen je Skizze hoch.
2. Commands: Element hinzufügen, entfernen, Punkte verschieben (eine Transaktion je Zug), Hilfslinie an/aus. Undo-Tests wie
   `test_history.cpp` (Zufallsbefehle auf Skizzen).
3. Dateiformat (AP-1.10): `sketch` im JSON, Roundtrip, Migration (neues Feld!).

## Teil 2.1b – Skizzen-Modus
1. Werkzeug „Skizze“ (Palette oder Werkzeuggruppe, nach Mockup). Klick auf Fläche (eben!) oder Grundebene → neuer `sketch`-Schritt mit
   Verweis auf die Fläche; Kamera `lookAtFace` animiert; Skizzenraster auf der Ebene (Renderer: Raster wie Bett, aber auf beliebiger Ebene).
2. 2D-Zeigerposition: Strahl der Maus mit der Ebene schneiden → (u, v) in Ebenen-Koordinaten. Funktion `interaction::planePoint(ray, plane)`.
3. Fangen in der Skizze (2D): Endpunkte, Mittelpunkte, Kreismitten, Raster, waagerecht/senkrecht zu vorhandenen Punkten (gestrichelte
   Hilfslinien). Reine Funktion `interaction::sketchSnap(...)` mit Tests.
4. Gekrümmte Fläche angeklickt: Hinweis „Skizzen gehen nur auf ebenen Flächen. Tipp: Hilfsebene (kommt mit AP-2.12)“.
5. Während des Skizzierens: andere Körper blass, Skizze kräftig; Rezept zeigt den Skizzen-Schritt aktiv.

## Teil 2.1c – Grundwerkzeuge
Linie (Klick-Klick-Kette, Doppelklick/Esc beendet, Ziehen am Ende einer Linie macht einen tangentialen Bogen), Rechteck (2 Ecken; Mitte+Ecke),
Kreis (Mitte+Radius; 3 Punkte), Bogen (3 Punkte; Mitte+Start+Ende), Langloch (2 Mitten + Breite), Polygon (Mitte, Ecke, Anzahl Seiten tippbar),
Punkt, Hilfslinie (Schalter oder ⌥ beim Zeichnen; gestrichelt, bildet keine Bereiche).
- **Tippen ersetzt Ziehen** auch hier (Regel 2): Länge/Radius während des Zeichnens tippen. (Als echte Maße erst mit AP-2.2; hier setzen sie
  nur die Koordinaten.)
- Jedes Werkzeug: Regel 10 (Hinweis, Tooltip, Synonyme).
- Tests je Werkzeug: Klickfolge (simuliert über die Controller-Funktionen) erzeugt genau die erwarteten Elemente mit Koordinaten.

## Teil 2.1d – Spline
Kubischer B-Spline durch Klickpunkte (Interpolation) oder mit Kontrollpunkten (Haltepunkt: Empfehlung Durchgangspunkte, verständlicher).
Umsetzung der Kurve in `geometry/occt` (`Geom2dAPI_Interpolate`), Anzeige als feine Polylinie.

## Teil 2.1e – Bearbeiten
Trimmen (über Linien streichen: Teile zwischen Schnittpunkten löschen), Verlängern (bis zur nächsten Kurve), Versetzen (Kette, Abstand
tippbar), Ecke abrunden/abschrägen (Radius/Abstand), Spiegeln (an Linie), Muster (Reihe/Kreis). Schnittpunkte 2D über OCCT
(`Geom2dAPI_InterCurveCurve`) in einer GAL-Funktion `geo::sketchIntersections(curves)`; Versatz über `BRepOffsetAPI_MakeOffset` oder
`Geom2d_OffsetCurve`. Jede Operation ein Command, Undo-Test.

## Teil 2.1f – Kanten übernehmen
Kanten eines Körpers auf die Skizzenebene projizieren (OCCT `BRepProj_Projection` bzw. Kurven projizieren); Ergebnis sind **verknüpfte**
Elemente (Verweis auf die Kante, AP-2.10) – ändern sich mit dem Körper. Kennzeichnung im Element (`projectedFrom`).

## Teil 2.1g – Bereichserkennung
1. GAL `std::vector<SketchRegion> sketchRegions(const SketchCurves&)`: alle geschlossenen Bereiche, auch bei Überlappung einzeln
   (Rechteck und Kreis, die sich schneiden → 3 Bereiche). OCCT: alle Kanten als planare Edges, große Grundfläche mit `BRepAlgoAPI_Splitter`
   (oder `BOPAlgo_Builder`) teilen, die entstandenen Faces sind die Bereiche; Hilfslinien ausgenommen.
2. Bereiche schattiert zeichnen (Akzent, leicht), überfahren hebt hervor, Klick wählt (für AP-2.3). Bereich-Ids aus den begrenzenden
   Elementen (stabil gegen Verschieben).
3. **Golden-Tests** (Akzeptanzkriterium 2): zwei überlappende Kreise → 3 Bereiche mit richtigen Flächeninhalten (Formel für Kreis-Linse);
   Rechteck mit Kreis innen → 2 Bereiche (Ring + Kreis); Rechteck + überlappender Kreis am Rand → 3; offene Linienkette → 0; L-Profil → 1.

## Abnahme
- Akzeptanzkriterium 1: L-Profil mit Rundung in < 1 min – mit dem Product Owner messen (Stoppuhr), Ergebnis in den PR.
- So probierst du es aus: „Skizze“, Grundebene klicken → Draufsicht mit Raster. Rechteck ziehen, Kreis darüber: drei Bereiche leuchten
  beim Überfahren einzeln. Linie, am Ende ziehen → Bogen. Fertig.

## Stolperfallen
- Ebenen-Koordinaten (u, v) und Welt-Koordinaten nie mischen; eine Klasse `SketchPlane { origin, uAxis, vAxis, normal }` mit `toWorld/toPlane`.
- Skizze auf einer Fläche eines Körpers, der sich später verschiebt (ADR-0010): die Ebene folgt über den Verweis.
- Ziffern 1–6 (Ansichten) im Skizzen-Modus: beim Tippen von Längen abfangen.

## Nicht tun
- Keine Regeln/Maße mit Solver (AP-2.2). Keine Körper aus Skizzen (AP-2.3). Keine Skizzen auf gekrümmten Flächen. Keine Text-Skizze (Text bleibt Form; Text auf Pfad ist AP-4.4).
