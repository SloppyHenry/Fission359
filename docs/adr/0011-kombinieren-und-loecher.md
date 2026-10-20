# ADR-0011: Kombinieren und Löcher ohne Gruppieren

- **Status:** vorgeschlagen (wartet auf Freigabe durch den Product Owner)
- **Datum:** 2026-10-20
- **Arbeitspaket:** AP-1.7

## Kontext
AP-1.7 bringt Vereinen, Ausschneiden, Überlappung behalten und Teilen. Vor allem verlangt Regel 13: Eine Form „Als Loch“ schneidet
**live** aus, sobald sie einen Körper berührt, ohne Gruppieren, und bleibt als Geisterform greifbar (UX §7.1, Ablauf D).

Rahmen:
- Ein Schritt hat Eingänge (Körper früherer Schritte) und Ausgänge, die immer **neue** Körper sind (`Document::addFeature`). Einen
  Schritt, der einen vorhandenen Körper ändert, und den Begriff „verbraucht“ gibt es nicht.
- Regel 3: Jede Änderung ist ein Command und ein Rezept-Eintrag. Das Ausschneiden darf nicht nur in der Ansicht passieren.
- Namens-Vertrag (ADR-0003): Flächen des Ergebnisses brauchen stabile Namen; Raten ist verboten (Regel 6).
- ADR-0010 Punkt 3 kündigt für AP-1.7 an: gemeinsames Verschieben kombinierter Körper und den ersten „Ausweich-Schritt“ (beim Teilen).
- Booleans sind teuer (Regel 9): nicht im UI-Thread, Griff-Vorschau unter 50 ms für einfache Fälle (Architektur §12).
- Kernel-Spike (AP-0.5): Booleans exakt gut, Fuzzy nur als Rückfall.

## Optionen
1. **A: Ein Schritt-Typ `combine`, Live-Löcher pflegt das Programm als gewöhnliche Schritte.** Der Schritt hat Eingänge Ziel, dann
   Werkzeuge, und einen neuen Ausgang (das Ergebnis). Wer ein Loch ablegt oder bewegt, bekommt im selben Undo-Schritt den passenden
   `combine`-Schritt angelegt, ergänzt oder entfernt.
   · Nur ein neuer Typ; Rezept, Undo, Datei und Neuberechnung funktionieren wie bei allen Schritten · Vereinen, Ausschneiden und
   Live-Loch sind dieselbe Sache · Das Rezept zeigt ehrlich, was passiert („Ausschneiden“)
   · Das Programm schreibt Schritte, die der Nutzer nicht selbst angelegt hat (nachvollziehbar durch das Etikett am Loch).
2. **B: „Als Loch“ ist ein Wert am Ziel** (Liste der Löcher im Schritt des Ziels). Kein eigener Schritt.
   · Rezept kurz · jede Form und jeder Schritt-Typ müsste die Löcherliste kennen; Löcher in kombinierten Körpern und mehrere Ziele
   werden verwickelt; Vereinen bräuchte trotzdem einen eigenen Typ.
3. **C: Nur in der Ansicht schneiden** (Boolean beim Zeichnen, nicht im Dokument). Verletzt Regel 3 und Regel 8 (Export,
   Messen, Druckcheck sähen kein Loch); verworfen.

## Entscheidung
Wir wählen **Option A**:

1. **Ein Typ `combine`** mit dem Wort `mode` (`merge` | `subtract` | `intersect`). `inputs[0]` = Ziel, `inputs[1..]` = Werkzeuge. Ausgang:
   **ein** neuer Körper (das Ergebnis). Name, Farbe und Sichtbarkeit übernimmt er vom Ziel; im Rezept steht
   „Quader 1: 2 Löcher ausgeschnitten“.
2. **„Verbraucht“ wird abgeleitet, nicht gespeichert:** `Document::consumerOf(BodyId) → optional<FeatureId>`. Verbrauchte Körper
   erscheinen nicht mehr als eigene Körper (Liste, Ansicht, Auswahl). **Ausnahme:** Werkzeuge eines `subtract` bleiben als Geist
   sichtbar und greifbar (Regel 13). Mit der Option „Werkzeug-Körper behalten“ (`keepTools`) ist das Werkzeug nicht verbraucht und bleibt
   ein normaler Körper.
