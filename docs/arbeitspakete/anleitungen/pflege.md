# Pflege: Fehler, Politur und Folgearbeiten

Kleine Aufgaben außerhalb der APs. Jede ist für sich ein PR (Branch `pflege-<nummer>-<stichwort>`). Reihenfolge: P1 zuerst (möglicher
Fehler mit Datenfolgen), dann nach Bedarf. Allgemeine Regeln: [README](README.md), [Handbuch](handbuch.md).

---

## P1 Kugel-Radius-Griff: unsauber und nicht rückgängig? (Fehler prüfen)
**Größe:** S · **Herkunft:** Rückmeldung Product Owner 2026-10-18 („fühlt sich wieder unsauber an“, „ließ sich später nicht rückgängig machen“).
PR #28 hat am selben Tag mehrere Kugel-Probleme behoben (Griff mit Steigung, gehaltener Griff bleibt bei leerer Griffliste). Ob der Fehler
danach noch besteht, ist **nicht geprüft**.
1. Test schreiben, **bevor** irgendetwas geändert wird (`tests/unit/ui/test_handles.cpp`): Kugel ablegen, Radius-Griff in 30 kleinen
   Schritten von 10 auf 40 mm ziehen, loslassen; danach eine andere Aktion (Quader ablegen); dann zweimal Undo → Radius wieder 10.
   Varianten: sehr schnell ziehen, über `kLargestMillimetres` hinaus ziehen, Esc mitten im Zug, Zug während einer laufenden Neuberechnung.
2. Fenster-Test `app_sphere_radius_undo` mit `put:sphere;tool:size;grab:radius,…;put:box;key:undo;key:undo;near:radius,10`
   (`key:`-Aktion für ⌘Z prüfen bzw. ergänzen).
3. Grün → in der Phasendatei/PR vermerken „nicht nachstellbar“, Product Owner um eine Schritt-für-Schritt-Beschreibung bitten, falls es
   wieder auftritt. Rot → Ursache suchen (typisch: `beginChange` ohne `commitChange`, weil der Zug verloren ging; siehe
   `HandleController::refresh`, Fall „Griff verschwindet mitten im Zug“), beheben, Test bleibt.
4. „Unsauber“: den Zug mit dem Product Owner ansehen lassen (Bild/Video), erst dann ändern (Haltepunkt).

## P2 Formen-Galerie schließt bei Klick aufs Bett
**Größe:** S · **Herkunft:** Wunsch Product Owner 2026-10-18.
Nach dem Hinzufügen einer Form soll ein Klick auf leeren Grund (Bett, Hintergrund) die geöffnete Galerie (`ShapeFlyout.qml`) schließen.
1. In der Ansicht gibt es einen Klick auf leeren Grund schon (hebt die Auswahl auf, `SelectionController`). Dort zusätzlich ein Signal
   `emptyClicked()` ausgeben; `Main.qml` schließt darauf die Galerie.
2. Nicht schließen, wenn gerade eine Form abgelegt wird oder der Klick auf einen Körper geht.
3. Fenster-Test: Galerie öffnen (`--simulate`-Aktion für das Öffnen der Galerie prüfen/ergänzen), `click:` auf leeres Bett, Prüfung, dass
   die Galerie zu ist (eigene Prüf-Aktion `expectclosed:shapes` oder Eigenschaft abfragen).

## P3 Politurdurchgang: Abstände und Beschriftungen
**Größe:** S–M · **Herkunft:** Product Owner 2026-10-16 („Beschriftungen in Feldern und Knöpfen unordentlich, Abstände zum Rand nicht
eingehalten“); die Werkzeugpalette ist mit PR #30 behoben.
1. Alle QML-Bausteine mit Text (Btn, Segmented, Toggle, NumberField, WordField, FontField, Section, PanelHeader, StatusBar, SnapControl,
   MenuStrip, DocTabs) gegen `docs/08-gestaltung.md` (Abstände, Schriftgrößen) prüfen. Bildschirmfotos vorher/nachher (Hell, Dunkel,
   Deutsch – deutsche Wörter sind länger).
2. Feste Abstände durch `Theme`-Werte ersetzen. Kein Text darf den Rand berühren; lange deutsche Wörter: Elide oder Umbruch nach Mockup.
3. Fenster-Vergleichstest `app_window_*_matches_design` muss grün bleiben; Abweichungen vom Mockup in `08-gestaltung.md` vermerken.

## P4 AP-1.3-Reste: Linux-Messung und Optik auf Linux
**Größe:** S · Offen aus AP-1.3.
1. `scripts/run --release -- --benchmark 2000000` auf einem Linux-Rechner mit Intel/AMD-iGPU (Testrechner B hat Intel HD 620; dort nur
   unter Xvfb + Vulkan messbar, siehe `docs/testumgebung.md`). Ergebnis (gpu_ms_avg) in AP-1.3 eintragen; < 16,7 ms → Kriterium 1 abhaken.
2. Weiche Schatten (SSAO) unter Vulkan/OpenGL ansehen: Bild mit und ohne (`--no-soft-shadows`), in den PR.
3. Druckerliste (`src/ui/src/printer_catalog.cpp`): Bettmaße gegen die Herstellerdatenblätter prüfen, Quelle je Drucker als Kommentar.

