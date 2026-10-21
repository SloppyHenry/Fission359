# AP-6.5 Beta-Programm und Feedback

**Größe:** S · **Phasendatei:** AP-6.5

## Ziel
Eine geschlossene Beta mit 30–100 Personen (Neulinge, Tinkercad- und Fusion-Nutzer, Linux und macOS). Ein Feedback-Knopf in der App (Text + optional
Bildschirmfoto). Wöchentliche Auswertung und Priorisierung. Bestanden: SUS ≥ 80 in der Beta-Umfrage, keine offenen Datenverlust- oder Absturzfehler der
Schwere 1.

## Voraussetzungen
AP-6.1 (Updates, damit Beta-Fixes ankommen), AP-6.2 (Diagnose-Paket).

## Haltepunkte
1. **Kanal:** Forum, Discord oder Matrix (Rückfragen offen). Empfehlung: GitHub Discussions + Issues (kein zusätzliches Konto außer GitHub; aber Neulinge
   haben oft keins) **oder** ein Formular ohne Konto. Entscheidung beim Product Owner.
2. **Wohin geht das Feedback aus der App?** Ohne Server: Feedback-Knopf erzeugt eine Datei bzw. öffnet eine vorausgefüllte Mail/GitHub-Issue-Seite im
   Browser (nichts wird im Hintergrund gesendet).

## Schritte
1. Feedback-Knopf (Hilfe ▸ Feedback geben): Textfeld, Häkchen „Bildschirmfoto anhängen“ (Vorschau), optional Diagnose-Paket; Versand nach Haltepunkt 2.
2. Beta-Kanal für Updates (AP-6.1), Beta-Kennzeichnung in der App.
3. Einladung und Anleitung für Teilnehmende (`docs/beta/einladung.md`, DE/EN), Umfrage (SUS + 5 offene Fragen).
4. Wöchentliche Auswertung: Vorlage `docs/beta/woche-<n>.md` (Fehler nach Schwere, Wünsche, Top-3 der Woche), Issues mit Labels (`beta`, `schwere-1` …).

## Abnahme
Kriterien aus der Phasendatei; Auswertung abgeschlossen.

## Nicht tun
Keine Daten ohne Einwilligung sammeln. Keine Teilnehmernamen im Repo.
