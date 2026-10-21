# AP-4.4 Text und Logos: Prägen und Gravieren

**Größe:** L, Teile a–d · **Phasendatei:** AP-4.4

## Ziel
Text (und SVG-Logos) **erhaben prägen** oder **vertieft gravieren**: auf ebenen Flächen präzise; auf Zylindern präzise über Abwicklung; auf beliebigen
Flächen über Netz-Projektion. Text auf Pfad und Kreisbogen, mehrere Zeilen, Ausrichtung, Laufweite, Fett/Kursiv. Mitgelieferte, gut druckbare freie
Schriften, darunter eine mit Stegen für Schablonen. „2026“ auf einer Kugel graviert (Ablauf C) und auf einem Becher präzise geprägt.

## Voraussetzungen
AP-1.5 (Text-Form), AP-4.1 (Netze für beliebige Flächen), AP-2.1 (Pfade aus Skizzen), AP-2.10 (Fläche als Verweis).

## Vorher lesen
`docs/02-ux-konzept.md` §12 (Prägen/Gravieren), §15 A Schritt 4, C Schritt 4; `src/geometry/occt/src/text.cpp`, `fonts.cpp` (Text-Konturen gibt es),
`docs/03-architektur.md` §8 (Inhalte nur OFL/CC0/CC-BY).

## Haltepunkte
1. **Schriften:** Vorschlag 3–5 OFL-Schriften mit guter Druckbarkeit (z. B. eine fette Grotesk, eine runde, eine Schablonenschrift mit Stegen,
   eine Schreibschrift mit verbundenen Buchstaben). Auswahl vom Product Owner bestätigen lassen; Lizenzdateien mitliefern, `THIRD_PARTY_NOTICES.md`.
2. **Prägen auf einen vorhandenen Text-Körper oder eigener Schritt?** Empfehlung: eigener Feature-Typ `emboss` (Eingang Ziel-Körper, Wörter Text/Schrift,
   Verweis Fläche, Werte Höhe/Tiefe, Größe, Lage auf der Fläche (u, v, Winkel)); die bisherige Form „Text“ bleibt für frei stehende Schrift.

## Teil 4.4a – Text-Layout
Zeilen, Ausrichtung (links/mitte/rechts), Laufweite, Zeilenabstand; Text auf Pfad (Skizzen-Kurve) und Kreisbogen: Glyphen entlang der Kurve platzieren
(Position + Tangentenwinkel je Glyphe). Reine Rechnung in der GAL, getestet mit Zahlen.

## Teil 4.4b – Ebene Flächen (präzise)
Glyphen-Profile auf der Fläche (Ebenen-Koordinaten wie Skizze), `extrude` + `combine` (merge = prägen, subtract = gravieren). Namen `F<n>/emboss/glyph<i>Top`
usw. (färbbar, AP-3.4). Golden: „LENA“ 1,5 mm auf Platte (Volumen = Platte + Glyphenfläche × Höhe).

## Teil 4.4c – Zylinder (präzise)
Abwicklung: Glyphen-Konturen in (Winkel, Höhe) umrechnen und auf die Zylinderfläche wickeln (OCCT: Kurven auf Fläche, `BRepBuilderAPI_MakeFace` auf
Zylinderfläche, dann radial prismatisch nach außen/innen); Golden mit Volumen-Formel für einfache Glyphe („I“). Ablauf: „2026“ auf Becher.

## Teil 4.4d – Beliebige Flächen (Netz)
Glyphen als Höhenfeld projizieren (Strahlen von der Textebene auf die Fläche), Ziel wird frei (Badge). Ablauf C Schritt 4 (Kugel) als Test:
wasserdicht, Gravurtiefe gemessen.

## SVG-Logos
Gleiche Wege mit SVG-Konturen (nanosvg ist eingebunden; Kurven → Profile). Ungültige/offene Pfade → Meldung.

## Abnahme
Kriterium (Kugel und Becher) als Tests + Bild. So probierst du es aus: Becher (Röhre), Deko ▸ Prägen, „2026“ auf die Außenseite → präzise; Kugel ▸
Gravieren „2026“.

## Nicht tun
Keine Schriften ohne OFL. Keine Prägung auf Freiform-Flächen als präzise Geometrie (nur Netz).
