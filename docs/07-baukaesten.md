# Baukästen: geführtes Gestalten statt fertiger Vorlagen

## Idee

Ein **Baukasten** ist **keine fertige Vorlage**, die man nur noch beschriftet. Er ist ein Gestaltungshelfer, der einen Schritt für Schritt durch die Entscheidungen führt, die für ein bestimmtes Objekt wichtig sind. Bei einer Weihnachtskarte sind das Format, Hintergrund, Motive und Text. Bei jedem Schritt wählt man aus Optionen mit Live-Vorschau, kombiniert frei, verwirft, probiert weiter. Jedes Ergebnis ist ein Einzelstück.

**Abgrenzung:**

| Begriff | Was es ist | Beispiel |
|---|---|---|
| **Generator** (AP-4.8) | *Eine* parametrische Form mit Reglern | Stern mit 8 Zacken |
| **Baustein** (AP-5.3) | Ein Fertigteil zum Andocken an ein Modell | Aufhänger-Öse, Scharnier |
| **Baukasten** (dieses Dokument) | Ein geführter Gestaltungsprozess, der Generatoren, Bausteine, Motive und Werkzeuge zu einem Objekt kombiniert | Weihnachtskarte, Vase, Figur |

---

## Grundregeln

1. **Kein Käfig.** Das Ergebnis ist ein ganz normales Modell. Jede Wahl im Baukasten erzeugt gewöhnliche Rezept-Schritte, zusammengefasst in einer Rezept-Gruppe („Weihnachtskarte“). Man kann den Baukasten jederzeit verlassen und mit allen Werkzeugen frei weiterarbeiten, und ihn per Doppelklick auf die Gruppe wieder öffnen.
2. **Freie Änderungen werden respektiert.** Hat man einen Schritt außerhalb des Baukastens verändert, markiert der Baukasten ihn als „von dir angepasst“ und überschreibt ihn nur auf ausdrücklichen Wunsch.
3. **Jeder Schritt ist optional und in beliebiger Reihenfolge erreichbar.** Die Schrittleiste ist eine Orientierung, kein Zwang.
4. **Live-Vorschau für jede Option.** Die Kacheln zeigen die Option *am eigenen aktuellen Modell* (Mini-Renderings, im Hintergrund erzeugt und zwischengespeichert), nicht ein generisches Beispielbild.
5. **Immer druckbar.** Die Optionen sind auf das aktive Druckerprofil abgestimmt (Bettgröße, Düse, Farbanzahl). Was nicht passt, wird erklärt und nicht stillschweigend weggelassen.
6. **„Überrasch mich“** würfelt in jedem Schritt (oder für alles) eine stimmige Kombination. Einzelne Schritte lassen sich per Schloss-Symbol vom Würfeln ausnehmen.
7. **Eigenes ist gleichberechtigt.** In jedem Motiv-, Form- oder Text-Schritt kann man auch eigene Inhalte verwenden: eigene SVGs, nachgezeichnete Zeichnungen (AP-4.5), eigene Kurven, eigene Schriften.
8. **Meine Kombinationen.** Eine Zusammenstellung lässt sich als persönlichen Startpunkt speichern, z. B. „Unser Familien-Kartenstil“.
9. **Baukästen sind Daten, kein Code.** Definition als Datei (Schritte, Optionen, Abbildung auf Features und Parameter). Neue Baukästen und Motive brauchen keinen neuen Programmcode. Später können sie über die Python-API (B-2) und die Community-Bibliothek (B-9) erweitert werden.

---

