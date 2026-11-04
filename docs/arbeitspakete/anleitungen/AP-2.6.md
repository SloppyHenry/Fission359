# AP-2.6 Aushöhlen und Schräge

**Größe:** M · **Phasendatei:** AP-2.6

## Ziel
**Aushöhlen**: Körper innen hohl machen, Wandstärke innen oder außen, offene Flächen wählen (Deckel weg), auf Wunsch je Fläche andere Stärke.
**Schräge** (Draft): Flächen leicht neigen, damit Gießformen sich lösen. Der klassische Problemfall (abgerundeter Quader mit offener Oberseite)
funktioniert.

## Voraussetzungen
AP-2.4 (Flächen-Offset), AP-2.10.

## Vorher lesen
- `docs/spikes/kernel-robustheit.md` (**wichtig:** Aushöhlen eines abgerundeten Quaders liefert in OCCT still ungültige oder falsche Körper;
  Gültigkeit allein beweist nichts; zuverlässig ist „großer Körper minus innerer Körper“), `tests/geometry/test_fillets.cpp` (Fälle C2, C2a, C2b)
- Phase-0-Datei, AP-0.5 „Offen“ (allgemeine Plausibilitätsprüfung einplanen)

## Haltepunkte
1. **Verfahren:** Empfohlen (aus AP-0.5): (1) Für eigene Formen (Grundformen, Extrusionen, Formen mit Rundungen aus `fillet`) Konstruktion
   „Körper minus nach innen versetzter Körper“: den Innenkörper **neu aufbauen** (Rezept mit um die Wandstärke verkleinerten Maßen und Radien:
   Rundung r → r − Wand, mindestens 0), offene Flächen durch Verlängern des Innenkörpers über die Fläche hinaus. (2) Für alle anderen Körper
   `BRepOffsetAPI_MakeThickSolid` **mit Plausibilitätsprüfung**; fällt sie durch → „braucht Hilfe“ mit Meldung „Aushöhlen hat hier nicht
   sicher geklappt“. Bestätigen lassen.

## Schritte
1. **Plausibilitätsprüfung als Baustein** (GAL, wiederverwendbar): `PlausibilityReport check(const Solid& before, const Solid& after, const
   Expectation&)` mit Erwartungen: Volumen in einem Bereich (beim Aushöhlen: zwischen Außenvolumen − Innenvolumen-Schätzung ± 2 %),
   Anzahl Körper (1), Anzahl Schalen (2 bei geschlossen, 1 bei offen), gültig. Tests mit absichtlich falschen Ergebnissen.
2. Feature-Typ `shell`: Eingang Körper, `refs["openFaces"]`, Werte `thickness`, Wort `side` (inside | outside), optional je Fläche Stärke
   (▸ Mehr: `refs` + Werte-Liste). Verfahren nach Haltepunkt 1: Feature-Typen bekommen optional `virtual std::optional<ResolvedParams>
   insetParams(const ResolvedParams&, double wall) const` (Werte des Innenkörpers); fehlt sie → Verfahren (2).
3. Feature-Typ `draft`: `refs["faces"]`, `refs["neutralPlane"]` (Fläche oder Ebene), Wert `angle`; OCCT `BRepOffsetAPI_DraftAngle`; Golden:
   Quader-Seiten 5° → Volumen nach Pyramidenstumpf-Formel.
4. Bedienung: Werkzeug „Aushöhlen“ (Gruppe Ändern), Klick auf Körper, dann offene Flächen anklicken; Griff für die Wandstärke (Pfeil auf der
   Innenseite), Tippen; Regel 10 (Synonyme „hohl“, „aushöhlen“, „wand“, „shell“, „hollow“). Schräge ebenso.
5. Meldungen: Wandstärke größer als halbe dünnste Stelle → „Die Wand ist dicker als der Körper an seiner dünnsten Stelle (%1 mm)“; Verfahren (2)
   nicht plausibel → „braucht Hilfe“ mit Erklärung.

## Tests
- **Akzeptanz:** abgerundeter Quader 40×40×20, Ecken R5, Oberseite offen, Wand 2 → gültig **und** Volumen nach Formel (Außen − Innen mit R3),
  Schalen-/Flächenzahl plausibel. Ebenso mit Wand 6 (> Radius: Innen-Radius 0 → scharfe Innenecken).
- Golden Quader, Zylinder, Extrusion; Fall C2 aus AP-0.5 über den neuen Weg.
- Plausibilitätsprüfung erkennt die bekannten falschen Ergebnisse von `MakeThickSolid` (Tests aus AP-0.5 als Eingabe).

## Abnahme
So probierst du es aus: Quader mit Eckenradius, Aushöhlen, Oberseite anklicken, `2` → offene Schale, im Schnitt (AP-2.11, sonst Röntgen-Darstellung)
gleichmäßige Wand.

## Nicht tun
Kein ungeprüftes `MakeThickSolid`-Ergebnis weiterreichen. Kein Lattice (AP-4.7).

