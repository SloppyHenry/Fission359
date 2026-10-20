# AP-1.7 Kombinieren und Löcher ohne Gruppieren

**Größe:** L, zerlegt in a–f (je ein PR) · **Phasendatei:** `phase-1-erster-koerper.md`, AP-1.7

## Ziel
Vereinen, Ausschneiden, Überlappung behalten und Teilen. Vor allem: **Löcher wirken live.** Eine Form „Als Loch“, die einen Körper
berührt, schneidet sofort aus, ohne Gruppieren (Regel 13, UX §7.1). Das Loch bleibt als Geisterform greifbar, verschiebbar und in der
Größe änderbar; ein Etikett zeigt „schneidet aus: Quader 1“. Löcher gehen standardmäßig ganz durch, auch wenn der Körper später
dicker wird. Ablauf D Schritte 2–4 laufen automatisiert grün.

## Voraussetzungen
AP-1.5 (erledigt), AP-1.6 erledigt (wegen gemeinsamem Verschieben kombinierter Körper und `moveBodyBy` aus 1.6e).

## Vorher lesen
- [Handbuch](handbuch.md) §4, §5, §6, §10; `docs/02-ux-konzept.md` §6, §7.1, §9, §15 D
- `docs/adr/0003-persistente-referenzen.md`, `docs/adr/0008-dokumentmodell-und-kernel-schnittstelle.md`,
  `docs/adr/0010-lage-im-rezept.md` (Punkt 3 und „Offen für AP-1.7“)
- `docs/spikes/kernel-robustheit.md`: „Empfohlene Standardeinstellungen“ (Booleans, Fuzzy) und „Bekannte Problemfälle“
- `tests/geometry/test_booleans.cpp` (wie OCCT-Booleans geprüft werden)
- `src/core/src/document.cpp` (`addFeature`: Eingänge müssen von früheren Schritten stammen; Ausgänge sind immer **neue** Körper)
- `src/core/src/recompute.cpp` (wie Eingänge an `execute` gehen)

## Ausgangslage im Code
- `Body::hole` (Schalter „Als Loch“) existiert und wird als Geist gezeichnet; geschnitten wird noch nicht.
- Ein Schritt hat `inputs` (Körper früherer Schritte) und `outputs` (immer neu angelegte Körper). Es gibt keinen Schritt, der einen
  vorhandenen Körper „ändert“, und keinen Begriff „verbraucht“.
- Der Kernel (`GeometryKernel`) kann noch keine Booleans.
- Menü Körper hat `merge`, `subtract`, `overlap`, `split` (aus).

## Teil 1.7a – Entwurf festlegen (ADR-0011), dann Kern
**Erst den ADR schreiben und freigeben lassen. Kein Produktionscode vor der Freigabe.**

### Empfohlener Entwurf (in den ADR übernehmen, Alternativen mit Vor- und Nachteilen daneben)
1. **Ein Feature-Typ `combine`** mit dem Wort `mode` (`merge` | `subtract` | `intersect`) in `Feature::texts`. Eingänge: zuerst das
   **Ziel**, dann die **Werkzeuge** (`inputs[0]` = Ziel, `inputs[1..]` = Werkzeuge). Ausgang: **ein neuer Körper**, das Ergebnis
   (`newBodies = 1`). Name, Farbe und Sichtbarkeit übernimmt der neue Körper vom Ziel.
2. **Verbraucht** ist ein Körper, der Eingang eines späteren `combine`-Schritts ist. Das wird **abgeleitet**, nicht gespeichert
   (`Document::consumerOf(BodyId) → optional<FeatureId>`). Verbrauchte Körper erscheinen nicht mehr als eigene Körper in der Liste
   und in der Ansicht, **außer** Werkzeuge eines `subtract`: Die bleiben als Geist sichtbar und greifbar (Regel 13).
   Für den Nutzer gibt es also nur das Ergebnis; seine Teile sind über den Geist (Löcher) und das Rezept erreichbar.
3. **Live-Löcher sind gewöhnliche `combine/subtract`-Schritte, die das Programm selbst pflegt.** Wenn eine Form mit `hole = true`
   abgelegt oder verschoben wird und einen sichtbaren, nicht verbrauchten Körper berührt (Hüllquader überlappen und Boolean nicht
   leer), legt das Programm in **demselben** Undo-Schritt einen `combine`-Schritt an (Ziel = berührter Körper, Werkzeug = Loch) oder
   ergänzt einen vorhandenen `combine`-Schritt dieses Ziels um das Loch. Verlässt das Loch den Körper ganz, wird es aus dem Schritt
   genommen (Schritt ohne Werkzeuge wird entfernt). Ziel beim Ablegen = Körper, auf dem das Loch abgelegt wurde (UX §7.1).
