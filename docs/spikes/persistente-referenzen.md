# Spike AP-0.6: Persistente Referenzen (Topological Naming)

**Stand:** 14.10.2026 · **Kernel:** OpenCASCADE 8.0.1 · **Geprüft auf:** Mac (Apple Silicon, macOS) und
Linux-Testrechner C (Ubuntu 26.04, x86_64, Debug mit Speicherfehler-Erkennung) · **Code:** `tests/naming/`
(Prototyp, kein Produktionscode; AP-2.10 baut die Lösung richtig in den Kern)

Ziel war zu wissen, ob wir „Fläche 7“-Verweise vermeiden können, bevor das Feature-System gebaut wird: Ein
Verweis (z. B. „diese Kante abrunden“) soll nach einer Änderung weiter oben im Rezept an der richtigen Stelle
bleiben, oder ehrlich sagen „braucht Hilfe“. **Nie still an die falsche Stelle zeigen.**

## Kurzfassung

1. **Das Konzept aus der Architektur (§ 3.4) trägt.** In 1 494 geprüften Verweisen aus 421 zufälligen
   Änderungen (Quader und Skizzen-Körper mit Bohrungen und Abrundungen) wurde **kein einziger still falsch**
   zugeordnet. 93 % wurden automatisch richtig gefunden, 6 % sind Fälle, in denen es keine eindeutig richtige
   Antwort gibt und das Programm zu Recht um Hilfe bittet, 0,2 % (3 Fälle) bat unnötig um Hilfe.
2. **Alle „bösen Fälle“ bestanden.** Fläche teilt sich, Fläche verschwindet, zwei gleiche Flächen tauschen die
   Plätze: der strenge Modus bat jedes Mal um Hilfe (7 von 7).
3. **Die Zufallsprobe hat einen echten stillen Fehler gefunden, den die handgeschriebenen Szenarien nicht
   zeigten:** Eine Abrundung „verbraucht“ eine Kante; die neue Kante daneben sieht fast genauso aus und wurde
   für die alte gehalten. Behoben durch ein „Gedächtnis für verbrauchte Elemente“. Ohne es gibt es in der
   Zufallsprobe 8 stille Fehler. Dieser Fall gehört dauerhaft in die Tests.
4. **Die großzügige Variante („Treffer mit ⚠ bitte prüfen“) bringt nichts.** Sie findet keinen einzigen
   zusätzlichen richtigen Treffer, aber 9 falsche mit Warnhinweis. Empfehlung: nur der strenge Modus.
5. **OCCTs eigenes Namenssystem (OCAF/TNaming) löst die einfachen Szenarien ebenfalls**, scheitert aber im
   Fall „zwei gleiche Flächen tauschen die Plätze“ (stiller Fehler), kennt keine Ähnlichkeitssuche, verlangt
   viel Buchführung (vergessene Aufräumarbeit ergab einen stillen Fehler im Test) und hat ein Speicherleck.
   Wir bauen deshalb auf der **Kernel-Historie mit eigener Schicht** auf (ADR-0003).
6. **Grenze der Aussage:** Der Prototyp kennt vier Features und zwei Körperarten. Null Fehler in 1 494 Fällen
   heißt statistisch „höchstens ca. 0,2 % mit 95 % Sicherheit“, nicht „nie“. AP-2.10 muss die Probe mit den
   echten Features wiederholen (siehe Empfehlung).

## So funktioniert es (ohne Fachbegriffe)

- **Jedes Feature benennt, was es erzeugt.** Ein Quader nennt seine Flächen `F1/box/face:+Z` (Oberseite),
  ein Extrudieren `F3/extrude/side[S2]` (Seitenfläche über Skizzen-Segment 2), eine Bohrung
  `F5/hole/wall` und `F5/hole/floor`. Die Nummer des Features ändert sich nie, auch wenn davor etwas
  eingefügt wird.
- **Die Namen wandern durch die Operationen.** Der Kernel meldet bei Vereinigen, Ausschneiden und Abrunden,
  welche neue Fläche aus welcher alten wurde. Daraus übernimmt der Prototyp die Namen.
- **Kanten heißen nach den zwei Flächen, die sie verbinden:** `edge[F1/box/face:+X|F1/box/face:+Z]`. Das ist
  „die Kante zwischen rechter Seite und Oberseite“, wie ein Mensch es sagen würde.
