// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The time marker of the recipe, "── now ──": everything above it counts. Drag it up to see an
// earlier state of the model (the steps behind it are not calculated); a new step goes in at the
// marker. A double click puts it back at the end. The line only reports where it was let go; the
// recipe moves it.
Item {
    id: root

    property bool atEnd: true
    // While it is dragged: the vertical distance moved.
    readonly property bool dragging: drag.active
    // Pressed on the line (before it moves): the list must not scroll away under the hand.
    readonly property bool held: tap.pressed || drag.active
    readonly property real moved: drag.active ? drag.activeTranslation.y : 0
    signal dragMoved(real dy)
    signal released(real dy)
    signal reset
    property real lastMoved: 0

    width: parent ? parent.width : 260
    height: 28
    z: dragging ? 2 : 0
    Accessible.name: qsTr("Time marker")
    Accessible.description: qsTr("Drag up to see an earlier state. Double-click to go back to now.")

    HoverHandler {
        id: hover

        cursorShape: Qt.SizeVerCursor
    }

    DragHandler {
        id: drag

        target: null
        xAxis.enabled: false
        // The distance is remembered: it is already gone when the drag ends.
        onActiveTranslationChanged: {
            root.lastMoved = activeTranslation.y
            root.dragMoved(activeTranslation.y)
        }
        onActiveChanged: {
            if (!active) {
                root.released(root.lastMoved)
                root.lastMoved = 0
            }
        }
    }

    TapHandler {
        id: tap

        onDoubleTapped: root.reset()
    }

    Row {
        x: 0
        width: parent.width
        height: parent.height
        spacing: 8

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - nowText.width - 16) / 2
            height: root.dragging || hover.hovered ? 3 : 2
            radius: 1
            color: Theme.accentLine
        }

        Txt {
            id: nowText

            anchors.verticalCenter: parent.verticalCenter
            text: root.atEnd ? qsTr("now") : qsTr("now (earlier state)")
            color: Theme.accentText
            font.pixelSize: Theme.fontSizeSmall
            font.weight: Font.DemiBold
            elide: Text.ElideNone
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: (parent.width - nowText.width - 16) / 2
            height: root.dragging || hover.hovered ? 3 : 2
            radius: 1
            color: Theme.accentLine
        }
    }
}
