# AP-2.12 Hilfsgeometrie

**Größe:** S · **Phasendatei:** AP-2.12

## Ziel
Hilfsebenen (versetzt, gewinkelt, durch 3 Punkte, tangential an eine Rundung, Mittelebene zwischen zwei Flächen), Hilfsachsen und Hilfspunkte,
um dort zu skizzieren, zu teilen oder zu spiegeln. Dezent dargestellt, im Rezept als Schritt sichtbar, ein- und ausblendbar, nie gedruckt
oder exportiert.

## Voraussetzungen
AP-2.1 (Skizzen brauchen Ebenen), AP-2.10 (Hilfsebenen beziehen sich auf Flächen).

## Vorher lesen
`docs/02-ux-konzept.md` §12 (Hilfsebene/Helper Plane), AP-2.1-Anleitung (`SketchPlane`), `docs/adr/0003`.

## Haltepunkte
1. **Darstellung:** Empfehlung: halbtransparentes Rechteck mit gestricheltem Rand (Größe passend zum bezogenen Körper, Griffe an den Ecken
   nur zur Darstellung), Achsen als gestrichelte Linie, Punkte als kleines Kreuz. Mockup/Bild vorher.

## Schritte
1. Feature-Typen `helperPlane` (Wort `mode`: offset | angle | threePoints | tangent | midPlane; Werte je Modus: `offset` mm, `angle` Grad;
   `refs`: Fläche(n)/Kante/Punkte), `helperAxis` (durch 2 Punkte, Kante, Zylinderachse), `helperPoint` (Ecke, Mitte, Koordinaten).
   `newBodies = 0`; Ergebnis ist eine **Bezugsgeometrie** (neue Ergebnis-Art `RecomputeResult::datums`, Ebene = Ursprung + Normale +
   u-Achse). Namen `F9/helperPlane/plane`.
2. Rechnungen rein in der GAL (`geo/datum.h`): Ebene aus 3 Punkten, Mittelebene aus zwei parallelen Ebenen (nicht parallel → Fehler
   `datumNotParallel`), Tangentialebene an Zylinder in einem Winkel, gewinkelte Ebene um eine Kante. Tests je Fall mit Zahlen.
3. Oberfläche: Werkzeuggruppe (nach Mockup), Hinweiszeile führt durch die Klicks („Klicke die Fläche, von der aus versetzt wird“), Griff für
   Abstand/Winkel (Pfeil bzw. Ring wie Gizmo), Tippen ersetzt Ziehen. Regel 10.
4. Skizze (AP-2.1) und Teilen (AP-1.7e) akzeptieren eine Hilfsebene als Ebene. Spiegeln (AP-2.8) ebenso.
5. Rezept: Schritt mit eigenem Symbol; Auge blendet aus (Ausblenden der **Anzeige**, nicht `suppressed` – Haltepunkt, falls unklar: Empfehlung
   eigene Sichtbarkeit wie bei Körpern).
6. Export/Druckcheck ignorieren Bezugsgeometrie (Test).

## Tests
Golden für jede Ebenenart (Normale, Ursprung); Verweis bleibt nach Maßänderung am Körper (Naming-Szenario, AP-2.10-Suite ergänzen); Undo.

## Abnahme
So probierst du es aus: Quader, Hilfsebene „versetzt“ 10 mm von der Oberseite → Ebene schwebt darüber; Skizze darauf. Quader höher → Ebene
wandert mit.

## Nicht tun
Keine Hilfsgeometrie im Export. Keine Hilfskörper.
