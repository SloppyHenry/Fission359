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
          hint: qsTr("Drag an arrow, a square or the body itself; it snaps to corners and middles. Hold Shift to switch snapping off. Arrow keys move by 1 mm (Shift: 10 mm)."),
          synonyms: { de: ["verschieben", "bewegen", "schieben", "versetzen", "position", "lage"],
                      en: ["move", "shift", "drag", "position", "place", "translate"] } },
        { id: "rotate", name: qsTr("Rotate"), icon: "rotate",
          tip: qsTr("Three rings turn the chosen body about its middle, around the axes of the bed. Round shapes stay round."),
          hint: qsTr("Drag a ring to turn in steps of 15°, or click it and type an angle. You can also drag the body itself."),
          synonyms: { de: ["drehen", "rotieren", "kippen", "winkel", "ausrichten"],
                      en: ["rotate", "turn", "spin", "tilt", "angle"] } },
        { id: "size", name: qsTr("Size"), icon: "scale",
          tip: qsTr("Dots on the faces change one size of the shape, the squares on the corners all of them alike (larger, smaller). Corners and holes stay round."),
          hint: qsTr("Drag a dot to change a size, or a corner square to scale everything evenly; it snaps to corners and middles. Hold Shift to switch snapping off. You can also drag the body itself."),
          synonyms: { de: ["größe", "maße", "breite", "höhe", "tiefe", "strecken", "länger", "skalieren", "vergrößern", "verkleinern", "proportional", "prozent", "gleichmäßig"],
                      en: ["size", "dimensions", "width", "height", "depth", "stretch", "resize", "scale", "enlarge", "shrink", "proportional", "percent", "uniform"] } }
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
