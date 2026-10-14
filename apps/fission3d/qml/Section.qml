// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// A block of the property panel with an optional heading.
Item {
    id: root

    default property alias content: body.data
    property string heading: ""
    property string aside: ""

    width: parent ? parent.width : 300
    implicitHeight: (visible ? column.implicitHeight + 28 : 0)
    height: implicitHeight

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.line
    }

    Column {
        id: column

        x: 14
        y: 14
        width: parent.width - 28
        spacing: 10

        Item {
            visible: root.heading !== ""
            width: parent.width
            height: 20

            Txt {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: root.heading
                color: Theme.fg
                font.weight: Font.DemiBold
                elide: Text.ElideNone
            }

            Txt {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.aside
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }
        }

        Column {
            id: body

            width: parent.width
            spacing: 9
        }
    }
}
