# AP-4.5 SVG-, DXF- und Bild-Import für Konturen, Ausstechformen

**Größe:** M · **Phasendatei:** AP-4.5

## Ziel
SVG/DXF werden zu Skizzen (Kurven bleiben Kurven), beim Import auf ein Zielmaß skaliert. **Bild nachzeichnen**: eine Schwarz-Weiß-Zeichnung (auch ein
Foto einer Kinderzeichnung) wird zur Kontur. **Ausstechform-Generator**: Kontur → dünne hohe Schneide + breite flache Griffkante, Wandstärke aus dem
Profil. Von der Kinderzeichnung zur Ausstechform in unter 2 Minuten.

## Voraussetzungen
AP-2.1 (Skizzen). Ausstechform: AP-2.3 (Ziehen), AP-3.1 (Mindestwand).

## Vorher lesen
`docs/03-architektur.md` §7, §8 (nanosvg, stb_image), Backlog B-17 (Nachzeichnen in Skizzen: verwandt, nicht hier).

## Haltepunkte
1. **potrace** (GPL-2.0-or-later, kompatibel) einbinden **oder** eigene Umsetzung (Schwelle, Marching Squares, Kurvenglättung)? Phasendatei verlangt
   Entscheidung nach Qualitätsvergleich: beides an 10 Testbildern vergleichen (Bilder + Ergebnisse in den PR), dann ADR. Empfehlung vorab: eigene
   Umsetzung, wenn sie bei den Testbildern gleichwertig ist (keine neue Abhängigkeit).
2. **Ausstechform-Maße:** Schneide 0,8–1,2 mm (mind. 2 × Düse), Höhe 12 mm, Griffkante 6 mm breit, 2 mm hoch. Bestätigen.

## Schritte
1. SVG → Skizze: nanosvg liefert kubische Béziers; in Skizzen-Splines bzw. Linien/Bögen wandeln; Einheiten (viewBox, mm/px) beachten. DXF: einfacher
   Leser für LINE, ARC, CIRCLE, LWPOLYLINE, SPLINE (eigener Leser, kein neues Paket). Dialog-frei: Import-Karte mit Zielbreite (Zahlenfeld), Seitenverhältnis fest.
2. Bild nachzeichnen: Bild laden (stb_image), Graustufen, Schwelle (automatisch Otsu + Regler), Flecken unter Mindestgröße entfernen, Kontur (Marching
   Squares), Glättung + Kurven-Anpassung (Douglas-Peucker + Bézier-Fit), Ergebnis Skizze. Vorschau live beim Regler.
3. Ausstechform-Generator (Feature-Typ `cookieCutter`, Eingang Skizzen-Bereich): Außenkontur nach innen/außen versetzen (Schneide), zweite Versetzung
   (Griffkante), zwei Extrusionen + Vereinen. Spitze Innenecken: Versatz rundet sie ab (Hinweis bei Details < Wandstärke).
4. Regel 10, Meldungen (offene Pfade, zu feine Details), Baukästen-tauglich (AP-5.13).

## Tests
SVG-Kreis 50 mm → Skizzen-Kreis r 25; DXF-Rechteck; Testbild (schwarzer Kreis auf weiß) → Kontur mit Fläche ±1 %; Ausstechform aus Stern → gültig,
Wand = Vorgabe (Messung).

## Abnahme
So probierst du es aus: Foto einer Zeichnung importieren, Schwelle schieben, Breite 80 mm, „Ausstechform“ → druckbare Form. Stoppuhr < 2 min.

## Nicht tun
Keine Farbbilder in mehrere Ebenen zerlegen (das ist Motiv-Bibliothek/Relief).
