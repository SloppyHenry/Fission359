# ADR-0010: Wo Verschieben und Drehen im Rezept leben

- **Status:** angenommen (Product Owner, 2026-10-18)
- **Datum:** 2026-10-18
- **Arbeitspaket:** AP-1.6

## Kontext
AP-1.6 bringt Verschieben, Drehen und gleichmäßiges Vergrößern. Akzeptanzkriterium 2 verlangt: „Jede Transformation steht als
Rezept-Eintrag im Verlauf und ist editierbar.“ Regel 3 (alles bleibt änderbar) verbietet, eine Bewegung nur in die Geometrie zu
schreiben. Bis AP-1.5 hat jede Form die Werte `x`, `y`, `z` (Mitte der Grundfläche); eine Drehung gibt es noch nicht.

Rahmen:
- Wer aus Tinkercad kommt, verschiebt ständig. Ein Rezept, das bei jedem Zug um einen Eintrag wächst, wird unlesbar
  (Design-Prinzip: verständlich für Einsteiger).
- Verweise auf Flächen und Kanten (ADR-0003, ADR-0009) müssen eine Bewegung überstehen. ADR-0003 lässt einen Namen nur gelten,
  wenn das Element um höchstens 45° gedreht ist.
- Das Dateiformat (AP-1.10) speichert Schritte mit ihren Werten.
- Spätere Körper entstehen nicht nur aus einer Form: Vereinen und Teilen (AP-1.7), Ziehen aus Skizzen (Phase 2), Import.

## Optionen
1. **A: Die Lage gehört zum Schritt, der den Körper macht.** Verschieben und Drehen ändern die Lage-Werte dieses Schritts
   (`x`, `y`, `z`, dazu in AP-1.6b drei Winkel). Im Rezept steht weiter „Quader 40 × 20 × 10 mm“; ein Klick zeigt rechts die Lage,
   editierbar, auch mit Formeln. „Letzte Aktionen“ sagt „Quader 1 verschoben“.
   · Rezept bleibt kurz · Lage steht an einer Stelle · Formeln möglich („x = Breite + 5“) · Verweise wandern mit, weil alle
   späteren Schritte am bewegten Körper mitwandern
   · Ein Schritt, der mehrere Körper speist (Teilen), lässt sich so nicht für nur einen Teil verschieben.
2. **B: Jede Bewegung ist ein eigener Schritt „Verschoben/Gedreht“** (Züge hintereinander am selben Körper werden zusammengefasst).
   · einheitlich für jede Art Körper
   · Rezept füllt sich schnell · die Lage steht an zwei Stellen (Ablegeort in der Form, Verschiebungen danach), schwer zu
   verstehen · beim Zurückziehen des Zeitmarkers springt der Körper.
3. **C: Lage nicht im Rezept** (nur in „Letzte Aktionen“, wie Tinkercad). Verletzt Regel 3 und Akzeptanzkriterium 2; verworfen.

## Entscheidung
Wir wählen **Option A**:

1. **Lage-Werte am erzeugenden Schritt.** Jeder Schritt, der einen Körper an einen Ort setzt (Grundformen, Text, später Import und
   Ziehen aus einer Skizze), trägt seine Lage als gewöhnliche Werte: `x`, `y`, `z` (schon vorhanden) und ab AP-1.6b die Drehung.
   Verschieben ändert diese Werte, in **einem** Befehl je Zug (ein Schritt in „Letzte Aktionen“, Schlüssel `moveBody`).
2. **Kriterium 2** gilt so: Die Bewegung steht im Rezept-Eintrag des Körpers (Klick → Lage-Felder rechts), nicht als eigener Eintrag.
3. **Ausweich-Schritt nur, wo nötig.** Kann ein Körper nicht über seinen erzeugenden Schritt bewegt werden (der Schritt speist
   mehrere Körper, z. B. nach Teilen; oder er hat keine Lage), legt das Programm dort einen eigenen Schritt „Verschoben“ an. Gebaut
   wird das, wenn der erste solche Fall entsteht (frühestens AP-1.7). Ein kombinierter Körper (Vereinen) wird verschoben, indem die
   Lage aller Formen, aus denen er besteht, gemeinsam geändert wird (ein Befehl).
4. **Verweise in Körper-Koordinaten.** Weil eine Lage-Änderung den ganzen Körper starr bewegt, vergleicht die Plausibilitätsprüfung
   von ADR-0003 („gedreht ≤ 45°“, „Plätze getauscht“) Elemente in den Koordinaten ihres erzeugenden Schritts (der Name `F5/…` nennt
   ihn). Sonst würde jede Drehung um 90° fälschlich „braucht Hilfe“ auslösen. Umzusetzen mit AP-2.10; die heutige Auswahl (ADR-0009)
   sucht nur nach Namen und ist nicht betroffen.
5. **Werte bleiben Ausdrücke.** Ein Zug schreibt Zahlen (wie die Maß-Griffe); wer eine Formel in die Lage tippt, behält sie, bis er
   den Körper wieder zieht.

## Konsequenzen
- **Einfacher:** kein neuer Feature-Typ, kein neues Feld im Dokument; das Dateiformat speichert die Lage wie jeden anderen Wert.
  Undo und Neuberechnung funktionieren ohne Sonderweg.
- **Schwerer:** Jeder neue Typ, der einen Körper an einen Ort setzt, muss die Lage-Werte anbieten (Teil der Typ-Schnittstelle, wie
  die Flächen-Namen). Der Ausweich-Schritt (Punkt 3) und das gemeinsame Verschieben kombinierter Körper sind Arbeit für AP-1.7.
