# ADR-0012: Eingebettete Dateien und der Import-Schritt

- **Status:** angenommen (2026-11-05)
- **Datum:** 2026-11-05
- **Arbeitspaket:** AP-2.13

## Kontext
AP-2.13 bringt STEP und IGES ins Programm. Ein importierter Körper hat keinen Verlauf, soll aber im Rezept stehen, sich verschieben
lassen, mit Undo funktionieren und nach dem Speichern und Öffnen derselbe sein (Regeln 3 und 7). Die Fremddatei muss deshalb in der
Projektdatei stecken: Wer das Projekt weitergibt, gibt die Datei mit, und eine Neuberechnung liest immer dieselben Bytes.

## Entscheidung
1. **Eingebettete Dateien** liegen im Ordner `assets/` der `.fission`-Datei (so sah es die Architektur §7 schon vor), jede mit
   Prüfsumme im Manifest (`assets/<Name>` → `sha256:…`). `document.json` und `formatVersion` bleiben unverändert: alte Programme
   öffnen neue Dateien (sie kennen nur den Schritt-Typ nicht), neue öffnen alte. Eine Datei in `assets/` ohne Prüfsumme gilt als
   beschädigt.
2. **Das Dokument hält die Dateien** (`Document::assets`, Name → unveränderliche Bytes, Kopien teilen sie). Der Name ist ein schlichter
   Dateiname (kein `/`, kein führender Punkt) und wird bei Doppelungen nummeriert (`teil-2.step`). Gespeichert werden nur Dateien, die ein
   Schritt noch nennt (`Document::usedAssetNames`, das Wort `asset`); wer einen Import rückgängig macht und speichert, trägt die Datei
   nicht mit.
3. **Ein Schritt-Typ `import`, ein Schritt je Körper der Datei.** Werte: `part` (Nummer des Körpers in der Datei), `unitFactor`
   (Standard 1) und die Lage `x`, `y`, `z`; Wörter: `asset` und `format` (`step` | `iges`). Der Import ist **ein** Undo-Schritt
   (`ImportFileCommand`), Körpername und Farbe kommen aus der Datei. Die Flächen heißen `F<n>/import/face<i>` in der Reihenfolge der
   Körperflächen. Ein erneuter Import derselben Datei legt eine zweite Datei und neue Schritte an; „Aktualisieren“ gibt es später.
4. **Einheit:** STEP trägt immer eine Einheit, OCCT rechnet in Millimeter um. IGES kann sie auslassen: dann gelten Millimeter, die
   Oberfläche sagt es und bietet „Es waren Zoll“ an (setzt `unitFactor` 25,4 an allen Schritten dieser Datei, ein Undo-Schritt).
5. **Reparieren nie still:** Ungültige Körper werden mit `ShapeFix_Shape` repariert und als „repariert“ gemeldet. Lose Flächen, die eine
   geschlossene Hülle bilden, werden zu einem Körper vernäht (Toleranz 0,0001 mm) und als „zusammengesetzt“ gemeldet. Alles andere
   (offene Flächen, Kurven) bleibt draußen und wird gezählt. Baugruppen werden flach gelesen, mit Hinweis.
6. **Der Kernel liest und schreibt über die GAL** (`importFile`, `exportStep`): STEP/IGES über OCCT XCAF (Farben, Namen). Es werden nur
   `TKDESTEP TKDEIGES TKXCAF TKXSBase TKDE` gelinkt (zusammen mit ihren Abhängigkeiten etwa 18,5 MB mehr im Paket, macOS arm64,
   unkomprimiert).

## Folgen
- Dateiformat wächst nur um den Ordner `assets/`; der Roundtrip- und Fremddatei-Test liegt in `tests/unit/io` und `tests/unit/geometry`.
- Importierte Körper lassen sich verschieben; Drehen und Größe ändern gehen über die allgemeinen Wege (Schritt `place`, AP-2.4 für
  direktes Bearbeiten).
- IGES ohne geschlossene Hüllen (reine Flächenmodelle) bleibt draußen, bis es Flächenkörper gibt.
- Die Datei steckt unverändert im Projekt; große Dateien machen große Projekte (kein Zusammenpacken über Dateien hinweg).
