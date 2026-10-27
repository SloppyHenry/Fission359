# Gestaltung (Design-System)

**Status: verbindlich seit 2026-10-13** (Entscheidung des Product Owners, siehe R17–R20 in
[06-rueckfragen.md](06-rueckfragen.md)).

Die Vorlage für Aussehen und Anordnung ist das klickbare Mockup
[mockups/oberflaeche.html](mockups/oberflaeche.html). Dieses Dokument hält fest, was daraus für die
Umsetzung in Qt/QML gilt: Farben, Schrift, Maße, Bausteine und Regeln. Wenn Mockup und dieses Dokument
sich widersprechen, gilt dieses Dokument. Bedienregeln (Was passiert beim Ziehen? Welche Begriffe?) stehen
weiterhin im [UX-Konzept](02-ux-konzept.md). Bei Unterschieden zum UX-Konzept gilt das UX-Konzept, und der
Unterschied kommt als Rückfrage in [06-rueckfragen.md](06-rueckfragen.md).

Die Strings, Szenen und Daten im Mockup sind Beispiele, kein Produktionscode.

---

## 1. Bildschirmaufbau

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│ ◐ Fission359  Datei Bearbeiten Ansicht Körper Fenster Hilfe  [⌕ Was möchtest du tun? ⌘K]  ⚙  Drucker │  Menüleiste 40 px
├──────────────────────────────────────────────────────────────────────────────────────┤
│ ⌂ [Schlüsselanhänger ×] [Wandhalterung]  +                          ↶ ↷  [Exportieren] │  Dokument-Tabs 52 px
├──────────────┬────────────────────────────────────────────────────────┬──────────────┤
│ Körper | Bib.│ [Perspektive ▾] [Drahtgitter|Volumen|Farben]    ┌─────┐ │ Objekt|Drucker│
│ ⌕ suchen     │ ┌──────┐                                        │OBEN │ │ Quader        │
│ ▾ Anhänger   │ │Wählen│        Kontextleiste (am Objekt)       └─────┘ │ Maße          │
│   ■ Quader   │ │Versch│                                         ⌂  ⛶  │ Breite 50 mm  │
│   ■ Text     │ │Drehen│              3D-ANSICHT                         │ Position      │
│ ▤ Druckbett  │ │Größe │                                                │ Farbe         │
│──────────────│ │──────│                                                │ Als Loch  ○   │
│ Rezept    ⌘Z │ │Formen│                                           ⟳    │ ▸ Alle Ein-   │
│ ① Quader  ✓  │ │Skizze│     ┌──────────── Lotse ─────────────┐   ✋   │   stellungen  │
│ ② Abrunden ✎ │ │ …    │     │ Schritt 3/8 · Runde die Ecken ab│   🔍   │   zeigen      │
│ ── jetzt ──  │ └──────┘     └─────────────────────────────────┘        │               │
├──────────────┴────────────────────────────────────────────────────────┴──────────────┤
│ Quader ausgewählt · Werkzeug: Verschieben · Hinweiszeile …          Einheit: mm · ● gespeichert │  Statuszeile 28 px
└──────────────────────────────────────────────────────────────────────────────────────┘
```

| Bereich | Breite / Höhe | Inhalt |
|---|---|---|
| Menüleiste | 40 px hoch | Logo, Menüs, Befehlssuche (Mitte, max. 440 px), Einstellungen (Zahnrad), Druckerprofil |
| Dokument-Tabs | 52 px hoch | Startseite, offene Projekte, Rückgängig/Wiederholen, Hauptknopf „Exportieren“ |
| Linke Leiste | 248 px (216 px unter 1100 px Fensterbreite) | Reiter „Körper“ und „Bibliothek“; darunter das Rezept mit Zeitmarker „jetzt“ |
| 3D-Ansicht | Rest | Schwebende Bedienelemente, siehe §5 |
| Rechte Leiste | 300 px (264 px unter 1100 px) | Reiter „Objekt“ und „Drucker“; Eigenschaften des Ausgewählten, Seltenes hinter „Alle Einstellungen zeigen“ |
| Statuszeile | 28 px hoch | Auswahl, Werkzeug, Hinweiszeile, Einheit und **Raster-Steuerung** (Auto-Knopf, Schieberegler, Zahlenfeld; Wunsch des Product Owners 2026-10-19, nicht im Mockup), Speicherstatus |

- **Fokus-Modus** (Knopf unter dem Ansichtswürfel, `⇧F`): Beide Seitenleisten verschwinden. Damit erreicht
  die 3D-Ansicht das Ziel von mindestens 65 % (gemessen: 84 % bei 1280 × 800). Mit beiden Leisten sind es
  48 % bei 1280 × 800 und 63 % bei 1920 × 1080; das ist so akzeptiert (R17).
- Bei einem Baukasten ersetzt der Baukasten-Schrittdialog die linke Leiste (312 px), die rechte entfällt.
- Unter 760 px Breite (nur Fenster-Sonderfall, kein Zielgerät) entfallen beide Seitenleisten.

## 2. Farben

Dunkel ist der Standard; Hell und „Wie das System“ sind wählbar. Alle Farben sind **Tokens**; kein Baustein
verwendet Farbwerte direkt. In QML entspricht das einem zentralen Theme-Singleton.

| Token | Dunkel | Hell | Verwendung |
|---|---|---|---|
| `chrome` | `#131416` | `#E9EBEE` | Fenstergrund, Menüleiste, Startseite |
| `bar` | `#1A1B1E` | `#F0F1F3` | Tab-Leiste, Suchfeld |
| `panel` | `#1F2023` | `#F7F8F9` | Seitenleisten, schwebende Leisten, Karten |
| `field` | `#27292D` | `#FFFFFF` | Eingabefelder |
| `field-2` | `#2E3035` | `#F0F2F5` | gedrückte Segmente, Kacheln |
| `raise` | `#26282C` | `#FFFFFF` | Auswahlkarten im Export und in den Einstellungen |
| `line` / `line-2` / `line-3` | `#34363B` / `#3A3D43` / `#4A4E55` | `#D9DCE1` / `#CDD1D7` / `#B5BBC4` | Trennlinien, Rahmen, Rahmen bei Fokus |
| `view` → `view-2` | `#2B2D31` → `#232528` | `#E3E6EA` → `#D4D8DE` | Verlauf der 3D-Ansicht (Mitte → Rand) |
| `fg` | `#E8E9EB` | `#1B1D21` | Haupttext |
| `fg-2` | `#C9CCD1` | `#3A3E45` | Fließtext |
| `fg-3` | `#A4A8B0` | `#50555E` | Nebentext, Beschriftungen |
| `fg-4` | `#979CA5` | `#5E646E` | leiseste Schrift (Hinweise, Einheiten) |
| `accent` | `#2563D9` | `#2563D9` | Hauptknöpfe, aktives Werkzeug, Schalter an |
| `accent-text` | `#7FB0FF` | `#1F57C2` | Links, Akzentschrift |
| `accent-line` | `#5B97FF` | `#2563D9` | Auswahl-Umriss, Maßbänder, Fokusrahmen |
| `accent-soft` | `#22314F` | `#DCE8FF` | Hintergrund ausgewählter Zeilen und Karten |
| `amber` | `#F0A53A` | `#8F5600` | **braucht Hilfe**, Lotse, Druckcheck-Hinweise |
| `ok` | `#5DBB63` | `#17692F` | **passt**, erledigt |
| `bad` | `#F47C74` | `#B71F1F` | **geht nicht**, Fehler |
| `ax-x` / `ax-y` / `ax-z` | `#E5534B` / `#57AB5A` / `#5B97FF` | gleich | Achsen und Achsen-Griffe |

