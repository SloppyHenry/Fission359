# AP-4.9 Kaleidoskop-Modus

**Größe:** M · **Phasendatei:** AP-4.9

## Ziel
Radiale Symmetrie (n-fach, optional gespiegelt) als **Live-Modus**: Man zeichnet oder modelliert in einem Segment, alle anderen aktualisieren sich in
Echtzeit. Geht in Skizzen (Schneeflocken, Mandalas) und mit Körpern. Eine eigene Schneeflocke gelingt in unter 3 Minuten von Hand.

## Voraussetzungen
AP-2.8 (Kreismuster, Spiegeln), AP-2.1 (Skizzen).

## Vorher lesen
AP-2.8-Anleitung (`circularPattern`, `mirror`); AP-2.1/2.2 (Skizzen-Datenmodell).

## Haltepunkte
1. **Umsetzung in Skizzen:** Empfehlung: Kaleidoskop ist eine **Eigenschaft der Skizze** (Werte `kaleidoN`, Wort `kaleidoMirror`, Mittelpunkt);
   gezeichnet wird im Segment, die Kopien sind abgeleitete Kurven (nicht editierbar, aber Teil der Bereichserkennung). Bei Körpern: ein Schritt
   `kaleidoscope` = `circularPattern` (+ `mirror`) über alle Körper, die nach dem Einschalten im Segment entstehen. Bestätigen.

## Schritte
1. Skizze: Segment-Grenzen gestrichelt anzeigen, Zeichnen nur im Segment (Hinweis, wenn außerhalb), Kopien live (Renderer zeichnet die abgeleiteten
   Kurven blass), Bereichserkennung (AP-2.1g) mit allen Kopien; Verbindung über Segmentgrenzen: Kurven, die die Grenze berühren, verschmelzen.
2. Körper: Modus einschalten (Werkzeug Muster ▸ Kaleidoskop), n und Spiegeln wählen; neue Körper im Segment bekommen automatisch das Muster.
3. Leistung: Live-Aktualisierung < 50 ms bei n = 12 und 200 Kurven (messen).
4. Regel 10 („kaleidoskop“, „mandala“, „symmetrie“, „schneeflocke“, „radial“).

## Tests
Linie im Segment, n = 6 → 6 Kopien mit richtigen Winkeln; gespiegelt → 12; Bereiche über Segmentgrenzen korrekt; Undo einer Zeichenaktion nimmt alle
Kopien mit (sie sind abgeleitet).

## Abnahme
So probierst du es aus: Skizze, Kaleidoskop 6 gespiegelt, ein paar Linien im Segment → Schneeflocke; Ziehen → Körper. Stoppuhr < 3 min.

## Nicht tun
Kein Kaleidoskop für Verformen/Verschmelzen in diesem AP.
