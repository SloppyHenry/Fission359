// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// A group of mutually exclusive buttons. model: [{ value, text, icon }]
Rectangle {
    id: root

    property var model: []
    property var current
    property string accessibleName: ""
    signal activated(var value)

    implicitWidth: row.implicitWidth + 6
    implicitHeight: 38
    radius: 8
    color: Theme.chrome
    border.width: 1
    border.color: Theme.line
    Accessible.role: Accessible.RadioButton
    Accessible.name: accessibleName

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: root.model

            delegate: AbstractButton {
                id: segment

                required property var modelData
                readonly property bool selected: root.current === modelData.value

                implicitHeight: 30
                implicitWidth: content.implicitWidth + 20
                hoverEnabled: true
                Accessible.name: modelData.text
                Accessible.checkable: true
                Accessible.checked: selected
                onClicked: root.activated(modelData.value)

                contentItem: Row {
                    id: content

                    spacing: 6

                    Icon {
                        visible: segment.modelData.icon !== undefined && segment.modelData.icon !== ""
                        anchors.verticalCenter: parent.verticalCenter
                        size: 14
                        name: segment.modelData.icon || ""
                        color: segment.selected || segment.hovered ? Theme.fg : Theme.fg3
                    }

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: segment.modelData.text
                        elide: Text.ElideNone
                        font.pixelSize: Theme.fontSizeSmallPlus
                        font.weight: segment.selected ? Font.DemiBold : Font.Normal
                        color: segment.selected || segment.hovered ? Theme.fg : Theme.fg3
                    }
                }

                background: Rectangle {
                    radius: Theme.radius
                    color: segment.selected ? Theme.field2 : "transparent"

                    Focusring {
                        visible: segment.visualFocus
                    }
                }
            }
        }
    }
}
