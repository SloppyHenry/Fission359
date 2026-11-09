# Bauanleitungen für offene Arbeitspakete

Jedes offene Arbeitspaket (AP) hat hier eine **Bauanleitung**. Sie ist so geschrieben, dass auch ein Coding-Modell, das das
Projekt nicht kennt und weniger erfahren ist, das AP **in unserem Sinne** umsetzen kann: Schritt für Schritt, mit den Dateien,
die es anfassen soll, den Tests, die es schreiben muss, und den Stellen, an denen es **anhalten und fragen** muss.

Wer die Arbeit verteilt, Haltepunkte entscheidet und mergt, regelt [orchestrierung.md](orchestrierung.md) (Festlegung 2026-11-05:
autonomer Ausbau durch eine leitende Sitzung mit Unter-Sitzungen als Umsetzer).

Die Phasendateien (`../phase-*.md`) sagen, **was** ein AP liefern soll (Umfang, Akzeptanzkriterien). Die Anleitung sagt, **wie**.
Bei Widersprüchen gilt: ADR > Phasendatei > Anleitung. Merkst du einen Widerspruch, halte an und berichte ihn der Orchestrierung.

## Bevor du anfängst (jedes Mal)

1. Lies [`CLAUDE.md`](../../../CLAUDE.md) (Projektregeln, vor allem die 13 Produktregeln), [orchestrierung.md](orchestrierung.md) und das [Handbuch](handbuch.md)
   (wie man baut, testet, formatiert, Texte übersetzt, einen Fenster-Test schreibt, einen PR öffnet).
2. Lies die Anleitung deines APs **ganz**, bevor du Code schreibst. Lies dann die Dateien, die unter „Vorher lesen“ stehen.
3. Prüfe die Abhängigkeiten: Steht jedes AP aus „Voraussetzungen“ in seiner Phasendatei auf **erledigt**? Wenn nicht: nicht
   anfangen, sondern der Orchestrierung sagen, welches AP zuerst kommt.
4. Leg eine eigene Arbeitskopie an (Handbuch §2). Arbeite **nie** im Hauptordner, dort können andere Sitzungen laufen.

## Aufbau jeder Anleitung

| Abschnitt | Was drinsteht |
|---|---|
| **Ziel** | Was eine Person danach kann, in Alltagssprache. Daran misst sich alles. |
| **Voraussetzungen** | APs, die erledigt sein müssen. |
| **Vorher lesen** | Dateien und Abschnitte, ohne die du falsch baust. |
| **Ausgangslage im Code** | Was es schon gibt (Klassen, Funktionen, Dateien), worauf du aufbaust. Prüfe es nach: der Code kann sich seit dem Schreiben geändert haben. |
| **Festlegungen** | Entscheidungen, die schon getroffen sind. Nicht neu verhandeln. |
| **Haltepunkte** | Fragen, die entschieden sein müssen, **bevor** der betroffene Teil gebaut wird. Jede hat eine Empfehlung. Die Orchestrierung entscheidet sie vor dem Auftrag und trägt das Ergebnis unter „Festlegungen“ ein; der Auftrag nennt die Entscheidungen. |
| **Schritte** | Nummerierte Arbeitsschritte. Nach jedem Schritt bauen, die genannten Tests **schreiben** und die Unit-Tests laufen lassen (Handbuch §10). Jeder Schritt ist ein Commit. |
| **Tests** | Konkrete Testfälle mit erwarteten Werten. Sie sind Pflicht, nicht Vorschlag. |
| **Abnahme** | Wie man prüft, dass das AP fertig ist, und die „So probierst du es aus“-Schritte für den PR. |
| **Stolperfallen** | Was hier schon schiefging oder leicht schiefgeht. |
| **Nicht tun** | Grenzen. Was außerhalb des APs liegt oder die Produktregeln verletzt. |

## Regeln für Umsetzer

- **Nichts raten, was nicht entschieden ist.** Haltepunkte, die der Auftrag nicht entscheidet: anhalten und berichten.
  Kleine technische Details darfst du selbst entscheiden; schreib sie in den Bericht unter „Annahmen“.
