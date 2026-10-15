# Phase 1 – Der erste Körper

**Ziel:** Alles, was man für ein erstes Erfolgserlebnis braucht: Formen platzieren, exakt bemaßen, bewegen, kombinieren, rückgängig machen, speichern, exportieren. Am Ende fühlt sich Fission3D schon besser an als Tinkercad.

**Meilenstein M1 (Demo):** Ablauf A „Schlüsselanhänger“ aus dem [UX-Konzept](../02-ux-konzept.md#15-beispielabläufe-referenz-für-tests) mit flachem Text (Prägen auf gekrümmte Flächen folgt in Phase 4) → STL/3MF-Export → druckbar.

---

### AP-1.1 Dokumentmodell und Feature-Graph
**Status:** erledigt (2026-10-14, PR #14–#18; vom Product Owner ausprobiert) · **Größe:** L · **Abhängigkeiten:** AP-0.5, AP-0.6

**Umfang:**
- `Document`, `Feature`, `Value`, `Body`, `Parameter` gemäß [Architektur §3](../03-architektur.md#3-core-datenmodell)
- Feature-Typ-Registry (Plugin-Schnittstelle: `validate`, `execute`, `describe`, `handles`)
- Recompute-Engine: Dirty-Propagation, Ergebnis-Cache, asynchroner Worker mit Abbruch, Status je Feature
- Ausdrucksparser mit Einheiten, Komma/Punkt, Fehlerpositionen und Zykluserkennung
- Signale/Events für die UI (Feature geändert, Recompute gestartet/fertig, Status geändert)

**Akzeptanzkriterien:**
- [x] Core ist ohne Qt kompilierbar und hat ≥ 85 % Testabdeckung
- [x] Eine Parameteränderung rechnet nur betroffene Features neu (per Test nachgewiesen)
- [x] Eine laufende Neuberechnung bricht bei einer neuen Änderung innerhalb von ≤ 50 ms ab
- [x] `"25,5 + 1 in"` ergibt `50,9 mm`; `"a = b, b = a"` liefert einen verständlichen Zyklusfehler

#### Zerlegung (2026-10-14, vom Product Owner bestätigt)
Je Teilpaket ein Pull Request mit grüner CI. Keine neuen Abhängigkeiten (`vcpkg.json` bleibt unberührt).

| Teil | Status | Inhalt |
|---|---|---|
| **a) Werte und Ausdrücke** | erledigt (PR #15) | AST-Parser mit Fehlerposition, `min`, `max`, `round`, `sin`, `cos`, `sqrt`, Abhängigkeiten zwischen benannten Maßen, Zykluserkennung, Einheiten-Art (Länge/Winkel/Zahl); `evaluateExpression` bleibt als Hülle für das Zahlenfeld; Messung der Testabdeckung |
| **b) Dokument und Features** | erledigt (PR #16) | `Document`, `Parameter`, `Feature`, `Body`, Feature-Typ-Registry; `GeometryKernel`-Schnittstelle in der GAL (ADR-0008); `PersistentRef` als Datentyp und Namens-Vertrag; Feature-Typen `box` und `cylinder` |
| **c) Neuberechnung** | erledigt (PR #17) | Abhängigkeits-Graph, Dirty-Propagation, Ergebnis-Cache, Worker mit Abbruch, Status je Feature, Ereignisse |
| **d) Anbindung an die Oberfläche** | erledigt (PR #18) | `DocumentModel`; der Demo-Würfel wird ein `box`-Feature im Dokument; Rezept-Platzhalter und Maß-Änderung sichtbar |

**Festlegungen**
- **Kernel-Zugriff:** Features rufen den Kernel über eine Schnittstelle der GAL auf, die App reicht die OCCT-Umsetzung hinein.
- **Texte:** `describe()` und Statusmeldungen liefern Schlüssel und Werte, nie fertigen Text; die Oberfläche übersetzt (Regel 4).
- **Abbruch ≤ 50 ms** heißt: Die Engine nimmt den neuen Auftrag sofort an und verwirft das alte Ergebnis; lange Schritte prüfen ein Abbruch-Token. Eine einzelne Kernel-Operation läuft im Hintergrund zu Ende, ohne die Oberfläche zu blockieren. Mit echten Kernel-Operationen wird das ab AP-1.7 neu bewertet.
- **Mehrere Dokumente:** Das Modell kennt keinen globalen Zustand; ein Test belegt zwei Dokumente parallel. Das Verdrahten von „Neu“ und mehreren Tabs gehört in AP-1.11.


#### Abschlussvermerk (2026-10-14)

**Umgesetzt**
- **a) Ausdrücke:** Formelparser mit Syntaxbaum und Fehlerstellen (Byte-Bereich im getippten Text), `min`, `max`, `round`,
  `sin`, `cos`, `sqrt`, Einheiten `mm cm m in " ft °`, Rechnen mit Länge, Winkel und Potenzen davon (die Diagonale
  `sqrt(a*a + b*b)` geht). Benannte Maße (`resolveMeasures`) mit Abhängigkeiten, „verwendet in“, Zyklus samt Pfad
  (`a → b → a`); Fehler bleiben am jeweiligen Maß. `evaluateExpression` bleibt als Hülle für das Zahlenfeld.
- **b) Dokument und Feature-Typen:** `Document` (Maße, Rezept, Körper) als kopierbarer Wert ohne globalen Zustand; stabile
  IDs, nie wiederverwendete Feature-Nummern; ein Schritt darf nur Körper früherer Schritte benutzen. Schnittstelle
  `FeatureType` (`validate`, `execute`, `describe`, `handles`) mit Registry und Namens-Vertrag. `PersistentRef` als Datentyp.
  `GeometryKernel` in der GAL (ADR-0008), Flächen-Rollen vom Kernel, Typen `box` und `cylinder`.
- **c) Neuberechnung:** Ergebnis-Speicher mit Schlüssel aus allem, was ins Ergebnis eingeht; Worker mit Abbruch (eigenes
  `CancelToken`), neueste Anforderung gewinnt; Ereignisse gestartet / Status geändert / fertig / abgebrochen. Ein scheiterndes
  Feature behält sein letztes gültiges Ergebnis (als veraltet markiert), davon abhängige Features zeigen eine Warnung.
