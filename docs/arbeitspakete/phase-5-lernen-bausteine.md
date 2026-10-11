# Phase 5 – Lernen, Bausteine und Baukästen

**Ziel:** Neulinge lernen das Programm im Programm. Wiederkehrende Bauteile gibt es als intelligente Bausteine. **Baukästen** führen Schritt für Schritt zu eigenen Karten, Vasen und Figuren, ohne starre Vorlagen (Konzept: [07-baukaesten.md](../07-baukaesten.md)).

**Meilenstein M5 (Demo):** Eine Person ohne CAD-Erfahrung schafft die Missionen 1–3 ohne fremde Hilfe, gestaltet danach im Baukasten **in ≤ 5 Minuten eine eigene zweifarbige Weihnachtskarte** und druckt sie.

---

### AP-5.1 Missions-Engine und erste Missionen
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.12

**Umfang:**
- Missionen als Datendateien (JSON/YAML): Schritte mit Text, Hervorhebung (UI-Element oder 3D-Element), **Bedingung auf dem echten Dokumentzustand** (z. B. `body.count == 1 && box.width ≈ 50`), Hilfe-Animation, Belohnung
- Fortschritt speichern, jederzeit abbrechen oder fortsetzen; Missionsübersicht mit Abzeichen (dezent)
- Missionen 1–5 aus [UX §11](../02-ux-konzept.md#11-lernen-im-programm)
- Mission bricht nicht, wenn Nutzer abweichen. Bei falschem Weg folgt ein sanfter Hinweis, kein Blockieren

**Akzeptanzkriterien:**
- [ ] Eine neue Mission lässt sich ohne Code-Änderung hinzufügen
- [ ] Automatisierter Test spielt jede Mission per Skript durch

---

### AP-5.2 Tooltip-Animationen und „Zeig mir wie“
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.12

**Umfang:**
- Kurze Loop-Animationen (3–5 s) je Werkzeug, erzeugt per Skript direkt aus der App (Aufnahme-Modus), damit sie bei UI-Änderungen neu generiert werden können
- „Zeig mir wie“-Knopf im Eigenschaften-Panel

**Akzeptanzkriterien:**
- [ ] Alle Werkzeuge der Phasen 1–4 haben eine Animation; Gesamtgröße < 30 MB

---

### AP-5.3 Bausteine-Bibliothek
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.9, AP-2.10

**Umfang:**
- Baustein = gespeichertes Teil-Rezept mit freigegebenen Maßen und **Andockpunkten** (z. B. „auf Fläche, Mitte“, „an Kante“)
- Mitgeliefert: Aufhänger-Öse, Schraubenloch-Gruppe, Scharnier (druckbar, Print-in-Place), Schnappverschluss, Kabelclip, Wandhalter-Lasche, Magnettasche, Füße, Griffmulde, Box-Ecken, Gewindeeinsatz, Logo-Platte
- Eigene Bausteine aus einer Auswahl erstellen; als Datei teilen (`.fissionpart`)
- Bibliotheks-Seitenleiste mit Suche und Vorschaubildern; Drag-and-drop mit Andocken

**Akzeptanzkriterien:**
- [ ] Aufhänger-Öse rastet an einer Sternspitze ein (Ablauf C, Schritt 3) und folgt ihr bei Parameteränderungen

---

### AP-5.4 Startbildschirm „Was möchtest du bauen?“
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-5.3, AP-5.8

**Umfang:**
- Kacheln: Karte, Vase/Gefäß, Figur, Christbaumschmuck, Schild, Halterung, Box, Ersatzteil, Leer
- Jede Kachel startet den passenden **Baukasten** (AP-5.8 ff.) bzw. bietet passende Missionen und Bausteine an. Es gibt **keine starren Fertig-Vorlagen**
- Inspirations-Galerie: Beispiele, die mit den Baukästen entstanden sind. „So gemacht“ öffnet den Baukasten mit genau diesen Einstellungen als Startpunkt
- „Meine Kombinationen“ (gespeicherte Baukasten-Startpunkte) und zuletzt geöffnete Dateien mit großen Vorschaubildern

**Akzeptanzkriterien:**
- [ ] Von jeder Kachel ist man in ≤ 2 Klicks im passenden Baukasten oder Werkzeug
- [ ] ≥ 20 Inspirations-Beispiele; jedes druckbar und mit Druckcheck grün

---

### AP-5.5 Materialien, Darstellung und Teilen
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-1.3

**Umfang:**
- Darstellungsmaterialien passend zu Filamenten (PLA matt, Silk, PETG glänzend, transparent, Marmor, Holz-PLA)
- Screenshot (freigestellt, hohe Auflösung), Drehteller-Video/GIF für Social Media

---

### AP-5.6 Barrierefreiheit und Tastatur
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.11

**Umfang:**
- Umsetzung von [UX §14](../02-ux-konzept.md#14-barrierefreiheit); Tastenkürzel-Übersicht (`?`) und frei belegbare Kürzel
- Audit mit VoiceOver (macOS) und Orca (Linux) für die Panels

**Akzeptanzkriterien:**
- [ ] Ablauf A ist ohne Maus machbar (außer der freien Positionierung, die per Zahleneingabe geht)

---

### AP-5.7 Versionsverlauf („Zeitreise“) und Varianten
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.10

**Umfang:**
- Automatische Versionen beim Speichern (in der Datei oder daneben, Größe begrenzt), Liste mit Vorschaubildern, „Diese Version wiederherstellen“ / „Als Kopie öffnen“
- Varianten: gleiche Datei, unterschiedliche Maß-Sätze (z. B. „Klein“, „Groß“), umschaltbar

**Akzeptanzkriterien:**
- [ ] Eine Version von vor 10 Speichervorgängen lässt sich wiederherstellen

---

## Baukästen

Gestaltungsregeln, Schritte und Optionen stehen im Konzept [07-baukaesten.md](../07-baukaesten.md). Die folgenden APs setzen es um.

### AP-5.8 Baukasten-Engine
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.9, AP-5.3

**Umfang:**
- Baukasten-Definition als Datendatei: Schritte → Optionsgruppen → Optionen → Abbildung auf Features, Parameter, Bausteine und Motive; Schema mit Validierung
- Baukasten-Seitenleiste mit Schrittleiste (frei anspringbar), Optionskacheln, „▸ Mehr“, Weiter/Zurück, „Frei weiter“
- **Live-Vorschau-Kacheln** am eigenen Modell: Varianten im Hintergrund berechnen und offscreen rendern, zwischenspeichern, abbrechbar; zuerst Platzhalter, dann Bild
- Ergebnis als **Rezept-Gruppe** mit normalen Features; Gruppe per Doppelklick wieder im Baukasten öffnen
- Erkennung „von dir angepasst“: Frei geänderte Schritte werden markiert und nur auf Nachfrage überschrieben
- „Überrasch mich“ (für einen Schritt oder alle, Schloss je Schritt, reproduzierbar per Seed)
- „Meine Kombinationen“ speichern und laden
- Druckerprofil-Bewusstsein: Optionen, die nicht aufs Bett oder zur Düse passen, sind erklärt statt versteckt

**Akzeptanzkriterien:**
- [ ] Ein neuer Test-Baukasten lässt sich ohne Code-Änderung hinzufügen
- [ ] Baukasten verlassen → frei weiterarbeiten → Baukasten wieder öffnen: Die eigenen Änderungen bleiben erhalten
- [ ] Vorschau-Kacheln blockieren nie die UI; die erste Kachel erscheint in < 1 s

---

### AP-5.9 Motiv-Bibliothek
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-4.4, AP-4.5

**Umfang:**
- Motivformat: SVG mit benannten Ebenen + Metadaten (Kategorie, Schlagwörter DE/EN, Autor, Quelle, Lizenz, Mindestgröße)
- Bibliotheks-Seitenleiste mit Suche (Synonyme), Kategorien und Favoriten; eigene Motive importieren (SVG, nachgezeichnete Bilder)
- Platzieren per Drag-and-drop auf Flächen (eben und gekrümmt): erhaben, vertieft, durchbrochen, aufgesetzt; Höhe und Farbe je Ebene; proportionales Skalieren, Drehen, Spiegeln
- **Streuen** (Dichte, Größenvariation, Drehung, Mindestabstand, Seed, Randabstand)
- Druckbarkeits-Prüfung je Motivgröße (Linienbreite ≥ 2 × Düse) mit Hinweis auf die Mindestgröße
- **Start-Umfang:** ≥ 150 Motive (≥ 50 Weihnachten/Winter), selbst erstellt oder CC0/CC-BY-4.0; Lizenz-Prüfskript in der CI

**Akzeptanzkriterien:**
- [ ] Jedes mitgelieferte Motiv ist bei seiner Mindestgröße druckbar (automatischer Test)
- [ ] Jede Motivdatei hat vollständige Lizenz- und Herkunftsangaben (CI-Prüfung)

---

### AP-5.10 Baukasten „Karten“
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-5.8, AP-5.9, AP-3.4, AP-4.7

**Umfang:**
- Schritte ① Format · ② Hintergrund · ③ Motive · ④ Text · ⑤ Feinschliff · ⑥ Drucken gemäß [07-baukaesten.md](../07-baukaesten.md#baukasten-1-karten)
- Kartenarten: flach, Klappkarte (druckbares Scharnier oder Steckverbindung), Aufstellkarte, Lichtkarte (Lithophanie, AP-4.6), Mini-Geschenkanhänger
- Textvorschläge DE/EN; Text automatisch in Flächen einpassen
- **Farbwechsel nach Höhe** für Drucker mit einer Düse (über AP-3.4) sowie Mehrfarbe je Motiv-Ebene
- **Versand-Check** (Umschlagformate, Dicke, Gewicht aus Material und Füllung) mit Länderdaten als Datei, zuerst Deutsche Post

**Akzeptanzkriterien:**
- [ ] Meilenstein-Demo: zweifarbige Weihnachtskarte in ≤ 5 min durch eine Person ohne CAD-Erfahrung
- [ ] Gedruckte Klappkarte lässt sich ≥ 50-mal öffnen und schließen, ohne zu brechen (PLA, Referenzdrucker)
- [ ] Farbwechsel nach Höhe kommt korrekt in PrusaSlicer und Bambu Studio/OrcaSlicer an (oder es gibt eine dokumentierte Anleitung mit Höhenangaben)

---

### AP-5.11 Baukasten „Vasen und Gefäße“
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-5.8, AP-4.8, AP-4.3, AP-4.7

**Umfang:**
- Schritte ① Zweck und Größe · ② Silhouette · ③ Querschnitt · ④ Struktur · ⑤ Rand und Boden · ⑥ Drucken gemäß [07-baukaesten.md](../07-baukaesten.md#baukasten-2-vasen-und-gefäße)
- Zweck-Voreinstellungen (Blumenvase wasserdicht, Deko, Windlicht, Stiftebecher, Übertopf, Schale)
- Profilkurve direkt in der 3D-Ansicht mit Griffen bearbeiten; Überhänge live markiert
- Querschnitt-Übergang unten → oben; Motive als umlaufendes Relief
- **Vasenmodus-Prüfung** (eine Wand, keine Inseln, keine waagerechten Dächer) und passende Export-Hinweise

**Akzeptanzkriterien:**
- [ ] Gedrehte Sternvase im Vasenmodus druckt ohne Stützen (Foto im Abschlussvermerk)
- [ ] Blumenvase „wasserdicht“ hält 24 h Wasser (Referenzdrucker, Standardprofil)

---

### AP-5.12 Baukasten „Figuren“
**Status:** offen · **Größe:** XL · **Abhängigkeiten:** AP-5.8, AP-4.2, AP-5.3, AP-3.6, AP-3.7

> Vor dem Start zerlegen (Figuren-Gerüst-Format, Proportions-Regler, Gesichter/Zubehör, Sockel und Standfestigkeit, automatisches Zerlegen).

**Umfang:**
- Schritte ① Grundfigur · ② Proportionen · ③ Gesicht · ④ Zubehör · ⑤ Sockel und Zweck · ⑥ Drucken gemäß [07-baukaesten.md](../07-baukaesten.md#baukasten-3-figuren)
- Figuren-Gerüst als Daten: Körperteile (parametrische SDF-Formen), Verschmelz-Weichheit, Regler → Parameter, Andockpunkte für Gesicht und Zubehör
- Start-Figuren: Schneemann, Wichtel, Engel, Rentier, Pinguin, Eule, Katze, Roboter; dazu „eigene Figur aus Formen“
- Gesicht und Zubehör geprägt oder als farbige Einsetzteile mit Passung
- Zwecke: Aufsteller, Christbaumanhänger (Öse am Schwerpunkt), Magnet, Platzkarte
- **Standfestigkeits-Check** (Schwerpunkt über Standfläche) mit Lösungsvorschlag
- Automatisches Zerlegen in druckbare Teile mit Steckzapfen für Mehrfarbe und Stützfreiheit

**Akzeptanzkriterien:**
- [ ] Jede Start-Figur ist über den gesamten Reglerbereich druckbar und standfest (Property-Tests)
- [ ] Ein Wichtel mit Mütze in anderer Farbe, gesteckt, passt gedruckt ohne Nacharbeit

---

### AP-5.13 Weitere Baukästen
**Status:** offen · **Größe:** M je Baukasten · **Abhängigkeiten:** AP-5.8, AP-5.9

**Umfang:**
- Reihenfolge nach Nutzerfeedback aus den Baukästen 1–3: Christbaumschmuck und Anhänger, Windlicht und Lampenschirm, Namens- und Türschilder, Ausstechformen und Stempel, Schmuck, Untersetzer und Schalen, Boxen (siehe [07-baukaesten.md](../07-baukaesten.md#weitere-baukästen-nach-den-ersten-drei))
- Jeder neue Baukasten ist reine Datenarbeit (plus ggf. fehlende Generatoren oder Bausteine als eigene APs)

