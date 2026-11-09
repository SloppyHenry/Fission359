# AP-2.1 Skizzen-Modus: Zeichnen

**Größe:** L, zerlegt in a–g (c in c1 und c2) · **Phasendatei:** AP-2.1

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

13. **Werkzeugleiste der Skizze (2.1c1):** Im Skizzen-Modus zeigt die Werkzeuggruppe „Skizze“ (Ort und Aussehen nach Mockup; fehlt dort etwas, eine Leiste oben in der Ansicht neben „Fertig“ im Stil von `docs/08-gestaltung.md`) die Werkzeuge. Tasten nur im Skizzen-Modus: L Linie, R Rechteck, C Kreis, P Punkt, X Hilfslinie an/aus. Varianten (Rechteck 2 Ecken / Mitte+Ecke, Kreis Mitte+Radius / 3 Punkte) als kleines Auswahlmenü am Werkzeugknopf; Standard ist jeweils die erste. Nach einem fertigen Element bleibt das Werkzeug aktiv (wie Fusion); Esc beendet erst das laufende Element, dann das Werkzeug, dann die Skizze (Festlegung 10; die Esc-Kurzbefehle in `Main.qml` schließen sich gegenseitig aus).
14. **Elemente im Datenmodell (2.1c1):** Rechteck = 4 Linien in einer Transaktion (ein Undo-Schritt); Kreis = Element Kreis {Mitte, Randpunkt}; der 3-Punkte-Kreis wird zu Mitte+Randpunkt umgerechnet (kollinear: kein Kreis, Hinweis in der Hinweiszeile, kein Fehlerdialog); eine Linienkette besteht aus einzelnen Linien, deren Endpunkte exakt gleiche Koordinaten haben (gemeinsame Punkte und Regeln kommen erst mit AP-2.2). Jedes fertige Element (bzw. Rechteck) ist ein Command (Regel 3). Elemente mit Länge/Radius < 0,01 mm werden nicht angelegt.
15. **Hilfslinie (2.1c1):** Schalter in der Leiste und Taste X schalten um; gedrückte ⌥/Alt zeichnet die aktuelle Form als Hilfslinie. Hilfslinien werden gestrichelt gezeichnet (`construction = true`).
16. **Tippen ersetzt Ziehen (2.1c1, Regel 2):** Während ein Element gezogen wird, öffnet eine Ziffer (oder Minus/Komma/Punkt) das gemeinsame Zahlenfeld am Zeiger (mit Ausdrücken, Einheiten, benannten Maßen). Linie: Länge, Tab → Winkel (Grad gegen die u-Achse); Rechteck: Breite, Tab → Höhe (bei Mitte+Ecke die ganze Breite/Höhe); Kreis: Radius (wie bei Zylinder und Kugel im Programm). Beim 3-Punkte-Kreis gibt es nichts zu tippen. Enter setzt das Element. Die Werte setzen nur Koordinaten (echte Maße erst AP-2.2).
17. **Ziffern 1–6 im Skizzen-Modus (2.1c1):** Solange ein Werkzeug aktiv ist, gehen Ziffern ins Zahlenfeld (oder werden verschluckt, wenn nichts zu tippen ist), nie an die Ansichten. Ohne aktives Werkzeug bleiben sie Ansichten.
18. **Vorschau (2.1c1):** Das entstehende Element wird beim Bewegen live gezeichnet (kräftig, mit Länge/Radius als kleiner Zahl am Zeiger); gefangen wird über `sketchSnap` (Festlegung 12), Shift schaltet das Fangen aus.
19. **Regel 10 (2.1c1):** Jedes Werkzeug hat Hinweiszeilen-Text, Tooltip-Karte und Synonyme für die Befehlssuche (DE+EN, Glossar §12): Linie (Strich, Kante zeichnen / line, segment), Rechteck (Viereck, Kasten / rectangle, box outline), Kreis (Rund, Loch-Umriss / circle, round), Punkt (Markierung / point, mark), Hilfslinie (Konstruktion / construction, helper line).

