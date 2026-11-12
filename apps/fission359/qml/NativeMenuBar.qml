// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Qt.labs.platform

// The menus in the system menu bar (macOS). Same model as the in-window menus of MenuStrip; an
// entry with `submenu` becomes a nested menu.
MenuBar {
    id: bar

    property var menuData: []
    signal actionTriggered(string id)

    Instantiator {
        model: bar.menuData

        delegate: Menu {
            id: menu

            required property var modelData

            title: modelData.title

            Instantiator {
                model: menu.modelData.items

                // Each entry is a plain item, or (with `submenu`, "Open recent") a nested menu.
                delegate: QtObject {
                    id: slot

                    required property var modelData
                    readonly property bool nested: modelData.submenu !== undefined

                    property MenuItem item: MenuItem {
                        // The system menu on macOS 27 does not draw its own tick (the state is there,
                        // only not shown), so the tick is part of the text; the entries that can be
                        // ticked keep their place when it is not.
                        text: (slot.modelData.checked === true ? "✓  " : slot.modelData.checked === false ? "     " : "")
                              + (slot.modelData.text || "")
                        separator: slot.modelData.separator === true
                        enabled: slot.modelData.enabled !== false
                        shortcut: slot.modelData.nativeShortcut || ""
                        role: slot.modelData.role === "about" ? MenuItem.AboutRole
                            : slot.modelData.role === "preferences" ? MenuItem.PreferencesRole
                            : slot.modelData.role === "quit" ? MenuItem.QuitRole
                            : MenuItem.NoRole
                        onTriggered: bar.actionTriggered(slot.modelData.id)
                    }

                    property Menu sub: Menu {
                        id: subMenu

                        title: slot.modelData.text || ""
                        enabled: slot.modelData.enabled !== false

                        Instantiator {
                            model: slot.nested ? slot.modelData.submenu : []

                            delegate: MenuItem {
                                required property var modelData

                                text: modelData.text || ""
                                enabled: modelData.enabled !== false
                                onTriggered: bar.actionTriggered(modelData.id)
                            }

                            onObjectAdded: (index, object) => subMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => subMenu.removeItem(object)
                        }
                    }
                }

                onObjectAdded: (index, object) => {
                    if (object.nested) {
                        menu.insertMenu(index, object.sub)
                    } else {
                        menu.insertItem(index, object.item)
                    }
                }
                onObjectRemoved: (index, object) => {
                    if (object.nested) {
                        menu.removeMenu(object.sub)
                    } else {
                        menu.removeItem(object.item)
                    }
                }
            }
        }

        onObjectAdded: (index, object) => bar.insertMenu(index, object)
        onObjectRemoved: (index, object) => bar.removeMenu(object)
    }
}
