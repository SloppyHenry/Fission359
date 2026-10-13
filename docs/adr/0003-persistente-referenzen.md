# ADR-0003: Persistente Referenzen (Topological Naming)

- **Status:** vorgeschlagen (Spike AP-0.6 abgeschlossen; Annahme durch den Product Owner steht aus)
- **Datum:** 2026-10-14
- **Arbeitspaket:** AP-0.6

## Kontext
Features verweisen auf Flächen und Kanten anderer Features („diese Kante abrunden“, „hier bohren“). Ändert
sich etwas weiter oben im Rezept, müssen diese Verweise an der richtigen Stelle bleiben. Ist das nicht sicher
möglich, darf das Programm nicht raten (Produktregel 6: „Nie still falsch“). Das ist das größte
Architekturrisiko des Feature-Systems (Architektur § 3.4).

Rahmenbedingungen: Kernel OpenCASCADE 8.0.1 (Historie über `Modified/Generated/IsDeleted`), Verweise müssen im
`.fission`-Format (JSON) speicherbar sein, `core` kennt keine Kernel-Typen, Auflösung läuft im
Hintergrund-Thread. Die Messungen stehen in [docs/spikes/persistente-referenzen.md](../spikes/persistente-referenzen.md).

## Optionen
1. **A: Eigene Namensschicht auf der Kernel-Historie.** Features benennen, was sie erzeugen; Namen wandern
   durch die Operationen; Verweise merken sich ihr Aussehen; Auflösung in drei Stufen (Name, klare
   Ähnlichkeit, Hilfe). · kurze, lesbare Namen · volle Kontrolle über die Strenge · speicherbar in JSON ·
   jedes Feature muss einen Namens-Vertrag erfüllen · Auflösung und Aufbewahrung gehören uns (Wartung).
2. **B: OCAF / TNaming (OCCT-eigen).** Labels, Attribute und `TNaming_Selector`. · vorhanden und
   erprobt · aber: zweiter Dokumentbaum neben unserem Dokumentmodell (Architektur § 3.1), Speichern über
   OCAF-Formate statt `.fission`, keine Ähnlichkeitssuche, im Test ein stiller Fehler (zwei gleiche Flächen
   tauschen Plätze) und einer durch vergessene Buchführung, Speicherleck in `TNaming_Builder::Select`
   (OCCT 8.0.1), zusätzliche Bibliotheken (TKCAF, TKLCAF, TKCDF).
3. **C: FreeCAD-Weg (aus der Historie zusammengesetzte Namen, gehasht).** · benötigt keinen Vertrag je
   Feature · Namen wachsen mit dem Rezept und werden gehasht (Hash-Tabelle muss gespeichert werden), Namen
   ändern sich bei Algorithmus-/OCCT-Wechsel, keine lesbaren Meldungen, in der Dokumentation keine
   Ähnlichkeitssuche und kein „braucht Hilfe“ im Sinne unserer Regel. Nur aus der Dokumentation bewertet.

## Entscheidung
Wir wählen **Option A**, mit diesen festen Bestandteilen:

1. **Semantische Namen je Feature** (`F5/hole/wall`, `F3/extrude/side[S2]`), stabil gegen Einfügen davor.
   Die Feature-Nummer wird nie wiederverwendet.
2. **Kanten heißen nach den zwei Flächen, die sie verbinden** (`edge[A|B]`), noch unbenannte Flächen nach ihren
   benannten Nachbarn. Namen wandern über die Kernel-Historie.
3. **Verweis = Name + Aussehen** (Art, Lage, Richtung, Größe, Radius, Nachbarn mit Aussehen). Nach jeder
   sicheren Auflösung wird er aufgefrischt („heilt“).
4. **Auflösung in drei Stufen mit Plausibilitätsprüfung:**
   - Name eindeutig → nur übernehmen, wenn die Art gleich ist, die Drehung ≤ 45° beträgt und kein gleich
     aussehendes Element an der alten Stelle steht, während das benannte weggewandert ist („Plätze getauscht“).
   - Name mehrdeutig (Element geteilt) → immer „braucht Hilfe“.
   - Name weg → Ähnlichkeitssuche; ein Sieger gilt nur, wenn er ≥ 0,80 erreicht, mindestens 0,20 vor dem Zweiten
     liegt **und** keinem verbrauchten Element ähnlicher sieht als einem lebenden.
5. **Gedächtnis für verbrauchte Elemente:** Der Körper führt benannte Elemente mit, die ein Feature aufgebraucht
   hat (Name nach der Operation nicht mehr vorhanden). Die Kernel-Historie reicht dafür für Kanten nicht aus.
6. **Nur der strenge Modus.** Die großzügige Variante („⚠ bitte prüfen“) wird nicht gebaut: In der Messung
   brachte sie keinen zusätzlichen richtigen Treffer, aber 9 bis 33 falsche mit Warnung.
7. **Ein Feature, das keinen Namens-Vertrag erfüllt, wird nicht aufgenommen.** Der Vertrag: alle erzeugten
   Flächen benennen und die Historie der Operation liefern.

## Konsequenzen
- **Messlatte erreicht:** 0 stille Fehler in 1 494 Verweisen aus 421 Zufallsänderungen und in allen
  Szenarien und bösen Fällen; 93 % automatisch richtig, 6 % zu Recht „braucht Hilfe“, 0,2 % unnötig.
- **Folge-Arbeiten (AP-2.10):** Namensschicht in `src/geometry/occt` (Kernel-Typen) und Verweis-Datentyp in
  `src/core` (ohne Kernel-Typen: Name + Signatur + Nachbarn); Zwischenspeicher je Körper für Signaturen;
  Auflösung im Hintergrund-Thread; Format-Felder im `.fission` mit Roundtrip- und Migrationstest;
  Zufallsprobe mit den echten Features als feste CI-Suite, samt „Baustein abschalten → Suite wird rot“.
- **Meldungskatalog (AP-2.14):** Die Gründe für „braucht Hilfe“ (geteilt, verbraucht, gedreht, Plätze
  getauscht, nicht gefunden) bekommen Hilfetexte mit Ein-Klick-Lösung.
- **Schwerer:** Jedes neue Feature braucht Namensregeln und Tests (Naming-Szenarien, laut Master-Prompt);
  Namen sind Teil des Dateiformats (ändern sie sich, braucht es eine Migration).
- **Noch nicht geprüft:** Fasen, Aushöhlen, Muster, Spiegeln, Text, Booleans mehrerer Körper, Verschmelzen
  gleicher Flächen, Laufzeit bei großen Körpern.
- **Revisionskriterium:** Wenn die erweiterte Zufallsprobe mit den echten Features auch nur einen stillen
  Fehler zeigt, den die Bausteine oben nicht beheben, oder wenn in realistischen Sitzungen mehr als etwa
  15 % der Verweise unnötig um Hilfe bitten, wird die Entscheidung überprüft (Kandidaten: Namen mit
  Herkunft wie bei FreeCAD, oder eine OCAF-Schicht nur für die Historie).

## Belege
- Bericht mit Zahlen: [docs/spikes/persistente-referenzen.md](../spikes/persistente-referenzen.md)
- Prototyp und Tests: `tests/naming/` (`scripts/test`, Zeile „Persistente Referenzen“)
- OCAF-Praxistest: `tests/naming/test_ocaf.cpp`
- FreeCAD: Quellkopf `src/App/ElementMap.h`, Wiki-Beschreibung des Naming-Algorithmus (nur gelesen, kein
  Code übernommen)
