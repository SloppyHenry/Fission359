// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission359.Ui
import Fission359.View

// The arrow on the chosen face (AP-2.4, UX §2.2): a click pulls the face out (a flat wall moves,
// the wall of a hole or pin changes its radius, a body with details is stretched). It stands only
// on bodies that have no size grips of their own; a basic shape keeps its measure dot (rule 11).
// It only offers; pulling itself is a step like any other, with a grip to drag.
Item {
    id: root

    property Viewport3D viewport
    /// The one face that is chosen: { body, number } (empty when none is).
    property var face: ({})
    /// The arrow may stand at all (right tool, nothing being dragged).
    property bool active: true

    signal requested()

    readonly property var anchor: face.body !== undefined ? DocumentModel.faceAnchor(face.body, face.number)
                                                           : ({ valid: false })

    // Where the arrow stands and the way it points on the screen.
    readonly property var spot: {
        if (!viewport || !anchor.valid || !anchor.arrow) {
            return null
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        const reach = 26 / Math.max(viewport.pixelsPerMillimetre, 0.001)
        const base = viewport.project(anchor.x, anchor.y, anchor.z)
        const tip = viewport.project(anchor.x + anchor.dx * reach, anchor.y + anchor.dy * reach, anchor.z + anchor.dz * reach)
        return { x: tip.x, y: tip.y, angle: Math.atan2(tip.y - base.y, tip.x - base.x) * 180 / Math.PI }
    }

    objectName: "faceArrow"
    visible: active && spot !== null

    Rectangle {
        id: dot

        x: root.spot ? root.spot.x - width / 2 : 0
        y: root.spot ? root.spot.y - height / 2 : 0
        width: 26
        height: 26
        radius: 13
        color: grip.containsMouse ? Theme.accent : Theme.raise
        border.width: 1.5
        border.color: Theme.accent

        Shape {
            anchors.centerIn: parent
            width: 14
            height: 14
            rotation: root.spot ? root.spot.angle : 0
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: grip.containsMouse ? "white" : Theme.accent
                strokeWidth: 2
                fillColor: "transparent"
                startX: 1
                startY: 7
                PathLine { x: 13; y: 7 }
                PathLine { x: 8; y: 2 }
                PathMove { x: 13; y: 7 }
                PathLine { x: 8; y: 12 }
            }
        }

        MouseArea {
            id: grip

            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.requested()
        }
    }
}
