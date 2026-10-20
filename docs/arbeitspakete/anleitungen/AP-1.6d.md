# AP-1.6d Abstandsmaße

**Größe:** M · **Branch:** `ap-1.6d-abstandsmasse` · **Phasendatei:** AP-1.6, Teil d

## Ziel
Beim Verschieben zeigt das Programm automatisch, wie weit der Körper von den nächsten Körpern und vom Bettrand entfernt ist, als
Maßbänder in der 3D-Ansicht. Ein Klick auf eine Zahl macht sie editierbar: Tippt man `5`, steht der Körper danach genau 5 mm vom
Rand. Das ersetzt das Tinkercad-Lineal (UX §2.3) und erfüllt das Akzeptanzkriterium „Eine Form lässt sich per Abstandsmaß exakt
5 mm vom Rand platzieren“.

## Voraussetzungen
AP-1.6c2 erledigt (gleiche Controller-Muster, gemeinsamer ⇧-Schalter).

## Vorher lesen
- [Handbuch](handbuch.md) §5, §7, §9, §10
- `docs/02-ux-konzept.md` §2.3 (Intelligente Abstandsmaße), `docs/08-gestaltung.md` (Maßbänder, Wertblase, Farben)
- `src/ui/src/gizmo_controller.cpp` (wie ein Zug beginnt, `moveTo`, Grafik in `refresh`), `GizmoLayer.qml` (Beschriftungen und
  Anfassflächen über der 3D-Ansicht)
- `src/ui/src/handle_controller.cpp`: wie Maßbänder (`extent`) gezeichnet werden und wie ein Band-Etikett zum Eingabefeld wird
  (`beginEdit`, `setValue`, `commitEdit`), `HandleLayer.qml`
- `src/ui/include/fission/ui/document_model.h`: `beginMove`, `moveCurrent`, `currentPosition`, `currentBodyBounds`, `commitChange`

## Ausgangslage im Code
- Verschieben schreibt die Lage-Werte x, y, z des Schritts (ADR-0010). Ein Zug ist ein Undo-Schritt „Quader verschoben“.
- Maßbänder kann der Renderer schon zeichnen (Bänder der Maß-Griffe: Linie, Hilfslinien, Pfeilspitzen, tiefenrichtig über
  `render::HandleGraphics`). Beschriftungen, die man anklicken und tippen kann, gibt es in `HandleLayer.qml`.
- Hüllquader: eigener Körper exakt über `DocumentModel::currentBodyBounds()`; andere Körper aus ihren Anzeige-Netzen
  (`render::RenderScene`, jedes Netz hat Punkte, daraus min/max).

## Festlegungen
- Abstand **von Kante zu Kante** (Seite des eigenen Hüllquaders zur zugewandten Seite des Hüllquaders des Nachbarn), nicht Mitte
  zu Mitte (Product Owner, 2026-10-18).
- Gedrehte Körper: es zählt der achsparallele Hüllquader (wie beim Fangen).
- Bänder nur in Bett-Richtung (±X, ±Y). Die Höhe über dem Bett zeigt die Lage z im Panel; kein eigenes Band.

## Haltepunkte (am Anfang fragen)
1. **Wann sind die Abstandsmaße zu sehen?** Empfehlung: im Werkzeug „Verschieben“, solange ein Körper gewählt ist, und während
   jedes Verschiebe-Zugs (Pfeil, Ebene, Körper ziehen); nicht in „Größe“, „Drehen“, „Wählen“. Folge: Sie sind anklickbar, auch wenn
   man gerade nicht zieht. Alternative: nur während des Zugs (dann kann man nicht klicken, das Kriterium wäre nicht erfüllt).
2. **Wie viele Bänder?** Empfehlung: je Richtung (links, rechts, vorne, hinten) das **nächste** Hindernis, höchstens 4 Bänder.
   Fehlt in einer Richtung ein Körper, zeigt das Band den Abstand zum Bettrand. Liegt der Körper außerhalb des Betts, kein Band in
   diese Richtung.

## Schritte

