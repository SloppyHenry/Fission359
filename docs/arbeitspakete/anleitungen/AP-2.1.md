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

## Festlegungen (2026-11-06)
Die Haltepunkte von Teil a sind entschieden; die Begründungen stehen in [ADR-0013](../../adr/0013-skizzen-im-dokument.md).
1. **Skizzen im Dokument:** Feature-Typ `sketch`, Feld `Feature::sketch` (`SketchData`), stabile Ids je Art (`P1`, `L3`, `C2`, `A1`,
   `S1`), Zahlen in mm in Ebenen-Koordinaten, Kennzeichen „Hilfslinie“, leere Liste für Regeln und Maße (AP-2.2), kein Körper,
   `RecomputeResult::sketches`, Namens-Vertrag `F7/sketch/L3`. Verworfen: Skizze als JSON-Wörter in `texts` (kein Undo je Element).
2. **Dateiformat Version 3** (neues Feld `sketch` je Schritt, sonst fehlt es); Migration 2 → 3 ohne Umschreiben; 1 → 2 → 3 lückenlos;
   Version-1-Fixture für den Typ `sketch`.
3. **Kein Mockup nötig:** Die Werkzeuggruppe „Skizze“ steht in `docs/mockups/oberflaeche.html`.
4. **Skizze beenden (Teil 2.1b):** Knopf „Fertig“ oben in der Ansicht und Esc, zweimal: erst das Werkzeug, dann die Skizze.
5. **Elemente sind Punkte** (Teil a, im ADR): Kreis = {Mitte, Punkt auf dem Kreis}, Bogen = {Mitte, Anfang, Ende} gegen den Uhrzeigersinn,
   Spline = durch alle Punkte. Radius und Winkel sind abgeleitet.
6. **Ebene aus einer Fläche** rechnet erst Teil 2.1b aus; bis dahin hat das Ergebnis der Skizze dann keine Ebene (nie raten, Regel 6).
7. **Einstieg (Teil 2.1b):** Knopf „Skizze“ in der Werkzeugleiste an der Stelle, die das Mockup zeigt; Befehlssuche mit Synonymen
   (DE: Skizze, zeichnen, 2D, Profil, Umriss; EN: sketch, draw, 2D, profile, outline); Hinweiszeile „Klicke eine ebene Fläche oder die
   Grundebene an“ und Tooltip-Karte (Produktregel 10). Texte DE+EN über den vorhandenen Übersetzungsweg, Glossar §12.
8. **Grundebene und Ebene aus einer Fläche:** Grundebene = Druckbett-Ebene z=0 (XY, Ursprung im Weltursprung, u=X, v=Y). Klick auf eine
   ebene Körperfläche: Der Schritt bekommt `refs["plane"]` auf die Fläche und den Körper als Eingang (`inputs`); die Ebene wird beim
   Neuberechnen aus der Fläche bestimmt und folgt dem Körper (ADR-0010). u-Achse: entlang der längsten geraden Kante der Fläche, sonst
   Projektion der Welt-X-Achse, sonst Welt-Y; v = Normale × u; Ursprung = Projektion des Flächenschwerpunkts. Ist der Verweis nicht
   auflösbar, ist der Schritt „braucht Hilfe“ (Regel 6), nie geraten. Ein Schritt `sketch` mit Eingang zählt nicht als Verbraucher des
   Körpers (`Document::consumerOf`): der Körper bleibt ein ganzer Körper mit Griffen.
9. **Gekrümmte Fläche angeklickt:** kein Schritt, nicht-modaler Hinweis aus dem Meldungskatalog: „Skizzen gehen nur auf ebenen Flächen.
   Tipp: Hilfsebenen kommen später.“ (EN entsprechend).
10. **Beenden:** Knopf „Fertig“ oben in der Ansicht und Esc (Esc beendet erst ein aktives Werkzeug, dann die Skizze; die Esc-Kurzbefehle
    in `Main.qml` schließen sich gegenseitig aus, denn zwei aktive Kurzbefehle mit derselben Taste feuern in Qt beide nicht). Eine Skizze
    ohne Elemente wird beim Beenden wieder entfernt (über einen normalen Command, sodass Undo konsistent bleibt); die Kamera kehrt
    animiert zur vorigen Ansicht zurück.
11. **Wieder öffnen:** Doppelklick auf den Skizzen-Schritt im Rezept öffnet den Skizzen-Modus erneut. Während des Skizzierens sind andere
    Körper blass (ca. 35 % Deckkraft), die Skizze kräftig, der Skizzen-Schritt im Rezept ist als aktiv markiert. Das Skizzenraster liegt
    auf der Ebene und hat den Rasterabstand der vorhandenen Bett-Raster-Einstellung (gröber, wenn es sonst enger als 6 Bildschirm-Pixel
    wäre).
