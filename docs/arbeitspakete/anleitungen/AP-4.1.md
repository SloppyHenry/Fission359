# AP-4.1 Freie Körper (Netze) als vollwertige Körper

**Größe:** L, Teile a–d · **Phasendatei:** `phase-4-spielwiese.md`, AP-4.1

## Ziel
Netz-Körper (STL/3MF/OBJ-Import, später Ergebnisse von Verschmelzen und Verformen) sind vollwertige Körper: verschieben, drehen, färben, mit präzisen
Körpern kombinieren (automatisch über Tessellierung), exportieren. Beim Import werden kaputte Netze repariert oder verständlich abgelehnt. Ein Badge
„frei geformt“ erklärt den Unterschied; Werkzeuge, die exakte Kanten brauchen, sind ausgegraut **mit Begründung**. Typischer Fall: STL von Printables
importieren, Loch hinzufügen, Namen gravieren.

## Voraussetzungen
AP-1.7 (Kombinieren), AP-1.10 (Dateien, Assets).

## Vorher lesen
- `docs/02-ux-konzept.md` §6 (präzise und frei: ein Modell; verbindlich), §7.3 (Auflösung freier Körper)
- `docs/03-architektur.md` §4 (GAL: `Mesh`, gemischte Booleans über Manifold), §8 (Manifold Apache-2.0), `cmake/FissionLayers.cmake` (Manifold nur in
  `fission_geometry_mesh`)
- `tests/geometry/test_manifold.cpp` (AP-0.5 Gegenprobe)
- `src/core/include/fission/core/document.h` (`BodyKind::free` gibt es schon als Wert)

## Haltepunkte (Teil a)
1. **ADR „Freie Körper im Feature-Graph“.** Empfohlener Entwurf: Die GAL bekommt einen zweiten Körpertyp `geo::MeshBody` (undurchsichtig wie `Solid`,
   intern `manifold::Manifold`). Ein Feature-Ergebnis ist `std::variant<Solid, MeshBody>`. Operationen, die beide mischen, wandeln den präzisen
   Eingang mit Export-Qualität (0,01 mm) in ein Netz und rechnen in Manifold; Ergebnis frei. Flächennamen gibt es bei Netzen nicht; stattdessen
   „Bereiche“ (Dreiecksgruppen mit Herkunft: aus welchem Eingangskörper/welcher Fläche) – das reicht für Farben und Auswahl, nicht für exakte
   Verweise. Verweise auf Elemente freier Körper sind **nicht** erlaubt (Werkzeuge ausgegraut).
2. **Import-Reparatur:** Empfehlung: Manifold-Konstruktion aus dem Netz (verschmilzt doppelte Punkte, prüft manifold); kleine Löcher schließen
   (eigene Funktion: Randschleifen ≤ N Kanten triangulieren); Normalen ausrichten; was danach nicht manifold ist → ablehnen mit Bericht („Das Netz hat
   37 Lücken, die zu groß zum Schließen sind“). Keine neue Bibliothek. Bestätigen.

## Teil 4.1a – Netz-Körper in der GAL
1. `geo::MeshBody` + Schnittstelle `MeshKernel` (Erzeugen aus Dreiecken, Transformieren, Boolean, Volumen/Fläche/Hülle, nach `TriangleMesh` für
   die Anzeige) in `src/geometry/gal`; Umsetzung `src/geometry/mesh/src/manifold_kernel.cpp`.
2. Golden: Würfel-Netz minus Kugel-Netz (Volumen ≈ Formel, Manifold-Toleranz), Transformation, manifold-Prüfung.

## Teil 4.1b – Feature-Graph
1. Ergebnis-Variante im Core (`FeatureOutput` → Variante), Neuberechnung, Cache, Szene (Anzeige wie bisher über `TriangleMesh`).
2. `combine` (AP-1.7) mit gemischten Eingängen: präzise Eingänge tessellieren (Export-Qualität), Manifold-Boolean, Ergebnis frei.
3. Feature-Typ `importMesh` (Datei in `assets/`, Lage-Werte nach ADR-0010, Wort `unit` mm|in|cm); Badge „frei geformt“ am Körper (Liste, Panel).
4. Ausgegraut mit Erklärung (UX §6): Abrunden, Abschrägen, Fläche ziehen, Aushöhlen-Verfahren 1, Bohrung (Hinweis: „Ein Loch geht über Formen ▸ Als
   Loch“). Text aus dem Meldungskatalog: „Abrunden braucht exakte Kanten. Dieser Körper ist frei geformt. Tipp: Runde die Kante vor dem Verschmelzen
   ab (Schritt ③).“ – Schritt-Nummer aus dem Rezept einsetzen.

## Teil 4.1c – Import mit Reparatur
1. Leser STL (binär/ASCII), 3MF (lib3mf, alle Objekte; Farben übernehmen), OBJ (einfacher eigener Leser: v/f, Gruppen = Körper) in `src/io`.
2. Reparatur nach Haltepunkt 2 im Hintergrund, Bericht als Karte („3 kleine Lücken geschlossen, 1200 Normalen gedreht“).
3. Einheit: STL ohne Einheit → mm; Hüllquader < 1 mm oder > 2 m → Hinweis „War das in Zoll/Metern?“ mit Ein-Klick-Umrechnung.
4. **Testkatalog** (Akzeptanzkriterium 1): 20 reale STL-Dateien (frei lizenziert, Quelle/Lizenz in `tests/fixtures/stl/README.md`, große Dateien mit Git LFS),
   davon ≥ 5 kaputt; erwartetes Ergebnis je Datei (importiert | repariert | abgelehnt mit Code) in `expected.json`.

## Teil 4.1d – Export und Remix-Ablauf
Export STL/3MF freier Körper direkt; STEP: als Netz mit Hinweis vorher (UX §6). UI-Test für den Remix-Fall: STL importieren, Zylinder „Als Loch“,
Ergebnis frei, Volumen plausibel, Undo (Akzeptanzkriterium 2).

## Abnahme
So probierst du es aus: Datei ▸ Importieren ▸ eine STL → Körper mit Badge „frei geformt“; Zylinder als Loch darauf → Loch; Kante anklicken → Abrunden
ausgegraut mit Erklärung.

## Nicht tun
Keine Umwandlung Netz → präzise Flächen. Keine Netz-Bearbeitung (Punkte ziehen).
