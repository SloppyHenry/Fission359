# Produktentscheidungen und offene Annahmen

## Entschieden

| # | Frage | Antwort | Auswirkung in den Dokumenten |
|---|---|---|---|
| R2 | Open Source oder kommerziell? | **Open Source** | Vorschlag **GPL-3.0-or-later** (Architektur §1 und §8). Abhängigkeiten müssen GPL-3-kompatibel sein. AP-0.1: LICENSE, CONTRIBUTING, Code of Conduct, Vorlagen. AP-6.6: DCO statt CLA |
| R4 | Fertigung | FDM-3D-Druck | – |
| R5 | Zieldrucker | Bambu Lab und Prusa, inkl. Mehrfarbdruck | AP-3.1, AP-3.4 |
| R15 | Schwächen von Tinkercad, die Fission359 lösen soll | 1) umständliches Ausschneiden, 2) Rundungen verziehen sich beim ungleichmäßigen Größe-Ändern, 3) niedrige Auflösung, 4) rudimentäre Funktionen | **UX §7.1–7.4** mit verbindlichen Antworten; neues **Design-Prinzip 12**; Produktregeln 11–13 (CLAUDE.md); **Ablauf D** als automatisierter Regressionstest; konkrete Kriterien in AP-1.3, 1.5, 1.6, 1.7, 2.4, 4.2, 4.3 |
| R2b | Projektlizenz | **GPL-3.0-or-later** bestätigt | Architektur §1/§8, CLAUDE.md |
| R17 | Wie soll die Oberfläche aussehen? | **Verbindlich** (bestätigt 2026-10-13): wie das klickbare Mockup [mockups/oberflaeche.html](mockups/oberflaeche.html): dunkel und ruhig, Werkzeugpalette in der 3D-Ansicht, Lotse für Missionen, Startseite, Druckcheck & Export als Assistent | Neues Dokument **[08-gestaltung.md](08-gestaltung.md)** (Design-System, in der Pflichtlektüre von CLAUDE.md); **UX §1** und **§13** verweisen darauf. Statt „Geführt \| Experte“ lässt sich der **Lotse jederzeit ein- und ausschalten**; er blendet nur Hilfe ein und aus, nie Werkzeuge oder Felder (Design-Prinzip 8). Mehr Felder gibt es unabhängig davon über „Alle Einstellungen zeigen“. **Fläche der 3D-Ansicht:** mit beiden Seitenleisten 48 % bei 1280 × 800; das Ziel 65 % wird über den **Fokus-Modus** erreicht (84 %, Knopf am Ansichtswürfel, ⇧F) |
| R18 | Akzentfarbe | **Frei wählbar** unter Einstellungen ▸ Aussehen: Vorschläge (Standard Blau) und eigene Farbe | **UX §13**, Glossar. Das Programm leitet alle Akzenttöne aus der gewählten Farbe ab und hält WCAG AA ein. Ähnelt die Farbe Rot, Gelb oder Grün (Hinweisfarben), erscheint ein Hinweis, gesperrt wird sie nicht |
| R20 | Wofür ist die Datei? | Der Export fragt nach dem Zweck: **3D-Druck, Spiel-Engine, Andere Programme, Web & AR, Bild**, und schlägt passende Formate vor | Mockup: Export-Assistent. **Neu gegenüber Architektur §5:** glTF/GLB, FBX und USDZ. Vor der Umsetzung als Arbeitspaket mit ADR aufnehmen (Bibliotheken und Lizenzen prüfen; kein proprietäres FBX-SDK). Bis dahin im Mockup als „Vorschlag“ markiert |
| R21 | Mindest-Schriftgröße | Das **Design-System gilt**: Grundgröße 13 px, kleinste Schrift 12 px bei 100 %; wer größer will, skaliert die Oberfläche (80–200 %) | Entscheidung 2026-10-14 (AP-0.8). UX-Konzept §14 angepasst. Qt Quick kennt nur ganze Pixel: 12,5 und 14,5 px des Mockups werden 13 und 15 px |
| R19 | Griffe | **Beide Griff-Arten bleiben** und sind einstellbar: „Achsen“ (Pfeile X/Y/Z, Maßbänder) und „Klassisch“ (Eck-Quadrate wie Tinkercad) | **UX §2.2**, Glossar. Beide ändern Maße gleich (Regeln 11–13 gelten für beide; Ablauf D wird mit beiden Griff-Arten geprüft). **Voreinstellung: Achsen** |
| R16 | Winkeltoleranz beim Export | **5°** statt 0,5° bei 0,01 mm Sehnenabweichung (2026-10-13): 0,5° gibt etwa zehnmal so viele Dreiecke und ist 15- bis 100-mal langsamer, ohne bessere Maßhaltigkeit | Architektur §6; Bericht [kernel-robustheit.md](spikes/kernel-robustheit.md) |
| R22 | Wer entscheidet und baut bis 1.0? | **Autonomer Ausbau** (2026-11-05): Eine leitende Sitzung entscheidet Haltepunkte, UX, Abhängigkeiten und Dateiformat selbst, lässt Unter-Sitzungen bauen, prüft und mergt bei grüner CI | [orchestrierung.md](arbeitspakete/anleitungen/orchestrierung.md); CLAUDE.md „Zusammenarbeit“ und „Arbeitsweise“; Handbuch §2 und §10. Gefragt wird nur noch bei Geld, Konten, Schlüsseln, Veröffentlichung außerhalb von GitHub, Umschreiben der Git-Geschichte und Eingriffen in Testrechner |

