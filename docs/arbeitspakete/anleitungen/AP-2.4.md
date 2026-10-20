# AP-2.4 Direktes Bearbeiten von Flächen

**Größe:** L, Teile a–e · **Phasendatei:** AP-2.4

## Ziel
Flächen beliebiger Körper direkt anfassen: **Fläche ziehen** (eben und zylindrisch: ein Loch größer machen), **intelligentes Strecken**
kombinierter Körper (Rundungen, Löcher und Text bleiben unverändert, Regel 11), **Fläche verschieben/drehen**, **Fläche entfernen**
(Rundungen, Löcher, Fasen „wegheilen“), **Fläche versetzen** (für Passungen). Funktioniert auch bei importierten STEP-Teilen ohne Verlauf.
Ablauf D Schritt 5 läuft automatisiert grün.

## Voraussetzungen
AP-1.4, AP-2.10 (Basis: Flächenverweise), AP-1.7 (kombinierte Körper). Für „importierte STEP-Teile“: AP-2.13 (sonst mit einem im Test
erzeugten Körper ohne Verlauf prüfen).

## Vorher lesen
- `docs/02-ux-konzept.md` §2.2 (ebene Fläche → Pfeil, Push/Pull), §7.2 (verbindlich: Strecken statt Skalieren), §15 D Schritt 5
- `docs/adr/0010` (Lage im Rezept), AP-2.10-Anleitung
- OCCT-Klassen: `BRepOffsetAPI_MakeOffsetShape` (Flächen-Offset), `BRepFeat_MakePrism`, `BRepAlgoAPI_Defeaturing`,
  `BRepOffsetAPI_MakeOffset`… (nur Hinweise; ausprobieren, was gültige Körper liefert)

## Haltepunkte
1. **Konflikt Pfeil auf Fläche vs. Gizmo/Griffe:** Heute zeigt ein gewählter Körper Gizmo bzw. Maß-Punkte. Empfehlung: Pfeil auf **gewählter
   Fläche** (Einfachklick wählt eine Fläche, AP-1.4) im Werkzeug „Größe“; bei Grundformen bleibt der Maß-Punkt (ändert den Parameter, Regel 11),
   bei allen anderen Körpern erscheint der Flächen-Pfeil (Push/Pull). Mockup/Bild vorher.
2. **Strecken: Wo liegt die Schnittebene?** Empfehlung nach Phasendatei: automatisch an einer Stelle ohne Details (Ebene senkrecht zur
   Zugrichtung, die keine gekrümmte Fläche, kein Loch und keinen Text schneidet; Suche über die Ausdehnung), sichtbar und verschiebbar.
   Findet sich keine: Meldung „Hier gibt es keine Stelle ohne Rundung oder Loch. Wähle die Stelle selbst.“

## Teil 2.4a – Fläche ziehen (Push/Pull)
1. Feature-Typ `movePlanarFace` bzw. `offsetFace`: Eingang Körper, `refs["face"]`, Wert `distance`. Ebene Fläche: Fläche entlang Normale
   verschieben (`BRepFeat_MakePrism` aus der Fläche bzw. Offset nur dieser Fläche); Zylinderfläche: Radius ändern (Offset der Fläche).
2. Pfeil-Griff nach Haltepunkt 1; Tippen ersetzt Ziehen; Ein Undo-Schritt je Zug.
3. Golden: Quader-Seite +5 → Volumen +5·Fläche; Loch Ø6 auf Ø8 (Zylinderfläche −1 mm Offset nach innen-außen beachten) → Volumen nach Formel;
   **Akzeptanzkriterium 1** mit einem Körper ohne Verlauf (STEP oder `importedSolid`-Testfeature).

## Teil 2.4b – Intelligentes Strecken
1. Ablauf: Körper an Ebene teilen (`split` aus AP-1.7e intern), eine Hälfte um Δ verschieben, Lücke mit dem **Querschnitt** füllen
   (Schnittfläche extrudieren), alles vereinen. Als **ein** Feature-Typ `stretch` (Eingang Körper, Wort `axis` bzw. Richtung, Werte
   `position` (Schnittebene), `distance`).
2. Schnittebene automatisch (Haltepunkt 2): GAL-Hilfsfunktion `std::optional<double> plainCut(const NamedSolid&, axis)` sucht entlang der
   Achse eine Lage, deren Querschnitt nur ebene Flächen schneidet, die parallel zur Achse laufen.
3. Ablauf D Schritt 5 als Test (Akzeptanzkriterium 3): Quader 40×40×20 mit durchgehendem Loch Ø6 (aus AP-1.7) an der rechten Seite um 20 mm
   breiter ziehen → Loch bleibt rund Ø6 (Kreis-Prüfung an Netzpunkten, Abweichung < 1e-6 bei exakter Geometrie: über die Zylinderfläche
   prüfen, Radius = 3), Abstand zur linken Kante unverändert, Volumen nach Formel.
4. Griff: bei kombinierten Körpern der Flächen-Pfeil (Haltepunkt 1) **streckt** statt die Fläche zu verschieben, wenn die Fläche eine
   Außenseite ist und der Körper Details hat; Hinweiszeile sagt „Strecken: Rundungen und Löcher bleiben, wie sie sind.“

## Teil 2.4c – Fläche verschieben/drehen
Gewählte Fläche(n) mit dem Gizmo verschieben (Translation) oder drehen (Ring); OCCT: Fläche ersetzen und Nachbarn anpassen
(`BRepOffsetAPI_*`/`LocOpe`) – die schwierigste Operation; Golden für einfache Fälle (Seitenfläche eines Quaders schräg stellen = Keil-Volumen).
Scheitert der Kernel: Meldung, nie ungültiges Ergebnis.

## Teil 2.4d – Fläche entfernen
`removeFaces`: `refs["faces"]`, `BRepAlgoAPI_Defeaturing`. Golden: Loch entfernen → Volumen wie Quader ohne Loch (Akzeptanzkriterium 2);
Rundung entfernen → scharfe Kante. Kettenauswahl: Klick auf eine Lochwand nimmt alle Flächen des Lochs (Hilfsfunktion „Merkmal erkennen“:
zusammenhängende Flächen, die gemeinsam ein Loch/eine Rundung bilden).

## Teil 2.4e – Fläche versetzen
`offsetFaces`: alle gewählten Flächen um d nach außen/innen (für Passungen, AP-3.3 nutzt es). Golden: Quader alle Flächen −0,2 → Kantenlängen −0,4.

## Für jeden Teil
Naming (neue Flächen benennen, Szenarien in der AP-2.10-Suite), Meldungskatalog, Regel 10, Rezeptsätze, Dateiformat (neue Typen).

## Abnahme
Kriterien 1–3 als Tests. So probierst du es aus: Quader mit Loch (Als Loch, AP-1.7), Fläche rechts anklicken, Pfeil ziehen → Körper wird länger,
Loch bleibt rund und an seinem Platz. Lochwand wählen, Pfeil → Loch größer. Rechtsklick Lochwand ▸ Fläche entfernen → Loch zu.

## Nicht tun
Kein ungleichmäßiges Skalieren als Ersatz für Strecken (Regel 11). „Verzerren“ (UX §7.2) ist ein eigenes Werkzeug mit Warnung (Ablauf D Schritt 7) –
gehört hierher als Teil f nur, wenn der Product Owner es will (Haltepunkt); sonst Phase 4 (Verformen).