4. **Verschieben eines kombinierten Körpers** ändert die Lage aller Formen, aus denen er besteht, gemeinsam (ADR-0010 Punkt 3),
   **einschließlich seiner Löcher** (Löcher wandern mit, wie eine Tinkercad-Gruppe). Ein Loch selbst verschieben bewegt nur das Loch.
5. **Durchgehend** (1.7c): Ein Werkzeug, dessen Form „prismatisch“ ist (Quader, Zylinder, Prisma, Stern, Röhre, Text), wird im
   `combine`-Schritt entlang seiner eigenen Hochachse über das Ziel hinaus verlängert, solange der Wert `through` der Loch-Form an ist
   (Standard: an, sobald „Als Loch“ eingeschaltet wird). Kugel, Kegel, Pyramide, Keil, Halbkugel, Torus: nie durchgehend (Schalter aus).
6. **Booleans im Kernel** exakt (Fuzzy 0), danach gleiche Flächen zusammenfassen (`ShapeUpgrade_UnifySameDomain`). Ergibt das
   Splitterflächen oder mehr Körper als erwartet, einmal mit Fuzzy 0,001 mm wiederholen und das als Hinweis melden (AP-0.5).
7. **Flächennamen des Ergebnisses** (Namens-Vertrag ADR-0003): Flächen, die aus einer Eingangsfläche hervorgehen, behalten deren
   Namen (`F1/box/top`); neue Flächen aus einem Werkzeug heißen nach dem Werkzeug-Schritt (`F3/cylinder/side`, als Schnittwand).
   OCCT liefert das über `BRepAlgoAPI_BooleanOperation::Modified/Generated`. Kann eine Fläche nicht eindeutig benannt werden, ist
   das ein Fehler am Schritt (`namingContractViolated`), kein Raten.

### Haltepunkte für 1.7a (mit dem ADR vorlegen)
1. Den Entwurf oben freigeben (oder Alternative wählen). Alternative B: „Als Loch“ wird ein Wert am **Ziel** (Liste der Löcher im
   Ziel-Schritt), kein eigener Schritt. Nachteil: Ziel-Schritte müssten alle diesen Wert kennen. Empfehlung: Entwurf oben.
2. Wandert ein Loch mit, wenn man den Körper verschiebt, aus dem es schneidet? Empfehlung: **ja**.
3. Wie heißt der kombinierte Körper? Empfehlung: Name des Ziels; im Rezept steht „Quader 1: 2 Löcher ausgeschnitten“.

### Kern-Schritte (nach Freigabe)
1. GAL: `virtual NamedSolid boolean(BooleanOp op, const NamedSolid& target, const std::vector<NamedSolid>& tools) const;`
   (`enum class BooleanOp { merge, subtract, intersect }`), Doku: Namensweitergabe, Fehlercodes (leeres Ergebnis =
   `GeometryErrorCode::emptyResult` neu, mehrere Körper erlaubt bei subtract → als ein Körper mit mehreren Schalen? **Nein:**
   zerfällt das Ergebnis in getrennte Teile, ist es trotzdem **ein** Körper (Compound); der Druckcheck meldet später lose Teile).
2. OCCT-Umsetzung in `src/geometry/occt/src/booleans.cpp` mit `BRepAlgoAPI_Fuse/Cut/Common`, `SetRunParallel(false)`,
   `UnifySameDomain`, Namensweitergabe über `Modified/Generated/IsDeleted`, `BRepCheck_Analyzer`.
3. Golden-Tests (Handbuch §10) in `tests/unit/geometry/test_booleans_gal.cpp`: Quader 40×40×10 minus Zylinder Ø6 durch → Volumen
   = 16000 − π·9·10, Flächenzahl 7, gültig, Namen: die 6 Quaderflächen behalten ihre Namen, die Lochwand heißt nach dem Zylinder;
   zwei Quader vereinen (überlappend) → Volumen nach Formel; Überlappung; Werkzeug außerhalb → `subtract` gibt das Ziel unverändert
   zurück (kein Fehler), `intersect` gibt `emptyResult`.
