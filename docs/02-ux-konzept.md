# UX-Konzept

Dieses Dokument beschreibt, **wie sich Fission3D anfühlt**. Es ist verbindlich für alle UI-Arbeitspakete. Wo dieses Dokument und eine technische Bequemlichkeit kollidieren, gewinnt dieses Dokument. Ausnahmen werden per ADR begründet.

---

## 1. Bildschirmaufbau

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ Fission3D — Schlüsselanhänger.fission                       ⌘K  Was suchst du?│
├───────────────────────────────────────────────────────────────────────────────┤
│ Formen   Skizze   Ändern   Kombinieren   Muster   Deko   Prüfen   Export       │
├─────────────┬─────────────────────────────────────────────────┬───────────────┤
│ REZEPT      │                                          ┌────┐ │ EIGENSCHAFTEN │
│ ① Quader    │                                          │OBEN│ │ Quader        │
│   50×20×4   │                                          └────┘ │ Breite  50 mm │
│ ② Rundung   │              3D-ANSICHT                         │ Tiefe   20 mm │
│   4 Kanten  │                                                 │ Höhe     4 mm │
│ ③ Text LENA │          ┌───────────────────────┐              │ Ecken    0 mm │
│ ④ Loch Ø5 ⚠ │          │ Ziehen Abrunden Loch …│ ← Kontext-   │ Farbe   ● ▾   │
│ ── jetzt ── │          └───────────────────────┘   leiste     │ ▸ Mehr        │
│             │                                                 │               │
│ KÖRPER      │            (Druckbett 256 × 256 mm)             │               │
│ ● Anhänger  │                                                 │               │
│ ● Schrift   │                                                 │               │
├─────────────┴─────────────────────────────────────────────────┴───────────────┤
│ Tipp: Ziehe an der Fläche, um sie zu verlängern – oder tippe einfach eine Zahl.│
└───────────────────────────────────────────────────────────────────────────────┘
```

> **Verbindliche Gestaltung (R17, 2026-10-13):** Aussehen und Anordnung legt das Design-System
> [08-gestaltung.md](08-gestaltung.md) fest, Vorlage ist das Mockup [mockups/oberflaeche.html](mockups/oberflaeche.html).
> Die Skizze oben zeigt nur die Aufteilung. Abweichend davon stehen die Werkzeuggruppen als Palette am linken
> Rand der 3D-Ansicht, das Rezept steht unter der Körperliste.

- **3D-Ansicht dominiert.** Die Seitenleisten sind schmal und einklappbar. Auf einem 13"-Laptop hat die 3D-Ansicht mindestens 65 % der Fläche, spätestens im **Fokus-Modus** (beide Seitenleisten aus, Knopf am Ansichtswürfel oder ⇧F).
- **Werkzeuggruppen oben:** höchstens 8 Gruppen mit Alltagsnamen. Ein Klick öffnet eine große, bebilderte Auswahl, keine Liste aus Mini-Icons.
- **Rezept links:** der Verlauf als lesbare Sätze (siehe Abschnitt 4). Darunter die Liste der Körper mit Sichtbarkeit und Farbe.
- **Eigenschaften rechts:** zeigt immer, *was ausgewählt ist* und *was man daran ändern kann*. Selten Gebrauchtes steckt hinter „▸ Mehr“.
- **Hinweiszeile unten:** sagt jederzeit in einem Satz, was man gerade tun kann.
- **Befehlssuche (⌘K / Strg+K):** findet Werkzeuge über Alltagswörter und Synonyme.

---

## 2. Interaktionsmodell

### 2.1 Auswahl zuerst, Werkzeug zuerst: beides geht
- **Auswahl → Aktion:** Fläche, Kante oder Körper anklicken. Eine **Kontextleiste** erscheint daneben, mit nur den Aktionen, die dafür Sinn ergeben (bei einer Fläche: Ziehen, Loch, Aushöhlen, Skizze darauf, Farbe …).
- **Werkzeug → Auswahl:** Werkzeug wählen. Die Hinweiszeile sagt, was als Nächstes anzuklicken ist. Passende Elemente leuchten beim Überfahren auf, unpassende reagieren nicht.

### 2.2 Direkte Griffe: anfassen, ohne ein Werkzeug zu suchen
| Man überfährt … | Es erscheint … | Ziehen bewirkt … |
|---|---|---|
| eine **ebene Fläche** | ein Pfeil in Normalenrichtung | Fläche herausziehen oder hineindrücken (Push/Pull) |
| eine **Skizzenfläche** | ein Pfeil | Körper hochziehen (Extrudieren); Neu, Vereinen oder Ausschneiden erkennt das Programm automatisch |
| eine **Kante** | ein kleiner Viertelkreis-Griff | Abrunden. Mit gedrückter ⌥/Alt-Taste: Abschrägen |
| einen **Körper** (ausgewählt) | Verschiebe-Pfeile, Drehringe, Skalier-Würfel | Verschieben, Drehen, Skalieren |
| eine **Grundform** | Maß-Griffe an Ecken und oben (wie Tinkercad) | Maße ändern |
| eine **Bohrung** | Durchmesser- und Tiefen-Griff | Bohrung anpassen |

Während des Ziehens zeigt eine **Wertblase** am Zeiger den aktuellen Wert.

**Griff-Art (Einstellung, R19):** Wie die Griffe aussehen, stellt man unter Einstellungen ▸ Griffe ein.
- **Achsen:** farbige Pfeile X/Y/Z zum Verschieben, Maßbänder entlang der Kanten (Klick oder Tippen ändert das Maß), Punkte auf den Flächen zum Strecken.
- **Klassisch:** weiße Eck-Quadrate, Hebe-Pfeil und Maßkästchen, vertraut für Tinkercad-Umsteiger.

Beide Arten lösen dieselben Commands aus und folgen denselben Regeln: Tippen ersetzt Ziehen, Maße ändern verzerrt nie, Rundungen und Löcher bleiben rund (§7). Die Wahl ändert nur die Darstellung, nie das Ergebnis. Voreingestellt ist **Achsen**.

### 2.3 Präzision ohne Reibung
- **Tippen ersetzt Ziehen.** Wer während eines Griffs eine Zahl tippt, ersetzt damit den Wert. `Enter` bestätigt, `Esc` bricht ab, `Tab` springt zum nächsten Maß (Breite → Tiefe → Höhe).
- **Formeln und Einheiten überall:** `25+3`, `40/2`, `1 in`, `2,5 cm`, `Breite/2`. **Komma und Punkt** gelten beide als Dezimaltrenner.
- **Intelligente Abstandsmaße:** Beim Verschieben zeigt das Programm automatisch die Abstände zu den nächsten Kanten an. Ein Klick auf ein Maß macht es editierbar („5 mm vom Rand“). Das ist die Verbesserung des Tinkercad-Lineals.
- **Fangen (Snapping):** Raster (fein, mit dem Zoom angepasst), Ecken, Kantenmitten, Kreismittelpunkte, Flächenmitten, Winkel in 15°-Schritten, Ausrichtungslinien. Gedrückte ⇧/Shift-Taste schaltet das Fangen vorübergehend aus.
- **Auf Fläche legen:** Neue Formen landen automatisch auf der Fläche unter dem Mauszeiger, die dabei zur Arbeitsebene wird (das macht Tinkercad gut).
- **Ausrichten:** mehrere Körper auswählen und mit Griffen bündig oder mittig ausrichten.

### 2.4 Vorschau ist immer live
Jede Änderung zeigt sofort das Ergebnis. Bei teuren Operationen erscheint zuerst eine schnelle Näherung (halbtransparent), dann das exakte Ergebnis. Die Oberfläche friert nie ein. Lange Berechnungen zeigen einen Fortschritt und lassen sich abbrechen.

---

## 3. Formen und Werkzeuggruppen

| Gruppe | Inhalt (Beispiele) |
|---|---|
| **Formen** | Quader, Zylinder, Kugel, Kegel, Torus, Röhre, Prisma, Pyramide, Text, Stern; „Als Loch“-Schalter bei jeder Form; **Baukästen** (Karte, Vase, Figur …) |
| **Skizze** | Linie, Rechteck, Kreis, Bogen, Langloch, Polygon, Kurve (Spline), Text; Maß, Regeln, Trimmen, Versetzen |
| **Ändern** | Ziehen, Drehen (Drehkörper), Entlang Pfad, Übergang, Abrunden, Abschrägen, Aushöhlen, Fläche verschieben, Fläche entfernen |
| **Kombinieren** | Vereinen, Ausschneiden, Überlappung behalten, Teilen, Weich verschmelzen |
| **Muster** | Reihe, Kreis, Entlang Pfad, Spiegeln, Kaleidoskop |
| **Deko** | Prägen, Gravieren, Verdrehen, Biegen, Verjüngen, Strukturen, Relief aus Bild, Generatoren |
| **Prüfen** | Messen, Schnitt, Druckcheck, Gewicht und Material |
| **Export** | Speichern als 3MF/STL/STEP, Im Slicer öffnen, Bild teilen |

**Bausteine** (parametrische Fertigteile wie Box mit Deckel, Scharnier, Schnappverschluss, Aufhänger-Öse, Zahnrad, Schraubenloch) öffnen sich über eine eigene Bibliothek-Seitenleiste, ebenso die **Motiv-Bibliothek** (2D-Motive für Karten, Reliefs, Schilder).

**Baukästen** führen Schritt für Schritt zu einem eigenen Objekt (z. B. Karte: Format → Hintergrund → Motive → Text → Feinschliff → Drucken), ohne starre Vorlage. Konzept und Bedienung: [07-baukaesten.md](07-baukaesten.md).

---

## 4. Das Rezept (Verlauf)

Das Rezept ist Fusions Timeline, nur verständlich.

```
REZEPT
 ① Quader 50 × 20 × 4 mm
 ② 4 Kanten abgerundet · 4 mm
 ③ Text „LENA“ geprägt · 1,5 mm
 ④ ⚠ Loch Ø 5 mm durchgehend      ← braucht Hilfe
 ─── jetzt ───                      ← verschiebbarer Zeitmarker
 ⑤ (ausgeblendet) Fase 0,5 mm
