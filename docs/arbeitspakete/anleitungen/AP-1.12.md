# AP-1.12 Hinweis-System, Befehlssuche, Meldungskatalog-Grundgerüst

**Größe:** M, Teile a–d · **Phasendatei:** AP-1.12

## Ziel
- Die **Hinweiszeile** unten sagt in jedem Zustand in einem Satz, was man jetzt tun kann.
- **Tooltip-Karten** (Titel, Satz, Tastenkürzel, Platz für eine kurze Animation) erscheinen nach 0,8 s über jedem Werkzeug.
- **Befehlssuche** ⌘K/Strg+K findet Werkzeuge und Befehle über Alltagswörter und Synonyme, auf Deutsch und Englisch, auch mit
  Tippfehlern: „runde ecken“ → Abrunden, „bohren“ → Bohrung, „hohl“ → Aushöhlen, „namen“ → Text.
- Ein **Meldungskatalog** (Schlüssel → Text mit Platzhaltern → Lösungsaktionen) ersetzt die verstreuten Fehlertexte; AP-2.14 baut ihn aus.

## Voraussetzungen
AP-1.11 erledigt.

## Vorher lesen
- [Handbuch](handbuch.md) §7, §8; `docs/02-ux-konzept.md` §1 (Befehlssuche), §9 (Fehler), §11 (Tooltip-Karten, Synonyme), §12 (Glossar)
- `apps/fission359/qml/ToolCatalog.qml`, `ShapeCatalog.qml` (Synonyme gibt es schon für Werkzeuge und Formen), `MenuStrip.qml`
  (Suchfeld „What do you want to do?“ ist da, ohne Funktion), `StatusBar.qml` (Hinweiszeile)
- Stellen, an denen heute Fehlertexte entstehen: `grep -rn "parameterOutOfRange\|textGlyphsMissing\|fontNotFound" apps/fission359/qml`

## Ausgangslage im Code
- Hinweiszeile: Texte kommen heute aus mehreren QML-Stellen (Werkzeug, Hover, Ablegen, Ziehen). Kein zentraler Ort.
- Tooltip-Karten: gibt es für die Paletten-Werkzeuge (`ToolButton3D.qml`), nicht für Menübefehle und Formen überall gleich.
- Kataloge: `ToolCatalog` (4 Werkzeuge), `ShapeCatalog` (Formen). Menü-Einträge stehen in `Main.qml` (`menus`) ohne Synonyme.
- Fehlertexte für `FeatureIssueCode` stehen direkt in QML.

## Festlegungen
- Regel 10: Jedes Werkzeug hat Hinweis, Tooltip-Karte, Synonyme.
- Texte nur über `qsTr`, Begriffe aus dem Glossar.
- Meldungen: Was ist passiert? Wo? Was kann ich tun (Ein-Klick-Lösung)? Kein roher Kernel-Text (Regel 5).

## Haltepunkte (am Anfang fragen)
1. **Unscharfe Suche:** Empfehlung: eigene kleine Bewertung (Wortanfang > enthalten > Tippfehler mit Levenshtein ≤ 1 je Wort, Umlaute
   normalisiert ä→a, ß→ss), keine neue Bibliothek. Bestätigen.
2. **Wo lebt der Katalog?** Empfehlung: **ein** Befehlskatalog als Datendatei `apps/fission359/qml/CommandCatalog.js` (oder JSON in
   `resources/commands.json`), der Werkzeuge, Formen und Menübefehle zusammenführt; `ToolCatalog`/`ShapeCatalog` werden daraus gespeist.
   Texte bleiben `qsTr` (also JS/QML, nicht JSON, sonst übersetzt lupdate sie nicht). Empfehlung: JS-Datei.

## Teil 1.12a – Befehlskatalog
1. `CommandCatalog.js` (in `FISSION_QML_FILES`): je Befehl `{ id, group, name, tip, hint, shortcut, synonyms: { de: [], en: [] },
   run: "<menu-id oder werkzeug-id>", enabledWhen: "<name einer Bedingung>" }`. Alle vorhandenen Werkzeuge, Formen und Menübefehle.