- **Klein anfangen, oft bauen.** Nach jedem Schritt: bauen, `scripts/tools/check_format.py`, Unit-Tests. Fenster-Tests laufen
  in der CI (Handbuch §10); jede neue Funktion bringt ihre neuen Tests sofort mit.
- **Tests zuerst bei Logik.** Für reine Rechnungen (Fangen, Maße, Geometrie) schreib den Test vor dem Code.
- **Nie still falsch** (Regel 6): Lieber ein Fehler am Schritt als ein falsches Ergebnis. Lieber eine Funktion weniger.
- **Keine neuen Abhängigkeiten** (Bibliotheken), die der Auftrag nicht nennt. Die Orchestrierung entscheidet sie mit ADR.
- **Ehrlich berichten:** Was nicht geht, nicht getestet ist oder vom Plan abweicht, steht im Bericht und in der PR-Beschreibung. Nie etwas als
  erledigt markieren, was nicht nachweislich funktioniert.
- **Wenn du feststeckst** (derselbe Fehler nach drei Versuchen, ein Test, den du nicht verstehst): anhalten, beschreiben was
  du versucht hast, berichten. Nicht Tests abschwächen, nicht Prüfungen abschalten.
- **Abschluss (Orchestrierung):** nach grüner CI und eigener Prüfung mergen, Status `erledigt` in der Phasendatei, knapper
  Abschlussvermerk dort (Datum, Ergebnis, weiter geltende Festlegungen, offene Reste). Ausführliches in den PR.
  Danach die Anleitung nach [erledigt/](erledigt/) verschieben und Folgearbeiten in die passende Anleitung übertragen.

## Übersicht

Reihenfolge = empfohlene Bau-Reihenfolge innerhalb der Phase. Größen: S (eine Sitzung) bis XL (mehrere Teilpakete).

### Phase 1 – Der erste Körper
Abgeschlossene Anleitungen (AP-1.6c2 bis AP-1.12) liegen zum Nachschlagen in [erledigt/](erledigt/).
| AP | Anleitung | Größe |
|---|---|---|
| 1.13 | [Nutzertest 1](AP-1.13.md) | S |
| – | [Pflege: Fehler, Politur und Folgearbeiten](pflege.md) | je S |

Nutzertests (AP-1.13, AP-2.15, AP-6.5) bereitet die Orchestrierung vor; sie blockieren den Ausbau nicht ([orchestrierung.md](orchestrierung.md), „Projektstand“).

### Phase 2 – Präzision
Erledigt (Anleitungen in [erledigt/](erledigt/)): AP-2.1, AP-2.10, AP-2.13, AP-2.9. Teil 1 in main: AP-2.14, AP-2.5, AP-2.4, AP-2.6, AP-2.11 (ohne Schnittansicht). Festlegung 2026-11-03: Phase 2 beginnt, während der Nutzertest 1 (AP-1.13) noch aussteht; AP-2.10 betrifft vor allem den Kern. Liegt die Auswertung vor, gehen ihre Top-3-Probleme jedem weiteren AP vor.
| AP | Anleitung | Größe |
|---|---|---|
| 2.14 | [Meldungskatalog und Fehler-UX](AP-2.14.md) | M |
| 2.2 | [Skizzen-Regeln und Maße (Solver)](AP-2.2.md) | XL |
| 2.12 | [Hilfsgeometrie](AP-2.12.md) | S |
| 2.3 | [Aus Skizzen werden Körper](AP-2.3.md) | L |
| 2.5 | [Abrunden und Abschrägen](AP-2.5.md) | L |
| 2.4 | [Direktes Bearbeiten von Flächen](AP-2.4.md) | L |
| 2.6 | [Aushöhlen und Schräge](AP-2.6.md) | M |
| 2.7 | [Bohrungen, Gewinde, Schraubverbindungen](AP-2.7.md) | L |
| 2.8 | [Muster und Spiegeln](AP-2.8.md) | M |
| 2.11 | [Messen und Analysieren](AP-2.11.md) | M |
| 2.15 | [Nutzertest 2](AP-2.15.md) | S |

