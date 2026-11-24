// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// A drop-down menu in the design of the program. items: [{ id, text, shortcut, enabled,
// checked, separator, submenu }]. An entry with `submenu` ("Open recent") opens a second menu of
// the same kind beside it (hover, click or the right arrow key).
Menu {
    id: menu

    property var items: []
    signal actionTriggered(string id)

    padding: 6
    topPadding: 6
    bottomPadding: 6
    implicitWidth: 240

    background: Rectangle {
        implicitWidth: 240
        radius: Theme.radiusFloat
        color: Theme.panel
        border.width: 1
        border.color: Theme.line2
    }

    // The row of an entry with a submenu: the controls make it from `delegate` for the submenu.
    delegate: MenuItem {
        id: subRow

        implicitHeight: 30
        implicitWidth: 228
        indicator: null
        arrow: Txt {
            x: subRow.width - width - 10
            anchors.verticalCenter: parent.verticalCenter
            text: "›"
            color: subRow.enabled ? Theme.fg2 : Theme.fg4
        }
        contentItem: Txt {
            leftPadding: 24
            text: subRow.text
            color: subRow.enabled ? Theme.fg : Theme.fg4
        }
        background: Rectangle {
            radius: 5
            color: subRow.highlighted ? Theme.accentSoft : "transparent"
        }
    }

    Repeater {
        model: menu.items

        delegate: Loader {
            id: slot

            required property var modelData
            property var subMenu: null

            sourceComponent: modelData.separator === true ? separatorComponent
                           : modelData.submenu !== undefined ? placeholderComponent
                           : entryComponent
            onLoaded: {
                item.entry = modelData
                if (modelData.submenu !== undefined) {
                    slot.addSubMenu()
                }
            }

            // The submenu goes right after this (empty) place in the menu.
            function addSubMenu() {
                const component = Qt.createComponent(Qt.resolvedUrl("AppMenu.qml"))
                subMenu = component.createObject(menu, { title: modelData.text, items: modelData.submenu })
                subMenu.enabled = modelData.enabled !== false
                subMenu.actionTriggered.connect(id => {
                    menu.actionTriggered(id)
                    menu.close()
                })
                for (let i = 0; i < menu.count; ++i) {
                    if (menu.itemAt(i) === slot) {
                        menu.insertMenu(i + 1, subMenu)
                        return
                    }
                }
                menu.addMenu(subMenu)
            }

            Component.onDestruction: {
                if (subMenu !== null) {
                    menu.removeMenu(subMenu)
                    subMenu.destroy()
                }
            }
        }
    }

    Component {
        id: placeholderComponent

        Item {
            property var entry
            implicitHeight: 0
            visible: false
        }
    }

    Component {
        id: separatorComponent

        MenuSeparator {
            property var entry
            padding: 4
            contentItem: Rectangle {
                implicitHeight: 1
                color: Theme.line
            }
        }
    }

    Component {
        id: entryComponent

        MenuItem {
            id: entryItem

            property var entry

            text: entry.text
            enabled: entry.enabled !== false
            // Not `checkable`: a checkable item turns its own tick over when clicked (and so
            // stops following `entry.checked`), and the style draws a tick of its own on top of
            // the one below. The tick is drawn from `entry.checked` alone.
            readonly property bool ticked: entry.checked === true
            Accessible.role: entry.checked !== undefined ? Accessible.CheckBox : Accessible.MenuItem
            Accessible.checked: ticked
            indicator: null
            implicitHeight: 30
            implicitWidth: 228
            onTriggered: {
                menu.actionTriggered(entry.id)
                menu.close()
            }

            contentItem: Row {
                spacing: 8

                Item {
                    width: 16
                    height: parent.height

                    Icon {
                        visible: entryItem.ticked
                        anchors.centerIn: parent
                        name: "ok"
                        size: 14
                        color: Theme.accentText
                        strokeWidth: 2.2
                    }
                }

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    width: entryItem.width - 16 - 8 - 24 - shortcutText.width - 8
                    text: entryItem.text
                    color: entryItem.enabled ? Theme.fg : Theme.fg4
                }

                Txt {
                    id: shortcutText

                    anchors.verticalCenter: parent.verticalCenter
                    text: entryItem.entry.shortcut || ""
                    color: Theme.fg4
                    mono: true
                    font.pixelSize: Theme.fontSizeSmall
                    elide: Text.ElideNone
                }
            }

            background: Rectangle {
                radius: 5
                color: entryItem.highlighted ? Theme.accentSoft : "transparent"
            }
        }
    }
}