2. Ein QML-Test oder Skript (`scripts/tools/check_commands.py`) prüft: jeder Eintrag hat `tip`, `hint`, mindestens 3 Synonyme je Sprache
   (Akzeptanzkriterium 2). In `tests/CMakeLists.txt` als Test anmelden.

## Teil 1.12b – Befehlssuche
1. Klick auf das Suchfeld oder ⌘K öffnet ein Such-Popup (nach Mockup): Eingabe, Ergebnisliste (Symbol, Name, Kürzel, ausgegraut mit Grund,
   wenn nicht möglich, z. B. „Wähle zuerst einen Körper“), ↑/↓, Enter führt aus, Esc schließt.
2. Bewertung (Haltepunkt 1) als reine Funktion in C++ (`src/ui/src/command_search.cpp`, testbar) oder JS; Empfehlung C++ mit Unit-Tests.
3. Akzeptanztest: „runde ecken“, „bohren“, „hohl“, „namen“ finden Abrunden, Bohrung, Aushöhlen, Text. **Achtung:** Abrunden, Bohrung,
   Aushöhlen gibt es erst in Phase 2. Sie kommen trotzdem in den Katalog, als **„kommt bald“** markiert (ausgegraut mit Hinweis), damit
   die Suche sie findet und das Kriterium prüfbar ist. Haltepunkt, falls das nicht gewünscht ist.
4. Unit-Tests: die vier Suchen; Tippfehler „verschiben“ → Verschieben; Umlaute „grosse“ → Größe; englische Wörter in deutscher Oberfläche
   finden auch.

## Teil 1.12c – Hinweiszeile und Tooltip-Karten
1. `HintLine` zentral: ein Objekt (QML-Singleton oder Eigenschaft in `Main.qml`) mit Vorrang: laufender Zug > Ablegen > Hover > Werkzeug >
   Leerlauf. Alle Stellen, die heute Hinweise setzen, melden sich dort an.
2. Tooltip-Karte als eine Komponente `TooltipCard.qml`: Titel, Satz, Kürzel, Bereich für Animation (Platzhalter bis AP-5.2), Verzögerung
   0,8 s, verschwindet bei Klick. Benutzt für Palette, Formen, Menübefehle der Kontextleiste.
3. Bild in den PR: Karte über drei verschiedenen Werkzeugen.

## Teil 1.12d – Meldungskatalog-Grundgerüst
1. `MessageCatalog.js`: je Code `{ code, title(values), explain(values), where, actions: [{ id, label }] }`. Codes = alle
   `FeatureIssueCode`s plus Ablage/Datei-Fehler aus AP-1.10. Platzhalter mit `qsTr("…%1…").arg(...)`.
2. Darstellung: Rezept-Badge + Erklärung inline (AP-1.9) und eine Karte an der Stelle in 3D (Position des Körpers), nie modal.
3. Lösungsaktionen: zunächst nur „Wert ändern“ (fokussiert das Feld) und „Schritt ausblenden“; AP-2.14 ergänzt.
4. Prüfung (Skript oder Test): jeder `FeatureIssueCode` aus `feature_type.h` hat einen Eintrag (Akzeptanz von AP-2.14 vorbereitet).

## Abnahme
- Akzeptanzkriterien 1 und 2 der Phasendatei nachweisbar (Test).
- So probierst du es aus: ⌘K, „runde ecken“ tippen → Abrunden steht oben („kommt bald“). „verschiben“ → Verschieben, Enter schaltet das
  Werkzeug. Über „Drehen“ fahren: nach kurzer Zeit erscheint die Karte. In „Breite“ 0 tippen: Erklärung am Schritt mit „Wert ändern“.

## Stolperfallen
- Ziffern 1–6 sind Ansichten: Solange das Suchfeld Fokus hat, dürfen sie nichts anderes auslösen.
- Doppelte Wahrheit vermeiden: Nach Teil a dürfen `ToolCatalog`/`ShapeCatalog` keine eigenen Texte mehr haben.

## Nicht tun
- Keine Animationen (AP-5.2). Kein vollständiger Katalog aller späteren Fehler (AP-2.14).