### Schritt 1 – Reine Rechnung (ohne Fenster)
Neue Datei `src/interaction/include/fission/interaction/distances.h` + `src/interaction/src/distances.cpp`:
```cpp
struct Box3 { Vec3 low, high; };                      // Hüllquader (mm), Vec3 aus snap.h
enum class Side { left, right, front, back };          // −X, +X, −Y, +Y
struct Gap {
    Side side;
    double distance;        // mm, ≥ 0; Kante zu Kante
    Vec3 from, to;          // Endpunkte des Bandes in der Welt (z = Höhe, auf der das Band liegt)
    bool toBed;             // true: zum Bettrand, false: zu einem Körper
    std::string otherBody;  // leer beim Bettrand
};
std::vector<Gap> nearestGaps(const Box3& mover, const std::vector<std::pair<std::string, Box3>>& others,
                             double bedSizeX, double bedSizeY);
```
Regeln (Schreibweise unten: `minX` = `low[0]`, `maxX` = `high[0]`, `minY` = `low[1]` usw.):
- Ein anderer Körper liegt „links“, wenn `other.maxX <= mover.minX` und sich die Grundrisse in Y überlappen (Überlappung > 0).
  Abstand = `mover.minX − other.maxX`. Nächster = kleinster Abstand. Entsprechend für rechts, vorne (−Y, Überlappung in X), hinten.
- Überlappen sich Körper (Abstand < 0), zählt dieser Körper nicht als Hindernis in dieser Richtung.
- Kein Körper in einer Richtung: Abstand zum Bettrand (Bett von −bedX/2 bis +bedX/2, Ursprung in der Mitte). Steht der Körper
  über den Rand hinaus, kein Band.
- Das Band liegt auf halber Höhe der Überlappung (bei Körpern) bzw. auf z = 0,2 mm über dem Bett (beim Bettrand) und in der Mitte
  des überlappenden Bereichs der anderen Achse.
- `cleanedCoordinate` auf die Abstände anwenden (Rauschen der Netze).
Datei in `src/interaction/CMakeLists.txt` eintragen.

### Schritt 2 – Abstand setzen (rein)
Funktion `std::optional<Vec3> offsetForDistance(const Gap& gap, double wanted)`: die Verschiebung (nur X oder nur Y), die den
Abstand dieses Bandes auf `wanted` bringt:
- links (Hindernis bei −X): `Δx = wanted − gap.distance` (7 → 5 ergibt −2: der Körper rückt nach links, näher heran)
- rechts (+X): `Δx = gap.distance − wanted`
- vorne (−Y): `Δy = wanted − gap.distance`
- hinten (+Y): `Δy = gap.distance − wanted`
`wanted < 0` oder keine Zahl → nichts (kein Ineinanderschieben über ein Abstandsmaß).

### Schritt 3 – Controller
Neue Klasse `ui::DistanceController` (`src/ui/include/fission/ui/distance_controller.h`, `src/ui/src/distance_controller.cpp`),
Vorbild `ScaleController`:
- Eigenschaften für QML: `labels` (Liste `{ side, x, y, value, toBed }` in Ansichtspixeln, Wert in mm), `editing`.
- `attach(Viewport3D*)`, `setEnabled(bool)` (QML schaltet nach Werkzeug und Auswahl, Haltepunkt 1).
- `refresh()`: Hüllquader des gewählten Körpers (`currentBodyBounds`), der anderen Körper (aus `m_model->scene()`, eigenen Körper
  über `bodyOfStep(currentStep)` auslassen, nur sichtbare, keine „Als Loch“-Körper), Bettgröße aus `DocumentModel::bedX/bedY`.
  `nearestGaps` → Grafik: je Band eine Linie mit Pfeilspitzen an beiden Enden und kurzen Hilfslinien (gleiche Bausteine wie die
  Maßbänder in `HandleController`), Farbe neutral (`Theme`-Token für Maße, siehe `08-gestaltung.md`). Eigene Quelle
  `Viewport3D::HandleSource::distances` (Enum erweitern, siehe 1.6c2 Schritt 5).
- Aktualisieren bei: `sceneReady`, `currentStepChanged`, Kamera bewegt (Projektion der Etiketten), während eines Zugs des Gizmos
  (Signal von `GizmoController`, z. B. `offsetChanged`).
- `Q_INVOKABLE bool setDistance(int index, const QString& text)`: Text mit `Numbers` auswerten (Formeln, Einheiten, Komma),
  `offsetForDistance`, dann `m_model->beginMove()`, `moveCurrent(neue Lage)`, `commitChange()`. Ein Undo-Schritt. Bei ungültigem
  Text false (Feld wird rot, nichts passiert).
- In `main.cpp` als Singleton `Distances` anmelden.

