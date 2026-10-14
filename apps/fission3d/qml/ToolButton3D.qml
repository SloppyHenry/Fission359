// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// A button of the tool palette: icon above, label below. `active` = the tool in use,
// `expanded` = its group is open.
AbstractButton {
    id: root

    property string iconName
    property bool active: false
    property bool expanded: false
    property string tipTitle: ""
    property string tipText: ""

    implicitWidth: 64
    implicitHeight: 52
    hoverEnabled: true
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: active

    // Tooltip card: after 0.8 s a title and a sentence (rule 10 of the master prompt).
    ToolTip {
        id: tip

        visible: root.hovered && root.tipText !== ""
        delay: 800
        x: root.width + 14
        y: 0
        padding: 10

        contentItem: Column {
            spacing: 3
            width: 230

            Txt {
                width: parent.width
                text: root.tipTitle !== "" ? root.tipTitle : root.text
                color: Theme.fg
                font.weight: Font.DemiBold
                elide: Text.ElideNone
            }

            Txt {
                width: parent.width
                text: root.tipText
                color: Theme.fg3
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
        }

        background: Rectangle {
            radius: Theme.radiusFloat
            color: Theme.panel
            border.width: 1
            border.color: Theme.line2
        }
    }

    contentItem: Column {
        spacing: 3

        Item {
            width: parent.width
            height: 18

            Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 18
                color: label.color
            }
        }

        Txt {
            id: label

            width: parent.width
            text: root.text
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSizeSmall
            lineHeight: 1.1
            elide: Text.ElideNone
            color: root.active ? "#FFFFFF" : root.expanded ? Theme.accentText : Theme.fg2
        }
    }

    background: Rectangle {
        radius: 7
        color: root.active ? Theme.accent : root.expanded ? Theme.accentSoft
             : root.hovered ? Theme.field : "transparent"

        Focusring {
            visible: root.visualFocus
        }
    }
}
