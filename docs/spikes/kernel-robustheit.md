# Spike AP-0.5: Kernel-Robustheit

**Stand:** 12.10.2026 · **Kernel:** OpenCASCADE 8.0.1, Manifold 3.5.4 ·
**Geprüft auf:** Mac (Apple Silicon, macOS) und Linux-Testrechner B (Debian 13, GCC 14.2, geteilter Rechner,
2 Threads) · **Ergebnis auf beiden Plattformen identisch**, nur die Zeiten unterscheiden sich.

Ziel war zu wissen, wo OpenCASCADE stark und wo es zickig ist, bevor Features darauf gebaut werden.
Dafür laufen 42 typische Maker-Fälle durch den Kernel. Jeder Fall prüft Gültigkeit (`BRepCheck_Analyzer`),
Volumen, Flächenzahl und Laufzeit. Das Volumen wird wenn möglich gegen eine **geschlossene Formel** geprüft,
nicht gegen das Ergebnis des Kernels selbst.

## Kurzfassung

1. **Der Kernel ist für den Alltag solide.** Löcher, Abrunden, Text, Gewinde, STEP und Netz-Export liefern
   richtige Volumina (Abweichung unter 1 : 1 000 000), auch bei Umlauten und bei 100 Löchern in einem Schnitt
   (0,1 s).
2. **Ein Fall liefert still falsche Ergebnisse: Aushöhlen eines abgerundeten Quaders.** Das ist der
   „klassische Problemfall“ aus AP-2.6. Der Kernel meldet *Fertig*, aber das Ergebnis ist ungültig; die
   „Reparatur“ macht es nur gültig, nicht richtig; die andere Verbindungsart gibt den Körper gar nicht
   ausgehöhlt zurück. **Gültigkeit allein beweist also nichts.** Zuverlässig ist nur die Konstruktion
   (großer Körper minus innerer Körper).
3. **Fast-bündige Flächen und winzige Spalten machen Ärger,** sind aber mit einem Fuzzy-Wert beherrschbar.
4. **Die Winkeltoleranz 0,5° aus der Architektur (§ 6) ist zu streng.** Ein Teil mit 20 Löchern wird dadurch
   zu 121 000 Dreiecken und braucht 38–86 Sekunden. Mit 5° sind es 12 000 Dreiecke in unter 0,6 Sekunden,
   bei weiterhin 0,002 mm Abweichung (gefordert: 0,01 mm). **Entscheidung nötig, siehe unten.**
5. **Gewinde per Wendel-Sweep funktioniert** und liefert gültige Körper, braucht aber 0,5–2 s: gehört in
   einen Hintergrund-Thread.

## Ergebnistabelle

✔ = richtig und gültig · ⚠ = gültig, aber mit Besonderheit · ✖ = falsch oder ungültig (bekanntes Problem).
Zeiten: reine Kernel-Rechenzeit, Mac / Linux-Testrechner (der Rechner ist geteilt, die Zeiten schwanken).

### A. Vereinigen und Ausschneiden

| Fall | | Ergebnis | Zeit |
|---|---|---|---|
| A1 Zwei Quader mit gemeinsamer Fläche vereinen | ✔ | Volumen exakt; 10 Flächen, nach „Flächen zusammenfassen“ 6 | 1 / 3 ms |
| A2 Rundes Loch durch eine Platte | ✔ | exakt; 7 Flächen | 1 / 3 ms |
| A3 Loch, das die Außenwand nur berührt | ⚠ | gültig, Volumen exakt, aber 9 statt 7 Flächen: Berührungslinie mit Wandstärke null | 2 / 6 ms |
| A4 Zwei gleich dicke Zylinder übereinander vereinen | ✔ | exakt; 4 Flächen, nach Zusammenfassen 3 | 1 / 2 ms |
| A5 Zylinder minus gleich dicker Zylinder | ✔ | exakt | 6 / 4 ms |
| A6 Lochraster 10 × 10 in einem Schnitt | ✔ | exakt; 106 Flächen | 79 / 299 ms |
| A7 Zwei Würfel, die sich nur an einer Kante berühren | ⚠ | gültig, aber **2 Körper** (nicht verschmolzen) | 1 / 2 ms |
| A8 Winziges Loch (0,05 mm) in 200-mm-Platte | ✔ | exakt (Volumenfehler < 1e-9) | 3 / 5 ms |
| A9 Quader mit 0,00001 mm Überlappung vereinen | ⚠ | gültig, aber **14 statt 6 Flächen** (Splitterflächen) | 2 / 6 ms |
| A9a Wie A9 mit Fuzzy-Wert 0,001 mm | ✔ | 6 Flächen, Volumen exakt | 2 / 4 ms |
| A10 Zwei Quader mit 0,001 mm Spalt vereinen | ⚠ | bleiben **2 Körper**; mit Fuzzy-Wert 0,01 mm wird es 1 Körper | 2 / 5 ms |

