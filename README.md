# Fission3D

> A native 3D construction program for macOS and Linux: as easy as clay, as precise as a caliper.
> Open source under GPL-3.0-or-later. (English summary; the project documentation is German.)

Fission3D ist ein installierbares 3D-Konstruktionsprogramm für **macOS (Apple Silicon)** und **Linux**.
Es soll sich so leicht anfühlen wie Tinkercad und so genau sein wie Fusion. Damit baust du
funktionale Teile (Halterungen, Ersatzteile, Boxen mit passenden Deckeln) genauso wie
verspielte Deko, vor allem für den FDM-3D-Druck. Keine Browser-App, kein Konto, keine Cloud-Pflicht.

**Stand:** Phase 0 (Fundament). Das Programm öffnet bisher nur ein leeres Fenster.
Den Fahrplan findest du in [docs/04-roadmap.md](docs/04-roadmap.md).

## Ausprobieren (ohne Programmierkenntnisse)

1. **Voraussetzungen:** ein Mac mit Apple Silicon und macOS 14 oder neuer plus **Xcode**
   (aus dem App Store), oder ein Linux-Rechner mit Ubuntu/Debian (x86_64).
2. **Nur Mac:** Installiere [Homebrew](https://brew.sh) (einmalig, der Befehl steht auf der Webseite).
3. **Projekt holen:** `git clone <Repository-Adresse> Fission3D` und dann `cd Fission3D`.
4. **Voraussetzungen einrichten:** `scripts/setup`
   Das Skript sagt dir genau, was fehlt, und installiert es nach Rückfrage.
5. **Bauen und starten:** `scripts/run`
   Beim ersten Mal dauert das einige Minuten. Danach öffnet sich das Fenster.
6. **Alles prüfen:** `scripts/test`
   Am Ende steht, ob alles in Ordnung ist, in einfachen Worten.

Etwas klappt nicht? `scripts/setup --check` zeigt, was fehlt. Bauprotokolle liegen in `build/*.log`.

## Für Entwickler:innen

```sh
cmake --preset macos-debug            # oder: linux-debug
cmake --build --preset macos-debug
ctest --preset macos-debug
```

Presets: `debug`, `release` und `asan` (Speicherfehler-Erkennung), je mit Präfix `macos-` bzw. `linux-`.
Ändern kannst du das Programm nach den Regeln aus [CONTRIBUTING.md](CONTRIBUTING.md).

### Aufbau

```
apps/fission3d/   Programmstart und QML-Einstieg
src/core/         Dokument, Features, Commands (kein Qt!)
src/geometry/     gal/ (Schnittstelle), occt/, mesh/, sketch/ (Kernel-Anbindung)
src/features/ io/ analysis/ services/ render/ interaction/ ui/
tests/            unit/, layering/ (Schichtregel-Tests)
docs/             Vision, UX-Konzept, Architektur, Roadmap, Arbeitspakete, ADRs
```

Jede Schicht ist ein eigenes CMake-Ziel. Abhängigkeiten zeigen nur nach unten; wer gegen die
Schichtregel verstößt, bekommt schon beim Bauen einen Fehler (Details in
[docs/03-architektur.md](docs/03-architektur.md)).

## Dokumentation

Einstieg: [docs/README.md](docs/README.md) · Testumgebung: [docs/testumgebung.md](docs/testumgebung.md) ·
Entscheidungen: [docs/adr/](docs/adr/)

## Lizenz

[GPL-3.0-or-later](LICENSE). Verwendete Bibliotheken: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Mitmachen? Bitte lies [CONTRIBUTING.md](CONTRIBUTING.md) und den [Verhaltenskodex](CODE_OF_CONDUCT.md).
