# Phase 3 – Druckbereit

**Ziel:** Fission3D denkt den 3D-Druck mit: Es warnt vor Problemen, *bevor* gedruckt wird, hilft bei Passungen und macht Mehrfarbdruck einfach.

**Meilenstein M3 (Demo):** Eine **Box mit Steckdeckel** (über Passungs-Assistent), zweifarbig, Druckcheck grün, „Im Slicer öffnen“. Gedruckt passt sie beim ersten Versuch.

---

### AP-3.1 Druckerprofile
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-1.11
**Bauanleitung:** [anleitungen/AP-3.1.md](anleitungen/AP-3.1.md)

**Umfang:**
- Profilformat (JSON): Bauraum, Düse, Schichthöhe-Standard, Mindestwand, Lochkorrektur, Standard-Spiel, Mehrfarb-Fähigkeit (Anzahl Farben), bevorzugter Slicer
- Mitgelieferte Profile gängiger Drucker, eigenes Profil anlegen und duplizieren
- Profilwechsel aktualisiert Druckbett und Prüfregeln sofort

**Akzeptanzkriterien:**
- [ ] ≥ 12 mitgelieferte Profile; Profil-Schema mit Validierung

---

### AP-3.2 Druckcheck
**Status:** offen · **Größe:** L · **Abhängigkeiten:** AP-3.1, AP-2.11
**Bauanleitung:** [anleitungen/AP-3.2.md](anleitungen/AP-3.2.md)

**Umfang:**
- Prüfungen: geschlossen/wasserdicht (manifold), sich selbst schneidende Netze, **Mindestwandstärke** (Strahl-basiert, Heatmap), **Überhänge** (> 45°/Profilwert, farbig), **zu kleine Details** (< Düsendurchmesser), **passt aufs Bett**, schwebende Teile, sehr dünne Spitzen
- Darstellung: Ampel-Zusammenfassung + Liste + Heatmap-Overlay in der Ansicht; „Stelle zeigen“ springt hin
- Lösungsvorschläge, wo möglich (Aushöhlen-Stärke erhöhen, Fase statt Überhang, drehen)
- Läuft im Hintergrund und aktualisiert sich nach Änderungen (abschaltbar)

**Akzeptanzkriterien:**
- [ ] Testkatalog mit 20 bekannten Problemteilen wird korrekt bewertet
- [ ] Analyse eines Modells mit 500k Dreiecken in < 2 s

---

### AP-3.3 Passungs-Assistent
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-2.4
**Bauanleitung:** [anleitungen/AP-3.3.md](anleitungen/AP-3.3.md)

**Umfang:**
- Zwei Flächen bzw. Körper wählen → Passungsart: **locker** (Schiebedeckel), **Steckpassung** (hält, lässt sich lösen), **Presspassung**, **Drehpassung** (Achse); Spiel-Werte aus dem Druckerprofil
- Umsetzung über Flächen-Versatz (AP-2.4) als Rezept-Eintrag „Spiel 0,2 mm für Deckel“
- **Kalibrier-Hilfe:** erzeugt ein kleines Testteil mit Stufen (0,1/0,15/0,2/0,25/0,3 mm). Der Nutzer trägt den passenden Wert ins Profil ein

**Akzeptanzkriterien:**
- [ ] Box-mit-Deckel-Vorlage passt gedruckt auf 2 Referenzdruckern

---

### AP-3.4 Farben und Mehrfarbdruck
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.10
**Bauanleitung:** [anleitungen/AP-3.4.md](anleitungen/AP-3.4.md)

