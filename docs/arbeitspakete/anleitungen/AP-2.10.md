# AP-2.10 Persistente Referenzen produktiv, Reparatur-UI

**Größe:** XL, zerlegt in a–e (je ein PR) · **Phasendatei:** `phase-2-praezision.md`, AP-2.10

## Ziel
Schritte, die sich auf eine Fläche oder Kante eines anderen Schritts beziehen („diese Kante abrunden“, „auf dieser Fläche bohren“),
finden ihr Element nach jeder Änderung weiter oben wieder: oder sie melden ehrlich **„braucht Hilfe“**, zeigen die alte Stelle als Geist
und lassen sich mit einem Klick neu zuordnen. **Nie still falsch** (Regel 6). Messlatte: 0 stille Fehlzuordnungen, ≥ 90 % automatisch
richtig in einer Suite mit ≥ 40 Änderungsszenarien.

## Voraussetzungen
AP-0.6 (Prototyp und ADR-0003), AP-1.1, AP-1.7 (Booleans mit Namensweitergabe), AP-1.10 (Verweise müssen gespeichert werden).
**Vor** AP-2.5 (Abrunden), AP-2.4 (Fläche ziehen), AP-2.7 (Bohrungen): Sie bauen darauf.

## Vorher lesen
- `docs/adr/0003-persistente-referenzen.md` (ganz, verbindlich), `docs/adr/0009-auswahl-und-elementnamen.md`,
  `docs/adr/0010-lage-im-rezept.md` Punkt 4 (Vergleich in Körper-Koordinaten)
- `docs/spikes/persistente-referenzen.md` (Messwerte, böse Fälle, Bausteine)
- Prototyp `tests/naming/naming.h` und `naming.cpp` (**die Algorithmen sind dort fertig und gemessen**; dieses AP überträgt sie in den
  Produktcode), `test_scenarios.cpp`, `test_evil.cpp`, `test_random.cpp`
- `src/core/include/fission/core/persistent_ref.h` (Datentyp gibt es), `element_names.h` (Namen der Elemente für die Auswahl)

## Ausgangslage im Code
- `PersistentRef { kind, name, look }` mit `ElementLook` (Form, Schwerpunkt, Richtung, Größe, Radius, Nachbarn) gibt es im Core,
  aber nichts erzeugt oder löst ihn auf.
- Flächen haben Namen (`F1/box/top`) über den Namens-Vertrag; Kanten/Ecken heißen in der Auswahl nach ihren Flächen
  (`core::ElementNames`, ADR-0009).
- Booleans (AP-1.7) geben Namen über die Kernel-Historie weiter.
- Kein Feature hat bisher `refs`.

## Festlegungen (ADR-0003, nicht neu verhandeln)
Name + Aussehen; Kanten nach ihren zwei Flächen; drei Stufen der Auflösung mit Plausibilität (gleiche Art, Drehung ≤ 45° **in
Körper-Koordinaten**, kein Platztausch); Ähnlichkeit ≥ 0,80 und ≥ 0,20 Vorsprung und nicht ähnlicher einem verbrauchten Element;
Gedächtnis für verbrauchte Elemente; nur strenger Modus.

## Haltepunkte
1. **Zerlegung bestätigen** (diese Datei, Teile a–e).
2. **Reparatur-UI:** Empfehlung: Schritt im Rezept ⚠, Klick darauf zeigt den Geist der alten Fläche/Kante (gestrichelt, Akzentfarbe) und
   die Hinweiszeile „Klicke die Fläche, die gemeint ist“; ein Klick ordnet neu zu (ein Undo-Schritt). Mockup vorher zeigen.

## Teil 2.10a – Namensschicht in den Kernel
1. GAL: Ergebnis jeder Operation trägt neben den Flächen-Rollen auch **Historie**: `struct ElementHistory { std::vector<std::pair<std::string,
   std::vector<std::string>>> modified; std::vector<std::string> deleted; }` als Teil von `NamedSolid` (oder daneben). Die OCCT-Umsetzung
   füllt sie aus `Modified/Generated/IsDeleted` (Vorbild `tests/naming/naming.cpp`).
