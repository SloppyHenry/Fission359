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
**Status:** erledigt auf dem Mac (2026-10-16, PR #20; vom Product Owner ausprobiert). Offen: Messung auf Linux-iGPU (Kriterium 1), siehe Teil 1.3e · **Größe:** L · **Abhängigkeiten:** AP-0.4, AP-1.1

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
- [~] 60 fps bei 2 Mio. Dreiecken auf Referenz-Hardware (M1, Intel Iris Xe/AMD iGPU): **M1 gemessen (8,0 ms je Bild)**, Linux-iGPU noch nicht
- [x] Ein Loch Ø 3 mm zeigt bei bildschirmfüllendem Zoom keine sichtbaren Facetten (gemessen: 0,03 Bildpunkte Abweichung; Foto-Test, sobald es Löcher gibt)
- [x] Alle Voreinstellungen sind in einem Test-Skript durchgespielt
- [x] Trackpad-Navigation fühlt sich auf macOS „wie Shapr3D/Fusion“ an (Product Owner: „für gut befunden“; ein Check durch eine Person statt drei)

**Stand nach AP-0.8 (2026-10-14):**
- *Schon da:* Renderer mit Körpern, mattem Licht, Schatten und Kantenlinien; Druckbett mit Raster (fest 256 × 256 mm, noch nicht aus dem Druckerprofil); Kamera mit Maus und Trackpad (Orbit, Pan, Zoom zum Zeiger); Perspektive/Ortho; Standardansichten mit Übergang; Ansichtswürfel; „Alles zeigen“ per Haus-Symbol.
- *Offen:* Rendern mehrerer Körper aus dem Dokument inkrementell (AP-1.1d legt den Anfang), SSAO, adaptive Feinheit, Doppelklick auf Fläche, klickbare Würfel-Kanten/-Ecken, Maus-Voreinstellungen (Fusion, Tinkercad, Blender, SketchUp), 2-Mio.-Dreiecke-Messung, Drucker-Bettgröße.

**Zerlegung (vom Product Owner am 2026-10-15 bestätigt):** ein PR je Teil.

| Teil | Inhalt | Status |
|---|---|---|
| 1.3a Navigation | Maus-Stile, Pinch-Zoom, Doppelklick auf Fläche, klickbare Würfel-Kanten und -Ecken | fertig (2026-10-15) |
| 1.3b Mehrere Körper | je Körper ein eigener Puffer, nur Geändertes wird hochgeladen | fertig (2026-10-15) |
| 1.3c Feinheit | beim Hineinzoomen im Hintergrund feiner tessellieren | fertig (2026-10-15) |
| 1.3d Optik | SSAO („weiche Verdeckungsschatten“), abschaltbar, schaltet sich bei verfehltem Budget selbst ab | fertig (2026-10-15) |
| 1.3e Messung und Bett | 2 Mio. Dreiecke auf M1 und Linux-GPU; Bettgröße aus einer Druckerliste | Mac fertig, Linux-Messung offen (2026-10-16) |

#### Abschlussvermerk Teil 1.3e (2026-10-15)

**Umgesetzt**
- **Druckerliste** (`ui/printer_catalog.h`, 12 Einträge mit Bettgröße, Höhe und Düse; Platzhalter für die Druckerprofile aus AP-3.1):
  Im Reiter **Drucker** rechts wählen; das Druckbett (Größe, Raster, Beschriftung „256 mm“) und die Angabe oben rechts folgen, die
  Ansicht zeigt wieder alles. Kein Undo-Schritt, weil das eine Einstellung ist, keine Änderung am Modell.
- **Messmodus** `fission3d --benchmark <Dreiecke> [--no-soft-shadows]`: baut eine Szene mit der gewünschten Zahl Dreiecke (16 gewellte
  Platten), dreht die Kamera 300 Bilder lang und gibt die **GPU-Zeit je Bild** aus (Zeitstempel der Grafikschnittstelle), die
  Aufzeichnungszeit auf dem Rechner und den Bildabstand. Die GPU-Zeit ist die Zahl, auf die es ankommt: der Bildschirm begrenzt, wie
  schnell Bilder gezeigt werden, nicht wie schnell sie gebaut werden könnten.

**Messwerte (Mac, Apple M1, Metal, Release-Bau, 2026-10-15)**

| Dreiecke | weiche Schatten | GPU-Zeit je Bild (Mittel / 95 %) | Budget 16,7 ms |
|---|---|---|---|
| 0,5 Mio. | an | 3,5 / 3,7 ms | erfüllt |
| 0,5 Mio. | aus | 2,4 / 2,5 ms | erfüllt |
| **2 Mio.** | an | **8,0 / 8,7 ms** | erfüllt (rund 2-fach Reserve) |
| **2 Mio.** | aus | 5,9 / 6,1 ms | erfüllt |
| 5 Mio. | an | 13,3 / 13,9 ms | erfüllt (knapp) |
| 5 Mio. | aus | 8,7 / 9,6 ms | erfüllt |

Der Bildabstand war in allen Läufen 16,7 ms (60 Hz des Bildschirms): **60 fps bei 2 Mio. Dreiecken auf dem M1 sind erreicht.**

**Wie getestet**
- `test_document_model.cpp`: Druckerwahl ändert die Szene (Bett 250 × 210 mm), unbekannte Kennung ändert nichts, kein Undo-Schritt;
  `test_printer_catalog.cpp`: Kennungen eindeutig, Maße plausibel, nur der Allgemein-Eintrag ohne Namen.
- Mac `scripts/test` 225 von 225 grün. Bild: Reiter Drucker mit der Liste (siehe „So probierst du es aus“).

**So probierst du es aus**
1. `scripts/run`, rechts den Reiter **Drucker** öffnen: „Drucker wählen“ zeigt die Liste.
2. Klicke „Prusa MINI+“: das Druckbett in der Mitte wird kleiner (180 × 180 mm), die Beschriftung und oben rechts folgen.
3. Klicke „Prusa XL“: größer; die Ansicht zeigt das ganze Bett.
4. Optional, die Messung: `scripts/run --release -- --benchmark 2000000` (und noch einmal mit `--no-soft-shadows`). In der letzten Zeile
   steht `gpu_ms_avg=…`; unter 16,7 heißt: 60 fps und mehr.

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Linux-Messung (Intel/AMD-iGPU) steht aus.** Kriterium 1 nennt M1 *und* eine integrierte Intel/AMD-GPU. Auf der Referenz-Hardware
  für Linux (Testrechner mit Intel HD 620) habe ich die Messung noch nicht gemacht; ADR-0002 hatte den Spike mit 165 fps bei
  2 Mio. Dreiecken gemessen, die Produktionsansicht kommt mit Schatten-Durchgang und weichen Schatten dazu. Das Kästchen bleibt
  offen, bis es gemessen ist.
- **Druckerliste:** Die Bettmaße stammen aus Herstellerangaben aus dem Gedächtnis und sind nicht gegen die Datenblätter geprüft; bitte
  vor der Veröffentlichung kontrollieren. „Eigene Maße“ gibt es noch nicht (kommt mit den Druckerprofilen, AP-3.1).
- Der gewählte Drucker wird **nicht gemerkt** (Einstellungen: AP-1.11) und liegt nicht in der Datei (AP-1.10).
- Die Höhe des Bauraums (`bedZ`) wird angezeigt, aber noch nicht in der Ansicht gezeichnet.

#### Abschlussvermerk Teil 1.3d (2026-10-15)

**Umgesetzt**
- **Weiche Schatten (Ambient Occlusion, SSAO):** Ecken, Spalten und die Stelle, wo ein Körper auf dem Druckbett steht, werden leicht
  abgedunkelt. Ablauf je Bild: (1) Tiefe und Flächen-Normalen der Szene aus Kamerasicht, (2) Verdeckung aus der Tiefe mit 12 Proben
  in halber Auflösung, (3) 4×4-Weichzeichnung, (4) die Körper- und Bett-Shader nutzen das Ergebnis. Der Radius richtet sich nach der
  Größe der Szene.
- **Schalter** im Menü *Ansicht ▸ Weiche Schatten*.
- **Schaltet sich selbst ab,** wenn die Grafikkarte mit den Schatten 90 Bilder hintereinander über 16,7 ms braucht (gemessen mit den
  Zeitstempeln der Grafikschnittstelle, wo sie das kann); dann erscheint ein Hinweis, der erklärt, wie man sie wieder einschaltet.
- Entwickler-Optionen `--camera yaw,pitch,distance` und `--no-soft-shadows` für Vergleichsbilder.

**Wie getestet**
- Bild vorher/nachher: [ap-1.3d-weiche-schatten-aus-an.png](../screenshots/ap-1.3d-weiche-schatten-aus-an.png) (links aus, rechts an;
  Mac, Metal). Ein erster Versuch mit Normalen aus der Tiefe zeigte Muster auf ebenen Flächen; deshalb schreibt der Vorab-Durchgang
  die echten Flächen-Normalen.
- `test_scene_renderer.cpp`: ein Bild mit Schatten läuft durch alle Durchgänge und folgt einer Größenänderung (Null-Schnittstelle).
- Mac `scripts/test` komplett grün. Linux: nur CI (dort wird nur geprüft, dass es baut und läuft, nicht wie es aussieht).

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Die Wirkung ist bei einem einzelnen Würfel dezent.** Deutlicher wird sie bei Teilen mit Vertiefungen, Ecken und Löchern (ab AP-1.5/1.7).
  Stärke und Radius sind Schätzwerte; bitte am echten Modell beurteilen.
- **Nur auf dem Mac gesehen.** Metal ist geprüft; Vulkan/OpenGL laufen über denselben Code mit denselben Umrechnungen für die Bildlage
  (wie beim Schattenwurf), sind aber nicht angesehen worden. Der Linux-Test am Bildschirm steht aus.
- Kanten-Glättung (4-fach) gilt für die Körper, nicht für das Schattenbild; an Silhouetten kann ein feiner heller Saum bleiben.
- Die automatische Abschaltung funktioniert nur, wo die Schnittstelle Zeitstempel liefert; sonst bleibt der Schalter von Hand.

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
  bleibt als Smart Zoom (Trackpad) und als Eintrag im Rechtsklick-Menü der Fläche. **Gelöst in AP-1.4:** Doppelklick wählt den Körper;
  „Auf Fläche schauen“ ist Ansicht ▸ Auf Fläche schauen (L); das Rechtsklick-Menü gibt es noch nicht.
- **Umschalt + linke Taste verschiebt die Ansicht** (Laptop-Hilfe). Mit der Rahmenauswahl aus AP-1.4 kollidiert das; dort ist
  dann Alt + Umschalt + links naheliegend.
- **Annahmen für die Stile:** Die Belegungen sind aus den Programmen nach Dokumentation übernommen, nicht gegen die Programme geprüft.
  Fusion und Blender haben mehr Gesten (z. B. Strg + Umschalt + Mitte), die ich bewusst weggelassen habe.
- Tooltip-Karten und Befehlssuche-Synonyme (Regel 10) für die Stil-Einträge gibt es erst mit der Befehlssuche (AP-1.11).

---

### AP-1.4 Auswahl und Hover
**Status:** erledigt (2026-10-16, PR #21; vom Product Owner ausprobiert und freigegeben; Hover-Zeit siehe Einschränkungen) · **Größe:** M · **Abhängigkeiten:** AP-1.3

**Umfang:**
- Picking von Körper, Fläche, Kante und Ecke über ID-Buffer; Hover-Hervorhebung
- Klick-Logik: Einfachklick wählt Fläche/Kante (je nachdem, was näher ist), Doppelklick wählt den Körper; ⇧ erweitert die Auswahl, Rahmenauswahl
- Auswahlfilter (nur Körper, nur Flächen …), abhängig vom aktiven Werkzeug
- Auswahl-Modell im Core (stabil über Neuberechnung via persistente Referenzen)

**Akzeptanzkriterien:**
- [~] Hover-Feedback < 16 ms: **die Arbeit der Grafikkarte** (Bild zeichnen und zurücklesen) dauert bei 2 Mio. Dreiecken auf dem M1 im
  Mittel **2,0 ms** (Test `pick on the graphics card: 2 million triangles in under 16 ms`). **Bis das Leuchten zu sehen ist,** vergeht
  etwa ein Bild mehr (das Ergebnis kommt mit dem nächsten Bild zurück): im laufenden Programm gemessen 16 ms im Mittel bei 200 000
  und etwa 47 ms bei 2 Mio. Dreiecken (`fission3d --benchmark <Dreiecke>`, Zeile `pick`; beide Werte mit Bildern von 8 ms Länge).
  Kürzer geht es mit einem ID-Bild nicht, solange das Zurücklesen auf ein Bild wartet; siehe Einschränkungen.
- [x] Nach einer Parameteränderung bleibt die Auswahl erhalten (wenn die Geometrie noch existiert): Test
  `a chosen face stays chosen when a value changes`; Kante und Ecke ebenso; was nicht sicher wiedergefunden wird, wird losgelassen

#### Zerlegung (2026-10-16, vom Product Owner freigegeben: „entscheide nach eigenem Ermessen“)

| Teil | Inhalt | Status |
|---|---|---|
| 1.4a Namen und Modell | Kanten und Ecken kennen ihre Flächen (Kernel-Schnittstelle), `core::ElementNames`, `core::Selection` | fertig (2026-10-16) |
| 1.4b Pick-Durchgang | ID-Bild um den Zeiger (Fläche, Kante, Ecke), Hover-Leuchten | fertig (2026-10-16) |
| 1.4c Klicken | Klick, Doppelklick, ⇧, Rahmen, Filter | fertig (2026-10-16) |
| 1.4d Oberfläche | Hinweiszeile, Statuszeile, Körperliste, Rezept-Verknüpfung, „Wählen: …“, Esc, Alles auswählen, Auf Fläche schauen | fertig (2026-10-16) |

Die Teile liegen in einem PR (ein Durchgang, weil der Pick-Durchgang ohne Bedienung nicht zu sehen ist).

#### Abschlussvermerk (2026-10-16)

**Umgesetzt**
- **Wählen mit der Maus:** Fahre über den Körper: die Fläche, Kante oder Ecke unter dem Zeiger leuchtet. Ein Klick wählt sie. Was
  „näher“ ist, entscheidet der Abstand in Bildpunkten: eine Ecke gewinnt bis 6, eine Kante bis 4,5 Bildpunkte Abstand (Ecken sind
  klein, deshalb werden sie bevorzugt); sonst gilt die Fläche. **Doppelklick wählt den ganzen Körper.** **⇧-Klick** (oder ⌘/Strg-Klick)
  fügt hinzu oder nimmt weg. Ein Klick auf den Hintergrund hebt die Auswahl auf (Esc ebenfalls).
- **Rahmen:** Mit der linken Taste auf leerem Grund (oder irgendwo) ziehen zeichnet einen Rahmen; losgelassen ist gewählt, **was im
  Rahmen zu sehen ist** (verdeckte Teile zählen nicht). Mit ⌘/Strg wird hinzugefügt. Im Filter „Alles“ wählt der Rahmen Körper, in den
  anderen Filtern Flächen, Kanten oder Ecken.
- **Filter** „Wählen: Alles | Körper | Flächen | Kanten | Ecken“ neben der Anzeigeleiste. Nicht Passendes reagiert nicht (Kanten
  und Ecken werden dann gar nicht erst gezeichnet, die Fläche darunter ist greifbar). Die Verknüpfung mit dem aktiven Werkzeug
  folgt mit den Werkzeugen (AP-1.5/1.6); die Schnittstelle dafür ist da (`Selection.filter`).
- **Auswahl bleibt:** Kanten, Ecken und Flächen bekommen **Namen** (siehe [ADR-0009](../adr/0009-auswahl-und-elementnamen.md)):
  Fläche = was der Schritt sie nannte (`F1/box/top`), Kante = die zwei Flächen, die sie verbindet, Ecke = die Flächen, die sich dort
  treffen. Nach jeder Neuberechnung sucht die Auswahl ihre Elemente über den Namen wieder und **lässt los, was nicht sicher
  zu finden ist** (Regel 6), statt es woanders hin zu setzen. Namen, die im Körper doppelt vorkämen, bekommt niemand.
- **Verknüpfung:** Ein gewählter Körper (oder ein Teil davon) wählt seinen Schritt im Rezept; rechts erscheinen seine Werte. Ein
  Klick auf einen Schritt oder auf eine Zeile der Körperliste wählt den Körper. Ausgeblendete Körper werden abgewählt.
- **Hinweis- und Statuszeile:** beim Überfahren „Fläche von Quader 1 – ein Klick wählt sie, ein Doppelklick den ganzen Körper.“, in
  der Statuszeile „Kante von Quader 1 ausgewählt“ bzw. „3 Dinge ausgewählt“, nach dem Wählen die Hinweise für ⇧, Rahmen und Esc.
- **Menü:** Bearbeiten ▸ „Alles auswählen“ (⌘A; in einem Zahlenfeld wählt es den Text) und „Nichts auswählen“ (Esc); Ansicht ▸ „Auf
  Fläche schauen“ (**L**): das ist der frühere Doppelklick-auf-Fläche aus AP-1.3.
- **Darstellung** nach dem Design-System (Akzent „Auswahl-Umriss“): Fläche getönt, Kante als Streifen, Ecke als Quadrat, Körper
  getönt mit Umriss aus seinen Kanten. Hover ist heller als „gewählt“.
- **Entwickler-Optionen:** `fission3d --simulate "hover:x,y;click:x,y;shiftclick:x,y;doubleclick:x,y;frame:x1,y1,x2,y2"` spielt Zeigeraktionen
  durch den echten Eingabeweg (Grafikkarte, Pick-Durchgang, Auswahl); zusammen mit `--smoke-test bild.png` entstehen so
  Bilder. `--benchmark` misst zusätzlich die Antwortzeit des Pick-Durchgangs.

**Wie getestet**
- Mac (arm64, Debug): `scripts/test` 261 von 261 grün; CI auf Mac und Ubuntu grün; Abdeckung `src/core` 95,0 % (`scripts/coverage`, Mindestwert 85 %).
- **Auswahl-Modell** (`tests/unit/core/test_selection.cpp`): Klick, ⇧-Klick, Hintergrund, Doppelklick, Rahmen, Filter, Namen von Quader und
  Zylinder (12 Kanten, 8 Ecken, Naht des Zylinders), doppelte Namen gehören niemandem, Wiederfinden nach Maßänderung, Loslassen.
- **Kernel:** Kanten kennen beide Flächen und Ecken (Quader: jede Fläche 4 Kanten, jede Ecke 3; Zylinder-Naht).
- **Pick-Durchgang auf der echten Grafikkarte** (`tests/unit/render/test_pick.cpp`, Metal; wo es keine gibt, übersprungen): Fläche, Kante
  und Ecke eines Quaders, **Bild nicht auf dem Kopf** (Kante 4 Bildpunkte darüber liegt oben im Bild), Filter lässt die Fläche
  durchscheinen, Körper behalten ihre Nummer, Rahmen, 2 Mio. Dreiecke in 2,0 ms.
- **Oberfläche:** `test_selection_controller.cpp` (Klick wählt Fläche und Schritt, bleibt nach „Breite 35“, Kante und Ecke ebenso,
  ausgeblendet = abgewählt, Schritt ↔ Körper, Filter, „Auf Fläche schauen“) und `test_viewport_navigation.cpp` (Klick fragt, ⇧,
  Doppelklick, Rahmen statt Klick).
- Bilder im echten Programm mit `--simulate` (Metal): [ap-1.4-hover.png](../screenshots/ap-1.4-hover.png),
  [ap-1.4-kante.png](../screenshots/ap-1.4-kante.png), [ap-1.4-koerper.png](../screenshots/ap-1.4-koerper.png).
- **Linux** (CI-gebautes AppImage, Ubuntu 24.04, auf dem Testrechner Debian 13 unter Xvfb mit OpenGL, Mesa-Software-Renderer): Klick
  auf eine Kante, Rahmen um den Körper und **Hover mit dem echten Mauszeiger** (per `xdotool`) liefern dasselbe wie auf dem Mac,
  das ID-Bild liegt auch unter OpenGL richtig herum (Kante an der richtigen Stelle). Nicht geprüft: Vulkan (unter Xvfb nicht
  startbar) und die Zeiten auf einer echten Intel/AMD-GPU (dort war nur der Software-Renderer erreichbar: 155 ms je Bild, die
  Messung gilt nicht). Die CI (Ubuntu 24.04, Debug mit Speicherprüfung und Release) ist grün; die Grafikkarten-Tests werden dort
  übersprungen, weil es keine gibt.

**So probierst du es aus**
1. `scripts/run`. Fahre mit der Maus über den Würfel: Die Fläche unter dem Zeiger leuchtet; unten steht, was ein Klick tut.
2. Klicke eine **Kante** an (die Linie zwischen zwei Flächen), dann eine **Ecke**: sie werden gewählt, unten steht „Kante von Quader 1
   ausgewählt“, links ist „Quader 1“ markiert, rechts erscheinen die Maße.
3. Halte **Umschalt** und klicke eine zweite Fläche: beide sind gewählt („2 Dinge ausgewählt“). Umschalt-Klick auf eine gewählte
   nimmt sie weg.
4. **Doppelklick** auf den Würfel wählt den ganzen Körper (Umriss). **Esc** hebt die Auswahl auf.
5. Wähle eine Fläche und ändere rechts die **Breite**: die Fläche bleibt gewählt. Mit ⌘Z und ⇧⌘Z ebenso.
6. Ziehe mit der linken Taste einen **Rahmen** um den Würfel: er wird gewählt. Oben „Wählen: Alles“ auf „Kanten“ stellen: jetzt
   leuchten und wählen nur Kanten. Mit **L** (eine Fläche gewählt) schaust du senkrecht auf die Fläche.

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Antwortzeit des Hovers:** Das Ergebnis des ID-Bildes kommt mit dem nächsten Bild zurück; das Leuchten erscheint also etwa
  1 bis 2 Bilder nach der Mausbewegung (16 bis 33 ms bei 60 Hz). Das Kriterium „< 16 ms“ ist für die **Arbeit** erfüllt, für die
  **Gesamtzeit** nicht ganz. Wenn das zu träge wirkt, wäre der Ausweg ein CPU-Suchbaum für Hover (Flächen sofort, ohne Grafikkarte);
  das ist eine Entscheidung für den Product Owner nach dem Ausprobieren (Vorschlag: erst ausprobieren).
- **Doppelklick:** wählt jetzt den Körper (Entscheidung zum Konflikt aus AP-1.3). „Senkrecht draufschauen“ ist **Ansicht ▸ Auf Fläche
  schauen (L)** und weiter Smart Zoom (Trackpad). Ein Rechtsklick-Menü gibt es noch nicht. UX-Konzept §10 ist angepasst.
- **Rahmen und Umschalt+links:** Umschalt + ziehen verschiebt weiter die Ansicht (Laptop-Hilfe aus AP-1.3); der Rahmen
  gilt für Ziehen ohne Umschalt/Alt, hinzufügen mit ⌘/Strg + Ziehen. Ein Ziehen auf einem Körper wird später (AP-1.6) das Bewegen
  sein; dann zeichnet der Rahmen nur noch auf leerem Grund.
- **Namen nur, wo ein Schritt sie vergibt:** Heute sind das Quader und Zylinder. Bei Features, die Flächen umbauen (Abrunden,
  Ausschneiden), kommt die Namensweitergabe über die Kernel-Historie mit AP-2.10; bis dahin lassen solche Schritte die Auswahl los.
  Wo ein Name im Körper doppelt vorkäme (zwei Kanten zwischen denselben zwei Flächen), bleibt er unbenannt: das Element lässt
  sich wählen, bleibt aber nicht über eine Änderung hinweg gewählt.
- **Auf Fläche schauen** bei einer ganz um den Körper laufenden Fläche (Zylindermantel) schaut von oben; flache Flächen sind der Fall,
  für den es gedacht ist.
- **Umriss des Körpers** besteht aus seinen Kanten (Kugeln ohne Kanten haben keinen Umriss, nur die Tönung). Ein Umriss aus der
  Silhouette folgt, wenn es Körper ohne Kanten gibt (AP-1.5).
- **Höchstens 1023 Körper** sind gleichzeitig wählbar (10 Bit im ID-Bild), höchstens eine Million Flächen je Körper.
- **Ecken-Auswahl:** Quadrate von 9 Bildpunkten Seite; auf sehr dichten Ecken (Netze) wäre Auswählen der Kante einfacher.
- **Texte:** Tooltip-Karten und Befehlssuche-Synonyme (Regel 10) für „Alles auswählen“, „Auf Fläche schauen“ und den Filter folgen mit der
  Befehlssuche (AP-1.11).
- **Vorgeschlagene Folge-Arbeiten:** Auswahl-Filter je Werkzeug (AP-1.5/1.6); Kontextleiste an der Auswahl (UX §2.1) mit AP-1.5/1.6;
  Rechtsklick-Menü; Namensweitergabe für alle Features (AP-2.10).

---

### AP-1.5 Grundformen
**Status:** erledigt (2026-10-17, PR #22–#26; vom Product Owner freigegeben, Griffe nach seinem Feedback überarbeitet) · **Größe:** M · **Abhängigkeiten:** AP-1.1, AP-1.4

**Umfang:**
- Quader (mit optionalem Eckenradius), Zylinder, Kugel, Kegel, Torus, Röhre, Prisma (n-eckig), Pyramide, Halbkugel, Keil, Stern (2D-Stern hochgezogen), Text (flach, System- und mitgelieferte Schriften)
- Formen-Galerie (große Kacheln mit 3D-Vorschau); Drag-and-drop in die Ansicht
- **Auf Fläche legen:** Die Form landet auf der Fläche unter dem Zeiger; diese wird zur Arbeitsebene
- Maß-Griffe (Tinkercad-artig) je Form; Wertblase; Tippen ersetzt Ziehen; `Tab` wechselt das Maß
- **„Als Loch“-Schalter** je Form (wirkt in AP-1.7)

- **Griffe ändern Parameter, nie die Skalierung** ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern)). Ist ein Körper aus einer Grundform entstanden, zeigen seine Griffe auch später die Maße dieser Grundform.

**Akzeptanzkriterien:**
- [x] Jede Form lässt sich per Drag-and-drop platzieren und über Griffe oder Tastatur exakt bemaßen
- [x] Quader mit Eckenradius 5 mm auf doppelte Breite gezogen → Radius bleibt exakt 5 mm (Golden-Test; Ablauf D, Schritt 1)
- [x] Text: Umlaute und ß gehen, Emojis werden sauber abgelehnt („Diese Schrift hat kein Zeichen für …“) *(Deutung siehe Festlegungen)*
- [x] Golden-Tests: Volumen jeder Form = analytischer Wert (rel. 1e-6) *(Teil a; Text folgt mit Teil b)*

#### Zerlegung (2026-10-16, vom Product Owner bestätigt)
Je Teilpaket ein Pull Request mit grüner CI. Keine neuen Abhängigkeiten (`vcpkg.json` bleibt unberührt).

| Teil | Status | Inhalt |
|---|---|---|
| **a) Formen im Kern** | erledigt | Alle Formen als Feature-Typen (Kugel, Halbkugel, Kegel, Torus, Röhre, Prisma, Pyramide, Keil, Stern), Quader mit Eckenradius, Namen für jede Fläche; Golden-Tests mit analytischen Werten |
| **b) Text** | erledigt | Text-Feature, mitgelieferte Schriften (IBM Plex) plus Systemschriften, Meldung „Diese Schrift hat kein Zeichen für …“ |
| **c) Galerie und Platzieren** | erledigt | Formen-Galerie mit 3D-Vorschau, Drag-and-drop, „Auf Fläche legen“, „Als Loch“-Schalter, Hinweiszeile, Tooltip-Karten, Synonyme (Regel 10), Texte für alle Formen (Deutsch und Englisch) |
| **d) Griffe** | erledigt | Maß-Griffe in 3D (Griff-Art „Achsen“; „Klassisch“ kommt mit AP-1.11), Wertblase, Tippen ersetzt Ziehen, `Tab` wechselt das Maß, Esc nimmt zurück; Ablauf D Schritt 1 als automatischer Test |
| **e) Fett und Kursiv** | erledigt | Schnitt als Feld des Text-Schritts (normal, fett, kursiv, fett kursiv), Schnitt-Suche in der Schriftverwaltung, Knöpfe **B** und **I** im rechten Panel, eigene Meldung „… gibt es nicht in fett“ (auf Wunsch des Product Owners nachgezogen) |

