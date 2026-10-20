# AP-3.8 Exportqualität und Vorschau

**Größe:** S · **Phasendatei:** AP-3.8

## Ziel
Die Export-Seite zeigt vorher, was herauskommt: Qualität (Grob / Normal / Fein / Benutzerdefiniert mit Sehnenfehler und Winkel), Anzahl Dreiecke und
ungefähre Dateigröße, eine Vorschau mit Netz-Overlay. Bei STL der Hinweis „immer mm“. Ein Zylinder Ø 5 hat in „Normal“ sichtbar runde Löcher im Druck.

## Voraussetzungen
AP-1.10 (Export STL/3MF).

## Vorher lesen
`docs/03-architektur.md` §6 (Export: 0,01 mm, 5°), `docs/02-ux-konzept.md` §7.3; `docs/06-rueckfragen.md` R16 und **R20** („Wofür ist die Datei?“:
3D-Druck, Spiel-Engine, Andere Programme, Web & AR, Bild); Mockup: Export-Assistent.

## Haltepunkte
1. **R20 jetzt?** Der Mockup-Export fragt „Wofür ist die Datei?“. glTF/GLB, FBX, USDZ brauchen laut R20 ein eigenes AP mit ADR (Bibliotheken,
   Lizenzen; kein proprietäres FBX-SDK). Empfehlung: Seite nach Mockup mit den Zwecken bauen; nur 3D-Druck (3MF, STL), Andere Programme (STEP, OBJ,
   ab AP-2.13) und Bild (PNG) aktiv; Spiel-Engine und Web & AR als „kommt später“ markiert. Eigenes AP für glTF/USDZ in der Phasendatei anlegen.

## Schritte
1. Export-Seite nach Mockup (Assistent: Zweck → Format → Qualität → Speichern), nicht modal (Seite/Panel).
2. Qualität: Voreinstellungen Grob (0,05 mm / 10°), Normal (0,01 / 5°), Fein (0,005 / 3°), Benutzerdefiniert (zwei Zahlenfelder).
3. Dreiecke/Größe: Tessellierung **im Hintergrund** mit den gewählten Werten, abbrechbar, Ergebnis gemerkt (gleiche Werte → kein Neurechnen); STL-Größe
   = 84 + 50 × Dreiecke Bytes; 3MF ungefähr (gezippt) schätzen.
4. Vorschau: die Ansicht zeigt das Export-Netz mit Netzlinien (Renderer: Drahtgitter-Darstellung gibt es, mit dem Export-Netz füttern).
5. Bild-Export (PNG): Ansicht ohne Oberfläche in hoher Auflösung (Offscreen-Rendering, wie `--smoke-test`), transparenter Hintergrund wählbar.

## Tests
Dreiecksanzahl steigt Grob < Normal < Fein; Ablauf D Schritt 6 (Abweichung ≤ 0,01 mm) läuft mit „Normal“; STL-Größe exakt nach Formel.

## Abnahme
Kriterium (gedruckter Zylinder) mit dem Product Owner; Foto in den PR.

## Nicht tun
Keine neuen Formate ohne AP/ADR (Haltepunkt 1).
