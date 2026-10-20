# AP-3.5 „Im Slicer öffnen“

**Größe:** S · **Phasendatei:** AP-3.5

## Ziel
Ein Klick exportiert das Modell als 3MF und öffnet es im Slicer: Bambu Studio, OrcaSlicer, PrusaSlicer oder Cura, auf macOS und Linux (auch
Flatpak und AppImage). Bevorzugter Slicer steht im Druckerprofil. Fehlt der Slicer: freundlicher Hinweis mit Download-Link.

## Voraussetzungen
AP-3.4 (3MF mit Farben), AP-3.1 (bevorzugter Slicer).

## Vorher lesen
`docs/03-architektur.md` §2 (App-Services: Slicer-Erkennung in `src/services`), §11; Handbuch §3.

## Haltepunkte
1. **Download-Links:** nur offizielle Seiten der Hersteller (Bambu Lab, OrcaSlicer GitHub, Prusa, UltiMaker). Bestätigen, dass Links in der App ok sind.

## Schritte
1. `src/services/slicers.*`: Erkennung je Plattform – macOS: `/Applications` und `~/Applications` nach Bundle-Namen bzw. Bundle-IDs
   (`com.bambulab.bambu-studio`, `com.softfever3d.orca-slicer`, `com.prusa3d.slic3r`, `nl.ultimaker.cura` – **prüfen**, echte IDs auf dem Mac mit
   `mdls -name kMDItemCFBundleIdentifier` ermitteln), Start mit `open -b <id> <datei>`; Linux: Flatpak (`flatpak info com.bambulab.BambuStudio`
   usw., IDs prüfen), AppImages in `~/Applications`, `~/.local/bin`, `/opt`, Programme im `PATH` (`bambu-studio`, `orca-slicer`, `prusa-slicer`,
   `cura`); Start mit `QProcess::startDetached`.
2. Erkennung im Hintergrund beim Start (schnell, aber Dateisystem), Ergebnis gemerkt.
3. Export in eine temporäre Datei (`QTemporaryDir`, die nach dem Start des Slicers **nicht** sofort gelöscht wird – sonst findet der Slicer sie
   nicht; aufräumen beim nächsten Programmstart).
4. Menü Datei ▸ „Im Slicer öffnen“ (⌘P?) und Knopf auf der Export-Seite; Untermenü bei mehreren Slicern.
5. Fehlt der Slicer: Karte „Bambu Studio ist nicht installiert. [Herunterladen] [Datei speichern statt öffnen]“.

## Tests
Erkennung mit gefälschten Verzeichnissen (Testordner mit leeren `.app`-Bündeln bzw. Skripten im `PATH`), Startbefehl wird korrekt gebaut (ohne
wirklich zu starten: Starter als austauschbare Funktion). Manuell: jeder Slicer auf Mac und Linux (Tabelle im PR, Akzeptanzkriterium).

## Abnahme
So probierst du es aus: Schlüsselanhänger, Datei ▸ Im Slicer öffnen → Bambu Studio öffnet ihn mit zwei Farben.

## Nicht tun
Nicht an Drucker senden (Backlog B-10). Keine Slicer-Profile verändern.
