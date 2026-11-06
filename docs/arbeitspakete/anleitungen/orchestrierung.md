# Orchestrierung: autonomer Ausbau mit Unter-Sitzungen

Festlegung 2026-11-05: Der Maintainer hat die Fertigstellung des Projekts an eine **leitende Sitzung (Orchestrierung)**
übergeben. Sie entscheidet selbst, verteilt die Arbeit an **Unter-Sitzungen (Umsetzer)** und führt die Ergebnisse zusammen.
Dieses Dokument regelt, wer was tut. Wie gebaut, getestet und formatiert wird, steht im [Handbuch](handbuch.md).

## Rollen

| Rolle | Wer | Aufgabe |
|---|---|---|
| **Orchestrierung** | eine leitende Sitzung mit dem stärksten Modell | Reihenfolge, Haltepunkte entscheiden, Anleitungen nachschärfen, Aufträge schreiben, Ergebnisse prüfen, PRs öffnen und mergen, Status und Abschlussvermerke, Projektstand pflegen |
| **Umsetzer** | Unter-Sitzung (Sonnet, Aufwand *medium*) | ein AP-Teilpaket oder eine Pflege-Aufgabe in eigener Arbeitskopie bauen, Tests schreiben und laufen lassen, committen, Bericht abgeben |
| **Helfer** | Unter-Sitzung (Sonnet, Aufwand *low*) | Mechanisches: Übersetzungen füllen, Formatieren, CI-Logs lesen und zusammenfassen, Doku nachziehen, Suchen im Code |
| **Maintainer** | Mensch | hat die Entscheidungen delegiert; wird nur bei den Punkten unter „Was weiter gefragt wird“ gefragt; testet nach eigenem Zeitplan |

Unter-Sitzungen starten keine weiteren Unter-Sitzungen. Sie entscheiden keine Haltepunkte.

## Entscheidungen

- **Haltepunkte entscheidet die Orchestrierung**, und zwar **bevor** sie den Auftrag schreibt. Maßstab: Produktregeln,
  Design-Prinzipien (`docs/01-vision.md`), Zielkonflikt-Reihenfolge in `CLAUDE.md`. Im Zweifel die Empfehlung der Anleitung.
- Jede Entscheidung wird festgehalten: kurz unter **Festlegungen** der Anleitung (vor dem Auftrag), im Abschlussvermerk der
  Phasendatei und, wenn sie über das AP hinaus gilt, in `docs/06-rueckfragen.md`. Architektur → ADR.
- **Neue Abhängigkeiten** sind erlaubt mit ADR, Lizenzprüfung (GPL-3-kompatibel, kein GPL-2.0-only) und Eintrag in
  `THIRD_PARTY_NOTICES.md`.
- **Dateiformat:** Änderungen nur mit Versionssprung, Migration und Roundtrip-Test (Architektur, ADR zum Format).
- **Ablauf D und die Produktregeln 1–13** werden nie abgeschwächt, auch nicht, um ein AP fertig zu bekommen. Lieber ein
  Werkzeug weniger (Zielkonflikt-Reihenfolge).

### Was weiter gefragt wird
Nur das, was nach außen wirkt oder sich nicht zurücknehmen lässt:
- Geld, Konten, Schlüssel und Zertifikate (z. B. Notarisierung, Domain, Update-Server), Veröffentlichung außerhalb von GitHub.
- Umschreiben veröffentlichter Git-Geschichte, Löschen außerhalb des Projekts.
- Eingriffe in Testrechner: Treiber, Kernel, Systemdienste, Firewall, Benutzer, Neustarts.
- Eine Lizenzänderung des Projekts.

Diese Fragen sammelt die Orchestrierung, arbeitet derweil an anderem weiter und stellt sie gebündelt (höchstens 3 Optionen,
mit Empfehlung und Folgen).

## Arbeitsschleife der Orchestrierung

1. **Nächstes Paket wählen:** Reihenfolge der Übersicht in [README](README.md), Voraussetzungen erledigt. Offene Branches
   und PRs zuerst abschließen, bevor neue entstehen.
2. **Anleitung prüfen:** „Ausgangslage im Code“ gegen den aktuellen `main` abgleichen (selbst oder per Helfer-Suche).
   XL-APs in Teilpakete zerlegen, je Teilpaket eine Anleitung.
