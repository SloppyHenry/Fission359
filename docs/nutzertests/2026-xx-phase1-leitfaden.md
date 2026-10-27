# Nutzertest 1 – Leitfaden für den Testleiter (Phase 1)

Für den Product Owner. Fünf Sitzungen à 20–30 Minuten mit Menschen **ohne CAD-Erfahrung** (davon zwei, die Tinkercad kennen).
Es wird das Programm getestet, nicht die Person.

## Vorher (einmal)
1. Das Testpaket aus der CI (`.dmg` bzw. AppImage des aktuellen `main`) auf einem **frischen Benutzerkonto** starten. Der Erststart-Assistent
   (Sprache, Drucker, Maus) muss erscheinen; falls nicht, Einstellungsordner löschen.
2. Bildschirmaufnahme einrichten: macOS ⇧⌘5 (mit Ton), Linux OBS. Testaufnahme von 10 Sekunden prüfen.
3. Drei Dinge bereitlegen: die Aufgabenkarte (unten), den SUS-Bogen (unten) und den Beobachtungsbogen (unten).
4. Aufnahmen und Bögen kommen in einen Ordner **außerhalb des Repositories** (Datenschutz). Im Bericht heißen die Personen P1 bis P5.

## Begrüßung (vorlesen)
„Danke, dass du dir Zeit nimmst. Wir testen das Programm, nicht dich: Du kannst nichts falsch machen. Wenn etwas nicht klappt,
liegt es am Programm. Bitte sag laut, was du denkst und suchst, auch wenn es dir banal vorkommt. Ich kann dir während der Aufgabe
nicht helfen, ich schaue nur zu und schreibe mit. Darf ich den Bildschirm und deine Stimme aufnehmen? Die Aufnahme sieht nur unser
Team und wird nach der Auswertung gelöscht.“ (Einverständnis ja/nein notieren; bei „nein“ ohne Aufnahme.)

## Aufgabenkarte (der Person hinlegen, nichts weiter erklären)
> **Baue einen Schlüsselanhänger mit deinem Namen**, ungefähr 5 × 2 cm, mit einem Loch für den Ring.
> **Speichere ihn als Datei für den Drucker.**

Keine Hinweise auf Werkzeugnamen, keine Tipps zum Aufbau. Zeit ab dem ersten Klick messen, Stopp bei „Fertig“ oder nach 10 Minuten.
Geschafft heißt: Schlüsselanhänger mit lesbarem Namen und Loch, als 3MF oder STL exportiert.

## Was der Testleiter nicht tun darf
- Nicht helfen, nicht zeigen, nicht „Versuch mal …“ sagen, nicht auf Menüs oder Knöpfe deuten.
- Nicht erklären, was ein Wort im Programm bedeutet.
- **Ausnahme:** nach **3 Minuten völligem Stillstand** darf ein Hinweis kommen („Schau mal in die Werkzeugleiste.“). Dann sofort
  notieren (Zeit, Stelle, was gesagt wurde), und die Person gilt als „mit Hilfe“.
- Keine Fehler beheben, keine neue Programmversion in der Testwoche (die Ergebnisse müssen vergleichbar bleiben).

## Beobachtungsbogen (eine Zeile je Auffälligkeit)
| Zeit | Stelle im Programm | Was die Person sagt oder tut | Schwere |
|---|---|---|---|
| mm:ss | z. B. Formen-Katalog | „Wo ist hier Text?“ | 1 blockiert · 2 kostet > 1 min · 3 Ärger |

Besonders beachten (ADR-0010): **Sucht die Person die Bewegung eines Körpers im Rezept** statt im Bild? Wenn ja, ankreuzen und
die Stelle notieren.

## Nach der Aufgabe: SUS-Fragebogen (deutsche Standardfassung)
Jede Aussage von 1 („stimme gar nicht zu“) bis 5 („stimme voll zu“) bewerten lassen:
1. Ich denke, dass ich dieses Programm gerne häufig benutzen würde.
2. Ich fand das Programm unnötig komplex.
3. Ich fand das Programm einfach zu benutzen.
4. Ich glaube, ich würde die Hilfe einer technisch versierten Person benötigen, um das Programm benutzen zu können.
5. Ich fand, die verschiedenen Funktionen in diesem Programm waren gut integriert.
6. Ich denke, das Programm enthielt zu viele Inkonsistenzen.
7. Ich kann mir vorstellen, dass die meisten Menschen den Umgang mit diesem Programm sehr schnell lernen.
8. Ich fand das Programm sehr umständlich zu nutzen.
9. Ich fühlte mich bei der Benutzung des Programms sehr sicher.
10. Ich musste eine Menge Dinge lernen, bevor ich mit dem Programm arbeiten konnte.

Auswertung: ungerade Fragen Wert − 1, gerade Fragen 5 − Wert, Summe × 2,5 (0–100; ab 68 gilt als gut).

Zum Schluss zwei offene Fragen: „Was war am schwersten?“ und „Was hat dir gefallen?“