**Festlegungen (Product Owner, 2026-10-16)**
- Griffe: nur „Achsen“ in diesem AP; „Klassisch“ gehört zu den Einstellungen (AP-1.11).
- „Umlaute, ß, Emojis werden sauber abgelehnt“ heißt: Umlaute und ß funktionieren, wenn die Schrift sie hat; nur Zeichen, die
  die gewählte Schrift nicht kennt (z. B. Emojis), werden mit der Meldung abgelehnt.
- Schriften: IBM Plex Sans und Mono (liegen schon im Projekt, OFL) plus die Systemschriften; Deko-Schriften kommen mit den Motiven.

**Entwurfsentscheidungen Teil a** (keine Rückfrage nötig, im Abschlussvermerk wiederholt)
- Alle Formen stehen mit der Unterseite auf der Ebene z und sind in der Draufsicht um (x, y) zentriert (wie der Quader).
- **Eckenradius** rundet die vier senkrechten Kanten (Ablauf D: „Ecken messen“); er darf höchstens die halbe kürzere Seite sein.
  Ein größerer Wert ist ein Fehler am Schritt, nie ein stilles Abschneiden (Regel 6). Die Griffe (Teil d) sperren deshalb beim
  Ziehen von Breite/Tiefe ab, bevor der Radius nicht mehr passt.
