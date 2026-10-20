// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The points that align several bodies (AP-1.6e, as in Tinkercad): with two or more bodies chosen
// and the tool "Move", nine points sit on the box round them, three for each direction (lowest
// side, middle, highest side). A click puts all chosen bodies flush there, or in the middle, along
// that direction. The 3D view draws the points and, for the one the pointer is on, the plane the
// bodies line up on; this layer holds the places to click and a small label.
Item {
    id: root

    objectName: "alignLayer"

    Binding {
        target: Align
        property: "colours"
        value: ({ x: Theme.axisX, y: Theme.axisY, z: Theme.axisZ, ink: Theme.handleLine, fill: Theme.handle })
    }

    // What a point does, in words: left / middle / right along X, front / middle / back along Y,
    // bottom / middle / top along Z.
    function wordFor(axis, to) {
        if (to === "centre") {
            return qsTr("Align in the middle")
        }
        if (axis === "x") {
            return to === "min" ? qsTr("Align left") : qsTr("Align right")
        }
        if (axis === "y") {
            return to === "min" ? qsTr("Align front") : qsTr("Align back")
        }
        return to === "min" ? qsTr("Align bottom") : qsTr("Align top")
    }

    Repeater {
        model: Align.points.length

        delegate: Item {
            id: part

            required property int index
            readonly property var info: index < Align.points.length ? Align.points[index] : null
            readonly property bool pointed: area.containsMouse

            onPointedChanged: Align.setPointed(index, pointed)

            anchors.fill: parent
            visible: info !== null

            MouseArea {
                id: area

                x: part.info ? part.info.x - width / 2 : 0
                y: part.info ? part.info.y - height / 2 : 0
                width: 24
                height: 24
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                Accessible.role: Accessible.Button
                Accessible.name: part.info ? root.wordFor(part.info.axis, part.info.to) : ""
                onClicked: Align.align(part.index)
            }

            // A label beside the point the pointer is on.
            Rectangle {
                visible: part.pointed && part.info !== null
                x: part.info ? part.info.x + 14 : 0
                y: part.info ? part.info.y - height - 6 : 0
                width: label.implicitWidth + 14
                height: 22
                radius: 11
                color: Theme.panel
                border.width: 1
                border.color: Theme.line2
                z: 10

                Txt {
                    id: label

                    anchors.centerIn: parent
                    text: part.info ? root.wordFor(part.info.axis, part.info.to) : ""
                    color: Theme.fg
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideNone
                }
            }
        }
    }
}
