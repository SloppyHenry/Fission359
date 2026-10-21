// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission3D.Ui
import Fission3D.View

// The cutting plane of the chosen split as a translucent sheet with a frame (AP-1.7e). It is
// only a guide: it lies over the view and cannot be grabbed; the arrow on it is the handle.
Item {
    id: root

    property Viewport3D viewport
    /// What `DocumentModel.splitPlane` says (`valid: false` when no split is chosen).
    property var plane: ({ valid: false })

    // Follows the camera: read its values so that the binding is evaluated again.
    readonly property var points: {
        if (!viewport || !plane.valid) {
            return []
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        return plane.corners.map(corner => viewport.project(corner.x, corner.y, corner.z))
    }

    visible: plane.valid && points.length === 4
    enabled: false // nothing to click on

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.9)
            strokeWidth: 1.5
            fillColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
            joinStyle: ShapePath.RoundJoin

            PathPolyline {
                path: root.points.length === 4
                      ? [root.points[0], root.points[1], root.points[2], root.points[3], root.points[0]]
                      : []
            }
        }
    }
}