### B. Abrunden und Abschrägen

| Fall | | Ergebnis | Zeit |
|---|---|---|---|
| B1 Eine Würfelkante abrunden | ✔ | Volumen stimmt mit Formel auf 1e-6 | 3 / 2 ms |
| B2 Drei Kanten an einer Ecke abrunden | ✔ | 10 Flächen (Eckflicken), gültig | 2 / 6 ms |
| B3 Alle 12 Kanten abrunden | ✔ | 26 Flächen; Volumen stimmt mit der Steiner-Formel überein | 4 / 9 ms |
| B4 Lochkante nach einem Ausschnitt abrunden | ✔ | stimmt mit Formel (Guldin) auf 1e-6 | 2 / 7 ms |
| B5 Radius zu groß für eine dünne Platte | ✔ | Kernel lehnt sauber ab (kein Absturz, kein falsches Ergebnis) | 9 / 23 ms |
| B6 Oberkante eines Zylinders abrunden | ✔ | stimmt mit Formel auf 1e-6 | < 1 / 1 ms |
| B7 Eine Würfelkante abschrägen | ✔ | exakt | 1 / 2 ms |
| B8 Innenecke eines L-Körpers verrunden | ✔ | stimmt mit Formel auf 1e-6 | 3 / 9 ms |

### C. Aushöhlen

| Fall | | Ergebnis | Zeit |
|---|---|---|---|
| C1 Quader mit offener Oberseite aushöhlen | ✔ | exakt | 8 / 6 ms |
| C2 **Abgerundeten Quader** aushöhlen (Standardaufruf) | ✖ | meldet *Fertig*, Körper ist **ungültig** (Volumen 9585 mm³ statt 8219,7) | 5 / 14 ms |
| C2a … danach mit Reparatur (`ShapeFix`) | ✖ | **gültig, aber das Volumen bleibt falsch** (9585 mm³) | 5 / 13 ms |
| C2b … mit anderer Verbindungsart und Toleranzen | ✖ | „Rundung“: ungültig; „Schnitt“: gültig, aber **nicht ausgehöhlt** (Volumen 30712 = der volle Körper) | 74 / 241 ms |
| C2c … durch Konstruktion (Körper minus innerer Körper) | ✔ | gültig, 8219,7 mm³, stimmt mit der Formel überein | 62 / 169 ms |
| C3 Zylinder mit offener Oberseite aushöhlen | ✔ | exakt | 1 / 2 ms |
| C4 Sehr dünne Wand (0,4 mm) | ✔ | exakt | 3 / 9 ms |

### D. Text

| Fall | | Ergebnis | Zeit |
|---|---|---|---|
| D1 Text auf eine Platte prägen („Hallo Welt“) | ✔ | gültig, Volumen = Platte + Text; 137 Flächen | 70 / 157 ms |
| D2 Text in eine Platte schneiden | ✔ | Volumen = Platte − Fläche × Tiefe, auf 1e-6 | 246 / 340 ms |
| D3 Umlaute und ß („Größe äöüß ÄÖÜ“) | ✔ | alle 26 Teilflächen vorhanden (12 Buchstaben, 14 Punkte) | < 1 ms |

### E. Gewinde und Übergänge