3. **Live-Löcher sind `combine/subtract`-Schritte, die das Programm pflegt.** Wird eine Form mit `hole = true` abgelegt, bewegt, in der
   Größe geändert oder „Als Loch“ umgeschaltet, prüft das Programm im **selben** Undo-Schritt die Berührung (Hüllquader, beim Loslassen
   zusätzlich Boolean nicht leer) und legt an, ergänzt oder entfernt das Werkzeug im `combine`-Schritt des Ziels. Schritt ohne Werkzeuge
   wird entfernt. Ziel beim Ablegen = Körper, auf dem das Loch abgelegt wurde; Änderung des Ziels (auch mehrere) über das Etikett am
   Loch. Mehrere Ziele = je Ziel ein `combine`-Schritt. Ist das Ziel schon kombiniert, hängt sich das Loch an dessen **letzten**
   `combine`-Schritt (keine neue Kette).
4. **Verschieben:** Ein kombinierter Körper wird verschoben, indem die Lage aller Formen, aus denen er besteht, **einschließlich seiner
   Löcher** gemeinsam geändert wird (ein Befehl, wie eine Tinkercad-Gruppe). Ein Loch allein verschieben bewegt nur das Loch.
   Der Ausweich-Schritt `place` aus ADR-0010 Punkt 3 kommt erst mit dem Teilen (1.7e), weil dort ein Schritt zwei Körper speist.
5. **Durchgehend:** Prismatische Werkzeuge (Quader, Zylinder, Prisma, Stern, Röhre, Text) werden im `combine`-Schritt entlang ihrer
   Hochachse über das Ziel hinaus verlängert (je 1 mm über den Hüllquader), solange der Wert `through` an ist (Standard an bei
   „Als Loch“). Kugel, Kegel, Pyramide, Keil, Halbkugel, Torus: nie durchgehend. Das Werkzeug wird dafür im `combine`-Schritt neu
   gebaut; die Form-Werte selbst bleiben unverändert (das Loch bleibt so groß, wie es der Nutzer gemacht hat).
6. **Kernel:** `GeometryKernel::boolean(op, target, tools)` exakt (Fuzzy 0), danach `ShapeUpgrade_UnifySameDomain`. Ergeben sich
   Splitterflächen oder unerwartet viele Körper, einmal mit Fuzzy 0,001 mm wiederholen und als Hinweis melden. Keine stille
   Reparatur: Der Hinweis bietet „bündig machen“ an. Zerfällt das Ergebnis in getrennte Teile, bleibt es **ein** Körper (Compound).
7. **Flächennamen:** Flächen, die aus einer Eingangsfläche hervorgehen, behalten ihren Namen (`F1/box/top`); neue Flächen aus einem
   Werkzeug heißen nach dem Werkzeug-Schritt (`F3/cylinder/side`). Grundlage ist `Modified/Generated/IsDeleted` von OCCT. Ist ein Name
   nicht eindeutig, ist das ein Fehler am Schritt (`namingContractViolated`), kein Raten.
8. **Berechnung im Hintergrund:** Während eines Zugs am Loch zeigt die Ansicht das letzte Ergebnis, die Neuberechnung läuft im
   Hintergrund und ist abbrechbar. Beim Zug selbst prüft nur der Hüllquader.

## Konsequenzen
- **Einfacher:** kein Gruppieren, keine Gruppen; Undo, Datei und Neuberechnung ohne Sonderweg. Vereinen, Ausschneiden und Live-Loch
  teilen eine Implementierung.
- **Schwerer:** Das Dokument bekommt den Begriff „verbraucht“ (abgeleitet); Liste, Szene und Auswahl müssen ihn kennen. Das Pflegen
  der Schritte beim Bewegen, Ändern und (später, AP-1.9) Löschen eines Lochs ist neue, sorgfältig zu testende Logik. Die Neuberechnung
  muss dem `combine`-Schritt die Werte des Werkzeug-Schritts geben (für „durchgehend“).
- **Folge-Arbeiten:** Etikett „schneidet aus: …“ (QML), Fehlertexte `combineNoOverlap/Empty/NearlyFlush/Invalid`, Glossar-Eintrag
  „Durchgehend“, `split` und `place` in 1.7e. Flächennamen nach Booleans sind die Grundlage für AP-2.10.
- **Revisionskriterium:** Wenn Einsteiger im Nutzertest (AP-1.13) die vom Programm angelegten Schritte im Rezept als verwirrend
  melden, oder wenn Neuberechnung beim Ziehen eines Lochs das Budget (50 ms Griff-Vorschau, einfache Fälle) nicht hält, prüfen wir
  Option B bzw. eine Vorschau-Näherung erneut.

## Belege
- UX §7.1 und Ablauf D (§15), `docs/spikes/kernel-robustheit.md`, `tests/geometry/test_booleans.cpp`.
- Tests folgen mit 1.7a–f (Golden-Tests Volumen, Fläche, BBox, Topologie, Gültigkeit; Ablauf D Schritte 2–4).
