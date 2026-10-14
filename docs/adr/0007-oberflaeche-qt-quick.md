# ADR-0007: Oberfläche – Qt Quick, Theme-Tokens in C++, Übersetzung mit Qt Linguist

- **Status:** angenommen (Übersetzung und Menüs: Product Owner, 2026-10-14; übrige Punkte Annahmen des Agenten)
- **Datum:** 2026-10-14
- **Arbeitspaket:** AP-0.8

## Kontext
AP-0.8 setzt das verbindliche Design ([08-gestaltung.md](../08-gestaltung.md), Mockup) im Programm um. Das Design
verlangt zentrale Farb-Tokens (hell/dunkel, ableitbare Akzentfarbe), Texte nur aus dem Glossar in Deutsch und Englisch,
eine echte Menüleiste auf dem Mac und einen automatischen Kontrasttest über alle Token-Paare.

## Entscheidung
1. **Theme als C++-Singleton** (`fission::ui::Theme`, Tokens in `theme_tokens.h`), nicht als QML-Datei. QML liest nur
   `Theme.panel`, `Theme.accentText` usw. Dadurch lassen sich Tokens, Akzent-Ableitung und Kontraste mit Catch2 prüfen
   (`tests/unit/ui/test_theme_tokens.cpp`), ohne Oberfläche.
2. **Texte: `qsTr` mit englischen Quelltexten**, Übersetzungen als Qt-Linguist-Dateien (`apps/fission3d/i18n/*.ts`,
   Deutsch und Englisch). Englisch als Quelltext (statt Deutsch) ist Annahme des Agenten: Quelltext und Kommentare sind
   ohnehin englisch (Master-Prompt), und neue Mitwirkende finden den Text so im Code wieder. Der Test
   `translations_complete` verlangt, dass jede Übersetzung fertig ist und kein Text fest eingebaut steht.
   Die Sprache lässt sich zur Laufzeit wechseln (`Language`, `engine.retranslate()`).
3. **Menüs:** ein Datenmodell in `Main.qml`. Auf macOS erzeugt `Qt.labs.platform` daraus die System-Menüleiste (mit den
   Rollen Über/Einstellungen/Beenden im App-Menü), auf Linux zeichnet `MenuStrip` dieselben Einträge im Fenster nach dem
   Mockup. Nicht vorhandene Befehle stehen ausgegraut da (Platzhalter mit echtem Aussehen).
4. **Steuerelemente:** Qt Quick Controls im Stil `Basic`, vollständig selbst gestaltet; kein Plattform-Stil. Schrift:
   IBM Plex Sans/Mono werden mitgeliefert (SIL OFL 1.1, in `THIRD_PARTY_NOTICES.md`).
5. **3D-Ansicht:** `SceneRenderer` (Qt RHI, ohne Fenster) liegt in der Schicht `render`; das Qt-Quick-Element
   `Viewport3D` in `ui`. Beide dürfen dafür `Qt6::GuiPrivate` nutzen (Schichtregeln angepasst, Version über vcpkg
   festgenagelt, siehe ADR-0002).
6. **Fensterprüfung:** unter Linux läuft das echte Fenster in der CI in Xvfb mit Software-OpenGL, unter macOS mit dem Metal
   des Rechners; `fission_screenshot_check` vergleicht Farben der Flächen mit den Tokens und Blöcke der Leisten mit einem
   Bild des Mockups.

## Konsequenzen
- **Einfacher:** neue Bausteine nutzen nur Tokens; Akzentfarbe und hell/dunkel kosten nichts mehr.
- **Schwerer:** Schriftgrößen sind in Qt Quick ganze Pixel (12,5 und 14,5 px des Mockups werden 13 und 15 px).
  `lupdate` mit QML-Unterstützung braucht ein Qt mit `qttools[qml]` (Homebrew und apt haben es; das vcpkg-Qt der CI nicht,
  dort läuft nur `lrelease`).
- **Folge-Arbeiten:** Einstellungen (Design, Akzent, Sprache), UI-Skalierung 80–200 %, Befehlssuche,
  Tooltip-Karten mit Animation, Bildschirmleser-Beschriftungen prüfen.
- **Revisionskriterium:** Wird `Qt.labs.platform` entfernt oder bringt Qt native Menüs im Standard-Controls-Modul, wird
  die System-Menüleiste umgestellt.
