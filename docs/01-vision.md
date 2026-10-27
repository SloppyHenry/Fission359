# Vision

## In einem Satz

**Fission359 ist ein installierbares 3D-Konstruktionsprogramm für macOS und Linux, mit dem Einsteiger in Minuten ihr erstes Teil bauen und mit dem Fortgeschrittene trotzdem funktionale Teile auf den Hundertstelmillimeter genau konstruieren können.**

Mit dem gleichen Werkzeug soll man eine Wandhalterung mit passgenauen Schraubenlöchern bauen können und genauso einen verspielten Weihnachtsstern mit weich verschmolzenen Zacken und eingraviertem Namen.

---

## Das Problem

| Programm | Stärke | Warum es nicht reicht |
|---|---|---|
| **Tinkercad** | Sofort verständlich, Formen zusammenstecken, „Loch“-Konzept | Läuft im Browser. Ausschneiden ist umständlich (Loch markieren, gruppieren, zum Ändern Gruppe auflösen). Beim ungleichmäßigen Größe-Ändern verziehen sich Rundungen und Löcher. Rundungen haben eine niedrige Auflösung und werden im Druck eckig. Keine Skizzen, kein echtes Abrunden, keine Parameter, kein Verlauf. Man stößt schnell an Grenzen. |
| **Fusion** | Sehr mächtig, präzise, parametrisch | Steile Lernkurve, viele Dialoge, Fachjargon, Timeline-Fehler sind kryptisch, Cloud-Zwang und Konto, Lizenz-Unsicherheit für Hobby-Nutzer, kein Linux |
| **FreeCAD** | Open Source, mächtig, läuft auf Linux | Bedienung schwer, uneinheitliche Workbenches, „Topological Naming“-Probleme |
| **Shapr3D** | Sehr intuitiv, direkt am Modell, präzise | Abo, kein Linux, wenig für Deko/Freiform |
| **Plasticity** | Schnell, kunstvoll, präzise | Auf Künstler zugeschnitten, wenig Druck-/Maker-Fokus, kein Einsteiger-Lernpfad |
| **SketchUp** | Push/Pull ist genial einfach | Mesh-basiert, für 3D-Druck ungenau, oft nicht wasserdicht |
| **OpenSCAD** | Exakt, parametrisch | Man muss programmieren |

**Die Lücke:** Es gibt kein Programm, das
- so zugänglich ist wie Tinkercad,
- so präzise und parametrisch ist wie Fusion,
- spielerische Deko-Werkzeuge mitbringt (weich verschmelzen, Muster, Prägen, Generatoren),
- den 3D-Druck von vornherein mitdenkt (Toleranzen, Schrauben, Druckcheck, Mehrfarbdruck),
- lokal und ohne Konto läuft und **auch unter Linux** zu Hause ist.

---

## Zielgruppen (Personas)

**1. Die Umsteigerin aus Tinkercad – „Lena“, 34**
Hat einen Bambu-Lab-Drucker, macht Geschenke und Deko (Namensschilder, Weihnachtsanhänger, Ausstechformen). Tinkercad ist ihr zu begrenzt, Fusion hat sie nach einer Stunde frustriert geschlossen.
*Braucht:* sofortige Erfolgserlebnisse, Text, Muster, Mehrfarbdruck, Baukästen für Karten, Vasen und Figuren.

**2. Der Heimwerker – „Tom“, 52**
Misst mit dem Messschieber und will Ersatzteile, Halterungen und Adapter bauen, die auf Anhieb passen.
*Braucht:* exakte Maße, Bohrungen für echte Schrauben, Passungen, Wandstärken, „passt das auf mein Druckbett?“.

**3. Die Schülerin – „Mia“, 14**
Will einfach loslegen, spielt gern herum, liest keine Handbücher.
*Braucht:* Missionen bzw. Tutorials im Programm, Fehler, die ihr nicht wehtun, Rückgängig ohne Ende, Spaß.

