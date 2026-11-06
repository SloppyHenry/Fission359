// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission359.Ui
import Fission359.View

// Where a reference used to point while a step is repaired (AP-2.10e): the old face as a dashed
// square of the same size, the old edge as a dashed line, in the accent colour. Only a guide: it
// lies over the view and cannot be clicked.
Item {
    id: root

    property Viewport3D viewport
    /// What `DocumentModel.repairGhost` says (`valid: false` when nothing is repaired).
    property var ghost: ({ valid: false })

    // Follows the camera: read its values so that the binding is evaluated again.
    readonly property var points: {
        if (!viewport || !ghost.valid) {
            return []
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        return ghost.points.map(point => viewport.project(point.x, point.y, point.z))
    }
    readonly property bool isFace: ghost.valid && ghost.kind === "face"

    objectName: "repairGhost"
    visible: ghost.valid && points.length >= 2
    enabled: false // nothing to click on

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Qt.rgba(Theme.accentLine.r, Theme.accentLine.g, Theme.accentLine.b, 0.95)
            strokeWidth: root.isFace ? 2 : 2.5
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            fillColor: root.isFace ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
                                   : "transparent"
            joinStyle: ShapePath.RoundJoin

            PathPolyline {
                path: root.points.length === 4
                      ? [root.points[0], root.points[1], root.points[2], root.points[3], root.points[0]]
                      : root.points.length === 2 ? root.points : []
            }
        }
    }
}