## Festlegungen Teil 2.1c2 (2026-11-08)
20. **Bogen (2.1c2):** Taste A. Varianten: „3 Punkte“ (Anfang, Ende, dann ein Punkt auf dem Bogen; Standard) und „Mitte, Anfang, Ende“. Im Datenmodell immer {Mitte, Anfang, Ende} gegen den Uhrzeigersinn (Festlegung 5): wurde im Uhrzeigersinn gezogen, werden Anfang und Ende getauscht. Kollineare 3 Punkte → kein Bogen, Hinweis `collinear` wie beim Kreis. Tippen: „3 Punkte“ nach dem Ende-Klick den Radius (die Seite folgt dem Zeiger; Radius kleiner als die halbe Sehne → Hinweis `tooSmall`, kein Bogen); „Mitte, Anfang, Ende“ nach dem Anfang den Öffnungswinkel in Grad (positiv = gegen den Uhrzeigersinn).
21. **Tangentialer Bogen (2.1c2):** Im Werkzeug Linie: Maus am Ende der zuletzt gesetzten Linie der Kette drücken und ziehen (mehr als 6 Bildschirm-Pixel) → Vorschau eines Bogens, der tangential an die Linie anschließt; Loslassen setzt ihn (ein Element, ein Command), die Kette geht am Bogenende als Linie weiter. Tippen während des Ziehens: Radius. Ohne vorige Linie gibt es keinen tangentialen Bogen (normales Klicken).
22. **Langloch (2.1c2):** Taste S (engl. slot). Ablauf: Mitte 1 klicken, Mitte 2 klicken, dann die Breite mit dem Zeiger (oder tippen). Im Datenmodell 2 Linien + 2 Halbkreis-Bögen in einer Transaktion (ein Undo-Schritt), Endpunkte exakt gleich. Tippen: nach Mitte 1 die Länge (Mittenabstand), Tab → Winkel; nach Mitte 2 die Breite (ganze Breite, nicht Radius). Breite < 0,01 mm oder Länge < 0,01 mm → nicht angelegt.
23. **Polygon (2.1c2, Glossar „Polygon“):** Taste G. Ablauf: Mitte klicken, dann eine Ecke. Varianten am Pfeil: „Ecke außen“ (Umkreis, Standard) und „Seitenmitte“ (Inkreis). Seitenzahl: Standard 6, während des Ziehens Tab → Feld „Seiten“ (ganze Zahl 3–64, andere Werte → Hinweis, alter Wert bleibt); die zuletzt benutzte Zahl gilt bis zum Ende der Skizze. Tippen: Radius (Umkreis bzw. Inkreis je Variante), Tab → Seiten, Tab → Drehwinkel. Im Datenmodell n Linien in einer Transaktion.
24. **Regel 10 (2.1c2):** Hinweis, Tooltip-Karte, Synonyme DE+EN: Bogen: Kreisbogen, Rundung zeichnen / arc, curve segment; Tangentialer Bogen (Hinweis im Linien-Werkzeug: „Am Linienende ziehen ergibt einen Bogen“); Langloch: Schlitz, Nut, Langes Loch / slot, oblong, obround; Polygon: Vieleck, Sechseck, Mehreck / polygon, hexagon, n-gon.
25. **Tests (2.1c2):** wie 2.1c1 je Werkzeug und Variante über `SketchTool`/`SketchController`: Uhrzeigersinn-Tausch beim Bogen, kollinear, Radius kleiner als halbe Sehne, tangentialer Bogen (Tangente stimmt auf 1e-9, Kette geht weiter), Langloch = 4 Elemente in einem Undo-Schritt mit geschlossenen Enden, Polygon 3/6/64 Seiten in beiden Varianten, ungültige Seitenzahl; Fenster-Test „Langloch und Polygon tippen“ und „Linie mit tangentialem Bogen“ (nur CI).

