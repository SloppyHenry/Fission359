# AP-5.8 Baukasten-Engine

**Größe:** L, Teile a–d · **Phasendatei:** AP-5.8

## Ziel
Die Maschine hinter allen Baukästen: Ein Baukasten ist eine **Datendatei** (Schritte → Optionsgruppen → Optionen → Abbildung auf Features, Werte,
Bausteine, Motive), validiert gegen ein Schema. Die Baukasten-Leiste ersetzt links die Seitenleiste: Schrittleiste (frei anspringbar), Optionskacheln
mit **Live-Vorschau am eigenen Modell**, „▸ Mehr“, Weiter/Zurück, „Frei weiter“. Das Ergebnis ist eine **Rezept-Gruppe** normaler Schritte; Doppelklick
öffnet den Baukasten wieder. Frei geänderte Schritte sind „von dir angepasst“ und werden nur auf Nachfrage überschrieben. „Überrasch mich“ mit Schloss
je Schritt, reproduzierbar per Seed. „Meine Kombinationen“ speichern/laden. Optionen, die nicht zum Drucker passen, werden erklärt statt versteckt.

## Voraussetzungen
AP-2.9, AP-5.3 (Gruppen im Rezept, Bausteine), AP-1.9 (Rezept).

## Vorher lesen
`docs/07-baukaesten.md` (**ganz**, Grundregeln 1–9 verbindlich), `docs/02-ux-konzept.md` §3, §11; Mockup (Baukasten-Leiste, falls vorhanden; sonst
Mockup zuerst – Haltepunkt).

## Haltepunkte
1. **Abbildung Option → Rezept** (Kern des ADR „Baukästen als Daten“). Empfehlung: Jede Option ist eine Liste von **Operationen** auf einer kleinen,
   festen Befehlssprache: `setValue(<schritt-rolle>, <wert>, <ausdruck>)`, `addStep(<rolle>, <typ>, <werte>)`, `removeStep(<rolle>)`, `placePart(<baustein>,
   <andock>)`, `placeMotif(<motiv>, …)`, `setColour(<rolle>, <palettenplatz>)`. Schritte der Gruppe haben **Rollen** (z. B. `background`, `frame`), über die
   Optionen sie finden. Keine Skriptsprache. Mockup der Leiste zeigen lassen.
2. **„Von dir angepasst“:** Empfehlung: Die Gruppe merkt sich je Rolle den Wert-Fingerabdruck, den der Baukasten zuletzt gesetzt hat; weicht der
   aktuelle ab, ist die Rolle „angepasst“ (Symbol am Schritt und an der Option). Bestätigen.

## Teil 5.8a – Format und Validierung
Schema `resources/kits/schema.json`, Baukästen `resources/kits/<id>.json` (Texte wie bei Missionen in `KitTexts.js` mit `qsTr`, AP-5.1 Haltepunkt 2).
Lader mit Validierung (Pflichtfelder, bekannte Operationen, bekannte Feature-Typen/Bausteine/Motive) – Fehler im Protokoll und Baukasten ausgeblendet.
Test: jede mitgelieferte Datei gültig; ein Test-Baukasten ohne Code-Änderung lädt und läuft (Akzeptanzkriterium 1).

## Teil 5.8b – Engine und Rezept-Gruppe
Anwenden einer Option = **eine** Transaktion mit den Operationen (ein Undo-Schritt „Hintergrund: Stern“). Gruppe im Rezept (aus AP-5.3) mit Name des
Baukastens; Doppelklick öffnet den Baukasten mit dem Zustand der Gruppe (gewählte Optionen je Schritt werden in der Gruppe gespeichert – Dateiformat:
Roundtrip + Migration). Verlassen („Frei weiter“), frei ändern, wieder öffnen → eigene Änderungen bleiben (Akzeptanzkriterium 2).

## Teil 5.8c – Vorschau-Kacheln
Je Option im sichtbaren Schritt: Kopie des Dokuments, Option anwenden, neu berechnen, offscreen rendern (kleines Bild), zwischenspeichern
(Schlüssel = Dokument-Revision + Option). Im Hintergrund, abbrechbar, sichtbarer Schritt zuerst, Platzhalter bis fertig. Erste Kachel < 1 s, UI nie blockiert
(Akzeptanzkriterium 3, messen). Speicher begrenzen (z. B. 200 Bilder).

## Teil 5.8d – Würfeln, Kombinationen, Drucker
„Überrasch mich“: je Schritt zufällige Option (Seed sichtbar/kopierbar), Schloss nimmt Schritte aus. „Meine Kombinationen“: gewählte Optionen + Seed als
kleine Datei im Benutzerordner. Drucker-Bewusstsein: Optionen haben Bedingungen (`requires: bedX >= 150`, `colours >= 2`); nicht erfüllt → Kachel
ausgegraut mit Satz („Passt nicht auf dein Bett (180 mm). Tipp: Klappkarte“).

## Abnahme
Kriterien 1–3. So probierst du es aus: Test-Baukasten öffnen, Optionen durchklicken (Kacheln zeigen das eigene Modell), Frei weiter, etwas ändern,
Gruppe doppelklicken → „von dir angepasst“ markiert.

## Nicht tun
Keine Baukasten-Logik in Code für einzelne Baukästen (Grundregel 9). Keine Online-Baukästen.
