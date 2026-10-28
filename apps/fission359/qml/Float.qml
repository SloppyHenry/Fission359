// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects
import Fission359.Ui

// A panel that floats above the 3D view: panel colour, 1 px frame, soft shadow.
Item {
    id: root

    default property alias content: inner.data
    property real cornerRadius: Theme.radiusFloat

    Rectangle {
        id: plate

        anchors.fill: parent
        radius: root.cornerRadius
        color: Theme.panel
        border.width: 1
        border.color: Theme.line
        visible: false
    }

    MultiEffect {
        anchors.fill: plate
        source: plate
        shadowEnabled: true
        shadowColor: Theme.shadowUi
        shadowBlur: 0.7
        shadowVerticalOffset: 6
        shadowOpacity: 1.0
    }

    Item {
        id: inner

        anchors.fill: parent
    }
}
