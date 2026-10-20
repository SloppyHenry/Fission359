# AP-5.13 Weitere Baukästen

**Größe:** M je Baukasten · **Phasendatei:** AP-5.13

## Ziel
Weitere Baukästen mit derselben Engine, in der Reihenfolge nach Nutzerfeedback aus den Baukästen 1–3: Christbaumschmuck und Anhänger, Windlicht und
Lampenschirm, Namens- und Türschilder, Ausstechformen und Stempel, Schmuck, Untersetzer und Schalen, Boxen und Schachteln (Schritte in
`docs/07-baukaesten.md` „Weitere Baukästen“). Jeder neue Baukasten ist **Datenarbeit**; fehlende Generatoren oder Bausteine sind eigene kleine APs.

## Voraussetzungen
AP-5.8, AP-5.9; je Baukasten die nötigen Werkzeuge (Lithophanie AP-4.6, Ausstechform AP-4.5, Box-Generator AP-4.8 …).

## Vorgehen je Baukasten (Anleitung zum Wiederholen)
1. **Haltepunkt:** Reihenfolge der Baukästen und Optionen je Schritt mit dem Product Owner festlegen (Tabelle aus `07-baukaesten.md` als Vorschlag).
2. Prüfen, welche Optionen mit vorhandenen Features/Bausteinen/Motiven gehen; Fehlendes als eigene Anleitung `AP-5.13-<baukasten>-<baustein>.md` anlegen
   und **vorher** bauen.
3. Baukasten-Datei `resources/kits/<id>.json` + Texte in `KitTexts.js`; Validierung grün.
4. Property-Test: alle Optionen druckbar; 3 Inspirations-Beispiele für die Startseite (AP-5.4).
5. Kachel auf der Startseite verbinden.

## Abnahme je Baukasten
So probierst du es aus: Startseite ▸ Kachel ▸ durch die Schritte; Ergebnis druckbar; Frei weiter und zurück.

## Nicht tun
Keinen Code für einen einzelnen Baukasten schreiben (Grundregel 9) – fehlt etwas, ist es ein Baustein/Generator.
