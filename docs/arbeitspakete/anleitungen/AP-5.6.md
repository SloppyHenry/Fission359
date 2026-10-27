# AP-5.6 Barrierefreiheit und Tastatur

**Größe:** M · **Phasendatei:** AP-5.6

## Ziel
Fission359 ist vollständig per Tastatur bedienbar (sichtbarer Fokus, sinnvolle Tab-Reihenfolge), Bildschirmleser beschriften alle Bedienelemente, Status
ist nie nur Farbe. Eine Tastenkürzel-Übersicht (`?`) und frei belegbare Kürzel. Ablauf A geht ohne Maus (Positionieren über Zahlen).

## Voraussetzungen
AP-1.11 (Einstellungen), am besten nach den meisten Werkzeugen.

## Vorher lesen
`docs/02-ux-konzept.md` §14; Qt-Dokumentation `Accessible` (QML), `Focusring.qml` (gibt es); `MenuStrip.qml`, `AppMenu.qml` (`Accessible.role` wird schon genutzt).

## Haltepunkte
1. **Tastatur-Bedienung der 3D-Ansicht:** Empfehlung: Tab erreicht die Körperliste und das Rezept; in der Ansicht wählt `Tab`/`⇧Tab` nacheinander die
   Körper, Pfeiltasten verschieben (gibt es), `Enter` öffnet das Panel; Flächen/Kanten per Tastatur nur über die Liste im Panel. Bestätigen.

## Schritte
1. **Audit**: jedes QML-Bedienelement hat `Accessible.name` (übersetzt) und `Accessible.role`; Skript `scripts/tools/check_accessible.py` (ähnlich
   `check_translations.py`) findet `MouseArea`/Knöpfe ohne Namen; als Test.
2. **Fokus**: sichtbarer Fokusring überall (`Focusring.qml`), Tab-Reihenfolge je Bereich (`KeyNavigation`/`activeFocusOnTab`).
3. **Status nie nur Farbe**: Rezept-Badges, Druckcheck-Ampel, Auswahl haben Symbol oder Text (prüfen, sonst ergänzen).
4. **Kürzel-Übersicht** `?` (Overlay mit allen Kürzeln aus dem Befehlskatalog, gruppiert); **eigene Belegung** in den Einstellungen: Kürzel je Befehl
   ändern, Konflikte anzeigen, gespeichert in den Einstellungen.
5. **Audit mit VoiceOver (macOS) und Orca (Linux)** für die Panels: Ablauf und Funde im PR (das macht der Product Owner oder der Umsetzer am Mac mit
   VoiceOver; Linux auf einem Testrechner mit Desktop).
6. Ablauf A ohne Maus als Fenster-Test (nur Tastenaktionen, `key:`) – Akzeptanzkriterium.

## Abnahme
So probierst du es aus: Maus weglegen; mit Tab, Pfeilen und Zahlen einen Schlüsselanhänger bauen und exportieren.

## Nicht tun
Keine eigenen Bildschirmleser-Ausgaben neben Qt Accessibility.