## Festlegungen Teil 2.1d (2026-11-09)
26. **Kurve (2.1d, Glossar „Kurve (Spline)“):** Taste K. Die Kurve geht **durch die geklickten Punkte** (Interpolation, kubisch, Haltepunkt entschieden: Durchgangspunkte sind verständlicher als Kontrollpunkte). Klick setzt den nächsten Punkt, die Vorschau zeigt die Kurve bis zum Zeiger. Doppelklick oder Enter setzt die Kurve (mindestens 2 Punkte; ein Element, ein Command). Esc verwirft die unfertige Kurve (Festlegung 13: erst das laufende Element), Rückschritt-Taste (⌫) nimmt den letzten Punkt zurück. Ein Klick auf den ersten Punkt (im Fangradius) schließt die Kurve und setzt sie.
27. **Geschlossene Kurve ohne Formatwechsel:** Im Datenmodell ist eine geschlossene Kurve ein Spline, dessen letzter Punkt exakt dem ersten gleicht; dann wird periodisch interpoliert (glatt ohne Knick am Anfang). Kein neues Feld, kein Dateiformatwechsel.
28. **Rechnen und Zeichnen:** Die Kurve rechnet die GAL (`geo::sketchSplineCurve`/`sketchSplinePolyline` o. ä. in `src/geometry`, OCCT nur in `src/geometry/occt`, `Geom2dAPI_Interpolate`), nicht die UI. Angezeigt wird eine Polylinie mit adaptiver Unterteilung, Abweichung zur echten Kurve ≤ 0,01 mm (Regel 12). Zwei Punkte ergeben eine gerade Kurve; doppelte aufeinanderfolgende Punkte werden beim Setzen entfernt; weniger als 2 verschiedene Punkte → nichts angelegt (`tooSmall`). Lehnt der Kernel ab, Hinweis aus dem Meldungskatalog, kein roher Fehler.
29. **Fangen und Tippen:** Die Punkte fangen wie alle anderen (Festlegung 12), Kurven-Durchgangspunkte sind danach selbst Fangpunkte (Endpunkte und Zwischenpunkte). Tippen: Länge und Winkel des nächsten Abschnitts (Sehne vom letzten Punkt), wie bei der Linie.
30. **Regel 10 (2.1d):** Hinweis, Tooltip-Karte, Synonyme DE+EN: Kurve: Spline, Freiform, Schwung, geschwungene Linie / spline, curve, freeform. Hinweiszeile nennt Doppelklick/Enter, ⌫ und Klick auf den ersten Punkt.
31. **Tests (2.1d):** Unit-Tests der GAL-Funktion (Kurve geht exakt durch alle Punkte, offen/geschlossen, geschlossen ist am Übergang glatt: Tangenten gleich auf 1e-9, Polylinien-Abweichung ≤ 0,01 mm bei engen Bögen, 2 Punkte = Gerade, doppelte Punkte), Zustandsautomat (Doppelklick, Enter, Esc verwirft, ⌫, Schließen am ersten Punkt, Tippen), Controller (ein Undo-Schritt). Fenster-Test „Kurve klicken und schließen“ (nur CI).

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

## Teil 2.1c – Grundwerkzeuge (geteilt, Entscheidung 2026-11-06)
- **2.1c1 (dieser Teil):** Linie (Klick-Klick-Kette; Doppelklick oder Esc beendet die Kette), Rechteck (2 Ecken; Mitte+Ecke), Kreis (Mitte+Radius; 3 Punkte), Punkt, Hilfslinie. Festlegungen 13–19.
- **2.1c2 (gebaut 2026-11-08):** Bogen (3 Punkte; Mitte+Start+Ende), tangentialer Bogen durch Ziehen am Linienende, Langloch, Polygon. Festlegungen 20–25.

Ursprüngliche Beschreibung für beide Teile:
Linie (Klick-Klick-Kette, Doppelklick/Esc beendet, Ziehen am Ende einer Linie macht einen tangentialen Bogen), Rechteck (2 Ecken; Mitte+Ecke),
Kreis (Mitte+Radius; 3 Punkte), Bogen (3 Punkte; Mitte+Start+Ende), Langloch (2 Mitten + Breite), Polygon (Mitte, Ecke, Anzahl Seiten tippbar),
Punkt, Hilfslinie (Schalter oder ⌥ beim Zeichnen; gestrichelt, bildet keine Bereiche).
- **Tippen ersetzt Ziehen** auch hier (Regel 2): Länge/Radius während des Zeichnens tippen. (Als echte Maße erst mit AP-2.2; hier setzen sie
  nur die Koordinaten.)