**Bedeutungsfarben:** Rot, Gelb (Amber) und Grün sind für Zustände reserviert und werden nie als Schmuck
verwendet. Jede Farbaussage hat zusätzlich ein Symbol oder einen Text (Farbenblindheit).

**Kontrast:** Jede Kombination aus Schrift-Token und Fläche erreicht mindestens 4,5 : 1 (WCAG AA); das wird
mit einem automatischen Test über alle Token-Paare beider Designs geprüft.

### Akzentfarbe (R18)
Die Akzentfarbe ist frei wählbar (Einstellungen ▸ Aussehen): Vorschläge Blau (Standard, `#2563D9`),
Violett `#7C4DDB`, Türkis `#0E9AA7`, Pink `#D6336C`, Schiefer `#5A6B85`, dazu eine eigene Farbe.
Aus der gewählten Farbe werden alle sechs Akzent-Tokens abgeleitet:

- `accent`: Helligkeit so weit absenken, bis weiße Schrift darauf ≥ 4,5 : 1 hat.
- `accent-text`: im dunklen Design aufhellen, im hellen abdunkeln, bis sie auf der ungünstigsten Fläche
  (`panel`, `field`, `chrome`, `view`, `accent-soft`) ≥ 4,5 : 1 erreicht.
- `accent-line`: dunkel = Farbe mit Helligkeit 62–72 %; hell = `accent`.
- `accent-soft`: `panel` mit 22 % (dunkel) bzw. 14 % (hell) der Farbe gemischt.
- `accent-hover`: `accent` ± 5 % Helligkeit.