2. Kanten- und Eckennamen aus Flächennamen (gibt es für die Auswahl in `core::ElementNames`; dieselbe Funktion nutzen, nicht verdoppeln).
3. **Aussehen** eines Elements (`ElementLook`) berechnen: GAL-Funktion `std::vector<ElementLook-ähnlich> looks(const Solid&)` liefert für
   jede Fläche und Kante Form, Schwerpunkt, Richtung, Größe, Radius, Nachbarn (OCCT: `BRepAdaptor_Surface`, `GProp_GProps`,
   `TopExp::MapShapesAndAncestors`). Zwischenspeicher je Körper-Ergebnis (gleiches Ergebnis = gleiche Looks).
   Typen: GAL kennt `ElementLook` nicht (das ist Core). Eigene GAL-Struktur `geo::ElementSignature` mit denselben Feldern, der Core
   übersetzt.
4. Gedächtnis für verbrauchte Elemente: am Ergebnis eines Körpers (`BodyResult`) eine Liste `consumed` (Name + Aussehen + Schritt, der es
   verbraucht hat). Jede Operation übernimmt die Liste der Eingänge und ergänzt, was sie löscht.
5. Tests: Für jeden vorhandenen Feature-Typ (Formen, Text, combine, split) prüfen, dass Historie und Looks vollständig sind; Golden-Werte
   der Looks für einen Quader (Schwerpunkt der Oberseite usw.).

## Teil 2.10b – Auflösung im Core
1. `src/core/include/fission/core/resolve.h`: `ResolveOutcome resolve(const PersistentRef&, const BodyLooks& now, const BodyLooks& consumed,
   const Placement& startTurn)` mit `ResolveOutcome { enum { found, needsHelp } state; std::string element; HelpReason reason; }` und
   `HelpReason { split, consumed, turned, swapped, notFound }` (Codes für den Meldungskatalog).
2. Algorithmus **1:1 aus dem Prototyp übernehmen** (`tests/naming/naming.cpp`), mit den Schwellen aus ADR-0003 als benannte Konstanten.
   Vergleiche in Körper-Koordinaten (ADR-0010 Punkt 4): Lage/Drehung des erzeugenden Schritts herausrechnen.
3. Aufruf in der Neuberechnung **vor** `execute` eines Schritts mit `refs`: aufgelöste Elemente gehen als `ExecuteInput::refs`
   (neues Feld: Name → aktueller Elementname) an den Feature-Typ. `needsHelp` → Status `warning` mit `FeatureIssueCode::referenceNeedsHelp`
   (neu) und Grund; der Schritt rechnet **nicht** mit geratener Geometrie, sondern behält sein letztes gültiges Ergebnis (gibt es schon).
4. Heilen: Nach sicherer Auflösung wird das Aussehen im Verweis aufgefrischt. Das ändert das Dokument → **nicht** im Hintergrund-Thread
   schreiben; Ergebnis meldet „Verweis X hat jetzt Aussehen Y“, die Oberfläche übernimmt es ohne Undo-Schritt (technische Pflege, kein
   Nutzerbefehl). Haltepunkt, falls das unsauber wirkt; Alternative: Aussehen nicht speichern, sondern bei jedem Laden neu.
5. Tests: alle Szenarien aus `tests/naming/test_scenarios.cpp` und `test_evil.cpp` gegen den Produktcode portieren
   (`tests/unit/core/test_resolve.cpp`), gleiche Erwartungen.

## Teil 2.10c – Verweise anlegen und speichern
1. `core::makeRef(elementName, looks)`: aus einer Auswahl (ADR-0009) einen `PersistentRef` bauen.
2. Feature-Typen bekommen `refs` über `FeatureDraft::refs` (gibt es) – noch nutzt sie keiner; ein **Test-Feature** `markFace` (nur in Tests
   registriert, färbt nichts, prüft nur, dass der Verweis aufgelöst wird) dient der Suite, bis 2.5/2.4/2.7 echte Nutzer bringen.
