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
    property bool faceChosen: false
    property string format: "3mf"
    property string quality: "normal"
    property bool onlyChosen: false

    readonly property bool isOutline: format === "svg" || format === "dxf"

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
            model: [{ value: "3mf", text: "3MF" }, { value: "stl", text: "STL" }, { value: "obj", text: "OBJ" }, { value: "step", text: "STEP" }]
            onActivated: value => root.format = value
        }

        Txt {
            visible: !root.isOutline
            width: parent.width
            text: root.format === "3mf" ? qsTr("Shape and colours, for slicers.")
                : root.format === "stl" ? qsTr("Shape only. Every program can read it.")
                : root.format === "obj" ? qsTr("Shape and colours, for 3D programs. The colours go into a second file.")
                : qsTr("The exact shape with names and colours, for other CAD programs.")
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Txt {
            text: qsTr("Outline for a laser cutter")
            font.pixelSize: Theme.fontSizeSmall
        }

        Segmented {
            visible: root.faceChosen
            accessibleName: qsTr("Outline for a laser cutter")
            current: root.format
            model: [{ value: "svg", text: "SVG" }, { value: "dxf", text: "DXF" }]
            onActivated: value => root.format = value
        }

        Txt {
            width: parent.width
            text: root.faceChosen ? qsTr("The edges of the chosen face, as seen from outside.")
                                  : qsTr("Choose a flat face to export its outline.")
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Txt {
            visible: !root.isOutline && root.format !== "step"
            text: qsTr("Quality")
            font.pixelSize: Theme.fontSizeSmall
        }

        Segmented {
            visible: !root.isOutline && root.format !== "step"
            accessibleName: qsTr("Quality")
            current: root.quality
            model: [{ value: "coarse", text: qsTr("Coarse") }, { value: "normal", text: qsTr("Normal") }, { value: "fine", text: qsTr("Fine") }]
            onActivated: value => root.quality = value
        }

        // "Coarse" lets curves deviate by up to 0.05 mm (rule 12 asks 0.01 mm of the others).
        Txt {
            visible: root.quality === "coarse" && !root.isOutline && root.format !== "step"
            width: parent.width
            text: qsTr("Curves can be angular.")
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Toggle {
            visible: !root.isOutline
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
