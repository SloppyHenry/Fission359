# AP-4.2 Weich verschmelzen („Knete“)

**Größe:** XL, Spike + Teile a–c · **Phasendatei:** AP-4.2

## Ziel
Formen fließen weich ineinander: weich vereinen, weich abziehen, weich überlappen, mit Regler „Weichheit“ (mm). Die Auflösung wählt das Programm nach
Größe, Weichheit und **Düse** (keine sichtbaren Facetten im Druck), Regler „Feinheit“ unter ▸ Mehr; Vorschau grob, Ergebnis fein. Präzise Eingänge
werden über ein Distanzfeld verrechnet, das Ergebnis ist frei. Kugel + Stern (Ablauf C) verschmilzt in < 2 s; das Ergebnis ist immer wasserdicht.

## Voraussetzungen
AP-4.1 (freie Körper).

## Vorher lesen
`docs/02-ux-konzept.md` §6, §7.3, §12 („Weich verschmelzen / Melt Together“), §15 C; `docs/03-architektur.md` §4 („SDF für Deko“: BVH + Winding Number
oder OpenVDB, Entscheidung im Spike), §8.

## Spike (zuerst, eigener PR, kein Produktionscode)
Ziel: belastbare Entscheidung per ADR. Vergleich zweier Wege, Signierte Distanzfelder (SDF) aus Netzen zu berechnen:
- **A: eigener BVH + Generalized Winding Number** (Vorzeichen innen/außen robust auch bei leicht kaputten Netzen) + Abtastung auf Gitter, Netz über
  Manifold `LevelSet` (oder Dual Contouring).
- **B: OpenVDB** (MPL-2.0; Lizenz GPL-3-kompatibel, aber großes Paket mit TBB/Boost-Abhängigkeiten – Paketgröße und Bauzeit messen).
Messen (Kugel + Stern Ablauf C, Figur aus 6 Teilen, 1 Mio. Dreiecke Eingang): Zeit je Auflösung, Speicher, Qualität (Abweichung vom analytischen SDF
bei Grundformen), Robustheit bei kaputtem Netz, Paketgröße. Bericht in `docs/spikes/weich-verschmelzen.md`, ADR mit Empfehlung, **Entscheidung durch den
Product Owner** abwarten. Analytische SDFs für Grundformen (Kugel, Quader mit Rundung, Zylinder, Torus …) direkt aus den Werten berechnen (exakt und
schnell) statt aus dem Netz – im Spike zeigen, wie viel das bringt.

## Teil 4.2a – Feld-Kern
`geo::Field` (implizite Form) in `src/geometry/mesh`: aus Netz (nach Spike), aus Grundform (analytisch), Operationen smoothUnion/Subtract/Intersect
(polynomielle Glättung mit Radius k in mm), Offset („Aufblasen/Schrumpfen“), Glätten. Zu Netz: Auflösung = min(k/3, Düse/2, Größe/256)… Formel im
ADR festlegen; Gitter nur im Hüllquader + Rand. Parallel, abbrechbar.

## Teil 4.2b – Feature
`smoothCombine`: Eingänge Körper, Wort `mode`, Werte `softness` (mm), `fineness` (▸ Mehr, Faktor). Vorschau beim Ziehen am Regler mit grober Auflösung
(sichtbar „Vorschau“), danach fein im Hintergrund. Ergebnis frei (Badge). Regel 10 („verschmelzen“, „knete“, „weich“, „melt“, „blend“, „smooth“).

## Teil 4.2c – Qualität und Druckbarkeit
Ergebnis immer manifold (Manifold-Konstruktion prüft) und ohne Selbstüberschneidung; Druckcheck läuft (AP-3.2). Property-Test: 50 zufällige
Kombinationen von Grundformen → immer wasserdicht. Zeit-Test Ablauf C < 2 s (Release, Referenzrechner M1; CI mit großzügiger Grenze).

## Abnahme
So probierst du es aus: Stern + Kugel in der Mitte, beide wählen ▸ Kombinieren ▸ Weich verschmelzen, Regler ziehen → Übergang wird weicher; Zacken im
Rezept 8 → 6 → neu verschmolzen.

## Nicht tun
Keine neue Bibliothek ohne Spike-Entscheidung. Kein Verschmelzen ohne Vorschau-Rückmeldung (UI darf nie hängen, Regel 9).
