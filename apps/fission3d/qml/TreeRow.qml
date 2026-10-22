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
    property bool cut: false           // something was cut out of this body: a small hole symbol follows the name
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

    // The eye is pinned to the right edge, so all eyes line up in one column, whether a row
    // shows a count (print bed) or not.
    Item {
        id: lead

        visible: root.iconName !== "" || root.swatch.a > 0
        anchors.left: parent.left
        anchors.leftMargin: root.child ? 24 : 6
        anchors.verticalCenter: parent.verticalCenter
        width: visible ? 18 : 0
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

    Icon {
        visible: root.cut
        anchors.left: nameText.left
        anchors.leftMargin: Math.min(nameText.contentWidth, nameText.width) + 5
        anchors.verticalCenter: parent.verticalCenter
        name: "hole"
        size: 13
        color: Theme.fg4
    }

    Txt {
        id: nameText

        anchors.left: lead.visible ? lead.right : parent.left
        anchors.leftMargin: lead.visible ? 6 : (root.child ? 24 : 6)
        anchors.right: countText.visible ? countText.left : eyeButton.visible ? eyeButton.left : parent.right
        anchors.rightMargin: root.cut ? 24 : 6
        anchors.verticalCenter: parent.verticalCenter
        text: root.text
        color: root.group || root.selected ? Theme.fg : root.eyeOn ? Theme.fg2 : Theme.fg4
        font.weight: root.group || root.selected ? Font.DemiBold : Font.Normal
        font.strikeout: !root.eyeOn && !root.group
    }

    Txt {
        id: countText

        visible: root.count !== ""
        anchors.right: eyeButton.visible ? eyeButton.left : parent.right
        anchors.rightMargin: eyeButton.visible ? 4 : 8
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
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: 24
        implicitHeight: 24
        hoverEnabled: true
        Accessible.name: root.eyeLabel
        Accessible.checkable: true
        Accessible.checked: root.eyeOn
        onClicked: root.eyeToggled()

        // The control stretches its content item to 24 x 24; the icon draws from its
        // top-left corner, so it sits centred in a holder instead of being the content item.
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: root.eyeOn ? "eye" : "eyeoff"
                color: eyeButton.hovered ? Theme.fg : Theme.fg4
            }
        }

        background: Rectangle {
            radius: 4
            color: "transparent"
        }
    }
}
