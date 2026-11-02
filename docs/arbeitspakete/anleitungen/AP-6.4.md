# AP-6.4 Dokumentation, Website, Beispiele

**Größe:** M · **Phasendatei:** AP-6.4

## Ziel
Ein kurzes Online-Handbuch (DE/EN), aufgebaut nach Aufgaben („Wie mache ich ein Loch für eine Schraube?“), nicht nach Menüs. Eine Landingpage mit Download,
Bildern und Videos. Beispielprojekte zum Herunterladen.

## Voraussetzungen
AP-5.4 (Startseite, Inspirations-Beispiele), AP-6.6 (Impressum/Datenschutz), AP-5.2 (Animationen als Material).

## Haltepunkte
1. **Werkzeug für die Website:** Empfehlung: statischer Generator mit Markdown (z. B. MkDocs mit Material-Theme, MIT) auf GitHub Pages – kein Programm-Paket,
   nur Werkzeug im Docs-Build. Oder schlichtes, handgeschriebenes HTML. Bestätigen.
2. **Domain** (aus AP-6.6).

## Schritte
1. Gliederung nach Aufgaben (≥ 30 Seiten): Erste Schritte, Formen ablegen, Maße tippen, Löcher, Kombinieren, Skizzen, Abrunden, Bohrungen, Passungen,
   Mehrfarbig, Export/Slicer, Baukästen, Fehler verstehen (je Meldungskatalog-Eintrag eine Erklärung), Tastenkürzel.
2. Bilder und kurze Animationen automatisch aus der App (`--smoke-test`, AP-5.2-Aufnahme), damit sie nach UI-Änderungen neu erzeugt werden können
   (Skript `scripts/tools/make_doc_images.py`).
3. „Hilfe“ in der App verlinkt auf die passende Seite (Werkzeug-Id → URL).
4. Landingpage (DE/EN): was es ist (Satz aus der Vision), Bilder, Download-Knöpfe (macOS, AppImage, Flathub), Gatekeeper-Hinweis falls ohne Notarisierung,
   Lizenz, Quellcode-Link.
5. Beispielprojekte = Inspirations-Beispiele (AP-5.4) als Downloads.

## Abnahme
Seiten DE und EN vollständig; jede Aufgabe von einer Testperson mit der Seite lösbar (Stichprobe 5 Seiten).

## Nicht tun
Kein Handbuch nach Menüstruktur. Keine Tracker/Analytics auf der Website.
