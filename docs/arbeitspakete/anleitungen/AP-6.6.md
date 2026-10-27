# AP-6.6 Rechtliches

**Größe:** S · **Phasendatei:** `phase-6-release.md`, AP-6.6 · Früh beginnen: Namensprüfung braucht Vorlauf.

## Ziel
Fission359 erfüllt alle Lizenzpflichten und ist rechtlich veröffentlichungsfähig: GPL-3.0-or-later bestätigt, Third-Party-Notices im Über-Dialog,
Quellcode-Angebot je Release, LGPL-Hinweise für Qt und OCCT, Beitragsregeln mit DCO, Namens-/Markenprüfung „Fission359“, Impressum und Datenschutz für
Website und Update-Server.

## Voraussetzungen
Keine (AP-6.1/6.4 brauchen die Ergebnisse).

## Haltepunkte
1. **Rechtsberatung:** Dieses AP ersetzt keine Rechtsberatung. Markenprüfung und Impressum/Datenschutz sollte der Product Owner fachlich prüfen lassen.
   Der Umsetzer bereitet Recherche und Entwürfe vor.
2. **Name:** Ergebnis der Recherche (Verwechslung mit „Fusion“, eingetragene Marken „Fission“ in Klasse 9/42) vorlegen; Entscheidung beim Product Owner (R13).

## Schritte
1. Über-Dialog zeigt `THIRD_PARTY_NOTICES.md` vollständig (scrollbar), Lizenztexte der Bibliotheken liegen im Paket (macOS: `Contents/Resources/licenses`,
   Linux: `share/licenses/fission359`); Test: Paket enthält die Dateien (CI-Paketjob).
2. LGPL-Pflichten Qt/OCCT: dynamisch gelinkt (prüfen im Paket mit `otool -L`/`ldd`), Hinweis im Über-Dialog „Diese Bibliotheken lassen sich austauschen“
   mit Verweis auf die Quellen.
3. Quellcode-Angebot: Release-Workflow (AP-6.1) erzeugt Git-Tag + Quell-Archiv (inkl. `third_party/` und vcpkg-Manifest) als Release-Anhang; Hinweis im
   Über-Dialog mit Link.
4. DCO: `CONTRIBUTING.md` um „Signed-off-by“ ergänzen, GitHub-App/Action „DCO“ als Prüfung (Action-Version festnageln wie die anderen).
5. Namensrecherche: DPMA/EUIPO/WIPO-Datenbanken (manuell, Ergebnisse als Tabelle in `docs/rechtliches/name.md`), Domains prüfen.
6. Entwürfe Impressum/Datenschutz für Website und Update-Server (keine Telemetrie, nur Server-Logs für Updates) in `docs/rechtliches/`.

## Abnahme
Checkliste in der Phasendatei abgehakt; Entscheidungen des Product Owners vermerkt.

## Nicht tun
Keine Rechtstexte aus anderen Projekten kopieren. Kein CLA.