```

- **Jeder Eintrag ist ein Satz** mit Symbol, den wichtigsten Werten und auf Wunsch einem Mini-Vorschaubild.
- **Überfahren** hebt die betroffene Geometrie hervor, **Klicken** öffnet die Bearbeitung mit denselben Griffen wie beim Erstellen.
- **Zeitmarker:** Man zieht ihn zurück, um das Modell in einem früheren Zustand zu sehen und dort etwas einzufügen.
- **Ausblenden** (statt Löschen) macht Schritte vorübergehend unwirksam.
- **Gruppen:** Mehrere Schritte lassen sich zu „Schraubenlöcher“ zusammenfassen und benennen.
- **Status:** grün = ok, gelb ⚠ = braucht Hilfe (z. B. Bezug verloren), rot = geht nicht. Ein Klick erklärt das Problem (siehe Abschnitt 9).
- **Umsortieren** per Ziehen ist erlaubt, wenn die Abhängigkeiten es zulassen. Sonst erklärt das Programm, warum nicht.

### Benannte Maße
Ein Bereich „Maße“ (ausklappbar unter dem Rezept, oder `⌘M`) zeigt eine Tabelle:

| Name | Wert | Verwendet in |
|---|---|---|
| Breite | 50 mm | ①, ④ |
| Lochabstand | Breite − 10 mm | ④ |

- In jedes Zahlenfeld kann man `Breite` schreiben, mit Autovervollständigung.
- **Schieberegler-Modus:** Maße lassen sich als Regler anzeigen. So wird jedes Modell zum Konfigurator („Mach den Anhänger breiter“).

---

## 5. Skizzieren für Einsteiger

Skizzen mit Regeln (Constraints) sind der mächtigste und zugleich abschreckendste Teil von Fusion. Unsere Antworten darauf:

1. **Skizze auf Klick:** „Skizze“ wählen und auf eine Fläche oder die Grundebene klicken. Die Ansicht dreht sich animiert senkrecht darauf.
2. **Regeln entstehen automatisch** beim Zeichnen: waagerecht, senkrecht, Endpunkt-auf-Endpunkt, tangential. Sie erscheinen als dezente Symbole und lassen sich per Klick entfernen.
3. **Maße durch Klicken und Tippen:** Linie anklicken, Zahl tippen, fertig. Es gibt keinen separaten Bemaßungsdialog.
4. **Statusfarben mit Worten:**
   - Blau = „kann sich noch bewegen“. Zieht man daran, sieht man, was noch frei ist.
   - Dunkel = „steht fest“.
   - Die Hinweiszeile sagt: „Diese Skizze kann sich noch in 2 Richtungen bewegen. Tipp: Gib der linken Linie ein Maß.“
   - **Wichtig:** Eine nicht voll bestimmte Skizze ist **kein Fehler**. Sie wird nie rot markiert.
5. **Widersprüche werden erklärt**, nicht nur markiert: „Diese zwei Regeln widersprechen sich: Linie A soll 30 mm lang sein, aber auch genauso lang wie Linie B (40 mm). [Längenmaß entfernen] [Gleich-lang-Regel entfernen]“
6. **Geschlossene Flächen werden automatisch erkannt** und schattiert. Darauf erscheint sofort der Zieh-Pfeil.
7. **Formen auch in Skizzen:** Rechteck, Kreis, Polygon und Langloch sind mit einem Klick da und nicht nur Linien.

---

## 6. Präzise Körper und freie Formen: ein Modell

Intern gibt es zwei Arten von Körpern:
- **Präzise** (B-Rep): exakte Flächen, ideal für Funktionsteile, STEP-fähig.
- **Frei** (Netz oder Feld): für weiches Verschmelzen, Verformen, Strukturen und Reliefs.

**Für Nutzer ist das ein Modell:**
- Man kann Deko-Operationen auf präzise Körper anwenden. Das Ergebnis ist dann ein freier Körper, gekennzeichnet durch ein kleines Badge „frei geformt“ mit Erklärung beim Überfahren.
- **Das Rezept bleibt trotzdem durchgängig änderbar.** Ändert man oben den Lochdurchmesser, wird das weiche Verschmelzen danach neu berechnet.
- Vorgänge, die bei freien Körpern nicht gehen (z. B. eine exakte Kante abrunden), sind nicht versteckt, sondern ausgegraut und erklärt: „Abrunden braucht exakte Kanten. Dieser Körper ist frei geformt. Tipp: Runde die Kante *vor* dem Verschmelzen ab (Schritt ③).“
- STL- und 3MF-Export funktionieren für alles. STEP exportiert freie Körper als Netz und sagt das vorher.

---

## 7. Tinkercad: was wir übernehmen und was wir besser machen

**Übernehmen:**
- Formen per Drag-and-drop aufs Bett ziehen, die sofort auf der Fläche darunter landen
- **„Als Loch“-Schalter** an jeder Form: wird intern zu „Ausschneiden“ im Rezept
- Weiße Maß-Griffe an Grundformen
- Sofortiges, spielerisches Gefühl; freundliche Farben

### Die vier Tinkercad-Ärgernisse aus echtem Nutzer-Feedback

Diese Punkte sind **verbindlich**. Ablauf D (Abschnitt 15) prüft sie als Regressionstest.

#### 7.1 Ausschneiden ist umständlich
**Tinkercad:** Form als „Loch“ markieren, Loch und Körper zusammen auswählen, „Gruppieren“. Vorher sieht man keine Wirkung. Danach steckt das Loch in der Gruppe, und um es zu ändern, muss man die Gruppe wieder auflösen.

**Fission3D:**
- **Sofort sichtbar:** Sobald eine Form „Als Loch“ markiert ist und einen Körper berührt, ist sie live ausgeschnitten. Es gibt **keinen Gruppieren-Schritt**.
- **Klares Ziel:** Ein Etikett am Loch zeigt „schneidet aus: Anhänger“. Das Ziel ist automatisch der Körper, auf dem das Loch abgelegt wurde. Ein Klick auf das Etikett ändert das Ziel, auch auf mehrere Körper.
- **Das Loch bleibt greifbar:** Es bleibt als halbtransparente Geisterform anklickbar. Verschieben, Größe ändern und Löschen gehen jederzeit, das Ergebnis aktualisiert sich live.
- **„Durchgehend“ ist Standard** für Löcher. Man muss die Tiefe nicht raten, und das Loch geht auch dann ganz durch, wenn der Körper später dicker wird.
- **Weitere Wege führen zum gleichen Ergebnis:** auf eine Fläche skizzieren und mit dem Pfeil hineindrücken (Ziehen erkennt „Ausschneiden“ automatisch), Bohrungs-Assistent für Schrauben, Werkzeug „Ausschneiden“ für beliebige Körper.

#### 7.2 Rundungen verziehen sich beim Größe-Ändern
**Tinkercad:** Ändert man ein Objekt ungleichmäßig in der Größe, wird alles mitgestreckt. Aus einer runden Ecke wird eine Ellipse, aus einem runden Loch ein Oval.

**Fission3D: „Maße ändern verzerrt nie.“**
- **Griffe ändern Maße, nicht die Skalierung.** Ein Quader mit 3-mm-Ecken wird breiter, die Ecken bleiben 3-mm-Viertelkreise. Löcher bleiben rund und behalten ihren Durchmesser.
- **Das gilt auch für fertige, kombinierte Körper.** Zieht man an einer Seitenfläche, wird der Körper **gestreckt** (Fläche verschoben bzw. „Intelligentes Strecken“: Körper an einer Ebene teilen, Mittelstück verlängern), nicht skaliert. Rundungen, Löcher und Text bleiben unverändert.
- **Echtes Verzerren gibt es nur auf ausdrücklichen Wunsch** (Ändern ▸ Mehr ▸ „Verzerren“), mit Hinweis: „Rundungen und Löcher werden dabei oval. Möchtest du stattdessen strecken?“
- **Proportionales Vergrößern** (alles gleichmäßig) bleibt einfach erreichbar: Eck-Griff mit ⇧ oder Prozentangabe.

#### 7.3 Niedrige Auflösung
**Tinkercad:** Rundungen bestehen aus wenigen Segmenten, und runde Löcher werden im Druck eckig.

**Fission3D: „Rund ist rund.“**
- Präzise Körper sind mathematisch exakt, ein Kreis ist ein Kreis. Es gibt **keinen „Seiten“-Regler**.
- Die Ansicht verfeinert die Darstellung automatisch beim Hineinzoomen. Auch aus der Nähe sind keine Facetten sichtbar.
- Beim Export weicht das Modell standardmäßig höchstens 0,01 mm ab, das ist feiner als jede Düse. Der Wert ist einstellbar.
- Freie Körper (Verschmelzen, Strukturen) wählen ihre Auflösung automatisch passend zu Düse und Größe. Ein Regler „Feinheit“ steckt unter ▸ Mehr.
- **Eckig nur, wenn gewollt:** „Low-Poly“ gibt es als Deko-Stil (Prisma mit n Seiten, Low-Poly-Verformer).

#### 7.4 Zu wenige Funktionen
**Tinkercad:** Es gibt keine Skizzen, keine Maße mit Bezügen, kein Abrunden beliebiger Kanten, keine Parameter, keinen Verlauf und keine Gewinde.

**Fission3D:** Das ist der Kern von Phase 2 (Präzision) und Phase 4 (Spielwiese). Alles ist so verpackt, dass man beim Einstieg nichts davon sehen muss (fortschreitende Tiefe).

**Außerdem besser:** exakte Abstände statt Augenmaß, präzise Bohrungen, kein Browser, offline, schnell.

---

## 8. Was wir von Fusion übernehmen und was nicht

**Übernehmen:**
- Parametrischer Verlauf, Skizzen mit Regeln, benannte Parameter
- Push/Pull, Abrunden, Aushöhlen, Muster, Bohrungs-Assistent
- ViewCube, Schnittansicht, Messen
- STEP-Austausch

**Weglassen oder vereinfachen:**
- Komponenten/Körper/Ursprünge-Hierarchie → nur **Körper**. Gruppen gibt es optional, Baugruppen kommen später.
- Modale Dialoge mit 15 Feldern → Griffe plus schlanke Eigenschaftsleiste
- Kryptische Timeline-Fehler → erklärte Fehler mit Lösungsvorschlag
- Workspaces (Design/Render/Manufacture …) → ein Arbeitsbereich
- Cloud und Konto → lokal

---

## 9. Fehler und Hinweise

**Regeln:**
1. Geometrie-Fehler öffnen **nie** einen modalen Dialog.
2. Jede Meldung beantwortet drei Fragen: **Was ist passiert? Wo? Was kann ich tun?**
3. Wenn möglich, gibt es **Ein-Klick-Lösungen**.
4. Das Modell zeigt den letzten gültigen Zustand. Das gescheiterte Ergebnis erscheint als rote Geistergeometrie.

**Beispiele:**

> **Abrunden mit 5 mm geht hier nicht.**
> Die Kante grenzt an eine Fläche, die nur 3,2 mm breit ist. Die Rundung passt dort nicht hinein.
> [Größtmöglichen Wert verwenden (3,1 mm)] [Diese Kante weglassen] [Wert ändern]

> **Das Loch hat seinen Bezug verloren.**
> Es saß auf einer Fläche, die es nach deiner Änderung in Schritt ② nicht mehr gibt.
> [Neue Fläche wählen] (die alte Position ist als Geist eingezeichnet)

> **Die Wand ist hier nur 0,3 mm dick.**
> Dein Drucker (0,4-mm-Düse) kann Wände unter 0,8 mm nicht zuverlässig drucken.
> [Stelle zeigen] [Aushöhlen-Stärke auf 1,2 mm setzen]

Die Texte stammen aus einem **Meldungskatalog** (siehe AP-2.14). Technische Kernel-Fehler werden darin auf verständliche Ursachen abgebildet.

---

## 10. Navigation und Eingabegeräte

- **Maus (Standard):** Rechte Taste ziehen = drehen, mittlere Taste ziehen = verschieben, Rad = Zoom zum Zeiger.
- **Voreinstellungen** für Umsteiger: „wie Fusion“, „wie Tinkercad“, „wie Blender“, „wie SketchUp“. Das wird beim ersten Start abgefragt.
- **Trackpad (macOS):** zwei Finger = drehen, Shift + zwei Finger = verschieben, Pinch = Zoom. Smart Zoom (Doppeltipp) zentriert auf die Fläche.
- **ViewCube** oben rechts: klickbar, mit animierten Übergängen.
- **Doppelklick auf eine Fläche:** Die Ansicht dreht sich senkrecht auf die Fläche.
- `F` = alles zeigen, `⇧F` = Fokus-Modus (Seitenleisten aus und wieder an), `Pos1`/`Home` = Startansicht, `1–7` = Standardansichten, `P` = Perspektive/Orthografisch.
- **3D-Maus (SpaceMouse):** später (Backlog).

---

## 11. Lernen im Programm

- **Erster Start:** Sprache, Drucker auswählen (bestimmt die Druckbettgröße), Maus-Stil, dann „Bist du neu hier? [Mission starten] [Einfach loslegen]“.
- **Startbildschirm „Was möchtest du bauen?“:** Kacheln für Karte, Vase/Gefäß, Figur, Christbaumschmuck, Schild, Halterung, Box, Ersatzteil und Leer. Jede Kachel startet den passenden **Baukasten** oder bietet passende Missionen und Bausteine an. Es gibt keine starren Fertig-Vorlagen.
- **Missionen:** geführte Mini-Projekte von 3 bis 10 Minuten. Schritte werden durch Hervorhebung erklärt. Das Programm **prüft den echten Modellzustand** („Super, der Quader ist 50 mm breit!“), statt nur „Weiter“-Klicks zu zählen.
- **Lotse:** Eine Karte unten in der 3D-Ansicht zeigt den nächsten Missionsschritt. Er lässt sich jederzeit ein- und ausschalten (an der Karte oder unter Einstellungen). Er blendet nur Hilfe ein und aus, nie Werkzeuge oder Felder; es gibt also keinen getrennten Anfänger- und Profimodus (Design-Prinzip 8).
  1. Dein erster Schlüsselanhänger
  2. Ein Stern für den Weihnachtsbaum
  3. Eine Wandhalterung, die hält
  4. Box mit Deckel, die passt
  5. Ausstechform aus einer Zeichnung
- **Tooltip-Karten:** Nach 0,8 s über einem Werkzeug erscheint eine Karte mit einer 3–5-Sekunden-Animation, einem Satz Erklärung und dem Tastenkürzel.
- **„Zeig mir wie“-Knopf** in der Eigenschaftsleiste spielt eine kurze Animation zum aktiven Werkzeug ab.
- **Befehlssuche mit Synonymen:** „runde ecken“ → Abrunden, „bohren“/„schraube“ → Bohrung, „hohl“ → Aushöhlen, „spiegeln“, „namen“ → Text.

---

## 12. Sprache: Glossar

UI-Texte verwenden ausschließlich die Begriffe in der mittleren Spalte. Englische UI-Begriffe in Klammern sind für die englische Übersetzung.

| Fachbegriff (Fusion) | Fission3D (DE) | Fission3D (EN) | Kurz-Erklärung im Tooltip |
|---|---|---|---|
| Extrude / Press Pull | Ziehen | Pull | Fläche oder Skizze in die Höhe ziehen |
| Revolve | Drehkörper | Spin | Profil um eine Achse drehen (Vasen, Knöpfe) |
| Sweep | Entlang Pfad | Along Path | Profil an einer Linie entlangführen |
| Loft | Übergang | Blend Between | Weicher Übergang zwischen zwei Formen |
| Fillet | Abrunden | Round | Kanten rund machen |
| Chamfer | Abschrägen (Fase) | Bevel | Kanten schräg abschneiden |
| Shell | Aushöhlen | Hollow | Innen hohl machen, Wandstärke behalten |
| Join | Vereinen | Merge | Zu einem Körper verbinden |
| Cut | Ausschneiden | Subtract | Eine Form aus einer anderen herausschneiden |
| Intersect | Überlappung behalten | Keep Overlap | Nur das, was beide gemeinsam haben |
| Split Body | Teilen | Split | Körper an einer Fläche zerteilen |
| Pattern | Muster | Repeat | Etwas mehrfach anordnen |
| Mirror | Spiegeln | Mirror | Spiegelbildlich kopieren |
| Sketch | Skizze | Sketch | 2D-Zeichnung als Grundlage |
| Constraint | Regel | Rule | z. B. „parallel“, „gleich lang“ |
| Dimension | Maß | Measure | Länge, Abstand oder Winkel festlegen |
| Fully Constrained | steht fest | locked | Die Skizze kann sich nicht mehr bewegen |
| Timeline | Rezept | Recipe | Alle Schritte nacheinander, jederzeit änderbar |
| User Parameter | Benanntes Maß | Named Measure | Ein Maß mit Namen, nutzbar in Formeln |
| Body | Körper | Body | Ein festes Teil |
| Construction Plane | Hilfsebene | Helper Plane | Unsichtbare Fläche zum Zeichnen |
| Offset | Versetzen | Offset | Parallel verschieben |
| Draft | Schräge | Taper Face | Fläche leicht neigen |
| Clearance | Spiel | Play / Gap | Luft zwischen Teilen, damit sie passen |
| Emboss / Deboss | Prägen / Gravieren | Emboss / Engrave | Erhaben / vertieft |
| Smooth Boolean | Weich verschmelzen | Melt Together | Formen fließen weich ineinander |
| – | Baukasten | Kit | Schritt für Schritt zum eigenen Objekt (Karte, Vase, Figur) |
| – | Motiv | Motif | 2D-Bild zum Prägen, Gravieren oder Durchbrechen |
| – | Griffe: Achsen | Handles: Axes | Farbige Pfeile und Maßbänder am ausgewählten Körper |
| – | Griffe: Klassisch | Handles: Classic | Eck-Quadrate und Maßkästchen, wie in Tinkercad |
| – | Akzentfarbe | Accent colour | Die Farbe für Knöpfe und Markierungen, frei wählbar |
| – | Lotse | Guide | Zeigt in Missionen den nächsten Schritt, ein- und ausschaltbar |
| – | Fokus | Focus | Blendet die Seitenleisten aus, die 3D-Ansicht wird groß |
| – | Streuen | Scatter | Ein Motiv zufällig über eine Fläche verteilen |

---

## 13. Visuelle Gestaltung

- **Ruhig, freundlich, modern.** Viel Weißraum, eine Akzentfarbe, abgerundete Ecken, klare Typografie (IBM Plex). Einzelheiten: [08-gestaltung.md](08-gestaltung.md).
- **Akzentfarbe wählbar (R18):** Standard ist Blau. Unter Einstellungen ▸ Aussehen gibt es Vorschläge und eine eigene Farbe. Alle Akzenttöne (Knöpfe, Markierungen, Auswahl) werden daraus abgeleitet, Schrift darauf hält WCAG AA (4,5 : 1). Rot, Gelb und Grün sind Hinweisfarben („geht nicht“, „braucht Hilfe“, „passt“); ähnelt die gewählte Farbe einer davon, erscheint ein Hinweis.
- **Modelle sehen aus wie gedruckt:** matte Kunststoff-Optik, weiche Umgebungsverdeckung (Ambient Occlusion), sanfter Schatten auf dem Druckbett, feine dunkle Kantenlinien.
- **Standardfarben der Körper:** freundliche, gut unterscheidbare Töne, auch für Farbenblinde geeignet.
- **Hell- und Dunkelmodus**, folgt dem System.
- **Animationen 150–250 ms** für Kamera, Panels und Griffe. Sie sind abschaltbar („Bewegung reduzieren“ des Systems wird respektiert).
- **Druckbett** mit Raster und Abmessung des gewählten Druckers. Der Ursprung liegt sichtbar in der Mitte.

---

## 14. Barrierefreiheit

- Vollständig per Tastatur bedienbar (Fokus sichtbar, Tab-Reihenfolge)
- UI-Skalierung 80–200 %, Mindest-Schriftgröße 12 pt
- Kontraste nach WCAG AA; Status nie nur über Farbe (immer auch Symbol oder Text)
- Screenreader-Beschriftungen für alle Bedienelemente (Qt Accessibility)

---

## 15. Beispielabläufe (Referenz für Tests)

Diese drei Abläufe sind **Abnahmeszenarien**. Sie müssen sich wie hier beschrieben durchklicken lassen.

### A) Schlüsselanhänger (Ziel: ≤ 10 min für Neulinge)
1. Startbildschirm → „Leer“. Das Druckbett des gewählten Druckers ist sichtbar.
2. Formen → Quader aufs Bett ziehen → `50` Tab `20` Tab `4` Enter.
3. Kontextleiste am Quader → „Alle senkrechten Kanten runden“ → `4`.
4. Formen → Text → auf die Oberseite klicken → `LENA` tippen. Der Text sitzt mittig. Höhe `1,5`, Schrift wählen.
5. Formen → Zylinder → „Als Loch“ → an den linken Rand ziehen. Die Abstandsmaße erscheinen: `5` mm vom Rand, Ø `5`.
6. Text-Körper auswählen → Farbe Rot.
7. Prüfen → Druckcheck: alles grün.
8. Export → „Im Slicer öffnen“. Der Slicer öffnet sich mit zwei Farben, als 3MF.

### B) Wandhalterung (Ziel: ≤ 20 min für Tinkercad-Umsteiger)
1. Skizze auf der Grundebene → L-Profil mit Rechteck-Werkzeug und Linien → Maße 60 × 40, Stärke 5.
2. Fläche ziehen → `30` mm.
3. Innenkante → Rundungsgriff → `6` mm (für Stabilität; Tooltip: „Innenrundungen machen Teile stabiler“).
4. Rückseite anklicken → „Loch“ → Bohrungs-Assistent: „Senkkopfschraube 4 mm (Holz)“. Senkung und Durchmesser kommen automatisch.
5. Muster → Reihe → 2 Löcher, Abstand `30`.
6. Druckcheck → Hinweis: „Für mehr Festigkeit auf die Seite legen.“ [Ausrichten]
7. Maße → `Breite` von 30 auf 40 ändern. Alles passt sich an, die Löcher bleiben mittig.

### C) Weihnachtsstern (Ziel: Spaß, ≤ 10 min)
1. Deko → Generatoren → Stern → Zacken `8`, Außen-Ø `70`, Innen-Ø `35`, Höhe `6`, „Spitzen zulaufend“ an.
2. Formen → Kugel in die Mitte, Ø `20` → Kombinieren → **Weich verschmelzen** → Weichheit per Regler.
3. Bausteine → Aufhänger-Öse → an eine Zacke ziehen. Sie rastet an der Spitze ein.
4. Deko → Gravieren → Text `2026`, auf die Kugel (gekrümmt).
5. Zweite Farbe für die Gravur → Export 3MF.
6. Rezept: Zacken von 8 auf 6 ändern. Alles wird neu berechnet, die Öse sitzt wieder an einer Spitze.

### D) Tinkercad-Fallen (Regressionstest, automatisiert)
Prüft die Zusagen aus Abschnitt 7.1–7.3. Läuft ab Phase 1 in der CI, die Schritte 4 und 5 ab Phase 2.
1. Quader 40 × 40 × 10 mm mit Eckenradius 5 mm → Breite am Griff auf 80 mm ziehen → **Ecken messen: weiterhin Kreisbögen R 5 mm** (keine Ellipsen).
2. Zylinder Ø 6 mm „Als Loch“ auf den Quader legen → **ist sofort ausgeschnitten, ohne Gruppieren**. Das Etikett zeigt „schneidet aus: Quader“.
3. Geister-Loch anklicken und um 10 mm verschieben → Ergebnis aktualisiert sich live. Ein Undo-Schritt.
4. Quaderhöhe auf 20 mm ändern → **das durchgehende Loch geht weiterhin ganz durch**.
5. Den kombinierten Körper an der rechten Seitenfläche um 20 mm breiter ziehen → **das Loch bleibt rund (Ø 6 mm)** und behält seinen Abstand zur linken Kante.
6. Export STL „Normal“ → **Abweichung der Lochkante vom idealen Kreis ≤ 0,01 mm** (automatisch gemessen).
7. Ändern ▸ Mehr ▸ Verzerren → Hinweis „Rundungen und Löcher werden oval …“ erscheint, bevor etwas passiert.