**4. Der Fusion-Kenner – „Jonas“, 28** *(sekundär)*
Kann CAD, will aber ein schnelles, lokales Werkzeug ohne Cloud, gern auf Linux.
*Braucht:* Tastenkürzel, Skizzen mit Bemaßungen, Parameter, STEP-Austausch und keine Bevormundung.

---

## Kernfähigkeiten

### Präzise (funktionale Teile)
- Exakter Volumenkörper-Kern (B-Rep, wie Fusion/SolidWorks) statt Dreiecksnetz
- Skizzen mit Bemaßungen und Regeln (Constraints), die Einsteigern nicht im Weg stehen
- Ziehen, Drehen, Entlang-Pfad, Übergang; Abrunden, Abschrägen, Aushöhlen
- Bohrungen und Gewinde für echte Schrauben, Muttern und Einschmelzmuttern
- Benannte Maße und Formeln (`Breite = 40 mm`, `Loch = Breite / 4`)
- Jede Operation steht im **Rezept** (Verlauf) und bleibt jederzeit änderbar
- Messen, Schnittansicht, STEP-Import und -Export

### Spielerisch (Deko und Freiform)
- Weich verschmelzen („Knete“): Formen fließen organisch ineinander
- Verdrehen, Biegen, Verjüngen, Wellen
- Text und Logos prägen oder gravieren, auch auf gekrümmten Flächen
- SVG und Bilder importieren (Ausstechformen, Reliefs, Lithophanien)
- Muster: Voronoi, Waben, Rändel, Oberflächenstrukturen
- Generatoren: Stern, Schneeflocke, Christbaumkugel, Vase, Zahnrad, Namensschild
- Kaleidoskop-Modus: einen Arm gestalten, das Programm vervielfältigt ihn radial

### Baukästen statt starrer Vorlagen
- Geführtes Gestalten Schritt für Schritt, z. B. **Weihnachtskarte**: Format → Hintergrund → Motive → Text → Farben → Drucken
- Erste Baukästen: **Karten**, **Vasen und Gefäße**, **Figuren**; danach Christbaumschmuck, Windlichter, Schilder, Ausstechformen, Schmuck, Boxen
- Motiv-Bibliothek mit druckgeprüften 2D-Motiven (mehrfarbig, frei lizenziert)
- Jedes Ergebnis ist ein Einzelstück und ein normales Modell, das man frei weiterbearbeiten kann
- Details: [07-baukaesten.md](07-baukaesten.md)

### Druckbereit
- Druckbett des eigenen Druckers im Arbeitsbereich
- Druckcheck: geschlossen? Wandstärke? Überhänge? Passt aufs Bett?
- Passungs-Assistent (Spiel für Deckel, Stecker, Scharniere)
- Mehrfarbig: Farben pro Körper oder Fläche, Export als 3MF für Bambu Studio, OrcaSlicer und PrusaSlicer
- „Im Slicer öffnen“ mit einem Klick

### Lernen ist eingebaut
- Startbildschirm „Was möchtest du bauen?“ mit Baukästen
- Missionen: geführte Mini-Projekte direkt im Programm
- Hinweiszeile, Tooltip-Karten mit kurzen Animationen, Befehlssuche mit Alltagswörtern („runde Ecken“ führt zu *Abrunden*)

---

## Bewusst NICHT im Umfang (Version 1)

- Browser- oder Cloud-Version, Kontozwang
- CAM/CNC-Werkzeugwege, FEM-Simulation, generatives Design
- Fotorealistisches Rendering-Studio (ein schöner Viewport und Screenshots reichen)
- Technische Zeichnungen (später, siehe Backlog)
- Baugruppen mit Gelenken und Bewegungssimulation (später, siehe Backlog)
- Windows (die Architektur hält es aber offen)

---

## Design-Prinzipien

Diese Prinzipien gelten für jede Entscheidung. Wenn zwei Lösungen technisch gleichwertig sind, gewinnt die, die besser zu diesen Prinzipien passt.