4. Core: `Document::consumerOf`, Feature-Typ `combine` in `src/features/src/combine.cpp` (`validate`: mindestens ein Werkzeug;
   `execute`: `kernel.boolean`; `describe`: Schlüssel `combine.<mode>` mit Anzahl der Werkzeuge; `handles`: keine).
5. `DocumentModel`: verbrauchte Körper aus `bodies` (Liste) und der Szene nehmen; Werkzeuge von `subtract` als Geist zeichnen
   (wie heute `hole`). `stepOfBody` eines Geists = der Schritt, der ihn **macht** (damit Griffe und Panel funktionieren).
6. Tests (Core): Hinzufügen/Entfernen eines `combine`-Schritts per Command, Undo; Verbraucht-Ableitung; Neuberechnung rechnet nur
   neu, was sich geändert hat (Zähler am Kernel, Vorbild `test_recompute.cpp`).

## Teil 1.7b – Live-Löcher
1. Nach `finishPlacing`, nach jedem Verschiebe-Zug und nach „Als Loch“ an/aus: Funktion `void DocumentModel::updateHoleCuts(BodyId hole)`
   im **selben** Undo-Schritt (vor `commitChange`). Sie prüft Berührung (Hüllquader überlappen, dann Kernel-Boolean nicht leer –
   die genaue Prüfung im Hintergrund ist zu langsam für den Zug; deshalb: während des Zugs nur Hüllquader, beim Loslassen genau).
2. Etikett „schneidet aus: Quader 1“ am Loch (QML-Overlay, Vorbild Griff-Beschriftungen). Klick öffnet eine kleine Liste der
   berührten Körper mit Häkchen; Ändern = Command, der Ziele des `combine`-Schritts ändert (auch mehrere Ziele: dann je Ziel ein
   `combine`-Schritt).
3. Undo: Ablegen eines Lochs auf einem Körper = **ein** Schritt in „Letzte Aktionen“ („Zylinder als Loch hinzugefügt“), nicht zwei.
4. Tests: Ablauf D Schritt 2 und 3 als UI-Tests und Fenster-Test (`app_flow_d_hole`): Zylinder Ø6 „Als Loch“ auf Quader ablegen →
   Volumen des Ergebnisses = Quader − Zylinder; Etikett-Ziel = Quader; Geist verschieben um 10 mm → Ergebnis neu, ein Undo-Schritt.

## Teil 1.7c – Durchgehend
1. Neuer Wert `through` (Zahl 0/1 oder bool-Wert, Typ `QuantityKind::number`) an den prismatischen Formen, Standard 1, wirksam nur bei
   `hole = true`. Panel: Schalter „Durchgehend“ (Glossar ergänzen, falls nötig: Haltepunkt).
2. `combine/subtract`: Werkzeug mit `through` entlang seiner Hochachse (lokale +Z nach Drehung) so verlängern, dass es den Hüllquader
   des Ziels auf beiden Seiten um 1 mm überragt. Umsetzung: Feature-Typ bekommt `virtual std::optional<ResolvedParams>
   throughParams(const ResolvedParams&, double below, double above) const` (gibt die Werte mit größerer Höhe und verschobenem z
   zurück); der `combine`-Schritt baut das Werkzeug damit neu. Formen ohne diese Funktion sind nie durchgehend.
   Dafür braucht `combine` den Typ und die Werte des Werkzeug-Schritts: über eine Abfrage an die Neuberechnung (`ExecuteInput`
   um die Werte der Eingangs-Schritte erweitern). Vorher prüfen, ob das sauber in `recompute.cpp` passt; sonst Haltepunkt.
3. Test: Ablauf D Schritt 4 (Quaderhöhe 10 → 20, Loch geht weiter durch: Volumen = 40·40·20 − π·9·20).

## Teil 1.7d – Werkzeuge Vereinen, Ausschneiden, Überlappung behalten
1. Mehrere Körper wählen → Menü Körper ▸ Vereinen / Ausschneiden / Überlappung behalten. Ziel = zuerst gewählter Körper.
2. Ausschneiden hat die Option „Werkzeug-Körper behalten“ (▸ Mehr im Panel des Schritts): dann ist das Werkzeug nicht verbraucht
   (bleibt als normaler Körper sichtbar). Umsetzung: Wort `keepTools` im Schritt.
