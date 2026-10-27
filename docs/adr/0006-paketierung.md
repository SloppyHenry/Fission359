# ADR-0006: Paketierung (macOS-dmg, Linux-AppImage und Flatpak)

- **Status:** angenommen (Entscheidungen 1–3 vom Product Owner am 2026-10-13; Werkzeugwahl als Annahme des Agenten)
- **Datum:** 2026-10-13
- **Arbeitspaket:** AP-0.7

## Kontext
Das Programm soll auf macOS (arm64) und Linux (x86_64) installierbar sein, ohne dass jemand Bibliotheken
von Hand beschafft. Es gibt nur ein **kostenloses** Apple-Developer-Konto (keine Developer-ID, keine
Notarisierung, [R7](../06-rueckfragen.md)). Die CI baut auf Ubuntu 24.04 und macOS 15. Qt kommt dort aus vcpkg,
auf Entwicklerrechnern oft aus Homebrew oder apt.

## Entscheidungen

### 1. App-Kennung: `io.github.sloppyhenry.Fission359`  *(Product Owner)*
Eine umgekehrte Domain-Kennung wird für Bundle-ID, Flatpak-ID, `.desktop`-Datei, AppStream und
Wayland-App-ID gebraucht. Die gewählte Form passt zum GitHub-Repository, wird von Flathub ohne eigene Domain
akzeptiert und veröffentlicht keine weiteren Daten. Eine spätere Änderung würde Einstellungen und
Dateizuordnungen verwaisen lassen, deshalb steht sie an genau einer Stelle (`cmake/FissionPackaging.cmake`).
Projektdateien: Endung `.fission`, MIME-Typ `application/x-fission359`, macOS-UTI `<Kennung>.project`.

### 2. Mindestsystem: Ubuntu 24.04 und aktuelle Fedora, nicht 22.04  *(Product Owner)*
Ein AppImage oder Flatpak läuft nur auf Systemen mit mindestens dem glibc des Bauplatzes. Die CI baut auf
Ubuntu 24.04 (glibc 2.39). Für 22.04 wäre ein zweiter Bibliotheks-Zwischenspeicher und ein alter Compiler
nötig; die Unterstützung von 22.04 endet im April 2027. Ältere Systeme bauen das Programm selbst
(Backlog B-15: dynamisches Installationsskript). macOS: 14 oder neuer (Ziel der Bibliotheken und des Programms).

### 3. Flatpak: fertiges Programm verpacken, kein Quellbau  *(Product Owner)*
Flathub verbietet Netzwerkzugriff beim Bauen: alle Bibliotheken müssten als Quell-Module im Manifest stehen
(rund zehn Module, stundenlange Bauzeit). Für AP-0.7 verpackt das Flatpak daher das AppDir des AppImage
(`org.freedesktop.Platform` 25.08, Qt und Bibliotheken gebündelt). Das baut und startet lokal, ist aber
**nicht Flathub-tauglich**. Der Quellbau mit der KDE-Laufzeit folgt vor AP-6.1.

### 4. Werkzeuge  *(Annahme)*
- **macOS:** `macdeployqt` (Teil von Qt), Ad-hoc-Signatur mit `codesign`, Disk-Image mit
  [`dmgbuild`](https://github.com/dmgbuild/dmgbuild) (Python, MIT; reines Build-Werkzeug, wird nicht
  mitgeliefert). `create-dmg` und AppleScript scheiden aus, weil sie das Finder-Fenster steuern und in
  der CI nicht zuverlässig laufen.
- **Linux:** `linuxdeploy` mit Qt- und AppImage-Plugin (MIT, festgenagelte Version, Prüfsumme in
  `packaging/linux/tools.txt`), `patchelf` für die Wayland-Plugins, die das Qt-Plugin von linuxdeploy für
  Qt 6 nicht mitnimmt.
- **Nachprüfung im Paket:** Das Skript verweigert das Paket, wenn irgendeine Datei auf einen Pfad außerhalb
  der App zeigt (z. B. Homebrew oder `.vcpkg`).
- **Notarisierung** (`xcrun notarytool`, `stapler`) ist als Schritt vorhanden und überspringt sich ohne
  Zugangsdaten. Developer-ID-Signatur mit Hardened Runtime und Entitlements (`allow-jit`) ist vorbereitet.

## Konsequenzen
- **Einfacher:** `scripts/package` erzeugt auf jedem Rechner ein Paket; die CI baut und startet jedes
  Paket bei jedem Lauf. Wer ein bezahltes Apple-Konto hat, trägt nur sechs Secrets ein.
- **Schwerer:** Lokal gebaute Pakete laufen nur auf dem System des Rechners (Qt aus Homebrew bzw. neueres
  glibc); verteilt wird nur, was die CI baut. Das Flatpak ist nicht für Flathub geeignet. Ohne
  Notarisierung braucht jede Installation auf dem Mac den Gatekeeper-Umweg.
- **Folge-Arbeiten:** Quellbau-Flatpak (vor AP-6.1), echtes Icon, Dateiöffnen per Doppelklick (braucht
  AP-1.x/`.fission`-Format), `.deb`/`.rpm` optional, Backlog B-15.
- **Revisionskriterium:** Zeigt sich, dass das gebündelte Qt auf einer Zielverteilung abstürzt (etwa durch
  Grafikbibliotheken), wird auf die KDE-Laufzeit mit Quellbau umgestellt.

## Belege
- Abschlussvermerk AP-0.7 in `docs/arbeitspakete/phase-0-fundament.md`.
- Anleitung und Secrets: [docs/paketierung.md](../paketierung.md).
