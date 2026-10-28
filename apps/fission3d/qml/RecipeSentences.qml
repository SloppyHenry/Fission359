// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The sentences of the recipe: one short line for every kind of step ("Box 50 × 20 × 4 mm").
// `scripts/tools/check_sentences.py` checks that every kind of step the program registers has one
// here (pflege.md P15).
QtObject {
    id: root

    /// function (style) -> the words for a font style, set by the window.
    property var styleLabel: style => style
    /// function (type) -> the name of a kind of step ("Box"), set by the window.
    property var typeLabel: type => type
    /// function (type) -> true if the shape catalogue knows that kind of step, set by the window.
    property var knownShape: type => true

    /// One sentence for a step of the recipe ("Box 50 × 20 × 4 mm").
    function text(step) {
        const n = value => Numbers.format(value, 2)
        const v = step.values
        if (step.type === "box" && v.length === 3) {
            return qsTr("Box %1 × %2 × %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "cylinder" && v.length === 2) {
            return qsTr("Cylinder, radius %1 mm, height %2 mm").arg(n(v[0])).arg(n(v[1]))
        }
        if (step.type === "sphere" && v.length === 1) {
            return qsTr("Sphere, radius %1 mm").arg(n(v[0]))
        }
        if (step.type === "hemisphere" && v.length === 1) {
            return qsTr("Hemisphere, radius %1 mm").arg(n(v[0]))
        }
        if (step.type === "cone" && v.length === 3) {
            return v[1] > 0
                ? qsTr("Cone, radius %1 mm at the bottom, %2 mm at the top, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
                : qsTr("Cone, radius %1 mm, height %2 mm").arg(n(v[0])).arg(n(v[2]))
        }
        if (step.type === "torus" && v.length === 2) {
            return qsTr("Torus, ring radius %1 mm, tube radius %2 mm").arg(n(v[0])).arg(n(v[1]))
        }
        if (step.type === "tube" && v.length === 3) {
            return qsTr("Tube, radius %1 mm, wall %2 mm, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "prism" && v.length === 3) {
            return qsTr("Prism with %1 sides, radius %2 mm, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "pyramid" && v.length === 3) {
            return qsTr("Pyramid %1 × %2 × %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "wedge" && v.length === 3) {
            return qsTr("Wedge %1 × %2 × %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "star" && v.length === 3) {
            return qsTr("Star with %1 points, radius %2 mm, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "text" && v.length === 2) {
            const style = step.texts.style
            return style !== undefined && style !== "regular"
                ? qsTr("Text “%1” (%2), letters %3 mm, height %4 mm").arg(step.texts.text).arg(root.styleLabel(style)).arg(n(v[0])).arg(n(v[1]))
                : qsTr("Text “%1”, letters %2 mm, height %3 mm").arg(step.texts.text).arg(n(v[0])).arg(n(v[1]))
        }
        if (step.type === "split" && v.length === 2) {
            return qsTr("Split across %1 at %2 mm").arg(["X", "Y", "Z"][Math.round(v[0])]).arg(n(v[1]))
        }
        if (step.type === "place" && v.length === 3) {
            return qsTr("Part placed, moved by %1, %2, %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "combine" && v.length === 1) {
            switch (step.texts.mode) {
            case "subtract": return qsTr("Cut out %n body(ies)", "", v[0])
            case "intersect": return qsTr("Overlap kept with %n body(ies)", "", v[0])
            default: return qsTr("Merged with %n body(ies)", "", v[0])
            }
        }
        // A kind of step this version does not know (a file from a later version).
        if (!root.knownShape(step.type) && ["combine", "split", "place"].indexOf(step.type) < 0) {
            return qsTr("Step %1 (unknown kind)").arg(step.index + 1)
        }
        return root.typeLabel(step.type)
    }
}