### Phase 3 – Druckbereit
| AP | Anleitung | Größe |
|---|---|---|
| 3.1 | [Druckerprofile](AP-3.1.md) | S |
| 3.8 | [Exportqualität und Vorschau](AP-3.8.md) | S |
| 3.2 | [Druckcheck](AP-3.2.md) | L |
| 3.3 | [Passungs-Assistent](AP-3.3.md) | M |
| 3.4 | [Farben und Mehrfarbdruck](AP-3.4.md) | M |
| 3.5 | [Im Slicer öffnen](AP-3.5.md) | S |
| 3.6 | [Druckausrichtung vorschlagen](AP-3.6.md) | M |
| 3.7 | [Große Modelle teilen](AP-3.7.md) | M |

### Phase 4 – Spielwiese
| AP | Anleitung | Größe |
|---|---|---|
| 4.1 | [Freie Körper (Netze)](AP-4.1.md) | L |
| 4.2 | [Weich verschmelzen](AP-4.2.md) | XL |
| 4.3 | [Verformen](AP-4.3.md) | M |
| 4.4 | [Prägen und Gravieren](AP-4.4.md) | L |
| 4.5 | [SVG/DXF/Bild-Konturen, Ausstechformen](AP-4.5.md) | M |
| 4.6 | [Relief und Lithophanie](AP-4.6.md) | M |
| 4.7 | [Muster und Oberflächenstrukturen](AP-4.7.md) | L |
| 4.8 | [Generatoren](AP-4.8.md) | L |
| 4.9 | [Kaleidoskop-Modus](AP-4.9.md) | M |
| 4.10 | zurückgestellt, keine Anleitung (erst nach Nutzerfeedback zu Phase 4) | XL |

### Phase 5 – Lernen, Bausteine, Baukästen
| AP | Anleitung | Größe |
|---|---|---|
| 5.1 | [Missions-Engine und erste Missionen](AP-5.1.md) | L |
| 5.2 | [Tooltip-Animationen, „Zeig mir wie“](AP-5.2.md) | M |
| 5.3 | [Bausteine-Bibliothek](AP-5.3.md) | L |
| 5.5 | [Materialien, Darstellung, Teilen](AP-5.5.md) | S |
| 5.6 | [Barrierefreiheit und Tastatur](AP-5.6.md) | M |
| 5.7 | [Versionsverlauf und Varianten](AP-5.7.md) | M |
| 5.8 | [Baukasten-Engine](AP-5.8.md) | L |
| 5.9 | [Motiv-Bibliothek](AP-5.9.md) | L |
| 5.4 | [Startbildschirm](AP-5.4.md) | M |
| 5.10 | [Baukasten Karten](AP-5.10.md) | L |
| 5.11 | [Baukasten Vasen und Gefäße](AP-5.11.md) | M |
| 5.12 | [Baukasten Figuren](AP-5.12.md) | XL |
| 5.13 | [Weitere Baukästen](AP-5.13.md) | M je Baukasten |

### Phase 6 – Release 1.0
| AP | Anleitung | Größe |
|---|---|---|
| 6.6 | [Rechtliches](AP-6.6.md) | S |
| 6.1 | [Automatische Updates](AP-6.1.md) | M |
| 6.2 | [Absturzberichte und Diagnose](AP-6.2.md) | M |
| 6.3 | [Performance- und Stabilitätshärtung](AP-6.3.md) | L |
| 6.4 | [Dokumentation, Website, Beispiele](AP-6.4.md) | M |
| 6.5 | [Beta-Programm](AP-6.5.md) | S |

## Neue Aufgaben

Wer eine neue Aufgabe anlegt (neues AP, Teilpaket, Folgearbeit), schreibt ihre Anleitung im selben Aufbau. Wer ein AP
zerlegt, legt je Teilpaket eine Anleitung an. Anleitungen für Pakete, die noch weit weg sind, werden vor dem Start von
der Orchestrierung gegen den dann aktuellen Code geprüft und nachgeschärft (Abschnitt „Ausgangslage im Code“).