- **Kegel** mit Deckelradius 0 ist spitz, mit Deckelradius > 0 ein Kegelstumpf; gleiche Radien ergeben einen Zylinder.
- **Pyramide** hat ein Rechteck (Breite × Tiefe) als Grundfläche, **Prisma** und **Stern** n Ecken (3 bis 64, ganze Zahl).
- **Keil** ist eine Rampe: vorne (−Y) Höhe 0, hinten (+Y) volle Höhe.
- **Text (Teil b):** Schrift gewählt per Familienname (Regular); mitgeliefert sind IBM Plex Sans und Mono (werden beim Start in den
  Cache-Ordner kopiert, weil der Kernel Dateien liest), dazu alle Schriften des Rechners. Eine fehlende Schrift wird **nie** durch
  eine andere ersetzt (Regel 6): „Die Schrift … gibt es auf diesem Rechner nicht“. Fehlt ihr ein Zeichen, steht es in der Meldung
  („Diese Schrift hat kein Zeichen für 😀“), auch Ersatzschriften des Betriebssystems sind abgeschaltet. „Größe“ ist die Höhe eines
  Großbuchstabens; „Höhe“ ist, wie weit die Buchstaben aufragen. Der Text sitzt mit seiner Tinte mittig auf dem Punkt (x, y), mehrere
  Zeilen (Enter) gehen. Jeder getrennte Buchstabenteil ist ein eigener Körperteil im selben Körper (`glyph0Top`, `glyph0Side3` …).
  Worte (Text, Schrift) sind eigene Felder im Schritt (`Feature::texts`), keine Formeln, mit eigenem Undo-Befehl.
