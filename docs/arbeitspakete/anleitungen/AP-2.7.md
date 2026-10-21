# AP-2.7 Bohrungen, Gewinde und Schraubverbindungen

**Größe:** L, Teile a–d · **Phasendatei:** AP-2.7

## Ziel
Ein **Bohrungs-Assistent** in Alltagssprache: „Für Schraube M3 … M10“, „Holzschraube 3–6 mm“, Durchgangsloch, Senkkopf, Zylinderkopf (Senkung),
Kernloch für Gewinde, **Einschmelzmutter**, **Mutternfalle** (Sechskant-Tasche, auch seitlich einschiebbar). Maße kommen aus Normtabellen.
**Echte Gewinde** (metrisch, Bolzen und Mutter) als Geometrie mit Druckspiel, oder „kosmetisch“ für die Anzeige. Lochkorrektur für den Druck aus
dem Druckerprofil, erklärt. Platzieren per Klick auf eine Fläche, Abstandsmaße zu Kanten, mehrere Löcher in einem Schritt.

## Voraussetzungen
AP-2.3, AP-2.10 (Fläche als Verweis), AP-1.6d (Abstandsmaße, hier auf Flächen übertragen), für die Lochkorrektur AP-3.1 (sonst fester Standard 0,2 mm
mit Hinweis, und Nachzug in 3.1).

## Vorher lesen
- `docs/02-ux-konzept.md` §2.2 (Bohrung: Durchmesser- und Tiefen-Griff), §15 B Schritt 4 („Senkkopfschraube 4 mm (Holz)“)
- `docs/03-architektur.md` §6 (Druck-Toleranzen), §9 (`resources/`)
- `tests/geometry/test_sweeps.cpp` (Gewinde per Wendel-Sweep: gültig, aber 0,5–2 s → Hintergrund; für die Anzeige vereinfacht)

## Haltepunkte
1. **Normtabellen-Umfang:** Empfehlung: ISO 273 (Durchgangslöcher fein/mittel/grob), ISO 10642 (Senkkopf Innensechskant), DIN 7991 (alt, gleich),
   ISO 4762 (Zylinderkopf, für Senkungen), ISO 4032 (Muttern, Schlüsselweite/Höhe), Kernlochtabelle M2–M12, Holzschrauben (Kopf 90°, Ø 3–6),
   Einschmelzmuttern **gängige Händlermaße** (M2, M2.5, M3, M4, M5 je kurz/lang; Quelle im Kommentar). Werte selbst abtippen und prüfen; Quellen
   nennen. Bestätigen, dass Normwerte (Fakten, keine geschützten Texte) so übernommen werden dürfen.
2. **Gewinde-Spiel:** Empfehlung: Standard 0,2 mm radial für FDM (aus Druckerprofil, AP-3.1), einstellbar.

## Teil 2.7a – Datentabellen
`resources/holes/*.json` (Schema mit Validierung, Test, der jede Datei gegen das Schema prüft): je Eintrag Name (Schlüssel für die Übersetzung),
Durchmesser, Senkung (Winkel, Ø), Kopfhöhe, Kernloch, Steigung, Quelle. Werte als Zahlen in mm. Zugriff über `src/features` (Laden beim Start,
an die Feature-Typen gereicht – kein globaler Zustand).

## Teil 2.7b – Bohrung als Feature
1. Feature-Typ `hole`: Eingang Körper, `refs["face"]` (ebene Fläche), Punkte auf der Fläche (Liste von (u,v) in Flächen-Koordinaten, als Werte
   `p1u`, `p1v`, … oder eigene Struktur – Muster mit AP-2.1-ADR abstimmen), Wörter `kind` (through | counterbore | countersink | tapDrill | insert |
   nutTrap), `standard` (Tabellen-Schlüssel), Werte `depth` (oder „durch“), `correction`.
2. Geometrie: Profil je Art (Rotationsprofil: Zylinder + Senkung) per `revolve` und `subtract`; Mutternfalle: Sechskant-Prisma, seitlich: Prisma
   bis zur Kante verlängert. Namen `F<n>/hole/wall[i]`, `countersink[i]`, `bottom[i]`.
3. Lochkorrektur: Durchmesser + Korrektur aus Profil; Erklärung im Panel („+0,2 mm, damit die Schraube nach dem Druck passt – Wert aus deinem
   Druckerprofil“).
4. Golden: gegen die eigene, geprüfte Normtabelle (Akzeptanzkriterium 2), z. B. Durchgangsloch M3 „mittel“ (ISO 273: Ø 3,4) und Senkung für
   ISO 10642 M4 (90°). Die Zahlen in dieser Anleitung sind nur Beispiele; maßgeblich ist die Tabelle mit Quellenangabe. Volumen nach Formel.

## Teil 2.7c – Assistent und Bedienung
1. Werkzeug „Bohrung“: Klick auf ebene Fläche setzt ein Loch (Vorschau folgt dem Zeiger, Fangen an Flächenmitte/Kantenmitten), weitere Klicks
   weitere Löcher im **selben** Schritt, Enter beendet. Abstandsmaße zu den Kanten der Fläche (Logik aus AP-1.6d auf 2D übertragen) – Klick und Tippen.
2. Panel/Assistent als Liste mit Alltagssprache: „Wofür ist das Loch?“ → Schraube (M2–M10) | Holzschraube | Einschmelzmutter | Mutter einlegen |
   Nur ein Loch (Ø frei); dann Kopfform (Senkkopf, Zylinderkopf, durch). Bilder je Option (kleine Zeichnung im Stil des Mockups).
3. Griffe: Durchmesser (Ring), Tiefe (Pfeil) am Loch (UX §2.2); bei Normlöchern ändert der Griff die Norm-Größe stufenweise (M3 → M4).
4. Regel 10: Synonyme „bohren“, „schraube“, „loch“, „senkkopf“, „mutter“, „gewindeeinsatz“, „insert“, „screw“, „drill“, „countersink“.
5. Ablauf B Schritt 4 als UI-Test.

## Teil 2.7d – Gewinde
1. Feature-Typ `thread`: Eingang Körper, `refs["face"]` (Zylinderfläche: außen = Bolzen, innen = Mutter), Wörter `standard` (M3…M12, Steigung aus
   Tabelle), `mode` (real | cosmetic), Werte `length`, `clearance`.
2. Echt: Wendel-Sweep des ISO-Profils (60°), Spiel radial abziehen/zugeben; **Hintergrund** (0,5–2 s), Anzeige bis dahin kosmetisch (Textur/
   Linienmuster). Kosmetisch: Zylinder bleibt, Darstellung zeigt Gewinde, Export ohne Gewinde (Hinweis beim Export).
3. Golden: M8 Bolzen 20 mm → Volumen in einem engen, berechneten Bereich (Kernvolumen < V < Außenvolumen), gültig, eine Wendelfläche.
4. **Druckprüfung (Akzeptanzkriterium 1):** Testteile als `.fission` + 3MF in `tests/fixtures/print/` (M3-Durchgang, M3-Einschmelzmutter, M8 Bolzen
   + Mutter). Drucken macht der Product Owner auf P1S und MK4; Fotos in den PR. Bis dahin Kriterium offen lassen.

## Abnahme
So probierst du es aus: Platte, Bohrung ▸ „Schraube M4 ▸ Senkkopf“, zwei Klicks auf die Fläche → zwei Senklöcher; Abstand zum Rand anklicken,
`10` tippen. Zylinder, Gewinde M8 → nach kurzer Zeit echtes Gewinde.

## Nicht tun
Keine zölligen Gewinde (später). Keine Normtexte kopieren, nur Zahlen.
