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
| B-9 | **Community-Bibliothek** | Bausteine und Vorlagen teilen (Website, kein Konto für Download) | L |
| B-10 | **Druckerintegration** | Direkt an Drucker senden (Bambu LAN, OctoPrint, Moonraker/Klipper, PrusaLink), über Slicer-CLI | M |
| B-11 | **Messen per Foto/Scan** | Konturen aus Fotos (mit Referenz-Lineal) für Ersatzteile; Scan-Netze als Referenz einblenden | L |
| B-12 | **Linux arm64** | z. B. für Raspberry Pi 5 / Asahi Linux | S |
| B-13 | **iPad-Version** | Qt unterstützt iOS; Bedienkonzept für Stift und Touch (Shapr3D-artig) | XL |
| B-14 | **Kernel-Grenzen** | Falls OCCT an Grenzen stößt: gezielt in OCCT selbst verbessern (Beiträge upstream) oder einzelne Operationen über Manifold/SDF lösen. Kommerzielle Kerne (Parasolid) sind mit der GPL nicht vereinbar und scheiden aus | XL |
