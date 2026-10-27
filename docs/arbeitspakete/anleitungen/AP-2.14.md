# AP-2.14 Meldungskatalog und Fehler-UX

**Größe:** M · **Phasendatei:** AP-2.14 · Baut auf dem Grundgerüst aus AP-1.12 auf.

## Ziel
Jeder Fehler, jede Warnung und jedes „braucht Hilfe“ hat einen Eintrag im Katalog: Titel, Erklärung mit Zahlen, Ort (Rezept-Badge, Karte
an der Stelle in 3D, rote Geistergeometrie) und Ein-Klick-Lösungen. Kein roher Kernel-Text erreicht die Oberfläche. Eine CI-Prüfung
verhindert neue Fehlercodes ohne Text.

## Voraussetzungen
AP-1.12 (Grundgerüst `MessageCatalog.js`), AP-0.5 (Liste der Problemfälle). Sinnvoll nach AP-2.10 (Gründe für „braucht Hilfe“) und
parallel zu 2.4/2.5/2.6/2.7 (deren Fehlercodes).

## Vorher lesen
- `docs/02-ux-konzept.md` §9 (Regeln und drei Beispiele – genau dieser Ton), §12 (Glossar)
- `docs/spikes/kernel-robustheit.md` „Bekannte Problemfälle“ (Tabelle = Pflichtumfang)
- `src/core/include/fission/core/feature_type.h` (`FeatureIssueCode`, `FeatureIssue`), `src/geometry/gal/include/fission/geo/error.h`
- `apps/fission359/qml/MessageCatalog.js` (aus AP-1.12)

## Ausgangslage im Code
- Fehler sind Codes + `subject` + `detail` (nur Protokoll). AP-1.12 hat sie im Katalog mit einfachen Texten und Aktionen „Wert ändern“,
  „Schritt ausblenden“.
- Es fehlen: Zahlen in Meldungen (z. B. größtmöglicher Radius), Ort in 3D, Geistergeometrie für gescheiterte Ergebnisse, Fallback.

## Festlegungen
- Drei Fragen je Meldung: Was ist passiert? Wo? Was kann ich tun? (UX §9). Keine modalen Dialoge.
- Das Modell zeigt den letzten gültigen Zustand; das gescheiterte Ergebnis erscheint, **wenn vorhanden**, als rote Geistergeometrie.

## Haltepunkte
1. **Fallback für Unbekanntes:** Empfehlung: „Das hat nicht geklappt. Das ist ein Fehler im Programm, nicht deiner.“ + [Details für einen
   Fehlerbericht kopieren] (kopiert Code, `detail`, Version, Schritt-Typ in die Zwischenablage; nie automatisch senden).
2. **Ton**: drei Beispielmeldungen aus diesem AP vorab zeigen und bestätigen lassen.

## Schritte
1. **Daten statt Text in C++:** `FeatureIssue` um `std::map<std::string, double> numbers` erweitern (z. B. `maxRadius`, `minWall`,
   `nozzle`), damit Meldungen Zahlen nennen. Alle Stellen, die Issues erzeugen, prüfen. Feature-Typen setzen sie (z. B. AP-2.5 setzt
   `maxRadius`).
2. **Ort:** `FeatureIssue` um optionale Stelle `std::optional<geo::Point3d> where` und `std::vector<std::string> elements` (betroffene
   Flächen/Kanten) ergänzen. Karte in 3D an `where` (oder am Körper), betroffene Elemente rot hervorgehoben.
3. **Geist:** Liefert der Kernel ein ungültiges Ergebnis, darf es für die Anzeige (nie für die Weiterberechnung!) als Netz mitgegeben
   werden (`FeatureState::failedPreview` als `TriangleMesh`). Rot halbtransparent zeichnen. Nicht für alles möglich; dann nur Karte.
4. **Katalog vervollständigen:** je Code (alle `FeatureIssueCode`, alle `GeometryErrorCode`, Datei-Codes aus AP-1.10, `HelpReason` aus
   AP-2.10): Titel, Erklärung (mit `%1`-Platzhaltern für Zahlen in mm mit Komma auf Deutsch), Aktionen. Pflichtumfang: alle Zeilen aus
   „Bekannte Problemfälle“ (AP-0.5): Radius zu groß → „Größtmöglichen Wert verwenden (3,1 mm)“; fast bündig → „Bündig machen“; Spalt →
   „Verbinden“; Aushöhlen gescheitert → „Anderes Verfahren versuchen“; Kante-zu-Kante → Hinweis.
5. **Aktionen** sind benannte Funktionen (`useMaximum`, `makeFlush`, `editValue`, `suppressStep`, `chooseAgain`, `copyDetails`), umgesetzt in
   `DocumentModel`/Controllern, jede ein Undo-Schritt, wo sie etwas ändert.
6. **CI-Prüfung** `scripts/tools/check_messages.py`: liest alle Enum-Werte aus `feature_type.h`, `error.h`, `resolve.h`, `fission_file.h` und
   prüft, dass `MessageCatalog.js` für jeden einen Eintrag hat; in `tests/CMakeLists.txt` als Test (Akzeptanzkriterium 1).
7. **Kein roher Text:** Test, der alle `detail`-Strings in einer Fehlerprovokation sammelt und prüft, dass keiner in den sichtbaren Texten
   erscheint (QML-Text-Eigenschaften im Fenster-Test durchsuchen oder `MessageCatalog` nutzt `detail` nirgends außer in `copyDetails`;
   Letzteres per grep-Test erzwingen).

## Tests
- `check_messages` grün; ein absichtlich ergänzter Code ohne Text lässt ihn rot werden (einmal ausprobieren, nicht committen).
- UI-Test: Radius zu groß (sobald AP-2.5 da ist) → Meldung mit Zahl, Aktion setzt den Wert, ein Undo-Schritt.

## Abnahme
So probierst du es aus: In „Tiefe“ 0 tippen → Karte am Körper „Tiefe muss größer als null sein“ mit [Wert ändern]. (Mit AP-2.5:) Kante mit 50 mm
abrunden → „Abrunden mit 50 mm geht hier nicht … [Größtmöglichen Wert verwenden (9,9 mm)]“.

## Nicht tun
- Keine Meldung, die nur „Fehler“ sagt. Keine Kernel-Begriffe („BRep“, „Shell“, „Wire“) in Texten.
