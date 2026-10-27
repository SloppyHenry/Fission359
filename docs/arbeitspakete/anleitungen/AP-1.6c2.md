# AP-1.6c2 Fangen bei Maß-Griffen, Eck-Griff und Ablegen

**Größe:** M · **Branch:** `ap-1.6c2-fangen-griffe` · **Phasendatei:** `phase-1-erster-koerper.md`, AP-1.6, Teil c2

## Ziel
Was beim Verschieben seit 1.6c1 geht, geht jetzt auch beim **Größe-Ändern** und beim **Ablegen**: Zieht man einen Maß-Punkt
(Werkzeug „Größe“), rastet die bewegte Fläche an Ecken, Kantenmitten und Flächenmitten anderer Körper und des Betts ein. Zieht
man ein Eck-Quadrat, rastet die gezogene Ecke ein. Legt man eine Form aus der Galerie ab, rastet ihr Grundriss an anderen Körpern
ein. Überall zeigt eine Marke, woran es hängt, und ⇧ schaltet das Fangen aus.

## Voraussetzungen
AP-1.6c1 erledigt (ist es, PR #34).

## Vorher lesen
- `CLAUDE.md`, [Handbuch](handbuch.md) §1, §2, §9, §10, §12
- Phasendatei AP-1.6: Zerlegung, „Festlegungen“ (Abschnitt **Fangen**), „Bekannte Einschränkungen“
- `src/interaction/include/fission/interaction/snap.h` (ganze Datei) und `src/interaction/src/snap.cpp` (`snapOffset`)
- `src/ui/include/fission/ui/snap_session.h`, `src/ui/src/snap_session.cpp`
- `src/ui/src/gizmo_controller.cpp`: `snappingOff`, `snapReach`, `clearSnap`, `beginSnap`, die Stelle in `dragTo`, die
  `m_snap.snap(...)` aufruft, und `typeInsteadOfDrag`. **Das ist das Vorbild für alles in diesem AP.**
- `src/ui/src/handle_controller.cpp`: `beginDrag`, `dragTo` (Zeilen um `interaction::snapped`), `refresh` (wo die Grafik gebaut wird)
- `src/ui/src/scale_controller.cpp`: `beginDrag`, `dragTo`, `refresh`
- `src/ui/src/placement_controller.cpp` (ganz)
- `tests/unit/ui/test_gizmo.cpp` (Testfälle mit Tag `[snap]`), `tests/unit/interaction/test_snap.cpp`

## Ausgangslage im Code
- `interaction::snapOffset(movers, targets, offset, free, reach)` rechnet den Fang Achse für Achse in **Weltachsen**.
  `SnapSession::begin(scene, movingBody, low, high)` sammelt die Ziele (andere Körper + Bett) und die Fangpunkte des bewegten
  Körpers aus dessen Hüllquader. `SnapSession::marksOf(...)` macht daraus die Grafik (Marke, gestrichelte Linie).
- `GizmoController` nutzt das schon. ⇧ wird in `GizmoController::snappingOff()` gelesen (plus `m_snapSuppressed`, gesetzt über
  `--simulate "snap:off"`). Die Reichweite (10 Pixel) wird je Achse in mm umgerechnet (`snapReach`).
- `HandleController::dragTo` rastet den Wert nur auf das Raster (`interaction::snapped(wert, step)`); `step` kommt aus
  `Viewport3D::snapStepFor`. Danach Bisektion, falls die Form unmöglich würde.
- `ScaleController::dragTo` rastet den Faktor auf ganze Prozent (`kFactorStep`).
- `PlacementController::landAt` rastet x und y auf `snapStep()` (Raster der Statuszeile oder 1 mm). Auf Wänden rastet
  `snappedOnFace` entlang der Fläche.
- Grafik: Jeder Controller gibt seine `render::HandleGraphics` mit einer Quelle an die Ansicht
  (`Viewport3D::HandleSource::size | gizmo | rings | corners`). Für das Ablegen gibt es noch keine Quelle.

## Festlegungen (schon entschieden, nicht neu fragen)
- Gefangen wird an denselben Zielen wie in c1: Ecken, Kantenmitten, Kreismitten, Flächenmitten anderer Körper und des Betts.
- Fangreichweite 10 Pixel (`interaction::kSnapReachPixels`).
- ⇧ schaltet Fangen **und** Raster aus, solange es gehalten wird. Getippte Zahlen werden nie gefangen.
- Der Eck-Griff bleibt immer gleichmäßig (Regel 11); ⇧ heißt dort nur „Fangen aus“.
- Wo nichts fängt, gilt wie bisher das Raster (Maß-Griffe: `snapStepFor`, Eck-Griff: ganze Prozent, Ablegen: `snapStep()`).

## Haltepunkte (vor Schritt 5 und 6 fragen, gesammelt am Anfang)
1. **Ablegen an Wänden:** Soll beim Ablegen an einer schrägen oder senkrechten Fläche auch gefangen werden? Empfehlung: **nein**
   in c2, dort bleibt das Raster entlang der Fläche (wie heute); Fangen nur beim Ablegen auf dem Bett und auf waagerechten
   Oberseiten. Folge: einfacher, aber an Wänden kein Ausrichten an Nachbarn.
2. **Eck-Griff: Fang oder ganze Prozent zuerst?** Empfehlung: Fang vor Prozent (wie beim Verschieben: Fang vor Raster). Folge:
   Ein gefangener Faktor kann krumm sein (z. B. 137,4 %); die Wertblase zeigt ihn mit einer Nachkommastelle.

## Schritte

### Schritt 1 – ⇧ an einer Stelle für alle Controller
Heute weiß nur `GizmoController`, ob ⇧ gedrückt ist. Zieh das in eine gemeinsame Funktion:
- In `src/ui/include/fission/ui/snap_session.h` eine freie Funktion `bool snappingSuppressed();` und eine Funktion
  `void setSnappingSuppressedForTests(bool);` (oder eine statische Klasse `SnapSwitch`) anlegen. Umsetzung in
  `snap_session.cpp`: true, wenn der Test-Schalter an ist **oder** eine `QGuiApplication` läuft und
  `QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier` gesetzt ist (genau wie `GizmoController::snappingOff`).
- `GizmoController::snappingOff()` ruft die neue Funktion (plus das eigene `m_snapSuppressed`, damit nichts bricht).
- `--simulate "snap:off|on"` in `apps/fission359/src/main.cpp` setzt künftig den gemeinsamen Schalter.
- Test: vorhandene `[snap]`-Tests bleiben grün.

### Schritt 2 – Fangpunkte einer einzelnen Fläche des Hüllquaders
Für einen Maß-Griff bewegt sich nicht der ganze Körper, sondern **eine Fläche** (die, auf der der Punkt sitzt). Ihre Fangpunkte
sind die 4 Ecken, 4 Kantenmitten und die Mitte der Seite des Hüllquaders, die in Griffrichtung liegt.
- In `snap.h`/`snap.cpp`: `std::vector<SnapPoint> snapPointsOfBoxSide(const Vec3& low, const Vec3& high, int axis, bool upper);`
  (`axis` 0/1/2 = x/y/z, `upper` = die Seite mit dem größeren Wert).
- Unit-Test `[snap]`: Seite +X eines Quaders (0,0,0)–(10,20,30) hat 9 Punkte, alle mit x = 10, die Mitte ist (10,10,15) und
  hat Art `faceMiddle`.

### Schritt 3 – Maß-Griffe fangen (HandleController)
In `HandleController::beginDrag`:
- Nur wenn die Griffrichtung (`m_dragAxis` **vor** der Umrechnung mit `rise`, also die `dx,dy,dz` aus `stepHandles`) parallel zu einer
  Weltachse ist (Betrag einer Komponente > 0,999) **und** der Griff keine Steigung hat (`rise == 0`): Fang vorbereiten. Sonst
  (gedrehter Körper, Kugel-Radius) nur Raster wie bisher.
- Fangen vorbereiten: Hüllquader des Körpers (`bodyBounds(lo, hi)` gibt es schon), Seite bestimmen (Achse aus der Griffrichtung,
  `upper` aus dem Vorzeichen), `SnapSession` mit `begin(scene, body, …)` starten und die Mover-Punkte mit Schritt 2 auf die Seite
  setzen. Dafür `SnapSession` um eine zweite Startfunktion erweitern:
  `void beginWithMovers(const render::RenderScene&, const std::string& movingBody, std::vector<interaction::SnapPoint> movers);`
- Die Reichweite in mm: wie `GizmoController::snapReach` (Hilfsfunktion in eine gemeinsame Stelle ziehen, z. B. `snap_session.cpp`,
  mit der Projektion als Parameter `std::function<QPointF(const QVector3D&)>` und dem mm-pro-Pixel-Wert).

In `HandleController::dragTo`, **bevor** auf das Raster gerundet wird:
- Ist Fangen aktiv und nicht unterdrückt (Schritt 1): Verschiebung der Fläche in mm entlang der Weltachse =
  `(wunschwert − startwert) × travel × sign` (Achtung: `m_dragTravel` und `m_dragSign` sind nach der Umrechnung gesetzt; rechne mit
  den Originalwerten aus `stepHandles`, die du in `beginDrag` speicherst). Mit `snapOffset(..., free = nur diese Achse, ...)`
  fangen. Wenn gefangen: `wanted = startwert + gefangeneVerschiebung / (travel × sign)`; **nicht** zusätzlich auf das Raster runden.
  Wenn nicht: wie bisher Raster.
- Die Bisektion („Form würde unmöglich“) gilt weiter **nach** dem Fang: Ein gefangener Wert, den `acceptsValue` ablehnt, wird
  verworfen und es geht mit dem Raster weiter.
- Marken: Ergebnis mit `SnapSession::marksOf` in der Grafik des HandleControllers mitzeichnen (in `refresh`, wo die Grafik gebaut
  wird, wie `GizmoController` `m_snapMarks` anhängt). Marken löschen bei `endDrag`, `cancelDrag`, `typeInsteadOfDrag`.
- Während des Zugs das Fangraster auf dem Bett zeigen wie beim Gizmo (`m_viewport->setSnapGridStep(...)`) und bei Ende auf 0 setzen.

### Schritt 4 – Eck-Griff fängt (ScaleController)
Die gehaltene Ecke wandert auf der Diagonale: `ecke(f) = anker + (ecke0 − anker) × f`. In `dragTo`, vor dem Runden auf ganze Prozent:
- Für jede Achse a mit `|ecke0[a] − anker[a]| > 1e-6` und jedes Ziel t: Kandidat `f = (t[a] − anker[a]) / (ecke0[a] − anker[a])`.
- Ein Kandidat gilt, wenn `|f − f_wunsch| × |ecke0[a] − anker[a]|` (Abstand in mm auf dieser Achse) kleiner als die Reichweite
  dieser Achse ist. Den mit dem kleinsten Abstand in Pixeln nehmen. Liegt die Ecke dann in **allen drei** Achsen auf einem Ziel,
  ist es ein Punkt-Fang (Vorrang, wie `snapOffset`).
- Diese Rechnung als reine Funktion in `snap.h` (`std::optional<double> snapScaleFactor(anchor, corner0, wanted, targets, reach,
  SnapMatch* match)`), damit sie ohne Fenster getestet werden kann.
- Ziele: `SnapSession` mit dem Körper als bewegtem Körper starten (seine eigenen Punkte sind keine Ziele).
- Akzeptiert `acceptsScale(f)` den Faktor nicht: verwerfen, Prozent-Raster wie bisher.
- Marke: Fangergebnis als `SnapResult` zusammenbauen (Mover = die neue Ecke, Target = das Ziel) und mit `marksOf` zeichnen
  (Quelle `corners`).
- Wertblase: Prozent mit einer Nachkommastelle zeigen, wenn der Faktor nicht ganzzahlig ist (QML `ScaleLayer.qml`).

### Schritt 5 – Ablegen fängt (PlacementController) (nach Haltepunkt 1)
- Neue Quelle `Viewport3D::HandleSource::placement` (Enum in `viewport_3d.h` erweitern; in `viewport_3d.cpp` prüfen, wo die
  Quellen gespeichert werden, `grep -n HandleSource src/ui/src/viewport_3d.cpp`, und dort Platz schaffen).
- `DocumentModel` braucht den Hüllquader der Form, die gerade abgelegt wird: neue Funktion `std::optional<geo::Bounds>
  placingBodyBounds() const` (Vorbild `currentBodyBounds`, Körper ist `m_placingFeature`; liefert nichts, solange das Ergebnis
  noch nicht da ist).
- In `landAt`, nur bei waagerechter Landung (`!landing->tilted`): Grundriss-Fangpunkte = Ecken, Kantenmitten und Mitte der
  **Unterseite** des Hüllquaders, verschoben an die gewünschte Stelle. `snapOffset` mit `free = {x, y}`. Gefangen: diese Stelle
  nehmen, nicht runden. Nicht gefangen: Raster wie bisher.
- Die Ziele beim ersten `landAt` nach `begin` sammeln (die neue Form selbst ausschließen: `placingBodyId`). Bei `finish` und
  `cancel` Marken und Sitzung beenden.
- ⇧ (Schritt 1): Fangen und Raster aus, die Form folgt dem Zeiger frei.

### Schritt 6 – Texte und Hinweiszeile
- Hinweiszeile des Werkzeugs „Größe“ in `ToolCatalog.qml` um das Fangen ergänzen (wie bei „Verschieben“:
  „…; it snaps to corners and middles. Hold Shift to switch snapping off.“). Hinweis der Formen-Galerie ebenso.
- Übersetzen (Handbuch §8).

## Tests
Unit-Tests (`tests/unit/interaction/test_snap.cpp`, Tag `[snap]`):
- `snap points of one side of a box: 4 corners, 4 edge middles, the middle` (Schritt 2).
- `snap scale: the corner catches on a corner of another box` – Anker (0,0,0), Ecke0 (10,10,10), Ziel (20,20,20) als Ecke eines
  anderen Körpers, Wunschfaktor 1,97, Reichweite 1 mm → Faktor genau 2,0 und Punkt-Fang.
- `snap scale: out of reach the factor stays as wished` – gleiche Lage, Wunsch 1,5 → nichts gefangen.
- `snap scale: a catch on one axis only` – Ziel nur in x passend → Faktor so, dass x stimmt; kein Punkt-Fang.

UI-Tests (`tests/unit/ui/test_handles.cpp`, `test_scale.cpp`, `test_placement.cpp`, Tag `[snap]`), Vorbild
`snap: dragging an arrow catches the side of a body on the side of another` in `test_gizmo.cpp`:
- Zwei Quader 20 mm nebeneinander mit 10,3 mm Lücke. Breiten-Punkt des linken in Richtung des rechten ziehen, so dass er 0,4 mm
  vor der Seite des rechten stünde → Breite wird genau so, dass die Seiten bündig sind (kein Raster-Wert).
- Derselbe Zug mit Fang aus (Schalter aus Schritt 1) → Breite ist ein Raster-Wert, nicht bündig.
- Kugel-Radius-Griff (hat `rise`) fängt nie, nur Raster.
- Eck-Griff: Quader 20 mm, daneben Quader 40 mm hoch; Ecke ziehen bis kurz unter 200 % → Faktor genau 2 (Oberkanten bündig).
- Ablegen: Quader auf dem Bett; zweiten Quader so ablegen, dass seine Seite 0,4 mm neben der des ersten stünde → bündig.
- Ablegen an einer Wand: kein Fang (Haltepunkt 1, Empfehlung), Raster entlang der Fläche wie bisher.
- Jeder Zug bleibt **ein** Undo-Schritt; Esc nimmt alles zurück.

Fenster-Tests (`tests/CMakeLists.txt`, Handbuch §9):
- `app_snap_size_handle`: `put:box;type:20,enter;put:box;…` (zweiten Quader mit `drop:` neben den ersten legen), `tool:size`,
  `grab:width,<dx>,0,8`, `near:width,<bündiger Wert>`. Werte so wählen, dass das Raster den bündigen Wert **nicht** trifft
  (z. B. Abstand mit 0,35 mm Rest).
- `app_snap_size_handle_off` als Gegenprobe mit `snap:off` und `WILL_FAIL TRUE`.

## Abnahme
- Alle neuen und alten Tests grün auf dem Mac; CI grün (macOS und Ubuntu).
- So probierst du es aus (für den PR):
  1. `scripts/run --release` im Ordner `Fission359-<deine Arbeitskopie>`.
  2. Zwei Quader aufs Bett legen. Links **Größe** wählen, am Breiten-Punkt des einen zum anderen ziehen: kurz vor der Berührung
     rastet die Seite bündig ein, eine Marke zeigt die Stelle.
  3. Dasselbe mit gedrückter ⇧: kein Einrasten.
  4. Ein Eck-Quadrat ziehen, bis die Oberkante auf Höhe des Nachbarn ist: rastet ein, die Blase zeigt den Prozentwert.
  5. Aus der Galerie einen Zylinder neben einen Quader ziehen: er richtet sich an Kanten und Mitte des Quaders aus.

## Stolperfallen
- `m_dragAxis`, `m_dragTravel`, `m_dragSign` werden in `beginDrag` für die Steigung (`rise`) umgerechnet. Für den Fang die
  Originalwerte aus `stepHandles` merken.
- Formeln: `setStepValue` mit dem gefangenen Wert schreibt eine Zahl; das ist beim Ziehen richtig (wie bisher).
- `SnapSession` cacht Punkte je Körper und Netzversion. Beim Maß-Griff ändert sich das Netz des eigenen Körpers während des Zugs;
  der eigene Körper ist aber kein Ziel. Die Mover-Punkte werden **einmal** bei `beginDrag` bestimmt (Fläche am Start), danach wird
  nur die Verschiebung gefangen.
- Gegenüberliegende Griffe: Ein Quader hat für `width` zwei Griffe (links und rechts, `travel = 0.5`). Die Seite ergibt sich aus
  dem Vorzeichen der Griffrichtung des **gehaltenen** Griffs.
- `queryKeyboardModifiers()` stürzt in Unit-Tests ohne `QGuiApplication` ab (Handbuch §10).

## Nicht tun
- Kein Fangen beim Drehen (das bleibt 15°-Schritte, eigenes Thema).
- Keine exakten Fangpunkte aus dem Kernel (Folgearbeit in [pflege.md](pflege.md)).
- Keine Abstandsmaße (das ist 1.6d).
- Den Fang nicht für gedrehte Körper „ungefähr“ machen: Wenn die Griffrichtung nicht achsparallel ist, gibt es nur das Raster.