| Fall | | Ergebnis | Zeit |
|---|---|---|---|
| E1 Gewindegang als Wendel-Sweep | ✔ | gültig; Volumen stimmt mit der Formel auf 6 Stellen (105,181 mm³) | 6 / 12 ms |
| E2 Schraube: Kern + Gewinde, Enden gekürzt | ✔ | gültig, 1 Körper, 18 Flächen | 554 / 1028 ms |
| E3 Mutter: Sechskant minus Gewindestange | ✔ | gültig, 1 Körper | 1047 / 2086 ms |
| E4 Übergang Kreis → Quadrat | ✔ | gültig; Volumen zwischen Zylinder und Quader | < 1 ms |

### F. Dateien und Export

| Fall | | Ergebnis | Zeit |
|---|---|---|---|
| F1 STEP speichern → laden | ✔ | Volumen, Fläche, Größe auf 1e-9, gleiche Flächenzahl, gültig | 18 / 43 ms |
| F2 Export-Netz: 0,01 mm Abweichung, 5° | ✔ | 12 188 Dreiecke, größte Abweichung 0,0019 mm | 0,2 / 0,6 s |
| F3 Einzelner Zylinder mit 0,001 mm Abweichung | ✔ | 5 756 Dreiecke, Abweichung 0,00006 mm | 0,1 / 0,3 s |
| F4 Wirkung des Winkel-Grenzwerts | ⚠ | siehe Befund 4: 0,5° ist 15- bis 100-mal teurer als 5° | 40 / 93 s (Gesamtlauf aller Stufen) |

### G. Netz-Gegenprobe mit Manifold

| Fall | | Ergebnis | Zeit (exakt / Netz) |
|---|---|---|---|
| G1 Loch durch Platte | ✔ | Volumen weicht um 2 : 100 000 ab | 4 / 2 ms |
| G2 Lochraster 100 Löcher | ✔ | 1,3 : 10 000 Abweichung | 83 ms / 283 ms |
| G3 Zwei Quader mit gemeinsamer Fläche | ✔ | exakt | 1 / 0,1 ms |
| G4 Loch an der Außenwand | ✔ | Abweichung 2 : 100 000 | 4 / 2 ms |
| G5 Überlappung 0,00001 mm | ✔ | exakt (Netz braucht dafür keine Sonderbehandlung) | 2 / 0,2 ms |

## Befunde im Detail

### 1. Aushöhlen eines abgerundeten Quaders (Fall C2): die gefährlichste Stelle

Ein Quader 40 × 40 × 20 mm, alle Kanten mit 4 mm abgerundet, oben offen, Wand 2 mm. Richtig sind
8219,7 mm³ (zwei voneinander unabhängige Wege führen darauf: Konstruktion und Formel).

| Weg | Kernel sagt | Wirklich |
|---|---|---|
| Standardaufruf (`MakeThickSolid`, Rundungs-Verbindung) | *Fertig* | **ungültiger** Körper, 9585 mm³ |
| … danach `ShapeFix` | *Fertig* | **gültig**, aber immer noch 9585 mm³ → „repariert“ ≠ richtig |
| Verbindungsart „Schnitt“ | *Fertig* | **gültig**, aber 30712 mm³: der Körper wurde gar nicht ausgehöhlt |
| Konstruktion (außen minus innen) | *Fertig* | richtig |

Lehre für die App: **Eine Prüfung auf Gültigkeit reicht nicht.** Jedes Ergebnis braucht zusätzlich eine
Plausibilitätsprüfung (hier: Volumen muss deutlich sinken und zur erwarteten Wandstärke passen). Für
Aushöhlen wählt AP-2.6 die Konstruktion über einen inneren Körper für die eigenen Formen, und den
Offset-Algorithmus nur mit Prüfung und ehrlichem „braucht Hilfe“, wenn er scheitert.

### 2. Fast-bündige Flächen und Spalten (A9, A10)

OpenCASCADE rechnet exakt: Eine Überlappung von 0,00001 mm erzeugt Splitterflächen (14 statt 6 Flächen), ein
Spalt von 0,001 mm lässt zwei getrennte Körper. Beides passiert in der Praxis durch getippte Werte wie
`10,001` oder Rundungsfehler. Ein **Fuzzy-Wert** von 0,001 mm (bzw. 0,01 mm für Spalten) beseitigt es, ist aber
bewusst keine Dauereinstellung (er kann echte kleine Details verschlucken).

