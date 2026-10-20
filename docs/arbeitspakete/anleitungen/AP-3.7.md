# AP-3.7 Große Modelle teilen

**Größe:** M · **Phasendatei:** AP-3.7

## Ziel
Ein Körper, der nicht aufs Bett passt, lässt sich an Ebene(n) teilen; das Programm schlägt das bei „passt nicht aufs Bett“ selbst vor. Verbinder
werden automatisch ergänzt: **Zapfen** (mit Spiel), **Schwalbenschwanz**, **Stiftlöcher** für Filament-Stücke oder Magnete. Alles bleibt im Rezept
änderbar. Ein 400 mm langes Teil wird für ein 256-mm-Bett in 2 Teile mit Zapfen geteilt.

## Voraussetzungen
AP-1.7e (Teilen), AP-3.3 (Spiel), AP-3.2 (Bett-Prüfung).

## Vorher lesen
AP-1.7-Anleitung Teil e (`split`, Ausweich-Schritt „Verschoben“), AP-3.3 (Spiel im aufnehmenden Teil), AP-3.2.

## Haltepunkte
1. **Anordnung nach dem Teilen:** Empfehlung: Die Teile bleiben im Modell an ihrem Platz (zusammengesetzt sichtbar); für den Druck legt der Export
   sie automatisch nebeneinander auf je ein Bett/eine Platte (3MF mit mehreren Platten, soweit die Slicer das lesen). Alternative: Teile im Modell
   auseinanderlegen (dann sieht man das Ganze nicht mehr).
2. **Magnet-Größen:** gängige Scheibenmagnete 6×2, 8×3, 10×3 mm als Auswahl; bestätigen.

## Schritte
1. Vorschlag: Druckcheck-Fund „passt nicht aufs Bett“ bietet „Teilen“ an: kleinste Anzahl Schnitte senkrecht zur längsten Achse, Lage so, dass
   jedes Teil passt und der Schnitt eine einfache Querschnittsfläche hat (Suche wie `plainCut` aus AP-2.4).
2. Feature-Typ `splitForPrint` oder Ausbau von `split`: mehrere Ebenen, je Ebene Verbinder-Art (none | pegs | dovetail | pinHoles | magnets) mit
   Werten (Anzahl, Größe, Spiel aus Profil). Verbinder werden im Querschnitt platziert (größte freie Kreise im Querschnitt: Punkte mit maximalem
   Abstand zum Rand, einfacher Raster-Ansatz), Zapfen an Teil A angesetzt, Loch (+Spiel) in Teil B.
3. Griffe: Schnittebene verschieben (Pfeil), Verbinder anklicken zum Auslassen.
4. Export nach Haltepunkt 1.

## Tests
Quader 400×40×20, Bett 256 → 2 Teile, jedes ≤ 256, 2 Zapfen Ø 6 mit 0,2 Spiel (Golden: Volumen Teil A + Teil B = Original + Zapfen − Löcher);
Schnittebene verschieben → Teile folgen; Undo.

## Abnahme
So probierst du es aus: Quader 400 mm lang → Druckcheck rot „passt nicht aufs Bett“ [Teilen] → zwei Teile mit Zapfen, Export 3MF mit zwei Platten.

## Nicht tun
Keine Teilung entlang freier Kurven. Keine Puzzle-Verbinder.
