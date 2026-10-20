# AP-5.11 Baukasten „Vasen und Gefäße“

**Größe:** M · **Phasendatei:** AP-5.11

## Ziel
Der Vasen-Baukasten nach `docs/07-baukaesten.md` (Baukasten 2): ① Zweck und Größe · ② Silhouette · ③ Querschnitt · ④ Struktur · ⑤ Rand und Boden ·
⑥ Drucken. Zweck-Voreinstellungen (Blumenvase wasserdicht, Deko, Windlicht, Stiftebecher, Übertopf, Schale). Profilkurve direkt in der 3D-Ansicht mit
Griffen, Überhänge live markiert. Querschnitt-Übergang unten → oben, Motive als umlaufendes Relief. **Vasenmodus-Prüfung** und passende Export-Hinweise.

## Voraussetzungen
AP-5.8, AP-4.8 (Vasen-Generator), AP-4.3 (Verdrehen), AP-4.7 (Strukturen), AP-4.4/4.6 (Relief).

## Vorher lesen
`docs/07-baukaesten.md` Baukasten 2 (Pflichtumfang); AP-4.8 (Vasen-Generator, Vasenmodus-Prüfung).

## Haltepunkte
1. **Windlicht-Sicherheit:** Hinweis „Nur mit LED-Teelicht bei PLA“ fest im Schritt ① (Text bestätigen); Mindestabstand zur Flamme als Wert.
2. **Wasserdicht-Hinweise:** Text mit Empfehlungen (Wandlinien, Temperatur) bestätigen; keine Garantie formulieren.

## Schritte
1. `resources/kits/vases.json` mit allen Optionen; Zweck setzt Standards (Wand, Boden, Vasenmodus an/aus, Ablaufloch).
2. Profil-Griffe in der Ansicht: Stützpunkte der Silhouette als Punkte am Rand der Vase ziehen; Überhang > 45° am Profil rot markiert (Steigung je Abschnitt).
3. Querschnitt-Übergang: Loft zwischen unterem und oberem Querschnitt bzw. Interpolation im Generator.
4. Vasenmodus-Prüfung (aus AP-4.8): eine Wand, keine Inseln je Schicht, keine waagerechten Dächer → Ergebnis mit Erklärung; Export-Hinweis „Im Slicer
   Spiralmodus/Vasenmodus einschalten“.

## Tests
Jede Kombination Zweck × Silhouette druckbar (Property-Test); gedrehte Sternvase besteht die Vasenmodus-Prüfung.

## Abnahme
Gedrehte Sternvase im Vasenmodus gedruckt (Foto); Blumenvase 24 h dicht (Product Owner).

## Nicht tun
Keine Versprechen zur Lebensmittelechtheit.
