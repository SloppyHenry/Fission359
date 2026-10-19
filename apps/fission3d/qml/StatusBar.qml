// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The status line (28 px) with the hint line: says in one sentence what can be done now.
Rectangle {
    id: root

    property string selection: ""
    property string toolName: ""
    property string hint: ""
    property string unitText: ""
    /// The grid steps to choose from: [{ id, text, checked }]; chosen one is `checked`.
    property var gridChoices: []
    signal gridChoiceRequested(string id)

    /// Opens the choice of the grid (as a click on it does); for the developer option --simulate.
    function openGridChoice() {
        gridButton.clicked()
    }

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

        // The grid: a click opens the choice of the snap step (the precision of the snapping).
        AbstractButton {
            id: gridButton

            anchors.verticalCenter: parent.verticalCenter
            visible: root.unitText !== ""
            hoverEnabled: true
            padding: 4
            leftPadding: 8
            rightPadding: 8
            Accessible.name: qsTr("Grid: change the precision of snapping")
            onClicked: gridMenu.popup(gridButton, gridButton.width - gridMenu.implicitWidth,
                                      -gridMenu.height - 4)

            contentItem: Txt {
                text: root.unitText
                color: gridButton.hovered || gridMenu.visible ? Theme.fg : Theme.fg3
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }

            background: Rectangle {
                radius: 5
                color: gridButton.hovered || gridMenu.visible ? Theme.field : "transparent"

                Focusring {
                    visible: gridButton.visualFocus
                }
            }

            AppMenu {
                id: gridMenu

                items: root.gridChoices
                onActionTriggered: id => root.gridChoiceRequested(id)
            }
        }
    }
}
