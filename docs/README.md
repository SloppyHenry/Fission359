# Fission3D – Projektdokumentation

> **Arbeitstitel:** Fission3D
> **Kurz:** Ein installierbares 3D-Konstruktionsprogramm für macOS und Linux. Es soll sich so leicht anfühlen wie Knete und so genau sein wie ein Messschieber.

## Dokumente

| Datei | Inhalt | Für wen |
|---|---|---|
| [01-vision.md](01-vision.md) | Warum, für wen, was es kann (und was bewusst nicht), Design-Prinzipien, Erfolgskriterien | alle |
| [02-ux-konzept.md](02-ux-konzept.md) | Bedienkonzept: Bildschirmaufbau, Interaktion, Rezept, Fehler, Lernen, Glossar, Beispielabläufe | Produkt, UI-Entwicklung |
| [03-architektur.md](03-architektur.md) | Tech-Stack, Schichten, Datenmodell, Präzision, Dateiformat, Projektstruktur | Entwicklung |
| [04-roadmap.md](04-roadmap.md) | Phasen, Meilensteine, Abhängigkeiten, Definition of Done | alle |
| [arbeitspakete/](arbeitspakete/) | Alle Arbeitspakete (AP) je Phase mit Akzeptanzkriterien | Entwicklung |
| [05-master-prompt.md](05-master-prompt.md) | **Der Prompt** für den KI-Coding-Agenten (z. B. Claude Code), mit Kurz-Prompts je Arbeitspaket | Agent |
| [07-baukaesten.md](07-baukaesten.md) | Baukästen: geführtes Gestalten von Karten, Vasen, Figuren u. v. m. (statt starrer Vorlagen), Motiv-Bibliothek | Produkt, Entwicklung |
| [06-rueckfragen.md](06-rueckfragen.md) | Offene Entscheidungen, jeweils mit der aktuell angenommenen Antwort | du |
| [adr/](adr/) | Architecture Decision Records (Entscheidungsprotokolle) | Entwicklung |
| [spikes/](spikes/) | Berichte der Untersuchungs-Arbeitspakete (Messwerte, Empfehlungen) | Entwicklung |
| [mockups/](mockups/) | Klickbares Oberflächen-Mockup, Gestaltungsvorlage (R17) | du, UI-Entwicklung |
| [prompts/](prompts/) | Fertige Auftragstexte für einzelne Sitzungen (z. B. Oberflächen-Mockup) | du, Agent |

## So geht's los

1. [06-rueckfragen.md](06-rueckfragen.md) durchgehen und die Annahmen bestätigen oder korrigieren.
2. Den Master-Prompt aus [05-master-prompt.md](05-master-prompt.md) als Projektanweisung für den Agenten verwenden, z. B. als `CLAUDE.md` im Repo-Root oder als ersten Prompt.
3. Die Arbeitspakete in der Reihenfolge der [Roadmap](04-roadmap.md) abarbeiten, beginnend mit **AP-0.1**.
