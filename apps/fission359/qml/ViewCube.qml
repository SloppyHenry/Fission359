// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission359.Ui

// The view cube with the axis gizmo (docs/02-ux-konzept.md §10): shows how the camera looks at
// the bed and jumps to a standard view when a face is clicked. Every face is cut into 3 x 3
// fields: the middle one is the face, the ones at the border are the edges, the corner ones
// the corners of the cube. Each field looks at the cube from its own side.
Item {
    id: root

    property real yaw: 0       // degrees, like the camera
    property real pitch: 0
    signal viewRequested(string view)
    signal directionRequested(real x, real y, real z)

    width: 112
    height: 134

    readonly property real cx: 56
    readonly property real cy: 54
    readonly property real unit: 27
    readonly property real ca: Math.cos(yaw * Math.PI / 180)
    readonly property real sa: Math.sin(yaw * Math.PI / 180)
    readonly property real ce: Math.cos(pitch * Math.PI / 180)
    readonly property real se: Math.sin(pitch * Math.PI / 180)

    // Position of a model point on the screen, like the camera.
    function px(x, y, z, ox, oy, s) {
        const u = x * ca - y * sa
        const w = x * sa + y * ca
        const up = z * ce + w * se
        return Qt.point(ox + u * s, oy - up * s)
    }

    readonly property var faces: [
        { key: "top", label: qsTr("TOP"), c: [0, 0, 1], u: [1, 0, 0], v: [0, -1, 0],
          pts: [[-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1]] },
        { key: "front", label: qsTr("FRONT"), c: [0, -1, 0], u: [1, 0, 0], v: [0, 0, -1],
          pts: [[-1, -1, -1], [1, -1, -1], [1, -1, 1], [-1, -1, 1]] },
        { key: "right", label: qsTr("RIGHT"), c: [1, 0, 0], u: [0, 1, 0], v: [0, 0, -1],
          pts: [[1, -1, -1], [1, 1, -1], [1, 1, 1], [1, -1, 1]] },
        { key: "back", label: qsTr("BACK"), c: [0, 1, 0], u: [-1, 0, 0], v: [0, 0, -1],
          pts: [[1, 1, -1], [-1, 1, -1], [-1, 1, 1], [1, 1, 1]] },
        { key: "left", label: qsTr("LEFT"), c: [-1, 0, 0], u: [0, -1, 0], v: [0, 0, -1],
          pts: [[-1, 1, -1], [-1, -1, -1], [-1, -1, 1], [-1, 1, 1]] },
        { key: "bottom", label: qsTr("BOTTOM"), c: [0, 0, -1], u: [1, 0, 0], v: [0, 1, 0],
          pts: [[-1, -1, -1], [-1, 1, -1], [1, 1, -1], [1, -1, -1]] }
    ]

    Repeater {
        model: root.faces

        delegate: Item {
            id: face

            required property var modelData

            // direction from the cube to the camera
            readonly property real facing: modelData.c[0] * -root.sa * root.ce
                                         + modelData.c[1] * -root.ca * root.ce
                                         + modelData.c[2] * root.se
            readonly property real depth: -(modelData.c[0] * root.sa + modelData.c[1] * root.ca) * root.ce
                                          + modelData.c[2] * root.se

            visible: facing > 0.02
            // the face nearer the camera is drawn later: smaller depth value is farther away
            z: depth * 10
            anchors.fill: parent

            Shape {
                id: shape

                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: Theme.line3
                    strokeWidth: 1
                    fillColor: Theme.field
                    joinStyle: ShapePath.RoundJoin

                    PathPolyline {
                        path: {
                            const out = []
                            for (let i = 0; i < 4; ++i) {
                                const p = face.modelData.pts[i]
                                out.push(root.px(p[0], p[1], p[2], root.cx, root.cy, root.unit))
                            }
                            out.push(out[0])
                            return out
                        }
                    }
                }
            }

            // the nine fields of the face: face, four edges, four corners
            Repeater {
                model: 9

                delegate: Shape {
                    id: field

                    required property int index

                    // position in the face: -1, 0 or 1 along its two directions
                    readonly property int fu: index % 3 - 1
                    readonly property int fv: Math.floor(index / 3) - 1
                    readonly property var edges: [-1, -1 / 3, 1 / 3, 1]

                    function corner(a, b) {
                        const m = face.modelData
                        return root.px(m.c[0] + a * m.u[0] + b * m.v[0],
                                       m.c[1] + a * m.u[1] + b * m.v[1],
                                       m.c[2] + a * m.u[2] + b * m.v[2],
                                       root.cx, root.cy, root.unit)
                    }

                    anchors.fill: parent
                    preferredRendererType: Shape.CurveRenderer
                    containsMode: Shape.FillContains

                    ShapePath {
                        strokeColor: "transparent"
                        fillColor: hover.hovered ? Theme.accentSoft : "transparent"

                        PathPolyline {
                            path: {
                                const a0 = field.edges[field.fu + 1]
                                const a1 = field.edges[field.fu + 2]
                                const b0 = field.edges[field.fv + 1]
                                const b1 = field.edges[field.fv + 2]
                                return [field.corner(a0, b0), field.corner(a1, b0),
                                        field.corner(a1, b1), field.corner(a0, b1), field.corner(a0, b0)]
                            }
                        }
                    }

                    HoverHandler {
                        id: hover

                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: {
                            const m = face.modelData
                            if (field.fu === 0 && field.fv === 0) {
                                root.viewRequested(m.key)
                            } else {
                                root.directionRequested(m.c[0] + field.fu * m.u[0] + field.fv * m.v[0],
                                                        m.c[1] + field.fu * m.u[1] + field.fv * m.v[1],
                                                        m.c[2] + field.fu * m.u[2] + field.fv * m.v[2])
                            }
                        }
                    }
                }
            }

            // the label lies in the face: an affine copy of the text plane
            Item {
                id: plane

                readonly property point o: root.px(face.modelData.c[0], face.modelData.c[1], face.modelData.c[2],
                                                   root.cx, root.cy, root.unit)
                readonly property point pu: root.px(face.modelData.c[0] + face.modelData.u[0],
                                                    face.modelData.c[1] + face.modelData.u[1],
                                                    face.modelData.c[2] + face.modelData.u[2],
                                                    root.cx, root.cy, root.unit)
                readonly property point pv: root.px(face.modelData.c[0] + face.modelData.v[0],
                                                    face.modelData.c[1] + face.modelData.v[1],
                                                    face.modelData.c[2] + face.modelData.v[2],
                                                    root.cx, root.cy, root.unit)

                x: o.x
                y: o.y
                transform: Matrix4x4 {
                    matrix: Qt.matrix4x4((plane.pu.x - plane.o.x) / root.unit, (plane.pv.x - plane.o.x) / root.unit, 0, 0,
                                         (plane.pu.y - plane.o.y) / root.unit, (plane.pv.y - plane.o.y) / root.unit, 0, 0,
                                         0, 0, 1, 0,
                                         0, 0, 0, 1)
                }

                Txt {
                    x: -width / 2
                    y: -height / 2
                    text: face.modelData.label
                    mono: true
                    renderType: Text.QtRendering // text in a skewed plane
                    color: Theme.fg3
                    font.weight: Font.DemiBold
                    font.pixelSize: face.modelData.label.length > 5 ? 10 : 12
                    elide: Text.ElideNone
                }
            }
        }
    }

    // axis gizmo at the lower left
    Shape {
        id: axes

        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        readonly property real gx: 16
        readonly property real gy: 116
        readonly property real gs: 13

        ShapePath {
            strokeColor: Theme.axisX
            strokeWidth: 2
            capStyle: ShapePath.RoundCap
            PathMove { x: axes.gx; y: axes.gy }
            PathLine {
                x: root.px(1, 0, 0, axes.gx, axes.gy, axes.gs).x
                y: root.px(1, 0, 0, axes.gx, axes.gy, axes.gs).y
            }
        }

        ShapePath {
            strokeColor: Theme.axisY
            strokeWidth: 2
            capStyle: ShapePath.RoundCap
            PathMove { x: axes.gx; y: axes.gy }
            PathLine {
                x: root.px(0, 1, 0, axes.gx, axes.gy, axes.gs).x
                y: root.px(0, 1, 0, axes.gx, axes.gy, axes.gs).y
            }
        }

        ShapePath {
            strokeColor: Theme.axisZ
            strokeWidth: 2
            capStyle: ShapePath.RoundCap
            PathMove { x: axes.gx; y: axes.gy }
            PathLine {
                x: root.px(0, 0, 1, axes.gx, axes.gy, axes.gs).x
                y: root.px(0, 0, 1, axes.gx, axes.gy, axes.gs).y
            }
        }
    }

    Repeater {
        model: [
            { name: "X", v: [1, 0, 0], color: Theme.axisX },
            { name: "Y", v: [0, 1, 0], color: Theme.axisY },
            { name: "Z", v: [0, 0, 1], color: Theme.axisZ }
        ]

        delegate: Txt {
            required property var modelData

            readonly property point end: root.px(modelData.v[0], modelData.v[1], modelData.v[2], axes.gx, axes.gy, axes.gs)

            x: axes.gx + (end.x - axes.gx) * 1.35 - width / 2
            y: axes.gy + (end.y - axes.gy) * 1.35 - height / 2
            text: modelData.name
            color: modelData.color
            font.pixelSize: 12
            font.weight: Font.Bold
            mono: true
            renderType: Text.QtRendering
            elide: Text.ElideNone
        }
    }
}