- **Galerie und Platzieren (Teil c):** Die Formen-Galerie öffnet sich neben der Werkzeugleiste („Formen“) oder über das Plus links; die
  Bilder auf den Kacheln sind die echten Formen aus dem Kern (Standardmaße, schräg von vorn links oben, mit Tiefenpuffer
  gezeichnet). Eine Kachel lässt sich in die Ansicht ziehen (die Form folgt dem Zeiger, landet aufs Bett oder auf die ebene Oberseite
  eines Körpers, Loslassen = ein Schritt im Verlauf, Esc = zurück, nichts bleibt liegen) oder anklicken (freier Platz auf dem Bett).
  Die Fläche, auf der zuletzt abgelegt wurde, ist die **Arbeitsebene** (nur die Höhe z; sie gilt, wenn der Zeiger über einer Wand oder
  neben dem Bett ist). Positionen rasten auf ganze Millimeter. **„Als Loch“** (Schalter in der Galerie, Menü Körper ▸ Als Loch) markiert
  den Körper (`Body::hole`, Undo-Befehl); er wird als halbdurchsichtige Geisterform gezeichnet und in der Körperliste als leerer
  Umriss gezeigt. Ausgeschnitten wird erst mit AP-1.7. Text: Nach dem Ablegen liegt der Cursor im Feld „Worte“ (rechts) mit
  markiertem Text; die Schrift wählt man in einer Liste mit Suche (alle Schriften des Rechners).
