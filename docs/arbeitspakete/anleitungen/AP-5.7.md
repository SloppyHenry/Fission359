# AP-5.7 Versionsverlauf („Zeitreise“) und Varianten

**Größe:** M · **Phasendatei:** AP-5.7

## Ziel
Beim Speichern entstehen automatisch Versionen (Größe begrenzt), mit Vorschaubildern; „Diese Version wiederherstellen“ oder „Als Kopie öffnen“.
**Varianten**: eine Datei, mehrere Maß-Sätze („Klein“, „Groß“), umschaltbar. Eine Version von vor 10 Speichervorgängen lässt sich wiederherstellen.

## Voraussetzungen
AP-1.10 (Dateiformat), AP-2.9 (benannte Maße – Varianten sind Maß-Sätze).

## Vorher lesen
`docs/03-architektur.md` §3.5 (Undo-Stapel nicht in der Datei; Versionsverlauf übernimmt das), §7; AP-1.10-Anleitung.

## Haltepunkte
1. **Wo liegen Versionen?** Empfehlung: **in der Datei** unter `versions/<n>/document.json` + `preview.png` (nur `document.json` ist klein; keine Caches),
   höchstens 50 Versionen oder 20 MB, älteste zuerst weg; „Versionen in der Datei behalten“ abschaltbar (für das Teilen ohne Verlauf).
   Alternative: Ordner neben der Datei (unordentlich).

## Schritte
1. Speichern (AP-1.10) legt vor dem Überschreiben die bisherige `document.json` als Version ab (mit Zeit, App-Version, Vorschaubild). Migration:
   alte Versionen werden beim Wiederherstellen migriert (gleiche Migrationskette).
2. Oberfläche: Datei ▸ Versionen… – Liste mit Bild, Zeit, kurzer Zusammenfassung (Anzahl Schritte/Körper); „Wiederherstellen“ (ersetzt das Dokument –
   ein Undo-Schritt? Empfehlung: ja, als „Version von 14:32 wiederhergestellt“, damit man zurück kann) und „Als Kopie öffnen“ (neuer Tab, AP-1.11).
3. Varianten: Dokument bekommt `variants` (Name → Map Maßname → Ausdruck) und `activeVariant`; Umschalter in der Maße-Tabelle (AP-2.9); Umschalten ist
   ein Undo-Schritt. Datei: neue Felder → Roundtrip + Migration.

## Tests
12-mal speichern mit Änderungen → Version 2 wiederherstellen = Inhalt von damals (`sameContentAs`); Grenze 50/20 MB greift; Varianten-Umschalten
ändert Maße und Geometrie, Undo.

## Abnahme
So probierst du es aus: Datei mehrmals speichern, Datei ▸ Versionen → alte Version „Als Kopie öffnen“. Maße ▸ Variante „Groß“ anlegen, umschalten.

## Nicht tun
Kein Zusammenführen von Versionen (kein Git im Programm).
