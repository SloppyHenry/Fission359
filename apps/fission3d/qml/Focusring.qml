// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The visible keyboard focus: a 2 px ring in the accent line colour, 2 px away from the control.
Rectangle {
    property real ringRadius: 4

    anchors.fill: parent
    anchors.margins: -4
    radius: ringRadius + 2
    color: "transparent"
    border.width: 2
    border.color: Theme.accentLine
}
