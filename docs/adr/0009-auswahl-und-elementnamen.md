# ADR-0009: Auswahl, Elementnamen und Pick-Durchgang

- **Status:** angenommen (Product Owner, 2026-10-16: „entscheide nach eigenem Ermessen“)
- **Datum:** 2026-10-16
- **Arbeitspaket:** AP-1.4

## Kontext
Man muss Körper, Flächen, Kanten und Ecken anklicken können, und die Auswahl soll eine Änderung weiter oben im Rezept
überleben, ohne je auf ein falsches Element zu springen (Regel 6). [ADR-0003](0003-persistente-referenzen.md) legt
Namen fest, aber gebaut war nur der Prototyp; im Programm hatten bisher nur Flächen Namen, und die Kernel-Netze kannten weder
Kantennachbarn noch Ecken. Die Namensschicht für alle Features entsteht erst mit AP-2.10.

## Optionen
1. **A: Namen schon jetzt aus dem, was es gibt.** Flächen heißen, wie der Schritt sie nannte; Kanten nach den zwei Flächen,
   die sie verbinden; Ecken nach den Flächen, die sich dort treffen (so wie ADR-0003 es beschreibt). Die Auswahl hängt an diesen
   Namen und lässt los, was nicht eindeutig zu finden ist. Keine Ähnlichkeitssuche. · einfach, ehrlich, entspricht ADR-0003 ·
   Features, die Flächen umbauen, brauchen noch die Namensweitergabe (AP-2.10).
2. **B: Nur Körper bleiben gewählt,** Flächen und Kanten werden bei jeder Neuberechnung zurückgesetzt. · am einfachsten · das
   Kriterium „Auswahl bleibt erhalten“ wäre nur halb erfüllt.
3. **C: Ähnlichkeitssuche gleich mit bauen** (Signaturen laut ADR-0003). · bleibt auch nach Umbauten erhalten · das ist AP-2.10;
   ohne die Namensweitergabe der Features wäre es halb fertig und schwer zu prüfen.

Für den Pick-Durchgang selbst: **ID-Bild** (Architektur §5) gegen **CPU-Suchbaum**. Das ID-Bild skaliert mit der Grafikkarte, trifft
genau, was sichtbar ist, und braucht keine zweite Datenstruktur; der Suchbaum wäre sofort (ohne Bild-Verzögerung) und ohne Grafikkarte
testbar.

## Entscheidung
**Option A** und das **ID-Bild** (wie in der Architektur festgelegt):
- `geo::EdgePolyline` bekommt die zwei Flächen und zwei Ecken, `geo::TriangleMesh` die Ecken (GAL; OCCT füllt sie über
  `TopExp::MapShapesAndAncestors`). Die Nummern gelten für den Körper, nicht für eine Feinheitsstufe.
- `core::ElementNames` benennt Flächen, Kanten (`edge[A|B]`), Ecken (`corner[A|B|C]`); doppelte Namen gehören niemandem.
- `core::Selection` (ohne Qt) hält die Auswahl mit Nummer **und** Name; nach einer Neuberechnung sucht `reconcile` über den Namen
  und lässt los, was fehlt oder nie einen Namen hatte.
- Der Pick-Durchgang zeichnet nur einen Ausschnitt um den Zeiger (zugeschnittene Projektion) und liest ihn asynchron zurück.
  Ein Hover-Ergebnis kommt dadurch mit dem nächsten Bild an; Klicks und Hover laufen durch dieselbe Warteschlange.

## Konsequenzen
- Auswahl und spätere Verweise (AP-2.x) sprechen dieselbe Namenssprache; AP-2.10 ersetzt `reconcile`s einfache Suche, nicht die Schnittstelle.
- Jedes neue Feature muss alle erzeugten Flächen benennen (steht schon im Namens-Vertrag, ADR-0003).
- Hover erscheint 1 bis 2 Bilder nach der Mausbewegung. **Revisionskriterium:** Wirkt das beim Ausprobieren träge, bekommt Hover einen
  CPU-Suchbaum (Flächen sofort), das ID-Bild bleibt für Kanten, Ecken und Rahmen.
- Höchstens 1023 Körper und eine Million Flächen je Körper sind im ID-Bild unterscheidbar.

## Belege
Messwerte und Bilder im Abschlussvermerk von AP-1.4 (`docs/arbeitspakete/phase-1-erster-koerper.md`); Tests
`tests/unit/render/test_pick.cpp`, `tests/unit/core/test_selection.cpp`.