### Schritt 4 – QML
Neue Datei `apps/fission3d/qml/DistanceLayer.qml` (in `FISSION_QML_FILES` eintragen), Vorbild Beschriftungen in `HandleLayer.qml`:
- Je Etikett ein kleines Kästchen mit der Zahl („12,5“), Klick → Eingabefeld (dieselbe `NumberField`-Komponente, Regel 2), Enter
  übernimmt (`Distances.setDistance`), Esc bricht ab, `Tab` springt zum nächsten Band.
- Etiketten weichen der Werkzeugleiste und anderen schwebenden Elementen aus (`clearplace.js`, wie die Griff-Beschriftungen).
- Ein Etikett, dessen Band hinter einem Körper liegt, tritt zurück (blass).
- Einbinden in `Main.qml` neben `GizmoLayer`. Sichtbar nach Haltepunkt 1.

### Schritt 5 – Simulieren und Akzeptanztest
- Neue `--simulate`-Aktion `distance:<seite>,<wert>` (Seite `left|right|front|back`), die `Distances.setDistance` mit dem Etikett
  dieser Seite aufruft (Handbuch §9).
- Fenster-Test `app_distance_five_from_edge`: Quader ablegen, `tool:move`, `distance:left,5`, dann `near:x,<erwartet>` mit
  erwartet = `−bedX/2 + 5 + Breite/2` für das Standardbett (256 mm: x = −128 + 5 + 10 = −113 bei 20 mm Breite). Das ist das
  Akzeptanzkriterium 1 als automatischer Ablauf.

### Schritt 6 – Texte
- Tooltip/Hinweis: Hinweiszeile „Verschieben“ ergänzen („Click a distance to type it.“). Übersetzen (Handbuch §8).
- Phasendatei: Kriterium 1 abhaken, wenn der Test grün ist.

## Tests
Unit (`tests/unit/interaction/test_distances.cpp`, Tag `[distance]`, in `tests/unit/CMakeLists.txt` eintragen):
- Ein Körper links mit 7 mm Lücke und Y-Überlappung → Band links, Abstand 7.
- Zwei Körper links (3 mm und 9 mm) → der mit 3 mm.
- Körper links, aber ohne Y-Überlappung → zählt nicht, Band zum Bettrand.
- Kein Nachbar → vier Bänder zum Bettrand mit richtigen Werten.
- Überlappende Körper → kein Band zu diesem Körper.
- Körper über den Bettrand hinaus → kein Band in diese Richtung.
- `offsetForDistance` für alle vier Seiten: Abstand 7 auf 5 setzen, danach mit `nearestGaps` neu messen → genau 5 (so prüft der
  Test das Vorzeichen über das Ergebnis, nicht über Δ). Ebenso 7 auf 12.
UI (`tests/unit/ui/test_distances.cpp`):
- Abstand setzen ist ein Undo-Schritt „Quader verschoben“; Undo stellt die alte Lage her.
- Eine Formel in y bleibt erhalten, wenn nur x geändert wird.
- Ungültiger Text („abc“) ändert nichts.

## Abnahme
- Akzeptanzkriterium 1 als Fenster-Test grün; CI grün.
- So probierst du es aus: Quader und Zylinder aufs Bett; Quader wählen (Werkzeug Verschieben): Maßbänder zum Zylinder und zu den
  Bettkanten erscheinen. Den Quader ziehen: Zahlen laufen mit. Auf die Zahl zum linken Rand klicken, `5` Enter: Quader steht
  genau 5 mm vom Rand. ⌘Z nimmt es zurück.

## Stolperfallen
- Die Hüllquader anderer Körper aus Anzeige-Netzen sind bei runden Körpern minimal kleiner als exakt (Sehnenfehler). Für die Anzeige
  egal; beim **Setzen** eines Abstands zu einem runden Körper weicht das Ergebnis um bis zu 0,01 mm ab. In der PR-Beschreibung
  nennen. Exakter wäre `GeometryKernel::exactBounds` für den Nachbarn (im Hintergrund, nicht im UI-Thread).
- Während eines Zugs wird `refresh` sehr oft gerufen: Hüllquader der anderen Körper einmal je Szene berechnen und merken.
- Ziffern-Tastatur: Wenn ein Abstands-Etikett Fokus hat, dürfen Ziffern nicht die Ansichten 1–6 auslösen.

## Nicht tun
- Keine Abstände zu einzelnen Kanten oder Flächen innerhalb eines Körpers (nur Hüllquader). Das wäre ein eigenes Werkzeug „Messen“
  (AP-2.11).
- Keine Abstände in der Höhe.
- Abstandsmaße sind keine Bezüge: Sie verschieben nur, sie halten den Abstand nicht fest, wenn sich der Nachbar ändert.
