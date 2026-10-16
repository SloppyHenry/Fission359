# ADR-0008: Dokumentmodell, Feature-Typen und Kernel-Schnittstelle

- **Status:** angenommen (Product Owner, 2026-10-14: Kernel-Schnittstelle, Texte als Schlüssel, Abbruch-Verständnis)
- **Datum:** 2026-10-14
- **Arbeitspaket:** AP-1.1

## Kontext
AP-1.1 baut das Dokumentmodell ([Architektur §3](../03-architektur.md#3-core-datenmodell)). Beim Einlesen fielen vier
Punkte auf, an denen die Architekturskizze nicht ausreicht oder dem Code im Weg steht:

1. **Kernel-Zugriff.** Features sollen den Kernel nur über die GAL ansprechen (Architektur §2). Die GAL enthielt aber nur
   Datentypen; `makeBox` und `makeCylinder` stehen in `fission_geometry_occt`, das `fission_features` laut
   `cmake/FissionLayers.cmake` nicht linken darf.
2. **Texte.** `describe()` soll einen Rezept-Satz liefern, der Core kennt aber kein `qsTr`; Regel 4 verlangt Deutsch
   **und** Englisch, Regel 5 verbietet rohe Kernel-Fehler in der Oberfläche.
3. **Status.** Die Skizze legt `status` und `outputs` in das Feature. Der Status ist aber abgeleitet (Ergebnis einer
   Neuberechnung, hängt von den benannten Maßen ab) und gehört nicht in die Datei.
4. **Flächennamen.** ADR-0003 verlangt, dass jedes Feature alle erzeugten Flächen benennt. Der Kernel reicht keine
   Namen heraus, und seine Vorstellung von „vorne“ weicht von der unseren ab (OpenCASCADE: vorne = +X).

## Entscheidung
1. **`geo::GeometryKernel`** (reine Schnittstelle in der GAL, `fission/geo/kernel.h`) mit `makeBox`, `makeCylinder`,
   `measure`, `tessellate`. Die App reicht `occt::makeKernel()` hinein, Tests können eine Attrappe nehmen. Features und
   Core sehen keinen Kernel-Typ. Weitere Operationen kommen mit den jeweiligen APs hinzu.
2. **Flächen-Rollen vom Kernel:** Operationen liefern `NamedSolid` (Körper + `NamedFace{faceId, role}`). Rollen
   werden im Kernel **geometrisch** zugeordnet (Normale, Fläche-Art), nicht aus den Namen des Kernels; die
   Implementierung prüft, dass jede Fläche genau eine Rolle hat. Quader: `bottom top front back left right`
   (vorne = −Y, links = −X wie im Ansichtswürfel), Zylinder: `bottom top side`.
3. **Voller Name** = `F<Nummer>/<Typ>/<Rolle>` (`elementName`), die Nummer zählt je Dokument hoch und wird nie
   wiederverwendet (ADR-0003).
4. **Namens-Vertrag wird zur Laufzeit geprüft:** `checkNamingContract` (alle Flächen benannt, keine doppelt,
   keine fehlende). Die Neuberechnung macht daraus einen Fehler am Schritt, ein Typ, der ihn verletzt, liefert kein
   Ergebnis.
5. **Texte als Schlüssel:** `describe()` liefert `Description{key, values}`, Fehler tragen `FeatureIssueCode` und
   `ExpressionErrorCode` samt Stelle im Text. Die Oberfläche macht daraus übersetzte Sätze (Meldungskatalog,
   AP-1.12/2.14). Der Kernel-Fehlertext bleibt im Protokoll (`detail`).
6. **Status gehört zum Ergebnis der Neuberechnung,** nicht zum Feature. `outputs` (Körper-IDs) bleiben am Feature,
   denn die IDs müssen stabil sein.
7. **Das Dokument ist ein Wert:** kopierbar, keine globalen Zustände, IDs aus einer eigenen Quelle je Dokument
   (`IdSource`, mit Startwert für Tests). Die Neuberechnung arbeitet auf einer Kopie; mehrere Dokumente sind
   unabhängig.
8. **Eingänge vor Ausgängen:** Ein Schritt darf nur Körper benutzen, die ein früherer Schritt erzeugt (`addFeature`
   lehnt anderes ab). Der Graph ist dadurch von Konstruktion an kreisfrei; Verweise auf Flächen anderer Schritte
   kommen mit AP-2.10 dazu.
9. **Grundformen über eine Beschreibung (AP-1.5):** Außer Quader und Zylinder (eigene Aufrufe) baut der Kernel alle
   Grundformen über `makePrimitive(PrimitiveSpec, base)`. `PrimitiveSpec` ist ein `std::variant` aus kleinen
   Beschreibungen (`SphereSpec`, `ConeSpec`, …) in `fission/geo/primitives.h`. Eine neue Form ist eine Beschreibung plus
   ein Feature-Typ; die Schnittstelle bekommt keine zehn neuen Aufrufe, und die Zähl-Hüllen der Tests brauchen genau
   eine Weiterleitung. Alle Formen stehen auf der Ebene z und sind in der Draufsicht um (x, y) zentriert; `makeBox`
   bekommt den Eckenradius als Parameter.
10. **Ausdrücke:** Funktionsargumente trennt `;` (oder Komma mit Leerzeichen), weil das Komma Dezimaltrenner ist.
   Rechnen mit Dimensionen (Länge, Winkel, Potenzen davon), damit `sqrt(a*a + b*b)` geht; ein Feld nimmt am Ende nur
   Länge, Winkel oder Zahl. Eine Zahl ohne Einheit ist in einem Längenfeld die Standardeinheit des Dokuments.

## Konsequenzen
- **Einfacher:** neue Werkzeuge sind ein neuer Typ plus Test; Core und Neuberechnung ändern sich nicht. Der Kernel lässt
  sich in Tests ersetzen. Fehler zeigen auf die Stelle im Text.
- **Schwerer:** Jede neue Kernel-Operation muss in `GeometryKernel` aufgenommen und in der OCCT-Umsetzung benannt
  werden. Die Schnittstelle wächst mit den APs.
- **Folge-Arbeiten:** Historie (`Modified/Generated/IsDeleted`) als Teil des Vertrags mit AP-1.7/2.10; `PersistentRef`
  wird dort aufgelöst. Speichern der Felder mit AP-1.10 (Roundtrip- und Migrationstest).
- **Revisionskriterium:** Zwingt eine Kernel-Operation (z. B. mit Fortschrittsanzeige zum Abbrechen) zu einer
  Schnittstelle, die kein anderer Kernel erfüllen kann, wird die Trennung neu bewertet.

## Belege
- Tests: `tests/unit/core/test_document.cpp`, `tests/unit/features/test_primitives.cpp` (Flächen-Rollen gegen die
  Normalen des Netzes geprüft).
- Fund beim Schreiben der Tests: OpenCASCADE nennt die Flächen des Quaders anders als unser Konzept
  (Front/Left vertauscht gegenüber dem Ansichtswürfel); deshalb ordnet der Kernel geometrisch zu.