- Jedes Werkzeug: Regel 10 (Hinweis, Tooltip, Synonyme).
- Tests je Werkzeug: Klickfolge (simuliert über die Controller-Funktionen) erzeugt genau die erwarteten Elemente mit Koordinaten.

### Teil 2.1c1 – gebaut 2026-11-06
Gebaut: Festlegungen 13–19. Wo es im Code liegt: Zustandsautomat `interaction::SketchTool` (`src/interaction`, reine Logik: Klick, Zeiger, Tippen →
Vorschau und fertige Elementgruppen), verdrahtet in `SketchController` (`chooseTool`, `clickAt`, `doubleClick`, `setTyped`/`acceptTyped`, `toggleHelperLine`);
je Elementgruppe ein Command-Schritt über `DocumentModel::addSketchElements` (eine Transaktion, Rezeptzeile „… zur Skizze hinzugefügt“, Rechteck = ein Schritt).
Oberfläche: `SketchToolCatalog.qml` (Name, Tooltip, Hinweis, Synonyme; von `check_commands.py` geprüft), `SketchFlyout.qml` (neben der Gruppe „Skizze“,
Pfeil am Knopf öffnet die Varianten), `SketchLayer.qml` (Zahlen am Zeiger, Tippen mit Tab/Enter/Esc), Tasten L/R/C/P/X nur im Skizzen-Modus
(P ist dort Punkt, sonst Ansicht Perspektive/Parallel), Ziffern gehen bei aktivem Werkzeug ins Zahlenfeld. Neue `--simulate`-Aktionen: `sketchmove`, `sketchclick`,
`sketchdouble`, `expectsketchtool`, `expectsketchelements`, `expectsketchsize`, `expectsketchtyping` (Menü-Ids `sketchtool.line|rectangle|circle|point|helper`).
Weiter geltende Annahmen: Klick und Doppelklick kommen als eigene Signale der Ansicht (`leftClicked`, `leftDoubleClicked`), nicht über den Auswahl-Durchlauf; der
3-Punkte-Kreis behält bei kollinearen Punkten die ersten zwei Punkte (ein anderer dritter geht); beim 3-Punkte-Kreis gibt es nichts zu tippen (Ziffern werden
verschluckt); bei Rechteck Mitte+Ecke sind getippte Breite/Höhe die ganzen Maße; getippte Werte gehen auf die Seite des Zeigers, negative Werte kehren sie um.
Offene Reste für 2.1c2: Bogen (3 Punkte; Mitte+Start+Ende), tangentialer Bogen durch Ziehen am Linienende (die Linienkette weiß das Ende der letzten Linie:
`SketchTool::placed_`), Langloch, Polygon (Seitenzahl tippbar) – jeweils als weitere Werte von `SketchToolKind` mit eigener Form in `SketchTool::shape`; Werkzeug-Katalog,
Flyout und Tasten (A Bogen, S Langloch, G Polygon als Vorschlag) in derselben Weise ergänzen. Fenster-Tests: `app_sketch_rectangle`, `app_sketch_line_circle`,
`app_sketch_helper_line`, `app_sketch_digits` (laufen nur in der CI).

