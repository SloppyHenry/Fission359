# Phase 5 – Lernen, Bausteine und Feinschliff

**Ziel:** Neulinge lernen das Programm im Programm. Wiederkehrende Bauteile gibt es als intelligente Bausteine, Vorlagen geben einen schnellen Start.

**Meilenstein M5 (Demo):** Eine Person ohne CAD-Erfahrung schafft die Missionen 1–3 ohne fremde Hilfe und baut danach eigenständig ein eigenes kleines Projekt.

---

### AP-5.1 Missions-Engine und erste Missionen
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-1.12

**Umfang:**
- Missionen als Datendateien (JSON/YAML): Schritte mit Text, Hervorhebung (UI-Element oder 3D-Element), **Bedingung auf dem echten Dokumentzustand** (z. B. `body.count == 1 && box.width ≈ 50`), Hilfe-Animation, Belohnung
- Fortschritt speichern, jederzeit abbrechen oder fortsetzen; Missionsübersicht mit Abzeichen (dezent)
- Missionen 1–5 aus [UX §11](../02-ux-konzept.md#11-lernen-im-programm)
- Mission bricht nicht, wenn Nutzer abweichen. Bei falschem Weg folgt ein sanfter Hinweis, kein Blockieren

**Akzeptanzkriterien:**
- [ ] Eine neue Mission lässt sich ohne Code-Änderung hinzufügen
- [ ] Automatisierter Test spielt jede Mission per Skript durch

---

### AP-5.2 Tooltip-Animationen und „Zeig mir wie“
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.12

**Umfang:**
- Kurze Loop-Animationen (3–5 s) je Werkzeug, erzeugt per Skript direkt aus der App (Aufnahme-Modus), damit sie bei UI-Änderungen neu generiert werden können
- „Zeig mir wie“-Knopf im Eigenschaften-Panel

**Akzeptanzkriterien:**
- [ ] Alle Werkzeuge der Phasen 1–4 haben eine Animation; Gesamtgröße < 30 MB

---

### AP-5.3 Bausteine-Bibliothek
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-2.9, AP-2.10

**Umfang:**
- Baustein = gespeichertes Teil-Rezept mit freigegebenen Maßen und **Andockpunkten** (z. B. „auf Fläche, Mitte“, „an Kante“)
- Mitgeliefert: Aufhänger-Öse, Schraubenloch-Gruppe, Scharnier (druckbar, Print-in-Place), Schnappverschluss, Kabelclip, Wandhalter-Lasche, Magnettasche, Füße, Griffmulde, Box-Ecken, Gewindeeinsatz, Logo-Platte
- Eigene Bausteine aus einer Auswahl erstellen; als Datei teilen (`.fissionpart`)
- Bibliotheks-Seitenleiste mit Suche und Vorschaubildern; Drag-and-drop mit Andocken

**Akzeptanzkriterien:**
- [ ] Aufhänger-Öse rastet an einer Sternspitze ein (Ablauf C, Schritt 3) und folgt ihr bei Parameteränderungen

---

### AP-5.4 Startbildschirm und Vorlagen-Galerie
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-5.3

**Umfang:**
- „Was möchtest du bauen?“: Deko, Halterung, Box, Ersatzteil, Schmuck, Spielzeug, Leer
- Je Kategorie 3–6 Vorlagen (parametrisch, mit Konfigurator) und passende Missionen
- Zuletzt geöffnet mit großen Vorschaubildern

**Akzeptanzkriterien:**
- [ ] ≥ 25 Vorlagen; jede druckbar und mit Druckcheck grün

---

### AP-5.5 Materialien, Darstellung und Teilen
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-1.3

**Umfang:**
- Darstellungsmaterialien passend zu Filamenten (PLA matt, Silk, PETG glänzend, transparent, Marmor, Holz-PLA)
- Screenshot (freigestellt, hohe Auflösung), Drehteller-Video/GIF für Social Media

---

### AP-5.6 Barrierefreiheit und Tastatur
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.11

**Umfang:**
- Umsetzung von [UX §14](../02-ux-konzept.md#14-barrierefreiheit); Tastenkürzel-Übersicht (`?`) und frei belegbare Kürzel
- Audit mit VoiceOver (macOS) und Orca (Linux) für die Panels

**Akzeptanzkriterien:**
- [ ] Ablauf A ist ohne Maus machbar (außer der freien Positionierung, die per Zahleneingabe geht)

---

### AP-5.7 Versionsverlauf („Zeitreise“) und Varianten
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.10

**Umfang:**
- Automatische Versionen beim Speichern (in der Datei oder daneben, Größe begrenzt), Liste mit Vorschaubildern, „Diese Version wiederherstellen“ / „Als Kopie öffnen“
- Varianten: gleiche Datei, unterschiedliche Maß-Sätze (z. B. „Klein“, „Groß“), umschaltbar

**Akzeptanzkriterien:**
- [ ] Eine Version von vor 10 Speichervorgängen lässt sich wiederherstellen
