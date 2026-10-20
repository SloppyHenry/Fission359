# AP-3.4 Farben und Mehrfarbdruck

**Größe:** M, Teile a–d · **Phasendatei:** AP-3.4

## Ziel
Farbe je Körper (gibt es) **und je Fläche** (z. B. eine Gravur farbig). Eine Filament-Palette passend zum Drucker (z. B. AMS mit 4 Farben),
Farben als Filamente benannt („Rot PLA“). **Farbwechsel nach Höhe** für Drucker mit einer Düse. 3MF so exportieren, dass Bambu Studio, OrcaSlicer
und PrusaSlicer Objekte und Farben ohne Nacharbeit übernehmen.

## Voraussetzungen
AP-1.10 (3MF-Export), AP-3.1 (Anzahl Farben im Profil), AP-1.8c (Farbe je Körper).

## Vorher lesen
`docs/02-ux-konzept.md` §15 A Schritte 6 und 8, C Schritt 5; `docs/03-architektur.md` §7 (3MF mit lib3mf).
**Wissen aus einem früheren Projekt des Product Owners** (Bambu-CLI, Blender, 2026-09): Eine selbst geschriebene 3MF ohne Projekt-Einstellungen
lässt Bambu Studio abstürzen bzw. ignoriert die Filamentzuordnung; verlässlich war, die Zuordnung über die Objekt-Metadaten (`extruder` am
**Objekt**, nicht nur am Teil) zu setzen und die Anzahl Filamente über eine Farbliste mitzugeben. Details müssen hier neu geprüft werden.

## Haltepunkte
1. **Bambu/Orca-Projektdaten:** Bambu und Orca lesen eigene Metadaten (`Metadata/model_settings.config`, `project_settings.config`). Empfehlung:
   3MF-Kern mit lib3mf (Standard: Objekte + `BaseMaterials`/`ColorGroup`), dazu **minimal** die Bambu-Metadaten für Objekt→Filament; nichts
   Weiteres (keine Druckeinstellungen). Je Slicer prüfen und Einschränkungen dokumentieren (Kriterium erlaubt das).
2. **Farbwechsel nach Höhe:** Empfehlung: Wert im Dokument (Liste „ab Höhe X mm Farbe Y“), Export als PrusaSlicer-Farbwechsel (`Metadata/
   Prusa_Slicer_custom_gcode_per_print_z.xml`) und Bambu/Orca-Pause/Filamentwechsel-Metadaten, wo bekannt; sonst Anleitung als Text („Bei 2,4 mm
   Filament wechseln“) im Export.

## Teil 3.4a – Farbe je Fläche
Core: Farben je Fläche als Liste am Körper (`Body::faceColors`: Flächen**name** → Farbe; Namen statt Nummern, damit sie Änderungen überstehen,
AP-2.10). Command, Undo, Dateiformat (Roundtrip + Migration). Renderer: Farbe je Dreieck aus der Flächen-Id. Werkzeug „Färben“ (Fläche anklicken).
Gravur/Prägung (AP-4.4) liefert benannte Flächen → färbbar.

## Teil 3.4b – Filament-Palette
Palette = Profil-Anzahl Farben (AMS 4, mit 2 AMS 8; Einzeldüse 1 + Farbwechsel nach Höhe). Je Platz Name, Farbe, Material. Körper/Flächen werden einem
**Platz** zugeordnet, nicht einer freien Farbe (Farbwahl aus AP-1.8 bleibt für die Anzeige; Haltepunkt, wie beides zusammenspielt: Empfehlung,
Farbwahl wählt automatisch den nächstliegenden Palettenplatz bzw. legt einen an, solange Plätze frei sind).

## Teil 3.4c – 3MF für drei Slicer
1. lib3mf: ein Objekt je Körper; bei Flächenfarben ein Objekt mit Dreiecks-Eigenschaften (`ColorGroup`); Bambu-Metadaten nach Haltepunkt 1.
2. Testdateien erzeugen (zweifarbiger Schlüsselanhänger aus Ablauf A), in allen drei Slicern öffnen (Product Owner bzw. Testrechner mit
   PrusaSlicer/Orca-CLI, falls vorhanden), Ergebnis-Tabelle je Slicer im PR (Akzeptanzkriterium).
3. Automatischer Test: 3MF wieder einlesen (lib3mf) → Anzahl Objekte, Farbzuordnung.

## Teil 3.4d – Farbwechsel nach Höhe
Panel „Farben“ (bei Einzeldüse): Liste „ab Höhe … Farbe …“, Vorschau in der Ansicht (Körper oberhalb in der neuen Farbe), Schichthöhe aus Profil
(Höhe auf Schichtgrenze runden und das sagen). Export nach Haltepunkt 2. Grundlage für Karten (AP-5.10).

## Abnahme
So probierst du es aus: Schlüsselanhänger mit rotem Text; Export 3MF; in Bambu Studio öffnen → zwei Filamente zugeordnet. Einzeldüsen-Profil:
Farbwechsel ab 2,4 mm → Vorschau zweifarbig, Export enthält den Wechsel.

## Nicht tun
Keine Farben in STL (geht nicht; Hinweis beim Export).
