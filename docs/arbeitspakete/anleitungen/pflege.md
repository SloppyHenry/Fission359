# Pflege: Fehler, Politur und Folgearbeiten

Kleine Aufgaben außerhalb der APs. Jede ist für sich ein PR (Branch `pflege-<nummer>-<stichwort>`). Allgemeine Regeln: [README](README.md), [Handbuch](handbuch.md).
Stand: 2026-11-06 (P17 erledigt mit PR #99, P22 umgesetzt), gegen den Code auf `main` geprüft.

## Reihenfolge
**Nach Bedarf**, der Nutzertest 1 (AP-1.13) kann die Reihenfolge ändern. P18, P2 und P3 sind vor dem Nutzertest erledigt worden.
**Braucht eine Entscheidung (trifft die Orchestrierung) oder ein Testsystem:** P1 (Bild oder Video), P5, P11, P14 (Entscheidung oder Rechner).

---

## P1 Kugel-Radius-Griff fühlt sich unsauber an
**Größe:** S ·

## P4 AP-1.3-Reste: Linux-Messung, Optik auf Linux, Bettmaße
**Größe:** S · Offen aus AP-1.3.
1. `scripts/run --release -- --benchmark 2000000` auf einem Linux-Rechner mit Intel/AMD-iGPU (dort ggf. nur unter Xvfb + Vulkan messbar, siehe `docs/testumgebung.md`). Ergebnis (gpu_ms_avg) in AP-1.3 eintragen; < 16,7 ms → Kriterium 1 abhaken.
2. Weiche Schatten (SSAO) unter Vulkan/OpenGL ansehen: Bild mit und ohne (`--no-soft-shadows`), in den PR.
3. Druckerliste (`src/ui/src/printer_catalog.cpp`): Bettmaße aller Einträge gegen die Herstellerdatenblätter prüfen, Quelle je Drucker als Kommentar.

## P5 AP-0.1: frischer Mac
**Größe:** S · Offen aus AP-0.1 (Kriterium 4). Ein virtueller Mac (siehe `docs/testumgebung.md`) nur mit den Kommandozeilen-Werkzeugen, ohne Homebrew und Xcode. Dort `git clone`, `scripts/setup`, `scripts/run` durchspielen; alles, was fehlt oder
unklar ist, in `scripts/setup` verbessern. Ergebnis in AP-0.1 eintragen.

**Stand 2026-11-03:** Durchgespielt bis Homebrew. Ohne Homebrew bricht `scripts/setup` mit klarer Anleitung ab (gewollt); ohne Terminal fragt es nicht und bricht ab, dann `scripts/setup --yes`. Der virtuelle Mac hat macOS 14: Homebrew liefert dafür keine fertigen Pakete mehr und baut alles aus dem Quelltext (auch Qt, viele Stunden). Für den Nachweis einen virtuellen Mac mit aktuellem macOS anlegen. Prüfen, ob `scripts/setup` bei zu altem macOS davor warnen soll.

## P6 Maß-Punkte und Bänder an gedrehten Körpern
**Größe:** M · Folgearbeit seit AP-1.6b1.
Heute werden Maßbänder gedrehter Körper nicht gezeichnet und die Beschriftungen stehen lose daneben. Ziel: Bänder entlang der gedrehten
Achsen des Körpers, außerhalb seines (gedrehten) Kastens. `FeatureType::placedHandles` liefert gedrehte Griffe; `HandleController::refresh`
baut die Bänder heute nur für achsparallele Richtungen. Bilder vorher/nachher; Test: gedrehter Quader (rotZ 30) hat drei Bänder, keines
schneidet den Körper.

## P9 Rezept umsortieren per Ziehen
**Größe:** M · UX §4 erlaubt es, AP-1.9 hat es ausgelassen. Ziehen einer Zeile; erlaubt nur, wenn keine Abhängigkeit verletzt wird
(Eingänge müssen vorher stehen); sonst Erklärung inline. Neuer Command `makeMoveFeature(id, newIndex)`, Test mit Undo und mit verbotener
Reihenfolge.

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

## P19 Kontextleiste und Hinweise: Reste
**Größe:** S–M · Aus AP-1.8 und AP-1.12. Tooltip-Karten für Menübefehle der Kontextleiste; „Wert ändern“ wählt den Schritt, soll auch das Feld fokussieren; „Mehr ▾“ und Ausrichten bei mehreren Körpern; die Leiste kann Maßzahlen der Ansicht kurz verdecken.

## P20 Kleinigkeiten aus AP-1.10 und AP-1.11
**Größe:** S · „Zuletzt geöffnet“ als Untermenü; Einheit „Zoll“ → P23 (Auswahl bis dahin ausgeblendet); Dateizuordnung unter macOS und Linux von Hand prüfen; Bildschirmfotos 80/100/150/200 % in den PR des nächsten Politurdurchgangs.

## P23 Anzeige in Zoll
**Größe:** L · Aus P20. Die Einstellung „Einheit für neue Projekte: Zoll“ ließ in neuen Projekten bloße Zahlen als Zoll zählen, angezeigt wurde
aber alles in mm. Sie ist bis zu dieser Aufgabe ausgeblendet, neue Projekte zählen in mm (Zoll lassen sich weiter eintippen, „2 in“). Nötig: Einheit je Dokument (Dateiformat-Feld mit Migration), Anzeige
aller Längen (Zahlenfeld, Maßbänder, Rezept-Sätze, Statuszeile, Bett-Beschriftung) in der Einheit des Dokuments, intern weiter mm.

## P21 Wacklige Fenster-Tests beobachten
**Größe:** S · Ursache von `app_ring_turn` und `app_snap_corner_handle` wohl behoben (2026-10-28, `pressAndPull`). Fällt einer in der CI erneut aus: Meldung dort ansehen, nicht abschwächen. Prüfungen von `--simulate` warten auf das Ende der Aktion, nie auf feste Zeiten (PR #51).
2026-11-08: Gefunden, dass eine simulierte Taste (`key:`) hinter einer schon fälligen Prüfung landen konnte, wenn das Programm kurz beschäftigt war (die Taste ging über einen weiteren Null-Timer); behoben, die Taste geht bei aktivem Fenster sofort. Neu beobachtet: `app_number_field_arrows` unter Linux debug-asan einmal 30,9 statt 30,8.


## P22 Fenster-Tests auch auf dem Mac in der CI
**Größe:** S · **Was:** Die Fenster-Tests (`app_*`) laufen in der CI auf macOS gegen das fertig paketierte Programm, mit dem Fenstersystem des Runners (nicht `offscreen`, das gibt auf dem Mac falsche grüne Ergebnisse). **Warum:** Im Build-Baum findet das Programm mit dem Qt aus vcpkg die QML-Plugins nicht; deshalb waren die Tests auf dem Mac abgeschaltet und ein Fehler nur der Mac-Oberfläche fiel nicht auf. **Wie:** Die Cache-Variable `FISSION_APP_UNDER_TEST` (Pfad der ausführbaren Datei im `.app`) meldet die `app_*`-Tests an und startet diese Datei statt des Build-Ziels. Nur die CI setzt sie: Der Schritt „Window tests against the packaged app (macOS)“ im Job „Package“ nimmt `build/package/stage/Fission359.app` und lässt `ctest -R '^app_'` laufen; der Job „Window tests (macOS arm64, packaged app)“ zeigt das Ergebnis als eigenen Check. Ohne die Variable bleibt lokal alles wie zuvor.
**Stand:** In main seit 2026-11-06 (PR #103). Der Runner-Bildschirm wird vor den Tests auf 1920×1080 gestellt (displayplacer); 53 von 56 grün. Echter Fehler gefunden und behoben (Zahlenblase nach dem Loslassen eines Griffs). Simulierte Griff-Züge warten auf das Ergebnis und ein gezeichnetes Bild.
**Rest:** Auf dem Mac wackeln Griff-Züge, die gar nichts bewirken: `app_sphere_radius_undo` (3 von 4 Läufen rot), `app_drop_to_bed` (3 von 4), vereinzelt `app_corner_scale_round_turned`, `app_handle_drag_front`. Ursache klären (Fenster-Aktivierung/Fokus unter macOS? Treffer der MouseArea?), beheben, dann den Check zum Pflicht-Check machen.

---

## Erledigt
- **P8** Lage beim Tippen nach dem Ablegen an einer Wand (2026-11-11): solange nichts anderes geschah, hält ein getipptes Maß die Unterseite an der Wand, im selben Schritt; mit benannten Maßen im Wert wird nicht nachgeführt.
- **P7** Exakte Fangpunkte aus dem Kernel (2026-11-11): `GeometryKernel::snapPoints` (Ecken, Kantenmitten, Kreismitten, Mitten ebener Flächen, exakt aus OCCT; Nahtkanten ausgenommen), im Hintergrund mit dem Netz berechnet, `SnapSession` nimmt sie statt der Netzpunkte (Rückfall: Netzpunkte).
- **P18** Abstandsmaß auf einer Platte (2026-11-03, PR #83): Ohne Nachbarn misst das Maß bis zum Rand des kleinsten Körpers, der die Grundfläche umfasst und auf dem der Körper steht oder in dem er steckt (nicht ein Körper darüber, z. B. Schrift über dem Loch); danach bis zum Bettrand. Festlegung ohne Umschalter. `app_flow_a_keychain` tippt den Abstand.
- **P2** Formen-Galerie schließt bei Klick auf leeren Grund (2026-11-03, PR #84), Simulations-Prüfung `expectflyout:`.
- **P3** Abstände der Grundbausteine aus `Theme.space1..6`, Elide in Statuszeile und Tabs (2026-11-03, PR #85). Bildschirmfotos vorher/nachher fehlen; nachholen mit P20.
- **Export ohne Werkzeug-Körper** (2026-11-03, PR #86): Die Werkzeuge von „Kombinieren ▸ Abziehen“ (Geisterformen) wurden als massive Objekte exportiert; gefunden bei der Slicer-Prüfung.
- **P12** `main.cpp` und `Main.qml` verkleinert (2026-10-28): `simulate.cpp`, `RecipeSentences.qml`, `FileNotices.qml`.
- **P13** Löcher im Hintergrund prüfen: gemessen, ein Zug-Schritt ≈ 31 ms bei 10 Körpern und 20 Löchern, unter der 50-ms-Grenze; kein Umbau. Wieder ansehen bei Loch-Mustern (AP-2.8) oder großen Körpern.
- **P15** Rezept-Sätze ausgelagert (Rest nach P19 und P21).
- **Prüfliste T1–T11 vom 2026-10-27:** alles gemergt (PR #68–#80).
- **P10** Tastatur-Zoom (2026-11-11): +/= und - zoomen zur Bildmitte um 20 %, ohne Strg/⌘ (bleibt für UI-Skalierung frei), aus beim Tippen.