12. **Zeiger und Fangen:** `interaction::planePoint(ray, plane)` (Strahl ∩ Ebene → u, v; parallel → kein Ergebnis) und die reine Funktion
    `interaction::sketchSnap(...)`: Endpunkte, Linienmittelpunkte, Kreis-/Bogenmitten, Raster, waagerecht/senkrecht zu vorhandenen
    Punkten (liefert Hilfslinien zum gestrichelten Zeichnen). Fangradius in Bildschirm-Pixeln; Vorrang Punkt > Mitte > Ausrichtung >
    Raster. Im Modus zeigt ein Punkt-Marker die gefangene Position. Zeichenwerkzeuge kommen erst in 2.1c.

## Teil 2.1a – Datenmodell und Commands (gebaut 2026-11-06)
1. `SketchData` in `src/core/include/fission/core/sketch.h` (Form im ADR-0013): `SketchElement { id, kind, points, construction }` mit Punkt, Linie,
   Kreis {Mitte, Randpunkt}, Bogen {Mitte, Anfang, Ende}, Spline {Durchgangspunkte}; `SketchPlane`; Ids zählen je Skizze und Art hoch;
   leere Liste `rules` (AP-2.2). Der Typ `sketch` steht in `src/features/src/sketch.cpp`.
2. Commands (`commands.h`): `AddSketchElementCommand`, `makeRemoveSketchElement`, `makeMoveSketchPoints` (eine Transaktion je Zug),
   `makeSetSketchConstruction`. Tests: `tests/unit/core/test_sketch.cpp`, Zufallstest in `test_history.cpp` (über `random_editor.h`).
3. Dateiformat 3 (AP-1.10): Feld `sketch` je Schritt vom Typ `sketch`; Tests in `test_serialize.cpp` und `test_fission_file.cpp`.
4. Für 2.1b: Die Ebene aus `refs["plane"]` (Fläche) fehlt im `SketchResult` noch (`plane` leer); der Schritt braucht dann einen Eingang (`inputs`) mit
   dem Körper der Fläche. Texte für „Letzte Aktionen“ (`addSketchElement`, `removeSketchElement`, `moveSketchPoints`, `sketchConstructionOn/Off`)
   und der Meldungstext für einen Fehler auf dem Skizzenschritt (Wort `basePlane`) fehlen noch.

## Teil 2.1b – Skizzen-Modus (gebaut 2026-11-06)
Gebaut: Schritte 1–5 mit den Festlegungen 7–12; die Reste aus 2.1a Punkt 4 sind erledigt (Texte für „Letzte Aktionen“, Meldungstext für
`kernelRejected` mit `basePlane`/`plane`/`sketch`). Wo es im Code liegt: `SketchController` (`src/ui`), `interaction/sketch_snap.h`, `core::planeOnFace`,
`GeometryKernel::flatFace`, `DocumentModel::addSketch` / `removeSketchIfEmpty` / `setActiveSketch`. Für 2.1c: Esc-Reihenfolge ist in
`SketchController::escape` vorbereitet (Werkzeug zuerst), `toolActive` ist noch immer falsch; Zeichenwerkzeuge hängen sich in `SketchController::onPicked` /
`pointAt` ein; die Ziffern 1–6 sind im Skizzen-Modus noch Ansichten.
Die Planung dazu:
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
- Akzeptanzkriterium 1: L-Profil mit Rundung in < 1 min – von Hand messen (Stoppuhr), Ergebnis in den PR.
- So probierst du es aus: „Skizze“, Grundebene klicken → Draufsicht mit Raster. Rechteck ziehen, Kreis darüber: drei Bereiche leuchten
  beim Überfahren einzeln. Linie, am Ende ziehen → Bogen. Fertig.

## Stolperfallen
- Ebenen-Koordinaten (u, v) und Welt-Koordinaten nie mischen; eine Klasse `SketchPlane { origin, uAxis, vAxis, normal }` mit `toWorld/toPlane`.
- Skizze auf einer Fläche eines Körpers, der sich später verschiebt (ADR-0010): die Ebene folgt über den Verweis.
- Ziffern 1–6 (Ansichten) im Skizzen-Modus: beim Tippen von Längen abfangen.

## Nicht tun
- Keine Regeln/Maße mit Solver (AP-2.2). Keine Körper aus Skizzen (AP-2.3). Keine Skizzen auf gekrümmten Flächen. Keine Text-Skizze (Text bleibt Form; Text auf Pfad ist AP-4.4).
