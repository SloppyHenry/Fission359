# Prompt: AP-1.6b Drehen und gleichmäßig vergrößern

**Verwendung:** In einer neuen Sitzung im Fission3D-Repository einfügen. Der Prompt enthält den Stand nach 1.6a,
die Bausteine, auf denen 1.6b aufsetzt, und die Fragen, die vor dem Bauen zu klären sind. Entscheidungen, die du
anders treffen willst, vor dem Absenden im Text ändern.

---

## Prompt

```markdown
# Auftrag: AP-1.6b Drehen und gleichmäßig vergrößern

Halte dich an CLAUDE.md (Master-Prompt). Lies zuerst `docs/arbeitspakete/phase-1-erster-koerper.md` (AP-1.6:
Zerlegung, Festlegungen vom 18.10.2026, Zwischenstand 1.6a mit Nachbesserung), `docs/adr/0010-lage-im-rezept.md`,
`docs/adr/0003-persistente-referenzen.md`, `docs/adr/0008-dokumentmodell-und-kernel-schnittstelle.md`,
`docs/02-ux-konzept.md` §2.2, §2.3 und §7.2, `docs/08-gestaltung.md` §5 (Griffe) und das Mockup
`docs/mockups/oberflaeche.html` (Werkzeugleiste: Wählen, Verschieben, Drehen, Größe).

## Stand (18.10.2026)
- AP-1.1 bis AP-1.5 sind erledigt. **AP-1.6a „Verschieben“ ist erledigt und in `main`** (PR #27). Der Product
  Owner hat es ausprobiert („fühlt sich top an“). `main` ist geschützt: nur per Pull Request mit allen grünen
  CI-Prüfungen.
- **Festlegungen (Product Owner, 18.10.2026), gelten weiter:**
  - **Lage im Rezept (ADR-0010):** Verschieben und Drehen ändern die Lage-Werte des Schritts, der den Körper
    macht. Ein Zug ist ein Schritt in „Letzte Aktionen“ („Quader verschoben“), kein neuer Rezept-Eintrag. Ein
    eigener Schritt „Verschoben“ kommt nur, wo ein Schritt mehrere Körper speist (frühestens AP-1.7).
  - **Das Werkzeug entscheidet die Griffe:**
    - Verschieben: Gizmo und Maßbänder.
    - Größe: Maß-Punkte und (neu in 1.6b) Eck-Griff.
    - Drehen: Ringe (neu in 1.6b).
    - Wählen: keine Griffe.
    Den Körper selbst ziehen verschiebt ihn in jedem Werkzeug außer „Wählen“.
  - **Der Eck-Griff ist immer gleichmäßig,** es gibt keinen verzerrenden. ⇧ während eines Zugs heißt nur
    „Fangen aus“ (kommt mit 1.6c). UX §7.2 ist schon angepasst.
  - **Gizmo-Achsen** zeigen immer in Bett-Richtung. **Drehen** geschieht um die Körpermitte.
  - Kopieren und Einfügen nur innerhalb des Programms. Abstandsmaße messen von Kante zu Kante. Zoomen mit der
    Tastatur gehört nicht zu AP-1.6.

## Was 1.6b umfasst (aus der Zerlegung)
- **Die Lage bekommt eine Drehung.** Drei Winkel als gewöhnliche Werte jeder Form, im rechten Panel unter
  „Lage“ editierbar, auch mit Formeln.
- **Werkzeug „Drehen“:**
  - Ringe um X, Y und Z, gezeichnet wie das Gizmo in der 3D-Ansicht mit Tiefe.
  - Rasten in 15°-Schritten.
  - Die Wertblase zeigt Grad. Tippen ersetzt Ziehen, Tab geht zum nächsten Ring, Esc nimmt zurück.
- **Werkzeug „Größe“, gleichmäßig vergrößern:**
  - Ein Eck-Griff und eine Prozentangabe ändern **alle Maße der Form im gleichen Verhältnis**.
  - Das sind Parameter, nie ein Skalieren der Geometrie (Regel 11). Ein Quader mit 5-mm-Ecken auf 200 %
    hat 10-mm-Ecken und bleibt rund.
- **Ablegen an Wänden und Schrägen:** Die Form richtet sich zur Fläche aus. Das war ein offener Punkt aus AP-1.5.

## Bausteine, auf denen 1.6b aufsetzt
- **`GizmoController`** (src/ui) und **`GizmoLayer.qml`**:
  - Aufbau: feste Plätze für alle Teile (`parts`, je mit `shown`), Wertblase, Tippen mit Tab.
  - Anker über der Körpermitte; Größe in Bildschirm-Pixeln.
  - Werkzeug-Schalter `enabled`.
  - Die Ringe gehören hierher oder in einen Geschwister-Baustein nach demselben Muster.
- **Zeichnen im Raum:** `render::HandleGraphics` kennt Linien, Punkte, Pfeile, gefüllte Spitzen (`heads`) und
  gefüllte Vierecke (`quads`). Ringe lassen sich als Linienzug zeichnen. Zwei Quellen (Maß-Griffe, Gizmo) zeigt
  `Viewport3D::setHandleGraphics(…, HandleSource)` zusammen.
- **Zieh-Rechnung** in `src/interaction/drag.h`:
  - `AxisDrag` zieht jede Achse; eine, die fast auf einen zeigt, über den Bildschirm (`screenOf`).
  - `PlaneDrag` zieht in einer Ebene.
  - Beide folgen nicht über den Fluchtpunkt bzw. Horizont hinaus. Dazu kommen `snapStep` und `snapped`.
  - Ein Drehen auf einem Ring ist ein Winkel in der Ring-Ebene: neu, aber nach demselben Muster und mit Unit-Tests.
- **Dokument:**
  - `DocumentModel::beginMove / moveCurrent / commitChange / cancelChange`.
  - Ein Zug ist ein Schritt mit eigenem Namen (`moveBody`). `History` behält einen eigenen Namen auch bei nur
    einer Änderung.
  - Für Drehen und Vergrößern entsprechend neue Namen (`rotateBody`, `scaleBody`) mit Sätzen in „Letzte Aktionen“.
- **Kernel (GAL):**
  - `geo::GeometryKernel` baut Formen mit `makeBox`, `makeCylinder`, `makePrimitive` auf einer Grundebene
    (Basis x, y, z).
  - Für die Drehung braucht es eine Kernel-Operation, die einen `NamedSolid` dreht **und die Flächen-Namen
    behält** (ADR-0008 Punkt 2 ordnet Rollen geometrisch zu: Nach einer Drehung neu zuzuordnen hieße, dass
    „oben“ eine andere Fläche wird. Das darf nicht passieren).
  - Golden-Tests: Volumen, Fläche, BBox, Gültigkeit, Namen nach Drehung.
- **Griffe drehen mit:** `FeatureType::handles` liefert die Maß-Griffe in Weltkoordinaten aus `x, y, z`. Bei einer
  gedrehten Form müssen Ursprung und Richtung mitgedreht werden, für alle Formen an einer Stelle (Helfer in
  `src/features/src/primitives.h`). `HandleController` färbt und bebändert nur Achsen, die genau in
  Bett-Richtung laufen (`axisOf`). Klären, wie die Maße eines gedrehten Körpers aussehen sollen.
- **Werkzeuge:** `ToolCatalog.qml` hat für „Drehen“ schon Tooltip, Hinweiszeile und Suchwörter (Text „kommt in
  Kürze“ ersetzen).
- **Nachweise:**
  - `--smoke-test bild.png --camera=yaw,pitch,abstand --simulate "tool:rotate;put:box;gizmo:…;type:…;near:x,12;key:Right"`.
  - Weitere vorhandene Aktionen: `grab:` (Maß-Punkt), `gizmo:teil,dx,dy,schritte[,hold]`, `drag:` (Körper),
    `expect:` (mindestens), `near:` (genau).
  - Neue Aktionen für Ringe und Eck-Griff gehören dazu. Fenster-Tests mit Ziehen bekommen `RUN_SERIAL`.

## Fragen, die du mir vor dem Bauen stellst (je höchstens 3 Optionen, mit Empfehlung)
1. **Wie wird die Drehung gespeichert und angezeigt?** Zum Beispiel drei Winkel um X, Y, Z in fester
   Reihenfolge, Achse plus Winkel oder eine Liste einzelner Drehungen. Was heißt das für Formeln in den
   Feldern, für Ringe um Bett-Achsen (eine Drehung um X ändert bei festen Winkeln womöglich alle drei Werte) und
   für Referenzen (ADR-0010 Punkt 4: Vergleich in Körper-Koordinaten)? Das ist eine Ergänzung zu ADR-0010.
2. **Gleichmäßig vergrößern und Formeln:** Ein Maß ist `Breite/2` (benanntes Maß). Wird es zur Zahl,
   wird `(Breite/2) × 1,5` daraus, oder bleibt es und nur Zahlen ändern sich? Wo tippt man die Prozentangabe
   (Wertblase am Eck-Griff, Panel)?
3. **Ablegen an einer Wand:** Steht die Form mit ihrer Unterseite auf der Wand (Zylinder zeigt aus der Wand
   heraus), automatisch bei jeder schrägen Fläche oder nur mit einer Taste? Was passiert mit der
   Arbeitsebene?

## Bekannte Fallen (aus den bisherigen Sitzungen)
- **Eigener Arbeitsordner:**
  - Arbeite in einem eigenen Worktree, zum Beispiel `../Fission3D-ap16b` auf Branch `ap-1.6b-drehen` von
    `origin/main`. Nach `git worktree add` immer `git branch --show-current` prüfen.
  - Den Upstream nicht auf `main` zeigen lassen (`git branch --unset-upstream`), beim ersten Push `-u` setzen.
  - Bauen mit `VCPKG_ROOT=<Haupt-Ordner>/.vcpkg cmake --preset macos-debug`.
- **Formatierung:** Die CI prüft mit clang-format **21.1.8**. Lege ein venv an
  (`python3 -m venv v && v/bin/pip install clang-format==21.1.8`) und rufe dann
  `CLANG_FORMAT=v/bin/clang-format python3 scripts/tools/check_format.py --fix` auf.
- **Übersetzungen:**
  - `cmake --build --preset macos-debug --target update_translations` aktualisiert die `.ts`-Dateien; danach
    die neuen Einträge füllen und mit `python3 scripts/tools/check_translations.py .` prüfen.
  - Beim Füllen per Skript nur `<source>[^<]*</source>` anfassen: Ein `.*?` über mehrere Nachrichten hinweg
    zerstört die Datei.
  - Texte, die von einem QML-Kontext in einen anderen wandern, verlieren ihre Übersetzung.
- **Gezogene Anfassflächen nie umbauen:** Eine QML-Anfassfläche, die während eines Zugs verschwindet oder neu
  entsteht, beendet den Zug. Das war die „verliert die Haftung“-Rückmeldung in 1.6a. Listen für Griffe behalten
  ihre Länge und Reihenfolge, das gehaltene Teil bleibt sichtbar.
- **Grenzen der Form:** Ein gezogener Wert, den die Form nicht erlaubt, hält beim nächsten möglichen Wert an
  (Bisektion in `HandleController::dragTo`). Er darf nie stehen bleiben. Das gilt auch für den Eck-Griff: Er
  stoppt, sobald ein Maß unmöglich würde.
- **Unsichtbare Anfassflächen:** Anfassflächen eines Werkzeugs, das gerade nicht aktiv ist, müssen
  `enabled: false` sein. Sonst schlucken sie Klicks auf den Körper (so verschluckten Maß-Punkte den Körper-Zug).
- **Fenster-Tests:**
  - Tests, die an Maß-Punkten ziehen, wählen vorher `tool:size`; Standard-Werkzeug ist „Verschieben“.
  - Eine Form aus `put:box` landet bei (−30, −30), weil die Mitte belegt ist.
  - Auf dem Mac teilen sich die Fenster-Tests den Bildschirm, darum `RUN_SERIAL`.
- **Bearbeiten auf dem Mac:** `sed -i` braucht `''`; Änderungen besser mit Python machen.
- **Ausprobier-Anleitung:** `scripts/run` baut Debug und ist mit OCCT langsam. Für den Product Owner
  `scripts/run --release` angeben, aus dem richtigen Arbeitsordner.
- **Linux:** nur über die CI (der Testrechner hat kein vcpkg).

## Arbeitsweise
- Beginne mit einer Zusammenfassung (3–5 Sätze), den offenen Fragen oben und einem Umsetzungsplan (Schritte,
  betroffene Module, Tests). Bei Bedarf 1.6b weiter zerlegen (etwa b1 Drehen, b2 gleichmäßig vergrößern, b3
  Ablegen an Wänden) und in die Phasendatei eintragen. **Warte auf mein OK**, bevor du baust.
- Kleine, lauffähige Schritte; nach jedem bauen und testen. Commits `AP-1.6: …`, ein PR je Teilpaket.
- Belege mit Tests:
  - Golden-Tests für gedrehte und gleichmäßig vergrößerte Formen (Volumen, Gültigkeit, Namen bleiben,
    Rundungen bleiben rund).
  - Unit-Tests für Ring-Ziehen, 15°-Rasten und Verlauf.
  - Fenster-Test mit echter Maus und Bildschirmfotos.
- Status am Ende ehrlich auf „Review“ setzen, nicht auf „erledigt“. Antworte kurz und in Alltagssprache.
```
