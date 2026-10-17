# Prompt: AP-1.6 Transformieren (Gizmo, Fangen, Abstandsmaße)

**Verwendung:** In einer neuen Sitzung im Fission3D-Repository einfügen. Der Prompt enthält den Stand, die
Bausteine, auf denen AP-1.6 aufsetzt, und die Fragen, die vor dem Bauen zu klären sind. Entscheidungen, die du
anders treffen willst, vor dem Absenden im Text ändern.

---

## Prompt

```markdown
# Auftrag: AP-1.6 Transformieren: Gizmo, Fangen, Abstandsmaße

Halte dich an CLAUDE.md (Master-Prompt). Lies zuerst `docs/arbeitspakete/phase-1-erster-koerper.md`
(AP-1.6, dazu die Abschlussvermerke von AP-1.4 und AP-1.5), `docs/02-ux-konzept.md` §2.2, §2.3 und §7,
`docs/08-gestaltung.md` §5 (Griffe), das Mockup `docs/mockups/oberflaeche.html` (Szene „Quader“ mit dem
Gizmo, Funktion `moveGizmo`; Szene „Loch“ mit dem editierbaren Abstandsmaß „… mm vom Rand“), `docs/03-architektur.md` und `docs/adr/`.

## Stand (17.10.2026)
- AP-1.1 bis AP-1.5 sind **erledigt** und in `main` (AP-1.5 = PR #22–#26). `main` ist geschützt: nur per
  Pull Request mit allen 7 grünen CI-Prüfungen.
- AP-1.5 hat Grundformen, Text, die Formen-Galerie mit Ablegen und „Als Loch“, und die **Maß-Griffe** gebracht.
  Zuletzt wurden die Griffe nach Feedback des Product Owners umgebaut. Er fand das Ergebnis gut und hat diese
  Richtung bestätigt:
  - Der Punkt sitzt auf der Fläche, die er bewegt.
  - Die Maßbänder liegen im Raum außerhalb des Körpers, mit der Beschriftung auf dem Band.
  - Neben-Maße (Rundung, Wand) bleiben ruhig.
  - **Griffe mit Tiefe:** Was ein Körper verdeckt, ist blass.

## Bausteine, auf denen AP-1.6 aufsetzt
- **Zeichnen im Raum:** `render::HandleGraphics` (Linien, Punkte, Pfeile; Größen in Fenster-Pixeln) wird von
  `SceneRenderer` in zwei Durchgängen gegen die Tiefe gezeichnet, blass verdeckt und kräftig sichtbar.
  Gizmo-Pfeile, Ebenen-Quadrate und Drehringe sollen dieselbe Zeichnung benutzen. Sie wird nicht neu gebaut,
  sondern erweitert, etwa um Ringe und gefüllte Flächen.
- **Griffe:** `HandleController` (src/ui) zeigt Beispiele für alles Nötige:
  - Ziehen über einen 3D-Strahl auf einer Ebene durch die Achse, die zum Betrachter zeigt.
  - Rasten in ganzen mm, feiner beim Zoomen.
  - Eine Achse, die fast auf den Betrachter zeigt, wird nicht gezogen.
  - Tippen ersetzt Ziehen.
  - Verdeckungsprüfung für Beschriftungen.
  Die Beschriftungen und Anfassflächen liegen in `HandleLayer.qml`.
- **Änderungen:** `DocumentModel::beginChange / setStepValue / commitChange / cancelChange`. Ein ganzer Zug
  ist ein Schritt im Verlauf, Esc nimmt alles zurück. Jede Form hat schon `x`, `y`, `z` (Felder „Lage“ rechts).
- **Auswahl:** `SelectionController`, Hover und Auswahl über den Pick-Durchgang (AP-1.4).
- **Arbeitsebene:** Sie stammt aus dem Ablegen (AP-1.5c): Formen landen auf dem Bett oder auf ebenen Oberseiten.
- **Nachweise:**
  - `--smoke-test bild.png --camera=yaw,pitch,abstand --simulate "put:box;type:…;grab:width,40,0,8;point:width;expect:…"`
  - Echte Mausereignisse durchs Fenster. Neue Aktionen für das Gizmo gehören dort dazu.

## Offene Punkte aus AP-1.5, die hierher gehören
- **Ablegen auf Wänden und Schrägen:** Bisher bleibt die Form waagerecht auf der Arbeitsebene. Sie
  auszurichten braucht Drehen.
- **Fangen:** Positionen und Maße rasten bisher nur auf ganze Millimeter. Das echte Fangen
  (Ecken, Kantenmitten, Kreismitten, Flächenmitten, 15°) kommt mit diesem Paket.
- **Idee des Product Owners, noch nicht beauftragt:** Zoomen mit der Tastatur (+/−, ⌘/Strg +/−).

## Vorschlag zur Zerlegung (AP ist L)
- **1.6a Verschieben:** Gizmo mit Achsen-Pfeilen und Ebenen-Quadraten, Wertblase, Tippen (`Tab` X → Y → Z),
  Esc. Klären, wie es mit den Maß-Griffen zusammenspielt (Werkzeug „Verschieben“ gegenüber „Größe“ in der
  Werkzeugleiste).
- **1.6b Drehen:** Ringe, 15°-Fangen, Tippen in Grad. Proportionales Vergrößern und Verkleinern über einen
  Eck-Griff mit ⇧ oder eine Prozentangabe. Bei Grundformen bedeutet das: alle Maße im gleichen Verhältnis,
  nie Verzerren (Regel 11).
- **1.6c Fangen:** Adaptives Raster, Ecken, Kantenmitten, Kreismitten und Flächenmitten, Ausrichtungslinien.
  ⇧ schaltet das Fangen aus.
- **1.6d Abstandsmaße:** Abstände zu den nächsten Kanten anderer Körper und zum Bett, anklickbar und editierbar
  („5 mm vom Rand“, Akzeptanzkriterium 1).
- **1.6e Rest:** Ausrichten mehrerer Körper, „Aufs Bett legen“, „Auf Fläche legen“ (auch an Wänden),
  Kopieren, Einfügen und Duplizieren (⌘D) mit Versatz.

## Fragen, die du mir vor dem Bauen stellst (je höchstens 3 Optionen, mit Empfehlung)
1. **Wo lebt eine Verschiebung oder Drehung im Rezept?** Akzeptanzkriterium 2 verlangt, dass jede Transformation
   ein editierbarer Rezept-Eintrag ist. Möglich wären die Lage-Felder der Form, ein eigener Schritt
   „Verschieben/Drehen“ oder beides je nach Fall. Das ist eine Architekturentscheidung, also ein ADR.
   Prüfe dabei auch, was sie für Referenzen (ADR-0003/0009) und das spätere Dateiformat (AP-1.10) bedeutet.
2. **Wann erscheint das Gizmo?** Zum Beispiel beim Werkzeug „Verschieben“ oder immer bei Auswahl, und wie
   verträgt es sich mit den Maß-Griffen auf demselben Körper, ohne dass das Bild wieder unruhig wird?
3. **Was heißt „Kopieren/Einfügen“ ohne Dateiformat?** Etwa nur innerhalb des Programms oder schon über die
   System-Zwischenablage.

## Bekannte Fallen (aus den bisherigen Sitzungen)
- **Eigener Arbeitsordner:** Arbeite in einem eigenen Worktree, zum Beispiel `../Fission3D-ap16` auf Branch
  `ap-1.6-…`. Nie den Branch in einem Ordner wechseln, den eine andere Sitzung benutzt. Nach
  `git worktree add` immer `git branch --show-current` prüfen.
- **Formatierung:** Die CI prüft mit clang-format **21.1.8**, Homebrew hat eine neuere Version. Lege ein venv
  an (`python3 -m venv v && v/bin/pip install clang-format==21.1.8`) und rufe dann
  `CLANG_FORMAT=v/bin/clang-format python3 scripts/tools/check_format.py --fix` auf.
- **Bearbeiten auf dem Mac:** macOS-`sed -i` braucht `''`; Änderungen besser mit Python machen. In zsh
  `--include=*.h` in Anführungszeichen setzen.
- **Ausprobier-Anleitung:** `scripts/run` baut Debug und ist mit OCCT langsam. Für den Product Owner
  `scripts/run --release` angeben, aus dem richtigen Arbeitsordner.
- **Fenster-Tests auf dem Mac:** Sie teilen sich den echten Bildschirm. Tests, die im Fenster ziehen, mit
  `RUN_SERIAL` laufen lassen, sonst nimmt ein anderes Testfenster den Fokus und der Zug endet.
- **Linux:** nur über die CI (der Testrechner hat kein vcpkg). Die Ziffern 1–6 sind Ansichts-Tasten, Tippen
  von Maßen beginnt nur nach dem Ablegen oder während eines Zugs.
- **Verdeckung:** Die Griffe zeichnet der Renderer. Neue Overlays nicht wieder flach in QML zeichnen, sonst
  sind sie nicht tiefenrichtig.

## Arbeitsweise
- Beginne mit Zusammenfassung (3–5 Sätze), den offenen Fragen oben und einem Umsetzungsplan mit der Zerlegung.
  Trage die Teilpakete in die Phasendatei ein und **warte auf mein OK**, bevor du baust.
- Kleine, lauffähige Schritte; nach jedem bauen und testen. Commits `AP-1.6: …`, ein PR je Teilpaket.
- Belege mit Tests (Unit-Tests für Ziehen, Fangen und Rezept-Einträge; Abstandsmaß „5 mm vom Rand“ als
  automatischer Ablauf) und mit Bildschirmfotos aus dem echten Fenster.
- Status am Ende ehrlich auf „Review“ setzen, nicht auf „erledigt“. Antworte kurz und in Alltagssprache.
```
