// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The card that appears after 0.8 s over a tool, a shape or a command (rule 10): a title, a
// sentence, the shortcut, and a place for a short animation (AP-5.2 fills it). Put it as a child
// of the control it explains and bind `visible` to "the pointer is on it".
ToolTip {
    id: root

    property string title: ""
    property string sentence: ""
    property string shortcut: ""
    /// Room for a short animation or picture of the tool (nothing in it yet).
    property bool animationSlot: false

    delay: 800
    padding: 10

    contentItem: Column {
        spacing: 3
        width: 230

        Row {
            width: parent.width
            spacing: 8

            Txt {
                width: parent.width - (shortcutText.visible ? shortcutText.implicitWidth + 8 : 0)
                text: root.title
                color: Theme.fg
                font.weight: Font.DemiBold
                elide: Text.ElideNone
            }

            Txt {
                id: shortcutText

                visible: root.shortcut !== ""
                text: root.shortcut
                color: Theme.fg3
                font.pixelSize: Theme.fontSizeSmall
            }
        }

        Txt {
            width: parent.width
            text: root.sentence
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Rectangle {
            visible: root.animationSlot
            width: parent.width
            height: 96
            radius: Theme.radius
            color: Theme.chrome
            border.width: 1
            border.color: Theme.line
        }
    }

    background: Rectangle {
        radius: Theme.radiusFloat
        color: Theme.panel
        border.width: 1
        border.color: Theme.line2
    }
}
