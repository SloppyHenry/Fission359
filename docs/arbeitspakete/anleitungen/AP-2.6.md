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
