// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The command search (⌘K): type what you want to do in everyday words, in German or English;
// Enter does the best match. A command that cannot run now stays in the list, greyed, with the
// reason ("Choose a body first"). Esc closes.
Float {
    id: root

    /// function (text) -> [{ id, name, shortcut, group, reason }]
    property var source: null
    property var results: []
    property int current: 0

    signal chosen(string id)
    signal closeRequested

    width: Math.min(560, parent.width - 32)
    height: Math.min(parent.height - 40, 64 + list.contentHeight + 12)
    Accessible.name: qsTr("Command search")

    function open() {
        field.text = ""
        refresh()
        field.forceActiveFocus()
    }

    function refresh() {
        results = source !== null ? source(field.text) : []
        current = 0
    }

    function groupLabel(group) {
        switch (group) {
        case "tool": return qsTr("Tool")
        case "shape": return qsTr("Shape")
        case "file": return qsTr("File")
        case "edit": return qsTr("Edit")
        case "body": return qsTr("Body")
        case "view": return qsTr("View")
        case "soon": return qsTr("Soon")
        default: return ""
        }
    }

    TextField {
        id: field

        objectName: "commandField"
        x: 16
        y: 12
        width: parent.width - 32
        height: 40
        placeholderText: qsTr("What do you want to do?")
        font.pixelSize: Theme.fontSize + 1
        color: Theme.fg
        placeholderTextColor: Theme.fg4
        leftPadding: 12
        background: Rectangle {
            radius: Theme.radius
            color: Theme.field
            border.width: 1
            border.color: field.activeFocus ? Theme.accentLine : Theme.line
        }
        onTextChanged: root.refresh()
        Keys.onDownPressed: root.current = Math.min(root.current + 1, root.results.length - 1)
        Keys.onUpPressed: root.current = Math.max(root.current - 1, 0)
        Keys.onEscapePressed: root.closeRequested()
        Keys.onReturnPressed: if (root.results.length > 0) root.chosen(root.results[root.current].id)
        Keys.onEnterPressed: if (root.results.length > 0) root.chosen(root.results[root.current].id)
    }

    ListView {
        id: list

        x: 8
        y: 64
        width: parent.width - 16
        height: parent.height - 72
        clip: true
        model: root.results
        currentIndex: root.current
        boundsBehavior: Flickable.StopAtBounds
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Rectangle {
            id: row

            required property var modelData
            required property int index

            objectName: "command_" + modelData.id
            width: list.width
            height: modelData.reason !== "" ? 50 : 38
            radius: Theme.radius
            color: index === root.current ? Theme.accentSoft : (rowArea.containsMouse ? Theme.field : "transparent")

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                Txt {
                    text: row.modelData.name
                    color: row.modelData.reason !== "" ? Theme.fg4 : Theme.fg
                    font.weight: Font.Medium
                }

                Txt {
                    visible: row.modelData.reason !== ""
                    text: row.modelData.reason
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.fg4
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Txt {
                    text: root.groupLabel(row.modelData.group)
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.fg4
                }

                Txt {
                    visible: row.modelData.shortcut !== ""
                    text: row.modelData.shortcut
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.fg3
                }
            }

            MouseArea {
                id: rowArea

                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.chosen(row.modelData.id)
            }
        }
    }
}