Nach jedem Vereinigen müssen die Flächen zusammengefasst werden (`ShapeUpgrade_UnifySameDomain`): Sonst sieht
die Flächenliste aus wie ein Flickenteppich (A1: 10 statt 6, A4: 4 statt 3).

### 3. Berührung ohne Volumen (A3, A7)

Ein Loch, das die Wand nur tangential berührt (A3), und zwei Körper, die sich nur an einer Kante berühren
(A7), sind gültig, haben aber eine Wandstärke von null bzw. bleiben zwei Körper. Der Kernel merkt das nicht von
selbst. Das gehört in den Druckcheck („Wand zu dünn“, „Teile hängen nur an einer Kante zusammen“).

### 4. Export-Netz: der Winkel-Grenzwert bestimmt die Kosten (F2, F4, G2)

Für einen Kreis mit 2 mm Radius braucht 0,01 mm Abweichung nur **32 Segmente**, aber 0,5° Winkel
verlangt **720**. Bei kleinen Rundungen entscheidet daher der Winkel, nicht die Abweichung.
Gemessen am Beispielteil (Platte mit Löchern):

| Winkel | Dreiecke (4 Löcher) | Zeit Mac | Größte Abweichung |
|---|---|---|---|
| 20° | 1 104 | 5 ms | 0,0050 mm |
| 10° | 1 536 | 8 ms | 0,0050 mm |
| **5°** | **2 908** | **20 ms** | **0,0019 mm** |
| 2° | 7 228 | 99 ms | 0,0003 mm |
| 0,5° | 28 828 | 1,7 s | 0,00002 mm |

Mit allen 20 Löchern: 0,5° → **121 052 Dreiecke, 38 s (Mac) bzw. 86 s (Linux-Testrechner)**; 5° → 12 188
Dreiecke in 0,2 s bzw. 0,6 s. Für einen Slicer ist die Abweichung entscheidend, nicht der Winkel; 0,0019 mm
liegt weit unter den geforderten 0,01 mm.

Dasselbe trifft die gemischten Booleans (Architektur § 4): Beim Umwandeln in Netze für Manifold wäre 0,5° ebenfalls
unbrauchbar (Lochraster: 0,3 s bei 5°, 10–33 s bei 0,5°).

### 5. Manifold als Gegenprobe

Auf Netzbasis rechnen alle Fälle sauber, auch die kritischen (A3, A9 ohne Sonderbehandlung). Die Volumina
liegen innerhalb 1,3 : 10 000 des exakten Ergebnisses, die Differenz entsteht durch die Facettierung der Löcher.
Bei einfachen ebenen Teilen ist das Netz schneller (0,1 ms gegenüber 1 ms). Beim Lochraster ist der exakte
Schnitt gut dreimal schneller (83 ms gegenüber 283 ms), und die Umwandlung ins Netz ist dabei noch nicht
mitgerechnet. Fazit: exakte Booleans für präzise Teile, Manifold nur für freie Körper und als Rückfallebene.

### 6. Text, Gewinde, STEP

- **Text** (OpenCASCADE-Schriftdarstellung aus einer Schriftdatei): Umlaute und ß funktionieren. Die
  Umrisse bestehen aus vielen kleinen Flächen (137 für „Hallo Welt“). Es braucht eine mitgelieferte Schrift
  (DejaVu Sans ist in `tests/fixtures/fonts/`); Systemschriften wurden nicht geprüft.
- **Gewinde:** Wendel auf einer Zylinderfläche, Profil per Sweep mit fester Binormale. Das Volumen stimmt
  mit der Formel auf 6 Stellen. Vereinigen und Abschneiden an den Enden (E2) und eine Mutter (E3) sind gültig;
  die Zeit (0,5–2 s) verlangt Hintergrund-Berechnung und eine schlichte Vorschau.
- **STEP** ist verlustfrei im Rahmen von 1e-9.

## Empfohlene Standardeinstellungen

