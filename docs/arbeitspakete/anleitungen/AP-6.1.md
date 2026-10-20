# AP-6.1 Automatische Updates

**Größe:** M · **Phasendatei:** AP-6.1

## Ziel
Fission3D findet neue Versionen selbst und aktualisiert sich ohne Datenverlust: macOS mit Sparkle 2 (EdDSA-signierter Appcast, Kanäle Stabil/Beta),
Linux-AppImage mit eingebetteter Update-Information (zsync) und Hinweis in der App, Flatpak über Flathub. Der Hinweis ist unaufdringlich, mit
Änderungsübersicht in Alltagssprache. Update 0.9 → 1.0 behält Einstellungen, Profile und Bausteine.

## Voraussetzungen
AP-0.7 (Pakete), **bezahltes Apple-Konto** für Developer-ID-Signatur und Notarisierung (Sparkle verlangt eine gültige Signatur der neuen App;
ad hoc signierte Updates scheitern an Gatekeeper). Ohne Konto: Haltepunkt.

## Vorher lesen
`docs/adr/0006-paketierung.md`, `docs/paketierung.md` (Secrets, Notarisierung vorbereitet), `.github/workflows/ci.yml` (Job „Package“),
`packaging/macos/`, `packaging/linux/`; `docs/03-architektur.md` §8 (Sparkle MIT).

## Haltepunkte
1. **Apple-Konto** (99 €/Jahr) vorhanden? Ohne: macOS-Updates nur als Hinweis „neue Version verfügbar“ mit Download-Link (kein Selbst-Update).
2. **Wo liegt der Appcast/Update-Server?** Empfehlung: GitHub Releases + Appcast-XML als Release-Asset oder GitHub Pages (kein eigener Server).
3. **Flathub:** Quellbau-Flatpak mit KDE-Runtime (AP-0.7 offen) und Einreichung bei Flathub – eigener Teil, Zeitbedarf durch Review dort.

## Schritte
1. Release-Workflow (`.github/workflows/release.yml`): bei Tag `v*` Pakete bauen, signieren (Secrets), notarisieren, Appcast erzeugen (Sparkle-Werkzeug
   `generate_appcast`, EdDSA-Schlüssel als Secret), AppImage mit `zsync`-Datei und Update-Info (`gh-releases-zsync|SloppyHenry|Fission3D|latest|*.AppImage.zsync`),
   Release-Notizen aus `docs/aenderungen/<version>.md` (Alltagssprache, DE/EN).
2. macOS: Sparkle 2 als Framework im Bundle (vcpkg-Port prüfen; sonst als Release-Binary mit Prüfsumme in `packaging/macos`, Lizenz in Notices),
   Brücke in `src/services/updates_macos.mm` (Objective-C++), Einstellungen: automatisch prüfen an/aus, Kanal.
3. Linux AppImage: Prüfung der neuesten Version über die GitHub-API (ohne Konto, Rate-Limit beachten, höchstens 1×/Tag), Hinweis-Karte mit „Jetzt
   aktualisieren“ (AppImageUpdate-Logik: zsync-Download der neuen Datei neben der alten, dann ersetzen) oder Link.
4. Flatpak: Manifest für Flathub (Haltepunkt 3).
5. Datenverlust-Schutz: Einstellungen, Profile, Bausteine, Motive liegen im Benutzerordner (nicht im Paket) – prüfen; Format-Versionen der Einstellungen
   mit Migration.

## Tests
Update-Prüfung mit Test-Appcast (lokale Datei-URL); Versionsvergleich (0.9.0 < 1.0.0 < 1.0.1-beta? Semver-Regeln testen); manueller Update-Test 0.9 → 1.0
auf beiden Plattformen (Akzeptanzkriterium, Protokoll im PR).

## Nicht tun
Keine Telemetrie über den Update-Check (nur Versionsabfrage). Keine Updates ohne Signaturprüfung.
