// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The small export page: which format, how fine, which bodies. The big page "What is the file
// for?" (R20) comes with AP-3.8. Asks for the file name only after the choice, in the system's
// own dialog (see Main.qml); nothing here blocks the work behind it.
Float {
    id: root

    property bool anyChosen: false
    property string format: "3mf"
    property string quality: "normal"
    property bool onlyChosen: false

    signal exportRequested(string format, string quality, bool onlyChosen)
    signal closeRequested

    width: 340
    height: content.implicitHeight + 32
    Accessible.name: qsTr("Export")

    Column {
        id: content

        x: 16
        y: 16
        width: parent.width - 32
        spacing: 12

        Txt {
            text: qsTr("Export")
            color: Theme.fg
            font.weight: Font.DemiBold
        }

        Txt {
            text: qsTr("File type")
            font.pixelSize: Theme.fontSizeSmall
        }

        Segmented {
            accessibleName: qsTr("File type")
            current: root.format
            model: [{ value: "3mf", text: qsTr("3MF (colours, for slicers)") }, { value: "stl", text: "STL" }]
            onActivated: value => root.format = value
        }

        Txt {
            text: qsTr("Quality")
            font.pixelSize: Theme.fontSizeSmall
        }

        Segmented {
            accessibleName: qsTr("Quality")
            current: root.quality
            model: [{ value: "coarse", text: qsTr("Coarse") }, { value: "normal", text: qsTr("Normal") }, { value: "fine", text: qsTr("Fine") }]
            onActivated: value => root.quality = value
        }

        // "Coarse" lets curves deviate by up to 0.05 mm (rule 12 asks 0.01 mm of the others).
        Txt {
            visible: root.quality === "coarse"
            width: parent.width
            text: qsTr("Curves can be angular.")
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Toggle {
            enabled: root.anyChosen
            text: qsTr("Only chosen bodies")
            checked: root.onlyChosen && root.anyChosen
            onToggled: root.onlyChosen = checked
        }

        Row {
            visible: !Exporter.exporting
            spacing: 8

            Btn {
                text: qsTr("Export…")
                variant: "primary"
                onClicked: root.exportRequested(root.format, root.quality, root.onlyChosen && root.anyChosen)
            }

            Btn {
                text: qsTr("Close")
                onClicked: root.closeRequested()
            }
        }

        Row {
            visible: Exporter.exporting
            spacing: 12

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Exporting… %1 %").arg(Math.round(Exporter.progress * 100))
                color: Theme.fg
            }

            Btn {
                text: qsTr("Cancel")
                onClicked: Exporter.cancel()
            }
        }
    }
}
