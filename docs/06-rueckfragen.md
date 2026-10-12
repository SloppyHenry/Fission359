# Rückfragen und offene Entscheidungen

## Beantwortet

| # | Frage | Antwort | Auswirkung in den Dokumenten |
|---|---|---|---|
| R1 | Wer entwickelt? | Der KI-Agent programmiert; der Product Owner kann etwas Python | Master-Prompt: Abschnitt „Zusammenarbeit“ (Deutsch, Alltagssprache, Nachweise statt Code-Reviews). AP-0.1: Ein-Befehl-Skripte `scripts/setup`, `run`, `test`. Backlog B-2: Skript-API in **Python** |
| R2 | Open Source oder kommerziell? | **Open Source** | Vorschlag **GPL-3.0-or-later** (Architektur §1 und §8). Abhängigkeiten müssen GPL-3-kompatibel sein. AP-0.1: LICENSE, CONTRIBUTING, Code of Conduct, Vorlagen. AP-6.6: DCO statt CLA |
| R4 | Fertigung | FDM-3D-Druck | wie angenommen |
| R5 | Drucker | Bambu Lab und Prusa, inkl. Mehrfarbdruck | wie angenommen (AP-3.1, AP-3.4) |
| R15 | Was stört an Tinkercad? | 1) umständliches Ausschneiden, 2) Rundungen verziehen sich beim ungleichmäßigen Größe-Ändern, 3) niedrige Auflösung, 4) rudimentäre Funktionen | **UX §7.1–7.4** mit verbindlichen Antworten; neues **Design-Prinzip 12**; Master-Prompt-Regeln 11–13; **Ablauf D** als automatisierter Regressionstest; konkrete Kriterien in AP-1.3, 1.5, 1.6, 1.7, 2.4, 4.2, 4.3 |
| R2b | Projektlizenz | **GPL-3.0-or-later** bestätigt | Architektur §1/§8, Master-Prompt |
| R7 | Apple-Developer-Konto | Xcode installiert; nur **kostenloses** Konto | AP-0.7: zuerst ad-hoc-Signatur plus Gatekeeper-Anleitung; Notarisierung vorbereitet, aktiv erst mit bezahltem Konto. Master-Prompt: Abschnitt „Testumgebung“ |
| R8 | Linux-Test | Eigener Linux-Rechner per **SSH mit Root-Rechten** | Master-Prompt: Abschnitt „Testumgebung“. Distribution und GPU ermittelt der Agent beim ersten Zugriff und hält sie in `docs/testumgebung.md` fest |

---

## Noch offen

Jede Frage hat eine **aktuelle Annahme**, auf der die Dokumente beruhen.

### R3 – Native UI wirklich Pflicht?
> **Annahme:** Ja. Komplett nativ mit C++/Qt, keine Web-Technik-UI (Tauri/Electron).

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
„Fission3D“ bleibt Arbeitstitel; Verwechslungsgefahr mit „Fusion“ wird vor dem Release geprüft (AP-6.6).

### R14 – Zeithorizont und Ambition
> **Annahme:** Ambitioniertes Open-Source-Projekt mit dem Ziel einer öffentlichen 1.0. Erster großer Meilenstein ist **M2** (Phasen 0–2).

### R16 – Winkeltoleranz beim Export (aus AP-0.5)
> **Annahme:** 5° statt der in Architektur § 6 genannten 0,5°, bei unveränderter Sehnenabweichung von 0,01 mm.
> Begründung: 0,5° erzeugt bei kleinen Rundungen etwa zehnmal so viele Dreiecke und ist 15- bis 100-mal
> langsamer, ohne dass die Maßhaltigkeit besser wird (siehe [kernel-robustheit.md](spikes/kernel-robustheit.md)).
> Bis zu deiner Bestätigung bleibt die Architektur-Seite unverändert.
