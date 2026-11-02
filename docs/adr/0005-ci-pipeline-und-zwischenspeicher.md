# ADR-0005: CI-Pipeline und Zwischenspeicher

- **Status:** angenommen
- **Datum:** 2026-10-13
- **Arbeitspaket:** AP-0.3

## Kontext
Jede Änderung soll automatisch auf macOS (arm64) und Ubuntu 24.04 gebaut und getestet werden, mit
Speicherfehler-Erkennung (ASan/UBSan) und als optimierte Version. Die Fremdbibliotheken (Qt,
OpenCASCADE, …) brauchen kalt etwa zwei Stunden; ein Pull Request darf nicht jedes Mal so lange dauern.
Rahmenbedingungen: GitHub Actions (öffentliches Repo, kostenlos), 10 GB Zwischenspeicher je Repo,
GitHub löscht Einträge nach 7 Tagen ohne Nutzung.

## Entscheidung
1. **Drei Stufen:** *Checks* (schnell, Linux), *Libraries* (je Betriebssystem ein Job, baut und speichert die
   Bibliotheken), *Build* (vier Jobs: macOS und Ubuntu × `debug-asan` und `release`). Die Build-Jobs hängen
   von *Libraries* ab und lesen nur noch den Zwischenspeicher.
2. **Qt kommt in der CI immer aus vcpkg** (`FISSION_QT_SOURCE=vcpkg`), damit beide Plattformen dieselbe Version
   prüfen (Ubuntu 24.04 hat nur Qt 6.4, siehe [ADR-0004](0004-abhaengigkeiten-vcpkg.md)).
3. **Ein Qt, schlank:** Das Host-Triplet ist gleich dem Ziel-Triplet. Ohne das baut vcpkg ein zweites, volles Qt
   (ICU, OpenSSL, PostgreSQL, Widgets; Debug und Release) nur für die Werkzeuge, wir cross-kompilieren aber nie.
4. **Zwischenspeicher:** vcpkg-Archive mit dem Schlüssel aus `vcpkg.json` und den Triplets; ein Rückfall-Schlüssel
   nach Betriebssystem erlaubt Teil-Treffer. Gespeichert wird nur auf `main` und auch bei einem fehlgeschlagenen
   Bau (`always()`), damit ein Neustart nicht bei null beginnt. Pull Requests lesen nur.
5. **Lebendig halten:** wöchentlicher Lauf (montags) gegen die 7-Tage-Löschung.
6. **Hinweise statt Blockade:** clang-tidy läuft nur in Pull Requests und nur auf geänderten Dateien, ohne zu
   blockieren, bis die Meldungen verlässlich sind.
7. **Sicherheit:** Aktionen sind auf Commit-Hashes festgenagelt, Berechtigungen minimal (`contents: read`),
   Dependabot schlägt Aktualisierungen vor.

## Konsequenzen
- Gemessen: kalt 2 h 11 min, warm 3 min 22 s (alle Jobs grün auf beiden Plattformen).
- Der Speicher belegt etwa 1,2 GB von 10 GB; alte Schlüssel verfallen von selbst.
- Änderungen an `vcpkg.json` oder `cmake/vcpkg-triplets/` lösen einen neuen, teuren Bibliotheksbau aus.
  Das ist gewollt (Reproduzierbarkeit), aber ein Grund, Abhängigkeiten gebündelt zu ändern.
- Systempakete der Runner sind Teil des Vertrags: `scripts/setup` ist die einzige Stelle, die sie nennt, und die
  CI ruft es auf. So bleibt die lokale Einrichtung und die CI deckungsgleich.
