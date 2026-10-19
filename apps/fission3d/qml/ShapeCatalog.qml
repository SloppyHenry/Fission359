// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// The basic shapes the gallery offers: the type the document knows, the name on the tile, a
// sentence for the tooltip and the words the command search finds it by (German and English at
// once, as in the mockup: whoever types "ball" finds the sphere in either language).
QtObject {
    readonly property string dragTip: qsTr("Drag onto the bed. Lands on the face under the pointer, also on a wall.")
    readonly property string clickTip: qsTr("A click puts it on a free place of the bed.")

    readonly property var shapes: [
        { type: "box", name: qsTr("Box"),
          synonyms: { de: ["quader", "würfel", "kiste", "block", "klotz", "rechteck"],
                      en: ["cube", "block", "brick", "rectangle", "square"] } },
        { type: "cylinder", name: qsTr("Cylinder"),
          synonyms: { de: ["zylinder", "rolle", "dose", "stab", "säule", "rund"],
                      en: ["can", "rod", "tube", "pillar", "round"] } },
        { type: "sphere", name: qsTr("Sphere"),
          synonyms: { de: ["kugel", "ball", "murmel", "perle"],
                      en: ["ball", "globe", "bead", "marble"] } },
        { type: "hemisphere", name: qsTr("Hemisphere"),
          synonyms: { de: ["halbkugel", "kuppel", "schale"],
                      en: ["half sphere", "dome", "bowl"] } },
        { type: "cone", name: qsTr("Cone"),
          synonyms: { de: ["kegel", "spitze", "trichter", "kegelstumpf"],
                      en: ["spike", "funnel", "truncated cone"] } },
        { type: "torus", name: qsTr("Torus"),
          synonyms: { de: ["torus", "ring", "reifen", "donut"],
                      en: ["ring", "donut", "tyre", "doughnut"] } },
        { type: "tube", name: qsTr("Tube"),
          synonyms: { de: ["röhre", "rohr", "hülse", "schlauch", "buchse"],
                      en: ["pipe", "sleeve", "hollow cylinder", "bushing"] } },
        { type: "prism", name: qsTr("Prism"),
          synonyms: { de: ["prisma", "sechseck", "achteck", "vieleck", "dreieck"],
                      en: ["hexagon", "octagon", "polygon", "triangle"] } },
        { type: "pyramid", name: qsTr("Pyramid"),
          synonyms: { de: ["pyramide", "dach", "spitzdach"],
                      en: ["roof", "tent"] } },
        { type: "wedge", name: qsTr("Wedge"),
          synonyms: { de: ["keil", "rampe", "schräge"],
                      en: ["ramp", "slope", "slant"] } },
        { type: "star", name: qsTr("Star"),
          synonyms: { de: ["stern", "zacken", "weihnachtsstern"],
                      en: ["points", "christmas star"] } },
        { type: "text", name: qsTr("Text"),
          synonyms: { de: ["text", "namen", "name", "schrift", "beschriften", "buchstaben"],
                      en: ["name", "lettering", "letters", "write", "label"] } }
    ]

    // The sentence for the recipe: what a step of this type is called.
    function nameOf(type) {
        for (let i = 0; i < shapes.length; ++i) {
            if (shapes[i].type === type) {
                return shapes[i].name
            }
        }
        return ""
    }
}
