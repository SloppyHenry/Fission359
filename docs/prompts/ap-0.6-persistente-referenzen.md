# Prompt: AP-0.6 Persistente Referenzen (Topological Naming)

**Verwendung:** In einer neuen Sitzung im Fission3D-Repository einfügen. Der Prompt enthält den Stand, die
bereits getroffenen Entscheidungen und den freigegebenen Plan. Entscheidungen, die du anders treffen willst,
vor dem Absenden im Text ändern.

---

## Prompt

```markdown
# Auftrag: AP-0.6 Persistente Referenzen (Topological Naming)

Halte dich an CLAUDE.md (Master-Prompt). Lies zuerst `docs/arbeitspakete/phase-0-fundament.md` (AP-0.5 und
AP-0.6), `docs/03-architektur.md` §3.4, `docs/spikes/kernel-robustheit.md` und `docs/adr/`.

## Stand (13.10.2026)
- AP-0.1, 0.2, 0.3, 0.5 stehen im Status **Review**, AP-0.4 ist **erledigt** (ADR-0002: Viewport auf Qt RHI;
  Intel-HD-620-Messung bestanden, Budget erfüllt). CI ist grün auf macOS arm64 und Ubuntu 24.04.
- Entschieden: Export-Winkeltoleranz 5° (R16); Oberfläche nach dem Mockup (R17–R20).
- Standard-Linux-Testrechner ist `secondtux` (SSH-Alias, geteilt: `FISSION_JOBS=4`, `nice -n 19`, keine
  fremden Prozesse anfassen). `vierstein` bleibt für den sauberen Ubuntu-24.04-Containertest. Details:
  `docs/testumgebung.md`. Mein Projektgedächtnis (`project-state`) hat den Rest.

## Schritt 0: Mockup übernehmen
Das Mockup ist fertig. Der Branch `mockup-oberflaeche` (4 Commits, enthält `docs/mockups/oberflaeche.html`,
Änderungen in UX §1/§13 und R17–R20) ist mit `main` sauber zusammenführbar. **Führe ihn jetzt in `main`
zusammen und pushe** (Merge-Commit, kein Squash), danach prüfe, dass die Links in `docs/README.md` und
`docs/06-rueckfragen.md` auf das Mockup funktionieren. Das ist hiermit freigegeben.

## Entscheidungen des Product Owners (aus der Planvorstellung)
- **Frage 1 – Strenge der Ähnlichkeitssuche: a) streng.** Nur ein klarer, eindeutiger Gewinner wird
  automatisch übernommen, sonst „braucht Hilfe“. Der Prototyp misst zusätzlich die großzügige Variante
  (Treffer mit ⚠ „bitte prüfen“), damit die Entscheidung bei AP-2.10 auf Zahlen beruht.
- **Frage 2 – Tiefe des OCAF-Vergleichs: a) kleiner Praxistest.** Ein bis zwei Szenarien mit OCAF/TNaming
  nachbauen und messen; FreeCAD 1.0 nur anhand der Dokumentation vergleichen, ohne Code zu übernehmen.
- Annahmen (gelten, solange nicht widersprochen): Skizzen im Prototyp sind einfache Linienzüge mit festen
  Segment-Nummern, ohne PlaneGCS. Der Prototyp liegt in `tests/naming/`, läuft in der CI auf Mac und Linux,
  ist kein Produktionscode (AP-2.10 baut die Lösung später richtig in den Kern). OCCT-Module TKCAF/TKLCAF
  werden nur für die Tests dazugeschaltet; jede weitere Abhängigkeit vorher ansprechen.

## Plan (bereits vorgestellt, freigegeben)
1. Gerüst: Namen, Namens-Karte, Referenz mit Ähnlichkeitsmerkmalen (Art, Schwerpunkt, Normale,
   Größe, Nachbarn), Status „gefunden / Rückfall / braucht Hilfe“.
2. Vier Mini-Features mit Namensvergabe über die OCCT-Historie (`Generated/Modified/IsDeleted`,
   `BRepTools_History`): Quader, Extrudieren eines Linienzugs, Bohrung, Abrunden.
3. Auflösung in drei Stufen (Name → Ähnlichkeit → Hilfe) mit Plausibilitätsprüfung.
4. Szenarien aus dem AP: Quader → Kante abrunden → Höhe ändern; Skizze → Extrudieren → Fläche anbohren →
   Skizze um ein Segment erweitern; ein Feature davor einfügen.
5. „Böse Fälle“, in denen Raten gefährlich ist: Fläche teilt sich, Fläche verschwindet, zwei gleiche
   Flächen tauschen die Plätze. Richtig ist jeweils „braucht Hilfe“.
6. Zufallsprobe mit einigen hundert zufälligen Änderungen; der Wahrheitswert kommt aus einer geometrischen
   Abfrage, nicht aus dem Namenssystem.
7. OCAF-Praxistest, FreeCAD aus der Dokumentation.
8. Bericht `docs/spikes/persistente-referenzen.md`, **ADR-0003 „Persistente Referenzen“** (Entscheidung mit
   Zahlen), Abschlussvermerk mit „So probierst du es aus“; Lauf auf Mac, secondtux und in der CI.

**Erfolgsmaß:** Hart: **null still falsche Zuordnungen** in allen Szenarien und in der Zufallsprobe.
Weich (nur gemessen): Anteil automatisch richtig gefunden gegenüber „braucht Hilfe“.

## Arbeitsweise
- Fasse in fünf Zeilen dein Verständnis zusammen und **starte dann direkt mit Schritt 0 und 1**; der Plan
  ist freigegeben. Halte an und frage, wenn sich etwas ergibt, das den Plan oder die Architektur ändert.
- Kleine, lauffähige Schritte, nach jedem Schritt bauen und testen; Commits `AP-0.6: …`.
- Status am Ende ehrlich setzen: „Review“, nicht „erledigt“. Was nicht belegt ist, benennst du als offen.
- Bei der Rückmeldung kurz und in Alltagssprache; Fachbegriffe über ihre Wirkung erklären.
```
