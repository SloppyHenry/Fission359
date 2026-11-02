# AP-4.8 Generatoren („Zauberformen“)

**Größe:** L · **Phasendatei:** AP-4.8

## Ziel
Ein Generator ist ein parametrisches Feature mit Konfigurator-Panel und Live-Vorschau-Galerie, das in Sekunden etwas Schönes und Druckbares macht:
**Stern**, **Schneeflocke** (zufällig mit Seed, symmetrisch), **Christbaumkugel** (mit Aufhänger, in zwei Hälften teilbar), **Vase** (Profilkurve +
Drehung + Muster, vasenmodus-tauglich), **Namensschild**, **Zahnrad** (Evolvente, Modul, Zähnezahl – funktional!), **Kette/Glieder**, **Box mit Deckel**.
Ein Würfel-Knopf „Überrasch mich“. Jeder Generator ist über den ganzen Parameterbereich druckbar (Property-Tests).

## Voraussetzungen
AP-2.9 (Konfigurator, Maße mit min/max), AP-4.1 (für freie Ergebnisse), AP-4.3 (Vase: Verdrehen), AP-3.3 (Box mit Deckel: Spiel).

## Vorher lesen
`docs/02-ux-konzept.md` §15 C Schritt 1 (Stern-Generator: Zacken 8, Außen-Ø 70, Innen-Ø 35, Höhe 6, „Spitzen zulaufend“); `docs/07-baukaesten.md` (Baukästen
nutzen Generatoren); `src/features/src/shapes.cpp` (Stern als Grundform gibt es: Generator erweitert ihn).

## Haltepunkte
1. **Generator = ein Feature-Typ je Generator** (Empfehlung), mit Grenzen je Wert (min/max) im Typ; Galerie zeigt 6 Varianten (Seed/Werte variiert).
   Bestätigen.
2. **Zahnrad-Norm:** Modul-Reihe 0,5–3, Eingriffswinkel 20°, Spiel aus Profil; Profilverschiebung nein (einfach halten). Bestätigen.

## Schritte (je Generator ein Teil-PR, Reihenfolge: Stern, Zahnrad, Box, Vase, Namensschild, Kugel, Schneeflocke, Kette)
1. Gemeinsames Gerüst: `GeneratorType : FeatureType` mit `parameters()` inkl. Grenzen, `randomize(seed)` (für Würfeln), `galleryVariants()`.
   Panel: Regler je Wert (aus den Grenzen), Galerie-Kacheln (Offscreen-Vorschau im Hintergrund, Platzhalter zuerst).
2. Je Generator Geometrie:
   - Stern: Zacken, Außen-/Innen-Ø, Höhe, „Spitzen zulaufend“ (Pyramiden-Zacken = Loft zur Mitte), Rundung der Spitzen.
   - Zahnrad: Evolventenprofil (Formeln mit Quelle im Kommentar), Bohrung, Nabe; Golden: Teilkreis = Modul × Zähne.
   - Box mit Deckel: Außenmaße, Wand, Ecken, Deckelart (Steck/Schiebe) mit `fit` (AP-3.3).
   - Vase: Profilkurve aus 3–5 Stützpunkten (Spline), Querschnitt (Kreis/Polygon/Stern), Verdrehen; Prüfung Vasenmodus (eine Wand, keine Inseln, keine
     waagerechten Dächer).
   - Namensschild: Platte + Text (AP-4.4) + Loch/Öse, Text passt sich ein.
   - Christbaumkugel: Hohlkugel, Aufhänger, Teilung in Hälften mit Steckrand.
   - Schneeflocke: 6-fach symmetrisch, Äste nach Regeln mit Seed (L-System-artig), Stegbreite ≥ Mindestwand.
   - Kette: n Glieder ineinander (Print-in-Place, Abstand ≥ Spiel).
3. Property-Tests: je Generator 100 zufällige Werte im erlaubten Bereich → gültig, wasserdicht, Druckcheck ohne rote Funde (Akzeptanzkriterium).
4. Regel 10 je Generator; Rezeptsatz; Meldungen.

## Abnahme
Ablauf C Schritt 1 als UI-Test. So probierst du es aus: Deko ▸ Generatoren ▸ Stern, Zacken 8 → Galerie zeigt Varianten; Würfeln. Zahnrad 20 Zähne Modul 1.

## Nicht tun
Keine Generatoren, die nur mit neuer Bibliothek gehen. Keine starren Vorlagen (alles bleibt Werte im Rezept).
