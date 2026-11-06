// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui
import Fission359.View

// The place of a problem in the 3D view (AP-2.14, UX §9): a red ring where it is (a rounding that
// is too large sits at its edge) and the short message beside it. The buttons stay in the recipe;
// this is the "where". It lies over the view and cannot be clicked.
Item {
    id: root

    property Viewport3D viewport
    /// `{ x, y, z }` in mm, or null when the problem has no place.
    property var spot: null
    property string title: ""
    property string text: ""

    // Follows the camera: read its values so that the binding is evaluated again.
    readonly property var point: {
        if (!viewport || !spot) {
            return null
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        return viewport.project(spot.x, spot.y, spot.z)
    }

    objectName: "problemPlace"
    visible: point !== null && title !== ""
    enabled: false

    Rectangle {
        x: root.point ? root.point.x - width / 2 : 0
        y: root.point ? root.point.y - height / 2 : 0
        width: 18
        height: 18
        radius: 9
        color: "transparent"
        border.width: 2.5
        border.color: Theme.bad
    }

    Rectangle {
        id: card

        x: root.point ? Math.min(root.point.x + 16, root.width - width - 8) : 0
        y: root.point ? Math.max(8, Math.min(root.point.y - height / 2, root.height - height - 8)) : 0
        width: Math.min(260, root.width - 16)
        height: column.implicitHeight + 16
        radius: Theme.radiusFloat
        color: Theme.raise
        border.width: 1
        border.color: Theme.bad

        Column {
            id: column

            x: 10
            y: 8
            width: parent.width - 20
            spacing: 2

            Txt {
                width: parent.width
                text: root.title
                color: Theme.fg
                font.pixelSize: Theme.fontSizeSmall
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }

            Txt {
                width: parent.width
                text: root.text
                color: Theme.fg2
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
        }
    }
}
