# AP-5.10 Baukasten „Karten“

**Größe:** L · **Phasendatei:** AP-5.10

## Ziel
Der Karten-Baukasten nach `docs/07-baukaesten.md` (Baukasten 1): ① Format · ② Hintergrund · ③ Motive · ④ Text · ⑤ Feinschliff · ⑥ Drucken. Kartenarten:
flach, Klappkarte (druckbares Scharnier oder Steckverbindung), Aufstellkarte, Lichtkarte (Lithophanie), Mini-Geschenkanhänger. Textvorschläge DE/EN, Text
passt sich in Flächen ein. Mehrfarbig je Motiv-Ebene oder **Farbwechsel nach Höhe** für Einzeldüsen. **Versand-Check** (Umschlag, Dicke, Gewicht; zuerst
Deutsche Post). Meilenstein M5: eine zweifarbige Weihnachtskarte in ≤ 5 min durch eine Person ohne CAD-Erfahrung.

## Voraussetzungen
AP-5.8, AP-5.9, AP-3.4, AP-4.7 (Durchbrüche), AP-4.6 (Lichtkarte), AP-5.3 (Scharnier).

## Vorher lesen
`docs/07-baukaesten.md` Baukasten 1 (Tabelle der Schritte und Optionen = Pflichtumfang).

## Haltepunkte
1. **Klappkarte-Scharnier:** Empfehlung: Filmscharnier (dünne Brücke 0,4–0,6 mm über die ganze Breite, im selben Druck) als Standard, Steckverbindung als
   Alternative. Belastungstest ≥ 50 Öffnungen auf Referenzdrucker (Product Owner). Bestätigen.
2. **Portodaten:** Deutsche Post (Standardbrief ≤ 5 mm/20 g, Kompaktbrief ≤ 10 mm/50 g, Großbrief ≤ 20 mm/500 g; Umschlagmaße C6, DL, C5) als Datei
   `resources/postage/de.json` mit Stand-Datum; der Product Owner prüft die Werte vor der Veröffentlichung.

## Schritte
1. Baukasten-Datei `resources/kits/cards.json` mit allen Optionen aus der Tabelle; jede Option über die Operationen aus AP-5.8 (Haltepunkt 1 dort).
2. Fehlende Bausteine/Generatoren als Teile dieses APs (je PR): Kartenformen (Rechteck, abgerundet, Stern, Tannenbaum, Kugel, Herz, Wolke) als Skizzen-
   Profile; Filmscharnier-Baustein; Steckfuß; Text „in Fläche einpassen“ (größte Schrift, die mit Rand in die Fläche passt – Suche über die Text-Hülle).
3. Gewicht/Dicke: Gewicht aus Volumen × Dichte × Füllfaktor (Annahme mit Hinweis), Dicke aus Hüllquader; Versand-Check-Karte im Schritt ⑥.
4. Farbwechsel nach Höhe (AP-3.4d) als Option im Schritt ⑤ bei Einzeldüse.

## Tests
Jede Option erzeugt ein gültiges, druckbares Modell (Property-Test über alle Optionen eines Schritts mit Standardwerten der anderen); Versand-Check-Werte
für Beispielkarten; Farbwechsel kommt im 3MF an (AP-3.4-Test wiederverwenden).

## Abnahme
Meilenstein-Demo mit einer Testperson (Product Owner organisiert); Klappkarte 50× öffnen (Foto/Video); Slicer-Nachweis Farbwechsel.

## Nicht tun
Keine festen Kartenvorlagen. Keine Motive ohne Lizenz.
