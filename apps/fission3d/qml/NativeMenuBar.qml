// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Qt.labs.platform

// The menus in the system menu bar (macOS). Same model as the in-window menus of MenuStrip.
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

                delegate: MenuItem {
                    required property var modelData

                    // The system menu on macOS 27 does not draw its own tick (the state is there, only
                    // not shown), so the tick is part of the text; the entries that can be ticked
                    // keep their place when it is not.
                    text: (modelData.checked === true ? "✓  " : modelData.checked === false ? "     " : "")
                          + (modelData.text || "")
                    separator: modelData.separator === true
                    enabled: modelData.enabled !== false
                    shortcut: modelData.nativeShortcut || ""
                    role: modelData.role === "about" ? MenuItem.AboutRole
                        : modelData.role === "preferences" ? MenuItem.PreferencesRole
                        : modelData.role === "quit" ? MenuItem.QuitRole
                        : MenuItem.NoRole
                    onTriggered: bar.actionTriggered(modelData.id)
                }

                onObjectAdded: (index, object) => menu.insertItem(index, object)
                onObjectRemoved: (index, object) => menu.removeItem(object)
            }
        }

        onObjectAdded: (index, object) => bar.insertMenu(index, object)
        onObjectRemoved: (index, object) => bar.removeMenu(object)
    }
}