- Flächen-Namen: `top`, `bottom`, `side`, `surface`; bei Prisma und Stern `side1` … (Prisma: `side1` schaut nach vorn, dann gegen den
  Uhrzeigersinn von oben; Stern: `side1` folgt der Spitze nach hinten); Eckenradius: `cornerFrontLeft` …


#### Abschlussvermerk (2026-10-16, ergänzt 2026-10-17)

**Umgesetzt**
- **a) Kern:** Kugel, Halbkugel, Kegel (auch Stumpf), Torus, Röhre, Prisma, Pyramide, Keil, Stern; Quader mit Eckenradius; jede Fläche
  hat einen Namen; ein Aufruf `makePrimitive` statt neun neuer (ADR-0008).
- **b) Text:** Beschriftung mit IBM Plex (mitgeliefert, auch fett und kursiv) und allen Schriften des Rechners; fehlende Zeichen oder Schriften sind
  verständliche Fehler, es gibt keine stille Ersatzschrift; Worte (Text, Schrift) sind eigene Felder im Dokument mit Undo.
- **c) Galerie:** 12 Kacheln mit Bildern der echten Formen; Ziehen in die Ansicht (die Form folgt dem Zeiger, landet auf Bett oder
  ebener Oberseite, Loslassen = ein Schritt, Esc = zurück) oder Anklicken (freier Platz); „Als Loch“ als Geisterform;
  Textfelder und Schriftliste rechts; Sätze, Hinweise, Tooltips und Suchwörter auf Deutsch und Englisch.
- **d) Griffe (nach der Vorlage „Achsen“ überarbeitet, Darstellung am 2026-10-17 nach Feedback neu geordnet):** Der **Punkt sitzt auf
  der Fläche, die er bewegt** (Breite auf der Seitenfläche, Tiefe auf der Vorderseite, Höhe oben). Jedes Hauptmaß hat ein **Maßband** wie
  in einer Zeichnung: es liegt **außerhalb des Körpers** im Raum (Breite und Tiefe auf dem Bett vor dem Körper, Höhe an der linken
  Außenkante), mit Hilfslinien zu den gemessenen Ecken; die **Beschriftung steht auf dem Band** („Breite 40“) und wird beim Anklicken
  zum Eingabefeld. Farben: Breite rot, Tiefe grün, Höhe blau; Radius und Neben-Maße neutral (weißer Punkt mit dunklem Rand).
  **Neben-Maße** (Eckenradius, Wand, Röhrendicke, Innenradius des Sterns) bleiben ruhig: Beschriftung erst beim Überfahren, der
  Eckenradius hat einen eigenen Griff mit Viertelkreis an der rechten oberen Ecke. **Pfeile** erscheinen beim Überfahren. Ist der
  Körper im Bild sehr klein, bleiben nur die Punkte der Hauptmaße, Bänder kommen beim Überfahren. Schaut man genau auf ein Maß (Höhe
  von oben), fällt sein Band weg und die Beschriftung steht neben dem Körper. Der Griff sitzt auf der Seite, die **dir zugewandt** ist
  (jede Form meldet alle gleichwertigen Stellen: beide Enden der Breite, alle Ecken, rundherum bei runden Formen; ein Radius wird am
  Umriss genommen, wo man ihn von der Seite sieht). **Griffe mit Tiefe:** Punkte, Bänder und Pfeile zeichnet die 3D-Ansicht selbst, in
  zwei Durchgängen gegen die Tiefe der Szene: was ein Körper verdeckt, erscheint blass (Röntgenblick), der Rest kräftig. Ein Band hinter
  einem Nachbarkörper liegt also dahinter statt darüber, und seine Beschriftung tritt ebenfalls zurück. Die Beschriftungen und die
  Anfassflächen bleiben Bausteine der Oberfläche (gestochen scharfe Schrift, Tippen wie bisher).
  Ziehen ändert das Maß (nie skalieren), der Punkt bleibt unter dem Zeiger, Tippen ersetzt Ziehen (auch mitten im Ziehen), `Tab`
  springt zum nächsten Maß, Enter übernimmt, Esc nimmt alles zurück; das Ziehen hört auf, wo die Form unmöglich würde (z. B. Breite
  unter 2 × Eckenradius). Ein Griff, der fast auf den Betrachter zeigt, wird nicht gezogen, kein Maß geht über 2000 mm.

**Wie getestet**
- Mac (arm64, Debug): `ctest` alle grün, darunter Golden-Tests (Volumen und Oberfläche jeder Form gleich dem analytischen Wert,
  rel. 1e-6, Gültigkeit, Flächen-Namen gegen die Normalen), Text (Größe, Mitte, Umlaute, Emojis mit der echten Plex-Schrift),
  Platzieren, Abbrechen, Loch, Kachelbilder, Griffe (auch: Punkt auf der Fläche, Band und Beschriftung nie auf dem Körper aus vier
  Blickrichtungen, Radius am Umriss, kleine Körper nur mit Punkten, Punkte/Bänder/Pfeile als 3D-Zeichnung, Beschriftung tritt hinter
  einer Wand zurück, Renderer zeichnet Griffe blass und kräftig) und **Ablauf D, Schritt 1 automatisch** (Quader 40 × 40 × 10 mit 5-mm-Ecken über
  den Griff auf 80 mm gezogen: alle vier Ecken sind weiter Kreisbögen mit R 5 mm, Netzpunkte gemessen, Volumen exakt).
