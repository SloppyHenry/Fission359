# AP-5.5 Materialien, Darstellung und Teilen

**Größe:** S · **Phasendatei:** AP-5.5

## Ziel
Körper sehen aus wie das Filament: PLA matt, Silk (glänzend), PETG glänzend, transparent, Marmor, Holz-PLA. Ein Bildschirmfoto (freigestellt, hohe
Auflösung) und ein Drehteller-Video/GIF lassen sich mit einem Klick für soziale Netze erzeugen.

## Voraussetzungen
AP-1.3 (Renderer), AP-3.4 (Filament-Palette, Material je Platz).

## Vorher lesen
`src/render/src/scene_renderer.cpp` (Shader, mattes Licht), `docs/08-gestaltung.md`, AP-5.2 (Aufnahme-Modus, Bildfolge).

## Haltepunkte
1. **Video-Format:** wie AP-5.2 ohne neue Abhängigkeit: animiertes WebP/GIF (eigener GIF-Schreiber oder Qt-Bildplugin); MP4 bräuchte FFmpeg (neue
   Abhängigkeit, ADR). Empfehlung: GIF + WebP.

## Schritte
1. Materialparameter je Körper (aus dem Palettenplatz): Glanz, Rauheit, Transparenz, einfache prozedurale Textur (Marmor = Rauschen, Holz = Ringe) im
   Shader. Transparenz: zweiter Durchgang, sortiert nach Körper (einfach), keine perfekte Ordnung nötig.
2. Bildschirmfoto: Offscreen in 2×/4× Auflösung, Hintergrund transparent oder neutral, ohne Bett wählbar.
3. Drehteller: Kamera 360° um das Modell, 3 s, 30 Bilder; Ausgabe nach Haltepunkt 1; Größe < 5 MB Ziel.
4. Menü Datei ▸ Teilen ▸ Bild / Drehteller (Datei speichern; kein Hochladen).

## Tests
Renderer-Test (Null-RHI): Materialparameter kommen im Uniform an; Bildgröße und Transparenz des Exports.

## Abnahme
So probierst du es aus: Körper Material „Silk Rot“ → glänzt; Teilen ▸ Drehteller → GIF.

## Nicht tun
Kein Raytracing. Kein Hochladen in soziale Netze.