- **Jeder Verweis merkt sich zusätzlich, wie sein Ziel aussah** (Art, Lage, Richtung, Größe, Nachbarn).
- **Auflösung in drei Stufen:**
  1. *Name:* Gibt es genau ein Element mit diesem Namen, und sieht es noch plausibel aus (gleiche Art, nicht
     um mehr als 45° gedreht, kein gleich aussehendes Element steht an der alten Stelle)? Dann ist es das.
  2. *Ähnlichkeit:* Ist der Name weg (z. B. weil das Feature durch ein neues ersetzt wurde), gewinnt nur ein
     **klarer** Sieger: sehr ähnlich, deutlich besser als der Zweite, nicht ähnlicher einem verbrauchten
     Element.
  3. *Hilfe:* Sonst „braucht Hilfe“. Das Feature wird dann nicht angewendet, der Rest rechnet mit dem letzten
     gültigen Stand weiter.
- **Nach jeder erfolgreichen Auflösung** zeigt der Verweis auf den aktuellen Namen und das aktuelle Aussehen
  („heilt“). Ein vom Nutzer nicht bestätigter Treffer wird nie eingetragen.

## Ergebnisse

✔ = richtig · Hilfe+ = zu Recht um Hilfe gebeten (es gibt keine eindeutig richtige Antwort) ·
Hilfe− = unnötig um Hilfe gebeten · **FALSCH = still falsch (muss 0 sein)**

### Szenarien aus dem Arbeitspaket (strenger Modus, Mac)

| Szenario | Verweise | ✔ per Name | ✔ per Ähnlichkeit | Hilfe+ | Hilfe− | FALSCH |
|---|---|---|---|---|---|---|
| Quader → Kante abrunden → Höhe, Breite, Tiefe ändern (5 Maße) | 5 | 5 | | | | 0 |
| Skizze → Extrudieren → Seite anbohren → Skizze um einen Punkt erweitern (an anderer Stelle, in der Seite selbst) | 2 | 2 | | | | 0 |
| Feature davor einfügen (Bohrung, Abrundung, Aufsatz) | 3 | 3 | | | | 0 |
| Rand einer Bohrung abrunden, Bohrung verschieben und vergrößern, Platte dicker machen | 4 | 4 | | | | 0 |
| Skizze neu gezeichnet (Feature ersetzt, neue Segment-Nummern) | 1 | | 1 | | | 0 |

### Böse Fälle (hier ist Raten gefährlich; richtig ist „braucht Hilfe“)

| Fall | Strenger Modus | Großzügiger Modus |
|---|---|---|
| Oberseite wird durch ein breites Loch in zwei Stücke geteilt | Hilfe ✔ | Hilfe ✔ |
| Große Bohrung lässt ein langes Stück und kleine Splitter übrig | Hilfe ✔ | Hilfe ✔ |
| Skizzen-Punkt entfernt, Seitenfläche verschwindet; Kante daran abgerundet | Hilfe ✔ | Hilfe ✔ |
| Aufsatz mit gleichem Grundriss verschluckt die Oberseite | Hilfe ✔ | Hilfe ✔ |
| Bohrung gelöscht, daneben liegt eine gleich große (ihr Rand ist die Falle) | Hilfe ✔ | Rät mit ⚠ (falsch) |
| Skizze gespiegelt (Segment steht auf der Gegenseite) | Hilfe ✔ | Hilfe ✔ |
| Zwei gleiche Bohrungen tauschen die Plätze | Hilfe ✔ | Hilfe ✔ |

### Zufallsprobe (strenger Modus, Mac)

421 zufällige Änderungen in 48 Ketten à 10 Änderungen (Quader: 210, Skizzen-Körper: 211). Die Änderungen sind:
Maße ändern, Skizzenpunkte verschieben/einfügen/entfernen, Bohrung verschieben/vergrößern/Tiefe ändern,
Abrundungsradius ändern, Bohrung oder Abrundung vor etwas einfügen, ein Feature löschen, das Grundfeature durch
ein gleiches mit neuer Nummer ersetzen. Der **Wahrheitswert** kommt aus einer geometrischen Abfrage mit den
Modell-Parametern („die Fläche mit Blickrichtung +Z in Höhe z = H“), nie aus dem Namenssystem. Wo das System
um Hilfe bat und es eine eindeutig richtige Antwort gab, „wählt“ der simulierte Nutzer das Element neu, damit
ein Fall nur einmal zählt.

