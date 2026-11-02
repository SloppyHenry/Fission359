# AP-2.8 Muster und Spiegeln

**Größe:** M · **Phasendatei:** AP-2.8

## Ziel
Etwas mehrfach anordnen: **Reihe** (1D/2D, Anzahl + Abstand oder Gesamtlänge), **Kreis** (Anzahl, Winkel, Achse), **entlang Pfad**, **Spiegeln**
(an Ebene oder Fläche). Das geht mit Features (z. B. Löchern) **und** mit Körpern. Einzelne Exemplare lassen sich per Klick auslassen. Ändert
man das Original, ändern sich alle. 100 Exemplare in unter 1 s.

## Voraussetzungen
AP-2.3, AP-2.10, AP-2.12 (Ebenen/Achsen), AP-1.7.

## Vorher lesen
`docs/02-ux-konzept.md` §3 (Gruppe Muster: Reihe, Kreis, Entlang Pfad, Spiegeln, Kaleidoskop – Letzteres AP-4.9), §12 (Muster/Repeat, Spiegeln),
§15 B Schritt 5; AP-2.7 (Löcher), `docs/adr/0003` (Namen der Exemplare).

## Haltepunkte
1. **Muster von Features:** Empfehlung: Ein Muster-Schritt wiederholt **das Ergebnis** eines Schritts (z. B. die Löcher eines `hole`-Schritts) als
   Geometrie-Operation (Werkzeug-Körper vervielfältigen und in einem Boolean anwenden), nicht den Schritt selbst. So bleibt das Rezept kurz.
   Bei `hole`-Schritten Sonderweg: Punkte vervielfältigen (billiger, Namen klarer). Bestätigen.
2. **Ausgelassene Exemplare** werden nach Index gespeichert (`skip: [3, 7]`); ändert sich die Anzahl, bleiben Indizes gleich. OK?

## Schritte
1. Reine Mathematik (GAL `geo/pattern.h`): Transformationen für Reihe (Richtung(en), Abstand, Anzahl), Kreis (Achse, Anzahl, Gesamtwinkel, gleichmäßig),
   Pfad (Punkte gleichen Abstands entlang einer Kurve + Ausrichtung), Spiegeln (Ebene). Tests mit Zahlen.
2. GAL-Operation: `NamedSolid pattern(const NamedSolid& tool, const std::vector<Transform>&, PatternApply)` – Werkzeug kopieren
   (`BRepBuilderAPI_Transform`, schnell, teilt Geometrie) und mit dem Ziel vereinen/abziehen in **einem** Boolean (viele Werkzeuge auf einmal:
   AP-0.5 zeigt 100 Löcher in 0,1 s). Namen der Exemplare: `…[i]` mit Index.
3. Feature-Typen `linearPattern`, `circularPattern`, `pathPattern`, `mirror`: Eingang (Körper oder Schritt-Verweis), Werte (Anzahl, Abstand, Winkel,
   Gesamtlänge – zwei Modi: „Abstand“ oder „verteilen über Länge“), `refs` (Richtung als Kante/Achse/Hauptachse), `skip`.
4. Bedienung: Werkzeuggruppe „Muster“; nach Wahl des Originals erscheinen Vorschau-Exemplare (Geister) und Griffe: Pfeil für Abstand,
   Zahl-Etikett für Anzahl (Tippen), bei Kreis ein Ring für den Gesamtwinkel. Klick auf ein Exemplar = auslassen/wieder an. Regel 10.
5. Spiegeln von Körpern: neuer Körper (Kopie gespiegelt) oder mit dem Original vereint (Wort `merge`).

## Tests
- Golden: 2D-Reihe 3×4 Löcher → Volumen = Platte − 12 × Loch; Kreis 6 × 60°; Spiegeln eines L-Körpers (Volumen ×2 bei getrennt, Vereinigung bei
  Berührung).
- **Akzeptanz 1:** Original-Loch Ø 5 → 6 → alle Exemplare Ø 6 (Volumen).
- **Akzeptanz 2:** 100 Exemplare < 1 s (Zeitmessung im Test, großzügige CI-Grenze 3 s, gemessener Wert im PR).
- Ablauf B Schritt 5 (2 Löcher, Abstand 30) als UI-Test.
- Naming: Rundung an einem Exemplar bleibt nach Anzahl-Änderung am richtigen Exemplar (Index), sonst „braucht Hilfe“.

## Abnahme
So probierst du es aus: Platte mit einem Loch, Muster ▸ Reihe, Anzahl `4`, Abstand `15` → vier Löcher; eines anklicken → ausgelassen.
Loch-Durchmesser ändern → alle ändern sich.

## Nicht tun
Kein Kaleidoskop-Live-Modus (AP-4.9). Keine Muster mit unterschiedlichen Größen je Exemplar (später „Variieren“).
