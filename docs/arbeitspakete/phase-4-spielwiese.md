# Phase 4 – Spielwiese (Deko und Freiform)

**Ziel:** Das, was Fusion nicht kann: verspielte, organische Formen, Muster, Texte auf Rundungen, Reliefs aus Bildern und Generatoren, die in Sekunden etwas Schönes erzeugen. Alles bleibt im Rezept änderbar und lässt sich mit präzisen Teilen kombinieren.

**Meilenstein M4 (Demo):** Ablauf C „Weihnachtsstern“ aus dem [UX-Konzept](../02-ux-konzept.md#c-weihnachtsstern-ziel-spaß--10-min), inkl. nachträglicher Änderung der Zackenanzahl.

---

### AP-4.1 Freie Körper als vollwertige Körper
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.7, AP-1.10
**Bauanleitung:** [anleitungen/AP-4.1.md](anleitungen/AP-4.1.md)

**Umfang:**
- Netz-Körper (Manifold) im Feature-Graph: Import (STL/3MF/OBJ), Transformieren, Booleans mit präzisen Körpern (automatische Tessellierung), Farbe, Export
- Netz-Reparatur beim Import (Löcher schließen, Normalen, Selbstüberschneidungen) mit Bericht
- Badge „frei geformt“ + Erklärung; ausgegraute Werkzeuge mit Begründung (siehe [UX §6](../02-ux-konzept.md#6-präzise-körper-und-freie-formen-ein-modell))
- Typischer Remix-Anwendungsfall: STL von Printables importieren, Loch hinzufügen, Namen gravieren

**Akzeptanzkriterien:**
- [ ] 20 reale STL-Dateien (inkl. kaputter) werden importiert, repariert oder verständlich abgelehnt
- [ ] Boolean aus präzisem und freiem Körper funktioniert und ist im Rezept änderbar

---

### AP-4.2 Weich verschmelzen („Knete“)
**Status:** offen · **Größe:** XL · **Abhängigkeiten:** AP-4.1
**Bauanleitung:** [anleitungen/AP-4.2.md](anleitungen/AP-4.2.md)

> Beginnt mit einem Spike: SDF aus Netzen per eigenem BVH + Winding Number **vs.** OpenVDB. Entscheidung per ADR.

**Umfang:**
- Smooth Union, Smooth Subtract, Smooth Intersect mit Regler „Weichheit“ (mm)
- Auflösung automatisch nach Größe, Weichheit und **Düsendurchmesser des Druckerprofils** (Ziel: keine sichtbaren Facetten im Druck), mit Regler „Feinheit“ unter ▸ Mehr; Vorschau grob, Ergebnis fein
- Präzise Eingänge werden über SDF verrechnet, Ergebnis ist frei
- Bonus: „Aufblasen/Schrumpfen“ (Offset im Feld), „Glätten“

**Akzeptanzkriterien:**
- [ ] Kugel + Stern (Ablauf C) verschmilzt in < 2 s in Normalqualität
- [ ] Ergebnis ist immer wasserdicht und druckbar

---

### AP-4.3 Verformen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-4.1
**Bauanleitung:** [anleitungen/AP-4.3.md](anleitungen/AP-4.3.md)

**Umfang:**
- Verdrehen (Twist), Biegen (Bend), Verjüngen (Taper), Wellen/Rauschen (Noise), Kugelig (Spherize)
- **Low-Poly** als bewusster Deko-Stil (Facetten-Anzahl wählbar). Das ist der einzige Weg zu sichtbaren Facetten ([UX §7.3](../02-ux-konzept.md#73-niedrige-auflösung))
- Griffe für Stärke und Achse; automatische Netzverfeinerung für glatte Ergebnisse
- Typische Nutzung: gedrehte Vase, gebogenes Namensschild

**Akzeptanzkriterien:**
- [ ] Verdrehte Vase aus Polygon-Prisma in 3 Klicks

---

### AP-4.4 Text und Logos: Prägen und Gravieren
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.5, AP-4.1
**Bauanleitung:** [anleitungen/AP-4.4.md](anleitungen/AP-4.4.md)

**Umfang:**
- Text auf Pfad und Kreisbogen (Skizze), Zeilen, Ausrichtung, Laufweite, Fett/Kursiv (je nach Schrift)
- **Prägen/Gravieren auf ebene Flächen präzise** (B-Rep), **auf gekrümmte Flächen** (Zylinder präzise via Abwicklung, beliebige Flächen über Netz-Projektion)
- Logos aus SVG ebenso
- Mitgelieferte freie Schriften (OFL) mit guter Druckbarkeit, darunter eine Schrift mit Stegen für Schablonen

**Akzeptanzkriterien:**
- [ ] „2026“ auf einer Kugel graviert (Ablauf C) und auf einem Becher (Zylinder) präzise geprägt

---

### AP-4.5 SVG-, DXF- und Bild-Import für Konturen (Ausstechformen)
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-2.1
**Bauanleitung:** [anleitungen/AP-4.5.md](anleitungen/AP-4.5.md)

**Umfang:**
- SVG/DXF → Skizze (Kurven bleiben Kurven), Skalierung auf Zielmaß beim Import
- **Bild nachzeichnen** (Schwarz-Weiß-Zeichnung → Kontur): potrace (GPL-2.0-or-later, kompatibel zur Projektlizenz) einbinden oder eigene Implementierung (Schwellwert, Marching Squares, Kurvenglättung). Entscheidung nach Qualitätsvergleich
- **Ausstechform-Generator:** Kontur → Schneide (dünn, hoch) + Griffkante (breit, flach), Wandstärke aus dem Profil

**Akzeptanzkriterien:**
- [ ] Kinderzeichnung (Foto) → Ausstechform in < 2 min

---

### AP-4.6 Relief und Lithophanie
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-4.1
**Bauanleitung:** [anleitungen/AP-4.6.md](anleitungen/AP-4.6.md)

**Umfang:**
- Bild → Höhenkarte → Relief (eben); Lithophanie (eben, gebogen, Zylinder/Lampenschirm, Kugel) mit Min-/Max-Dicke, Rahmen, Vorschau „gegen Licht“
- Auf Flächen eines Körpers aufbringen (z. B. Relief auf Box-Deckel)

**Akzeptanzkriterien:**
- [ ] Lithophanie-Vorschau entspricht qualitativ dem Druck (Foto-Vergleich im Abschlussvermerk)

---

### AP-4.7 Muster und Oberflächenstrukturen
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-4.1, AP-2.8
**Bauanleitung:** [anleitungen/AP-4.7.md](anleitungen/AP-4.7.md)

**Umfang:**
- Durchbruch-Muster in Flächen: **Voronoi**, **Waben**, Kreise, Rauten; Randabstand, Stegbreite (≥ Mindestwand)
- Oberflächenstrukturen (Displacement): **Rändel** (funktional für Griffe), Holz, Leder, Hammerschlag, Wellen; Tiefe und Skalierung
- Lattice/Gitterfüllung für leichte Teile (▸ Mehr)

**Akzeptanzkriterien:**
- [ ] Voronoi-Deckel und Rändel-Drehknopf druckbar (Druckcheck grün)

---

### AP-4.8 Generatoren („Zauberformen“)
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.9
**Bauanleitung:** [anleitungen/AP-4.8.md](anleitungen/AP-4.8.md)

**Umfang:**
- Generator = parametrisches Feature mit Konfigurator-Panel und Live-Vorschau-Galerie
- Erste Generatoren: **Stern**, **Schneeflocke** (zufällig mit Seed + symmetrisch), **Christbaumkugel** (mit Aufhänger, teilbar in zwei Hälften), **Vase** (Profilkurve + Drehung + Muster; Vasenmodus-tauglich), **Namensschild**, **Zahnrad** (Evolvente, Modul, Zähne; funktional!), **Kette/Glieder**, **Box mit Deckel**
- Würfeln-Knopf („Überrasch mich“) für Zufallsvarianten

**Akzeptanzkriterien:**
- [ ] Jeder Generator liefert druckbare Ergebnisse für den gesamten Parameterbereich (Property-Tests)

---

### AP-4.9 Kaleidoskop-Modus
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-2.8, AP-2.1
**Bauanleitung:** [anleitungen/AP-4.9.md](anleitungen/AP-4.9.md)

**Umfang:**
- Radiale Symmetrie (n-fach, optional gespiegelt) als **Live-Modus**: Man zeichnet oder modelliert in einem Segment, alle anderen Segmente aktualisieren sich in Echtzeit
- Funktioniert in Skizzen (Schneeflocken, Mandalas) und mit Körpern

**Akzeptanzkriterien:**
- [ ] Eine eigene Schneeflocke in < 3 min von Hand

---

### AP-4.10 Freiform-Modellieren (Subdivision) *(optional)*
**Status:** zurückgestellt · **Größe:** XL · **Abhängigkeiten:** AP-4.1

**Umfang:**
- Subdivision-Modellieren (wie Fusion „Form“ / T-Splines, vereinfacht): Grundkörper, Flächen ziehen, Kanten schärfen, Symmetrie; OpenSubdiv für Auswertung
- Ergebnis als freier Körper; später ggf. Umwandlung in präzise Flächen (NURBS)

> Erst nach Nutzerfeedback zu Phase 4 entscheiden, ob das gebraucht wird oder ob „Weich verschmelzen“ und „Verformen“ ausreichen.
