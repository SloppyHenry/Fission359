// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The status line (28 px) with the hint line: says in one sentence what can be done now.
Rectangle {
    id: root

    property string selection: ""
    property string toolName: ""
    property string hint: ""
    property string unitText: ""

    height: Theme.statusHeight
    color: Theme.chrome

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.line
    }

    Row {
        id: leftRow

        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: rightRow.left
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16
        clip: true

        Txt {
            visible: root.selection !== ""
            text: root.selection
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }

        Txt {
            text: qsTr("Tool: %1").arg(root.toolName)
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }

        Txt {
            text: root.hint
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
            Accessible.role: Accessible.StaticText
            Accessible.name: root.hint
        }
    }

    Row {
        id: rightRow

        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Txt {
            text: root.unitText
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }
    }
}
