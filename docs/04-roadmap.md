# Roadmap

## Phasen im Überblick

| Phase | Name | Ziel | Meilenstein-Demo | Datei |
|---|---|---|---|---|
| **0** | Fundament | Build, CI, Pakete, technische Risiken früh klären | Leeres Fenster mit drehbarem 3D-Würfel, signiert installierbar auf macOS und Linux | [phase-0-fundament.md](arbeitspakete/phase-0-fundament.md) |
| **1** | Der erste Körper | Grundformen, Bewegen, Kombinieren, Speichern, Export | **M1:** Schlüsselanhänger (Ablauf A ohne Text-Prägung auf gekrümmten Flächen) | [phase-1-erster-koerper.md](arbeitspakete/phase-1-erster-koerper.md) |
| **2** | Präzision | Skizzen, Ziehen/Drehen, Abrunden, Bohrungen, Parameter, Rezept | **M2:** Wandhalterung (Ablauf B) komplett inkl. Maßänderung | [phase-2-praezision.md](arbeitspakete/phase-2-praezision.md) |
| **3** | Druckbereit | Druckcheck, Passungen, Mehrfarbig, Slicer | **M3:** Box mit Deckel, die beim ersten Druck passt, zweifarbig | [phase-3-druckbereit.md](arbeitspakete/phase-3-druckbereit.md) |
| **4** | Spielwiese | Weich verschmelzen, Verformen, Prägen, Bilder, Generatoren | **M4:** Weihnachtsstern (Ablauf C) | [phase-4-spielwiese.md](arbeitspakete/phase-4-spielwiese.md) |
| **5** | Lernen, Bausteine und Baukästen | Missionen, Bausteine, Motiv-Bibliothek, Baukästen (Karten, Vasen, Figuren) | **M5:** Neuling schafft 3 Missionen und gestaltet in ≤ 5 min eine eigene Weihnachtskarte | [phase-5-lernen-bausteine.md](arbeitspakete/phase-5-lernen-bausteine.md) |
| **6** | Release 1.0 | Updates, Stabilität, Lokalisierung, Beta | **M6:** Öffentliche 1.0 | [phase-6-release.md](arbeitspakete/phase-6-release.md) |
| — | Backlog | Baugruppen, Skript-API, KI-Assistent, Zeichnungen, Windows … | — | [backlog.md](arbeitspakete/backlog.md) |

> **Wichtig:** Lernhilfen (Hinweiszeile, Tooltip-Texte, verständliche Fehlermeldungen) sind **nicht** erst Phase 5. Jedes Arbeitspaket liefert seine eigenen Texte gleich mit. Phase 5 baut die Missions-Engine, die Bibliotheken und die Baukästen.

## Kritischer Pfad

```
AP-0.1 Repo/Build ─► AP-0.2 Abhängigkeiten ─► AP-0.4 Viewport-Spike ─► AP-1.3 Viewport ─► AP-1.4 Auswahl ─┐
                                   │                                                                      │
                                   └► AP-0.5 Kernel-Spike ─► AP-1.1 Dokument/Features ─► AP-1.2 Undo ──────┤
                                                    │                                                      ▼
                                                    └► AP-0.6 Naming-Spike ───────────────► AP-2.10 Naming produktiv
                                                                                                           │
  AP-1.5 Formen ─► AP-1.6 Gizmo ─► AP-1.7 Kombinieren ─► AP-2.1/2.2 Skizzen ─► AP-2.3 Ziehen ─► AP-2.5 Abrunden …
```

Die Abhängigkeiten stehen im Detail an jedem Arbeitspaket.

## Größen

Die Größenangaben an den Arbeitspaketen sind relativ, nicht in Tagen:

| Größe | Bedeutung |
|---|---|
| **S** | überschaubar, ein klar abgegrenztes Teilproblem |
| **M** | mehrere Komponenten, einige offene Detailfragen |
| **L** | großes Teilsystem, braucht vermutlich mehrere Iterationen |
| **XL** | eigenes Forschungs- bzw. Risikothema; sollte vor dem Start in kleinere APs zerlegt werden |

## Statuspflege

Jedes Arbeitspaket hat eine Zeile `**Status:**` mit einem dieser Werte:
`offen` · `in Arbeit` · `Review` · `erledigt` · `zurückgestellt`.
Wer an einem AP arbeitet (Mensch oder Agent), aktualisiert den Status in der Phasendatei und ergänzt am Ende des APs einen knappen **Abschlussvermerk** (Datum, Ergebnis, weiter geltende Festlegungen, offene Reste). Testnachweise und die Ausprobier-Anleitung stehen in der PR-Beschreibung.

## Definition of Done (gilt für jedes Arbeitspaket)

Ein AP ist erst **erledigt**, wenn alle Punkte erfüllt sind:

- [ ] Alle Akzeptanzkriterien des APs sind erfüllt und nachweisbar (Test oder Demo-Anleitung).
- [ ] Baut ohne Warnungen auf **macOS (arm64) und Linux (x86_64)** in der CI; alle Tests grün.
- [ ] Neue Logik hat Unit-Tests; neue Geometrie-Operationen haben Golden-Tests.
- [ ] Jede neue Nutzeraktion ist **rückgängig machbar** und übersteht **Speichern → Laden**.
- [ ] Alle UI-Texte sind auf **Deutsch und Englisch** vorhanden und verwenden das Glossar aus dem UX-Konzept.
- [ ] Neue Werkzeuge haben **Hinweiszeilen-Text, Tooltip-Karte und Synonyme** für die Befehlssuche.
- [ ] Fehlerfälle erzeugen **verständliche Meldungen** aus dem Meldungskatalog (keine rohen Kernel-Fehler).
- [ ] Keine Verletzung der Performance-Budgets (Architektur, Abschnitt 12).
- [ ] Doku aktualisiert: AP-Status und Abschlussvermerk; bei Architekturentscheidungen ein ADR.
- [ ] Eine **„So probierst du es aus“-Anleitung** (3–6 Schritte) steht in der PR-Beschreibung.