- **d) Oberfläche:** `DocumentModel` (Qt) verbindet Dokument und QML. Der Würfel ist ein echtes `box`-Feature; das Rezept
  zeigt „Quader 20 × 20 × 20 mm“, ein Klick auf den Schritt zeigt seine Werte rechts (Größe, Lage) in Zahlenfeldern, auch mit
  Formeln. Zahlenfelder kennen die benannten Maße des Dokuments. Der Augen-Schalter der Körperliste schreibt ins Dokument.
- **Nebenbei:** `scripts/coverage` misst die Zeilenabdeckung von `src/core` (Clang und GCC, ohne Zusatzwerkzeuge);
  `scripts/test` erzwingt auf dem Mac nicht mehr die Plattform „offscreen“ (zwei Fenster-Tests waren dort mit
  `scripts/test` rot, einzeln grün).

**Wie getestet**
- Mac (arm64, Debug): `scripts/test` 166 von 166 grün. Thread-Sanitizer (Mac) über Neuberechnung und Worker: sauber.
- Abdeckung `src/core`: **94,4 %** (`scripts/coverage`, Mindestwert 85 %).
- Akzeptanz: `"25,5 + 1 in"` = 50,9 mm; `a`↔`b` → Zyklus mit Pfad; Parameteränderung rechnet nur Betroffene neu (Zähler am
  Kernel: geänderte Maße, Mitte der Kette, letzter Schritt, unabhängiger Schritt, Undo ohne Kernel-Aufruf); laufende
  Rechnung stoppt ≤ 50 ms nach der neuen Anforderung; Maß ändern bewegt den Körper im Szenengraphen (`test_document_model.cpp`).
- Linux: nur CI (Ubuntu 24.04, `release` und `debug-asan`). Der Testrechner `maintux` wurde **nicht** benutzt (dort fehlt
  vcpkg, ein Neuaufbau dauert rund 75 Minuten).
- Bild: [docs/screenshots/ap-1.1-macos-de.png](../screenshots/ap-1.1-macos-de.png).

**So probierst du es aus**
1. `scripts/run` (oder das `.dmg` aus dem CI-Lauf des PRs). Links im Rezept steht „Quader 20 × 20 × 20 mm“, in der Mitte der Würfel.
2. Auf den Schritt im Rezept klicken. Rechts erscheinen Breite, Tiefe, Höhe und die Lage X, Y, Z.
3. Bei „Breite“ `35` tippen und Enter: der Würfel wird breiter, der Satz im Rezept ändert sich.
4. Bei „Höhe“ `2 cm + 5` tippen: der Quader wächst nach oben auf 25 mm (er steht auf dem Bett, wird nicht um die Mitte skaliert).
5. Bei „Tiefe“ `abc` tippen: das Feld wird rot, nichts ändert sich. Bei `0` tippen: der Schritt bekommt ein rotes Kreuz mit
   „Tiefe muss größer als null sein.“, der Würfel bleibt in der letzten gültigen Form.
6. Das Auge neben „Quader 1“ blendet den Körper aus und ein.

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Änderungen gehen noch am Befehls-System vorbei:** `DocumentModel::setStepValue` und `setBodyVisible` ändern das Dokument
  direkt. Das ist die Stelle, an der AP-1.2 Commands (Undo/Redo) einsetzt; bis dahin gibt es kein Rückgängig.
- **Noch nicht in der Oberfläche:** benannte Maße anlegen und ansehen (nur im Kern und über Formeln nutzbar), mehrere
  Dokument-Tabs („Neu“, in AP-1.11 eingetragen), Auswahl durch Klick in der 3D-Ansicht (AP-1.4), Verweise auf Flächen (AP-2.10).
- **Texte:** Satzbau und Fehlersätze stehen vorläufig in `Main.qml` (Deutsch und Englisch, `qsTr`); der Meldungskatalog
  (AP-1.12 / AP-2.14) übernimmt das. Kernel-Fehlertexte erreichen die Oberfläche nicht (nur im `detail`-Feld für das Protokoll).
- **Abbruch:** Die Engine gibt sofort auf und verwirft das alte Ergebnis; eine einzelne Kernel-Operation läuft im Hintergrund zu
  Ende. Bei Box und Zylinder spielt das keine Rolle, bei Abrunden und Booleans muss es mit AP-1.7 neu gemessen werden.
- **Tessellierung** läuft im Worker-Thread nach der Neuberechnung (ein langer Körper verzögert dort die nächste Rechnung).
  Inkrementelles und blickabhängiges Tessellieren gehört zu AP-1.3.
- **Annahmen:** Position eines Quaders ist der Mittelpunkt der Grundfläche (X, Y) und die Unterkante (Z), damit der Körper wie in
  Tinkercad auf dem Bett steht; Funktionsargumente trennt `;` oder „Komma + Leerzeichen“ (das Komma ist Dezimaltrenner);
  Speicherkapazität des Ergebnis-Speichers 512 Einträge, Speicherverbrauch nicht begrenzt.