**Umfang:**
- Farbe je Körper (Phase 1) **und je Fläche** (Flächen einfärben, z. B. Gravur farbig)
- Filament-Palette passend zum Profil (Anzahl Farben, z. B. AMS mit 4); Farben als Filamente benennen („Rot PLA“)
- **Farbwechsel nach Höhe** für Drucker mit einer Düse: Farbe ab Höhe X mm; Übergabe an den Slicer (z. B. PrusaSlicer-Farbwechsel-Metadaten im 3MF, Bambu/Orca-Projektdaten) oder als Anleitung mit Höhenangaben. Grundlage für Karten und Schilder (AP-5.10)
- 3MF-Export mit Objekten und Farben so, dass **Bambu Studio, OrcaSlicer und PrusaSlicer** die Zuordnung übernehmen (Hersteller-Erweiterungen prüfen, z. B. Bambu-Projekt-Metadaten)

**Akzeptanzkriterien:**
- [ ] Zweifarbiger Schlüsselanhänger öffnet sich in allen 3 Slicern mit korrekter Farbzuordnung, ohne manuelles Nacharbeiten (oder mit dokumentierter Einschränkung je Slicer)

---

### AP-3.5 „Im Slicer öffnen“
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-3.4
**Bauanleitung:** [anleitungen/AP-3.5.md](anleitungen/AP-3.5.md)

**Umfang:**
- Installierte Slicer erkennen (macOS: `/Applications`, Bundle-IDs; Linux: Flatpak-IDs, AppImages in Standardorten, PATH)
- Export in temporäres 3MF und Übergabe an den Slicer; bevorzugter Slicer im Profil
- Fehlt der Slicer: freundlicher Hinweis mit Download-Link

**Akzeptanzkriterien:**
- [ ] Funktioniert mit Bambu Studio, OrcaSlicer, PrusaSlicer, Cura auf macOS und Linux (Flatpak und AppImage)

---

### AP-3.6 Druckausrichtung vorschlagen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-3.2
**Bauanleitung:** [anleitungen/AP-3.6.md](anleitungen/AP-3.6.md)

**Umfang:**
- Kandidaten-Ausrichtungen (große ebene Flächen nach unten, Hauptachsen) bewerten nach Überhang-Fläche, Kontaktfläche, Höhe und optional **Festigkeit** (Lastrichtung vs. Schichtrichtung, einfache Heuristik mit Erklärung)
- Vorschläge als Kärtchen mit Mini-Vorschau; Übernehmen = Transformation im Rezept

**Akzeptanzkriterien:**
- [ ] Bei der Wandhalterung (Ablauf B) wird die festigkeitsgünstige Lage vorgeschlagen und begründet

---

### AP-3.7 Große Modelle teilen
**Status:** offen · **Größe:** M · **Abhängigkeiten:** AP-1.7, AP-3.3
**Bauanleitung:** [anleitungen/AP-3.7.md](anleitungen/AP-3.7.md)

**Umfang:**
- Körper an Ebene(n) teilen, damit er aufs Bett passt; automatischer Vorschlag bei „passt nicht aufs Bett“
- Verbinder automatisch hinzufügen: **Zapfen** (mit Spiel), **Schwalbenschwanz**, **Stiftlöcher** für Filament-Stücke oder Magnete

**Akzeptanzkriterien:**
- [ ] Ein 400 mm langes Teil wird für ein 256-mm-Bett in 2 Teile mit Zapfen geteilt und ist im Rezept änderbar

---

### AP-3.8 Exportqualität und Vorschau
**Status:** offen · **Größe:** S · **Abhängigkeiten:** AP-1.10
**Bauanleitung:** [anleitungen/AP-3.8.md](anleitungen/AP-3.8.md)

**Umfang:**
- Export-Dialog: Qualität (Grob/Normal/Fein/Benutzerdefiniert mit Sehnenfehler und Winkel), Anzeige von Dreiecksanzahl und Dateigröße, Vorschau-Modus mit Netz-Overlay
- Einheiten-Hinweis bei STL (immer mm)

**Akzeptanzkriterien:**
- [ ] Zylinder Ø 5 mm hat in „Normal“ sichtbar runde Löcher im Druck (Sehnenfehler ≤ 0,01 mm)