1. **Sehen statt Suchen.** Werkzeuge erscheinen dort, wo man sie braucht: an der Auswahl, am Mauszeiger.
2. **Anfassen statt Dialoge.** Man zieht direkt am Modell. Eingabefelder ergänzen das, sie ersetzen es nicht.
3. **Jede Zahl ist eintippbar.** Präzise Eingabe ist jederzeit möglich, ohne Moduswechsel und mit Formeln und Einheiten.
4. **Alles bleibt änderbar.** Jeder Schritt landet im Rezept, jeder Wert lässt sich nachträglich ändern.
5. **Alltagssprache.** Kein Fachbegriff ohne Erklärung. „Abrunden“ statt „Fillet“, „Aushöhlen“ statt „Shell“.
6. **Fehler sind Hilfen.** Jede Fehlermeldung erklärt, was los ist, zeigt die Stelle und schlägt eine Lösung vor.
7. **Nichts geht verloren.** Autosave, unbegrenztes Rückgängig, Versionsverlauf, Absturz-Wiederherstellung.
8. **Fortschreitende Tiefe.** Das Einfache ist sofort da, das Mächtige einen Klick tiefer („▸ Mehr“). Es gibt keinen getrennten Anfänger- und Profimodus.
9. **Gemacht für echte Dinge.** Drucker, Toleranzen und Normschrauben sind eingebaut.
10. **Spaß ist ein Feature.** Sofortiges Feedback, flüssige Animationen, Generatoren zum Ausprobieren.
11. **Lokal und privat.** Keine Cloud-Pflicht, kein Konto, Telemetrie nur mit Zustimmung. Deine Dateien gehören dir.
12. **Maße ändern verzerrt nie, rund ist rund.** Größenänderungen verändern Maße, nicht die Form: Rundungen und Löcher bleiben rund und exakt. Facetten gibt es nur, wenn man sie als Stil will. (Direkt aus den Tinkercad-Erfahrungen, siehe UX-Konzept §7.)

---

## Erfolgskriterien (messbar)

| Kriterium | Ziel |
|---|---|
| Erstes Teil | Jemand ohne CAD-Erfahrung baut **ohne Anleitung in ≤ 10 Minuten** einen Schlüsselanhänger mit Namen und Loch und exportiert ihn |
| Funktionsteil | Tinkercad-Umsteiger bauen eine **Wandhalterung mit zwei Senkkopf-Schraubenlöchern nach Maß in ≤ 20 Minuten** |
| Genauigkeit | Geometrie ≤ 0,001 mm Abweichung von der Eingabe; STEP-Export exakt; STL/3MF-Sehnenfehler einstellbar (Standard 0,01 mm) |
| Zufriedenheit | System Usability Scale (SUS) **≥ 80** in Nutzertests |
| Profitauglichkeit | Fusion-Nutzer sagen: „Damit könnte ich 80 % meiner Maker-Projekte machen.“ |
| Performance | Start < 3 s, Viewport 60 fps, keine UI-Hänger > 100 ms, typische Neuberechnung < 300 ms |
| Stabilität | Keine Datenverluste; Absturzrate < 1 pro 50 Nutzungsstunden in der Beta |

---

## Ehrliche Einordnung

Hinter Fusion stecken über zehn Jahre Entwicklung mit großen Teams. „So mächtig wie Fusion“ heißt deshalb hier:

- **Die gleiche geometrische Genauigkeit.** Wir nutzen die gleiche Kernel-Klasse (B-Rep mit OpenCASCADE). Dadurch sind Maße exakt und nicht nur angenähert.
- **Die 20 % der Funktionen, die 80 % der Maker-Projekte abdecken.** Die sollen dafür deutlich besser bedienbar sein.
- **Dazu ein eigenes Profil,** das Fusion nicht hat: Deko- und Freiform-Werkzeuge, ein eingebauter Druck-Fokus und Lernen im Programm.

Der realistische erste große Meilenstein ist **Phase 0 bis 2** (siehe [Roadmap](04-roadmap.md)): ein präzises, einsteigerfreundliches Konstruktionsprogramm, das Tinkercad klar übertrifft.
