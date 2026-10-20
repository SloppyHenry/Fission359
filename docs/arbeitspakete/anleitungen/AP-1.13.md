# AP-1.13 Nutzertest 1

**Größe:** S · **Phasendatei:** AP-1.13 · **Wer:** Der Product Owner führt den Test mit Menschen durch; der Umsetzer bereitet vor und wertet aus.

## Ziel
Fünf Menschen ohne CAD-Erfahrung (davon zwei Tinkercad-Nutzer) bauen einen Schlüsselanhänger mit ihrem Namen. Wir sehen, wo sie hängen
bleiben, und beheben die drei schlimmsten Probleme vor Phase 2. Bestanden, wenn ≥ 4 von 5 es in ≤ 10 Minuten schaffen.

## Voraussetzungen
Alle APs der Phase 1 erledigt. Ein Paket (`.dmg`/AppImage) aus der CI, das auf den Test-Rechnern startet.

## Vorbereitung (Umsetzer)
1. **Testpaket:** CI-Artefakt des aktuellen `main` herunterladen, auf einem frischen Benutzerkonto starten (Erststart-Assistent prüfen).
2. **Leitfaden** `docs/nutzertests/2026-xx-phase1-leitfaden.md` schreiben (Deutsch, für den Product Owner):
   - Begrüßung (Satz zum Vorlesen: „Wir testen das Programm, nicht dich.“), Einverständnis (Aufnahme ja/nein), Laut-Denken erklären.
   - Aufgabe als Karte zum Vorlegen: „Baue einen Schlüsselanhänger mit deinem Namen, ungefähr 5 × 2 cm, mit einem Loch für den Ring.
     Speichere ihn als Datei für den Drucker.“ **Keine** Hinweise auf Werkzeugnamen.
   - Was der Testleiter **nicht** tun darf (helfen, zeigen); wann er doch hilft (nach 3 Minuten völligem Stillstand, und dann notieren).
   - Beobachtungsbogen: Zeitstempel, Stelle, was die Person sagt/tut, Schwere (1 blockiert, 2 kostet > 1 min, 3 Ärger).
   - SUS-Fragebogen (10 Fragen, deutsche Standardfassung) als Ausdruck/Formular.
3. **Bildschirmaufnahme:** macOS: Bildschirmaufnahme (⇧⌘5) mit Ton; Linux: OBS. Anleitung in den Leitfaden.
4. **Auswertungsvorlage** `docs/nutzertests/2026-xx-phase1.md`: Tabelle je Person (Zeit, geschafft ja/nein, SUS-Wert), Liste der
   Stolperstellen sortiert nach Schwere × Häufigkeit.

## Durchführung (Product Owner)
Fünf Sitzungen à 20–30 Minuten nach Leitfaden. Aufnahmen und Bögen in einen Ordner legen (nicht ins Repo, Datenschutz).

## Auswertung (Umsetzer, mit den Notizen des Product Owners)
1. Notizen und Aufnahmen durchgehen, Stolperstellen zusammenfassen (gleiche Ursache = ein Eintrag).
2. SUS ausrechnen (Standardformel: ungerade Fragen Wert − 1, gerade 5 − Wert, Summe × 2,5).
3. Je Stolperstelle: Ursache (UX-Konzept verletzt? fehlt etwas? missverständlicher Text?), Vorschlag, Aufwand (S/M/L).
4. **Top 3** markieren. Für jede eine Bauanleitung im Format dieses Ordners anlegen (`AP-1.13-fix-<n>.md`), mit Haltepunkt, wenn die Lösung
   eine UX-Entscheidung ist.
5. Bericht in `docs/nutzertests/2026-xx-phase1.md` (ohne Namen der Personen: P1–P5), Ergebnis in die Phasendatei.
6. Prüfen gegen ADR-0010 Revisionskriterium: Suchten Einsteiger die Bewegung im Rezept? Wenn ja, im Bericht hervorheben.

## Abnahme
- Bericht liegt vor; ≥ 4/5 in ≤ 10 min, sonst Top-3-Fixes als Anleitungen angelegt und vor Phase 2 eingeplant.

## Nicht tun
- Keine echten Namen, Gesichter oder Stimmen ins Repo. Keine Fixes während der Testtage (Ergebnisse sonst nicht vergleichbar).
