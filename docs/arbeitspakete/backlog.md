# Backlog (nach 1.0)

Ideen und größere Themen, die bewusst nicht in 1.0 sind. Die Reihenfolge ist eine grobe Priorität und wird nach dem Beta-Feedback neu bewertet.

| # | Thema | Kurzbeschreibung | Größe |
|---|---|---|---|
| B-1 | **Baugruppen und Gelenke** | Mehrere Teile zueinander positionieren (Gelenk: Scharnier, Schiene, Kugel), Bewegung ausprobieren, **Kollisionsprüfung** (passt der Deckel beim Öffnen?). Print-in-Place-Prüfung | XL |
| B-2 | **Skript- und Plugin-API** | **Python** (eingebettet, z. B. über pybind11); Zugriff auf Dokument, Features, eigene Generatoren; Plugin-Verzeichnis. Python ist gesetzt, weil es für Maker die zugänglichste Sprache ist und der Product Owner sie selbst nutzen kann | XL |
| B-3 | **KI-Assistent** (optional, lokal oder per API) | „Beschreib, was du bauen willst“ → Vorschlag als Rezept (editierbar!); „Erklär mir diesen Fehler“; Maße aus Foto mit Referenzobjekt schätzen | L |
| B-4 | **Technische Zeichnungen** | Ansichten, Maße, Schnitte → PDF/DXF (z. B. für Werkstatt oder Fräsdienst) | L |
| B-5 | **Windows-Port** | Qt und OCCT sind plattformübergreifend; Aufwand vor allem Paketierung, Signatur, Tests | M |
| B-6 | **3D-Maus (SpaceMouse)** | 3Dconnexion SDK (macOS), spacenavd (Linux) | S |
| B-7 | **Blech und Lasercut** | Flache Teile mit Laschen und Abwicklung; Export als DXF/SVG für Lasercutter; Fingerzinken-Boxen | L |
| B-8 | **Freiform (Subdivision)** | Siehe AP-4.10, falls zurückgestellt | XL |
| B-9 | **Community-Bibliothek** | Bausteine, Motive, Baukästen und Figuren teilen (Website, kein Konto für Download) | L |
| B-10 | **Druckerintegration** | Direkt an Drucker senden (Bambu LAN, OctoPrint, Moonraker/Klipper, PrusaLink), über Slicer-CLI | M |
| B-11 | **Messen per Foto/Scan** | Konturen aus Fotos (mit Referenz-Lineal) für Ersatzteile; Scan-Netze als Referenz einblenden | L |
| B-12 | **Linux arm64** | z. B. für Raspberry Pi 5 / Asahi Linux | S |
| B-13 | **iPad-Version** | Qt unterstützt iOS; Bedienkonzept für Stift und Touch (Shapr3D-artig) | XL |
| B-14 | **Kernel-Grenzen** | Falls OCCT an Grenzen stößt: gezielt in OCCT selbst verbessern (Beiträge upstream) oder einzelne Operationen über Manifold/SDF lösen. Kommerzielle Kerne (Parasolid) sind mit der GPL nicht vereinbar und scheiden aus | XL |
| B-15 | **Selbst bauen auf älteren Systemen** | Wer ein Linux ohne passendes fertiges Paket hat (z. B. Ubuntu 22.04), kann das Programm selbst bauen. Dafür ein **dynamisches Installationsskript**, das die Distribution erkennt, die nötigen Pakete bzw. Qt/vcpkg beschafft, baut und installiert (Menüeintrag, Dateizuordnung). Baut auf `scripts/setup` und AP-0.7 auf | M |
| B-16 | **Referenzbilder als Hintergrund** | 2–3 Bilder (PNG/JPG) in die Szene laden und als Vorlage zum Modellieren benutzen, z. B. Vorder-, Seiten- und Draufsicht eines Ersatzteils oder eine Zeichnung. Bild lässt sich auf eine Ebene legen, verschieben, drehen, **über ein bekanntes Maß auf Originalgröße bringen** („diese Strecke ist 42 mm“), in der Deckkraft ändern, ein- und ausblenden und sperren. Bilder gehören **nicht** zur Geometrie: kein Export, kein Druck, nie Teil von Prüfungen. Liegen als Asset in der `.fission`-Datei, Änderungen sind Undo/Redo-fähig | M |
| B-17 | **Nachzeichnen (Tracing)** | Aus einem Referenzbild eine **Skizze** machen: Kontur per Klick als Linien/Kurven übernehmen oder automatisch erkennen (Vektorisieren, Schwelle und Glättung einstellbar), danach wie jede andere Skizze mit Maßen und Regeln bearbeitbar. Wichtig für technische Teile, bei denen man ein Foto oder eine Zeichnung als Vorlage hat. Setzt B-16 (Maßstab über bekanntes Maß) voraus; die Kontur-Erkennung teilt sich Bausteine mit B-11 | L |
| B-18 | **MCP-Server (KI-Anbindung)** | Fission3D als lokaler MCP-Server, damit KI-Assistenten (z. B. Claude) Modelle lesen, ändern, prüfen und exportieren können. Nur auf Wunsch, ohne Cloud und ohne Konto. Jede Änderung läuft über dieselben Commands wie in der Oberfläche (Undo, Rezept). Skizze unten | L |

