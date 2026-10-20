# AP-2.2 Skizzen-Regeln und Maße (Constraint-Solver)

**Größe:** XL, zerlegt in a–d · **Phasendatei:** AP-2.2

## Ziel
Skizzen werden präzise: Regeln (waagerecht, senkrecht, parallel, senkrecht zueinander, tangential, gleich lang, symmetrisch, Mittelpunkt,
fest, Punkt auf Kurve, konzentrisch, deckungsgleich) und Maße (Länge, Abstand waagerecht/senkrecht/schräg, Radius/Durchmesser, Winkel), die
**Ausdrücke und benannte Maße** verstehen. Regeln entstehen beim Zeichnen **automatisch** (dezente Symbole, per Klick entfernbar, ⌥
schaltet aus). Die Skizze sagt in Worten, wie viel noch frei ist („kann sich noch in 2 Richtungen bewegen“). Widersprüche werden in
Klartext erklärt, mit Ein-Klick-Lösungen. Ziehen an freier Geometrie löst live (≥ 30 Hz bei 200 Elementen).

## Voraussetzungen
AP-2.1 erledigt.

## Vorher lesen
- `docs/02-ux-konzept.md` §5 Punkte 2–5 (verbindlich: **nicht voll bestimmt ist kein Fehler**, nie rot), §12 (Regel, Maß, steht fest)
- `third_party/planegcs/README.md`, `third_party/planegcs/App/planegcs/GCS.h` (API: `GCS::System`, `addConstraint…`, `solve`, `diagnose`,
  `getConflicting`, `getRedundant`, `dofsNumber`), `tests/smoke/smoke_planegcs.cpp` (kleines Beispiel)
- FreeCAD-Sketcher als Vorbild für die **Abbildung** von Regeln auf PlaneGCS (`src/Mod/Sketcher/App/Sketch.cpp` in FreeCAD; nur lesen,
  Lizenz LGPL – nichts kopieren, was über die API-Nutzung hinausgeht)
- ADR „Skizzen im Dokument“ aus AP-2.1

## Ausgangslage im Code
PlaneGCS ist eingebunden und baut (`planegcs`-Bibliothek, darf nur von `fission_geometry_sketch` gelinkt werden). Skizzen haben Elemente
mit festen Koordinaten (AP-2.1), noch keine Regeln.

## Festlegungen
- Unterbestimmte Skizzen sind gültig, nie rot. Farben: Blau = „kann sich noch bewegen“, dunkel = „steht fest“; immer mit Worten.
- Maße sind Ausdrücke (wie überall, Regel 2).

## Haltepunkte
1. **Zerlegung** (a–d unten) bestätigen.
2. **Was passiert mit Koordinaten beim Lösen?** Empfehlung: Die gelösten Koordinaten werden im Dokument gespeichert (als Startwerte für das
   nächste Lösen, deterministisch); Regeln und Maße sind die Wahrheit. Ändert sich ein Maß, wird neu gelöst. Alternative: Koordinaten nicht
   speichern (Lösung von null hängt vom Startwert ab → nicht deterministisch, verworfen).
3. **Symbole für Regeln:** Mockup mit den 12 Regel-Symbolen zeigen lassen.

## Teil 2.2a – Solver-Anbindung hinter der GAL
1. GAL: `src/geometry/gal/include/fission/geo/sketch_solver.h`: abstrakte Schnittstelle `SketchSolver` mit
   `SolveResult solve(const SketchSystem&, const SolveOptions&)`; `SketchSystem` = Punkte (u, v), Kurven, Regeln (Art + beteiligte Ids),
   Maße (Art + Ids + Wert in mm/Grad), gehaltene Punkte (beim Ziehen). `SolveResult` = neue Koordinaten, `dof` (Freiheitsgrade),
   `conflicting` (Regel-Ids), `redundant` (Regel-Ids), `status` (ok | conflict | failed).
2. Umsetzung in `src/geometry/sketch/src/planegcs_solver.cpp`: Abbildung jeder Regel und jedes Maßes auf PlaneGCS-Constraints; Diagnose mit
   `diagnose()`, `getConflicting`, `getRedundant`; Algorithmus DogLeg (Standard), Rückfall LM bei Misserfolg.
3. Der Core ruft den Solver über die GAL (wie den Geometrie-Kern: als Teil von `ExecuteContext` oder als zweite Schnittstelle, im ADR-Nachtrag
   festhalten).