Ähnelt die Farbe Rot, Gelb oder Grün (Farbton 345–15°, 25–65°, 90–155°, Sättigung ≥ 25 %), erscheint ein
Hinweis; gesperrt wird sie nicht. Die Referenz-Umsetzung steht im Mockup (`accentTokens`).

## 3. Schrift

- **IBM Plex Sans** für die Oberfläche, **IBM Plex Mono** für Zahlen in Feldern, Maße und Formate.
  Lizenz: SIL Open Font License 1.1. Die Schriftdateien werden mitgeliefert (kein Download zur Laufzeit) und
  beim Einbinden in THIRD_PARTY_NOTICES.md eingetragen.
- Ersatz, falls nicht verfügbar: Systemschrift (macOS: SF Pro, Linux: Cantarell bzw. Noto Sans).
- Grundgröße 13 px, Zeilenhöhe 1,45. Abstufungen: 12 · 12,5 · 13 · 14 · 15 · 17 px; Überschriften der
  Startseite 28 px (fett 700). Zahlen in Spalten mit gleich breiten Ziffern.
- Die Größen gelten bei 100 % UI-Skalierung; die Skalierung 80–200 % (UX §14) wirkt auf alles.

## 4. Formen, Abstände, Bewegung

- Radien: Felder, Knöpfe, Segmente 6 px · schwebende Leisten 9 px · Karten 10–12 px · Dialog 14 px.
- Höhen: Knopf und Feld 32 px, kleiner Knopf 28 px, Baumzeile 33 px.
- Abstände in Schritten von 4 px (4 · 8 · 12 · 16 · 20 · 24); Innenabstand der Seitenleisten 12–16 px.
- Schwebende Elemente (`float`): Fläche `panel`, 1 px Rahmen `line`, weicher Schatten.
- Animationen 150–250 ms (Standard 180 ms); „Bewegung reduzieren“ des Systems schaltet sie ab.

## 5. 3D-Ansicht

- **Werkzeugpalette** links (84 px breit, schwebend; das Mockup hat 76 px, das reicht für „Verschieben“ und „Kombinieren“ nicht): oben Wählen, Verschieben, Drehen, Größe; darunter die
  Werkzeuggruppen Formen, Skizze, Ändern, Kombinieren, Muster, Deko, Prüfen. Eine Gruppe öffnet eine bebilderte
  Auswahl direkt neben der Palette. Jede Schaltfläche hat Symbol **und** Beschriftung.
- **Darstellungsleiste** oben links: Perspektive/Orthografisch, Drahtgitter | Volumen | Farben.
- **Ansichtswürfel** oben rechts, darunter Startansicht und Fokus-Modus.
- **Navigationsknöpfe** unten rechts: Drehen, Verschieben, Zoomen (zeigen beim Klick, wie die Maus es macht).
- **Kontextleiste** schwebt über dem ausgewählten Objekt (nie unter Würfel oder Palette).
- **Lotse** (Missionen) unten in der Mitte; mit „Verstanden“ wird er zur kleinen Pille, mit ✕ aus.
  Er blendet nur Hilfe ein und aus, nie Werkzeuge oder Felder (Design-Prinzip 8).
