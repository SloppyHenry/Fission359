# AP-4.6 Relief und Lithophanie

**Größe:** M · **Phasendatei:** AP-4.6

## Ziel
Ein Bild wird zur Höhenkarte und damit zum **Relief** (eben) oder zur **Lithophanie** (eben, gebogen, Zylinder/Lampenschirm, Kugel) mit Min-/Max-Dicke,
Rahmen und einer Vorschau „gegen Licht“, die qualitativ dem Druck entspricht. Reliefs lassen sich auf Flächen eines Körpers aufbringen (z. B. Deckel).

## Voraussetzungen
AP-4.1 (Ergebnis frei).

## Vorher lesen
`docs/03-architektur.md` §7 (Bilder in `assets/`); AP-4.1, AP-4.4 (Projektion auf Flächen).

## Haltepunkte
1. **Auflösung:** Empfehlung: Punktabstand = Düse/2 (0,2 mm bei 0,4-Düse), Bild wird entsprechend skaliert; Höchstzahl Dreiecke 2 Mio. (sonst
   Hinweis und gröber). Bestätigen.

## Schritte
1. Feature-Typ `heightmap` (Asset = Bild, Werte Breite, minDicke, maxDicke, Rahmenbreite, Wort `shape` flat|curved|cylinder|sphere, Krümmungsradius,
   Wort `invert`; Lithophanie = dunkel dick). Netz direkt als Gitter erzeugen (geschlossen: Rückseite + Rand), Ergebnis frei.
2. Auf Fläche aufbringen: Höhenfeld auf eine ebene Fläche eines Körpers (präzise Fläche → Netz-Kombination, AP-4.1).
3. Vorschau „gegen Licht“: Renderer-Darstellung mit Hintergrundlicht (Durchlicht ~ exp(−k·Dicke)); Schalter in der Ansicht.
4. Bedienung: Bild ziehen ins Fenster oder Deko ▸ Relief; Karte mit Reglern; Regel 10.

## Tests
Graustufen-Verlauf → Dicken linear zwischen min und max (Messung an Gitterpunkten); Zylinder-Lithophanie geschlossen und wasserdicht; Volumen
plausibel; Dreieck-Grenze greift.

## Abnahme
Kriterium (Vorschau vs. Druck) mit Foto des Product Owners. So probierst du es aus: Foto ▸ Lithophanie ▸ Zylinder Ø 80 → Lampenschirm, „gegen Licht“
zeigt das Bild.

## Nicht tun
Keine Farb-Lithophanien (mehrfarbig) in diesem AP.
