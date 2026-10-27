// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// A button with text and an optional icon. variant: "default", "primary" (one per area) or
// "amber" (help buttons).
Button {
    id: control

    property string variant: "default"
    property bool small: false
    property string iconName: ""

    readonly property bool primary: variant === "primary"
    readonly property bool amber: variant === "amber"

    implicitHeight: small ? Theme.smallControlHeight : Theme.controlHeight
    implicitWidth: contentRow.implicitWidth + leftPadding + rightPadding
    leftPadding: small ? 10 : 13
    rightPadding: leftPadding
    hoverEnabled: true
    font.pixelSize: small ? Theme.fontSizeSmall : Theme.fontSize
    Accessible.name: text

    contentItem: Row {
        id: contentRow

        spacing: 7

        Icon {
            visible: control.iconName !== ""
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            color: label.color
        }

        Txt {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            elide: Text.ElideNone
            font.pixelSize: control.font.pixelSize
            font.weight: control.primary || control.amber ? Font.DemiBold : Font.Medium
            color: control.primary ? Theme.accentFg : control.amber ? Theme.amberFg : Theme.fg
            opacity: control.enabled ? 1.0 : 0.45
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: control.primary ? (control.hovered ? Theme.accentHover : Theme.accent)
             : control.amber ? Theme.amber
             : control.hovered ? Theme.field : "transparent"
        border.width: 1
        border.color: control.primary ? Theme.accent
                    : control.amber ? Theme.amber
                    : control.hovered ? Theme.line3 : Theme.line2
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Theme.duration }
        }

        Focusring {
            visible: control.visualFocus
        }
    }
}
