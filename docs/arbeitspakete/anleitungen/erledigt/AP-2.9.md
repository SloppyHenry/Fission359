# AP-2.9 Benannte Maße und Konfigurator

**Größe:** M · **Phasendatei:** AP-2.9

## Ziel
Eine **Maße-Tabelle** (Name, Ausdruck, Ergebnis, Einheit, Kommentar, „verwendet in“) unter dem Rezept oder mit ⌘M. Aus jedem Zahlenfeld per
Rechtsklick „Als benanntes Maß speichern“. **Autovervollständigung** in allen Zahlenfeldern. Umbenennen aktualisiert alle Ausdrücke in **einem**
Undo-Schritt. Eine **Schieberegler-Ansicht** (min/max/Schritt je Maß) macht jedes Modell zum Konfigurator. Ablauf B Schritt 7 (Breite 30 → 40,
alles passt sich an) funktioniert.

## Voraussetzungen
AP-1.8 (Zahlenfeld). Core kann benannte Maße schon (AP-1.1: `resolveMeasures`, Zyklen, „verwendet in“; Commands für Anlegen/Ändern/Umbenennen/
Löschen in AP-1.2).

## Vorher lesen
`docs/02-ux-konzept.md` §4 „Benannte Maße“, §2.3; `src/core/include/fission/core/measures.h`, `expression.h`; `commands.h` (Maß-Commands);
`NumberField.qml`; Mockup (falls Maße-Tabelle gezeigt).

## Ausgangslage im Code
Der Core rechnet mit benannten Maßen inklusive „verwendet in“ und Zyklus-Erkennung; `makeRenameParameter` benennt das Maß um, ändert aber
**nicht** die Ausdrücke, die es benutzen (prüfen!).

## Haltepunkte
1. **Wo steht die Tabelle?** Empfehlung: ausklappbarer Bereich „Maße“ unter dem Rezept (UX §4), ⌘M klappt auf; Schieberegler-Ansicht als
   Umschalter oben im Bereich.
2. **Min/Max/Schritt** gehören zum Maß und werden in der Datei gespeichert (neues Feld) – OK?

## Schritte
1. **Umbenennen überall** (Akzeptanzkriterium 2): Core-Funktion `std::string renameInExpression(expr, oldName, newName)` (nur ganze Namen, nicht
   Teilwörter; Tokenizer aus `expression.cpp` benutzen). Command `makeRenameMeasureEverywhere`: benennt das Maß um **und** schreibt alle Ausdrücke
   in Schritten und anderen Maßen um – ein Command, ein Undo-Schritt. Tests: Maß in 10 Ausdrücken, Umbenennen, Undo; „Breite“ und „Breite2“
   werden nicht verwechselt.
2. **Tabelle** (QML `MeasuresPanel.qml`): Zeilen mit Name (umbenennbar), Ausdruck (Zahlenfeld), Ergebnis (nur Anzeige), Einheit (aus der Art),
   Kommentar, „verwendet in“ (Liste von Schritt-Nummern, Klick wählt den Schritt), Fehler je Zeile (Zyklus mit Pfad „a → b → a“). Hinzufügen
   (+), Löschen (mit Hinweis, wenn verwendet: inline-Karte, wie AP-1.9).
3. **Als benanntes Maß speichern:** Kontextmenü am Zahlenfeld: legt ein Maß mit dem aktuellen Wert an, Name vorgeschlagen aus Feldname
   („Breite“, bei Kollision „Breite 2“), ersetzt den Feldwert durch den Namen – ein Undo-Schritt.
4. **Autovervollständigung** im `NumberField`: beim Tippen eines Buchstabens Liste der passenden Maße (Anfang des Namens, dann enthalten), ↑/↓, Tab
   übernimmt. Die Liste kommt aus `Numbers` (C++), sortiert.
5. **Konfigurator:** je Maß `min`, `max`, `step` (Felder in der Tabelle, ▸ Mehr); Schieberegler-Ansicht zeigt nur Maße mit min/max als Regler;
   Ziehen = Transaktion (ein Undo-Schritt), Live-Vorschau. Datei: neue Felder → Roundtrip + Migration.
6. Ablauf B Schritt 7 als UI-Test (Wandhalterung mit `Breite`, Löcher mittig über `Breite/2`; Breite 30 → 40; Löcher bleiben mittig; Volumen).

## Tests
Siehe Schritte; außerdem Autovervollständigung (Funktion in C++ mit Unit-Test) und Zyklusanzeige.

## Abnahme
So probierst du es aus: Wandhalterung (Ablauf B) mit benanntem Maß `Breite`; ⌘M, Regler-Ansicht, Breite ziehen → Halterung wird breiter, Löcher
bleiben mittig. `Breite` in `Halterbreite` umbenennen → alle Formeln folgen; ⌘Z einmal → alles zurück.

## Nicht tun
Keine Varianten (Maß-Sätze „Klein/Groß“) – das ist AP-5.7.