- Fenster mit offener Galerie, abgelegten Formen und getippten Maßen (`app_shape_gallery`); Bilder:
  [ap-1.5-formen-de.png](../screenshots/ap-1.5-formen-de.png), [ap-1.5-tippen-de.png](../screenshots/ap-1.5-tippen-de.png), [ap-1.5-schnitt-de.png](../screenshots/ap-1.5-schnitt-de.png), [ap-1.5-griffe-de.png](../screenshots/ap-1.5-griffe-de.png), [ap-1.5-griffe-tiefe-de.png](../screenshots/ap-1.5-griffe-tiefe-de.png) (Band hinter dem Nachbarwürfel).
- Linux: nur CI (Ubuntu 24.04, `release` und `debug-asan`); nicht von Hand auf dem Testrechner ausprobiert. Ein Text-Test war dort rot:
  eine mitgelieferte Schrift lieh sich einen fehlenden Schnitt (kursiv) von einer gleichnamigen Systemschrift. Jetzt gilt eine
  mitgelieferte Schrift als vollständig; ein Schnitt, den sie nicht hat, fehlt (Meldung statt stiller Ersatz).

**So probierst du es aus**
1. `scripts/run`. Links in der Werkzeugleiste auf **Formen** klicken: die Galerie öffnet sich.
2. Die **Kugel** in die 3D-Ansicht ziehen und loslassen: sie liegt auf dem Bett. Ziehe einen **Zylinder** auf die Oberseite des
   Würfels: er sitzt oben auf. Esc beim Ziehen nimmt die Form wieder weg.
3. Gleich nach dem Ablegen **`50` ⇥ `20` ⇥ `4` Enter** tippen: die Form bekommt diese Maße (ein Schritt im Verlauf, ⌘Z nimmt alles zurück).
4. An einer gewählten Form einen **Punkt ziehen**: das Maß ändert sich, die Beschriftung auf dem Band zeigt es; während des Ziehens eine
   Zahl tippen ersetzt den Wert. Mit dem Zeiger über einen Punkt fahren: der Pfeil zeigt, wohin er geht. Beim Quader: rechts „Eckenradius“ auf 5 stellen, dann die Breite am Punkt verdoppeln: die Ecken bleiben rund.
5. **Als Loch** in der Galerie einschalten und einen Zylinder ablegen: er ist durchsichtig (Geisterform), in der Körperliste ein leerer
   Umriss. Ausschneiden tut er erst mit AP-1.7.
6. Kachel **Text** ablegen: das Feld „Worte“ rechts ist markiert, `LENA` tippen. Schrift darunter wählen, mit **B** und **I** fett
   und kursiv schalten. Probiere ein Emoji: die Meldung nennt das Zeichen.

**Einschränkungen, Annahmen, Folge-Arbeiten**
- **Ablegen nur auf waagerechten Flächen:** Auf Wänden und Schrägen bleibt die Form auf der Arbeitsebene (Höhe der zuletzt benutzten
  Fläche). Eine Form an einer senkrechten Fläche auszurichten braucht Drehen (AP-1.6).
- **Griffe:** Bei kleinen Körpern im Bild können sich Beschriftungen nahekommen (sie weichen einander aus). Ob eine Beschriftung verdeckt
  ist, prüft das Programm an einem Punkt (Mitte des Bandes bzw. der Punkt selbst), nicht an der ganzen Beschriftung. Ein verdeckter Punkt
  lässt sich weiter anfassen. Die Lage der Bänder richtet sich nach dem Quader um den Körper; bei runden Formen beginnen die Hilfslinien der
  Höhe deshalb kurz neben dem Umriss. Nur die Griff-Art „Achsen“. „Klassisch“ kommt mit den Einstellungen (AP-1.11).
- **Tippen ohne Ziehen:** Ziffern fangen nur direkt nach dem Ablegen an zu tippen (sonst sind 1 bis 6 die Ansichten). Auch `Tab` oder
  Enter beginnen das Tippen. Bei einer schon vorhandenen Form: Punkt anfassen und dann tippen, oder die Felder rechts benutzen.
- **Seiten und Zacken** (Prisma, Stern) haben keinen Griff; sie stehen als Zahl im Feld rechts.
- **Text (Teil e):** Der Schnitt ist ein eigenes Feld im Text-Schritt. Mitgeliefert sind IBM Plex Sans und Mono jeweils normal, fett,
  kursiv und fett kursiv (die Dateien stammen aus dem IBM-Plex-Repository, Lizenz OFL, liegen unter `apps/fission3d/resources/fonts`).
  Bei Systemschriften sind genau die Schnitte wählbar, die die Schrift auf dem Rechner hat. Gibt es einen Schnitt nicht, ist **B**
  bzw. **I** grau; wechselt man zu einer Schrift ohne den benutzten Schnitt, wird die Schrift wieder normal, mit Hinweis, in
  demselben Schritt. Ein Schnitt, der in einer geladenen Datei fehlt, ist ein Fehler am Schritt („… gibt es nicht in fett“), nie ein
  stiller Ersatz. Gesperrt bleibt: Das Dokument speichert die Schrift nicht in der Datei (kommt mit AP-1.10); fehlt sie auf einem anderen
  Rechner, steht dort die Meldung „Die Schrift … gibt es auf diesem Rechner nicht“. Die Plex-Dateien kommen von der aktuellen Version
  des IBM-Repositorys, die normale Schrift vom früheren Stand: die Buchstabenformen sind gleich, Feinheiten der Abstände können abweichen.
- **Raster:** Positionen und gezogene Maße rasten auf ganze Millimeter (feiner bei starkem Zoom); das echte Fangen kommt mit AP-1.6.
- **Neu im Kern:** `fission_geometry_occt` linkt jetzt `TKService` und `TKV3d` (nur OpenCASCADE, schon im Paket); beim ersten Start
  kopiert das Programm zwei Schriftdateien in den Cache-Ordner.
- **Nebenbei behoben:** Beim Beenden konnte das Programm abstürzen, wenn gerade noch eine Abfrage „was liegt unter dem Zeiger“
  unterwegs war (aus AP-1.4; die Antwort kam an einen schon abgebauten Baustein). Die beiden Zieh-Tests im echten Fenster laufen allein
  (`RUN_SERIAL`): auf dem Mac nahm ihnen sonst ein gleichzeitig startendes Testfenster den Fokus.
- **Vorgeschlagene Folge-Arbeiten:** Ausrichten an schrägen Flächen (AP-1.6; das Gizmo kann die Griff-Zeichnung im Renderer
  mitbenutzen), Ausschneiden der Löcher (AP-1.7), Befehlssuche mit den Suchwörtern der Formen (AP-1.12), Schrift in der Datei (AP-1.10).

---

### AP-1.6 Transformieren: Gizmo, Fangen, Abstandsmaße
**Status:** in Arbeit (1.6a erledigt) · **Größe:** L · **Abhängigkeiten:** AP-1.4

