// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// A small menu that opens beside a row (right click): a list of actions, nothing modal.
// `entries`: [{ id, text, icon, danger }]. Esc or a click beside closes it.
Popup {
    id: root

    property var entries: []
    signal chosen(string id)

    padding: 4
    width: 190
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        radius: Theme.radiusFloat
        color: Theme.raise
        border.width: 1
        border.color: Theme.line2
    }

    contentItem: Column {
        spacing: 1

        Repeater {
            model: root.entries

            delegate: AbstractButton {
                id: entry

                required property var modelData

                objectName: "menu_" + modelData.id
                width: parent.width
                height: 30
                hoverEnabled: true
                Accessible.name: modelData.text
                onClicked: {
                    root.close()
                    root.chosen(modelData.id)
                }

                background: Rectangle {
                    radius: 5
                    color: entry.hovered ? Theme.field : "transparent"
                }

                contentItem: Row {
                    leftPadding: 8
                    spacing: 8

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: entry.modelData.icon
                        color: entry.modelData.danger === true ? Theme.bad : Theme.fg3
                    }

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.modelData.text
                        color: entry.modelData.danger === true ? Theme.bad : Theme.fg
                    }
                }
            }
        }
    }
}
