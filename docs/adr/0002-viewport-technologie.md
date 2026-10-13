# ADR-0002: Viewport-Technologie

- **Status:** vorgeschlagen (Entscheidung des Product Owners steht aus)
- **Datum:** 2026-10-13
- **Arbeitspaket:** AP-0.4

## Kontext
Die 3D-Ansicht ist das Herz der Bedienung (Anfassen statt Dialoge, Hervorheben, Griffe, Vorschau).
Anforderungen aus Architektur § 5 und Roadmap: 60 Bilder/s bei 2 Mio. Dreiecken auf einem M1 bzw. einer
integrierten Intel/AMD-Grafik, Auswahl unter 16 ms, gute Optik (Schatten, Kanten, Umgebungsverdeckung),
macOS (Metal) und Linux (Wayland/X11, Vulkan/OpenGL), vollständig testbar. Technik: Qt Quick, OpenCASCADE.

## Optionen
1. **A: eigener Renderer auf Qt RHI** · läuft auf Metal, Vulkan, OpenGL und D3D · volle Kontrolle über
   Optik und Auswahl · nativ in Qt Quick (`QQuickRhiItem`) · mehr Eigenbau (Auswahl von Kanten und Ecken,
   Umgebungsverdeckung, Silhouetten) · nutzt Qt-RHI-Kopfzeilen ohne Kompatibilitätsgarantie.
2. **B: OpenCASCADE AIS/V3d (OpenGL) eingebettet** · Auswahl von Flächen, Kanten und Ecken fertig ·
   weniger Anfangsaufwand · nur OpenGL (auf macOS veraltet), erzwingt den OpenGL-Modus von Qt Quick ·
   Optik nur mit OCCT-eigenen Shadern anpassbar · headless schwerer zu testen.
3. **Hybrid:** AIS nur zum Auswählen, eigener Renderer zum Zeichnen · doppelte Datenhaltung, kein klarer
   Vorteil gegenüber A, das schon Auswahl hat.

## Entscheidung (Vorschlag)
Wir wählen **Option A (Qt RHI)**, weil

- beide Optionen das Leistungsbudget erfüllen (M1: 2 Mio. Dreiecke mit 163 Bildern/s, Auswahl unter 2 ms;
  RTX 3060: 5 Mio. mit 496 Bildern/s), die Leistung also nicht trennt;
- A unabhängig von OpenGL ist und damit den Wegfall von OpenGL auf macOS überlebt, B nicht;
- A die Optik der Architektur (Bett, Schatten, Kanten, später SSAO) in unseren eigenen Shadern liefert;
- A sich vollständig headless messen und testen lässt (Metal, Vulkan), was für die CI wichtig ist;
- die Lücke bei Kanten- und Eckenauswahl ein bekannter, begrenzter Aufwand ist.

## Konsequenzen
- AP-1.3 baut auf `spikes/viewport/a_rhi/` auf (`MeshPass` ist unabhängig vom Fenster).
- Folge-Arbeiten: Kanten- und Eckenauswahl (zusätzliche ID-Durchläufe), SSAO, Silhouetten, Griffe in 3D,
  adaptive Tessellierung, Instanziierung/Culling für sehr große Modelle.
- Qt-Version wird über vcpkg festgenagelt, damit sich die RHI-Schnittstelle nicht unbemerkt ändert; ein
  Qt-Update ist ein bewusster Schritt mit Test der Ansicht.
- **Revisionskriterium:** Wenn A für Kanten-/Eckenauswahl oder SSAO deutlich mehr als geplant kostet oder die
  Bildrate auf integrierter Grafik das Budget verfehlt (Test auf Intel HD 620 steht aus), wird B oder der
  Hybrid neu bewertet.

## Belege
- [Bericht mit Messwerten und Bildern](../spikes/viewport-technologie.md)
- Code: `spikes/viewport/`, Messen: `scripts/spike messen`