## Stand 2026-11-04
Haltepunkt 1 vom Maintainer bestätigt: Innenkörper neu bauen, sonst die Bibliotheks-Funktion mit Plausibilitätsprüfung; nie ein ungeprüftes Ergebnis.

Gebaut:
- **Plausibilitätsprüfung** (`src/geometry/gal/include/fission/geo/plausibility.h`, kernelunabhängig): `checkPlausibility(before, after, Expectation, mayGrow)` prüft gültig, Zahl der Körper, Zahl der Schalen (neu: `SolidReport::shells`), Volumenbereich und „nicht größer als vorher“. Tests mit absichtlich falschen Ergebnissen.
- **Aushöhlen im Kern** (`GeometryKernel::hollow`, `src/geometry/occt/src/shell.cpp`): Ein Quader (mit runden senkrechten Ecken, Radius r → r − Wand, mindestens 0) und ein Zylinder werden erkannt (Flächenarten und Volumen gegen die Formel) und der Innenkörper wird neu gebaut und abgezogen; offene Flächen verlängern ihn über die Fläche hinaus. „Außen“ legt die Wand um den Körper. Ohne offene Fläche entsteht ein geschlossener Hohlkörper (zwei Schalen). Das Ergebnis wird gegen die Formel geprüft (Außen − Innen, 1e-6). Jeder andere Körper geht über `BRepOffsetAPI_MakeThickSolid` und wird nur geglaubt, wenn Wandvolumen ≈ Fläche × Wand (±25 %), eine Schale, ein Körper, gültig; sonst `invalidInput` mit Betreff „unsure“ und nichts kommt zurück. Die Wand darf dort höchstens ein Viertel der dünnsten Ausdehnung sein.
- Namen: Flächen des Körpers behalten ihre Namen (die offene Oberseite bleibt als Rand der Wand „top“), die Hohlraum-Flächen heißen `F<n>/shell/inner[<Ort>]` (Ort = `bottom`, `left`, `cornerFrontLeft`, `side` …).
- **Schräge im Kern** (`GeometryKernel::draftFaces`, `src/geometry/occt/src/draft.cpp`, `BRepOffsetAPI_DraftAngle`): neutrale Ebene durch einen Punkt, Zugrichtung, Winkel; positiv macht den Körper entlang der Zugrichtung schmaler. Test: Quader-Seiten 5° → Pyramidenstumpf-Volumen. Flächen, die der Kernel unverändert zurückgibt, werden über Art und Lage ihren Namen zugeordnet.
- Schritte `shell` (Wert `thickness`, Wort `side` innen/außen, Verweis `openFaces`, Griff an der ersten offenen Fläche nach innen) und `draft` (Wert `angle`, Verweis `faces`, versteckte Ebene und Zugrichtung) in `src/features/src/shell_draft.cpp`.
- Meldungen: „Die Wand ist dicker als der Körper an seiner dünnsten Stelle (%1 mm)“ mit der größten Wand und dem Knopf „Größtmöglichen Wert verwenden“; „Aushöhlen hat nicht sicher geklappt“ (nichts wird gemacht).
- Oberfläche: Menü „Körper ▸ Aushöhlen / Schräge“, Befehlssuche mit Synonymen, Rezeptsätze. Aushöhlen mit nur einem gewählten Körper lässt die Oberseite offen, mit gewählten Flächen diese; ohne Auswahl der Seiten gewinnt bei „Schräge“ der Körper (alle Seitenflächen), 3° zum Start.
- Tests: `tests/unit/geometry/test_plausibility.cpp`, `test_hollow.cpp`, `test_draft.cpp`, `tests/unit/features/test_shell_draft.cpp`, `tests/unit/ui/test_push_face_ui.cpp`.

Offen:
- **Je Fläche andere Stärke** (▸ Mehr), Schnittansicht zum Prüfen der Wand (AP-2.11).
- **`insetParams`** (Innenkörper aus dem Rezept des Eingangs neu bauen) ist nicht gebaut: der Kern erkennt Quader und Zylinder an ihrer Form, nicht am Rezept. Körper mit Rundungen aus `fillet`/`chamfer`, Extrusionen und aus Verbinden entstandene Körper laufen über den Bibliotheks-Weg mit Prüfung und kommen oft mit „nicht sicher“ zurück. Der klassische Fall (Quader mit allen Kanten gerundet) ist als Test festgehalten: Ergebnis entweder geprüft oder abgelehnt, nie falsch.
- **Geschlossener Hohlkörper** nur für Quader und Zylinder; die Oberfläche bietet ihn nicht an (ohne Auswahl bleibt die Oberseite offen).
- **Werkzeug „Aushöhlen“** als eigener Modus (Klick auf Körper, dann Flächen anklicken) fehlt: heute wählt man Fläche(n) oder Körper und ruft den Befehl auf; dann steht der Griff der Wand. Schräge: Griff/Bogen für den Winkel fehlt (Winkel wird in der Leiste getippt), neutrale Ebene und Zugrichtung sind fest (Boden, nach oben) und nicht wählbar.
- Fenster-Test der Griffe, Schnittbild zum Prüfen (AP-2.11).