3. Regel 10: Tooltip-Karten, Hinweiszeile, Synonyme („vereinen“, „verbinden“, „gruppieren“, „zusammenfügen“, „merge“, „union“,
   „join“, „group“; „ausschneiden“, „abziehen“, „loch“, „subtract“, „cut“; „überlappung“, „schnittmenge“, „intersect“, „common“).
   **„gruppieren“ als Synonym für Vereinen** hilft Tinkercad-Umsteigern.
4. Tests: je Modus Golden + Undo; Vereinen von drei Körpern ist **ein** Schritt.

## Teil 1.7e – Teilen
1. Feature-Typ `split`: Eingang ein Körper, Werte: Ebene (Achse x|y|z als Wort, Lage in mm). Ausgang: **zwei** neue Körper (`newBodies = 2`).
   (Teilen an einer Fläche eines Körpers: Haltepunkt, Empfehlung erst in Phase 2 mit Hilfsebenen AP-2.12.)
2. Griff: die Schnittebene als halbtransparente Fläche mit einem Pfeil-Griff (Vorbild Gizmo-Pfeil), Tippen ersetzt Ziehen.
3. Die zwei Teile sind einzeln verschiebbar: Das ist der erste Fall von ADR-0010 Punkt 3 („Ausweich-Schritt Verschoben“), weil ein
   Schritt zwei Körper speist. Feature-Typ `place` (Eingang ein Körper, Werte x, y, z, rotX… als Verschiebung) bauen; Verschieben eines
   Teils legt ihn an oder ändert ihn. Haltepunkt, falls das im Nutzertest schwer verständlich wirkt.
4. Golden: Quader in der Mitte teilen → zwei Körper mit je halbem Volumen, je 6 Flächen, die Schnittflächen benannt (`F<n>/split/cutA`, `cutB`).

## Teil 1.7f – Fehlerfälle und Abnahme
1. Fehlercodes (neu in `FeatureIssueCode`): `combineNoOverlap` (Werkzeug berührt das Ziel nicht; bei Live-Löchern kein Fehler, dann
   gibt es keinen Schritt), `combineEmpty` (Überlappung behalten ohne Überlappung), `combineNearlyFlush` (Fuzzy-Fall, Hinweis mit
   Ein-Klick „bündig machen“ = Wiederholung mit Fuzzy), `combineInvalid` (Ergebnis ungültig). Texte in QML (bis AP-1.12/2.14).
2. Alle Kombinationen aus `tests/geometry/test_booleans.cpp` (AP-0.5) über die GAL laufen lassen: gültig oder verständliche Meldung
   (Akzeptanzkriterium 1).
3. Ablauf D Schritte 2–4 als ein Fenster-Test (`app_flow_d_2_4`) und als UI-Test; „es gibt keinen Schritt Gruppieren“ heißt: das Rezept
   enthält nach Schritt 2 genau Quader, Zylinder, Ausschneiden.

## Abnahme
- Akzeptanzkriterien 1–3 der Phasendatei nachweisbar; CI grün.
- So probierst du es aus: Quader ablegen; Formen ▸ „Als Loch“ an, Zylinder auf den Quader ziehen → sofort ein Loch, Etikett
  „schneidet aus: Quader 1“. Geist anfassen und verschieben → Loch wandert live. Quader höher machen → Loch geht weiter durch.
  Zwei Körper wählen ▸ Körper ▸ Vereinen. ⌘Z nimmt jeden Schritt einzeln zurück.

## Stolperfallen
- Booleans sind teuer: nie im UI-Thread; während eines Zugs am Geist rechnet die Neuberechnung im Hintergrund, die Ansicht zeigt bis
  dahin das letzte Ergebnis (Regel 9). Budget: Griff-Vorschau < 50 ms für einfache Fälle (Architektur §12) – messen und im PR nennen.
- Flächennamen nach Booleans sind die Grundlage für AP-2.10; nicht abkürzen.
- Ein `combine` auf ein Ziel, das selbst schon kombiniert ist: Kette (Ergebnis des ersten wird Ziel des zweiten). Live-Löcher hängen
  sich an den **letzten** `combine`-Schritt des Ziels an, statt eine neue Kette zu bauen.
- Löschen eines Lochs (später AP-1.9) muss den `combine`-Schritt mitpflegen.

## Nicht tun
- Kein „Gruppieren“-Befehl, keine Gruppen.
- Keine weichen Booleans (AP-4.2), keine Netz-Booleans (AP-4.1).
- Keine stille Fuzzy-Reparatur: Fuzzy nur als angebotene Lösung.
