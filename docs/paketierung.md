# Paketierung: Installieren und Pakete bauen

Stand: 2026-10-13 (AP-0.7, Entscheidungen in [ADR-0006](adr/0006-paketierung.md)).
Die fertigen Pakete baut die CI bei jedem Lauf (Reiter **Actions** → Lauf öffnen → Abschnitt **Artifacts**,
14 Tage lang abrufbar). Lokal gebaute Pakete laufen nur auf dem Rechner, auf dem sie entstanden sind.

## Systemvoraussetzungen

| System | Voraussetzung |
|---|---|
| macOS | 14 oder neuer, Apple Silicon (arm64) |
| Linux (AppImage, Flatpak) | x86_64, Ubuntu 24.04 oder neuer bzw. aktuelle Fedora; Wayland oder X11; Vulkan oder OpenGL |
| Ältere Linux-Systeme (z. B. Ubuntu 22.04) | Es gibt kein fertiges Paket. Das Programm selbst bauen (`scripts/setup`, `scripts/run`); ein Installationsskript dafür steht im Backlog (B-15) |

## Installieren (für Nutzer)

### macOS (.dmg)

Fission3D ist noch nicht bei Apple registriert (das kostet 99 €/Jahr). macOS warnt deshalb beim ersten Start. Das ist
erwartet und kein Fehler im Programm.

1. Die Datei `Fission3D-<Version>-macos-arm64.dmg` doppelklicken.
2. **Fission3D** auf **Programme** (Applications) ziehen.
3. Fission3D aus **Programme** öffnen. macOS meldet, das Programm könne nicht überprüft werden. Auf **Fertig** klicken.
4. **Systemeinstellungen → Datenschutz & Sicherheit** öffnen, nach unten scrollen und bei Fission3D auf
   **Trotzdem öffnen** klicken. Mit dem Passwort bestätigen.
   *(Bis macOS 14 genügt Rechtsklick auf das Programm → „Öffnen“ → „Öffnen“.)*
5. Ab jetzt startet Fission3D wie jedes andere Programm.

Alternativ im Terminal: `xattr -dr com.apple.quarantine /Applications/Fission3D.app`

Dieselbe Anleitung liegt zweisprachig als Datei **Installationshilfe** im Disk-Image.

### Linux: AppImage

```sh
chmod +x Fission3D-<Version>-x86_64.AppImage
./Fission3D-<Version>-x86_64.AppImage
```

Auf Ubuntu 24.04 braucht ein AppImage `libfuse2t64`: `sudo apt install libfuse2t64`.
Ohne FUSE geht es auch so: `./Fission3D-….AppImage --appimage-extract-and-run`.

### Linux: Flatpak

```sh
flatpak install --user Fission3D.flatpak
flatpak run io.github.sloppyhenry.Fission3D
```

Dieses Flatpak ist für die lokale Nutzung gedacht (siehe ADR-0006); in Flathub ist es noch nicht.

## Pakete selbst bauen

```sh
scripts/setup              # einmalig: Voraussetzungen
scripts/package            # baut optimiert und paketiert: .dmg (Mac) bzw. AppImage (Linux)
scripts/package --flatpak  # Linux: zusätzlich ein Flatpak (lädt einmalig die Flatpak-Laufzeit, ca. 1 GB)
```

Das Ergebnis liegt in `build/package/`. Auf Linux braucht `scripts/package` zusätzlich `patchelf`
(`sudo apt install patchelf`), für das Flatpak `flatpak` und `flatpak-builder`.

Was dabei passiert:

- **macOS:** `macdeployqt` kopiert Qt und alle Bibliotheken in die App, ein Aufräumschritt macht alle Verweise
  relativ und **bricht ab, wenn noch etwas nach außen zeigt** (z. B. nach Homebrew). Dann Ad-hoc-Signatur
  (`codesign -s -`) und das Disk-Image mit Hintergrund und Programme-Verknüpfung.
- **Linux:** `cmake --install` legt die Dateien in ein AppDir, `linuxdeploy` bündelt Qt und Bibliotheken
  (X11, Wayland und Offscreen), daraus entsteht das AppImage. Das Flatpak verpackt dasselbe AppDir.

Wichtig: Ein AppImage oder Flatpak läuft nur auf Systemen mit mindestens dem glibc des Rechners, auf dem es gebaut
wurde. Verteilte Pakete kommen deshalb immer aus der CI (Ubuntu 24.04). Ein Paket von einem neueren Rechner
(z. B. Ubuntu 26.04) startet dort, aber nicht auf Ubuntu 24.04.

## Signieren und Notarisieren (CI)

Der CI-Job **Package** läuft bei jedem Lauf. Ohne Secrets signiert er **ad hoc** und überspringt die Notarisierung
(Ausgabe: „Notarisierung übersprungen“). Mit einem **bezahlten** Apple-Developer-Konto reichen sechs Secrets
(GitHub → Settings → Secrets and variables → Actions → New repository secret), dann signiert die CI mit der
Developer ID (Hardened Runtime, `packaging/macos/entitlements.plist`) und notarisiert das Disk-Image samt Staple:

| Secret | Inhalt |
|---|---|
| `MACOS_CERTIFICATE` | Zertifikat „Developer ID Application“ als `.p12`, base64-kodiert (`base64 -i zertifikat.p12 \| pbcopy`) |
| `MACOS_CERTIFICATE_PASSWORD` | Passwort der `.p12`-Datei |
| `MACOS_SIGNING_IDENTITY` | Name des Zertifikats, z. B. `Developer ID Application: Name (TEAMID)` |
| `NOTARY_APPLE_ID` | Apple-ID des Kontos |
| `NOTARY_TEAM_ID` | Team-ID (10 Zeichen) |
| `NOTARY_PASSWORD` | App-spezifisches Passwort (appleid.apple.com → Anmeldung und Sicherheit) |

Regeln: Zugangsdaten stehen **nie** im Repository, in Doku, Commits oder Protokollen. Pull Requests aus fremden
Forks bekommen Secrets nicht. Das Zertifikat wird in der CI in einen Schlüsselbund auf dem Runner importiert, der nach
dem Lauf verschwindet.

## Kennungen

| | |
|---|---|
| Anwendungs-ID (Bundle-ID, Flatpak, `.desktop`, Wayland) | `io.github.sloppyhenry.Fission3D` |
| Projektdateien | `.fission`, MIME-Typ `application/x-fission3d`, macOS-UTI `io.github.sloppyhenry.Fission3D.project` |

Die Dateizuordnung ist eingetragen; das Öffnen einer Datei per Doppelklick folgt, sobald das Dateiformat im
Programm existiert (Phase 1).

## Bekannte Einschränkungen

- Das App-Icon ist ein Platzhalter (`scripts/tools/make_icons.py`).
- Ohne Notarisierung braucht jede Installation auf dem Mac den Gatekeeper-Umweg.
- Das Flatpak ist nicht Flathub-tauglich (Quellbau folgt vor AP-6.1).
- Das Paket enthält noch alle Qt-Quick-Module, die `macdeployqt`/`linuxdeploy` finden; eine Verkleinerung steht aus.
