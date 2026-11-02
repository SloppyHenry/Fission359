# AP-3.6 Druckausrichtung vorschlagen

**Größe:** M · **Phasendatei:** AP-3.6

## Ziel
Das Programm schlägt gute Lagen für den Druck vor: große ebene Flächen nach unten, wenig Überhang, gute Haftung, geringe Höhe und – mit kurzer
Erklärung – **Festigkeit** (Last nicht quer zu den Schichten). Vorschläge als Kärtchen mit Mini-Vorschau; Übernehmen ändert die Drehung im Rezept.
Bei der Wandhalterung (Ablauf B) wird die festigkeitsgünstige Lage vorgeschlagen und begründet.

## Voraussetzungen
AP-3.2 (Überhang-Analyse), AP-1.6 (Drehung im Rezept, ADR-0010).

## Vorher lesen
`docs/02-ux-konzept.md` §15 B Schritt 6 („Für mehr Festigkeit auf die Seite legen.“); AP-3.2-Anleitung; ADR-0010 (Drehung = drei Winkel am Schritt).

## Haltepunkte
1. **Festigkeit:** Empfehlung einfache, erklärbare Heuristik: Der Nutzer gibt optional die **Lastrichtung** an (Pfeil an einer Fläche: „hier hängt
   etwas dran“); ohne Angabe nimmt das Programm bei Halterungen (Körper mit Bohrungen in einer Fläche) an, dass die Last senkrecht zur
   Bohrungsfläche wirkt. Bewertung: Schichten sollen nicht senkrecht zur Biegespannung liegen → Lage, in der die Biegekante parallel zu den
   Schichten liegt, wird bevorzugt. Erklärungstext je Vorschlag. Bestätigen.
2. **Kombinierte Körper/mehrere Körper:** Empfehlung: Vorschlag je gewähltem Körper.

## Schritte
1. Kandidaten (rein, `src/analysis/orientation.*`): je große ebene Fläche (Top 10 nach Größe) „diese Fläche nach unten“, dazu die 6 Hauptlagen;
   Duplikate entfernen.
2. Bewertung je Kandidat: Überhangfläche (aus AP-3.2, auf gedrehtem Netz), Kontaktfläche zum Bett, Höhe, Festigkeit (Haltepunkt 1); gewichtete
   Summe mit festen Gewichten (Konstanten, im PR begründet). Ergebnis: Top 3 mit Gründen als Codes (`lessOverhang`, `bigBase`, `stronger`, `lower`).
3. Drehung für „Fläche nach unten“: `geo::turnOntoFace` (gibt es, AP-1.6b3) bzw. Winkel aus der Normale; danach aufs Bett legen (AP-1.6e).
4. Oberfläche: „Druckausrichtung“ im Druckcheck-Panel und als Lösung bei Überhang-Funden; Kärtchen mit Mini-Vorschau (Offscreen-Bild klein) und
   Sätzen („Weniger Überhang: 2 % statt 31 %“, „Stabiler: die Schichten liegen quer zur Last“). Übernehmen = ein Undo-Schritt (Drehung + z).

## Tests
Würfel mit Fase → Fase oben; L-Winkel → auf der langen Seite; Wandhalterung (Ablauf B) → festigkeitsgünstige Lage auf Platz 1 mit Grund `stronger`
(Akzeptanzkriterium); Übernehmen ändert nur `rotX/rotY/rotZ` und `z`.

## Abnahme
So probierst du es aus: Wandhalterung, Druckcheck → „Für mehr Festigkeit auf die Seite legen“ [Ausrichten] → liegt auf der Seite.

## Nicht tun
Keine FEM-Berechnung. Keine automatische Ausrichtung ohne Klick.
