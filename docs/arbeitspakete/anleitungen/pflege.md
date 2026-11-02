# Pflege: Fehler, Politur und Folgearbeiten

Kleine Aufgaben außerhalb der APs. Jede ist für sich ein PR (Branch `pflege-<nummer>-<stichwort>`). Allgemeine Regeln: [README](README.md), [Handbuch](handbuch.md).
Stand: 2026-11-02, gegen den Code auf `main` geprüft.

## Reihenfolge
**Vor dem Nutzertest 1 (AP-1.13):** P18 (trifft genau die Testaufgabe), P2, P3. **Danach nach Bedarf**, der Nutzertest kann die Reihenfolge ändern.
**Braucht eine Entscheidung oder ein Testsystem:** P1 (Bild oder Video), P5, P8, P10, P11, P14 (Entscheidung oder Rechner).

---

## P1 Kugel-Radius-Griff fühlt sich unsauber an
**Größe:** S ·

## P2 Formen-Galerie schließt bei Klick aufs Bett
**Größe:** S ·
Nach dem Hinzufügen einer Form soll ein Klick auf leeren Grund (Bett, Hintergrund) die geöffnete Galerie (`ShapeFlyout.qml`) schließen.
1. In der Ansicht gibt es einen Klick auf leeren Grund schon (hebt die Auswahl auf, `SelectionController`). Dort zusätzlich ein Signal
   `emptyClicked()` ausgeben; `Main.qml` schließt darauf die Galerie.
2. Nicht schließen, wenn gerade eine Form abgelegt wird oder der Klick auf einen Körper geht.
3. Fenster-Test: Galerie öffnen (`--simulate`-Aktion für das Öffnen der Galerie prüfen/ergänzen), `click:` auf leeres Bett, Prüfung, dass
   die Galerie zu ist (eigene Prüf-Aktion `expectclosed:shapes` oder Eigenschaft abfragen).

## P3 Politurdurchgang: Abstände und Beschriftungen
**Größe:** S–M ·
eingehalten“); die Werkzeugpalette ist mit PR #30 behoben.
1. Alle QML-Bausteine mit Text (Btn, Segmented, Toggle, NumberField, WordField, FontField, Section, PanelHeader, StatusBar, SnapControl,
   MenuStrip, DocTabs) gegen `docs/08-gestaltung.md` (Abstände, Schriftgrößen) prüfen. Bildschirmfotos vorher/nachher (Hell, Dunkel,
   Deutsch – deutsche Wörter sind länger).
2. Feste Abstände durch `Theme`-Werte ersetzen. Kein Text darf den Rand berühren; lange deutsche Wörter: Elide oder Umbruch nach Mockup.
3. Fenster-Vergleichstest `app_window_*_matches_design` muss grün bleiben; Abweichungen vom Mockup in `08-gestaltung.md` vermerken.

## P4 AP-1.3-Reste: Linux-Messung, Optik auf Linux, Bettmaße
**Größe:** S · Offen aus AP-1.3.
1. `scripts/run --release -- --benchmark 2000000` auf einem Linux-Rechner mit Intel/AMD-iGPU (dort ggf. nur unter Xvfb + Vulkan messbar, siehe `docs/testumgebung.md`). Ergebnis (gpu_ms_avg) in AP-1.3 eintragen; < 16,7 ms → Kriterium 1 abhaken.
2. Weiche Schatten (SSAO) unter Vulkan/OpenGL ansehen: Bild mit und ohne (`--no-soft-shadows`), in den PR.
3. Druckerliste (`src/ui/src/printer_catalog.cpp`): Bettmaße aller Einträge gegen die Herstellerdatenblätter prüfen, Quelle je Drucker als Kommentar.

## P5 AP-0.1: frischer Mac
**Größe:** S · Offen aus AP-0.1 (Kriterium 4). Ein virtueller Mac (siehe `docs/testumgebung.md`) nur mit den Kommandozeilen-Werkzeugen, ohne Homebrew und Xcode. Dort `git clone`, `scripts/setup`, `scripts/run` durchspielen; alles, was fehlt oder
unklar ist, in `scripts/setup` verbessern. Ergebnis in AP-0.1 eintragen.

## P6 Maß-Punkte und Bänder an gedrehten Körpern
**Größe:** M · Folgearbeit seit AP-1.6b1.
Heute werden Maßbänder gedrehter Körper nicht gezeichnet und die Beschriftungen stehen lose daneben. Ziel: Bänder entlang der gedrehten
Achsen des Körpers, außerhalb seines (gedrehten) Kastens. `FeatureType::placedHandles` liefert gedrehte Griffe; `HandleController::refresh`
baut die Bänder heute nur für achsparallele Richtungen. Bilder vorher/nachher; Test: gedrehter Quader (rotZ 30) hat drei Bänder, keines
schneidet den Körper.

## P7 Exakte Fangpunkte aus dem Kernel
**Größe:** M · Folgearbeit seit AP-1.6c1.
Fangpunkte kommen heute aus dem Anzeige-Netz (Rauschen ~1e-5 mm, gebogene Kantenmitten genähert). Neue GAL-Funktion
`std::vector<SnapPointGal> snapPoints(const Solid&)` (Ecken, Kantenmitten, Kreismitten, Mitten ebener Flächen, exakt aus OCCT), im
Hintergrund je Ergebnis berechnet, in `SnapSession` statt der Netzpunkte. Test: Zylinder Ø 6,3 → Kreismitte exakt, Kantenmitte eines Bogens
exakt auf dem Kreis.