---

## Annahmen

Jede Frage hat eine **aktuelle Annahme**, auf der die Dokumente beruhen.

### R3 – Native UI wirklich Pflicht?
> **Annahme:** Ja. Komplett nativ mit C++/Qt, keine Web-Technik-UI (Tauri/Electron).
> **Ergänzung 2026-11-10:** Zusätzlich eine Browser-Version aus derselben Codebasis (Qt for WebAssembly), rechnet lokal, ohne Konto (ADR-0014, AP-6.7).

### R4b – Auch Resin (SLA)?
> **Annahme:** Nein, nur FDM. Resin bräuchte andere Druckcheck-Regeln (Hohlräume, Ablauflöcher).

### R6 – Baugruppen mit Gelenken in Version 1?
> **Annahme:** Nicht in 1.0 (Backlog B-1). Mehrere Körper, Passungs-Assistent und Print-in-Place-Scharnier gibt es schon.

### R9 – Sprachen
> **Annahme:** Deutsch und Englisch von Anfang an; weitere Sprachen später durch die Community (z. B. über Weblate).

### R10 – Code-Hosting und CI
> **Annahme:** Öffentliches GitHub-Repo mit GitHub Actions (für Open-Source-Projekte kostenlos, inkl. macOS-Runner). Alternative: Codeberg (Forgejo), dort sind macOS-Runner allerdings schwieriger.

### R11 – KI-Funktionen im Programm?
> **Annahme:** Nicht in 1.0 (Backlog B-3). Falls gewünscht, dann optional und abschaltbar.

### R12 – Freiform-Modellieren (Subdivision wie Fusion „Form“)
> **Annahme:** Zurückgestellt (AP-4.10). „Weich verschmelzen“ und „Verformen“ decken die Deko-Fälle zuerst ab.

### R13 – Name
„Fission359“ bleibt Arbeitstitel; Verwechslungsgefahr mit „Fusion“ wird vor dem Release geprüft (AP-6.6).

### R14 – Zeithorizont und Ambition
> **Annahme:** Ambitioniertes Open-Source-Projekt mit dem Ziel einer öffentlichen 1.0. Erster großer Meilenstein ist **M2** (Phasen 0–2).
