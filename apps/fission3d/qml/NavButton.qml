// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// A navigation button: a click tells how the mouse does it, dragging does it.
Item {
    id: root

    property string iconName
    property string label
    property string hint
    signal hintRequested(string text)
    signal dragged(real dx, real dy)

    width: 34
    height: 34
    Accessible.name: label
    Accessible.role: Accessible.Button

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: handler.active || hover.hovered ? Theme.field : "transparent"
    }

    Icon {
        anchors.centerIn: parent
        name: root.iconName
        color: hover.hovered || handler.active ? Theme.fg : Theme.fg3
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.SizeAllCursor
    }

    DragHandler {
        id: handler

        target: null
        property point last
        onActiveChanged: if (active) last = Qt.point(0, 0)
        onTranslationChanged: {
            root.dragged(translation.x - last.x, translation.y - last.y)
            last = Qt.point(translation.x, translation.y)
        }
    }

    TapHandler {
        onTapped: root.hintRequested(root.hint)
    }
}