| Thema | Empfehlung |
|---|---|
| Nach jeder Operation | Gültigkeit **und** Plausibilität prüfen (Volumen, Flächenzahl, Körperzahl); sonst „braucht Hilfe“ |
| Booleans | exakt (Fuzzy 0), danach Flächen zusammenfassen; bei Splitterflächen oder verdächtigem Spalt einmal mit Fuzzy 0,001 mm wiederholen und es dem Nutzer als „Teile liegen fast bündig, bündig machen?“ anbieten |
| Parallel | `SetRunParallel(false)` gemessen; der Gewinn bei Parallelbetrieb ist noch nicht untersucht |
| Abrunden | Fehlschlag ist sauber; den **maximal möglichen Radius** muss die App selbst bestimmen (Intervallhalbierung), der Kernel nennt ihn nicht |
| Aushöhlen | eigene Formen: Konstruktion mit innerem Körper; beliebige Körper: Offset nur mit Plausibilitätsprüfung |
| Export-Netz | Abweichung **0,01 mm**, Winkel **5°** (statt 0,5°); im Export-Dialog einstellbar |
| Text | Schrift mitliefern; Berechnung im Hintergrund |
| Gewinde | Wendel-Sweep mit fester Binormale; Hintergrund-Thread; für die Anzeige ein vereinfachtes Gewinde |

## Bekannte Problemfälle (Grundlage für den Meldungskatalog, AP-2.14)

| Situation | Was der Kernel tut | Gewünschte Hilfe in der App |
|---|---|---|
| Abgerundeten Körper aushöhlen | still ungültig oder falsch | „Aushöhlen hat nicht geklappt“: anderes Verfahren versuchen, sonst „braucht Hilfe“ |
| Radius zu groß | lehnt ab, ohne Zahl | „Radius zu groß, höchstens <Zahl> mm möglich“, Ein-Klick: auf Maximum setzen |
| Fast bündige Flächen (< 0,001 mm) | Splitterflächen | „Zwei Flächen liegen fast bündig. Bündig machen?“ |
| Spalt unter 0,01 mm | bleiben zwei Körper | „Die Teile berühren sich nicht ganz. Verbinden?“ |
| Berührung nur an Kante oder Linie | zwei Körper oder Wand null | Druckcheck: „Teile hängen nur an einer Kante zusammen“ |
| Schlechte Exporteinstellung (0,5°) | riesige Netze | Export-Dialog zeigt Dreiecksanzahl und Zeit schon vorher |

## Grenzen dieser Untersuchung

- Alle Fälle sind einzeln und klein. **Große Modelle** (hunderte Features) und **fremde STEP-Dateien**
  wurden nicht geprüft.
- Nicht untersucht: variabler Abrundungsradius, Aushöhlen mit mehreren offenen Flächen, Text mit Systemschriften,
  Multi-Thread-Booleans, das Verhalten bei Dateien aus Fusion oder FreeCAD.
- Die Tests nutzen OpenCASCADE direkt (wie die Smoke-Tests); Produktcode geht später durch die Geometrie-Schicht.
- Plausibilitätsprüfungen (Volumen usw.) sind hier Handarbeit je Fall; die App braucht eine allgemeine Form davon.

## Nachvollziehen

```sh
scripts/test                      # enthält alle Fälle (eine Zeile in der Zusammenfassung)

# Ergebnistabelle erzeugen (auf dem Mac heißt das Programm je nach Preset anders):
FISSION_GEOMETRY_REPORT=build/geometry-report.csv build/macos-debug/tests/geometry/fission_geometry_tests
python3 scripts/tools/geometry_report.py build/geometry-report.csv

# zusätzlich die langsamen Vergleichsläufe (0,5°, mehrere Sekunden):
FISSION_GEOMETRY_SLOW=1 FISSION_GEOMETRY_REPORT=... build/macos-debug/tests/geometry/fission_geometry_tests
```

Die Fälle stehen in `tests/geometry/` (A: `test_booleans.cpp`, B und C: `test_fillets.cpp`, D: `test_text.cpp`,
E: `test_sweeps.cpp`, F: `test_io.cpp`, G: `test_manifold.cpp`). Fälle, die ein bekanntes Problem zeigen,
sind mit `[!mayfail]` markiert: Sie halten die CI grün, die Ergebnistabelle zeigt sie als „known problem“.
