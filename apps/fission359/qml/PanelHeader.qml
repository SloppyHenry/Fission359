// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The head of the property panel: icon, name and a line of explanation.
Item {
    id: root

    property string iconName
    property string title
    property string subtitle

    width: parent ? parent.width : 300
    height: 64

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.line
    }

    Rectangle {
        id: iconBox

        x: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 36
        radius: 8
        color: Theme.accentSoft

        Icon {
            anchors.centerIn: parent
            name: root.iconName
            color: Theme.accentText
        }
    }

    Column {
        anchors.left: iconBox.right
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter

        Txt {
            width: parent.width
            text: root.title
            color: Theme.fg
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Font.DemiBold
        }

        Txt {
            width: parent.width
            text: root.subtitle
            color: Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
        }
    }
}
