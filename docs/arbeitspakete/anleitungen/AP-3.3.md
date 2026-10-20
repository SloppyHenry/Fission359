# AP-3.3 Passungs-Assistent

**Größe:** M · **Phasendatei:** AP-3.3

## Ziel
Zwei Flächen oder Körper wählen, dann die Passungsart in Alltagssprache: **locker** (Schiebedeckel), **Steckpassung** (hält, lässt sich lösen),
**Presspassung**, **Drehpassung** (Achse). Das Spiel kommt aus dem Druckerprofil; im Rezept steht „Spiel 0,2 mm für Deckel“. Eine
**Kalibrier-Hilfe** erzeugt ein kleines Testteil mit Stufen (0,1 / 0,15 / 0,2 / 0,25 / 0,3 mm); den passenden Wert trägt man ins Profil ein.
Eine Box mit Deckel passt gedruckt auf zwei Referenzdruckern.

## Voraussetzungen
AP-2.4 (Fläche versetzen), AP-3.1 (Profilwerte).

## Vorher lesen
`docs/02-ux-konzept.md` §12 (Spiel/Play), `docs/03-architektur.md` §6 (Standard-Spiel 0,2 mm); AP-2.4-Anleitung Teil e (`offsetFaces`).

## Haltepunkte
1. **Werte je Passungsart** (Empfehlung, als Vielfaches des Profil-Spiels s, Standard s = 0,2 mm): locker = 2 s (0,4), Steck = s (0,2),
   Press = 0 bis −0,05, Dreh = 1,5 s (0,3). Bestätigen; im Profil überschreibbar.
2. **Welches Teil wird verändert?** Empfehlung: das **aufnehmende** Teil (Loch, Nut, Deckel innen) wird erweitert, das eingesteckte bleibt
   maßhaltig (so hat es der Product Owner auch in Blender gehalten: Spiel immer im aufnehmenden Teil). Der Assistent erkennt „aufnehmend“
   (Fläche zeigt nach innen / konkav) und fragt nur, wenn unklar.

## Schritte
1. Feature-Typ `fit`: Eingänge zwei Körper, `refs["faces"]` (Kontaktflächen des aufnehmenden Teils), Wort `kind`, Wert `clearance` (Ausdruck,
   Standard aus Profil × Faktor). Wirkung: `offsetFaces` (AP-2.4) der gewählten Flächen um die Hälfte bzw. das volle Spiel (je nach Geometrie:
   radial bei Zylindern = Spiel/2 je Seite? → **Festlegen**: „Spiel“ bedeutet Luft **je Seite**; im Panel so erklärt mit Skizze).
2. Bedienung: Werkzeug „Passung“ (Gruppe Ändern oder Prüfen; nach Mockup), Klick auf zwei Flächen/Körper, Karten mit Bildern der vier Arten, Wert
   änderbar. Regel 10 („passung“, „spiel“, „toleranz“, „deckel passt“, „fit“, „clearance“, „tolerance“).
3. Prüfung: nach dem Anwenden misst das Programm den kleinsten Abstand der Kontaktflächen (AP-2.11) und zeigt ihn („Luft 0,20 mm“).
4. **Kalibrier-Hilfe:** Menü „Passung testen…“ erzeugt zwei Körper: Platte mit 5 Löchern Ø 10 + Stufen-Spiel, 5 Stifte Ø 10 (beschriftet „0.1“ …
   „0.3“ durch Prägen, falls AP-4.4 fertig; sonst kleine Kerben 1–5). Text-Anleitung: „Drucke beide, steck die Stifte, trage den Wert der ersten
   Steckpassung, die hält, unter Einstellungen ▸ Drucker ▸ Spiel ein.“
5. Box-mit-Deckel: Beispiel-Datei `resources/examples/box-mit-deckel.fission` (Box 60×40×30, Wand 2, Steckdeckel mit Rand) mit `fit` gebaut.

## Tests
Golden: Stift Ø 10 in Loch Ø 10, Steck → Loch Ø 10,4 (bei Spiel je Seite 0,2); Deckel-Rand 0,2 Luft rundum (Abstandsmessung); Undo.

## Abnahme
Kriterium (gedruckt auf 2 Druckern) mit dem Product Owner, Fotos im PR.

## Nicht tun
Kein automatisches Erkennen aller Passungen im Modell (nur gewählte Flächen).