- **Griffe (R19):** Einstellung „Achsen“ (Standard) oder „Klassisch“, siehe UX §2.2.
  - Achsen: Pfeile in `ax-x/y/z` mit Beschriftung, Maßbänder parallel zur Kante mit Maß-Pille (Mono-Schrift),
    Punkte in `accent-line` zum Strecken, Auswahl-Umriss in `accent-line`.
  - Maß-Griffe der Grundformen: Punkt auf der Fläche, die er bewegt (weiß, Rand in `ax-x/y/z`); Maßband außerhalb des
    Körpers (auf dem Bett davor bzw. an der Außenkante), Maß-Pille mittig auf dem Band. Maße ohne Achse (Radius) und
    Neben-Maße (Rundung, Wand) neutral: weißer Punkt mit Rand `handle-line`, Rundung mit Viertelkreis; Pille der
    Neben-Maße und alle Pfeile erst beim Überfahren. Nie zwei Bedeutungen in derselben Farbe. Griffe sind tiefenrichtig:
    was ein Körper verdeckt, ist blass (etwa 30 % Deckkraft), die Pille dazu ebenso.
  - Klassisch: weiße Quadrate 10 px mit dunklem Rand, Hebe-Pfeil, Maßkästchen.
- **Fehler und Hinweise** stehen als Karte am Modell, mit Linie zur Stelle (UX §9); nie als Fenster.
- Druckbett: Raster 5/10 mm (je nach Zoom), Achsen rot/grün, Ursprung als Punkt in der Mitte.

## 6. Bausteine

| Baustein | Regel |
|---|---|
| Hauptknopf | Fläche `accent`, Schrift weiß; höchstens einer je Bereich |
| Knopf | transparent mit Rahmen `line-2` |
| Hinweisknopf | Fläche `amber` (Lotse „Zeig mir wie“, Ein-Klick-Lösungen im Druckcheck) |
| Zahlenfeld | eine Komponente für alle Zahlen (Produktregel 2 in CLAUDE.md): Mono-Schrift, Einheit rechts, Achsen-Kennung links farbig |
| Segmentschalter | Gruppe auf `chrome`, gewähltes Segment `field-2` und fett |
| Schalter | 32 × 18 px, an = `accent` |
| Auswahlkarte | `raise` mit Rahmen; gewählt: Rahmen `accent-line`, Fläche `accent-soft` |
| Kennzeichnung „wird bearbeitet“, „geht nicht“, „passt“ | kleine Pille in `accent-soft` / `bad-soft` / `ok-soft` mit Symbol |

## 7. Startseite, Export, Einstellungen

- **Startseite:** Navigation links (Start, Missionen, Baukästen, Meine Projekte, Bausteine, Drucker,
  Einstellungen), rechts „Weitermachen“ mit Missionsfortschritt, „Zuletzt geöffnet“, Baukästen als Kacheln,
  Missionen als Karten.
- **Export-Assistent** (Fenster, weil keine Geometrie-Operation): drei Spalten „Wofür ist die Datei?“ und
  Format · Prüfung (beim 3D-Druck der Druckcheck) · Vorschau mit Kennzahlen. Zwecke laut R20: 3D-Druck,
  Spiel-Engine, Andere Programme, Web & AR, Bild. Formate über die Architektur hinaus (glTF/GLB, FBX, USDZ)
  erst nach eigenem Arbeitspaket mit ADR.
- **Einstellungen** (Zahnrad, `⌘,`): Aussehen (Design, Akzentfarbe, Sprache), Griffe mit Vorschaubild,
  Bedienung (Maus-Stil, Lotse).

## 8. Was beim Umsetzen zu prüfen ist

- Screenshot-Vergleich je Hauptansicht (dunkel und hell) gegen das Mockup als Teil der Abnahme.
- Kontrast-Test über alle Token-Paare und über die abgeleiteten Akzentfarben (Vorschläge plus Extremwerte
  Weiß, Schwarz, Gelb, Hellgrün).
- Alle sichtbaren Begriffe stehen im Glossar (UX §12), in Deutsch und Englisch.
