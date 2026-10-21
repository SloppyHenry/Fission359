# AP-4.3 Verformen

**Größe:** M · **Phasendatei:** AP-4.3

## Ziel
Verdrehen, Biegen, Verjüngen, Wellen/Rauschen, Kugelig; dazu **Low-Poly** als bewusster Deko-Stil (der einzige Weg zu sichtbaren Facetten, UX §7.3).
Griffe für Stärke und Achse, automatische Netzverfeinerung für glatte Ergebnisse. Eine verdrehte Vase aus einem Polygon-Prisma entsteht in 3 Klicks.

## Voraussetzungen
AP-4.1 (Ergebnis ist frei).

## Vorher lesen
`docs/02-ux-konzept.md` §7.2 (Verzerren nur ausdrücklich, mit Hinweis – Ablauf D Schritt 7), §7.3; AP-4.1-Anleitung.

## Haltepunkte
1. **„Verzerren“** (ungleichmäßiges Skalieren, UX §7.2 / Ablauf D Schritt 7) hier als Verformer mit Warnung „Rundungen und Löcher werden dabei oval.
   Möchtest du stattdessen strecken?“ – bestätigen, dass es hierher gehört (wenn nicht schon in AP-2.4 gebaut).

## Schritte
1. Verformer als reine Funktionen auf Netzpunkten (`src/geometry/mesh/src/deform.cpp`): `twist(axis, degreesPerMm | totalDegrees)`, `bend(axis,
   angle, region)`, `taper(axis, factor)`, `noise(amplitude, scale, seed)`, `spherize(amount)`, `stretchNonUniform` (Verzerren). Vor dem Verformen
   **adaptiv verfeinern** (Kanten länger als Ziel-Länge teilen, Ziel aus Düse und Stärke der Verformung), damit Ergebnisse glatt sind.
2. Low-Poly: Netz vereinfachen auf N Facetten (Quadric-Vereinfachung, eigene einfache Umsetzung oder Manifold-Funktion falls vorhanden) – Wert
   „Facetten“.
3. Feature-Typ `deform` (Wort `kind`, Werte je Art, Achse als Wort/Verweis), Ergebnis frei. Präzise Eingänge werden mit Export-Qualität tesselliert.
4. Griffe: Achse als Linie mit Pfeil; Stärke als Ring (Verdrehen) bzw. Pfeil; Tippen ersetzt Ziehen. Regel 10.
5. Verzerren: Warnhinweis **bevor** etwas passiert (Ablauf D Schritt 7 als Test: Hinweis erscheint, nichts geändert bis zur Bestätigung –
   nicht modal: Karte mit [Trotzdem verzerren] [Stattdessen strecken]).

## Tests
Verdrehen eines Prismas: Volumen bleibt (Verdrehen ist volumenerhaltend, Toleranz 0,5 %), wasserdicht; Verjüngen Faktor 0,5 → Volumen nach Formel
(Pyramidenstumpf-ähnlich); Low-Poly N=20 → genau ≤ 20 Facetten-Gruppen; Ablauf D Schritt 7.

## Abnahme
So probierst du es aus: Prisma 6 Seiten, Verformen ▸ Verdrehen, Ring ziehen auf 90° → gedrehte Vase. Kugel ▸ Low-Poly 30.

## Nicht tun
Kein Verformen präziser Körper „in place“ (Ergebnis ist immer ein neuer freier Körper).
