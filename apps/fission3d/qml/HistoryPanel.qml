// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// "Recent actions": what was done in this session, oldest first. A click goes back (or
// forward) to the state after that action. Not a dialog: it floats above the 3D view and
// everything else stays usable.
Float {
    id: root

    // [{ text, done }]; position = number of actions that are done
    property var entries: []
    property int position: 0
    property string startText: qsTr("Start of this session")
    signal goToRequested(int position)
    signal closeRequested

    width: 280
    height: Math.min(header.height + 1 + list.contentHeight + 12, 380)

    Item {
        id: header

        width: parent.width
        height: 40

        Txt {
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Recent actions")
            color: Theme.fg
            font.weight: Font.DemiBold
        }

        IconBtn {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            iconName: "close"
            tip: qsTr("Close")
            onClicked: root.closeRequested()
        }
    }

    Rectangle {
        y: header.height
        width: parent.width
        height: 1
        color: Theme.line
    }

    ListView {
        id: list

        y: header.height + 1
        width: parent.width
        height: parent.height - y - 6
        topMargin: 6
        leftMargin: 6
        rightMargin: 6
        clip: true
        model: root.entries.length + 1 // the start, then one row per action
        // Keep the newest action in view.
        onCountChanged: positionViewAtEnd()

        delegate: Rectangle {
            id: row

            required property int index
            // Row 0 is the start (position 0); row i stands for the state after action i.
            readonly property bool isCurrent: index === root.position
            readonly property bool isRedoable: index > root.position
            readonly property string label: index === 0 ? root.startText : root.entries[index - 1].text

            width: list.width - list.leftMargin - list.rightMargin
            height: Theme.treeRowHeight
            radius: Theme.radius
            color: isCurrent ? Theme.accentSoft : hover.hovered ? Theme.field : "transparent"

            HoverHandler {
                id: hover
            }

            TapHandler {
                onTapped: root.goToRequested(row.index)
            }

            Txt {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: Text.AlignVCenter
                text: row.label
                // Things that were undone are shown pale: they can still be redone.
                color: row.isRedoable ? Theme.fg4 : Theme.fg
                font.italic: row.isRedoable
                font.weight: row.isCurrent ? Font.DemiBold : Font.Normal
            }

            Accessible.role: Accessible.ListItem
            Accessible.name: row.label
        }
    }
}
