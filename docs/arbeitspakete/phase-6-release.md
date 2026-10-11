# Phase 6 – Release 1.0

**Ziel:** Stabil, aktualisierbar, gut dokumentiert. Bereit für Menschen, die das Programm nicht von den Entwicklern erklärt bekommen.

**Meilenstein M6:** Öffentliche Version 1.0 (macOS `.dmg`, Linux AppImage + Flathub).

---

### AP-6.1 Automatische Updates
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.7

**Umfang:**
- macOS: Sparkle 2 (EdDSA-signierter Appcast), Kanäle Stabil/Beta
- Linux: AppImage mit eingebetteter Update-Information (zsync) und Update-Hinweis in der App; Flatpak über Flathub
- Update-Hinweis unaufdringlich, mit Änderungsübersicht in Alltagssprache

**Akzeptanzkriterien:**
- [ ] Update von 0.9 auf 1.0 funktioniert auf beiden Plattformen ohne Datenverlust (Einstellungen, Profile, Bausteine)

---

### AP-6.2 Absturzberichte und Diagnose (opt-in)
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-0.3

**Umfang:**
- Crashpad (oder vergleichbar) **nur mit ausdrücklicher Zustimmung**; Empfang über einen selbst gehosteten Open-Source-Dienst (z. B. GlitchTip) oder ein Open-Source-Förderprogramm eines Anbieters
- „Diagnose-Paket erstellen“: Log, Systeminfo, optional das Modell. Der Nutzer sieht vorher, was enthalten ist
- Keine Telemetrie ohne Opt-in; Datenschutzerklärung

---

### AP-6.3 Performance- und Stabilitätshärtung
**Status:** offen · **Größe:** L · **Abhängigkeiten:** Phasen 1–5

**Umfang:**
- Benchmark-Suite mit großen Modellen (500 Features, 5 Mio. Dreiecke, 200 Körper)
- Profiling (Instruments, perf, Tracy), Speicherlecks (ASan/LSan, Leaks), Langzeit-Test (8 h zufällige Bedienung, „Monkey-Test“)
- Fuzzing aller Datei-Lader für 24 h ohne Fund

**Akzeptanzkriterien:**
- [ ] Alle Budgets aus [Architektur §12](../03-architektur.md#12-performance-budgets) eingehalten
- [ ] Monkey-Test 8 h ohne Absturz

---

### AP-6.4 Dokumentation, Website, Beispiele
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-5.4

**Umfang:**
- Kurzes Online-Handbuch (DE/EN), aufgebaut nach Aufgaben („Wie mache ich ein Loch für eine Schraube?“), nicht nach Menüs
- Landingpage mit Download, Screenshots, Videos
- Beispielprojekte zum Herunterladen

---

### AP-6.5 Beta-Programm und Feedback
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-6.1

**Umfang:**
- Geschlossene Beta (30–100 Personen, gemischt: Neulinge, Tinkercad- und Fusion-Nutzer, Linux und macOS)
- Feedback-Knopf in der App (Text + optional Screenshot); Kanal (Forum/Discord/Matrix, siehe Rückfragen)
- Wöchentliche Auswertung und Priorisierung

**Akzeptanzkriterien:**
- [ ] SUS ≥ 80 in der Beta-Umfrage; keine offenen Datenverlust- oder Absturz-Bugs der Schwere 1

---

### AP-6.6 Rechtliches
**Status:** offen · **Größe:** S · **Abhängigkeiten:** –

**Umfang:**
- Projektlizenz GPL-3.0-or-later final bestätigen; Third-Party-Notices im Über-Dialog
- Lizenzpflichten erfüllen: Quellcode-Angebot zu jedem Release (Git-Tag + Quell-Archiv), LGPL-Hinweise für Qt und OCCT
- Beitragsregeln: Developer Certificate of Origin (DCO, `Signed-off-by`) statt CLA
- Namens- und Markenprüfung „Fission3D“ (Verwechslungsgefahr mit „Fusion“ bewerten), Domain
- Impressum/Datenschutz für Website und Update-Server
