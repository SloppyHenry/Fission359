# AP-6.2 Absturzberichte und Diagnose (opt-in)

**Größe:** M · **Phasendatei:** AP-6.2

## Ziel
Abstürze lassen sich melden – **nur mit ausdrücklicher Zustimmung**. Ein „Diagnose-Paket erstellen“ sammelt Protokoll, Systeminfo und optional das
Modell; der Nutzer sieht vorher genau, was enthalten ist. Keine Telemetrie ohne Opt-in; Datenschutzerklärung.

## Voraussetzungen
AP-0.3 (CI), AP-6.6 (Datenschutz-Entwurf).

## Vorher lesen
`docs/03-architektur.md` §8 (Crashpad opt-in, Apache-2.0); `src/services/log.*` (Protokoll gibt es).

## Haltepunkte
1. **Empfang:** selbst gehosteter Dienst (z. B. GlitchTip, Open Source) oder Förderprogramm eines Anbieters? Kosten und Betrieb entscheidet der Product
   Owner. Ohne Empfang: nur lokales Diagnose-Paket (Datei), das man per Mail/Issue anhängt. Empfehlung: zuerst nur das lokale Paket.
2. **Crashpad** als neue Abhängigkeit (ADR) erst nach Haltepunkt 1.

## Schritte
1. Diagnose-Paket (ohne neue Abhängigkeit): ZIP mit Protokolldateien, Systeminfo (OS, GPU-Backend, Qt, OCCT, App-Version – gibt es im Über-Fenster),
   optional aktuelles Modell (Häkchen, standardmäßig aus); Vorschau-Liste der Dateien vor dem Speichern; Menü Hilfe ▸ „Diagnose-Paket erstellen…“.
2. Absturz-Erkennung beim nächsten Start (Marker aus AP-1.10 Autosave) → Karte „Fission3D wurde unerwartet beendet. [Diagnose-Paket erstellen] [Nein danke]“.
3. Nach Haltepunkt 1/2: Crashpad-Handler (nur aktiv, wenn eingewilligt), Minidumps, Upload an den gewählten Dienst; Einwilligung jederzeit widerrufbar.
4. Datenschutzerklärung (AP-6.6) verlinkt.

## Tests
Paket enthält genau die angekündigten Dateien; ohne Häkchen kein Modell; keine Netzwerkzugriffe ohne Einwilligung (Test mit Netz-Stub).

## Nicht tun
Keine Telemetrie, keine Nutzungsstatistik, nichts im Hintergrund senden.