---

## B-18 MCP-Server (Skizze)

**Status:** zurückgestellt (nach 1.0) · **Größe:** L · **Abhängigkeiten:** Release 1.0; teilt sich die Automations-Schnittstelle mit B-2 (Skript-API)

**Idee:** Wer Fission3D zusammen mit einem KI-Assistenten nutzt, soll sagen können „Mach die Halterung 5 mm breiter und prüf, ob sie druckbar ist“. Der Assistent arbeitet dabei im selben Rezept wie der Mensch, nicht an einer Nebenwelt.

**Umfang:**
- Eigenes Programm `fission3d-mcp` (MCP über stdio) mit zwei Betriebsarten:
  - **Ohne Fenster:** lädt eine `.fission`-Datei direkt über Core und GAL, ändert, prüft, speichert und exportiert. Läuft auch per SSH auf dem Linux-Testrechner.
  - **Mit laufender App:** verbindet sich über einen lokalen Socket mit dem geöffneten Dokument. Änderungen erscheinen sofort in der 3D-Ansicht, der Mensch kann jederzeit eingreifen oder rückgängig machen.
- **Werkzeuge (Tools):** Dokument beschreiben (Körper, Rezept, Parameter, in Alltagssprache) · Form oder Feature anlegen · Parameter und Maße ändern (Ausdrücke, Einheiten) · Passungen und Löcher · Druckcheck · Export (STL, 3MF, STEP) · Ansicht als Bild rendern.
- **Ressourcen:** aktuelles Dokument, Rezept, Parameterliste, Glossar.
- Texte und Fehler kommen aus dem Meldungskatalog; „braucht Hilfe“-Markierungen werden an die KI weitergegeben, damit sie nichts still falsch macht.

**Regeln (aus den Produktregeln abgeleitet):**
- Alle Änderungen sind Commands: ein KI-Schritt ist ein Undo-Schritt und ein Rezept-Eintrag, im Rezept als „durch KI“ gekennzeichnet (Regeln 3 und 6).
- Geometrische Ergebnisse werden wie immer validiert; ungültige Ergebnisse sind Fehler (Regel 8).
- Lange Berechnungen sind abbrechbar und blockieren die Oberfläche nicht (Regel 9).
- **Standardmäßig aus.** Der Nutzer schaltet die Anbindung in den Einstellungen ein, wählt „nur lesen“ oder „lesen und ändern“ und sieht, wer verbunden ist. Kein Netzwerk-Port, kein Fernzugriff; Dateizugriff nur auf das geöffnete Dokument und den Export-Ordner.

**Akzeptanzkriterien:**
- [ ] Eine KI baut per MCP die Wandhalterung (Ablauf B) und ändert danach ein Maß. Ergebnis ist gültig, jeder KI-Schritt lässt sich einzeln rückgängig machen.
- [ ] Ohne Einschalten in den Einstellungen ist nichts erreichbar; im „nur lesen“-Modus wird jede Änderung abgelehnt.
- [ ] Der Lauf ohne Fenster funktioniert auf Linux ohne Display (automatisierter Test).
- [ ] Jedes Werkzeug hat ein Schema und Tests für Fehlerfälle; kein roher Kernel-Fehler erreicht die KI.
- [ ] Einrichtungsanleitung in Alltagssprache (Claude Desktop und Claude Code), mit drei Beispiel-Aufträgen.

**Offene Fragen:**
- **Eine Schnittstelle für alles:** Empfehlung ist eine gemeinsame Automations-Schnittstelle im Core (Dokument lesen, Commands auslösen), auf der MCP-Server und Python-API (B-2) als dünne Hüllen sitzen. Dann wird nichts doppelt gebaut. Das spricht dafür, B-2 und B-18 zusammen zu planen.
- **MCP-Bibliothek oder eigene Umsetzung:** Das Protokoll ist klein (JSON-RPC). Die Entscheidung braucht einen ADR; Lizenz muss GPL-3-kompatibel sein.
