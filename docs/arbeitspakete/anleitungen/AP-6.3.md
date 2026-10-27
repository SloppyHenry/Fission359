# AP-6.3 Performance- und Stabilitätshärtung

**Größe:** L · **Phasendatei:** AP-6.3

## Ziel
Alle Budgets aus Architektur §12 sind mit großen Modellen eingehalten (500 Schritte, 5 Mio. Dreiecke, 200 Körper). Keine Speicherlecks. 8 Stunden
zufällige Bedienung („Monkey-Test“) ohne Absturz. Alle Datei-Lader 24 h gefuzzt ohne Fund.

## Voraussetzungen
Phasen 1–5 weitgehend fertig.

## Vorher lesen
`docs/03-architektur.md` §10 (Fuzzing, Benchmarks mit Schwellen), §12 (Budgets); `--benchmark` (AP-1.3), `scripts/coverage`, CI `debug-asan`.

## Haltepunkte
1. **Benchmarks in der CI** mit Schwelle (Regression > 15 % = rot) brauchen stabile Läufer; GitHub-Runner schwanken. Empfehlung: Benchmarks nur
   beratend in der CI, verbindlich auf einem festen Rechner (Testrechner) per Skript vor jedem Release.
2. **Tracy** (BSD, Profiler) als Entwickler-Abhängigkeit nur in einem eigenen Preset? ADR, falls ja.

## Schritte
1. **Benchmark-Modelle** erzeugen (Skript `scripts/tools/make_bench_models.py` erzeugt `.fission` mit 500 Schritten, 200 Körpern, 5 Mio. Dreiecken über
   feine Tessellierung), in `tests/fixtures/bench/` (LFS).
2. **Benchmark-Programm** `fission359 --bench-suite` misst: Kaltstart, Öffnen, Neuberechnung nach Maßänderung, Griff-Vorschau, Bildzeit, Picking, UI-Blockade
   (längste Pause im UI-Thread über einen Watchdog-Timer); Ausgabe JSON; Vergleich mit Baseline-Datei.
3. **Profiling** (Instruments/`perf`) der Ausreißer; Optimierungen je eigener PR mit Messung vorher/nachher.
4. **Lecks:** ASan/LSan-Lauf aller Tests (gibt es in der CI) plus Langläufe; macOS `leaks`.
5. **Monkey-Test:** `--simulate`-Generator mit zufälligen, gültigen Aktionen (Seed), 8 h auf dem Testrechner (Xvfb), Absturz → Seed und Aktionsfolge
   protokollieren (reproduzierbar).
6. **Fuzzing:** libFuzzer-Ziele (Clang) für Ausdrucksparser, `.fission`-Lader, 3MF, STL, SVG, DXF, STEP-Import (OCCT – nur Abstürze in unserem Code
   zählen); eigenes Preset `fuzz`; 24 h je Ziel auf dem Testrechner; Korpus im Repo (klein).

## Abnahme
Budget-Tabelle mit gemessenen Werten im PR; Monkey-Test-Protokoll 8 h; Fuzzing-Protokolle.

## Nicht tun
Keine Optimierung ohne Messung. Keine Budgets „anpassen“, um grün zu werden (Haltepunkt beim Product Owner, wenn unerreichbar).
