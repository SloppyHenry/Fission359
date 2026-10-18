// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// The four tools at the top of the palette (docs/08-gestaltung.md §5): the name on the button,
// the sentence of its tooltip card, what the hint line says while it is on, and the words the
// command search finds it by (German and English at once, like the shapes). Rule 10 of the
// master prompt: every tool brings these along.
QtObject {
    readonly property var tools: [
        { id: "select", name: qsTr("Select"), icon: "select",
          tip: qsTr("Choose bodies, faces and edges without handles. Dragging draws a frame, also across bodies."),
          hint: qsTr("Shift+click adds or removes. Drag a frame to choose several. Esc chooses nothing."),
          synonyms: { de: ["wählen", "auswählen", "markieren", "rahmen", "anklicken"],
                      en: ["select", "choose", "pick", "frame", "mark"] } },
        { id: "move", name: qsTr("Move"), icon: "move",
          tip: qsTr("Arrows move the chosen body along one direction, squares in a plane, the dot across the bed. You can also drag the body itself."),
          hint: qsTr("Drag an arrow, a square or the body itself. Arrow keys move by 1 mm (Shift: 10 mm)."),
          synonyms: { de: ["verschieben", "bewegen", "schieben", "versetzen", "position", "lage"],
                      en: ["move", "shift", "drag", "position", "place", "translate"] } },
        { id: "rotate", name: qsTr("Rotate"), icon: "rotate",
          tip: qsTr("Rings turn the chosen body. Coming soon."),
          hint: qsTr("Rotating arrives soon. Until then you can move the body or change its size."),
          synonyms: { de: ["drehen", "rotieren", "kippen", "winkel", "ausrichten"],
                      en: ["rotate", "turn", "spin", "tilt", "angle"] } },
        { id: "size", name: qsTr("Size"), icon: "scale",
          tip: qsTr("Dots on the faces change the sizes of the shape. Corners and holes stay round."),
          hint: qsTr("Drag a dot to change a size, or click a label and type. You can also drag the body itself."),
          synonyms: { de: ["größe", "maße", "breite", "höhe", "tiefe", "strecken", "länger"],
                      en: ["size", "dimensions", "width", "height", "depth", "stretch", "resize"] } }
    ]

    function find(id) {
        for (let i = 0; i < tools.length; ++i) {
            if (tools[i].id === id) {
                return tools[i]
            }
        }
        return null
    }

    function nameOf(id) {
        const tool = find(id)
        return tool ? tool.name : ""
    }
}