**Umfang:**
- Gizmo: Verschieben (Achsen und Ebenen), Drehen (Ringe), **proportional** Vergrößern/Verkleinern (Eck-Griff oder Prozentangabe; der Eck-Griff ist immer gleichmäßig)
- **Kein ungleichmäßiges Skalieren über das Gizmo.** Breiter/höher machen geht über die Maß-Griffe der Grundform (AP-1.5) bzw. ab Phase 2 über Strecken (AP-2.4). Echtes Verzerren nur über Ändern ▸ Mehr ▸ „Verzerren“, mit Warnhinweis ([UX §7.2](../02-ux-konzept.md#72-rundungen-verziehen-sich-beim-größe-ändern))
- Fangen: adaptives Raster, Ecken, Kantenmitten, Kreismittelpunkte, Flächenmitten, 15°-Winkel; ⇧ = aus
- **Intelligente Abstandsmaße** zu den nächsten Kanten anderer Körper und zum Bett, anklickbar und editierbar
- Ausrichten-Werkzeug (mehrere Körper bündig oder mittig)
- „Aufs Bett legen“ (Körper fällt auf z = 0), „Auf Fläche legen“
- Kopieren/Einfügen/Duplizieren (⌘D) mit Versatz

**Akzeptanzkriterien:**
- [ ] Eine Form lässt sich per Abstandsmaß exakt „5 mm vom Rand“ platzieren
- [ ] Jede Transformation steht als Rezept-Eintrag im Verlauf und ist editierbar

#### Zerlegung (2026-10-18, vom Product Owner bestätigt)
Je Teilpaket ein Pull Request mit grüner CI. Keine neuen Abhängigkeiten geplant. Gizmo, Ringe, Fang-Marken und Abstandsmaße
zeichnet die 3D-Ansicht tiefenrichtig (`render::HandleGraphics`, erweitert um Ringe und gefüllte Flächen); in QML liegen nur
Beschriftungen und Anfassflächen.

| Teil | Status | Inhalt |
|---|---|---|
| **a) Verschieben** | erledigt (2026-10-18, PR #27) | ADR-0010 (wo eine Verschiebung im Rezept lebt); die Werkzeuge der Palette wirken (Wählen, Verschieben, Größe); Gizmo mit Achsen-Pfeilen und Ebenen-Quadraten; **Körper selbst ziehen verschiebt ihn** auf der Arbeitsebene (Rahmen dann nur noch auf leerem Grund); Wertblase, Tippen mit `Tab` X → Y → Z, Enter, Esc; Raster in ganzen mm wie bisher |
| **b) Drehen und proportional** | offen | Die Lage bekommt eine Drehung; Ringe, 15°-Schritte, Tippen in Grad; Drehen um die Körpermitte; Eck-Griff (Werkzeug Größe) und Prozentangabe ändern **alle Maße im gleichen Verhältnis** (Parameter, nie Skalieren, Regel 11); Ablegen an Wänden und Schrägen richtet die Form zur Fläche aus |
| **c) Fangen** | offen | Adaptives Raster; Fangen an Ecken, Kantenmitten, Kreismitten, Flächenmitten anderer Körper und am Bett; Ausrichtungslinien; Fang-Marke in der Ansicht; ⇧ während des Ziehens schaltet das Fangen aus; gilt auch für Maß-Griffe und das Ablegen |
| **d) Abstandsmaße** | offen | Beim Verschieben Abstände zu den nächsten Kanten anderer Körper und zum Bettrand als Maßband in der Ansicht; ein Klick macht die Zahl editierbar („5 mm vom Rand“), Tippen setzt den Körper genau dorthin; Akzeptanzkriterium 1 als automatischer Ablauf |
| **e) Rest** | offen | Ausrichten mehrerer Körper (bündig, mittig); „Aufs Bett legen“; „Auf Fläche legen“ (auch an Wänden); Kopieren, Einfügen, Duplizieren (⌘D) mit Versatz |

**Festlegungen (Product Owner, 2026-10-18: „ok“ zu allen Empfehlungen)**
- **Lage im Rezept:** Verschieben und Drehen ändern die Lage-Werte des Schritts, der den Körper macht; ein eigener Schritt
  „Verschoben“ nur, wo ein Schritt mehrere Körper speist ([ADR-0010](../adr/0010-lage-im-rezept.md)). Kriterium 2 heißt damit: Die
  Bewegung steht im Rezept-Eintrag des Körpers (Klick → Lage rechts) und ist dort editierbar.
- **Griffe nach Werkzeug** wie im Mockup: Verschieben (Standard) = Gizmo und Maßbänder ohne Punkte, Größe = Punkte und Eck-Griff,
  Drehen = Ringe, Wählen = keine. Den Körper selbst ziehen verschiebt ihn in jedem Werkzeug außer Wählen.
- **Kopieren/Einfügen** nur innerhalb des Programms (System-Zwischenablage mit dem Dateiformat, AP-1.10).
- Annahmen bestätigt: Abstandsmaß von Kante zu Kante; Eck-Griff immer gleichmäßig (⇧ nur „Fangen aus“, während eines Zugs);
  Gizmo-Achsen immer in Bett-Richtung; Drehen um die Körpermitte; Zoomen mit der Tastatur ist nicht Teil von AP-1.6.

#### Zwischenstand 1.6a Verschieben (2026-10-18, erledigt: vom Product Owner ausprobiert, „fühlt sich top an“)

**Umgesetzt**
- **Werkzeuge der Palette wirken:** „Verschieben“ (Standard) zeigt am gewählten Körper das Gizmo und die Maßbänder (Beschriftung
  anklicken und tippen geht weiter); „Größe“ zeigt die Maß-Punkte wie bisher; „Wählen“ zeigt keine Griffe; „Drehen“ sagt in der
  Hinweiszeile, dass es bald kommt. Jedes Werkzeug hat Tooltip-Karte, Hinweiszeile und Suchwörter (ein Katalog, `ToolCatalog.qml`).
- **Gizmo** nach dem Mockup: Pfeile X (rot), Y (grün), Z (blau) mit Buchstaben, kleine Quadrate für die Ebenen XY, XZ, YZ und ein weißer
  Punkt in der Mitte (über das Bett). Es sitzt über der Mitte der Oberseite und behält auf dem Bildschirm seine Größe (bei kleinen
  Körpern kürzer). Was man genau von vorn sieht (ein Pfeil, der auf einen zeigt, ein Quadrat von der Kante), fällt weg. Gezeichnet in der
  3D-Ansicht mit Tiefe (verdeckt blass), wie die Maß-Griffe; neue Bausteine der Zeichnung: gefüllte Pfeilspitzen und Flächen.
- **Ziehen:** Pfeil = nur diese Richtung, Quadrat = in der Ebene, Punkt = über das Bett. Während des Zugs treten die anderen Teile zurück,
  eine dünne Linie zeigt die Richtung, die **Wertblase** sagt, wie weit („X +20 mm“). Die Lage rastet auf ganze Millimeter (feiner beim
  Zoomen). **Tippen ersetzt Ziehen:** eine Zahl ist der Abstand in dieser Richtung, `Tab` geht X → Y → Z, Enter übernimmt, Esc setzt zurück.
- **Körper selbst anfassen und ziehen** verschiebt ihn über das Bett (der angefasste Punkt bleibt unter dem Zeiger), in jedem Werkzeug
  außer „Wählen“. Der Rahmen zum Auswählen entsteht dann nur noch auf leerem Grund (in „Wählen“ überall).
- **Pfeiltasten** verschieben den gewählten Körper um 1 mm, mit ⇧ um 10 mm, in die Bett-Richtung, die auf dem Bildschirm am besten
  passt; Bild↑/Bild↓ heben und senken ihn.
- **Rezept und Verlauf** (ADR-0010): Ein Zug (auch mit Tippen) ist **ein** Schritt in „Letzte Aktionen“: „Quader verschoben“. Er ändert die
  Lage-Werte x, y, z der Form; ein Wert, der sich nicht bewegt, bleibt unberührt (eine Formel darin bleibt erhalten). Das Rezept bekommt
  keinen neuen Eintrag.

**Wie getestet**
- Mac (arm64, Debug): `ctest` 361 von 361 grün (nach der Nachbesserung).
- Neu: `tests/unit/interaction/test_drag.cpp` (Achse, Ebene, Rasten), `tests/unit/ui/test_gizmo.cpp` (Teile sichtbar/ausgelassen, X-Pfeil
  ändert nur x in ganzen mm, Formel in y bleibt, ein Schritt, Undo; Esc; Ebene und Punkt nie nach oben; Tippen mit Tab; Körper ziehen durch
  die echten Mausfunktionen der Ansicht, Punkt bleibt unter dem Zeiger; in „Wählen“ wird es ein Rahmen; Pfeiltasten; Maß-Griffe je Modus),
  `test_history.cpp` (ein Schritt mit eigenem Namen behält ihn).
