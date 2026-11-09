// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// The drawing tools of the sketch mode (AP-2.1c1): the name on the button, the sentence of its
// tooltip card, what the hint line says, and the words the command search finds it by (German and
// English at once). Rule 10 of the master prompt: every tool brings these along. `key` is the key
// that chooses the tool while a sketch is open (AP-2.1c2 added arc A, slot S and polygon G; AP-2.1d the curve K, AP-2.1e1 Trim T and Extend E, AP-2.1e2 Offset O and Round corner F, AP-2.1e3 Mirror M and Pattern W (Row or Circle); Round corner has the way "Chamfer"; the entry "chamfer" is only a second name for it in the command search and is not listed in the tool palette). `variants` are the ways to draw it (the first is
// the standard); "helper" is a switch, not a tool.
QtObject {
    readonly property var tools: [
        { id: "line", name: qsTr("Line"), icon: "sketchline", key: "L",
          tip: qsTr("Draws straight lines. Every click adds the next line of a chain; a double click or Esc ends the chain. Type a length while you draw. Pull on the end of the last line to make an arc that joins it."),
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
        { id: "arc", name: qsTr("Arc"), icon: "sketcharc", key: "A",
          tip: qsTr("Draws a piece of a circle: by its start, its end and a point on it, or by its middle, its start and its end. Type the radius or the angle while you draw."),
          hint: qsTr("Click where the arc starts."),
          synonyms: { de: ["bogen", "kreisbogen", "rundung zeichnen", "rundung", "viertelkreis"],
                      en: ["arc", "curve segment", "circular arc", "bow", "quarter circle"] },
          variants: [qsTr("Three points"), qsTr("Middle, start and end")] },
        { id: "slot", name: qsTr("Slot"), icon: "sketchslot", key: "S",
          tip: qsTr("Draws a slot with round ends: click both middles, then pull the width. Type the length, the angle and the width while you draw."),
          hint: qsTr("Click the middle of one end of the slot."),
          synonyms: { de: ["langloch", "schlitz", "nut", "langes loch", "oval"],
                      en: ["slot", "oblong", "obround", "slotted hole", "stadium"] },
          variants: [] },
        { id: "polygon", name: qsTr("Polygon"), icon: "sketchpolygon", key: "G",
          tip: qsTr("Draws a regular polygon from its middle and a corner, or from its middle and the middle of a side. Tab switches to the number of sides (3 to 64) and the angle."),
          hint: qsTr("Click the middle of the polygon."),
          synonyms: { de: ["polygon", "vieleck", "sechseck", "mehreck", "fünfeck"],
                      en: ["polygon", "hexagon", "n-gon", "regular polygon", "pentagon"] },
          variants: [qsTr("Corner (outer circle)"), qsTr("Side middle (inner circle)")] },
        { id: "spline", name: qsTr("Curve"), icon: "sketchspline", key: "K",
          tip: qsTr("Draws a smooth curve that goes through the points you click. A double click or Enter sets it, Backspace takes the last point back. Click the first point again to close the curve."),
          hint: qsTr("Click where the curve starts."),
          synonyms: { de: ["kurve", "spline", "freiform", "schwung", "geschwungene linie", "schlangenlinie"],
                      en: ["curve", "spline", "freeform", "smooth curve", "wavy line", "bezier"] },
          variants: [] },
        { id: "trim", name: qsTr("Trim"), icon: "sketchtrim", key: "T",
          tip: qsTr("Takes away the piece of a line, circle or arc between two crossings. Click a piece, or drag across several to take them all in one step."),
          hint: qsTr("Click the piece to take away."),
          synonyms: { de: ["trimmen", "abschneiden", "kürzen", "überstand weg", "stück entfernen"],
                      en: ["trim", "cut back", "remove segment", "cut off", "clean up"] },
          variants: [] },
        { id: "extend", name: qsTr("Extend"), icon: "sketchextend", key: "E",
          tip: qsTr("Makes a line or an arc longer, up to the next line, circle or curve in its way. Click near the end you want to extend."),
          hint: qsTr("Click near the end to extend."),
          synonyms: { de: ["verlängern", "verlängern bis", "strecken bis", "anschließen", "weiterführen"],
                      en: ["extend", "lengthen", "extend to", "stretch to", "reach"] },
          variants: [] },
        { id: "offset", name: qsTr("Offset"), icon: "sketchoffset", key: "O",
          tip: qsTr("Makes a parallel copy of a line, an arc, a circle or a whole chain of them. Click it, then move to the side and click again, or type the distance."),
          hint: qsTr("Click the line, arc or circle to offset."),
          synonyms: { de: ["versetzen", "parallel", "abstand", "kontur versetzen", "wandstärke"],
                      en: ["offset", "parallel copy", "outline offset", "wall thickness", "inset"] },
          variants: [] },
        { id: "fillet", name: qsTr("Round corner"), icon: "sketchfillet", key: "F",
          tip: qsTr("Rounds a corner where two lines or arcs meet, with a tangent arc. Click near the corner, or pull to change the radius, or type it. The arrow switches to chamfering."),
          hint: qsTr("Click near the corner to round."),
          synonyms: { de: ["ecke abrunden", "rundung", "radius an ecke", "verrunden", "ecke runden"],
                      en: ["round corner", "fillet", "corner radius", "round off", "round the corner"] },
          variants: [qsTr("Round"), qsTr("Chamfer")] },
        { id: "chamfer", name: qsTr("Chamfer corner"), icon: "sketchchamfer", key: "", listed: false,
          tip: qsTr("Cuts a corner where two lines or arcs meet with a straight line, the same distance from the corner on both sides. Click near the corner, or pull to change the distance, or type it."),
          hint: qsTr("Click near the corner to chamfer."),
          synonyms: { de: ["ecke abschrägen", "fase", "schräge an ecke", "abschrägen", "ecke kappen"],
                      en: ["chamfer corner", "chamfer", "bevel corner", "cut corner", "bevel"] },
          variants: [] },
        { id: "mirror", name: qsTr("Mirror"), icon: "sketchmirror", key: "M",
          tip: qsTr("Makes mirrored copies of lines, arcs, circles, curves and points. Choose what to mirror, then click the line to mirror at. The originals stay."),
          hint: qsTr("Click what you want to mirror."),
          synonyms: { de: ["spiegeln", "symmetrisch", "Spiegelbild", "umklappen", "spiegelachse"],
                      en: ["mirror", "flip", "symmetric", "reflect", "mirror image"] },
          variants: [] },
        { id: "pattern", name: qsTr("Pattern"), icon: "sketchpattern", key: "W",
          tip: qsTr("Repeats what you choose: in a row (a start, a direction and a distance) or round a point. Type the count and the distance or angle. The copies are separate elements."),
          hint: qsTr("Click what you want to repeat."),
          synonyms: { de: ["Reihe", "Kreismuster", "wiederholen", "vervielfältigen", "Raster"],
                      en: ["pattern", "array", "repeat", "circular pattern", "linear pattern"] },
          variants: [qsTr("Row"), qsTr("Circle")] },
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
    function hintFor(id, variant, placed, pullingArc) {
        if (pullingArc) {
            return qsTr("Let go to set the arc that joins the line, or type its radius. Enter sets it, Esc drops it.")
        }
        switch (id) {
        case "line":
            return placed === 0 ? qsTr("Click where the line starts. Esc ends the tool.")
                                : qsTr("Click the next point, or type a length (Tab: the angle). Pulling on the end of the last line makes an arc that joins it. Double-click or Esc ends the chain.")
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
        case "arc":
            if (variant === 1) {
                return placed === 0 ? qsTr("Click the middle of the arc. Esc ends the tool.")
                     : placed === 1 ? qsTr("Click where the arc starts. Esc cancels.")
                                    : qsTr("Move to turn the arc, click where it ends, or type the angle. Enter sets it, Esc cancels.")
            }
            return placed === 0 ? qsTr("Click where the arc starts. Esc ends the tool.")
                 : placed === 1 ? qsTr("Click where the arc ends. Esc cancels.")
                                : qsTr("Click a point on the arc, or type the radius. Enter sets it, Esc cancels.")
        case "slot":
            return placed === 0 ? qsTr("Click the middle of one end of the slot. Esc ends the tool.")
                 : placed === 1 ? qsTr("Click the middle of the other end, or type the length (Tab: the angle). Esc cancels.")
                                : qsTr("Pull the width and click, or type the width. Enter sets it, Esc cancels.")
        case "polygon":
            return placed === 0 ? qsTr("Click the middle of the polygon. Esc ends the tool.")
                                : qsTr("Click a corner, or type the radius (Tab: the number of sides, then the angle). Enter sets it, Esc cancels.")
        case "spline":
            return placed === 0 ? qsTr("Click where the curve starts. Esc ends the tool.")
                 : placed === 1 ? qsTr("Click the next point of the curve, or type a length (Tab: the angle). Esc throws the curve away.")
                                : qsTr("Click the next point. Double-click or Enter sets the curve, Backspace takes the last point back, a click on the first point closes it. Esc throws it away.")
        case "trim":
            return qsTr("Click a piece to take it away, or drag across several pieces. Esc ends the tool.")
        case "extend":
            return qsTr("Click near the end of a line or an arc to extend it up to the next curve. Esc ends the tool.")
        case "offset":
            return placed === 0 ? qsTr("Click a line, arc or circle: its whole chain is offset. Esc ends the tool.")
                                : qsTr("Move to the side and click, or type the distance. Enter sets it, Esc takes the chain back.")
        case "fillet":
        case "chamfer":
            if (id === "chamfer" || variant === 1) {
                return qsTr("Click near a corner to chamfer it with the last distance, pull to change the distance, or type it. Esc ends the tool.")
            }
            return qsTr("Click near a corner to round it with the last radius, pull to change the radius, or type it. Esc ends the tool.")
        case "mirror":
            return placed === 0 ? qsTr("Click what you want to mirror (a click on a chosen chain takes it out again). Esc ends the tool.")
                 : placed === 1 ? qsTr("Click more, then press Enter or Continue. Esc ends the tool.")
                                : qsTr("Click the line to mirror at. Esc goes back to choosing.")
        case "pattern":
            if (placed === 0) {
                return qsTr("Click what you want to repeat (a click on a chosen chain takes it out again). Esc ends the tool.")
            }
            if (placed === 1) {
                return qsTr("Click more, then press Enter or Continue. Esc ends the tool.")
            }
            if (variant === 1) {
                return placed === 2 ? qsTr("Click the middle of the circle. Esc goes back to choosing.")
                                    : qsTr("Type the count (Tab: the angle), then press Enter, or click to set it. Esc goes back to choosing.")
            }
            return placed === 2 ? qsTr("Click where the row starts. Esc goes back to choosing.")
                                : qsTr("Move to set the direction and the distance, or type the count (Tab: the distance). Click or Enter sets it. Esc goes back to choosing.")
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
