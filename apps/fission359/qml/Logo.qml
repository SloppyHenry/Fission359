// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission359.Ui

// The logo mark: two facets of a cube in the accent colour.
Item {
    id: root

    property real size: 18

    width: size
    height: size

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: "transparent"
            fillColor: Theme.accentLine

            PathSvg {
                path: "M10.6 2.6 3 6.9v9.4l7.6 4.4z"
            }
        }

        ShapePath {
            strokeColor: "transparent"
            fillColor: Qt.rgba(Theme.accentLine.r, Theme.accentLine.g, Theme.accentLine.b, 0.5)

            PathSvg {
                path: "M13.4 2.6 21 6.9v9.4l-7.6 4.4z"
            }
        }
    }
}