## P5 AP-0.1: frischer Mac
**Größe:** S · Offen aus AP-0.1 (Kriterium 4). Der virtuelle Mac `fission3d-mac14` (siehe `docs/testumgebung.md`) hat nur die
Kommandozeilen-Werkzeuge, kein Homebrew, kein Xcode. Dort `git clone`, `scripts/setup`, `scripts/run` durchspielen; alles, was fehlt oder
unklar ist, in `scripts/setup` verbessern. Ergebnis in AP-0.1 eintragen; Status nur nach OK des Product Owners auf „erledigt“.

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
**Größe:** S · Idee aus AP-1.6b3 (vom Product Owner als „Folgearbeit“ akzeptiert, nicht beauftragt). **Haltepunkt:** erst fragen, ob gewünscht.
Problem: Nach dem Ablegen an einer Wand und `50 ⇥ 20 ⇥ 4` ändert sich die Höhe um die Körpermitte; die Form rutscht von der Wand.
Lösung: Solange der Schritt „frisch abgelegt“ ist (bis zum ersten anderen Befehl), Lage so nachführen, dass die Unterseite auf der Wand bleibt
(`geo::seatOnFace` mit der gemerkten Fläche).

## P9 Rezept umsortieren per Ziehen
**Größe:** M · UX §4 erlaubt es, AP-1.9 hat es ausgelassen. Ziehen einer Zeile; erlaubt nur, wenn keine Abhängigkeit verletzt wird
(Eingänge müssen vorher stehen); sonst Erklärung inline. Neuer Command `makeMoveFeature(id, newIndex)`, Test mit Undo und mit verbotener
Reihenfolge.

## P10 Tastatur-Zoom
**Größe:** S · Angeboten am 2026-10-16, keine Antwort. **Haltepunkt:** erst fragen. Vorschlag: `+`/`-` und ⌘+/⌘− zoomen zur Mitte der Ansicht
(`Viewport3D::zoomBy`); Achtung: ⌘+/⌘− wären sonst UI-Skalierung (AP-1.11) – Belegung mit dem Product Owner klären.

## P11 ⌥ beim Ablegen unter Linux prüfen
**Größe:** S · Offen aus AP-1.6b3. Manche Linux-Fenstermanager (KDE, GNOME) nehmen ⌥(Alt)+Maus für sich (Fenster verschieben). Auf einem
Linux-Desktop prüfen (Product Owner oder Testrechner mit Desktop-Sitzung); wenn es nicht geht: Haltepunkt mit Vorschlag einer Ersatztaste
(z. B. Strg) und Hinweis in der Hinweiszeile.

## P12 `main.cpp` aufräumen
**Größe:** S · `apps/fission3d/src/main.cpp` hat ~950 Zeilen; `playActions` (die `--simulate`-Aktionen) in eine eigene Datei
`apps/fission3d/src/simulate.cpp` ziehen, ohne Verhaltensänderung. Alle Fenster-Tests müssen unverändert grün sein.

## P13 Löcher im Hintergrund prüfen (Regel 9)
**Größe:** M · Offen aus AP-1.7. `core::reconcileCuts` rechnet die Berührung von Löchern und Körpern (Booleans) im Oberflächen-Thread: kalt
26 ms, warm 4 ms bei einem Loch in einer Platte, wächst mit der Zahl der Körper und Löcher. **Vor dem Nutzertest 1 (AP-1.13) erledigen.**
1. Messung zuerst: ein Dokument mit 10 Körpern und 20 Löchern (Test in `tests/unit/core/test_cuts.cpp`, nur melden); ab etwa 50 ms ist es ein Fehler.
2. Lösung: die Prüfung läuft in der Neuberechnung (`RecomputeEngine`) im Hintergrund und liefert eine Liste „Loch X braucht Schnitt Y“;
   die Oberfläche wendet sie beim Loslassen im selben Undo-Schritt an. Beim Ziehen zeigt die Ansicht das letzte Ergebnis (wie heute).
3. Die Tests `[cuts]` und `holes cut live …` müssen unverändert grün bleiben; Zeitmessung im PR nennen.

## P14 Teile eines geteilten Körpers drehen und vergrößern
**Größe:** S · Offen aus AP-1.7e. Ein Teil hat nur den Schritt „Teil platziert“ (nur x, y, z): Drehen und Größe ändern sind aus
(`DocumentModel::beginTurn/beginScale` geben false). Vorschlag: den Schritt `place` um `rotX/rotY/rotZ` ergänzen (Drehung des Teils um seine Mitte,
wie bei den Formen) und für das Vergrößern den Eck-Griff auf den Schritt davor zurückführen (Teilen und Quelle gemeinsam vergrößern, `scaleStartOf`
geht dafür die Kette zurück). **Haltepunkt:** erst mit dem Product Owner klären, ob Vergrößern eines Teils das ganze Geteilte vergrößern soll.

---

## P15 Reste aus AP-1.8 und AP-1.9
**Größe:** S–M · **Herkunft:** Abschlussvermerke AP-1.8 und AP-1.9.
- Kontextleiste: „Mehr ▾“ (Seltenes), Ausrichten bei mehreren Körpern; Kante bekommt Aktionen mit AP-2.5. Die Leiste kann Maßzahlen der Ansicht kurz verdecken.
- Rezept: Sätze aus `Main.qml` (`stepText`) in eine Tabelle `RecipeSentences.js` ziehen, mit einem Test, dass jeder registrierte Feature-Typ einen Satz hat.
- Fenster-Tests `app_ring_turn` und `app_snap_corner_handle` fallen unter Last gelegentlich durch (einzeln grün): Ursache suchen, nicht abschwächen (Ablauf D). Tipp aus `app_flow_d_hole` (PR #51): Eine Prüfung von `--simulate` muss auf das Ende der Aktion warten, nicht auf feste Zeiten; ein Diagnose-PR mit zusätzlichen Meldungen zeigt die Ursache direkt in der CI.