3. **Haltepunkte entscheiden** und als Festlegungen eintragen.
4. **Auftrag schreiben** (Vorlage unten) und den Umsetzer starten. Unabhängige Pakete dürfen parallel laufen, höchstens
   **zwei Builds gleichzeitig** auf einem Rechner (OCCT und Qt brauchen viel Speicher).
5. **Bericht prüfen:** Diff lesen (`git -C <arbeitskopie> diff origin/main...`), gegen Akzeptanzkriterien, Definition of
   Done und Produktregeln. Mängel als Nachauftrag an denselben Umsetzer (fortsetzen statt neu starten).
6. **PR öffnen**, CI abwarten. Rote CI: Log per Helfer zusammenfassen lassen, Nachauftrag an den Umsetzer.
7. **Mergen**, sobald die CI grün ist und die Prüfung in Schritt 5 bestanden ist. Status `erledigt`, Abschlussvermerk,
   Arbeitskopie und Branch entfernen (Handbuch §2), Projektstand nachtragen.
8. Folgearbeiten in die passende Anleitung oder [pflege.md](pflege.md) übertragen. Weiter mit 1.

**Wenn ein Umsetzer zweimal am selben Problem scheitert**, übernimmt die Orchestrierung die Fehlersuche selbst oder zerlegt
die Aufgabe kleiner. Ein Paket, das nach drei Anläufen nicht korrekt wird, wird mit Begründung `zurückgestellt` und in den
Backlog geschrieben, statt es mit abgeschwächten Tests durchzudrücken.

## Wahl des Modells

| Arbeit | Unter-Sitzung |
|---|---|
| AP-Teilpaket, Pflege-Aufgabe mit Logik, Fehlersuche mit klarer Spur | Sonnet, *medium* |
| Übersetzungen, Formatieren, Doku nach Vorgabe, CI-Log lesen, Code durchsuchen | Sonnet, *low* |
| Haltepunkte, Zerlegen von XL-APs, Prüfung der Diffs, Architektur, festgefahrene Fehler | Orchestrierung selbst |

## Vorlage für einen Auftrag

Der Umsetzer kennt das Gespräch der Orchestrierung nicht. Der Auftrag muss für sich allein reichen.

```text
Du setzt in Fission359 <AP-x.y / Pflege Pn: Titel> um. Projekt: <absoluter Pfad zum Projektordner>.

Arbeitskopie: Lege sie an mit
  git -C <Hauptordner> fetch && git -C <Hauptordner> worktree add ../<Ordner> -b <branch> <basis>
und arbeite nur dort. Prüfe danach `git -C ../<Ordner> branch --show-current`.

Lies zuerst ganz: CLAUDE.md, docs/arbeitspakete/anleitungen/README.md, handbuch.md, die Anleitung <Datei>.
Die Haltepunkte sind entschieden: <Liste der Entscheidungen>. Halte dich daran; frag nicht nach.
Umfang dieses Auftrags: <Schritte n–m der Anleitung / genaue Abgrenzung>. Nicht tun: <Grenzen>.

Arbeitsweise: ein Commit je Schritt (englische Nachricht `AP-x.y: …`, Co-Authored-By-Zeile wie vorgegeben).
Nach jedem Schritt bauen (Release-Preset) und clang-format 21.1.8 (Handbuch §12). Schreib die Tests der Anleitung
mit und lass die Unit-Tests laufen (Handbuch §10). Keine Fenster-Tests auf dem Mac; die laufen in der CI.
Nicht pushen, keinen PR öffnen.

Hältst du an einer Stelle, die nicht entschieden ist, oder scheiterst du dreimal am selben Fehler: aufhören und berichten.
Tests nie abschwächen, Prüfungen nie abschalten.

Bericht am Ende (knapp): Branch und letzter Commit · was gebaut ist (Stichpunkte, je Akzeptanzkriterium erfüllt/offen) ·
Tests (neu, gelaufen, Ergebnis) · Annahmen · offene Reste · Entwurf „So probierst du es aus“ (3–6 Schritte).
```

## Projektstand

Status und Abschlussvermerke stehen in den Phasendateien, offene Kleinigkeiten in [pflege.md](pflege.md), Ideen für später im
[Backlog](../backlog.md). Nutzertests (AP-1.13, AP-2.15, AP-6.5) brauchen echte Menschen: Die Orchestrierung bereitet sie vor
(Aufgabenzettel, Fragebogen, Build), sie blockieren aber den weiteren Ausbau nicht. Ihre Ergebnisse gehen, sobald sie
vorliegen, jedem weiteren Paket vor.
