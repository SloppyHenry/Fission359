// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// A square button that only shows an icon; `tip` is its tooltip and its accessible name.
Button {
    id: control

    property string iconName
    property string tip
    property real buttonSize: 30
    property real iconSize: 16
    property bool framed: false

    implicitWidth: buttonSize
    implicitHeight: buttonSize
    hoverEnabled: true
    Accessible.name: tip
    ToolTip.visible: hovered && tip !== ""
    ToolTip.text: tip
    ToolTip.delay: 700

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: !control.enabled ? Theme.fg4 : control.hovered || control.checked ? Theme.fg : Theme.fg3
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: control.hovered ? Theme.field : control.framed ? Theme.panel : "transparent"
        border.width: control.framed ? 1 : 0
        border.color: Theme.line
        opacity: control.enabled ? 1.0 : 0.5

        Focusring {
            visible: control.visualFocus
        }
    }
}