### Teil 2.1c2 – gebaut 2026-11-08
Gebaut: Festlegungen 20–25. Wo es im Code liegt: die Geometrie als reine Funktionen in `src/interaction/include/fission/interaction/sketch_shapes.h`
(`arcThroughThreePoints`, `arcOverChord`, `arcBySweep`, `tangentArc`, `slotElements`, `polygonLines`; `circleThroughThreePoints` zog dorthin um). Der Zustandsautomat
`interaction::SketchTool` kennt `SketchToolKind::arc | slot | polygon`; neu sind `press`/`release`/`dragging` (tangentialer Bogen am Ende der Linienkette,
`lastDirection_`), `setTyped` liefert einen Hinweis (`badSides`), `polygonSides`. Im Controller: `pressAt`/`dragTo`/`releaseAt` (der Ansicht kommen die neuen Signale
`leftPressed`/`leftDragged`/`leftReleased` und `Viewport3D::setDrawDrag`), Eigenschaft `pullingArc`. Oberfläche: drei neue Einträge in `SketchToolCatalog.qml`
(Tasten A, S, G; Varianten Bogen „3 Punkte / Mitte, Anfang, Ende“, Polygon „Ecke / Seitenmitte“), Symbole `sketcharc|sketchslot|sketchpolygon`, Feld „Seiten“
in `SketchLayer.qml`. Neue `--simulate`-Aktion `sketchdrag:x1,y1,x2,y2[,hold]`; Menü-Ids `sketchtool.arc|slot|polygon`. Fenster-Tests (nur CI): `app_sketch_slot_polygon`,
`app_sketch_tangent_arc`.
Weiter geltende Annahmen: Der Bogen „3 Punkte“ mit getipptem Radius ist immer der kürzere Bogen (Mitte liegt auf der abgewandten Seite), die Seite folgt dem Zeiger (genau auf
der Sehne gilt links); beim Bogen „Mitte, Anfang, Ende“ verfolgt der Automat die Drehung des Zeigers stückweise (über 180° hinaus, beide Richtungen, höchstens knapp 360°);
getippte 360° oder mehr sind „zu klein“. Beim tangentialen Bogen gilt als „Ziehen“ ein Abstand über 6 Bildschirm-Pixeln vom Druckpunkt, der Druck muss innerhalb des Fangradius
am Ende der letzten Linie liegen; zielt der Zeiger genau in Richtung der Linie, kommt der Hinweis `collinear` (der Text sagt jetzt „Punkte auf einer Geraden“); die Kette geht nach einem
Bogen in der Richtung weiter, in der er endet, auch ein weiterer Bogen schließt an. Esc während des Ziehens verwirft nur den Bogen. Das Langloch fragt nach dem zweiten Klick die
ganze Breite (doppelter Abstand des Zeigers von der Mittellinie); Enter nach der getippten Länge setzt die zweite Mitte (Feld wird danach leer). Polygon: Drehwinkel getippt gilt
für die Ecke (Variante Ecke) bzw. die Seitenmitte (Variante Seitenmitte); die Seitenzahl bleibt bis zum Ende der Skizze (`beginOn` setzt auf 6 zurück). Rezeptzeile eines
einzelnen Bogens heißt immer „Bogen“, auch wenn das Linienwerkzeug ihn zeichnete; ein Langloch/Polygon heißt „Langloch“/„Polygon“.
Offene Reste für 2.1d/e: Bögen, Langlöcher und Polygone haben noch keine gemeinsamen Punkte oder Regeln (AP-2.2: Tangentialität bleibt als Regel „tangential“ zu
ergänzen, jetzt nur geometrisch); Trimmen/Versetzen/Spiegeln müssen mit Bögen umgehen (Bogen = {Mitte, Anfang, Ende} gegen den Uhrzeigersinn); der Spline (2.1d) hängt sich als
weiteres `SketchToolKind` in `SketchTool::shape` ein; Symbole der drei Werkzeuge sind einfache Umrisse und sollten mit der Gestaltung (`docs/08-gestaltung.md`) abgeglichen werden.

