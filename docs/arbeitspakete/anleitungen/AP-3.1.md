# AP-3.1 Druckerprofile

**Größe:** S · **Phasendatei:** `phase-3-druckbereit.md`, AP-3.1

## Ziel
Ein Druckerprofil beschreibt, was der Drucker kann: Bauraum, Düse, Standard-Schichthöhe, Mindestwand, Lochkorrektur, Standard-Spiel, Anzahl Farben,
bevorzugter Slicer. ≥ 12 Profile sind mitgeliefert; eigene lassen sich anlegen und duplizieren. Ein Profilwechsel ändert sofort Druckbett und
Prüfregeln.

## Voraussetzungen
AP-1.11 (Einstellungen, Drucker-Auswahl).

## Vorher lesen
`src/ui/include/fission/ui/printer_catalog.h` + `.cpp` (heutige feste Liste mit 12 Einträgen, Bettmaße **ungeprüft**, siehe [pflege.md](pflege.md) P4);
`docs/03-architektur.md` §6 (Druck-Toleranzen), §9 (`resources/printers/`); `docs/02-ux-konzept.md` §11 (Erster Start: Drucker).

## Haltepunkte
1. **Profil im Dokument?** Empfehlung: Das gewählte Profil ist Programm-Einstellung; das Dokument speichert nur die **Id** des Profils, mit dem es
   gebaut wurde (für Hinweise beim Öffnen auf einem anderen Rechner), nicht die Werte.
2. **Liste der mitgelieferten Profile** (Vorschlag aus UX §11: Bambu A1, A1 mini, P1S, X1C; Prusa MK4, Core One, MINI; Creality Ender-3 V3, K1;
   Voron 2.4 (300); Anycubic Kobra 3; Elegoo Neptune 4) bestätigen.

## Schritte
1. Schema `resources/printers/schema.json` und je Drucker eine Datei `resources/printers/<id>.json`: `id`, `brand`, `model`, `bed {x,y,z}`, `nozzle`,
   `layerHeight`, `minWall` (Standard 2 × Düse), `holeCorrection` (mm, Standard 0,2), `clearance` (Standard 0,2), `colours` (1, 4 für AMS…),
   `slicer` (bambu | orca | prusa | cura), `source` (URL des Datenblatts).
2. Laden in `src/services/printer_profiles.*` (JSON mit nlohmann), Validierung gegen das Schema (eigene einfache Prüfung: Pflichtfelder, Bereiche;
   kein neues Paket), Fehler im Protokoll und Profil ausgelassen.
3. Eigene Profile im Benutzerordner (`AppDataLocation/printers/`), „Duplizieren“ und „Bearbeiten“ auf der Einstellungsseite (AP-1.11), Zahlenfelder.
4. `PrinterCatalog` ersetzen: `DocumentModel` (bzw. Einstellungen) liest die Profile; Druckbett, Bauhöhe (endlich auch zeichnen: Rahmen der Bauhöhe
   gestrichelt, AP-1.3 offen), und die Werte für Druckcheck (AP-3.2), Lochkorrektur (AP-2.7), Spiel (AP-3.3).
5. Bettmaße **gegen Datenblätter prüfen** und Quelle in `source` (P4 damit erledigt).

## Tests
Jede mitgelieferte Datei ist gültig (Test lädt alle); kaputte Datei wird ausgelassen; eigenes Profil anlegen/laden; Profilwechsel ändert Szene
(Bettgröße) ohne Undo-Schritt (wie heute).

## Abnahme
Kriterium: ≥ 12 Profile mit Validierung. So probierst du es aus: Einstellungen ▸ Drucker ▸ „Prusa MINI+“ → Bett klein; „Duplizieren“, Düse 0,6 →
eigenes Profil.

## Nicht tun
Keine Slicer-Einstellungen (Temperaturen, Geschwindigkeiten) – das macht der Slicer.
