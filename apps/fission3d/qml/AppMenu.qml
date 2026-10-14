// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// A drop-down menu in the design of the program. items: [{ id, text, shortcut, enabled,
// checked, separator }]
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

    Repeater {
        model: menu.items

        delegate: Loader {
            required property var modelData

            sourceComponent: modelData.separator === true ? separatorComponent : entryComponent
            onLoaded: item.entry = modelData
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
            checkable: entry.checked !== undefined
            checked: entry.checked === true
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
                        visible: entryItem.checked
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