| Verweis auf … | Verweise | ✔ per Name | ✔ per Ähnlichkeit | Hilfe+ | Hilfe− | FALSCH |
|---|---|---|---|---|---|---|
| Quader: Fläche (für Bohrung) | 463 | 363 | 100 | 0 | 0 | **0** |
| Quader: Kante (abrunden) | 204 | 172 | 32 | 0 | 0 | **0** |
| Quader: Bohrungsrand (abrunden) | 103 | 56 | 10 | 37 | 0 | **0** |
| Skizze: Seitenfläche (für Bohrung) | 370 | 328 | 22 | 19 | 1 | **0** |
| Skizze: obere Kante (abrunden) | 276 | 239 | 17 | 19 | 1 | **0** |
| Skizze: Bohrungsrand (abrunden) | 78 | 53 | 4 | 20 | 1 | **0** |
| **Summe** | **1 494** | **1 211** | **185** | **95** | **3** | **0** |

- **Automatisch richtig:** 1 396 von 1 494 (93,4 %), davon 185 per Ähnlichkeitssuche (nach „Grundfeature
  ersetzt“).
- **Zu Recht Hilfe:** 95 (6,4 %): Das Ziel wurde gelöscht, verbraucht, geteilt oder die Skizzen-Seite hat sich
  um mehr als 45° gedreht.
- **Unnötig Hilfe:** 3 (0,2 %).
- Die Probe läuft in 20–30 s (Debug, Mac).

### Was die Zufallsprobe gefunden hat

Beim ersten Lauf gab es in einer Kette einen **stillen Fehler**, der über drei aufeinanderfolgende Änderungen
bestehen blieb: Der Rand eines Lochs sollte abgerundet werden, war aber schon von einer früheren Abrundung
verbraucht; die neue Kante am Übergang Wand/Rundung sah fast gleich aus und gewann die Ähnlichkeitssuche. Die
handgeschriebenen Szenarien hatten diesen Fall nicht. Gelöst:

1. **Gedächtnis für verbrauchte Elemente („Grabsteine“):** Jeder Körper merkt sich, welche benannten Flächen
   und Kanten ein Feature aufgebraucht hat, samt Aussehen. In der Ähnlichkeitssuche treten sie als
   Konkurrenten an: Sieht der Verweis einem verbrauchten Element mindestens so ähnlich wie jedem lebenden, gibt
   es „braucht Hilfe“. Als verbraucht zählt ein Name, der nach der Operation nicht mehr existiert; die
   Kernel-Historie selbst ist für Kanten nicht verlässlich (Nachbarkanten einer Abrundung werden neu gebaut
   und tauchen in der Historie nicht auf).
2. Ein Verweis, dessen Name auf **mehrere** Stücke passt (geteilt), ist immer ein Raten und wird auch im
   großzügigen Modus nur mit ⚠ angeboten (die Linux-Zufallsprobe hatte das aufgedeckt).

Außerdem eine Plattform-Lehre: Die Standard-Zufallsverteilungen von C++ liefern auf Mac und Linux **andere**
Folgen. Die Probe verwendet deshalb einen eigenen Generator, damit beide Plattformen dieselben Ketten prüfen.

### Wirkung der einzelnen Bausteine

Dieselben 48 Ketten (1 494 Verweise), jeweils mit einer Entscheidung abgeschaltet oder verändert
(`fission_naming_tests "[sensitivity]"`, dauert ca. 2 Minuten, nicht Teil von `scripts/test`):

| Variante | ✔ richtig | Hilfe+ | Hilfe− | FALSCH | w. falsch (⚠) |
|---|---|---|---|---|---|
| **Standard (streng)** | 1 396 | 95 | 3 | **0** | 0 |
| ohne Gedächtnis für verbrauchte Elemente | 1 396 | 87 | 3 | **8** | 0 |
| sehr locker (Ähnlichkeit ≥ 0,45, kein Abstand zum Zweiten nötig) | 1 396 | 73 | 3 | **22** | 0 |
| ohne Erkennung der Nachbarn nach ihrem Aussehen | 1 379 | 95 | 20 | 0 | 0 |
| ohne „klar durch Lage“-Regel | 1 396 | 95 | 3 | 0 | 0 |
| strenger (≥ 0,90 / Abstand 0,30) | 1 396 | 95 | 3 | 0 | 0 |
| sehr streng (≥ 0,97 / Abstand 0,50) | 1 396 | 95 | 3 | 0 | 0 |
| Drehgrenze 25° statt 45° | 1 394 | 95 | 5 | 0 | 0 |
| Drehgrenze 90° | 1 400 | 94 | 0 | 0 | 0 |
| großzügiger Modus, Standardgrenzen | 1 394 | 88 | 3 | 0 | 9 |
| großzügiger Modus, sehr weit (≥ 0,35) | 1 394 | 64 | 3 | 0 | 33 |

