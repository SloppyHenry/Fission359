# AP-5.2 Tooltip-Animationen und „Zeig mir wie“

**Größe:** M · **Phasendatei:** AP-5.2

## Ziel
Jedes Werkzeug hat eine kurze Loop-Animation (3–5 s) in seiner Tooltip-Karte. Die Animationen werden **per Skript aus der App selbst** aufgenommen
(Aufnahme-Modus), damit sie bei UI-Änderungen neu erzeugt werden können. Ein Knopf „Zeig mir wie“ im Eigenschaften-Panel spielt die Animation zum
aktiven Werkzeug. Alle Werkzeuge der Phasen 1–4, zusammen < 30 MB.

## Voraussetzungen
AP-1.12 (Tooltip-Karte mit Platz für Animation).

## Vorher lesen
Handbuch §9 (`--simulate`, `--smoke-test`), `apps/fission3d/src/main.cpp` (`playActions`); `docs/02-ux-konzept.md` §11.

## Haltepunkte
1. **Format:** Empfehlung: animiertes WebP oder kurze Bildfolge (PNG-Sprite) statt Video, weil Qt Multimedia eine neue Abhängigkeit wäre. Qt kann
   animierte Bilder über `AnimatedImage` (GIF/WebP mit Qt-Plugin `qwebp` – prüfen, ob es im Paket ist). Bestätigen und Größe messen.

## Schritte
1. **Aufnahme-Modus:** Kommandozeile `--record <datei> --size 480x300 --simulate "<aktionen>"`: spielt Aktionen und nimmt jedes Bild auf
   (`QQuickWindow::grabWindow` je Bild, 15 fps), Mauszeiger als gezeichnetes Overlay (sichtbar, da echte Maus fehlt).
2. **Skripte** je Werkzeug in `resources/animations/<werkzeug>.sim` (eine Zeile Aktionen + Kamera), Erzeugungsskript `scripts/tools/make_animations.py`
   (ruft das Programm je Datei, wandelt in das Format nach Haltepunkt 1, prüft Gesamtgröße).
3. Einbinden in `TooltipCard.qml` (AP-1.12) und Knopf „Zeig mir wie“ (Panel).
4. CI: Animationen werden **nicht** in jedem Lauf erzeugt; ein Test prüft nur, dass jedes Werkzeug im Befehlskatalog eine Animationsdatei hat und die
   Gesamtgröße < 30 MB ist.

## Abnahme
So probierst du es aus: Über „Drehen“ fahren → Karte mit kleiner Animation; Panel ▸ „Zeig mir wie“.

## Nicht tun
Keine von Hand gezeichneten Animationen (veralten). Kein Ton.