- **Offen für AP-1.7:** ob ein Loch mitwandert, wenn man den Körper verschiebt, aus dem es schneidet (Vorschlag: ja, wie eine
  Tinkercad-Gruppe).
- **Revisionskriterium:** Wenn in AP-1.7/Phase 2 mehr als etwa jeder fünfte Zug einen Ausweich-Schritt braucht, oder wenn
  Einsteiger im Nutzertest (AP-1.13) die Bewegung im Rezept suchen und nicht finden, prüfen wir Option B erneut.

## Belege
- Zerlegung und Abschlussvermerk von AP-1.6 in `docs/arbeitspakete/phase-1-erster-koerper.md`.
- Tests: `tests/unit/ui/test_gizmo.cpp` (ein Zug = ein Schritt, Lage-Werte geändert, Esc nimmt zurück).

## Ergänzung 2026-10-18 (AP-1.6b): Die Drehung
Entschieden vom Product Owner („ok zu allen Empfehlungen“).

1. **Drei Winkel am erzeugenden Schritt.** `rotX`, `rotY`, `rotZ` (Grad, Standard 0) sind gewöhnliche Werte jedes Schritts, der einen
   Körper an einen Ort setzt, neben `x`, `y`, `z`. Sie stehen im Panel unter „Drehung“ und dürfen Formeln sein.
2. **Feste Reihenfolge:** zuerst um die X-Achse des Betts, dann um Y, dann um Z (Matrix Rz·Ry·Rx), alle Achsen fest im Raum und durch die
   **Mitte der Hülle** der ungedrehten Form (`FeatureType::bodyCentre`). Die Lage `x, y, z` bleibt der Fußpunkt der ungedrehten Form. Eine
   Drehung flach auf dem Bett (Z) ist so immer genau ein Wert. Der Kernel dreht den fertigen Körper (`GeometryKernel::rotate`) und behält
   Flächen, Reihenfolge und Namen: „oben“ bleibt „oben“, auch wenn es nach der Seite zeigt.
3. **Ringe in Bett-Richtung.** Das Werkzeug „Drehen“ dreht um die festen Achsen des Betts (wie das Gizmo des Verschiebens); das Programm
   rechnet die drei Winkel nach jedem Zug aus der entstandenen Drehung neu (`geo::anglesOf`, die dem alten Wert nächste Darstellung, damit
   nichts springt). Nur Winkel, die sich wirklich ändern, werden zu Zahlen (Punkt 5 oben gilt weiter). Verworfen: Kardanringe, bei denen
   jeder Ring genau einen Wert ändert, aber schräg steht, sobald der Körper gedreht ist.
4. **Verweise** (Punkt 4 oben) vergleichen Elemente in Körper-Koordinaten, die Drehung ist starr und fällt damit heraus.
5. **Nach dem Drehen bleibt die Mitte stehen.** Der Körper kann schweben oder im Bett stecken; „Aufs Bett legen“ (AP-1.6e) holt ihn zurück.
   Verworfen: selbsttätiges Absetzen, weil es `z` auch dort überschriebe, wo eine Formel steht.

## Ergänzung 2026-10-18 (AP-1.6b2): Gleichmäßig vergrößern
Entschieden vom Product Owner („ok“ zu allen Empfehlungen).

1. **Ein Faktor für alle Maße, keine eigene Größe im Dokument.** Vergrößern ändert die Werte des erzeugenden Schritts: jedes Maß
   (`FeatureType::sizeParameters`: jede Länge außer der Lage, also auch Rundung, Wanddicke, Schriftgröße) wird mit dem Faktor
   multipliziert. Anzahlen (Zacken, Seiten) und die Drehung bleiben. Es gibt keinen Wert „Maßstab“ und kein Skalieren der Geometrie
   (Regel 11): Ein Quader mit 5-mm-Ecken auf 200 % hat runde 10-mm-Ecken.
2. **Formeln bleiben.** Eine Zahl wird zur neuen Zahl (eine Einheit wird dabei zu mm). Eine Formel wird zu `(Formel) * Faktor`, ein
   einzelner Name zu `Name * Faktor`; steht schon ein solcher Faktor am Ende, wird er zusammengefasst, und beim Faktor 1 steht wieder
   die Formel wie getippt (`core::scaledExpression`).
3. **Die Ecke gegenüber bleibt stehen.** Die Eck-Griffe sitzen an den oberen Ecken des Kastens um den Körper, dessen Kanten in
   Bett-Richtung laufen; beim Ziehen bleibt die gegenüberliegende untere Ecke dieses Kastens, wo sie ist. Dafür wird die Lage `x, y, z`
   mit verschoben. Das geht ohne Sonderfall auch für gedrehte Körper: Werden alle Maße mit `f` multipliziert und bleibt die Lage gleich,
   ist das Ergebnis der Körper, um den Fußpunkt der Lage mit `f` vergrößert (der Drehpunkt wächst mit); jeder andere feste Punkt ist
   dann nur eine Verschiebung dazu. Ein Lage-Wert, der sich dabei nicht ändert (die Höhe eines Körpers auf dem Bett), behält, was
   getippt wurde.
4. **Ein Zug = ein Schritt** in „Letzte Aktionen“: „Quader auf 150 % vergrößert“ bzw. „… verkleinert“ (Schlüssel `scaleBody`, die
   Prozentzahl als Wert); kein neuer Rezept-Eintrag.
