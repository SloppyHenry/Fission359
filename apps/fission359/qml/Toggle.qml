// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// An on/off switch with its label (the "As hole" switch of the shape gallery).
AbstractButton {
    id: control

    checkable: true
    implicitHeight: 28
    implicitWidth: row.implicitWidth + 8
    hoverEnabled: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked

    contentItem: Row {
        id: row

        spacing: 8
        leftPadding: 4

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 18
            radius: 9
            color: control.checked ? Theme.accent : Theme.field2
            border.width: 1
            border.color: control.checked ? Theme.accent : Theme.line2

            Rectangle {
                x: control.checked ? parent.width - width - 2 : 2
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                radius: 7
                color: control.checked ? Theme.accentFg : Theme.fg3

                Behavior on x {
                    enabled: !AppOptions.reduceMotion
                    NumberAnimation { duration: Theme.duration }
                }
            }
        }

        Txt {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.checked ? Theme.fg : Theme.fg2
            elide: Text.ElideNone
        }
    }

    background: Item {
        Focusring {
            visible: control.visualFocus
        }
    }
}
