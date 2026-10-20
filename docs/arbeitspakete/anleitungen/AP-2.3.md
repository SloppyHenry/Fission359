# AP-2.3 Aus Skizzen werden Körper

**Größe:** L, Teile a–e · **Phasendatei:** AP-2.3

## Ziel
Aus Skizzen-Bereichen entstehen Körper: **Ziehen** (Distanz, bis Fläche, symmetrisch, durch alles; Verjüngung; Neu/Vereinen/Ausschneiden/
Überlappung **automatisch vorgeschlagen**), **Drehkörper** (um Achse, Winkel), **Entlang Pfad**, **Übergang** zwischen Profilen. Ein Pfeil
direkt auf der Skizzenfläche zieht ohne Werkzeugwahl. Ändert man die Skizze, folgt der Körper; spätere Schritte behalten ihre Bezüge.

## Voraussetzungen
AP-2.2 (gelöste Skizzen mit Bereichen), AP-2.10 (Bezüge), AP-1.7 (Booleans für Vereinen/Ausschneiden).

## Vorher lesen
- `docs/02-ux-konzept.md` §2.2 (Skizzenfläche: Pfeil, Modus automatisch), §7.1 (Ausschneiden per Hineindrücken), §12 (Ziehen/Pull,
  Drehkörper/Spin, Entlang Pfad/Along Path, Übergang/Blend Between), §15 B Schritt 2
- `docs/adr/0003` Punkt 1 (Namen `F3/extrude/side[S2]`), ADR „Skizzen im Dokument“ (AP-2.1)
- AP-1.7-Anleitung (`combine`), `tests/geometry/test_sweeps.cpp` (AP-0.5, Wendel-Sweep)

## Ausgangslage im Code
Skizzen liefern Bereiche (Flächen in der Ebene) mit Ids aus ihren Kanten. Booleans gibt es (`GeometryKernel::boolean`).

## Haltepunkte
1. **Ein Schritt oder zwei bei „Ziehen + Ausschneiden“?** Empfehlung: **ein** Schritt `extrude` mit Wort `mode` (new | merge | subtract |
   intersect) und Ziel-Körper(n) als Eingänge; im Rezept „Ausgeschnitten 5 mm aus Quader 1“. Alternative: `extrude` (neuer Körper) +
   automatisch `combine` (zwei Einträge, unübersichtlicher).
2. **Automatischer Modus:** Empfehlung: Richtung in einen Körper hinein = Ausschneiden; aus einer Körperfläche heraus = Vereinen; auf leerer
   Ebene = Neu. Vorschlag sichtbar am Pfeil (Symbol), ein Klick ändert ihn.

## Teil 2.3a – Ziehen (Kern)
1. GAL: `NamedSolid extrude(const SketchProfile&, const ExtrudeSpec&)` mit `ExtrudeSpec { double distance; ExtrudeExtent extent (distance |
   symmetric | toFace | throughAll); double taperDegrees; std::optional<Solid> toFaceSolid…; }`. OCCT: `BRepPrimAPI_MakePrism`; Verjüngung
   über `BRepOffsetAPI_DraftAngle` oder `LocOpe_DPrism`/`BRepFeat_MakeDPrism` (testen, was gültige Körper liefert; Haltepunkt bei Problemen).
2. Flächennamen: Seiten nach der Skizzen-Kante (`side[L3]`), Deckel `capStart`, `capEnd`. Kein Name doppelt (Namens-Vertrag).
3. Golden: Rechteck 60×40, 30 mm → Volumen 72000, 6 Flächen, Namen; L-Profil; Kreis → Zylinder; Bereich mit Loch (Ring) → Rohr; Verjüngung 5°
   (Volumen nach Pyramidenstumpf-Formel); symmetrisch; durch alles.

## Teil 2.3b – Ziehen (Bedienung und Modi)
1. Feature-Typ `extrude` (Eingang: Skizzen-Schritt per Verweis auf den Bereich, Ziel-Körper bei Modus ≠ new). Werte `distance`, `taper`, Wörter
   `extent`, `mode`.
2. Pfeil-Griff direkt auf einem Skizzenbereich (Hover zeigt ihn, UX §2.2), Ziehen = Distanz (negative = in die andere Richtung), Tippen
   ersetzt Ziehen, Modus-Vorschlag nach Haltepunkt 2. „Bis Fläche“: während des Ziehens über eine Fläche fahren rastet ein.
3. Live-Vorschau beim Ziehen (Architektur §3.3: nur dieses Feature neu, auf Basis des gecachten Vorgängers). Budget < 50 ms für einfache Profile messen.
4. Tests: Undo; Modus automatisch richtig in drei Lagen (leere Ebene, Fläche nach außen, Fläche nach innen); Skizze ändern → Körper folgt,
   eine Rundung auf einer Seitenkante bleibt (Naming-Szenario, sobald AP-2.5 da ist; bis dahin `markFace`-Testfeature aus AP-2.10).

## Teil 2.3c – Drehkörper
`revolve`: Profil + Achse (Skizzenlinie, Körperkante, Hauptachse X/Y/Z), Winkel (Standard 360°). OCCT `BRepPrimAPI_MakeRevol`. Namen
`side[L3]` (Rotationsfläche je Profilkante), `capStart/capEnd` bei < 360°. Golden: Rechteck um Achse → Zylinder/Rohr (Formel); Halbkreis →
Kugel. Profil, das die Achse kreuzt → Fehler `revolveCrossesAxis` (Meldung mit Lösung „Profil auf eine Seite der Achse legen“).

## Teil 2.3d – Entlang Pfad
`sweep`: Profil (Bereich) + Pfad (Skizzenkette oder Kanten), Ausrichtung (Standard: Profil bleibt senkrecht zum Pfad – `BRepOffsetAPI_MakePipeShell`
mit Frenet bzw. fester Binormale, siehe AP-0.5 Gewinde). Golden: Kreis entlang Gerade = Zylinder; entlang Viertelkreis = Torus-Viertel (Formel).
Selbstüberschneidung → Fehler mit Erklärung.

## Teil 2.3e – Übergang
`loft`: zwei oder mehr Profile (verschiedene Skizzen/Ebenen), optional Führungskurven. `BRepOffsetAPI_ThruSections`. Golden: Quadrat → gleich großes
Quadrat versetzt = Prisma; Kreis → Kreis anderer Größe = Kegelstumpf (Formel). Ungleich viele Kanten: OCCT verkraftet das meist; testen, sonst
Fehler mit Erklärung.

## Für jeden Teil
Regel 10 (Hinweis, Tooltip, Synonyme: „hochziehen“, „extrudieren“, „pull“, „extrude“; „drehen“, „rotationskörper“, „revolve“, „spin“; …),
Rezeptsatz (AP-1.9 `RecipeSentences.js`), Meldungskatalog-Einträge (AP-2.14), Dateiformat (neue Typen: Roundtrip + Alt-Datei).

## Abnahme
- Akzeptanzkriterien: Golden-Tests aller Modi; Skizze ändern → Körper aktualisiert, abhängige Features behalten Bezüge (Naming-Tests).
- Ablauf B Schritt 2 als UI-Test.
- So probierst du es aus: Skizze mit L-Profil, Pfeil auf dem Profil ziehen, `30` tippen → Körper. Skizze öffnen, Stärke 5 → 8: Körper folgt.
  Rechteck auf der Oberseite skizzieren, Pfeil nach unten ziehen → schneidet aus.

## Nicht tun
Keine Verjüngung bei Drehkörpern; keine Twist-Extrusion (das ist Verformen, AP-4.3).