Lesehilfe:

- **Gedächtnis für Verbrauchtes und Mindestabstand schützen vor stillen Fehlern.** Ohne sie entstehen 8
  bzw. 22.
- **Die Schwellen sind unkritisch:** Zwischen 0,70 und 0,97 ändert sich nichts. Ein Treffer ist entweder
  praktisch gleich (≈ 1,0) oder gar nicht ähnlich. Das ist gut: Die Entscheidung hängt nicht an einer
  fein abgestimmten Zahl.
- **Die Nachbarn nach ihrem Aussehen zu erkennen** (wenn ihr Name weg ist) bringt 17 zusätzliche Treffer.
- **Die Regel „klar durch Lage“ hat in diesen Ketten keinen messbaren Effekt.** Sie wurde eingeführt, weil ein
  Zwischenstand der Ketten (anderer Zufallsgenerator) sieben unnötige Hilfen zeigte: Der Rand eines Sacklochs
  sieht dem Rand an seinem Boden ähnlich. Sie ist ungefährlich (greift nur, wenn der Sieger genau an der alten
  Stelle liegt und mindestens zehnmal näher als der Zweite), aber in den endgültigen Ketten nicht belegt; in
  AP-2.10 mit echten Features erneut messen und sonst streichen.
- **Der großzügige Modus verschlechtert das Ergebnis:** keine zusätzlichen richtigen Treffer, dafür 9–33 falsche
  mit Warnhinweis.
- **Die 45°-Grenze** kostet 3 unnötige Hilfen gegenüber 90°. 90° wäre dort sicher, ist aber für Fälle wie
  „Spiegeln“ zu weit. Bei 45° bleiben.

### Plattformvergleich

Siehe Abschnitt „Prüfung auf Mac und Linux“ am Ende dieser Datei.

## Vergleich mit OCAF / TNaming (OCCT-eigen) – kleiner Praxistest

Die Szenarien wurden mit `TNaming_Builder` und `TNaming_Selector` nachgebaut, so wie das OCCT-Beispiel
„DNaming“ es macht: Features registrieren ihre Formen und ihre Historie auf Labels, ein Selector hält den
Verweis (`tests/naming/test_ocaf.cpp`).

| Szenario | OCAF/TNaming | Prototyp |
|---|---|---|
| Quader → Kante abrunden → Höhe ändern (3 Höhen) | ✔ 3 von 3 | ✔ |
| Skizze → Extrudieren → Skizze um Punkt erweitern (2 Varianten) | ✔ 2 von 2 | ✔ |
| Fläche teilt sich | liefert beide Stücke als „Compound“ (ehrlich, aber die Anwendung muss das erkennen) | Hilfe |
| Fläche verschwindet | „nicht gelöst“ (ehrlich), **aber nur, nachdem wir die Labels des entfernten Segments selbst löschten**; ohne das lieferte OCAF still eine Fläche (der alte Stand blieb auf dem Label stehen) | Hilfe |
| Zwei gleiche Flächen tauschen Plätze (Skizze gespiegelt) | **still falsch**: folgt der Identität | Hilfe |
| Feature durch neues ersetzt | nicht vorgesehen (keine Ähnlichkeitssuche; neue Labels, alter Verweis bleibt am alten Stand) | ✔ per Ähnlichkeit |

Weitere Beobachtungen:

- **Aufwand:** Jedes Feature muss sein Ergebnis, seine Teilflächen und seine Historie selbst auf Labels
  registrieren (drei Unter-Labels je Operation, weil ein NamedShape nur eine Art Entwicklung trägt). Das ist
  derselbe Gedanke wie in unserem Prototyp, aber mit mehr Pflicht-Buchführung als eigene Schicht.
