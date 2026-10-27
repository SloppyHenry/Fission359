// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The first start, in three small steps: language, printer, mouse. A card above the empty view,
// not a dialog: it can be skipped at any time ("Later") and everything stays changeable in the
// settings. Missions ("Are you new here?") come with AP-5.1; until then it ends with "Let's go".
Float {
    id: root

    property int step: 0
    readonly property int lastStep: 2

    signal finished

    width: Math.min(520, parent.width - 32)
    height: content.implicitHeight + 36
    Accessible.name: qsTr("Welcome")

    function done() {
        Preferences.firstRunDone = true
        root.finished()
    }

    Column {
        id: content

        x: 20
        y: 18
        width: parent.width - 40
        spacing: 12

        Txt {
            text: root.step === 0 ? qsTr("Welcome to Fission359! Which language do you prefer?")
                : root.step === 1 ? qsTr("Which printer do you have?")
                : qsTr("How do you like to move the view?")
            color: Theme.fg
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            width: parent.width
        }

        Segmented {
            visible: root.step === 0
            accessibleName: qsTr("Language")
            current: Preferences.language
            model: [{ value: "system", text: qsTr("Like the computer") }, { value: "de", text: "Deutsch" }, { value: "en", text: "English" }]
            onActivated: value => Preferences.language = value
        }

        Flow {
            visible: root.step === 1
            width: parent.width
            spacing: 6

            Repeater {
                model: DocumentModel.printers

                Btn {
                    required property var modelData

                    text: modelData.name !== "" ? modelData.name : qsTr("My printer")
                    small: true
                    variant: DocumentModel.printerId === modelData.id ? "primary" : "default"
                    onClicked: {
                        DocumentModel.setPrinter(modelData.id)
                        Preferences.printerId = modelData.id
                    }
                }
            }
        }

        Segmented {
            visible: root.step === 2
            accessibleName: qsTr("Mouse")
            current: Preferences.mouseStyle
            model: [{ value: "fission", text: "Fission359" }, { value: "fusion", text: "Fusion" }, { value: "tinkercad", text: "Tinkercad" }, { value: "blender", text: "Blender" }, { value: "sketchup", text: "SketchUp" }]
            onActivated: value => Preferences.mouseStyle = value
        }

        Txt {
            visible: root.step === 2
            width: parent.width
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            text: Preferences.mouseStyle === "fusion" || Preferences.mouseStyle === "blender" || Preferences.mouseStyle === "sketchup"
                ? qsTr("The middle mouse button moves or turns the view, the wheel zooms.")
                : qsTr("The right mouse button turns the view, the middle button moves it, the wheel zooms.")
        }

        Txt {
            visible: root.step !== 2
            width: parent.width
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            font.pixelSize: Theme.fontSizeSmall
            text: qsTr("You can change all of this later in the settings.")
        }

        Row {
            spacing: 8

            Btn {
                objectName: "firstRunNext"
                text: root.step === root.lastStep ? qsTr("Let's go") : qsTr("Next")
                variant: "primary"
                onClicked: root.step === root.lastStep ? root.done() : root.step += 1
            }

            Btn {
                objectName: "firstRunLater"
                text: qsTr("Later")
                onClicked: root.done()
            }
        }
    }
}