## Bedienung

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ Baukasten: Weihnachtskarte                     [🎲 Überrasch mich] [Frei weiter]│
│ ① Format ─ ② Hintergrund ─ ③ Motive ─ ④ Text ─ ⑤ Feinschliff ─ ⑥ Drucken       │
├──────────────────────────┬────────────────────────────────────────────────────┤
│ ② HINTERGRUND            │                                                    │
│ Form                     │                                                    │
│ [▭] [▢] [★] [🎄] [◯] [✎] │              Live-3D-Ansicht der Karte             │
│ Fläche                   │                                                    │
│ [glatt] [Schnee] [Holz]  │          (Motive direkt anfassbar: ziehen,         │
│ Durchbrüche              │           drehen, Größe ändern, Ebene wählen)      │
│ [keine] [Sterne] [Flocken│                                                    │
│ Rahmen                   │                                                    │
│ [keiner] [Zierrand] [✎]  │                                                    │
│ ▸ Mehr (Dicke, Ecken …)  │                                                    │
├──────────────────────────┴────────────────────────────────────────────────────┤
│ Tipp: Ziehe Motive aus der Bibliothek direkt auf die Karte.      [‹ Zurück] [Weiter ›]│
└───────────────────────────────────────────────────────────────────────────────┘
```

- Der Baukasten ersetzt die linke Seitenleiste. Die 3D-Ansicht bleibt voll bedienbar: Motive lassen sich direkt verschieben, drehen und in der Größe ändern.
- Einstieg über den Startbildschirm „Was möchtest du bauen?“ oder jederzeit über **Formen ▸ Baukästen**.

---

## Motiv-Bibliothek (gemeinsame Grundlage)

Motive sind **2D-Vektorgrafiken**, die in Karten, Vasen (als Relief), Figuren-Sockeln, Anhängern und Schildern verwendet werden.

- **Kategorien:** Weihnachten, Winter, Ostern, Geburtstag, Hochzeit und Liebe, Natur und Blumen, Tiere, Sterne und Himmel, Ornamente und Rahmen, Symbole, Muster (kachelbar).
- **Mehrfarbig aufgebaut:** Motive bestehen aus benannten Ebenen (z. B. Baum, Stern, Kugeln). Jede Ebene bekommt eigene Farbe und Höhe.
- **Druckgeprüft:** Jede Linie ist ≥ 2 × Düsendurchmesser breit (bei 0,4 mm also ≥ 0,8 mm). Beim Verkleinern warnt das Programm, wenn Details zu fein werden („Unter 30 mm werden die Schneeflocken-Spitzen zu dünn“).
- **Platzierungsarten:** erhaben (Relief, Höhe einstellbar), vertieft (Gravur), durchbrochen (Fenster), aufgesetzt als eigenes Teil in anderer Farbe.
- **Streuen:** ein Motiv zufällig verteilen (Dichte, Größenvariation, Drehung, Mindestabstand, Seed), z. B. Schneeflocken über den ganzen Hintergrund.
- **Größe ändern verzerrt nie** (Design-Prinzip 12): Motive werden proportional skaliert. Spiegeln ist erlaubt.
- **Lizenzen:** Nur selbst erstellte Motive oder **CC0** bzw. **CC-BY-4.0** (Namensnennung im Über-Dialog und in den Motiv-Infos). Kein „nur nicht-kommerziell“. Jede Motivdatei enthält Herkunft, Autor und Lizenz.
- **Start-Umfang:** ≥ 150 Motive, davon ≥ 50 für Weihnachten und Winter.

---

## Baukasten 1: Karten

Weihnachts-, Geburtstags-, Glückwunsch-, Dankes- und Einladungskarten sowie Lesezeichen.

| Schritt | Optionen |
|---|---|
| **① Format** | **Größe:** A6 (148 × 105), Quadratisch 120/150, DIN lang (210 × 99, wenn das Bett es zulässt), Postkarte, Mini-Karte (Geschenkanhänger 85 × 55), eigene Größe. **Art:** flache Karte; **Klappkarte** (zwei Hälften mit druckbarem Scharnier oder Steckverbindung); **Aufstellkarte** (mit Steckfuß); **Lichtkarte** (Lithophanie, gegen das Licht halten). **Ausrichtung:** hoch/quer. Größen über der Bettgröße werden erklärt und mit Klappkarte oder Teilen als Lösung angeboten |
| **② Hintergrund** | **Form:** Rechteck, abgerundet, Stern, Tannenbaum, Kugel, Herz, Wolke, eigene Kontur. **Fläche:** glatt, Struktur (Schnee, Holz, Leinen, Hammerschlag). **Durchbrüche:** Sterne, Schneeflocken, Voronoi, Punkte. **Rahmen:** keiner, schlicht, Zierrahmen aus der Motiv-Bibliothek, eigener. **Dicke** (▸ Mehr) |
| **③ Motive** | Motive aus der Bibliothek oder eigene (SVG, nachgezeichnete Zeichnung) per Ziehen platzieren; Art (erhaben, vertieft, durchbrochen, aufgesetzt); Höhe; Farbe je Ebene; Streuen; Anordnen-Hilfen (zentrieren, Reihe, Kreis, spiegelsymmetrisch) |
| **④ Text** | Vorschläge in DE/EN („Frohe Weihnachten“, „Frohes Fest“, „Merry Christmas“, „Alles Gute“, „Danke“ …) oder freier Text; Schrift (druckgeeignete OFL-Schriften); gerade, im Bogen, entlang einer Kurve; erhaben oder vertieft; automatisch passend in eine Fläche einpassen. **Innentext** bei Klappkarten (Rückseite) |
| **⑤ Feinschliff** | Farbschema (passende Paletten zum aktiven Filament-Set); **Mehrfarbig**: je Motiv/Ebene ein Filament (AMS/MMU) **oder Farbwechsel nach Höhe** für Drucker mit nur einer Düse (z. B. Hintergrund weiß, ab 1,2 mm rot); Aufhänge-Loch oder Öse; Kanten runden |
| **⑥ Drucken** | Druckcheck; **Versand-Check**: passt in welchen Umschlag (C6, DL …), Dicke und Gewicht (aus Material und Füllung) gegen Brief-Formate (Daten pro Land; zuerst Deutsche Post: Standardbrief ≤ 5 mm/20 g, Kompaktbrief ≤ 10 mm/50 g, Großbrief ≤ 20 mm/500 g); Tipp für dünne, leichte Karten (Füllung, Wandlinien); Export bzw. „Im Slicer öffnen“ |

---

## Baukasten 2: Vasen und Gefäße

Vasen, Windlichter (Teelicht), Stiftebecher, Übertöpfe, Schalen.

| Schritt | Optionen |
|---|---|
| **① Zweck und Größe** | **Zweck:** Blumenvase (wasserdicht), Trockenblumen-/Deko-Vase, Windlicht (mit Teelicht-Maß, Sicherheitsabstand zur Flamme, Hinweis: nur mit LED-Teelicht bei PLA), Stiftebecher, Übertopf (optional mit Ablaufloch und Untersetzer), Schale. **Höhe und Öffnung** in mm. Der Zweck setzt sinnvolle Standards (Wandstärke, Boden, Wasserdichtigkeit) |
| **② Silhouette** | Profil-Formen: zylindrisch, bauchig, Flasche, Tulpe, Kegel, Sanduhr, Amphore, oder **Kurve selbst ziehen** (Griffe am Profil in der 3D-Ansicht). Überhänge > 45° werden direkt am Profil markiert |
| **③ Querschnitt** | Kreis, Vieleck (n Seiten), Stern, Blüte, Welle, Tropfen, eigene Kontur. Optional **Übergang**: unten anders als oben (unten Kreis, oben Stern) |
| **④ Struktur** | Verdrehung (Grad über die Höhe, auch gegenläufig), Rippen, Spirale, Wellen, Voronoi-Durchbrüche (nur Deko/Windlicht), Low-Poly, Oberflächenstrukturen, **Motive als Relief** aus der Motiv-Bibliothek (umlaufend, wiederholt) |
| **⑤ Rand und Boden** | Rand: glatt, gewellt, gezackt, Lippe, nach außen gebogen. Boden: Bodenstärke, Standring, Füße, Gravur unter dem Boden (Name, Jahr) |
| **⑥ Drucken** | **Druckart:** Vasenmodus/Spiralmodus (eine Wand, schnell; das Programm prüft, ob die Form dafür geeignet ist) oder normale Wand mit Wandstärke. Wasserdicht-Hinweise (Wandlinien, Temperatur, ggf. Beschichtung). Druckcheck, Export |

Technik: Grundlage ist der Vasen-Generator (AP-4.8) mit Verformen (AP-4.3), Strukturen (AP-4.7) und Relief (AP-4.6/4.4). Das Ergebnis ist ein freier Körper; präzise bleibt er, solange nur Silhouette und einfache Querschnitte gewählt sind (dann auch als STEP exportierbar).

---

## Baukasten 3: Figuren

Niedliche, stilisierte Figuren: Schneemann, Wichtel, Engel, Rentier, Pinguin, Eule, Katze, Hase, Roboter, Monster. Keine realistischen Menschen, das ist kein Sculpting-Programm.

| Schritt | Optionen |
|---|---|
| **① Grundfigur** | Figur aus der Auswahl **oder** „eigene Figur aus Formen“ (Rumpf, Kopf, Gliedmaßen frei aus Grundformen). Jede Figur ist ein **Gerüst aus Körperteilen** (z. B. Schneemann = drei Kugeln; Wichtel = Kegelkörper + Bart + Mütze), die weich verschmolzen werden (AP-4.2) |
| **② Proportionen** | Regler in Alltagssprache: Größe, Kopfgröße, „pummelig ↔ schlank“, Beinlänge, Ohrenlänge … Jede Figur definiert ihre Regler und sinnvolle Bereiche. Direkt in der 3D-Ansicht an Körperteilen ziehen |
| **③ Gesicht** | Augen, Nase, Mund und Wangen in mehreren Stilen (Punkte, Knopfaugen, Comic, geschlossen/schlafend). Umsetzung: geprägt, vertieft oder **als Einsetzteil in anderer Farbe** (mit Passung, AP-3.3) |
| **④ Zubehör** | Mütze, Schal, Knöpfe, Besen, Laterne, Geschenk, Stern, Brille, Fliege … als Bausteine mit **Andockpunkten** an der Figur (AP-5.3), die bei Proportionsänderungen mitwandern |
| **⑤ Sockel und Zweck** | Sockel (rund, eckig, Schneehügel, Baumstamm, keiner), Text oder Name auf dem Sockel; Zweck: Aufsteller, **Christbaumanhänger** (Öse am Schwerpunkt, damit er gerade hängt), Kühlschrankmagnet (Magnettasche), Kerzenhalter, Platzkarte (Schlitz für Namenskärtchen). **Standfestigkeits-Check:** Schwerpunkt über der Standfläche, sonst Vorschlag (Sockel vergrößern, Gewicht unten) |
| **⑥ Drucken** | Supportfreiheit prüfen, Druckausrichtung vorschlagen (AP-3.6); **automatisch in druckbare Teile zerlegen** (z. B. Kopf separat für Mehrfarbe oder ohne Stützen) mit Steckzapfen (AP-3.7); Farben je Teil; Export |

Technik: Figuren-Definitionen sind Daten (Körperteile als parametrische SDF-Grundformen, Verschmelz-Weichheit, Regler → Parameter-Abbildung, Andockpunkte). Neue Figuren erfordern keinen Code.

---

## Weitere Baukästen (nach den ersten drei)

Gleiche Engine, eigene Schritte. Die Reihenfolge richtet sich nach Nutzerfeedback:

| Baukasten | Schritte (Kurzform) |
|---|---|
| **Christbaumschmuck und Anhänger** | Grundform (flach, Kugel, Stern, Glocke, Tropfen) → Motive/Durchbrüche → Text/Jahreszahl → Aufhängung → Farben |
| **Windlicht und Lampenschirm** | Form → Lithophanie-Bild oder Motiv-Durchbrüche → Lichtquelle (Teelicht, LED-Kerze, E14) → Sockel |
| **Namens- und Türschilder** | Form → Text → Motive → Befestigung (Schraublöcher, Klebefläche, Aufhänger, Stecker für Blumentopf) |
| **Ausstechformen und Stempel** | Kontur (Motiv, Zeichnung, Text) → Schneidenhöhe → Griff → Präge-Einsatz |
| **Schmuck** | Anhänger, Ohrringe, Armreif → Motiv → Öse/Haken |
| **Untersetzer und Schalen** | Form → Muster → Rand → Rutschfest-Füße |
| **Boxen und Schachteln** | Form → Deckelart (Steck, Scharnier, Schraub) → Fächer → Dekor (Motive/Text auf Deckel) |