- **Fehlerquelle Buchführung:** Wer ein entferntes Segment nicht von seinem Label befreit, bekommt still eine
  Fläche des alten Stands. Das ist genau die Art Fehler, die wir ausschließen wollen.
- **Speicherleck in OpenCASCADE 8.0.1:** `TNaming_Builder::Select` (aus `TNaming_Selector::Solve`) verliert
  einige Bytes je Aufruf (im Testlauf gut 1 KB; AddressSanitizer). Für den Test per Suppression abgefangen
  (`tests/naming/lsan.supp`).
- **Zusätzliche Abhängigkeit im Programm:** TKCAF, TKLCAF, TKCDF (mit eigenem Dokument-Baum aus Labels und
  Attributen, der neben unser Dokumentmodell träte; Speichern über OCAF-Formate statt `.fission`). Für den Test
  nur in der Testdatei verlinkt.
- **Fazit:** OCAF ist brauchbar für die einfachen Szenarien, bringt aber doppelte Datenhaltung, kein
  „Ähnlichkeit“, und im Fall „Plätze tauschen“ einen stillen Fehler. Nicht als Grundlage geeignet.

## Vergleich mit FreeCAD 1.0 (nur anhand der Dokumentation, kein Code übernommen)

Quellen: FreeCAD-Quellkopf `ElementMap.h`, Wiki zum Naming-Algorithmus von realthunder, Beschreibung der
1.0-Entwicklung. Nicht selbst ausprobiert.

- **Ansatz:** Jedes Element bekommt einen **zusammengesetzten Namen aus seiner Herkunft**: Name des
  Ausgangselements plus ein Marker des erzeugenden Schritts (z. B. `Edge10;:T1:6`). Die Namen stammen wie bei
  uns aus der Kernel-Historie (`Modified`/`Generated`).
- **Kosten:** Namen wachsen mit der Rezeptlänge; deshalb werden sie gehasht und die Hash-Tabelle muss mit
  gespeichert werden (laut Dokumentation mitunter größer als die Namen). Ändert sich der Algorithmus oder die
  OCCT-Version, ändern sich die Namen: FreeCAD speichert pro Feature eine Versionskennung und rechnet bei
  Abweichung neu.
- **Was in der Dokumentation nicht vorkommt:** eine Ähnlichkeitssuche mit Schwellwert und ein Status
  „braucht Hilfe“ im Sinne unserer Regel 6 (nicht im Code geprüft). Aus Forumsbeiträgen: Das Problem ist
  „gemildert, nicht gelöst“, Abrunden/Fasen machen weiter Schwierigkeiten.
- **Unterschied zu uns:** Unsere Namen sind **kurz und für Menschen lesbar** (`F5/hole/wall`) und hängen nur am
  erzeugenden Feature, nicht an der ganzen Herkunft. Das spart Speicher, erlaubt lesbare Meldungen
  („Rand der Bohrung F5“) und vermeidet die Hash-Tabelle. Dafür muss jedes Feature seine Flächen selbst
  benennen (Vertrag, siehe ADR-0003).

## Was nicht geprüft ist (Grenzen)

- **Nur vier Features und zwei Körperarten.** Nicht dabei: Fasen, Aushöhlen, Spiegeln, Muster, Text,
  Gewinde, Verschieben/Drehen von Teilen, Booleans mit mehreren Körpern, Rundungen mit variablem Radius.
- **Verschmelzen von Flächen** (`ShapeUpgrade_UnifySameDomain` nach Vereinigungen) ist nicht eingebaut: Dort
  müssen zwei Namen auf eine Fläche zeigen. Die Namensliste je Element unterstützt das, es ist aber nicht
  gemessen.
- **Die Wahrheitsregel „Identität folgt dem Feature“** gilt für die Zufallsprobe. Ob ein Mensch bei
  „Skizzenpunkt entfernt, die Seite daneben dreht sich um 40°“ dasselbe meint, ist eine UX-Frage; heute folgt
  der Verweis, ab 45° fragt er nach.
- **Laufzeit der Auflösung nicht gemessen.** Der Prototyp berechnet für die Ähnlichkeitssuche die Signatur
  jedes Kandidaten neu (O(n)); für große Körper (> 1 000 Flächen) ist ein Zwischenspeicher je Körper
  nötig, und die Auflösung gehört in den Hintergrund-Thread (Regel 9).
