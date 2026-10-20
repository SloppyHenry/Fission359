# AP-3.2 Druckcheck

**Größe:** L, Teile a–c · **Phasendatei:** AP-3.2

## Ziel
Fission3D warnt, **bevor** gedruckt wird: nicht geschlossen, sich selbst schneidend, **Wand zu dünn** (mit Heatmap), **Überhänge** über 45°
(oder Profilwert, farbig), **Details kleiner als die Düse**, passt nicht aufs Bett, schwebende Teile, sehr dünne Spitzen. Eine Ampel fasst zusammen,
eine Liste erklärt, „Stelle zeigen“ springt hin, Lösungen werden angeboten. Läuft im Hintergrund, aktualisiert sich nach Änderungen.
500 000 Dreiecke in < 2 s.

## Voraussetzungen
AP-3.1 (Profilwerte), AP-2.11 (Analyse-Bausteine). Für Lösungen: AP-2.6 (Aushöhlen-Stärke), AP-2.5 (Fase), AP-1.6 (drehen).

## Vorher lesen
`docs/02-ux-konzept.md` §9 (Beispiel „Die Wand ist hier nur 0,3 mm dick“), §15 A Schritt 7, B Schritt 6; `docs/03-architektur.md` §8 (Embree optional),
§12; `docs/spikes/kernel-robustheit.md` („Berührung nur an Kante“).

## Haltepunkte
1. **Embree** (Apache-2.0, schnelle Strahlabfragen) als neue Abhängigkeit für die Wandstärke? Empfehlung: zuerst eigener BVH (Strahl-Dreieck,
   SAH-Baum) in `src/analysis`; Embree nur, wenn das Budget (500k in < 2 s) damit nicht erreicht wird – dann ADR.
2. **Grenzwerte:** Überhang 45° (Profil), Mindestwand = Profil `minWall` (Standard 2 × Düse), Details < Düse, Spitzen dünner als Düse über mehr als
   2 mm Länge. Bestätigen.

## Teil 3.2a – Analysen (rein, ohne Fenster)
In `src/analysis` auf dem **Druck-Netz** (Export-Tessellierung „Normal“):
1. Geschlossenheit/Manifold (jede Kante genau 2 Dreiecke), Selbstüberschneidung (BVH-Paare, Dreieck-Dreieck-Test).
2. Wandstärke: je Dreieck Strahl entgegen der Normalen, Treffer mit antiparalleler Normale (Skalarprodukt < −0,85) = Wand; Ergebnis je Dreieck,
   Treffer in der Bettebene ignorieren (sonst entstehen Randartefakte); schräge Treffer (keine antiparallele Normale) gesondert als „Keil/Grat“
   werten, nicht als dünne Wand.
3. Überhang: Winkel der Normalen gegen −Z; waagerechte Unterseiten über dem Bett = Brücke (eigene Kategorie); Dreiecke auf z = 0 sind Bettkontakt.
4. Schwebend: Zusammenhangskomponenten (Körper), deren tiefster Punkt > 0,05 mm über dem Bett liegt und nicht auf anderem aufliegt; lokale
   Z-Minima über dem Bett = Startpunkte in der Luft.
5. Bett: Hüllquader gegen Bauraum des Profils.
6. Kleine Details: Bereiche, deren Wandstärke < Düse; Spitzen (Fläche mit großer Krümmung) – einfache Variante: Wandstärke < Düse auf
   zusammenhängender Länge > 2 mm.
Ergebnis: `PrintReport { traffic (green|yellow|red); std::vector<Finding> findings; per-triangle values for heatmaps }`, `Finding { code, severity,
where (Punkt), numbers }`.
**Leistung:** BVH einmal je Netz; Strahlen parallel (`std::thread`/Thread-Pool), Abbruch-Token. Test mit 500k Dreiecken < 2 s (Release-Build).

## Teil 3.2b – Darstellung
1. Werkzeug/Panel „Druckcheck“ (Gruppe Prüfen): Ampel, Liste (je Fund Satz aus dem Meldungskatalog mit Zahlen: „Die Wand ist hier nur 0,3 mm dick.
   Dein Drucker (0,4-mm-Düse) kann Wände unter 0,8 mm nicht zuverlässig drucken.“), Knöpfe „Stelle zeigen“ (Kamera fährt hin, Fund leuchtet) und
   Lösung.
2. Heatmap-Overlay in der Ansicht (Wandstärke, Überhang) als Farbwerte je Dreieck (Renderer: zusätzliches Attribut oder eigener Durchgang); Legende.
   Farben je **Fläche/Dreieck**, nicht je Ecke (sonst Streifen).
3. Läuft nach jeder Neuberechnung im Hintergrund (abschaltbar in den Einstellungen); Anzeige veraltet → Hinweis „wird geprüft…“.

## Teil 3.2c – Lösungen und Testkatalog
1. Lösungen (wo möglich, je ein Command): Aushöhlen-Stärke erhöhen (wenn die dünne Wand aus `shell` stammt), „Fase statt Überhang“ (Hinweis mit
   Werkzeug-Start), Drehen (übergibt an AP-3.6, solange nicht da: Hinweis), aufs Bett legen (AP-1.6e), „zu groß fürs Bett → teilen“ (AP-3.7).
2. **Testkatalog** (Akzeptanzkriterium 1): 20 Problemteile als `.fission` in `tests/fixtures/printcheck/` (offenes Netz, Überschneidung, Wand 0,3,
   Überhang 60°, Brücke, schwebender Körper, zu groß, Kante-an-Kante, dünne Spitze, …) mit erwarteten Funden in `expected.json`. Test vergleicht.

## Abnahme
So probierst du es aus: Ablauf A Schritt 7 → alles grün. Aushöhlen mit 0,4 mm → rot, Heatmap zeigt dünne Wand, Lösung setzt 1,2 mm.

## Nicht tun
Keine Slicer-Simulation (Schichten, Stützen berechnen). Keine Festigkeitsanalyse (AP-3.6 macht nur eine Heuristik).
