// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// The drawing tools of the sketch mode (AP-2.1c1): the name on the button, the sentence of its
// tooltip card, what the hint line says, and the words the command search finds it by (German and
// English at once). Rule 10 of the master prompt: every tool brings these along. `key` is the key
// that chooses the tool while a sketch is open. `variants` are the ways to draw it (the first is
// the standard); "helper" is a switch, not a tool.
QtObject {
    readonly property var tools: [
        { id: "line", name: qsTr("Line"), icon: "sketchline", key: "L",
          tip: qsTr("Draws straight lines. Every click adds the next line of a chain; a double click or Esc ends the chain. Type a length while you draw."),
          hint: qsTr("Click where the line starts."),
          synonyms: { de: ["linie", "strich", "kante zeichnen", "gerade", "strecke"],
                      en: ["line", "segment", "draw edge", "straight line", "stroke"] },
          variants: [] },
        { id: "rectangle", name: qsTr("Rectangle"), icon: "sketchrect", key: "R",
          tip: qsTr("Draws a rectangle from two corners, or from its middle and one corner. Type the width and the height while you draw."),
          hint: qsTr("Click the first corner."),
          synonyms: { de: ["rechteck", "viereck", "kasten", "quadrat", "kästchen"],
                      en: ["rectangle", "box outline", "square", "quad", "rect"] },
          variants: [qsTr("Two corners"), qsTr("Middle and corner")] },
        { id: "circle", name: qsTr("Circle"), icon: "sketchcircle", key: "C",
          tip: qsTr("Draws a circle from its middle and a point on it, or through three points. Type the radius while you draw."),
          hint: qsTr("Click the middle of the circle."),
          synonyms: { de: ["kreis", "rund", "loch-umriss", "rund zeichnen", "ring"],
                      en: ["circle", "round", "hole outline", "disc", "ring"] },
          variants: [qsTr("Middle and radius"), qsTr("Three points")] },
        { id: "point", name: qsTr("Point"), icon: "sketchpoint", key: "P",
          tip: qsTr("Places a point, as a mark to catch on later."),
          hint: qsTr("Click where the point goes."),
          synonyms: { de: ["punkt", "markierung", "marke", "fixpunkt", "stelle"],
                      en: ["point", "mark", "marker", "dot", "spot"] },
          variants: [] },
        { id: "helper", name: qsTr("Helper line"), icon: "sketchhelper", key: "X",
          tip: qsTr("Switch: what you draw next is only an aid, drawn dashed, and never forms an area. Holding Alt while you draw does the same for one shape."),
          hint: qsTr("Helper lines are dashed aids. Press the key again to draw normal lines."),
          synonyms: { de: ["hilfslinie", "konstruktion", "konstruktionslinie", "hilfe", "gestrichelt"],
                      en: ["helper line", "construction", "construction line", "guide", "dashed"] },
          variants: [] }
    ]

    function find(id) {
        for (let i = 0; i < tools.length; ++i) {
            if (tools[i].id === id) {
                return tools[i]
            }
        }
        return null
    }

    // What the hint line says while the tool `id` (in `variant`) is on and `placed` clicks of the
    // element are made.
    function hintFor(id, variant, placed) {
        switch (id) {
        case "line":
            return placed === 0 ? qsTr("Click where the line starts. Esc ends the tool.")
                                : qsTr("Click the next point, or type a length (Tab: the angle). Double-click or Esc ends the chain.")
        case "rectangle":
            if (variant === 1) {
                return placed === 0 ? qsTr("Click the middle of the rectangle. Esc ends the tool.")
                                    : qsTr("Click a corner, or type the width (Tab: the height). Enter sets it, Esc cancels.")
            }
            return placed === 0 ? qsTr("Click the first corner. Esc ends the tool.")
                                : qsTr("Click the opposite corner, or type the width (Tab: the height). Enter sets it, Esc cancels.")
        case "circle":
            if (variant === 1) {
                return placed === 0 ? qsTr("Click the first point on the circle. Esc ends the tool.")
                     : placed === 1 ? qsTr("Click the second point on the circle. Esc cancels.")
                                    : qsTr("Click the third point on the circle. Esc cancels.")
            }
            return placed === 0 ? qsTr("Click the middle of the circle. Esc ends the tool.")
                                : qsTr("Click a point on the circle, or type the radius. Enter sets it, Esc cancels.")
        case "point":
            return qsTr("Click where the point goes. Esc ends the tool.")
        }
        return ""
    }

    function nameOf(id) {
        const tool = find(id)
        return tool ? tool.name : ""
    }
}
