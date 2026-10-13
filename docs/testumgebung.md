# Testumgebung

Stand: 2026-10-13 (AP-0.4). Zugangsdaten, Hostnamen und Schlüssel stehen bewusst **nicht** hier.
Es gibt drei Linux-Testrechner. **Testrechner C** (Ubuntu 26.04 mit RTX 3060) ist seit AP-0.4 der
Standard-Testrechner, vor allem wegen der echten GPU für die Viewport-Messungen. B bleibt für die
Ubuntu-24.04-Container-Prüfung. A war zuletzt nicht erreichbar.

## macOS (Entwicklungsrechner)

| | |
|---|---|
| System | macOS 27.0, Apple Silicon (arm64) |
| Compiler | AppleClang 21 (Xcode 27) |
| Qt | 6.11.2 (Homebrew: `qtbase`, `qtdeclarative`, `qtshadertools`, `qtsvg`) |
| Build-Werkzeuge | CMake 4.4, Ninja 1.13, pkg-config (Homebrew); Bibliotheken über vcpkg |
| Signierung | nur kostenloses Apple-Developer-Konto: ad hoc bzw. lokal, keine Notarisierung (siehe AP-0.7) |

## Linux-Testrechner A (per SSH, schnell)

| | |
|---|---|
| Distribution | Ubuntu 26.04.1 LTS, Kernel 7.0, x86_64 |
| Hardware | 32 Kerne, 31 GB RAM |
| Grafik | NVIDIA GeForce RTX 3080 (10 GB), proprietärer Treiber 595.91; zusätzlich Mesa 26.0 (llvmpipe) |
| Vulkan | 1.4 (NVIDIA-GPU und llvmpipe sichtbar) |
| Sitzungen | KDE Plasma, Wayland **und** X11 installiert |
| Compiler | GCC 15.2 |
| Build-Werkzeuge | CMake 4.2.3, Ninja 1.13, clang-format 21, clang-tidy 21, ccache 4.12 (apt) |
| Qt | 6.10.2 (apt: `qt6-base-dev`, `qt6-declarative-dev`, `qt6-shadertools-dev`, `qt6-svg-dev`, QML-Module) |
| Catch2 | 3.7.1 (apt) |
| Rechte | SSH-Zugang per Schlüssel, `sudo` ohne Passwort |

Zusätzlich liegt dort eine **CMake 3.28.6** (offizielles Binärpaket, nur im Heimatverzeichnis unter
`tools/`), um die Mindestversion des Projekts zu prüfen.

### Wie getestet wird

- Bauen und Tests laufen headless per SSH: `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`.
  Der Fenstertest (`app_opens_window`) speichert einen Screenshot des ersten Frames als
  `build/<preset>/smoke-test.png`.
- Dieser Test prüft **nicht** die GPU. Tests mit echter Grafik (Vulkan/OpenGL, Wayland/X11) macht der
  Product Owner am Bildschirm oder sie folgen mit AP-0.4.
- Vorgehen zum Übertragen: Projekt per `rsync` (ohne `build/` und `.git/`) in ein eigenes Verzeichnis
  im Heimatverzeichnis kopieren, dort `scripts/setup --yes` und `scripts/test` ausführen.

### Was dort verändert wurde

- Per `scripts/setup` installierte apt-Pakete (Qt 6 Entwicklungspakete, Ninja, Catch2, clang-format,
  clang-tidy, ccache, Xvfb sowie deren Abhängigkeiten).
- Projektkopie unter `~/fission3d-ap01`, Build-Verzeichnisse darin.
- CMake 3.28.6 unter `~/tools/cmake-3.28.6`.

Nicht verändert: Treiber, Kernel, Systemdienste, Firewall, Benutzer.

### Offene Punkte

- Die Plasma-Version wurde nicht ermittelt (`plasmashell --version` benötigt eine grafische Sitzung).
- Ubuntu 24.04 (Zielplattform laut AP-0.2/AP-0.7) und Fedora stehen noch nicht als Testrechner zur
  Verfügung. Ubuntu 24.04 liefert über apt nur Qt 6.4; dort braucht es andere Qt-Quellen (vcpkg, AP-0.2).

## Linux-Testrechner B: kleiner Server (per SSH, Docker)

| | |
|---|---|
| Distribution | Debian 13 (trixie), Kernel 6.12, x86_64 |
| Hardware | 4 Kerne, 7,6 GB RAM, 8 GB Swap, Intel HD Graphics 620 |
| Sitzungen | Xfce (Wayland und X11 installiert); es läuft keine Desktop-Sitzung |
| Compiler | GCC 14.2 |
| Build-Werkzeuge | CMake 3.31, Ninja, clang-format/-tidy, ccache (apt, über `scripts/setup`) |
| Qt | 6.8.2 (apt; System-Qt reicht, vcpkg baut kein Qt) |
| Docker | 29.5, Benutzer ist in der Gruppe `docker`; `sudo` ohne Passwort |

