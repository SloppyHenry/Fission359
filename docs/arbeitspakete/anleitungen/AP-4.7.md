# AP-4.7 Muster und Oberflächenstrukturen

**Größe:** L · **Phasendatei:** AP-4.7

## Ziel
**Durchbruch-Muster** in Flächen (Voronoi, Waben, Kreise, Rauten) mit Randabstand und Stegbreite (≥ Mindestwand). **Oberflächenstrukturen**
(Rändel für Griffe, Holz, Leder, Hammerschlag, Wellen) mit Tiefe und Maßstab. Gitterfüllung (Lattice) für leichte Teile unter ▸ Mehr. Voronoi-Deckel
und Rändel-Drehknopf sind druckbar (Druckcheck grün).

## Voraussetzungen
AP-4.1, AP-2.8, AP-3.2 (Prüfung), AP-3.1 (Mindestwand).

## Vorher lesen
AP-2.8 (Muster), AP-4.6 (Höhenfeld-Technik), `docs/02-ux-konzept.md` §3 (Deko).

## Haltepunkte
1. **Durchbrüche präzise oder als Netz?** Empfehlung: Auf **ebenen** Flächen präzise (2D-Muster als Skizzen-Profile, Ausschneiden; Rändel auf Zylinder
   präzise als Muster von Nuten); alles Gekrümmte und alle Displacement-Strukturen als Netz (Ergebnis frei).

## Schritte
1. 2D-Muster-Generatoren (rein, `src/geometry/gal/pattern2d.*` oder `src/features`): Voronoi (Punkte mit Seed + Lloyd-Relaxation, Zellen schrumpfen um
   Stegbreite/2), Waben, Kreise, Rauten; beschnitten auf die Fläche minus Randabstand. Ergebnis: Profile (Polygone mit gerundeten Ecken).
2. Feature-Typ `perforate` (Eingang Körper + Flächen-Verweis, Wort `pattern`, Werte Zellgröße, Steg, Rand, Seed): Profile durch die Wand ausschneiden
   (präzise, AP-1.7). Steg < Mindestwand → Fehler mit Lösung „Steg auf 0,8 mm setzen“.
3. Feature-Typ `texture` (Displacement): Rändel (diamant/gerade, präzise auf Zylinder als Nuten-Muster), Holz/Leder/Hammerschlag/Wellen als prozedurale
   Höhenfunktion → Netz verfeinern, Punkte entlang Normale verschieben (AP-4.3 Verfeinerung). Ergebnis frei.
4. Lattice (▸ Mehr): Innenraum mit Gitter (kubisch/Gyroid) füllen – Gyroid als implizite Funktion im Feld (AP-4.2) innerhalb einer Schale.
5. Bedienung: Deko ▸ Muster/Struktur, Fläche anklicken, Galerie mit Vorschau-Kacheln, Regler, Würfel-Knopf für neuen Seed. Regel 10.

## Tests
Voronoi: alle Stege ≥ Vorgabe (Messung minimaler Abstand zwischen Zellen), deterministisch bei gleichem Seed; Rändel-Knopf Volumen plausibel, gültig;
Druckcheck grün für beide Akzeptanz-Teile (automatisch).

## Abnahme
So probierst du es aus: Box-Deckel ▸ Muster ▸ Voronoi, Steg 1,2 → Durchbrüche; Würfeln → neues Muster. Zylinder ▸ Rändel → griffiger Knopf.

## Nicht tun
Keine Strukturen aus Bildern (das ist Relief, AP-4.6).