- **Gefundene Probleme:** (1) OpenCASCADE nennt „vorne“ und „links“ eines Quaders anders als der Ansichtswürfel; Rollen werden
  deshalb geometrisch zugeordnet. (2) `std::stop_token` ist in libc++ des CI-Macs noch experimentell; ersetzt durch ein eigenes,
  kleines `CancelToken`. (3) `scripts/test` hat auf dem Mac zwei Fenster-Tests rot gemacht (Plattform-Variable), behoben.
- **Vorgeschlagene Folge-Arbeiten:** Maße-Tabelle in der Oberfläche (UX §4), Dokument-Änderungs-Ereignisse mit den Commands
  (AP-1.2), Speichern der Felder inklusive Migrationstest (AP-1.10), Ergebnis-Speicher-Größe nach Speicherverbrauch.

---

### AP-1.2 Commands und Undo/Redo
**Status:** erledigt (2026-10-15, PR #19; vom Product Owner ausprobiert) · **Größe:** M · **Abhängigkeiten:** AP-1.1 (erledigt)

**Umfang:**
- Command-Basisklasse, Transaktionen (zusammenfassen beim Ziehen), unbegrenzter Undo-Stack
- Standard-Commands: Feature hinzufügen, entfernen, Parameter setzen, ausblenden, Körper umbenennen, Farbe ändern
- Menü- und Tastenkürzel: ⌘Z / ⇧⌘Z bzw. Strg+Z / Strg+Y / Strg+⇧+Z
- Undo-Historie als Liste einsehbar (Bearbeiten → **Letzte Aktionen**; der Name ist vom Product Owner gewählt, weil
  „Verlauf“ schon das Rezept meint)

**Akzeptanzkriterien:**
- [x] Property-basierter Test: 1000 zufällige Commands, danach komplett rückgängig, ergibt den Ausgangszustand
- [x] Ein Zieh-Vorgang erzeugt genau einen Undo-Schritt

#### Abschlussvermerk (2026-10-14)

**Umgesetzt**
- **Kern:** `Command` (`apply`/`revert`/`describe`), `History` (besitzt das Dokument und gibt es nur lesend heraus, damit die
  Oberfläche es gar nicht mehr direkt ändern kann), Transaktionen (auch verschachtelt, abbrechbar), „gespeicherter Stand“-Markierung
  für AP-1.10. Befehle: Schritt hinzufügen / entfernen, Wert setzen, Schritt ausblenden, Bezeichnung, Körper umbenennen /
  färben / ein- und ausblenden, benanntes Maß anlegen / ändern / umbenennen / löschen. Jeder Befehl beschreibt sich als Schlüssel
  plus Werte (Regel 4).
- **Genau wiederherstellen:** Ein zurückgenommenes „Schritt entfernen“ bringt denselben Schritt mit gleicher Id, Nummer, Stelle und
  denselben Körpern zurück; ein zurückgenommenes „Schritt hinzufügen“ gibt auch die Nummer frei (`Document::takeFeature`,
  `restoreFeature`).
- **Oberfläche:** Alle Änderungen (Wert eines Schritts, Auge an/aus) laufen über Befehle. Rückgängig/Wiederholen in der Leiste und im
  Menü „Bearbeiten“ (mit Text: „Rückgängig: Breite auf 35 geändert“), Kürzel inklusive Strg+Y, schwebende Liste „Letzte Aktionen“
  (Klick geht zu diesem Stand; Wiederholbares blass). Das gewählte Rezept-Element bleibt gewählt, auch wenn Schritte davor wegfallen.
- **Zieh-Vorgänge:** `DocumentModel::beginChange / commitChange / cancelChange` für die Griffe aus AP-1.5; Esc nimmt alles zurück.
- **Entwickler-Option** `--show-history` (Screenshots), siehe [ap-1.2-macos-de.png](../screenshots/ap-1.2-macos-de.png).

**Wie getestet**
- Mac (arm64, Debug): `scripts/test` 195 von 195 grün; Abdeckung `src/core` 95,0 % (`scripts/coverage`, Mindestwert 85 %).
- **Zufallstests** (`tests/unit/core/test_history.cpp`): 1000 zufällige Befehle × 5 Startwerte, alles zurück, dann alles vor, jeweils
  Inhalt gleich (Ids und Nummern inklusive); zusätzlich ein Vergleich mit einem Referenzmodell („nach jedem Schritt eine Kopie“)
  über Rückgängig, Wiederholen, Springen und abgeschnittene Wiederholen-Äste.
- **Zieh-Test:** 200 Zwischenwerte = ein Schritt; Zieh-Vorgang, der am Ausgangswert endet, hinterlässt keinen Schritt.
- **Oberfläche:** Rückgängig/Wiederholen bringen die Form wieder, **ohne dass der Kernel neu baut** (Zähler am Kernel);
  unveränderter Wert und abgelehnter Wert ergeben keinen Schritt.
- Linux: nur CI (Ubuntu 24.04). `maintux` wurde nicht benutzt.

**So probierst du es aus**
1. `scripts/run` (oder das `.dmg` aus dem CI-Lauf des PRs). Links im Rezept steht „Quader 20 × 20 × 20 mm“.
2. Klicke den Schritt an, tippe bei „Breite“ `35` und Enter. Dann bei „Höhe“ `2 cm + 5`.
3. Drücke ⌘Z (Linux: Strg+Z): die Höhe geht zurück, dann die Breite. Mit ⇧⌘Z (Linux: Strg+Y oder Strg+Umschalt+Z) kommen sie wieder.
4. Das Auge neben „Quader 1“ an und aus, dann ⌘Z: der Körper erscheint und verschwindet entsprechend.
5. Menü **Bearbeiten ▸ Letzte Aktionen**: die Liste. Ein Klick auf „Breite auf 35 geändert“ geht genau dorthin zurück; die
   späteren Einträge stehen blass da und lassen sich mit Wiederholen holen.
6. Mach nach einem Rückgängig etwas Neues: die blassen Einträge verschwinden (wie in jedem Programm).

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Noch ohne Bedienelement:** Schritt hinzufügen/entfernen, Körper umbenennen/färben, benannte Maße sind im Kern fertig und getestet,
  aber die Oberfläche bekommt sie erst mit AP-1.5 (Formen), AP-1.9 (Rezept) und der Maße-Tabelle. Dort bitte `DocumentModel`
  um die jeweilige Funktion erweitern und `History::execute` benutzen, nie das Dokument direkt.
- **Nicht rückgängig:** Auswahl, Kamera, Panels (das sind keine Änderungen am Dokument). Den Stapel gibt es nicht in der Datei
  (Architektur §3.5); der Versionsverlauf ist AP-5.7.
- **Zahlenfeld:** Hat ein Feld getippten, noch nicht bestätigten Text (`pendingEdit`), gilt ⌘Z für diesen Text, sonst für das
  Dokument. Geprüft mit echten Tastendrücken im laufenden Programm (Menüzustand unter macOS ausgelesen): Feld fokussiert → Rückgängig
  aktiv; „50“ getippt → aus; Enter → „Rückgängig: Breite auf 50 geändert“ aktiv. Der Product Owner meldete trotzdem, dass ⌘Z nicht
  reagiere. Ursache: Er hatte den Stand von `main` gestartet, der AP-1.2 noch nicht enthielt (nicht das Arbeitsverzeichnis des PRs). Ein echter Fehler war ein Feld, das nach einem
  unveränderten Wert den Fokus behält: dort ging ⌘Z an den Feldtext (behoben).
- **Schritte ohne Bedienelement im Verlauf-Text:** „Körper ausgeblendet“ nennt den Namen oder den Typ, nicht die Nummer in der Liste.
- **Doppelte Namen von Maßen** (erlaubt, siehe `Document::addParameter`) werden über ihre Position zurückgenommen, nicht über den
  Namen; die Zufallstests erzeugen absichtlich doppelte Namen.
- **Auge an/aus ist ein Schritt.** Wer das nicht möchte (Ansicht statt Änderung), kann es später ausnehmen; die Entscheidung steht
  in der Annahmenliste der Sitzung.
- **Vorgeschlagene Folge-Arbeiten:** Wiederherstellen nach Absturz (AP-1.10) nutzt `History::isClean`; Tastenkürzel-Hinweis
  „⌘Z“ im Rezept-Kopf ist noch eine Attrappe aus dem Mockup und sollte mit AP-1.9 entweder echt werden oder entfallen.

---

### AP-1.3 Viewport und Navigation
**Status:** in Arbeit (Teile 1.3a bis 1.3c fertig, siehe unten) · **Größe:** L · **Abhängigkeiten:** AP-0.4, AP-1.1

**Umfang:**
- Rendering aller Körper (Tessellierung mit Face/Edge-IDs, inkrementell nur geänderte Körper)
- Optik gemäß [UX §13](../02-ux-konzept.md#13-visuelle-gestaltung): matte Kunststoff-Optik, SSAO, Bettschatten, Kantenlinien
- Druckbett mit Raster in der Größe des aktiven Druckerprofils
- Kamera: Orbit, Pan, Zoom zum Zeiger; Perspektive/Ortho; Standardansichten; „Alles zeigen“; animierte Übergänge
- ViewCube (klickbar: Flächen, Kanten, Ecken)
- Maus-Voreinstellungen (Fission, Fusion, Tinkercad, Blender, SketchUp) und Trackpad-Gesten auf macOS
- Doppelklick auf Fläche = senkrecht draufschauen

- **Adaptive Darstellungsfeinheit:** Präzise Körper werden beim Hineinzoomen feiner tesselliert (im Hintergrund, ohne Ruckeln), damit Rundungen nie facettiert aussehen ([UX §7.3](../02-ux-konzept.md#73-niedrige-auflösung))

**Akzeptanzkriterien:**
- [ ] 60 fps bei 2 Mio. Dreiecken auf Referenz-Hardware (M1, Intel Iris Xe/AMD iGPU)
- [ ] Ein Loch Ø 3 mm zeigt bei bildschirmfüllendem Zoom keine sichtbaren Facetten (Screenshot-Test)
- [ ] Alle Voreinstellungen sind in einem Test-Skript durchgespielt
- [ ] Trackpad-Navigation fühlt sich auf macOS „wie Shapr3D/Fusion“ an (subjektiver Check durch 3 Personen)

**Stand nach AP-0.8 (2026-10-14):**
- *Schon da:* Renderer mit Körpern, mattem Licht, Schatten und Kantenlinien; Druckbett mit Raster (fest 256 × 256 mm, noch nicht aus dem Druckerprofil); Kamera mit Maus und Trackpad (Orbit, Pan, Zoom zum Zeiger); Perspektive/Ortho; Standardansichten mit Übergang; Ansichtswürfel; „Alles zeigen“ per Haus-Symbol.
- *Offen:* Rendern mehrerer Körper aus dem Dokument inkrementell (AP-1.1d legt den Anfang), SSAO, adaptive Feinheit, Doppelklick auf Fläche, klickbare Würfel-Kanten/-Ecken, Maus-Voreinstellungen (Fusion, Tinkercad, Blender, SketchUp), 2-Mio.-Dreiecke-Messung, Drucker-Bettgröße.

**Zerlegung (vom Product Owner am 2026-10-15 bestätigt):** ein PR je Teil.

| Teil | Inhalt | Status |
|---|---|---|
| 1.3a Navigation | Maus-Stile, Pinch-Zoom, Doppelklick auf Fläche, klickbare Würfel-Kanten und -Ecken | fertig (2026-10-15) |
| 1.3b Mehrere Körper | je Körper ein eigener Puffer, nur Geändertes wird hochgeladen | fertig (2026-10-15) |
| 1.3c Feinheit | beim Hineinzoomen im Hintergrund feiner tessellieren | fertig (2026-10-15) |
| 1.3d Optik | SSAO („weiche Verdeckungsschatten“), abschaltbar, schaltet sich bei verfehltem Budget selbst ab | offen |
| 1.3e Messung und Bett | 2 Mio. Dreiecke auf M1 und Linux-GPU; Bettgröße aus einer Druckerliste | offen |

#### Abschlussvermerk Teil 1.3c (2026-10-15)

**Umgesetzt**
- **Fünf Feinheitsstufen** (`render/detail.h`): Stufe 0 ist die bisherige Anzeige (Sehnenfehler 0,01 mm, 5°), jede Stufe halbiert den
  Sehnenfehler (bis 0,000625 mm bei Stufe 4; der Winkel schrumpft langsamer). Die nötige Stufe richtet sich nach dem Maßstab: der
  Fehler auf dem Bildschirm soll höchstens eine Viertel-Bildpunkt-Breite sein. Mit Hysterese, damit die Ansicht an einer Schwelle nicht
  hin- und herspringt.
- **Im Hintergrund:** Die 3D-Ansicht meldet beim Zoomen die gewünschte Stufe; das Dokumentmodell wartet 120 ms, bis das Zoomen sich
  beruhigt hat, und tesselliert dann die sichtbaren Körper auf einem eigenen Arbeitsfaden. Bis das Ergebnis da ist, bleibt die grobe
  Darstellung stehen (kein Ruckeln, kein Flackern); ein neuer Wunsch bricht eine laufende Arbeit zwischen zwei Körpern ab.
- Ändert sich das Dokument, bekommt der neue Körper zuerst das grobe Netz und dann das feine. Körper, für die eine feinere Stufe über
  3 Mio. Dreiecke bräuchte, behalten ihr grobes Netz (Schutz der Bildrate).
- Der Renderer sendet dank 1.3b nur den einen Körper neu.

**Wie getestet**
- `tests/unit/render/test_detail.cpp`: Stufen steigen mit dem Maßstab, Hysterese, Fehler ≤ ¼ Bildpunkt auf der gewählten Stufe.
- **Akzeptanzkriterium „Loch Ø 3 mm ohne sichtbare Facetten“:** Ein Zylinder Ø 3 mm wird für „füllt eine Bildschirmhöhe von
  1080 Bildpunkten“ (360 Bildpunkte je mm) tesselliert; die größte Abweichung des Kreis-Vielecks vom echten Kreis beträgt **0,03
  Bildpunkte** (Stufe 4) gegenüber 0,51 Bildpunkten mit dem Grundnetz.
- `test_document_model.cpp`: Zoomen gibt ein feineres Netz mit neuer Version, Herauszoomen bringt das Grundnetz zurück, eine
  Änderung des Dokuments bekommt anschließend wieder das feine Netz.
- Mac `scripts/test` komplett grün.

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Kein Bildschirmfoto als Nachweis:** In der Oberfläche lässt sich noch kein Loch bauen (Formen AP-1.5, Ausschneiden AP-1.7). Der
  Nachweis ist deshalb die gemessene Abweichung in Bildpunkten, nicht ein Foto. Sobald es Löcher gibt, ein Foto-Test nachliefern.
- Der Maßstab wird **am Blickpunkt** gemessen. Teile, die der Kamera viel näher sind, sind in Wirklichkeit größer abgebildet; mit
  Perspektive und sehr nahem Blick kann eine Kante knapp über ¼ Bildpunkt liegen. Behebbar mit einer Stufe je Körper nach Abstand.
- Eine einzelne Kernel-Tessellierung lässt sich nicht mitten drin abbrechen (nur zwischen Körpern); bei sehr großen Körpern wäre das
  ein eigenes Thema (siehe AP-1.1: „Abbruch“).

#### Abschlussvermerk Teil 1.3b (2026-10-15)

**Umgesetzt**
- Jeder Körper hat im Renderer **eigene Grafik-Puffer** (Flächen, Index, Kantenlinien) und eine Kennung (`RenderBody::id`, `meshVersion`).
  Bei jeder neuen Szene werden nur **neue oder geänderte** Körper (anderes Netz oder andere Farbe) hochgeladen, unveränderte behalten
  ihre Puffer, verschwundene werden freigegeben. Ein neues Kantenlinien-Farbmuster baut alle Linien neu (die Farbe steckt im Puffer).
- Die Netze werden **geteilt statt kopiert** (`shared_ptr`): eine neue Szene baut kein einziges Dreieck um. Das Dokumentmodell reicht
  den Zwischenspeicher der Tessellierung direkt durch.
- Schatten-, Flächen- und Linien-Durchgang zeichnen je Körper; die Reihenfolge der Szene bleibt.

**Wie getestet**
- `tests/unit/render/test_scene_renderer.cpp` lässt den echten Renderer auf Qts „Null“-Grafikschnittstelle laufen (kein Fenster, keine
  GPU nötig) und zählt die Uploads: drei Körper → 3; dieselbe Szene → 0 (3 behalten); ein Körper neu → 1; neue Farbe → 1; Körper
  entfernt → 1 entfernt; neue Kantenfarbe → alle; Körper ohne Kennung werden nach Platz unterschieden.
- Mac `scripts/test` komplett grün; Bild unverändert zu 1.3a (Würfel, Schatten, Kanten).

**Einschränkungen, Annahmen, Folge-Arbeiten**
- In der Oberfläche gibt es erst einen Körper (Formen kommen mit AP-1.5); die Wirkung zeigt sich im Test, nicht in einer Bedienung.
- Die Messung mit sehr vielen Dreiecken (Kriterium 60 fps) folgt in Teil 1.3e.
- Jeder Körper ist ein eigener Zeichenaufruf. Bei hunderten kleiner Körper wäre ein Zusammenfassen sinnvoll; nicht gemessen, daher
  nicht gebaut.

#### Abschlussvermerk Teil 1.3a (2026-10-15)

**Umgesetzt**
- **Maus-Stile** als Datentabelle (`render/navigation.h`): *Fission3D* (rechts drehen, Mitte verschieben), *Fusion* (Mitte verschiebt,
  Umschalt + Mitte dreht), *Tinkercad* (rechts dreht, Umschalt + rechts oder Mitte verschiebt), *Blender* (Mitte dreht, Umschalt + Mitte
  verschiebt, Strg + Mitte zoomt), *SketchUp* (Mitte dreht, Umschalt + Mitte verschiebt). In jedem Stil: Mausrad zoomt zum Zeiger;
  Alt + linke Taste dreht, Umschalt + linke Taste verschiebt (für Laptops ohne mittlere Taste). Auswahl im Menü **Ansicht**;
  die Hinweiszeile und die Hinweise an den Navigationsknöpfen passen sich an.
- **Trackpad:** zwei Finger drehen, Umschalt verschiebt, Spreizen/Zusammenziehen zoomt zum Zeiger, Smart Zoom (Doppeltipp mit zwei Fingern)
  rückt die Fläche unter dem Zeiger in die Mitte und passt die Größe an.
- **Doppelklick auf eine Fläche** (linke Taste): die Ansicht dreht sich senkrecht auf diese Fläche und nimmt sie als neue Mitte. Auf
  dem Druckbett daneben schaut sie von oben. Gekrümmte Flächen werden an der Klickstelle senkrecht angesehen. Der Strahl-Test läuft
  auf der CPU über die Anzeige-Dreiecke (`render/picking.h`); das reicht für einen Klick, Hover und Auswahl brauchen in AP-1.4 den
  ID-Puffer.
- **Ansichtswürfel:** jede Fläche ist in 3 × 3 Felder geteilt. Mitte = Fläche (wie bisher), Rand = Kante, Ecke = Ecke des Würfels;
  das Feld unter dem Zeiger leuchtet auf.

**Wie getestet**
- Mac (arm64, Debug): `scripts/test` 212 von 212 grün. Neu: `tests/unit/render/test_navigation.cpp` (Tabelle je Stil),
  `test_picking.cpp` (Strahl, Fläche, Bett, nächster Körper), Kamera-Tests (Ecke/Kante/Draufsicht/Rahmen) und
  `tests/unit/ui/test_viewport_navigation.cpp`, das **alle fünf Stile mit künstlichen Mausereignissen durch das echte
  3D-Ansichts-Element spielt** (Akzeptanzkriterium „Voreinstellungen im Test-Skript durchgespielt“), außerdem Mausrad, Trackpad,
  Pinch und Doppelklick auf Fläche, Bett und Hintergrund.
- Linux: nur CI. Die Maus-Stile sind plattformunabhängig; Pinch und Smart Zoom gibt es nur auf macOS.

**So probierst du es aus**
1. `scripts/run`. Menü **Ansicht** ▸ unten die fünf „Maus: wie in …“-Einträge; wähle zum Beispiel „wie in Blender“.
2. Mit der mittleren Taste ziehen dreht jetzt die Ansicht, Umschalt + mittlere Taste verschiebt sie. Die Hinweiszeile unten zeigt das an.
3. Oben rechts am Ansichtswürfel: fahre über die **Kanten und Ecken** (sie leuchten) und klicke. Die Ansicht fährt dorthin.
4. Doppelklick auf eine Würfelseite im Bild: die Ansicht dreht sich genau auf diese Seite. Ebenso auf das Druckbett.
5. Trackpad (Mac): Spreizen zoomt zum Zeiger; zwei Finger drehen; Umschalt + zwei Finger verschiebt.

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Maus-Stil wird nicht gemerkt** (gilt bis zum Schließen). Das Speichern gehört zu den Einstellungen und zum Erststart-Assistenten (AP-1.11).
- **Doppelklick-Konflikt:** AP-1.4 nennt „Doppelklick wählt den Körper“, UX §10 und AP-1.3 „Doppelklick auf Fläche = senkrecht
  draufschauen“. Beides gleichzeitig geht nicht. Vorschlag für AP-1.4: Doppelklick wählt den Körper, „Ansicht senkrecht auf Fläche“
  bleibt als Smart Zoom (Trackpad) und als Eintrag im Rechtsklick-Menü der Fläche.
- **Umschalt + linke Taste verschiebt die Ansicht** (Laptop-Hilfe). Mit der Rahmenauswahl aus AP-1.4 kollidiert das; dort ist
  dann Alt + Umschalt + links naheliegend.
- **Annahmen für die Stile:** Die Belegungen sind aus den Programmen nach Dokumentation übernommen, nicht gegen die Programme geprüft.
  Fusion und Blender haben mehr Gesten (z. B. Strg + Umschalt + Mitte), die ich bewusst weggelassen habe.
- Tooltip-Karten und Befehlssuche-Synonyme (Regel 10) für die Stil-Einträge gibt es erst mit der Befehlssuche (AP-1.11).

---

### AP-1.4 Auswahl und Hover
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.3

**Umfang:**
- Picking von Körper, Fläche, Kante und Ecke über ID-Buffer; Hover-Hervorhebung
- Klick-Logik: Einfachklick wählt Fläche/Kante (je nachdem, was näher ist), Doppelklick wählt den Körper; ⇧ erweitert die Auswahl, Rahmenauswahl
- Auswahlfilter (nur Körper, nur Flächen …), abhängig vom aktiven Werkzeug
- Auswahl-Modell im Core (stabil über Neuberechnung via persistente Referenzen)

**Akzeptanzkriterien:**
- [ ] Hover-Feedback < 16 ms
- [ ] Nach einer Parameteränderung bleibt die Auswahl erhalten (wenn die Geometrie noch existiert)

---

### AP-1.5 Grundformen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1, AP-1.4

**Umfang:**
- Quader (mit optionalem Eckenradius), Zylinder, Kugel, Kegel, Torus, Röhre, Prisma (n-eckig), Pyramide, Halbkugel, Keil, Stern (2D-Stern hochgezogen), Text (flach, System- und mitgelieferte Schriften)
- Formen-Galerie (große Kacheln mit 3D-Vorschau); Drag-and-drop in die Ansicht
- **Auf Fläche legen:** Die Form landet auf der Fläche unter dem Zeiger; diese wird zur Arbeitsebene
- Maß-Griffe (Tinkercad-artig) je Form; Wertblase; Tippen ersetzt Ziehen; `Tab` wechselt das Maß
- **„Als Loch“-Schalter** je Form (wirkt in AP-1.7)

- **Griffe ändern Parameter, nie die Skalierung** ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern)). Ist ein Körper aus einer Grundform entstanden, zeigen seine Griffe auch später die Maße dieser Grundform.

**Akzeptanzkriterien:**
- [ ] Jede Form lässt sich per Drag-and-drop platzieren und über Griffe oder Tastatur exakt bemaßen
- [ ] Quader mit Eckenradius 5 mm auf doppelte Breite gezogen → Radius bleibt exakt 5 mm (Golden-Test; Ablauf D, Schritt 1)
- [ ] Text: Umlaute, ß, Emojis werden sauber abgelehnt („Diese Schrift hat kein Zeichen für …“)
- [ ] Golden-Tests: Volumen jeder Form = analytischer Wert (rel. 1e-6)

---

### AP-1.6 Transformieren: Gizmo, Fangen, Abstandsmaße
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.4

**Umfang:**
- Gizmo: Verschieben (Achsen und Ebenen), Drehen (Ringe), **proportional** Vergrößern/Verkleinern (Eck-Griff mit ⇧ oder Prozentangabe)
- **Kein ungleichmäßiges Skalieren über das Gizmo.** Breiter/höher machen geht über die Maß-Griffe der Grundform (AP-1.5) bzw. ab Phase 2 über Strecken (AP-2.4). Echtes Verzerren nur über Ändern ▸ Mehr ▸ „Verzerren“, mit Warnhinweis ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern))
- Fangen: adaptives Raster, Ecken, Kantenmitten, Kreismittelpunkte, Flächenmitten, 15°-Winkel; ⇧ = aus
- **Intelligente Abstandsmaße** zu den nächsten Kanten anderer Körper und zum Bett, anklickbar und editierbar
- Ausrichten-Werkzeug (mehrere Körper bündig oder mittig)
- „Aufs Bett legen“ (Körper fällt auf z = 0), „Auf Fläche legen“
- Kopieren/Einfügen/Duplizieren (⌘D) mit Versatz