## Teil 2.1d – Kurve (Spline), gebaut 2026-11-09
Gebaut: Festlegungen 26–31. Wo es im Code liegt: Die Kurve rechnet der Kern: `GeometryKernel::sketchSpline(punkte, toleranz)` (`src/geometry/gal/include/fission/geo/sketch_curve.h`,
`SketchSplineCurve`: Polylinie, Anfangs- und Endrichtung, `closed`), Umsetzung `src/geometry/occt/src/sketch_curve.cpp` (`Geom2dAPI_Interpolate`, Parameter nach Sehnenlänge, periodisch,
wenn der letzte Punkt dem ersten gleicht; die Durchgangspunkte sind genau Ecken der Polylinie; Unterteilung, wo die Kurve mehr als die halbe Toleranz von der Sehne abweicht; Prüfung, dass die
Kurve wirklich durch alle Punkte geht; `Standard_Failure` wird `GeometryError(kernelFailure)`). Zustandsautomat: `SketchToolKind::spline` in `interaction::SketchTool` (Klick = Punkt, Doppelklick/Enter
setzt, `undoPoint()` für ⌫, Klick auf den ersten Punkt schließt, `placedPoints()`). Controller: `SketchController::removeLastPoint`, die Punkte der laufenden Kurve fangen mit (der erste schließt), kleiner
Zwischenspeicher der berechneten Polylinien (`curveOf`, damit Kamerabewegungen den Kern nicht fragen); `DocumentModel::sketchSplineLine` (Anzeige) und die Prüfung in `addSketchElements` (der Kern
lehnt ab → nichts angelegt, Hinweis `curveFailed`). Oberfläche: Eintrag `spline` in `SketchToolCatalog.qml` (Taste K, Symbol `sketchspline`), Hinweise, Hinweis `curveFailed` im Meldungskatalog,
Rückschritt-Taste und Enter als eigene Kurzbefehle nur beim Zeichnen einer Kurve (Rückschritt ist sonst „Löschen“, die beiden Kurzbefehle schließen sich gegenseitig aus; die Taste K hat sonst keinen
Kurzbefehl, nur Strg/Cmd+K ist die Befehlssuche). Das Wort „Kurve“/„curve“ gehört jetzt dem Werkzeug Kurve, nicht mehr dem Bogen (Synonyme des Bogens angepasst). Fenster-Test (nur CI):
`app_sketch_curve_close`.
Weiter geltende Annahmen: Anzeige- und Prüftoleranz der Polylinie 0,005 mm (unter den 0,01 mm der Regel 12); Enter mit getippter Länge/Winkel setzt den nächsten Punkt (die Kurve geht weiter), Enter ohne
Tippen setzt die Kurve durch die bisherigen Punkte (der Zeiger zählt nicht); nach einem Doppelklick auf demselben Punkt wie der letzte wird der doppelte Punkt nur einmal genommen; ein Klick auf den ersten Punkt
mit nur zwei gesetzten Punkten schließt nicht (Hinweis `tooSmall`); eine geschlossene Kurve braucht drei verschiedene Punkte; Fangen aus (Umschalt) schließt auch nicht, weil das Schließen über das Fangen
des ersten Punkts läuft; die Hilfslinie (Schalter/Alt) gilt auch für die Kurve (gestrichelt entlang der Polylinie). Die Punkte einer fertigen Kurve lassen sich noch nicht verschieben (kein Werkzeug dafür).
Offene Reste für 2.1e–g: Trimmen, Verlängern, Versetzen, Spiegeln und Muster müssen Splines kennen (Schnittpunkte Kurve/Linie über OCCT, `Geom2dAPI_InterCurveCurve` auf der interpolierten Kurve; Versatz einer
Kurve ergibt keine Interpolationskurve durch neue Punkte, sondern wieder eine Punktfolge, die der Kern neu berechnen muss: dafür braucht die GAL dann eine Funktion „Kurve auswerten bei Parameter“ oder die
Polylinie selbst); Spiegeln/Muster einer Kurve sind einfach (Punkte spiegeln/verschieben), ohne Formatwechsel. 2.1f: projizierte Kanten, die Splines sind, brauchen eine Umrechnung in Durchgangspunkte (aus der
Polylinie ausdünnen). 2.1g: Bereiche brauchen die Kurve als Kante (`sketchSpline().polyline` oder die B-Spline selbst); die Kurve einer Skizze wird bisher nur zum Zeichnen berechnet, nicht im
`SketchResult`. Das Symbol `sketchspline` ist ein einfacher Umriss und sollte mit `docs/08-gestaltung.md` abgeglichen werden.

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
