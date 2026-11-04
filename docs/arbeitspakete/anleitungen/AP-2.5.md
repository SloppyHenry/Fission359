# AP-2.5 Abrunden und Abschrägen

**Größe:** L · **Phasendatei:** AP-2.5

## Ziel
Kanten abrunden (konstanter Radius; variabler Radius unter „▸ Mehr“) und abschrägen (Abstand; Abstand + Winkel; zwei Abstände). Direkt in der
Ansicht: Über eine Kante fahren zeigt einen kleinen Viertelkreis-Griff; Ziehen rundet, mit ⌥ schrägt es ab. Tangential verbundene Kanten
werden mitgenommen; „alle Kanten einer Fläche“ und „alle senkrechten Kanten“ gehen mit einem Klick. Geht ein Radius nicht, nennt das Programm
den **größtmöglichen** und bietet ihn mit einem Klick an.

## Voraussetzungen
AP-2.10 a–c (Kanten-Verweise), AP-2.14 (Meldungen mit Zahlen; sonst die Meldung hier provisorisch und in 2.14 nachziehen).

## Vorher lesen
- `docs/02-ux-konzept.md` §2.2 (Kante → Viertelkreis-Griff, ⌥ = Abschrägen), §9 (Beispiel „Abrunden mit 5 mm geht hier nicht“), §15 A Schritt 3,
  B Schritt 3
- `docs/spikes/kernel-robustheit.md` (Abrunden: Fehlschlag sauber, Maximalradius per Intervallhalbierung), `tests/geometry/test_fillets.cpp`
- AP-2.10-Anleitung (Verweise auf Kanten)

## Haltepunkte
1. **Kontextleiste Ablauf A:** „Alle senkrechten Kanten runden“ an einem Quader. Empfehlung: Eintrag in der Kontextleiste bei gewähltem
   Körper, wenn er senkrechte Kanten hat. Bestätigen.
2. **Quader mit Eckenradius vs. Abrunden:** Beim Quader gibt es schon `cornerRadius` (senkrechte Kanten). Empfehlung: „Alle senkrechten
   Kanten runden“ auf einem frischen Quader setzt `cornerRadius` (einfacher, bleibt Grundform); auf anderen Körpern ein `fillet`-Schritt.

## Schritte
1. GAL: `NamedSolid fillet(const NamedSolid&, const std::vector<std::string>& edges, const FilletSpec&)` (`FilletSpec { radius; optional
   endRadius für variabel }`) und `chamfer(…, ChamferSpec { mode; d1; d2; angle })`. OCCT `BRepFilletAPI_MakeFillet`/`MakeChamfer`.
   Kantennamen → OCCT-Kanten über die Namensschicht (AP-2.10). Neue Flächen heißen `F<n>/fillet/round[<kantenname>]`.
   Fehlschlag → `GeometryErrorCode::filletTooLarge` (neu).
2. **Größtmöglicher Radius:** im Feature-Typ bei Fehlschlag Intervallhalbierung zwischen 0 und gewünschtem Radius (≤ 20 Versuche, im
   Hintergrund, abbrechbar), Ergebnis in `FeatureIssue::numbers["maxRadius"]` auf 0,1 mm abgerundet (sicher machbar), Aktion „Größtmöglichen
   Wert verwenden (3,1 mm)“. Cache: gleiche Eingabe → gleiches Maximum.
3. **Kettenauswahl:** GAL `std::vector<std::string> tangentChain(const NamedSolid&, edge)` (Nachbarkanten mit G1-Übergang), „Kanten einer
   Fläche“, „senkrechte Kanten“ (Richtung parallel zu Z in Körper-Koordinaten).
4. Feature-Typen `fillet`, `chamfer`: Eingang ein Körper, `refs["edges"]` (Kantenverweise), Werte `radius` bzw. `distance`/`angle`/`distance2`.
   Ergebnis: neuer Körper (wie `combine`: Eingang wird verbraucht, Ergebnis übernimmt Name/Farbe; Haltepunkt, wenn das anders gelöst werden
   soll – Muster aus ADR-0011 übernehmen).
5. Griff: Viertelkreis an der Kante beim Hover (Renderer, `HandleGraphics`), Ziehen = Radius (Bildschirm-Abstand senkrecht zur Kante),
   Tippen ersetzt Ziehen, ⌥ beim Ziehen = Abschrägen. Mehrere gewählte Kanten = ein Schritt. Regel 10.
6. Bestehender Schritt: Klick im Rezept → Griffe an allen seinen Kanten; Kanten hinzufügen/entfernen per ⇧-Klick.

