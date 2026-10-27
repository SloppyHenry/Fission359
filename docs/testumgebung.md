# Testumgebung

Zugangsdaten, Hostnamen und Schlüssel stehen bewusst **nicht** hier.

## Rechner

| | System | Hardware / Grafik | Hinweise |
|---|---|---|---|
| **Mac** (Entwicklung) | macOS 27, arm64, AppleClang 21, Qt aus Homebrew | Apple M1, Metal | nur kostenloses Apple-Konto: ad hoc signieren |
| **Linux A** | Ubuntu 26.04, GCC 15.2, Qt 6.10 (apt) | 32 Kerne, 31 GB, RTX 3080; KDE (Wayland + X11) | kein vcpkg-Stand: Linux-Builds mit allen Bibliotheken laufen über die CI |
| **Linux B** | Debian 13, GCC 14.2, Qt 6.8 (apt), Docker | 4 Kerne, 7,6 GB, Intel HD 620 | **geteilt:** `FISSION_JOBS=2 nice -n 19`; kein `rsync` (tar über SSH) |
| **Linux C** | Ubuntu 26.04, Docker, Flatpak | 24 Threads, ~5 GB RAM frei, RTX 3060 (nur ~2,4 GB frei) | **geteilt** (Sprachmodell, Container): `FISSION_JOBS=4 nice -n 19` |

Auf allen geteilten Rechnern: fremde Container und Prozesse nicht anfassen; eigene Container, Volumes und Kopien tragen den
Präfix `fission359-`. Treiber, Kernel, Dienste, Firewall und Benutzer bleiben unverändert (Ausnahme mit Freigabe: Benutzer auf B
in der Gruppe `render` für GPU-Zugriff).

## Headless prüfen

- **Bauen und Tests:** Projekt per `rsync` (ohne `build/`, `.git/`) in ein eigenes Verzeichnis kopieren, `scripts/setup --yes`,
  `scripts/test`. Fenster-Tests mit `QT_QPA_PLATFORM=offscreen`.
- **Vulkan ohne Bildschirm:** `xvfb-run -a` mit `QT_QPA_PLATFORM=xcb` und `QT_XCB_GL_INTEGRATION=none` (`offscreen` kann keine
  Vulkan-Instanz anlegen). Steht im Ergebnis `llvmpipe`, ist die Messung wertlos.
- **OpenGL unter Xvfb** ist nur Software-Mesa; Zeiten sind dort nicht aussagekräftig.
- **Virtuelles Wayland:** `kwin_wayland --virtual --socket <name> --no-lockscreen`, dann `WAYLAND_DISPLAY=<name> QT_QPA_PLATFORM=wayland`.
- **Bedienung mit echtem Zeiger (B):** `Xvfb :97 -screen 0 1600x1000x24 &`, dann `DISPLAY=:97 QT_QPA_PLATFORM=xcb
  QSG_RHI_BACKEND=opengl ./squashfs-root/AppRun --smoke-test bild.png --simulate "…"` und dazu `xdotool mousemove x y`.
- **Ubuntu 24.04:** `scripts/tools/docker-ubuntu-check` (Container, vcpkg baut Qt; Cache im Volume `fission359-ubuntu-vcpkg`).
- **Pakete:** Auf Ubuntu 26.04 gebaute AppImages/Flatpaks laufen nur auf gleich neuen Systemen. Verteilbare Pakete immer aus der CI
  testen, z. B. in `ubuntu:24.04`- und `fedora:44`-Containern.
- **Mac:** Messungen mit `QT_QPA_PLATFORM=offscreen` (Metal) laufen auch bei gesperrtem Bildschirm; echte Fenster nicht.

## Virtueller Mac für Installationstests

[tart](https://github.com/cirruslabs/tart) 2.41.0 in `~/tools/tart-2.41.0` (die Homebrew-Formel ist kaputt), Maschine
`fission359-mac14` (macOS 14.8.7, nur Kommandozeilen-Werkzeuge, rund 27 GB). Start mit Fenster:
`~/tools/tart-2.41.0/tart.app/Contents/MacOS/tart run fission359-mac14`; ohne Fenster `--no-graphics` und `tart ip fission359-mac14`.