4. **50 Referenzskizzen** (Akzeptanzkriterium 3) als Daten-Dateien in `tests/fixtures/sketches/*.json`: L-Profil, Langloch, Rechteck mit
   Kreisen, Bogenketten, symmetrische Formen, unterbestimmte, überbestimmte (redundant), widersprüchliche. Test: jede konvergiert
   **deterministisch** (zweimal lösen → bitgleiche Koordinaten; auf Mac und Linux gleiche Ergebnisse innerhalb 1e-9).

## Teil 2.2b – Maße und Regeln setzen
1. Datenmodell: Regeln und Maße in `SketchData` (ADR aus 2.1), Maße als `Value` (Ausdruck). Commands + Undo + Dateiformat (neue Felder:
   Roundtrip und Migration).
2. Bedienung nach UX §5.3: **Linie anklicken, Zahl tippen** setzt ein Längenmaß; Kreis anklicken + Zahl = Durchmesser (Haltepunkt: Radius oder
   Durchmesser als Standard? Empfehlung Durchmesser, Maker denken in Ø); zwei Linien + Zahl = Abstand bzw. Winkel. Maße erscheinen als
   Bemaßung in der Skizze, Klick → Zahlenfeld.
3. Regeln über Kontextleiste der Auswahl (z. B. zwei Linien gewählt → „parallel“, „senkrecht“, „gleich lang“).
4. Neuberechnung: Skizze lösen **vor** Bereichserkennung; Ergebnis-Koordinaten gehen in die Kurven.

## Teil 2.2c – Auto-Regeln und Freiheitsgrade
1. Beim Zeichnen erkennen (mit Toleranz in Pixeln): nahezu waagerecht/senkrecht, Endpunkt auf Endpunkt, Punkt auf Kurve, tangential am
   Bogen-Übergang, gleich lang wie Nachbar (nur wenn sehr nah). Vorschau-Symbol am Zeiger; ⌥ gehalten = keine Auto-Regeln.
2. Symbole an der Geometrie, Klick → Regel entfernen (ein Undo-Schritt).
3. Freiheitsgrade aus `dof`; Hinweiszeile in Worten mit **Tipp**, welches Element ein Maß braucht (Element mit den meisten freien Richtungen;
   einfache Heuristik). Farbe je Element: frei (blau) / fest (dunkel). Ziehen an freier Geometrie: Solver mit gehaltenem Punkt, live.
4. Leistung: ≥ 30 Hz bei 200 Elementen messen (Test mit Zeitmessung, als Benchmark-Test markiert, nicht in jedem CI-Lauf, aber im PR berichtet).

## Teil 2.2d – Konfliktdiagnose
1. Bei `conflict`: Meldung (Meldungskatalog) nennt die beteiligten Regeln in Worten und bietet je Regel „entfernen“ an (UX §5.5 Beispiel
   wörtlich als Testfall: Linie A 30 mm, gleich lang wie B (40 mm) → zwei Lösungen).
2. Bei `redundant`: Hinweis (gelb, kein Fehler) „Diese Regel ist doppelt, sie folgt schon aus …“ mit „entfernen“.
3. Die Skizze bleibt im letzten gelösten Zustand, nichts springt.
4. Tests: jede Konfliktart → beteiligte Ids korrekt, ≥ 1 Lösung angeboten (Akzeptanzkriterium 2); Lösung anwenden → ok.

## Abnahme
- Kriterien 1–3 nachweisbar (Tests). Ablauf B Schritt 1 (L-Profil 60 × 40, Stärke 5 mit Maßen) als UI-Test.
- So probierst du es aus: Skizze, L-Profil mit Linien zeichnen (Regeln erscheinen von selbst), Linien anklicken und 60, 40, 5 tippen →
  „steht fest“. Eine Länge auf `Breite` (benanntes Maß) setzen. Ein widersprüchliches Maß setzen → Erklärung mit zwei Knöpfen.

## Stolperfallen
- PlaneGCS arbeitet mit Zeigern auf `double`s, die während des Lösens gültig bleiben müssen: Werte in einem stabilen Container (z. B.
  `std::deque` oder reservierter `std::vector`, nie umbauen während des Lösens).
- Winkel in PlaneGCS sind Radiant; Maße bei uns Grad.
- Determinismus: Reihenfolge der Constraints immer nach Id sortiert hinzufügen.
- Läuft in der Neuberechnung (Hintergrund); live-Ziehen braucht einen eigenen schnellen Pfad (nur Skizze lösen, keine Körper) im UI-Thread
  **nur**, wenn < 10 ms gemessen; sonst Hintergrund mit Verwerfen veralteter Anfragen.

## Nicht tun
- Kein eigener Solver. Keine 3D-Regeln. Keine Regeln zwischen verschiedenen Skizzen (später).
