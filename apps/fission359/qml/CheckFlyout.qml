// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The tools of the group "Check", next to its button in the palette (AP-2.11): Measure.
Float {
    id: root

    property bool measuring: false
    signal measureToggled

    width: row.implicitWidth + 16
    height: row.implicitHeight + 16
    z: 5

    Row {
        id: row

        x: 8
        y: 8
        spacing: 4

        ToolButton3D {
            width: 96
            text: qsTr("Measure")
            iconName: "check"
            active: root.measuring
            tipTitle: qsTr("Measure")
            tipText: qsTr("Click faces, edges or corners to see their length, area, radius, distance or angle. The number can be copied.")
            onClicked: root.measureToggled()
        }
    }
}