3. Dateiformat: `refs` mit Aussehen in `document.json` (AP-1.10); Roundtrip- und Migrationstest (neues Feld!).

## Teil 2.10d – Suite mit echten Features
1. `tests/naming/` bleibt der Prototyp. Neue Produkt-Suite `tests/unit/naming/` (oder `tests/naming_product/`) mit ≥ 40 Szenarien:
   Maß ändern (Quader höher/breiter), Schritt davor einfügen, Schritt ausblenden, Loch hinzufügen/entfernen/verschieben, Drehen um 90°/180°,
   Teilen, Vereinen, Text ändern, Skizze ändern (sobald AP-2.3 da ist; bis dahin markieren und nachziehen).
2. Zufallsprobe aus `test_random.cpp` auf echte Features umstellen (Wahrheit aus Geometrie, nie aus Namen); feste Seeds, eigener Zufallsgenerator
   (wie im Prototyp, plattformgleich).
3. „Baustein abschalten → Suite wird rot“ (wie `[sensitivity]` im Prototyp): je Baustein (Gedächtnis, Plätze-getauscht-Prüfung, 45°-Regel)
   ein Test, der beweist, dass er gebraucht wird.
4. Bericht-Zeile in `scripts/test` („Persistente Referenzen (Produkt): N Szenarien, 0 still falsch“).
5. Abnahme-Zahlen: 0 still falsch, ≥ 90 % automatisch richtig.

## Teil 2.10e – Reparatur-UI (nach Haltepunkt 2)
1. Rezept: Status ⚠ mit Grund (Meldungskatalog: `split` „Die Fläche wurde geteilt“, `consumed` „Die Kante gibt es nach Schritt ③ nicht mehr“,
   `turned`, `swapped`, `notFound`) und Knopf „Neu wählen“.
2. Geist: Das alte Element aus dem Aussehen zeichnen (Fläche: Umriss um Schwerpunkt mit Größe, oder besser die letzte gültige Geometrie des
   Elements, falls im Ergebnis-Cache; Kante: Linie). Quelle `HandleSource` erweitern.
3. Neu wählen: Auswahlmodus nur für die passende Art (Fläche/Kante), ein Klick setzt den Verweis neu (Command `makeSetFeatureRefs`, gibt es
   als Document-Funktion; Command ergänzen), ein Undo-Schritt.
4. Tests: Szenario, das `needsHelp` erzeugt → Neu wählen → Status ok; Undo stellt `needsHelp` wieder her.

## Abnahme
- Suite grün mit den Zahlen oben; Laufzeit der Auflösung bei 50 Schritten < 50 ms (messen, in den PR).
- So probierst du es aus (nach AP-2.5): Quader, Kante abrunden, Quader höher machen → Rundung bleibt an der Kante. Quader-Schritt so ändern,
  dass die Kante verschwindet (z. B. durch Teilen) → ⚠ „braucht Hilfe“, Geist zeigt die alte Kante, Klick auf eine neue Kante repariert.

## Stolperfallen
- Der Prototyp ist Testcode (darf OCCT direkt benutzen). Im Produkt gilt die Schichtregel: Historie und Looks entstehen in `geometry/occt`,
  Auflösung im Core ohne Kernel-Typen.
- `std::rand`/`std::uniform_*_distribution` liefern auf macOS und Linux verschiedene Folgen: eigenen Generator nehmen (wie im Prototyp).
- Zeitbudget: Looks aller Elemente eines großen Körpers sind teuer; nur für Körper berechnen, auf die ein Verweis zeigt.

## Nicht tun
- Keine großzügige „bitte prüfen“-Stufe (ADR-0003 Punkt 6).
- Kein OCAF.
- Schwellen nicht „mal eben“ lockern, um mehr Treffer zu bekommen: Änderung nur mit neuer Messung und ADR-Nachtrag.
