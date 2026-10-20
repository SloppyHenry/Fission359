# AP-5.12 Baukasten „Figuren“

**Größe:** XL, vor dem Start zerlegen · **Phasendatei:** AP-5.12

## Ziel
Der Figuren-Baukasten nach `docs/07-baukaesten.md` (Baukasten 3): ① Grundfigur · ② Proportionen · ③ Gesicht · ④ Zubehör · ⑤ Sockel und Zweck ·
⑥ Drucken. Niedliche, stilisierte Figuren (Schneemann, Wichtel, Engel, Rentier, Pinguin, Eule, Katze, Roboter, „eigene Figur aus Formen“) aus
weich verschmolzenen Körperteilen. Gesicht und Zubehör geprägt oder als farbige Einsetzteile mit Passung. Zwecke: Aufsteller, Christbaumanhänger (Öse am
Schwerpunkt), Magnet, Platzkarte. Standfestigkeits-Check mit Lösung. Automatisches Zerlegen in druckbare Teile mit Steckzapfen.

## Voraussetzungen
AP-5.8, AP-4.2 (Weich verschmelzen), AP-5.3 (Andockpunkte), AP-3.6 (Ausrichtung), AP-3.7 (Teilen mit Zapfen), AP-3.3 (Passung).

## Zerlegung (vorschlagen, bestätigen lassen, je Teil ein PR mit eigener Anleitung nach diesem Muster)
1. **5.12a Figuren-Gerüst-Format:** Datei je Figur: Körperteile (analytische SDF-Grundformen aus AP-4.2 mit Werten), Verschmelz-Weichheit je Paar,
   Regler (Name, Bereich, Abbildung Regler → Werte mehrerer Teile, linear/Kurve), Andockpunkte (Gesicht, Kopf, Hand …) relativ zu Teilen. Schema +
   Validierung; eine Figur (Schneemann) als Beweis.
2. **5.12b Proportions-Regler:** Regler in Alltagssprache, direktes Ziehen an Körperteilen (Griff je Teil → Regler), Vorschau grob/fein (AP-4.2).
3. **5.12c Gesicht und Zubehör:** Stile als kleine Teile/Motive, geprägt oder Einsetzteil (Tasche + Teil mit Spiel, AP-3.3), Zubehör als Bausteine an
   Andockpunkten, die bei Proportionsänderungen mitwandern.
4. **5.12d Sockel, Zweck, Standfestigkeit:** Schwerpunkt (Volumen-Schwerpunkt bei gleichmäßiger Dichte) über der Standfläche (konvexe Hülle der
   Bettkontakt-Fläche); Lösungen „Sockel vergrößern“, „Gewicht unten“. Öse am Schwerpunkt (oberhalb, senkrecht darüber).
5. **5.12e Automatisches Zerlegen:** Teile nach Farbe/Stützfreiheit trennen (Schnitt an Teil-Grenzen der Figur), Steckzapfen (AP-3.7), Ausrichtung je Teil (AP-3.6).
6. **5.12f Start-Figuren:** die 8 Figuren als Dateien + „eigene Figur aus Formen“.

## Haltepunkte
1. Zerlegung oben bestätigen. 2. Stil der Figuren: Bilder/Skizzen der 8 Start-Figuren vorab zeigen.

## Tests (Akzeptanz)
Jede Start-Figur über den gesamten Reglerbereich druckbar und standfest (Property-Test, Druckcheck ohne rote Funde); Wichtel mit Mütze in anderer Farbe,
gesteckt, passt gedruckt ohne Nacharbeit (Product Owner, Foto).

## Nicht tun
Keine realistischen Menschen (UX/Konzept: kein Sculpting). Keine Figuren als Netz-Vorlagen von außen.
