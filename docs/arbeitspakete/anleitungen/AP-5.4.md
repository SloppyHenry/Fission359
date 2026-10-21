# AP-5.4 Startbildschirm „Was möchtest du bauen?“

**Größe:** M · **Phasendatei:** AP-5.4

## Ziel
Kacheln: Karte, Vase/Gefäß, Figur, Christbaumschmuck, Schild, Halterung, Box, Ersatzteil, Leer. Jede startet den passenden Baukasten oder bietet passende
Missionen und Bausteine an – keine starren Vorlagen. Eine Inspirations-Galerie mit ≥ 20 druckbaren Beispielen; „So gemacht“ öffnet den Baukasten mit genau
diesen Einstellungen. Dazu „Meine Kombinationen“ und zuletzt geöffnete Dateien mit großen Vorschaubildern. Von jeder Kachel in ≤ 2 Klicks am Ziel.

## Voraussetzungen
AP-5.3, AP-5.8 (Baukästen), AP-1.11d (Startseite Version 1, `StartPage.qml`), AP-5.1 (Missionen).

## Vorher lesen
`docs/02-ux-konzept.md` §11 (Startbildschirm), `docs/07-baukaesten.md`; Mockup (Startseite).

## Haltepunkte
1. **Kacheln ohne eigenen Baukasten** (Halterung, Ersatzteil, Box falls 5.13 noch nicht da): Empfehlung: Halterung → Mission 3 + Bausteine
   (Wandhalter-Lasche, Schraubenlöcher); Ersatzteil → kurzer Einstieg „Maße abnehmen“ (Hinweiskarte) + Skizze; Box → Generator Box (AP-4.8).

## Schritte
1. `StartPage.qml` erweitern (nach Mockup): Kachelreihe, Galerie, Zuletzt, Kombinationen.
2. Inspirations-Beispiele: `resources/inspiration/<id>/` mit `.fission` (oder Baukasten-Zustand + Seed) und Vorschaubild; ≥ 20; Test: jedes lädt, ist gültig,
   Druckcheck ohne rote Funde (Akzeptanzkriterium 2).
3. „So gemacht“: öffnet ein neues Dokument mit dem Baukasten im gespeicherten Zustand.
4. Klickpfade messen (Test: je Kachel Ziel in ≤ 2 Aktionen – Fenster-Test mit `click:`-Folgen oder Prüfung der Konfiguration).

## Abnahme
So probierst du es aus: Programm starten → „Karte“ → Baukasten Karten; Galerie ▸ „So gemacht“ → Baukasten mit diesen Einstellungen.

## Nicht tun
Keine festen Vorlagen-Dateien, die nur beschriftet werden.
