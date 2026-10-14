// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui
import Fission3D.View

// The size labels at the edges of the print bed. They float in the 3D view and are shown while
// the bed is small on the screen; zoomed in they would only be in the way.
Item {
    id: root

    property Viewport3D viewport
    property string printerText: ""
    readonly property real halfX: viewport ? viewport.bedSizeX / 2 : 0
    readonly property real halfY: viewport ? viewport.bedSizeY / 2 : 0
    readonly property bool shown: viewport && viewport.bedVisible && viewport.pixelsPerMillimetre < 3.2

    // Positions in the view; read the camera properties so that they follow it.
    function place(x, y) {
        const yaw = viewport.cameraYaw
        const pitch = viewport.cameraPitch
        const scale = viewport.pixelsPerMillimetre
        return viewport.project(x, y, 0) // depends on the values read above
    }

    visible: shown

    Txt {
        readonly property point p: root.visible ? root.place(0, -root.halfY) : Qt.point(0, 0)

        x: p.x - width / 2
        y: p.y + 24 - height / 2
        text: qsTr("%1 mm").arg(root.viewport ? root.viewport.bedSizeX : 0)
        color: Theme.fg3
        mono: true
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideNone
    }

    Txt {
        readonly property point p: root.visible ? root.place(root.halfX, 0) : Qt.point(0, 0)

        x: p.x + 10
        y: p.y + 20 - height / 2
        text: qsTr("%1 mm").arg(root.viewport ? root.viewport.bedSizeY : 0)
        color: Theme.fg3
        mono: true
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideNone
    }

    Txt {
        readonly property point p: root.visible ? root.place(-root.halfX, -root.halfY) : Qt.point(0, 0)

        x: p.x - 6 - width
        y: p.y + 22 - height / 2
        text: root.printerText
        color: Theme.fg3
        mono: true
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideNone
    }
}
