// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission359.Ui
import "icons.js" as Icons

// A line icon on a 24 x 24 grid (the shapes of the design mockup).
Item {
    id: root

    property string name
    property color color: Theme.fg
    property real size: 16
    property real strokeWidth: 1.7

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg {
                path: Icons.paths[root.name] || ""
            }
        }
    }
}
