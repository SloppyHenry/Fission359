// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The field for a typed number inside the 3D view (value bubbles, distance labels, the grid
// step): one look, and one way to say what is wrong (rule 2: every number can be typed; rule 5:
// a wrong input is explained, never silently taken). The owner keeps its own keys and calls
// `check()` before it takes the text: a text that is no number or formula is not taken, the
// field stays open and a line under it says why. Typing again takes the line away.
TextInput {
    id: root

    /// "length", "angle" or "plain" (as for `Numbers.evaluate`).
    property string kind: "length"
    /// Smaller letters and a narrower field (labels of measures).
    property bool compact: false
    /// The explanation goes above the field (for a field at the lower edge of the window).
    property bool problemAbove: false
    property bool invalid: false
    property var errorInfo: ({})
    /// The explanation under the field while the text is wrong, else empty.
    readonly property string problem: invalid ? explainer.text(errorInfo, kind) : ""

    width: Math.max(compact ? 36 : 40, contentWidth + 6)
    color: Theme.fg
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentFg
    font.family: Theme.monoFamilies[0]
    font.pixelSize: compact ? Theme.fontSizeSmall : Theme.fontSize
    font.weight: Font.Medium
    inputMethodHints: Qt.ImhFormattedNumbersOnly

    onTextChanged: invalid = false

    /// True if the text may be taken: empty (nothing typed yet) or a number or formula that fits
    /// `kind`. Otherwise the explanation is shown and false is returned.
    function check() {
        if (text.trim() === "" || !isNaN(Numbers.evaluate(text, kind))) {
            invalid = false
            return true
        }
        errorInfo = Numbers.explain(text, kind)
        invalid = true
        return false
    }

    ValueExplanation { id: explainer }

    // The line under the bubble that holds the field.
    Rectangle {
        objectName: "valueInputProblem"
        visible: root.invalid && root.visible
        x: 0
        y: root.problemAbove ? -height - 12 : root.height + 12
        width: Math.min(260, message.implicitWidth + 16)
        height: message.implicitHeight + 10
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: Theme.bad
        z: 20

        Txt {
            id: message

            x: 8
            y: 5
            width: Math.min(244, implicitWidth)
            text: root.problem
            color: Theme.bad
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }
}
