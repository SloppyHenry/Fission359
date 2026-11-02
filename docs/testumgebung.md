# Testen auf anderen Systemen

Ziele: macOS arm64 und Linux x86_64 (CI: macOS und Ubuntu 24.04). Auf geteilten Rechnern `FISSION_JOBS=2 nice -n 19 scripts/test` und eigene Container mit dem Präfix `fission359-` benennen.

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

[tart](https://github.com/cirruslabs/tart) mit einer macOS-14-Maschine, die nur die Kommandozeilen-Werkzeuge hat (rund 27 GB). Start: `tart run <name>`, ohne Fenster `--no-graphics`, Adresse mit `tart ip <name>`.
