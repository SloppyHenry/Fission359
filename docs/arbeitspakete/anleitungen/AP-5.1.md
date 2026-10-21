# AP-5.1 Missions-Engine und erste Missionen

**Größe:** L · **Phasendatei:** `phase-5-lernen-bausteine.md`, AP-5.1

## Ziel
Neulinge lernen im Programm: geführte Mini-Projekte (3–10 min) mit Hervorhebung, die **den echten Modellzustand prüfen** („Super, der Quader ist 50 mm
breit!“) statt Weiter-Klicks zu zählen. Wer abweicht, bekommt einen sanften Hinweis, wird nie blockiert. Fortschritt wird gespeichert; Abbrechen und
Fortsetzen jederzeit. Missionen sind Datendateien: eine neue Mission braucht keinen Code. Ein Test spielt jede Mission per Skript durch.

## Voraussetzungen
AP-1.12 (Hinweise, Befehlskatalog). Für die Missionen 2–5 die jeweiligen Werkzeuge (Stern-Generator AP-4.8, Bohrung AP-2.7, Passung AP-3.3, Ausstechform AP-4.5).

## Vorher lesen
`docs/02-ux-konzept.md` §11 (Missionen, **Lotse**: ein-/ausschaltbar, blendet nur Hilfe, nie Werkzeuge), §15; `docs/08-gestaltung.md` (Lotse-Karte);
Mockup (Lotse „Schritt 1/8“); `docs/06-rueckfragen.md` R17 (kein Geführt/Experte-Modus).

## Haltepunkte
1. **Bedingungssprache:** Empfehlung: kleine, eigene, sichere Ausdruckssprache auf einem **Lese-Modell** des Dokuments (z. B. `bodies.count == 1`,
   `step(last).type == "box"`, `abs(step(last).width - 50) < 0.5`), ausgewertet mit dem vorhandenen Formelparser erweitert um Namen/Vergleiche –
   **kein** JavaScript-`eval` und kein Python. Bestätigen.
2. **Missionsformat:** JSON in `resources/missions/<id>.json`, Texte als Übersetzungsschlüssel? Problem: `qsTr` liest keine JSON-Dateien. Empfehlung:
   Texte in einer QML/JS-Datei `MissionTexts.js` mit `qsTr` (Schlüssel = Missions-Id + Schritt), JSON nur für Ablauf und Bedingungen.

## Schritte
1. **Lese-Modell** (C++, `ui::MissionContext`): liefert Werte aus Dokument und Auswahl (Anzahl Körper, Typ/Werte des letzten oder gewählten Schritts,
   Farbe, ob Loch, Werkzeug aktiv). Nur lesend.
2. **Bedingungen** auswerten (Haltepunkt 1), mit Unit-Tests (wahr/falsch/Fehler in der Bedingung = Mission meldet „Mission defekt“ im Protokoll, nie Absturz).
3. **Engine:** Schritte mit `text`, `highlight` (UI-Element über `objectName` oder 3D-Element über Körper/Fläche), `condition`, optional
   `hintAfterSeconds` + `hint`, `reward` (kurzer Text), `wrongWay` (Bedingungen für typische Abweichungen + sanfter Hinweis).
   Fortschritt in den Einstellungen (AP-1.11) je Mission.
4. **Lotse** (Karte unten in der Ansicht nach Mockup): „Schritt 2/8“, Text, Hervorhebung (pulsierender Rahmen um das UI-Element), Abbrechen, Ein/Aus.
5. **Missionsübersicht** mit dezenten Abzeichen; Einstieg aus dem Erststart (AP-1.11 Haltepunkt 3) und der Startseite.
6. **Missionen 1–5** (UX §11) als Dateien; je Mission ein Testskript: Die Mission wird mit `--simulate`-Aktionen durchgespielt und muss am Ende „fertig“
   melden (neue Prüf-Aktion `expectmission:<id>,done`). Akzeptanzkriterium 2.
7. Akzeptanzkriterium 1: eine Test-Mission nur als Datei hinzufügen (im Test-Ordner), ohne Code-Änderung → läuft.

## Abnahme
So probierst du es aus: Startseite ▸ Mission „Dein erster Schlüsselanhänger“ → Lotse führt; Quader 40 statt 50 breit → sanfter Hinweis, nicht blockiert.

## Nicht tun
Keine Missionen, die Werkzeuge ausblenden (UX §11). Kein Punktesystem/Bestenliste.
