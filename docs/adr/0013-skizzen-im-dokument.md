# ADR-0013: Skizzen im Dokument

- **Status:** angenommen (2026-11-06)
- **Datum:** 2026-11-06
- **Arbeitspaket:** AP-2.1 (Teil a)

## Kontext
Eine Skizze besteht aus vielen Punkten und Kurven, später auch aus Regeln und Maßen (AP-2.2). Ein Rezept-Schritt kennt bisher nur
Werte (`params`), Wörter (`texts`), Verweise (`refs`) und Eingänge. Eine Skizze muss wie alles andere ein Schritt im Rezept sein
(Regel 3), ändern lassen sich ihre Elemente einzeln mit Undo, sie wird gespeichert (Regel 7), und Körper aus Skizzen (AP-2.3) müssen
stabile Flächennamen bekommen (ADR-0003). Sie erzeugt selbst keinen Körper.

## Optionen
1. **Eigene Struktur im Schritt (`Feature::sketch`)** · Elemente mit Ids und Zahlen, ein Undo-Schritt je Änderung, Datei lesbar und
   prüfbar · neues Feld im Dateiformat.
2. **Skizze als JSON in einem Wort (`texts`)** · kein neues Feld · jede Änderung ersetzt das ganze Wort, kein Undo je Element, keine
   Prüfung beim Laden, Verweise auf Elemente nur über Textsuche. **Verworfen.**
3. **Jedes Element ein eigener Schritt** · passt ohne neue Struktur · das Rezept hätte Hunderte Zeilen, Regeln über mehrere Elemente
   hätten keinen Ort. **Verworfen.**

## Entscheidung
Option 1.

1. **Feature-Typ `sketch`** mit neuem Feld `Feature::sketch` (`std::optional<SketchData>`, Struktur im Core ohne Qt,
   `fission/core/sketch.h`). Nur Schritte vom Typ `sketch` haben es.
2. **Ebene:** entweder ein Verweis `refs["plane"]` auf eine ebene Fläche (Skizze auf einem Körper, ADR-0003 und ADR-0010) oder das
   Wort `basePlane` (`xy` | `xz` | `yz`), dazu die Lage `x`, `y`, `z` und die Drehung `rotX`, `rotY`, `rotZ` der Ebene (wie bei
   Körpern). Die Klasse `SketchPlane { origin, uAxis, vAxis, normal }` rechnet mit `toWorld` und `toPlane` zwischen Ebenen- und
   Weltkoordinaten um; beide werden nie gemischt. `xy`: u = X, v = Y; `xz`: u = X, v = Z (Ansicht von vorn); `yz`: u = Y, v = Z
   (Ansicht von rechts). Bei einer Fläche rechnet der Schritt die Ebene selbst aus der Fläche (Teil 2.1b: Mitte, Normale, längste gerade
   Kante, `GeometryKernel::flatFace`); ein Verweis, der nicht sicher gefunden wird, macht den Schritt „braucht Hilfe“, statt die Ebene zu raten (Regel 6).
3. **Elemente:** Punkt, Linie, Kreis, Bogen, Spline (keine Ellipse). Alles ist Punkte in Ebenen-Koordinaten (u, v) in mm als `double`,
   damit ein einziger Befehl „Punkte verschieben“ alle Elemente ändert: Linie {Anfang, Ende}; Kreis {Mitte, Punkt auf dem Kreis}
   (Radius = Abstand); Bogen {Mitte, Anfang, Ende} gegen den Uhrzeigersinn (Radius = Abstand Mitte–Anfang, das Ende gibt nur die
   Richtung); Spline = durch alle Punkte (zwei oder mehr). Jedes Element hat das Kennzeichen `construction` (Hilfslinie: gestrichelt,
   bildet keine Bereiche).
4. **Ids:** Buchstabe der Art plus Nummer: `P` Punkt, `L` Linie, `C` Kreis, `A` Bogen, `S` Spline. Die Nummern zählen je Skizze und
   Art hoch (`lastNumber`), werden **nie wiederverwendet** (Entfernen gibt die Nummer nicht frei; nur das Rückgängigmachen des
   Hinzufügens gibt sie zurück, wie bei Schrittnummern) und sind beim Laden geprüft (Nummer ≤ Zähler).
5. **Regeln und Maße (AP-2.2):** eine eigene, jetzt leere Liste `rules` mit Ids `R1`, … (`SketchRule`: Id, Art, betroffene
   Element-Ids). Der Wert eines Maßes kommt mit AP-2.2 als optionales Feld hinzu und ist dann ein `Value` (Ausdruck, benannte Maße
   wirken). Ein Element zu entfernen, das eine Regel betrifft, regelt AP-2.2.
6. **Ergebnis:** Die Skizze erzeugt keinen Körper (`newBodies = 0`). `RecomputeResult::sketches` (Schritt-Id → `SketchResult` mit
   Ebene und Elementen) macht sie zu einem Ergebnis neben den Körpern. Kurven, Bereiche und Schnittpunkte kommen mit den späteren Teilen
   (2.1g). Die Skizzendaten gehen in den Schlüssel des Zwischenspeichers ein.
7. **Namens-Vertrag:** Kanten und Bereiche einer Skizze heißen nach den Element-Ids: `F7/sketch/L3` (Kante der Linie `L3`), damit
   Körper aus Skizzen stabile Flächennamen bekommen (`F8/extrude/side[L3]`, ADR-0003 Punkt 1). Ids ändern sich nie, auch wenn Punkte
   wandern.
