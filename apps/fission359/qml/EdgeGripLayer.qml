// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission359.Ui
import Fission359.View

// The quarter-circle grip over an edge the pointer rests on (AP-2.5, UX §2.2): a click rounds the
// edge, with Alt/⌥ held it bevels it. The grip stays a moment after the pointer leaves the edge so
// that it can be reached. It only offers; the rounding itself is a step like any other.
Item {
    id: root

    property Viewport3D viewport
    /// What `Selection.hover` says: { kind, bodyIndex, number } (empty when nothing is hovered).
    property var hover: ({})
    /// Edges can be hovered and chosen at all (the filter of the view).
    property bool active: true

    signal requested(string bodyId, int number, bool bevel)

    property string bodyId: ""
    property int number: 0
    property var anchor: ({ valid: false })

    readonly property var spot: {
        if (!viewport || !anchor.valid) {
            return null
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        // A little way into the body, so that the grip does not sit on the edge it belongs to.
        const reach = 18 / Math.max(viewport.pixelsPerMillimetre, 0.001)
        return viewport.project(anchor.x + anchor.dx * reach, anchor.y + anchor.dy * reach, anchor.z + anchor.dz * reach)
    }

    objectName: "edgeGrip"
    visible: active && spot !== null

    onHoverChanged: {
        if (active && hover.kind === "edge") {
            const id = DocumentModel.bodyIdAt(hover.bodyIndex)
            bodyId = id
            number = hover.number
            anchor = DocumentModel.edgeAnchor(id, hover.number)
            hideTimer.stop()
        } else if (!grip.containsMouse) {
            hideTimer.restart()
        }
    }

    Timer {
        id: hideTimer

        interval: 700
        onTriggered: if (!grip.containsMouse) { root.anchor = ({ valid: false }) }
    }

    Rectangle {
        id: dot

        x: root.spot ? root.spot.x - width / 2 : 0
        y: root.spot ? root.spot.y - height / 2 : 0
        width: 22
        height: 22
        radius: 11
        color: grip.containsMouse ? Theme.accent : Theme.raise
        border.width: 1.5
        border.color: Theme.accent

        // The quarter circle that says "round".
        Shape {
            anchors.centerIn: parent
            width: 12
            height: 12
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: grip.containsMouse ? "white" : Theme.accent
                strokeWidth: 2
                fillColor: "transparent"
                startX: 1
                startY: 11
                PathArc { x: 11; y: 1; radiusX: 10; radiusY: 10; direction: PathArc.Clockwise }
            }
        }

        MouseArea {
            id: grip

            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: mouse => {
                root.requested(root.bodyId, root.number, (mouse.modifiers & Qt.AltModifier) !== 0)
                root.anchor = ({ valid: false })
            }
            onExited: hideTimer.restart()
        }
    }
}
