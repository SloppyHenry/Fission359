// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// One row of the body list: swatch or icon, name, optional count and an eye button.
Rectangle {
    id: root

    property bool group: false
    property bool child: false
    property bool selected: false
    property bool hasEye: true
    property bool eyeOn: true
    property string eyeLabel: ""
    property string iconName: ""
    property color swatch: "transparent"
    property bool hole: false          // a hole: the swatch is an empty outline (a ghost)
    property string text: ""
    property string count: ""
    signal eyeToggled
    /// The row was clicked (not its eye). `extend`: with Shift or Ctrl/Cmd held.
    signal clicked(bool extend)

    width: parent ? parent.width : 200
    height: Theme.treeRowHeight
    radius: Theme.radius
    color: selected ? Theme.accentSoft : hover.hovered ? Theme.field : "transparent"
    border.width: 1
    border.color: selected ? Qt.rgba(Theme.accentLine.r, Theme.accentLine.g, Theme.accentLine.b, 0.45)
                           : "transparent"

    HoverHandler {
        id: hover
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: (point, button) => {
            if (!eyeButton.hovered) {
                root.clicked((point.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0)
            }
        }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: root.child ? 24 : 6
        anchors.rightMargin: 4
        spacing: 6

        Item {
            visible: root.iconName !== "" || root.swatch.a > 0
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18

            Icon {
                visible: root.iconName !== ""
                anchors.centerIn: parent
                name: root.iconName
                color: root.eyeOn ? Theme.fg2 : Theme.fg4
            }

            Rectangle {
                visible: root.iconName === "" && root.swatch.a > 0
                anchors.centerIn: parent
                width: 12
                height: 12
                radius: 3
                color: root.hole ? "transparent" : root.swatch
                border.width: root.hole ? 2 : 1
                border.color: root.hole ? root.swatch : Theme.edge
            }
        }

        Txt {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - (root.iconName !== "" || root.swatch.a > 0 ? 24 : 0)
                   - countText.width - (root.hasEye ? 30 : 0) - 12
            text: root.text
            color: root.group || root.selected ? Theme.fg : root.eyeOn ? Theme.fg2 : Theme.fg4
            font.weight: root.group || root.selected ? Font.DemiBold : Font.Normal
            font.strikeout: !root.eyeOn && !root.group
        }

        Txt {
            id: countText

            visible: root.count !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: root.count
            color: Theme.fg4
            mono: root.count.indexOf("×") >= 0
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }

        AbstractButton {
            id: eyeButton

            visible: root.hasEye
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 24
            implicitHeight: 24
            hoverEnabled: true
            Accessible.name: root.eyeLabel
            Accessible.checkable: true
            Accessible.checked: root.eyeOn
            onClicked: root.eyeToggled()

            contentItem: Icon {
                name: root.eyeOn ? "eye" : "eyeoff"
                color: parent.hovered ? Theme.fg : Theme.fg4
            }

            background: Rectangle {
                radius: 4
                color: "transparent"
            }
        }
    }
}