8. **Befehle** (Handbuch §5): Element hinzufügen, Element entfernen, Punkte verschieben (viele Zwischenwerte eines Zugs fasst die
   Transaktion zu einem Schritt zusammen), Hilfslinie an/aus. Jeder ist ein Command mit Rezept-Eintrag.
9. **Dateiformat Version 3:** je Schritt vom Typ `sketch` ein Feld `sketch` (`elements`, `lastNumber`, `rules`, `lastRuleNumber`); bei
   allen anderen Schritten fehlt es. Die Migration 2 → 3 hat nichts umzuschreiben (ein fehlendes Feld ist eine leere Skizze auf der
   Grundebene `xy`); ältere Dateien öffnen weiter, Migrationen bleiben lückenlos (1 → 2 → 3). Ein Programm mit Version 2 öffnet eine
   Version-3-Datei mit Hinweis „neuere Fassung“ und kennt dann nur den Schritt-Typ nicht.

## Konsequenzen
- Skizzen sind prüfbar, einzeln änderbar mit Undo und tragen stabile Namen für spätere Körper.
- Alle Elemente als Punkte vereinfacht Verschieben, Fangen und Solver (AP-2.2 schreibt die Punkte zurück); die Rundung „Radius“ ist
  abgeleitet, ein Radius-Maß verschiebt den Randpunkt.
- Folgearbeiten: Ebene aus einer Fläche (2.1b), Kurven/Bereiche (2.1g), `Value` für Maße und Entfernen mit Regeln (AP-2.2).
- Revisionskriterium: Wenn Elemente mehr als Punkte brauchen (Ellipse, gewichtete Splines), wird `SketchElement` um Zahlenfelder
  erweitert (neue Dateiversion), die Ids und Befehle bleiben.

## Belege
Befehls- und Rückgängig-Tests: `tests/unit/core/test_sketch.cpp`, `test_history.cpp` (Zufallstest mit Skizzen);
Datei: `tests/unit/core/test_serialize.cpp`, `tests/unit/io/test_fission_file.cpp`.

## Nachtrag 2.1f (2026-11-09): übernommene Kanten, Dateiformat 4
Kanten eines Körpers lassen sich in eine Skizze übernehmen (AP-2.1, Festlegungen 45–49). Das Element bleibt mit seiner Kante verknüpft
und folgt dem Körper.

1. **Das Feld `link` im Element** (`SketchElement::link`, optional): `{input, ref}`. `input` ist die Stelle des Körpers in den Eingängen
   (`Feature::inputs`) des Skizzen-Schritts, `ref` die Stelle des Verweises im Slot `edges` der Verweise (`Feature::refs["edges"]`).
   Nur Linie, Kreis und Bogen werden verknüpft (Festlegung 46). Ein Element ohne Feld ist gewöhnlich.
2. **Stabile Stellen:** Die Liste im Slot `edges` wird nur verlängert. Löst die Person die Verknüpfung oder wird das Element entfernt, bleibt
   der Verweis an seiner Stelle liegen und wird nicht mehr nachgeschlagen (`FeatureType::refInput` liefert dafür „nicht benutzt“); Undo
   stellt Element und Verweis mit denselben Stellen wieder her. Eine Stelle wird nie neu vergeben. Der Preis sind übrig gebliebene Verweise
   in der Datei, klein und ohne Wirkung. Verworfen: Verweise bei jedem Entfernen aus der Liste nehmen (Stellen verschieben sich, jede
   Verknüpfung müsste umnummeriert werden, Undo wäre fehleranfällig).
3. **Mehrere Körper:** Der Verweis wird im Körper nachgeschlagen, den `input` nennt, nicht nur im ersten Eingang (der bleibt für die
   Ebene `refs["plane"]` zuständig). Nur Körper, die in der Reihenfolge vor dem Skizzen-Schritt fertig sind, sind wählbar (keine Zyklen,
   ADR-0010).
4. **Folgen beim Neuberechnen:** `SketchType::execute` rechnet die Punkte verknüpfter Elemente aus der Kante neu
   (`GeometryKernel::projectEdgeToSketch`) und liefert sie im `SketchResult`. Nach jedem Lauf schreibt `applyHealedRefs` die gefolgten
   Punkte als technische Pflege (wie aufgefrischte Verweise, kein Undo-Schritt) in das Dokument, damit ein Schritt, der seine Kante später
   nicht mehr findet, die zuletzt gültigen Punkte behält. Wird die Kante nicht sicher gefunden, ist der Schritt „braucht Hilfe“ (Regel 6),
   die Elemente bewegen sich nicht. Lässt sich die Kante nach einer Änderung nicht mehr exakt projizieren (der Körper wurde gekippt, ein
   Kreis würde eine Ellipse), ist das ein Fehler auf dem Schritt, nie eine Näherung.
5. **Geschützt:** Der Befehl „Punkte verschieben“ lehnt verknüpfte Elemente ab. „Verknüpfung lösen“ (`makeSetSketchLink`) ist ein eigener Befehl.
6. **Dateiformat Version 4:** neues optionales Feld `link` am Element (`{"input": n, "ref": m}`). Die Migration 3 → 4 hat nichts umzuschreiben
   (kein Feld = nicht verknüpft); 1 → 2 → 3 → 4 bleibt lückenlos. Ein Programm mit Version 3 öffnet eine Version-4-Datei mit dem Hinweis
   „neuere Fassung“, die Verknüpfung geht damit nicht still verloren, weil es die Datei nicht als gleichwertig behandelt.
