# AP-5.3 Bausteine-Bibliothek

**Größe:** L · **Phasendatei:** AP-5.3

## Ziel
Bausteine sind intelligente Fertigteile: ein gespeichertes Teil-Rezept mit freigegebenen Maßen und **Andockpunkten** („auf Fläche, Mitte“, „an Kante“,
„an Spitze“). Mitgeliefert: Aufhänger-Öse, Schraubenloch-Gruppe, Scharnier (Print-in-Place), Schnappverschluss, Kabelclip, Wandhalter-Lasche,
Magnettasche, Füße, Griffmulde, Box-Ecken, Gewindeeinsatz, Logo-Platte. Eigene Bausteine aus einer Auswahl erstellen und als `.fissionpart` teilen.
Bibliotheks-Leiste mit Suche und Vorschaubildern; Ziehen mit Andocken. Die Aufhänger-Öse rastet an einer Sternspitze ein und folgt ihr, wenn sich der
Stern ändert (Ablauf C Schritt 3).

## Voraussetzungen
AP-2.9 (Maße), AP-2.10 (Verweise: Andocken ist ein Verweis), AP-1.10 (Dateiformat), AP-4.8 (Stern als Generator).

## Vorher lesen
`docs/07-baukaesten.md` (Abgrenzung Baustein/Generator/Baukasten), `docs/02-ux-konzept.md` §3 (Bibliothek), §15 C; AP-2.10-Anleitung.

## Haltepunkte
1. **ADR „Bausteine im Rezept“.** Empfehlung: Ein Baustein wird beim Einfügen zu einer **Rezept-Gruppe** gewöhnlicher Schritte (Regel 3: alles bleibt
   änderbar) plus einem Schritt `dock` (Verweis auf das Andock-Element, Andockart, Versatz/Drehung), der die Lage der Gruppe bestimmt. Gruppen im
   Rezept (UX §4 „Gruppen“) entstehen damit erstmals: Datenmodell `Document::groups` (Name, Schritt-Ids). Alternative: Baustein als ein einziger
   Schritt mit eingebettetem Rezept (kompakter, aber innen nicht frei änderbar).
2. **Andockpunkte für Spitzen:** „Spitze“ eines Körpers (Ecke mit kleinstem Öffnungswinkel in einer Richtung) ist für den Stern nötig. Erkennung per
   Ecken-Namen aus dem Generator (`tip[i]`) statt Geometrie-Suche. Bestätigen.

## Schritte
1. **Format `.fissionpart`** = `.fission`-Container mit zusätzlichem `part.json`: freigegebene Maße (Name, Grenzen), Andockpunkte (Art, Element-Name im
   Baustein, Ausrichtung), Vorschaubild, Metadaten (Autor, Lizenz CC0/CC-BY). Roundtrip-Test.
2. **Einfügen + Andocken:** Ziehen aus der Bibliothek; während des Ziehens sucht das Programm passende Ziele (Flächen, Kanten, Spitzen nach Andockart),
   rastet mit Vorschau ein; Loslassen = Gruppe + `dock`-Schritt (ein Undo-Schritt). Ändert sich das Ziel (Stern 8 → 6 Zacken), löst AP-2.10 den Verweis
   auf (Spitze `tip[i]` – bei weniger Zacken: „braucht Hilfe“, wenn i nicht mehr existiert; Haltepunkt, falls stattdessen „nächste Spitze“ gewünscht).
3. **Mitgelieferte Bausteine** als Dateien in `resources/parts/`, jeder mit Vorschaubild, Grenzen, Golden-Test (gültig über den Bereich der Maße).
4. **Eigene Bausteine:** Auswahl (Körper/Schritte) ▸ „Als Baustein speichern“: Maße freigeben (Liste mit Häkchen), Andockpunkt wählen (Fläche/Kante),
   Name, speichern in den Benutzerordner; Teilen = Datei.
5. **Bibliotheks-Leiste** (links, nach Mockup): Kategorien, Suche (Befehlssuche-Logik aus AP-1.12), große Vorschaukacheln.

## Tests
Öse an Sternspitze, Stern ändern (Radius) → Öse folgt (Lage gemessen); Scharnier Print-in-Place: Spalt ≥ Spiel aus Profil; jeder mitgelieferte Baustein
lädt und ist gültig; eigener Baustein Roundtrip.

## Abnahme
Ablauf C Schritt 3 als UI-Test. So probierst du es aus: Stern, Bibliothek ▸ Aufhänger-Öse an eine Spitze ziehen → rastet ein; Zacken-Radius ändern → Öse wandert mit.

## Nicht tun
Keine Online-Bibliothek (Backlog B-9). Keine Bausteine mit fremden Lizenzen.