- Fenster mit echten Mausereignissen: `app_gizmo_move` (Pfeil und Punkt gezogen); die beiden Zieh-Tests der Maß-Griffe wählen jetzt
  vorher das Werkzeug „Größe“.
- Bilder: [ap-1.6a-gizmo-de.png](../screenshots/ap-1.6a-gizmo-de.png), [ap-1.6a-ziehen-de.png](../screenshots/ap-1.6a-ziehen-de.png),
  [ap-1.6a-tippen-de.png](../screenshots/ap-1.6a-tippen-de.png).
- Linux: nur CI.

**So probierst du es aus** (`scripts/run --release` aus dem Hauptordner, sobald `main` aktualisiert ist)
1. `scripts/run --release`. Doppelklick auf den Würfel: oben erscheinen die Pfeile X, Y, Z, die Quadrate und der weiße Punkt.
2. Am **roten Pfeil** ziehen: der Würfel wandert nur nach rechts/links, die Blase zeigt „X +…“, rechts unter „Lage“ ändert sich X.
3. Während du ziehst, `25` tippen, dann `Tab` und `-5`, dann Enter: der Würfel steht 25 mm weiter in X und 5 mm zurück in Y.
   ⌘Z nimmt den ganzen Zug zurück („Quader verschoben“).
4. Den **Würfel selbst** anfassen und ziehen: er rutscht über das Bett. Esc während des Ziehens stellt ihn zurück.
5. Pfeiltasten (mit und ohne Umschalt) und Bild↑/Bild↓ probieren. Links auf **Größe** wechseln: jetzt sind die Maß-Punkte da, das Gizmo
   ist weg; auf **Wählen**: keine Griffe, Ziehen zeichnet einen Rahmen.

**Nachbesserung nach dem ersten Ausprobieren (Product Owner, 2026-10-18)**
- *„Pfeil anklicken und bewegen: verliert je nach Weg die Haftung.“* Ursache: Drehte die Perspektive einen Pfeil beim Ziehen fast auf
  den Betrachter zu, nahm das Programm ihn aus der Liste der Gizmo-Teile, und das Fenster ließ den Zug los. Jetzt hat jedes der sieben
  Teile einen festen Platz, das gehaltene bleibt immer. Pfeile verschwinden erst, wenn sie wirklich fast auf einen zeigen, und die
  Anfassflächen sind größer. Test: `the arrow that is held stays while the body moves far`.
- *„Beim Größe-Ändern verschwand der Quader nach fröhlichem Größer- und Kleinerziehen.“* Ursache: Zog man ein Maß kleiner, als die Form
  erlaubt (unter null), blieb der Wert stehen, statt beim kleinsten möglichen anzuhalten. Kleinerziehen ging dann nicht mehr, Größerziehen
  schon; nach ein paar Runden füllte der Körper das Bild. Jetzt hält das Maß beim kleinsten möglichen Wert an (z. B. 1 mm). Und nahe am
  Fluchtpunkt einer Achse rast der Wert nicht mehr davon. Tests: `pulled smaller than possible …`, `near the vanishing point …`.
- *„Tiefe lässt sich nicht ziehen, von vorn schon gar nicht (extra?)“* Ja, das war eine Schutzregel aus AP-1.5: Eine Achse, die weniger als
  30° neben der Blickrichtung liegt, wurde gesperrt, weil sie sonst davonrannte. Das traf die Tiefe in jeder etwas frontalen Ansicht.
  Jetzt wird so eine Achse **über den Bildschirm** gezogen, gleichmäßig wie ein Maß quer zum Blick: Der Wert folgt dem Pfeil, der beim
  Überfahren erscheint (von leicht oben gesehen heißt das: nach unten ziehen macht tiefer). Gilt für Maß-Griffe und Gizmo-Pfeile. Test im
  Fenster: `app_handle_drag_front`.

**Zweite Nachbesserung: Kugel (Product Owner, 2026-10-18, nach dem Merge von PR #27)**
- *„Den Radius einer Kugel zu ändern läuft nicht rund; zu weit gezogen, geht ⌘Z nicht mehr; die App hängt.“*
  - **Hängen:** Die Darstellung rechnete jede runde Fläche mit fester Genauigkeit von 0,01 mm. Eine Kugel mit 800 mm Radius brauchte
    dafür Hunderttausende Dreiecke und Minuten. Währenddessen hielt der Hintergrund-Rechner ein Schloss, auf das auch die Oberfläche
    wartet: Das Fenster fror ein, auch ⌘Z ging nicht. Jetzt wächst die Darstellungs-Genauigkeit bei großen Körpern mit ihrer Größe
    (`render::kRelativeChord`, ein Zehntausendstel der Diagonale, auf dem Bildschirm weiter unter einem Viertel Bildpunkt; Körper bis
    100 mm bleiben wie bisher; der Export ist davon nicht betroffen). Und das Schloss wird nur noch zum Nachsehen und Ablegen gehalten,
    nie während der Kernel rechnet. Eine Kugel mit 2000 mm Radius erscheint in Sekunden. Neue Kernel-Abfrage `bounds` (umschließender
    Quader ohne Volumen und Prüfung).
  - **Nicht rund:** Die Kugel steht auf dem Bett; wächst sie, hebt sich ihr Äquator mit. Der Radius-Punkt wurde aber nur waagerecht
    gezogen und rutschte dem Zeiger davon. Griffe kennen jetzt eine Steigung (`HandleSpec::rise`): Der Punkt folgt dem schrägen Weg
    und bleibt unter dem Zeiger (ebenso die Rohrdicke des Torus). Nach dem Loslassen bleibt der Punkt, wo er ist, bis man die Ansicht
    dreht (vorher sprang er an eine andere Stelle des Rings).
  - **Sicherung:** Verschwinden die Griffe eines Schritts mitten im Zug kurz (er wird gerade berechnet oder scheitert), bleibt der
    gehaltene Griff im Fenster, damit der Zug ordentlich endet und Rückgängig wieder geht.
  - Tests: `sphere: a big one is drawn quickly …`, `detail: a big body is drawn with a coarser mesh …`, `handles: the radius dot of
    a sphere stays under the pointer …`.
- *„Steht der Radius-Wert links, verschwindet er hinter der Werkzeugleiste.“* Beschriftungen und Wertblasen der Griffe kannten nur den
  Rand der 3D-Ansicht. Jetzt meldet die Ansicht, was über ihr schwebt (Werkzeugleiste, Darstellungsleiste, Wählen-Filter,
  Formen-Galerie, Ansichtswürfel, Navigationsknöpfe), und eine Beschriftung weicht aus (`clearplace.js`): neben einem Punkt zuerst auf
  dessen andere Seite, sonst auf kürzestem Weg in die freie Fläche. Das gilt auch für die Wertblase des Gizmos.

**Einschränkungen, Annahmen**
- Das Fangen an Ecken, Kanten und Mitten anderer Körper kommt mit 1.6c; bis dahin rastet die Lage auf das Raster (ganze mm).
- Die Wertblase zeigt den **Abstand** des Zugs, die Felder rechts die **Lage** (Annahme: so sieht man beim Ziehen, wie weit, und rechts,
  wo). Getippt wird ebenfalls der Abstand.
- Bild↑/Bild↓ fehlen auf Mac-Laptops als Taste (dort Fn+↑/↓).
- „Drehen“ zeigt noch nichts (1.6b; Auftrag: [docs/prompts/ap-1.6b-drehen.md](../prompts/ap-1.6b-drehen.md)).

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
