// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The number of the measure beside its line in the 3D view (the line itself is drawn by the
// view). Follows the camera.
Item {
    id: root

    /// The main number as text, from the card.
    property string text: ""
    readonly property var found: Measure.result

    Binding {
        target: Measure
        property: "lineColour"
        value: Theme.fg2
    }

    Rectangle {
        visible: Measure.enabled && root.text !== "" && root.found.labelX !== undefined
        x: (root.found.labelX !== undefined ? root.found.labelX : 0) - width / 2
        y: (root.found.labelY !== undefined ? root.found.labelY : 0) - height - 6
        width: label.implicitWidth + 16
        height: label.implicitHeight + 8
        radius: 6
        color: Theme.panel
        border.width: 1
        border.color: Theme.line

        Txt {
            id: label

            anchors.centerIn: parent
            text: root.text
            color: Theme.fg
            mono: true
        }
    }
}