## P8 Lage beim Tippen von Maßen nach dem Ablegen an einer Wand mitführen
**Größe:** S · Idee aus AP-1.6b3. **Haltepunkt:** erst fragen, ob gewünscht.
Problem: Nach dem Ablegen an einer Wand und `50 ⇥ 20 ⇥ 4` ändert sich die Höhe um die Körpermitte; die Form rutscht von der Wand.
Lösung: Solange der Schritt „frisch abgelegt“ ist (bis zum ersten anderen Befehl), Lage so nachführen, dass die Unterseite auf der Wand bleibt
(`geo::seatOnFace` mit der gemerkten Fläche).

## P9 Rezept umsortieren per Ziehen
**Größe:** M · UX §4 erlaubt es, AP-1.9 hat es ausgelassen. Ziehen einer Zeile; erlaubt nur, wenn keine Abhängigkeit verletzt wird
(Eingänge müssen vorher stehen); sonst Erklärung inline. Neuer Command `makeMoveFeature(id, newIndex)`, Test mit Undo und mit verbotener
Reihenfolge.

## P10 Tastatur-Zoom
**Größe:** S · Angeboten am 2026-10-16, keine Antwort. **Haltepunkt:** erst fragen. Vorschlag: `+`/`-` und ⌘+/⌘− zoomen zur Mitte der Ansicht
(`Viewport3D::zoomBy`); Achtung: ⌘+/⌘− wären sonst UI-Skalierung (AP-1.11) – Belegung mit dem Maintainer klären.

## P11 ⌥ beim Ablegen unter Linux prüfen
**Größe:** S · Offen aus AP-1.6b3. Manche Linux-Fenstermanager (KDE, GNOME) nehmen ⌥(Alt)+Maus für sich (Fenster verschieben). Auf einem
Linux-Desktop prüfen; wenn es nicht geht: Haltepunkt mit Vorschlag einer Ersatztaste
(z. B. Strg) und Hinweis in der Hinweiszeile.

## P14 Teile eines geteilten Körpers drehen und vergrößern
**Größe:** S · Offen aus AP-1.7e. Ein Teil hat nur den Schritt „Teil platziert“ (nur x, y, z): Drehen und Größe ändern sind aus
(`DocumentModel::beginTurn/beginScale` geben false). Vorschlag: den Schritt `place` um `rotX/rotY/rotZ` ergänzen (Drehung des Teils um seine Mitte,
wie bei den Formen) und für das Vergrößern den Eck-Griff auf den Schritt davor zurückführen (Teilen und Quelle gemeinsam vergrößern, `scaleStartOf`
geht dafür die Kette zurück). **Haltepunkt:** erst mit dem Maintainer klären, ob Vergrößern eines Teils das ganze Geteilte vergrößern soll.

## P16 Griff-Art „Klassisch“ vervollständigen
**Größe:** M · Offen aus AP-1.11. Es fehlen Eck-Griffe, die Breite und Tiefe zugleich ändern, und der schwarze Hebe-Pfeil (Lage z). Regeln 11–13 gelten; Ablauf D muss auch mit „Klassisch“ grün sein (R19).

## P17 Kamera je Tab merken
**Größe:** S · Offen aus AP-1.11. Beim Tab-Wechsel bleibt heute die Kamera des vorigen Projekts. Test: zwei Tabs, Ansicht ändern, wechseln, Ansicht stimmt.

## P18 Abstandsmaß auf einer Platte misst bis zum Bettrand
**Größe:** S–M · Gefunden in Pflege T8 (2026-10-28). Ablauf A Schritt 5 verlangt „5 mm vom linken Rand der Platte“, das Abstandsmaß bezieht sich aber auf den Bettrand. Der Test `app_flow_a_keychain` umgeht das und setzt die Lage direkt (`value:x`). **Haltepunkt:** kurz zeigen, wie der Bezug gewählt wird (nächster Körper zuerst oder umschaltbar). Danach misst der Test über das Abstandsmaß.

## P19 Kontextleiste und Hinweise: Reste
**Größe:** S–M · Aus AP-1.8 und AP-1.12. Tooltip-Karten für Menübefehle der Kontextleiste; „Wert ändern“ wählt den Schritt, soll auch das Feld fokussieren; „Mehr ▾“ und Ausrichten bei mehreren Körpern; die Leiste kann Maßzahlen der Ansicht kurz verdecken.

## P20 Kleinigkeiten aus AP-1.10 und AP-1.11
**Größe:** S · „Zuletzt geöffnet“ als Untermenü; Einheit „Zoll“ auch in der Statuszeile und in bestehenden Projekten; Dateizuordnung unter macOS und Linux von Hand prüfen; Bildschirmfotos 80/100/150/200 % in den PR des nächsten Politurdurchgangs.

## P21 Wacklige Fenster-Tests beobachten
**Größe:** S · Ursache von `app_ring_turn` und `app_snap_corner_handle` wohl behoben (2026-10-28, `pressAndPull`). Fällt einer in der CI erneut aus: Meldung dort ansehen, nicht abschwächen. Prüfungen von `--simulate` warten auf das Ende der Aktion, nie auf feste Zeiten (PR #51).

---

## Erledigt
- **P12** `main.cpp` und `Main.qml` verkleinert (2026-10-28): `simulate.cpp`, `RecipeSentences.qml`, `FileNotices.qml`.
- **P13** Löcher im Hintergrund prüfen: gemessen, ein Zug-Schritt ≈ 31 ms bei 10 Körpern und 20 Löchern, unter der 50-ms-Grenze; kein Umbau. Wieder ansehen bei Loch-Mustern (AP-2.8) oder großen Körpern.
- **P15** Rezept-Sätze ausgelagert (Rest nach P19 und P21).
- **Prüfliste T1–T11 vom 2026-10-27:** alles gemergt (PR #68–#80).