## Stand 2026-11-04
Haltepunkte vom Maintainer bestätigt (Kontextleisten-Eintrag „Senkrechte Kanten abrunden“; frischer Quader setzt `cornerRadius`).
Gebaut:
- GAL: `GeometryKernel::blendEdges` (`BlendSpec`: Abrunden, Abschrägen mit einem oder zwei Abständen) und `tangentChain`; neuer `GeometryErrorCode::blendTooLarge` (landet als `kernelRejected` mit `subject` = Wert beim Schritt). OCCT in `src/geometry/occt/src/blends.cpp`. Neue Flächen heißen `F<n>/fillet/round[<Kantenname>]` bzw. `F<n>/chamfer/bevel[…]`, Eckflächen `…#corner<k>`; alte Flächen behalten ihre Namen.
- Schritt-Typen `fillet` (Wert `radius`) und `chamfer` (`distance`, unter „Mehr“ `distance2`) in `src/features/src/blend.cpp`: ein Körper, Kanten aus dem Verweis `edges`, Ergebnis wie `combine` (Name/Farbe des Körpers bleiben). Tangentiale Kanten nimmt OCCT selbst mit (`tangentChain` zeigt sie).
- **Größtmöglicher Wert:** bei Fehlschlag Intervallhalbierung (≤ 20 Versuche, abbrechbar), auf 0,1 mm abgerundet und gegengeprüft, als `numbers["maxValue"]` mit `given`, `where` (Kantenmitte) und `elements`. Die Meldung und der Knopf „Größtmöglichen Wert verwenden“ kommen aus AP-2.14; die Karte an der Stelle zeichnet `ProblemPlaceLayer.qml`.
- `DocumentModel::blendEdges`, `roundVerticalEdges`, `edgesOf` (Kette, Kanten einer Fläche, senkrechte Kanten); Menü „Körper“, Kontextleiste (gewählte Kanten: Abrunden/Abschrägen; Körper mit senkrechten Kanten: „Senkrechte Kanten abrunden“), Befehlssuche mit Synonymen, Rezeptsatz, Verlaufstext.
- Tests: `tests/unit/geometry/test_blends.cpp`, `tests/unit/features/test_blend.cpp`, `tests/unit/ui/test_blend_edges.cpp`.

Offen:
- **Griff in der Ansicht:** gebaut ist der Griff am Schritt: Nach „Abrunden“/„Abschrägen“ sitzt am ersten Rand ein Viertelkreis-Griff, Ziehen ändert den Wert (Werkzeug „Größe“ wird gewählt). Der Anker (Kantenmitte, Richtung nach innen) steht in den versteckten Werten `anchorX…anchorDz` des Schritts; ändert sich der Körper, bleibt er an der alten Stelle (nur die Anzeige, nie das Ergebnis). Dazu der **Hover-Griff** (`EdgeGripLayer.qml`): bei Filter „Kante“ erscheint über der Kante unter dem Zeiger ein Viertelkreis-Knopf (bleibt 0,7 s, damit man ihn erreicht); Klick rundet die Kante ab, ⌥-Klick schrägt sie ab, danach zieht man am Griff des Schritts. Das Ziehen direkt aus dem Hover-Knopf heraus (ein Zug statt Klick + Zug) ist nicht gebaut. Offen: Kanten hinzufügen/entfernen mit ⇧ am bestehenden Schritt, Anker nachführen, Fenster-Test des Hover-Knopfs (am Mac nur von Hand, nicht offscreen).
- Die 20 Fälle aus AP-0.5 (`tests/geometry/test_fillets.cpp`) über die GAL und Szenarien in der Namens-Produkt-Suite (`tests/unit/naming/`, `markFace` ersetzen).
- Winkelvariante der Fase (Abstand + Winkel), variabler Radius.
- Cache des größtmöglichen Wertes (heute neu gesucht bei jeder Neuberechnung eines fehlschlagenden Schritts).
- Ausgrauen mit Erklärung für freie Körper (UX §6) gibt es erst mit den freien Körpern.

## Tests
- Golden: Quader 20³, eine Kante R 2 → Volumen = 8000 − (4 − π)·20 (Formel für Viertelkreis-Streifen), Flächen +1; vier senkrechte Kanten;
  Kette tangentialer Kanten; Abschrägung 1 mm (Volumen − 0,5·20).
- **20 typische Fälle aus AP-0.5** (`test_fillets.cpp`) über die GAL: stabil (Akzeptanzkriterium 2).
- Zu großer Radius → Fehler mit `maxRadius`; Aktion setzt ihn, Ergebnis gültig (Akzeptanzkriterium 1).
- Naming: Quader höher → Rundung bleibt an derselben Kante (AP-2.10-Suite).

## Abnahme
So probierst du es aus: Quader; über eine Kante fahren → Viertelkreis; ziehen → Rundung, `4` tippen. ⌥ ziehen → Fase. Kante 50 mm runden →
Meldung mit „Größtmöglichen Wert verwenden“.

## Nicht tun
Keine Rundung auf freien Körpern (UX §6: ausgegraut mit Erklärung „Runde die Kante vor dem Verschmelzen ab“).

## Folgearbeit aus AP-2.10: Namens-Szenarien nachziehen
Sobald Abrunden Verweise nutzt (`FeatureDraft::refs`, `ExecuteInput::refs`), gehören seine Szenarien in die Produkt-Suite
`tests/unit/naming/` (Vorbild: `test_scenarios.cpp`, die Szenarien des Prototyps in `tests/naming/test_scenarios.cpp` und `test_evil.cpp`
mit denselben Erwartungen) und in die Zufallsprobe (`test_random.cpp`). Der Test-Schritt `markFace` (`refs_support.h`) wird dabei durch den
echten Schritt ersetzt. Ein Schritt ohne Namens-Vertrag (alle erzeugten Flächen benennen, `ElementHistory` liefern) wird nicht aufgenommen.
