# Prompt: Klickbares Mockup der Fission3D-Oberfläche

**Verwendung:** In einer neuen Sitzung im Fission3D-Repository einfügen. Das Ergebnis ist ein privates
Artifact (eine HTML-Seite) zum Anschauen und Klicken, kein Produktionscode.

---

## Prompt

```markdown
# Auftrag: Klickbares Mockup der Fission3D-Oberfläche

Halte dich an CLAUDE.md (Master-Prompt). Lies vor dem Entwurf:
- docs/01-vision.md (Design-Prinzipien, verbindlich)
- docs/02-ux-konzept.md, besonders §1 Bildschirmaufbau, §2 Interaktionsmodell, §4 Rezept,
  §9 Fehler und Hinweise, §11 Lernen, §12 Glossar, §13 Visuelle Gestaltung, §14 Barrierefreiheit,
  §15 Ablauf A (Schlüsselanhänger)
- docs/07-baukaesten.md (nur der Abschnitt zum Baukasten „Karte“)

## Ziel
Ein **klickbares Mockup**, das zeigt, wie sich Fission3D anfühlt. Es soll dem Product Owner (kennt
Tinkercad, programmiert nicht) als Entscheidungsgrundlage dienen: „So sieht es aus, so bedient man es.“
Es ist eine Attrappe: keine echte Geometrie, kein Speichern, keine Berechnung. Alles muss aber glaubwürdig,
in sich stimmig und mit Tastatur und Maus durchklickbar sein.

## Was das Mockup zeigen muss
Eine einzige HTML-Seite mit umschaltbaren Zuständen. Ein „Szenen“-Schalter (oder die Pfeiltaste →)
führt durch Ablauf A. Mindestens diese Zustände:

1. **Startbildschirm „Was möchtest du bauen?“** mit Kacheln: Karte, Vase/Gefäß, Figur, Christbaumschmuck,
   Schild, Halterung, Box, Ersatzteil, Leer. Darunter „Zuletzt geöffnet“.
2. **Arbeitsansicht leer:** Druckbett mit Raster und Abmessung (256 × 256 mm), ViewCube, Hinweiszeile.
3. **Ablauf A in Schritten** (jeweils ein Zustand):
   - Quader aufs Bett ziehen: Maß-Griffe an Ecken und oben, **Wertblase** am Zeiger.
   - Zahlen tippen: `50` Tab `20` Tab `4` Enter. Die Felder zeigen das Tippen live, Komma und Punkt gelten.
   - Kante überfahren: Viertelkreis-Griff, Abrunden mit `4`; Kontextleiste mit den passenden Aktionen.
   - Text „LENA“ geprägt: Eigenschaften rechts (Höhe, Schrift, Farbe).
   - **Loch als Geisterform** („Als Loch“-Schalter, Etikett „schneidet aus: Quader“) mit
     Abstandsmaßen („5 mm vom Rand“, anklickbar und editierbar).
   - Druckcheck: alles grün, ein Hinweis mit Ein-Klick-Lösung.
4. **Fehler als Hilfe:** „Abrunden mit 5 mm geht hier nicht.“ als Meldung am Ort des Problems, mit drei
   Ein-Klick-Knöpfen, roter Geistergeometrie und einem ⚠-Eintrag im Rezept. **Kein modaler Dialog.**
5. **Befehlssuche** (⌘K): „runde ecken“ → Abrunden, „hohl“ → Aushöhlen, „schraube“ → Bohrung.
6. **Baukasten „Karte“:** Schrittleiste Format → Hintergrund → Motive → Text → Feinschliff → Drucken.
7. **Dunkelmodus als Standard**, Umschalter zum Hellmodus, sowie ein Umschalter **Deutsch / English**.

## Verbindliche Regeln (aus dem Projekt, nicht verhandelbar)
- Aufbau wie UX-Konzept §1: Werkzeuggruppen oben (höchstens 8, Alltagsnamen), **Rezept links**
  (Sätze mit Symbol und Werten, Zeitmarker „jetzt“, Status grün / ⚠ / rot), **Eigenschaften rechts**
  (selten Gebrauchtes hinter „▸ Mehr“), **Hinweiszeile unten**. Die 3D-Ansicht hat mindestens 65 % der Fläche.
- **Anfassen statt Dialoge:** jede Operation hat einen Griff in der 3D-Ansicht. Eingabefelder ergänzen nur.
- **Jede Zahl ist eintippbar,** Formeln und Einheiten erlaubt, Komma und Punkt.
- **Alles bleibt änderbar:** das Rezept zeigt jeden Schritt als Satz, Klick öffnet die Bearbeitung.
- **Nur Begriffe aus dem Glossar** (UX §12), jeweils Deutsch und Englisch. Keine Fachwörter wie
  „Extrude“, „Fillet“, „Boolean“.
- **Fehler sind Hilfen:** was ist passiert, wo, was kann ich tun (Ein-Klick-Lösung).
- **Maße ändern verzerrt nie:** Rundungen und Löcher bleiben beim Ziehen rund (Ablauf D als Szene
  „Loch bleibt rund“ andeuten ist willkommen).
- **Barrierefreiheit:** Kontrast nach WCAG AA, Status nie nur über Farbe, sichtbarer Fokus, volle
  Tastaturbedienung, Schrift mindestens 12 pt, „Bewegung reduzieren“ respektieren.

## Gestaltung
**Wunsch des Product Owners: dunkel und modern, eine Mischung angelehnt an Photoshop und Blender.**
- **Dunkel ist der Standard** (der Hellmodus bleibt als Umschalter). Ruhige, abgestufte Grautöne für
  Flächen und Leisten, die 3D-Ansicht als dunkelste Fläche, damit die Modelle leuchten.
- **Angelehnt an Photoshop:** schlanke, klar getrennte Seitenleisten (Rezept, Eigenschaften, Ebenen-artige
  Körperliste), eine ruhige Optionsleiste oben unter der Werkzeuggruppen-Leiste, kompakte Zahlenfelder mit
  Reglern, einklappbare Bereiche.
- **Angelehnt an Blender:** großer 3D-Bereich mit Raster und farbigen Achsen, ViewCube bzw. Achsen-Gizmo
  oben rechts, Wertblase beim Ziehen, dezente Overlays, Hover-Hervorhebung mit Akzentfarbe.
- **Eigenständig bleiben:** keine Marken, Logos oder 1:1-Kopien dieser Programme; Begriffe nur aus dem
  Glossar, nichts von Blenders Fachsprache. Das Programm soll sich für Einsteiger freundlich anfühlen,
  nicht wie ein Profi-Werkzeug mit hundert Knöpfen.
- **Eine Akzentfarbe** (z. B. ein warmes Orange oder ein klares Blau, begründe deine Wahl), abgerundete
  Ecken, klare Typografie (Systemschrift oder Inter), Kontrast nach WCAG AA.
- Modelle wirken wie gedruckt (matter Kunststoff, weicher Schatten aufs Bett, feine Kantenlinien).
  Animationen 150–250 ms. Farbwerte als Tokens definieren, auch für den Hellmodus.

## Varianten
Liefere **zwei Varianten der Arbeitsansicht** zum Umschalten und vergleiche sie in einer kurzen Tabelle
(Vorteile und Nachteile für Einsteiger):
- **A – konzepttreu:** genau der Aufbau aus UX §1.
- **B – Alternative:** eine begründete, mutigere Idee (z. B. schwebende Werkzeugleiste, Rezept als
  untere Leiste, Eigenschaften als Karte am Objekt). Sie darf die Regeln oben nicht verletzen.

## Technik
- **Eine selbstständige HTML-Datei** ohne Build. Inline CSS und JS. Als Artifact veröffentlichen
  (privat; der Product Owner entscheidet über das Teilen).
- Die 3D-Ansicht ist eine **Attrappe:** isometrische SVG- oder Canvas-Darstellung, ein paar
  vorbereitete Ansichten. Eine 3D-Bibliothek ist nur erlaubt, wenn sie von cdnjs mit fester Version
  geladen wird und das Mockup klarer macht.
- Layout für 1280 × 800 bis 1920 × 1080 und auf Tablets bedienbar, ohne horizontales Scrollen.

## Arbeitsweise
1. Beginne mit **Zusammenfassung, offenen Fragen (höchstens 3, mit Empfehlung) und Plan** und warte auf das OK.
2. Baue dann in kleinen Schritten; zeige nach der Arbeitsansicht A einen Zwischenstand.
3. Schließe mit: Link zum Artifact, was das Mockup **nicht** zeigt, offene Fragen an den Product Owner
   und einer „So probierst du es aus“-Anleitung (3–6 Schritte).
4. Erfinde keine Funktionen, die nicht in den Dokumenten stehen. Wo du etwas ergänzt, markiere es als Vorschlag.
```