**Der Rechner betreibt dauerhaft andere Dienste** (Docker-Container). Deshalb gilt dort:

- Builds mit `FISSION_JOBS=2` und `nice -n 19` starten (höchstens 2 parallele Aufgaben).
- Container für Tests mit Limits starten (`--cpus`, `--memory`), siehe
  `scripts/tools/docker-ubuntu-check`.
- Eigene Container und Volumes tragen den Präfix `fission3d-`; fremde Container werden nicht angefasst.

### Zugriff auf die Intel-Grafik (13.10.2026)

Der Benutzer war nicht in der Gruppe `render`, Vulkan sah deshalb nur den Software-Renderer. Mit Freigabe des
Product Owners wurde er in die Gruppe `render` aufgenommen (`usermod -aG render`; wirkt ab der nächsten
SSH-Verbindung, laufende Dienste sind nicht betroffen). Vulkan-Messung ohne Bildschirm:
`QT_QPA_PLATFORM=xcb xvfb-run -a -s "-screen 0 1920x1080x24" viewport_a_bench --backend vulkan …`
(mit `offscreen` lässt sich unter Qt keine Vulkan-Instanz anlegen). Das Gerät steht im Messergebnis
(`"device"`); bei `llvmpipe` wäre die Messung wertlos.

### Ubuntu 24.04 (Zielplattform) per Container

`scripts/tools/docker-ubuntu-check` baut und testet einen frischen Stand in einem `ubuntu:24.04`-Container.
Dort liefert apt nur Qt 6.4, also baut vcpkg Qt selbst. Der Zwischenspeicher liegt im Docker-Volume
`fission3d-ubuntu-vcpkg`, damit spätere Läufe schnell sind.

### Bauzeiten (erster Lauf, ohne Zwischenspeicher)

| Plattform | Umfang | Dauer |
|---|---|---|
| Mac (Apple Silicon, alle Kerne) | alle Bibliotheken ohne Qt (System-Qt) | rund 15 Minuten |

## Linux-Testrechner C: Ubuntu 26.04 mit RTX 3060 (per SSH, geteilt)

| | |
|---|---|
| Distribution | Ubuntu 26.04.1 LTS, Kernel 7.0, x86_64 |
| Hardware | 24 Threads, 15 GB RAM (davon meist nur ca. 5 GB frei), 16 GB Swap |
| Grafik | NVIDIA GeForce RTX 3060 (12 GB), proprietärer Treiber 595.91, Vulkan 1.4 |
| Sitzungen | KDE Plasma (Wayland); beim Anmeldebildschirm, keine Desktop-Sitzung aktiv |
| Compiler | GCC (Ubuntu 26.04), CMake, Ninja, Git, Python 3 vorhanden |
| Werkzeuge | `vulkaninfo`, `glxinfo`, `eglinfo`, `Xvfb`; `sudo` ohne Passwort |

**Der Rechner betreibt dauerhaft andere Dienste** (Docker-Container und ein lokaler Sprachmodell-Server, der
rund 9,7 GB der 12 GB Grafikspeicher belegt). Deshalb gilt:

- Builds mit `FISSION_JOBS=4` und `nice -n 19` starten, auf den Arbeitsspeicher achten (nur ca. 5 GB frei).
- **Grafikspeicher:** nur etwa 2,4 GB sind frei. Die Viewport-Prototypen müssen mit dieser Grenze
  auskommen; Messwerte können schwanken, wenn der Sprachmodell-Server gerade rechnet.
- Fremde Container und Prozesse werden nicht angefasst; eigene Container und Volumes tragen den Präfix `fission3d-`.
- Messungen ohne Bildschirm per SSH (Offscreen); ein Fenstertest am Bildschirm macht der Product Owner.

### Grafik-Messungen ohne Bildschirm (Spike AP-0.4)

- **Vulkan ohne Fenster:** `QT_QPA_PLATFORM=xcb`, `DISPLAY` auf ein virtuelles Display (`Xvfb`), dazu
  `QT_XCB_GL_INTEGRATION=none`. Die GPU rechnet über Vulkan, das virtuelle Display wird nur für die
  Qt-Plattform gebraucht. Die Plattform `offscreen` kann keine Vulkan-Instanz erzeugen.
- **OpenGL (OpenCASCADE-Anzeige):** `Xvfb` hat kein NVIDIA-GLX; dort läuft nur Software-OpenGL (Mesa). Echte
  GPU-Werte für OpenGL brauchen eine Desktop-Sitzung oder einen X-Server mit NVIDIA-Treiber (eine
  Systemänderung, die vorher abzustimmen ist).
- **Mac:** Messungen ohne Fenster mit `QT_QPA_PLATFORM=offscreen` (Metal) laufen auch bei gesperrtem Bildschirm;
  Fenster werden bei ausgeschaltetem Display von macOS nicht gezeichnet.
