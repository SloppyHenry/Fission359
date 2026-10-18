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

    // Paddings as in the mockup (.palette .pt: 7 px top, 6 px bottom, 2 px sides), so the
    // highlight sits evenly around icon and label.
    implicitWidth: 64
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    topPadding: 7
    bottomPadding: 6
    horizontalPadding: 2
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
