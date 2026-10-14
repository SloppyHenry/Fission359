// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The top bar (40 px): logo, menus (in the window except on macOS, where they are in the
// system menu bar), command search, settings and printer profile.
Rectangle {
    id: root

    property var menus: []          // [{ title, items: [{ id, text, shortcut, enabled, checked, separator }] }]
    property bool inWindowMenus: true
    property string printerText: ""
    signal actionTriggered(string id)
    signal searchRequested
    signal settingsRequested
    signal printerRequested

    height: Theme.menuBarHeight
    color: Theme.chrome

    // Opens menu number `index` (developer option --open-menu, for screenshots).
    function openMenu(index) {
        const button = menuRepeater.itemAt(index)
        if (button) {
            button.openPopup()
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.line
    }

    Row {
        id: left

        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Row {
            id: brand

            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            rightPadding: 10

            Logo {
                anchors.verticalCenter: parent.verticalCenter
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: "Fission3D"
                color: Theme.fg
                font.weight: Font.DemiBold
                elide: Text.ElideNone
            }
        }

        Repeater {
            id: menuRepeater

            model: root.inWindowMenus ? root.menus : []

            delegate: AbstractButton {
                id: menuButton

                required property var modelData
                property bool open: popup.visible

                implicitHeight: 28
                implicitWidth: label.implicitWidth + 18
                hoverEnabled: true
                Accessible.name: modelData.title
                function openPopup() {
                    popup.popup(0, height + 2)
                }

                onClicked: popup.visible ? popup.close() : openPopup()

                contentItem: Txt {
                    id: label

                    text: menuButton.modelData.title
                    color: menuButton.hovered || menuButton.open ? Theme.fg : Theme.fg2
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideNone
                }

                background: Rectangle {
                    radius: 5
                    color: menuButton.hovered || menuButton.open ? Theme.field : "transparent"

                    Focusring {
                        visible: menuButton.visualFocus
                    }
                }

                AppMenu {
                    id: popup

                    items: menuButton.modelData.items
                    onActionTriggered: id => root.actionTriggered(id)
                }
            }
        }
    }

    // Command search: always in the middle, up to 440 px wide.
    AbstractButton {
        id: search

        // like the design grid: the free space between the menus and the buttons on the right,
        // the search in the middle of it, up to 440 px wide
        readonly property real free: root.width - 24 - left.width - right.width - 24
        width: Math.max(160, Math.min(440, free))
        x: 12 + left.width + 12 + Math.max(0, (free - width) / 2)
        visible: free >= 140 // very narrow windows: the search moves to the keyboard (Ctrl+K)
        anchors.verticalCenter: parent.verticalCenter
        height: 28
        hoverEnabled: true
        Accessible.name: qsTr("Open command search")
        onClicked: root.searchRequested()

        background: Rectangle {
            radius: 7
            color: Theme.bar
            border.width: 1
            border.color: search.hovered ? Theme.line3 : Theme.line2

            Focusring {
                visible: search.visualFocus
            }
        }

        contentItem: Row {
            spacing: 10
            leftPadding: 11
            rightPadding: 6

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                color: search.hovered ? Theme.fg3 : Theme.fg4
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                width: search.width - 11 - 6 - 16 - 10 * 2 - kbd.width
                text: qsTr("What do you want to do?")
                color: search.hovered ? Theme.fg3 : Theme.fg4
            }

            Rectangle {
                id: kbd

                anchors.verticalCenter: parent.verticalCenter
                width: kbdText.implicitWidth + 12
                height: 20
                radius: 4
                color: Theme.field
                border.width: 1
                border.color: Theme.line2

                Txt {
                    id: kbdText

                    anchors.centerIn: parent
                    text: Qt.platform.os === "osx" ? "⌘K" : qsTr("Ctrl+K")
                    mono: true
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.Medium
                    elide: Text.ElideNone
                }
            }
        }
    }

    Row {
        id: right

        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        IconBtn {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "gear"
            tip: Qt.platform.os === "osx" ? qsTr("Settings (⌘,)") : qsTr("Settings (Ctrl+,)")
            onClicked: root.settingsRequested()
        }

        AbstractButton {
            id: printer

            anchors.verticalCenter: parent.verticalCenter
            implicitHeight: 28
            implicitWidth: printerRow.implicitWidth + 18
            hoverEnabled: true
            Accessible.name: root.printerText
            onClicked: root.printerRequested()

            contentItem: Row {
                id: printerRow

                spacing: 6

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "printer"
                    color: printer.hovered ? Theme.fg : Theme.fg4
                }

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.printerText
                    color: printer.hovered ? Theme.fg : Theme.fg4
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideNone
                }
            }

            background: Rectangle {
                radius: 5
                color: printer.hovered ? Theme.field : "transparent"

                Focusring {
                    visible: printer.visualFocus
                }
            }
        }
    }
}
