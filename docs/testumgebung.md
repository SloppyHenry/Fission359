# Testumgebung

Stand: 2026-10-11 (AP-0.1). Zugangsdaten, Hostnamen und Schlüssel stehen bewusst **nicht** hier.

## macOS (Entwicklungsrechner)

| | |
|---|---|
| System | macOS 27.0, Apple Silicon (arm64) |
| Compiler | AppleClang 21 (Xcode 27) |
| Qt | 6.11.2 (Homebrew: `qtbase`, `qtdeclarative`, `qtshadertools`, `qtsvg`) |
| Build-Werkzeuge | CMake 4.4, Ninja 1.13, Catch2 3.16 (Homebrew) |
| Signierung | nur kostenloses Apple-Developer-Konto: ad hoc bzw. lokal, keine Notarisierung (siehe AP-0.7) |

## Linux-Testrechner (per SSH)

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
