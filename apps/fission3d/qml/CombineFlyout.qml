// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The ways to combine, next to the group "Combine" of the palette. A way works on the bodies that
// are chosen (the first one chosen is the one that is changed).
Float {
    id: root

    readonly property alias catalog: combineCatalog
    /// How many bodies are chosen.
    property int chosen: 0
    signal wayChosen(string way)

    width: row.implicitWidth + 16
    height: row.implicitHeight + 16
    z: 5

    CombineCatalog {
        id: combineCatalog
    }

    Row {
        id: row

        x: 8
        y: 8
        spacing: 4

        Repeater {
            model: combineCatalog.ways

            delegate: ToolButton3D {
                required property var modelData

                text: modelData.name
                iconName: modelData.icon
                enabled: modelData.needs === 1 ? root.chosen === 1 : root.chosen >= 2
                opacity: enabled ? 1.0 : 0.45
                tipText: enabled ? modelData.tip
                                 : modelData.needs === 1 ? qsTr("Choose one body first.")
                                                         : qsTr("Choose two or more bodies first (Shift+click).")
                tipTitle: modelData.name
                onClicked: root.wayChosen(modelData.id)
            }
        }
    }
}