**Akzeptanzkriterien:**
- [ ] Eine Form lässt sich per Abstandsmaß exakt „5 mm vom Rand“ platzieren
- [ ] Jede Transformation steht als Rezept-Eintrag im Verlauf und ist editierbar

---

### AP-1.7 Kombinieren
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.5

**Umfang:**
- Vereinen, Ausschneiden, Überlappung behalten, Teilen (an Ebene oder Fläche)
- **Löcher ohne Gruppieren** gemäß [UX §7.1](../02-ux-konzept.md#71-ausschneiden-ist-umständlich):
  - „Als Loch“-Formen schneiden **live** aus, sobald sie einen Körper berühren
  - Ziel = Körper, auf dem das Loch abgelegt wurde; Etikett „schneidet aus: …“ am Loch, per Klick änderbar (auch mehrere Ziele)
  - Das Loch bleibt als Geisterform auswählbar, verschiebbar und in der Größe änderbar
  - Tiefe „durchgehend“ als Standard (bleibt durchgehend, wenn das Ziel dicker wird)
- Option „Werkzeug-Körper behalten“ beim Werkzeug Ausschneiden
- Fehlerfälle (nicht überlappend, koplanar, ungültig) mit Meldungen aus dem Katalog

**Akzeptanzkriterien:**
- [ ] Alle Kombinationen aus dem Kernel-Spike (AP-0.5) liefern gültige Körper oder eine verständliche Meldung
- [ ] Ändert man ein Loch nachträglich (Größe, Position), wird es korrekt neu ausgeschnitten
- [ ] Ablauf D, Schritte 2–4, läuft automatisiert grün; es gibt keinen Schritt „Gruppieren“

---

### AP-1.8 Eigenschaften-Panel und Werteingabe
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1

**Umfang:**
- Kontextabhängiges Panel: Was ist ausgewählt, welche Werte sind änderbar; „▸ Mehr“ für Seltenes
- Zahlenfeld-Komponente: Ausdrücke, Einheiten, Komma/Punkt, Pfeil hoch/runter (±1, mit ⇧ ±10, mit ⌥ ±0,1), Ziehen am Label ändert den Wert, Fehleranzeige inline
- Farbwahl je Körper (Palette + frei)
- Kontextleiste an der Auswahl in der 3D-Ansicht (die 4–6 wahrscheinlichsten Aktionen)

**Akzeptanzkriterien:**
- [ ] Zahlenfeld-Komponente ist einheitlich überall verwendet (keine Sonderfelder)
- [ ] Ungültige Eingabe wird erklärt („‚Breitee‘ kenne ich nicht. Meintest du ‚Breite‘?“)

---

### AP-1.9 Rezept-Panel (Version 1)
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.2, AP-1.8

**Umfang:**
- Liste der Features als Sätze (aus `describe()`), Symbol, Status-Badge
- Hover hebt Geometrie hervor; Klick öffnet die Bearbeitung mit Griffen
- Ausblenden, Löschen (mit Hinweis auf abhängige Schritte), Umbenennen
- Zeitmarker „jetzt“ verschiebbar (Rollback); neue Features werden an der Markerposition eingefügt
- Körper-Liste darunter: Sichtbarkeit, Farbe, Name, Typ-Badge

**Akzeptanzkriterien:**
- [ ] Ein Schritt in der Mitte des Rezepts lässt sich ändern, alles danach wird korrekt neu berechnet
- [ ] Löschen eines Schritts mit Abhängigen fragt mit klarer Erklärung nach

---

### AP-1.10 Dateien: Speichern, Laden, Autosave, Import und Export
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.1

**Umfang:**
- `.fission`-Format gemäß [Architektur §7](../03-architektur.md#7-dateiformat-fission) inkl. Vorschaubild
- Atomisches Speichern, Autosave, Wiederherstellung nach Absturz („Wir haben deine Arbeit von 14:32 gerettet.“)
- Zuletzt geöffnete Dateien; Dateizuordnung auf macOS und Linux
- Export: STL (binär), 3MF (Körper als Objekte, mit Farben), Tessellierungsqualität wählbar
- Import: STL und 3MF als freie Körper (Netz)

**Akzeptanzkriterien:**
- [ ] Roundtrip-Tests: Speichern → Laden ergibt ein identisches Dokument und identische Geometrie
- [ ] Ein simulierter Absturz während des Speicherns hinterlässt nie eine kaputte Datei
- [ ] Exportiertes 3MF öffnet sich korrekt in Bambu Studio, OrcaSlicer und PrusaSlicer (manuell geprüft, Screenshots im Abschlussvermerk)

---

### AP-1.11 App-Shell, Einstellungen, Sprachen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.8

**Umfang:**
- Native Menüs (macOS-Menüleiste, Linux-Fenstermenü), Tastenkürzel-Konzept (plattformgerecht)
- Erststart-Assistent: Sprache, Drucker (Profile: Bambu A1/A1 mini/P1S/X1C, Prusa MK4/Core One/Mini, Creality Ender-3 V3/K1, Voron, eigenes), Maus-Stil
- Einstellungen: Einheit, Nachkommastellen, Maus-Stil, Thema (hell/dunkel/System), UI-Skalierung, Autosave
- i18n-Infrastruktur (Qt Linguist), Deutsch und Englisch vollständig
- Startbildschirm (Version 1): Neu, Zuletzt geöffnet, Beispiele

**Akzeptanzkriterien:**
- [ ] Sprachwechsel ohne Neustart
- [ ] Keine hartkodierten UI-Texte (Prüfskript in der CI)

**Stand nach AP-0.8 (2026-10-14):**
- *Schon da:* Fenster nach Mockup, Menüs (Mac: Systemmenüleiste, Linux: im Fenster), Deutsch und Englisch mit Umschalten zur Laufzeit (`--language`), `qsTr` mit Übersetzungstest in der CI, Hell/Dunkel und Akzentfarbe (Theme), Über-Fenster, Protokolldatei.
- *Offen:* Erststart-Assistent, Einstellungen (Einheit, Nachkommastellen, Maus-Stil, UI-Skalierung, Autosave), Druckerprofile, Startbildschirm, Tastenkürzel-Konzept, **„Neu“ und mehrere Dokument-Tabs verdrahten** (aus AP-1.1 hierher verschoben; das Dokumentmodell kann mehrere Dokumente).

---

### AP-1.12 Hinweis-System und Befehlssuche
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.11

**Umfang:**
- Hinweiszeile: Jedes Werkzeug und jeder Zustand liefert einen Satz („Klicke auf eine Fläche …“)
- Tooltip-Karten (Titel, Satz, Tastenkürzel, Platz für Animation/GIF/Video-Loop)
- Befehlssuche ⌘K mit unscharfer Suche und Synonym-Liste je Werkzeug (DE und EN)
- Meldungskatalog-Grundgerüst (Schlüssel → Text mit Platzhaltern → Aktionen); wird in AP-2.14 ausgebaut

**Akzeptanzkriterien:**
- [ ] „runde ecken“, „bohren“, „hohl“ und „namen“ finden das richtige Werkzeug
- [ ] Jedes Werkzeug aus Phase 1 hat Hinweis, Tooltip-Karte und Synonyme

---

### AP-1.13 Nutzertest 1
**Status:** offen · **Größe:** S · **Abhängigkeiten:** alle APs der Phase 1

**Umfang:**
- 5 Personen ohne CAD-Erfahrung (davon 2 Tinkercad-Nutzer): Aufgabe „Schlüsselanhänger mit deinem Namen“, Think-Aloud, Bildschirmaufnahme, SUS-Fragebogen
- Auswertung: Stolperstellen nach Schwere sortiert → neue Tickets oder AP-Ergänzungen

**Akzeptanzkriterien:**
- [ ] Bericht `docs/nutzertests/2026-xx-phase1.md`
- [ ] ≥ 4 von 5 schaffen die Aufgabe in ≤ 10 min; sonst werden die Top-3-Probleme vor Phase 2 behoben