- **Statistik:** 0 von 1 494 entspricht „höchstens ca. 0,2 % mit 95 % Sicherheit“.
- **Nur drei Kernel-Hilfen:** Die Namen für Primitive werden aus der Lage der Flächen vergeben (Quader);
  Features mit gekrümmten Flächen ohne klare Richtung (Kugel, Torus) brauchen eigene Namensregeln.

## Empfehlung für AP-2.10

1. **Namens-Vertrag je Feature:** Jedes Feature benennt alle erzeugten Flächen (semantisch, mit Feature-Nummer)
   und liefert seine Historie (`Modified/Generated/IsDeleted`). Ein Feature ohne Namensregeln darf nicht
   eingebaut werden (Prüfung in der Testsuite).
2. **Kanten nach ihren zwei Flächen benennen**; Knoten bei Bedarf nach drei Flächen.
3. **Gedächtnis für verbrauchte Elemente** im Körper-Ergebnis mitführen (Namen und Aussehen). Nicht aus der
   Kernel-Historie für Kanten ableiten, sondern aus „Name nicht mehr vorhanden“.
4. **Nur der strenge Modus** (Entscheidung des Product Owners, durch die Messung bestätigt). Keine ⚠-Treffer.
5. **Prüfungen vor dem Übernehmen:** gleiche Art, Drehung ≤ 45°, kein gleich aussehendes Element an der alten
   Stelle; bei geteiltem Element immer Hilfe.
6. **Die Zufallsprobe mit den echten Features erweitern** und als feste Testsuite in der CI halten; die
   Szenarien dieses Spikes sind der Grundstock. Dazu eine „Mutations-Prüfung“ wie in der Tabelle oben: Wird ein
   Baustein abgeschaltet, muss die Suite rot werden.
7. **Meldungskatalog (AP-2.14):** Die Gründe aus `Resolution::reason` (Name wegen Teilung mehrdeutig, an alter
   Stelle steht ein gleiches Element, von Feature F… verbraucht, Seite um mehr als 45° gedreht) sind die
   Grundlage für die Hilfetexte mit Ein-Klick-Lösung.

## Selbst nachvollziehen

```bash
scripts/test                          # alles, mit einer Zeile "Persistente Referenzen: …"
build/<preset>/tests/naming/fission_naming_tests          # Szenarien, böse Fälle, Zufallsprobe
build/<preset>/tests/naming/fission_ocaf_tests            # OCAF-Praxistest
build/<preset>/tests/naming/fission_naming_tests "[sensitivity]"   # Bausteine abschalten (2 Min.)
FISSION_NAMING_CHAINS=100 build/<preset>/tests/naming/fission_naming_tests "[random]"   # mehr Ketten
FISSION_NAMING_VERBOSE=1 …            # jede unnötige Hilfe mit den Punktzahlen der Kandidaten
```

## Prüfung auf Mac und Linux

| | Mac (arm64, Debug) | Linux (x86_64, Debug + AddressSanitizer/UBSan) |
|---|---|---|
| Tests dieses APs | 16 von 16 grün | 16 von 16 grün (gesamter Lauf 81 von 81) |
| Zufallsänderungen (angewendet) | 421 | 419 |
| Verweise geprüft | 1 494 | 1 481 |
| ✔ automatisch richtig | 1 396 (93,4 %) | 1 398 (94,4 %) |
| Hilfe zu Recht | 95 | 79 |
| Hilfe unnötig | 3 | 4 |
| **still falsch** | **0** | **0** |
| Kernel lehnt Operation ab | 7 | 15 |

- **Dieselben Ketten, andere Zahlen:** Die Folge der Zufallsänderungen ist auf beiden Plattformen identisch
  (eigener Generator). Die Zahlen weichen trotzdem ab, weil OpenCASCADE bei einigen Abrundungen auf Linux
  anders entscheidet (auf Linux lehnt der Kernel mehr Operationen ab, ohne dass wir die Ursache untersucht haben). Das
  Ergebnis des Spikes ändert das nicht; es erinnert daran, dass Verhalten des Kernels plattformabhängig ist und
  die Suite auf beiden laufen muss (macht die CI).
- Der OCAF-Praxistest läuft auf beiden mit denselben Ergebnissen. Unter AddressSanitizer meldet er das
  Speicherleck in OpenCASCADE (siehe oben); es ist im Test ausgenommen.
- Die Prüfung auf Ubuntu 24.04 (Zielplattform) macht die CI.
