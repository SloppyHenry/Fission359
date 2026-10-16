// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// Chooses a font: a field that opens a list with a search box. `fonts` is the list of names.
Item {
    id: root

    property string label: ""
    property string value: ""
    property var fonts: []
    signal chosen(string name)

    implicitWidth: 200
    implicitHeight: column.implicitHeight

    Column {
        id: column

        width: parent.width
        spacing: 5

        Txt {
            visible: root.label !== ""
            width: parent.width
            text: root.label
            color: Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
        }

        AbstractButton {
            id: button

            width: parent.width
            height: Theme.controlHeight
            hoverEnabled: true
            Accessible.name: root.label + ": " + root.value
            onClicked: popup.opened ? popup.close() : popup.open()

            contentItem: Row {
                spacing: 6
                leftPadding: 8
                rightPadding: 8

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    width: button.width - 16 - 20
                    text: root.value
                    color: Theme.fg
                }

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chev"
                    size: 14
                    color: Theme.fg3
                }
            }

            background: Rectangle {
                radius: Theme.radius
                color: Theme.field
                border.width: 1
                border.color: popup.opened || button.hovered ? Theme.accentLine : Theme.line

                Focusring {
                    visible: button.visualFocus
                }
            }
        }
    }

    Popup {
        id: popup

        y: column.height + 4
        width: root.width
        height: Math.min(300, list.contentHeight + search.height + 20)
        padding: 6
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: {
            search.text = ""
            search.forceActiveFocus()
        }

        background: Rectangle {
            radius: Theme.radiusFloat
            color: Theme.panel
            border.width: 1
            border.color: Theme.line2
        }

        contentItem: Column {
            spacing: 6

            Rectangle {
                width: parent.width
                height: 30
                radius: 6
                color: Theme.field
                border.width: 1
                border.color: search.activeFocus ? Theme.accentLine : Theme.line

                TextInput {
                    id: search

                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    color: Theme.fg
                    font.family: Theme.fontFamilies[0]
                    font.pixelSize: Theme.fontSize
                    Accessible.name: qsTr("Search fonts")
                    Keys.onReturnPressed: if (list.count > 0) root.pick(list.itemAtIndex(0).name)

                    Txt {
                        visible: search.text === ""
                        text: qsTr("Search fonts")
                        color: Theme.fg4
                    }
                }
            }

            ListView {
                id: list

                width: parent.width
                height: popup.height - search.parent.height - 26
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: {
                    const wanted = search.text.trim().toLowerCase()
                    return root.fonts.filter(name => wanted === "" || name.toLowerCase().indexOf(wanted) >= 0)
                }

                delegate: AbstractButton {
                    id: row

                    required property string modelData
                    readonly property string name: modelData

                    width: list.width
                    height: 30
                    hoverEnabled: true
                    onClicked: root.pick(modelData)

                    contentItem: Txt {
                        text: row.modelData
                        leftPadding: 8
                        verticalAlignment: Text.AlignVCenter
                        color: row.modelData === root.value ? Theme.accentText : Theme.fg
                        font.weight: row.modelData === root.value ? Font.DemiBold : Font.Normal
                    }

                    background: Rectangle {
                        radius: 6
                        color: row.hovered ? Theme.field : "transparent"
                    }
                }

                Txt {
                    visible: list.count === 0
                    anchors.centerIn: parent
                    text: root.fonts.length === 0 ? qsTr("Looking for fonts …") : qsTr("No font with that name.")
                    color: Theme.fg4
                }
            }
        }
    }

    function pick(name) {
        popup.close()
        if (name !== root.value) {
            root.chosen(name)
        }
    }
}
