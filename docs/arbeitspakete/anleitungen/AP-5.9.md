# AP-5.9 Motiv-Bibliothek

**Größe:** L · **Phasendatei:** AP-5.9

## Ziel
2D-Motive (SVG mit benannten Ebenen + Metadaten: Kategorie, Schlagwörter DE/EN, Autor, Quelle, Lizenz, Mindestgröße) in einer Bibliotheks-Leiste mit
Suche, Kategorien, Favoriten; eigene Motive importieren. Platzieren per Ziehen auf ebene und gekrümmte Flächen: erhaben, vertieft, durchbrochen,
aufgesetzt; Höhe und Farbe je Ebene; proportional skalieren, drehen, spiegeln. **Streuen** (Dichte, Größe, Drehung, Abstand, Seed, Rand). Jedes Motiv
ist bei seiner Mindestgröße druckbar (automatischer Test); jede Datei hat vollständige Lizenzangaben (CI). Start: ≥ 150 Motive, ≥ 50 Winter/Weihnachten.

## Voraussetzungen
AP-4.4 (Prägen/Gravieren auf Flächen), AP-4.5 (SVG-Import), AP-3.4 (Farben).

## Vorher lesen
`docs/07-baukaesten.md` „Motiv-Bibliothek“, `docs/03-architektur.md` §8 (Inhalte nur selbst erstellt, CC0, CC-BY-4.0, OFL; Metadaten + CI-Skript).

## Haltepunkte
1. **Herkunft der 150 Motive:** selbst zeichnen (Umsetzer erzeugt einfache SVGs programmatisch/von Hand) und CC0-Quellen (z. B. Openclipart = CC0;
   prüfen) – Liste der Quellen vom Product Owner bestätigen lassen. Kein Motiv ohne geprüfte Lizenz.
2. **Metadaten-Format:** Empfehlung: im SVG selbst (`<metadata>` mit eigenem Namensraum) **und** eine Indexdatei `resources/motifs/index.json` für schnelle
   Suche (vom Skript erzeugt).

## Schritte
1. Format + Prüfskript `scripts/tools/check_motifs.py`: Lizenz ∈ {selbst, CC0, CC-BY-4.0}, Autor, Quelle, Kategorie, Schlagwörter de/en, Mindestgröße;
   als Test in der CI.
2. Druckbarkeit je Motiv bei Mindestgröße: kleinste Linienbreite ≥ 2 × Düse (0,8 mm bei 0,4) – Messung über Rasterisierung (Distanztransformation) in
   einem Test (Akzeptanzkriterium 1). Verkleinern unter Mindestgröße → Hinweis mit Zahl.
3. Platzieren: Feature `motif` (Asset-Verweis, Fläche, Lage (u,v,Winkel,Größe,Spiegel), Art je Ebene, Höhe/Farbe je Ebene) über AP-4.4-Wege
   (eben präzise, Zylinder präzise, sonst Netz). Griffe: Ziehen (Lage), Ecke (Größe proportional, Regel 11), Ring (Drehung).
4. Streuen: Feature `scatter` (Motiv, Fläche, Dichte, Größenbereich, Drehung, Mindestabstand, Seed, Rand) – Poisson-Disk-Verteilung, deterministisch.
5. Bibliotheks-Leiste: wie Bausteine (AP-5.3) – Suche über Schlagwörter (DE/EN), Favoriten (Einstellungen), Import eigener SVGs (Lizenzfeld Pflicht:
   „eigenes Werk“).
6. Inhalte: 150 Motive in Kategorien laut Konzept; Erzeugungsskripte für geometrische Motive (Sterne, Schneeflocken, Rahmen) im Repo, damit sie
   reproduzierbar sind.

## Abnahme
Kriterien 1–2 (Tests). So probierst du es aus: Platte, Bibliothek ▸ Weihnachten ▸ Tannenbaum auf die Platte ziehen → erhaben; Ebene „Kugeln“ rot;
Schneeflocken streuen.

## Nicht tun
Keine Motive aus dem Netz ohne Lizenzprüfung. Kein „nur nicht-kommerziell“.
