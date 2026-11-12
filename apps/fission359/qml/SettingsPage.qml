// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The settings page: a panel in the window (no modal dialog). Everything changes at once and is
// kept for the next start; nothing here is a step of the history.
Float {
    id: root

    signal closeRequested
    signal printerRequested

    readonly property var accentChoices: ["#2563d9", "#7c3aed", "#0d9488", "#ea580c", "#db2777"]
    readonly property string accentNow: Preferences.accentColor !== "" ? Preferences.accentColor : "#2563d9"
    readonly property string accentWarning: Preferences.accentWarning(accentNow)

    width: Math.min(560, parent.width - 32)
    height: Math.min(parent.height - 32, scroll.contentHeight + 76)
    Accessible.name: qsTr("Settings")

    Txt {
        id: heading

        x: 20
        y: 16
        text: qsTr("Settings")
        color: Theme.fg
        font.pixelSize: Theme.fontSizeTitle
        font.weight: Font.DemiBold
    }

    IconBtn {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        iconName: "close"
        tip: qsTr("Close")
        onClicked: root.closeRequested()
    }

    Flickable {
        id: scroll

        anchors.fill: parent
        anchors.topMargin: 52
        anchors.bottomMargin: 8
        contentWidth: width
        contentHeight: column.implicitHeight + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column

            x: 20
            width: scroll.width - 40
            spacing: 10

            component Heading: Txt {
                color: Theme.fg
                font.weight: Font.DemiBold
                topPadding: 8
            }

            component Label: Txt {
                font.pixelSize: Theme.fontSizeSmall
            }

            Heading { text: qsTr("General") }

            Label { text: qsTr("Language") }
            Segmented {
                accessibleName: qsTr("Language")
                current: Preferences.language
                model: [{ value: "system", text: qsTr("Like the computer") }, { value: "de", text: "Deutsch" }, { value: "en", text: "English" }]
                onActivated: value => Preferences.language = value
            }

            Label { text: qsTr("Unit for new projects") }
            Segmented {
                accessibleName: qsTr("Unit")
                current: Preferences.units
                model: [{ value: "mm", text: qsTr("Millimetres") }, { value: "in", text: qsTr("Inches") }]
                onActivated: value => Preferences.units = value
            }

            Label { text: qsTr("Decimal places shown") }
            Segmented {
                accessibleName: qsTr("Decimal places")
                current: Preferences.decimals
                model: [{ value: 0, text: "0" }, { value: 1, text: "1" }, { value: 2, text: "2" }, { value: 3, text: "3" }, { value: 4, text: "4" }]
                onActivated: value => Preferences.decimals = value
            }

            Toggle {
                text: qsTr("Save automatically in the background")
                checked: Preferences.autosave
                onToggled: Preferences.autosave = checked
            }

            Segmented {
                visible: Preferences.autosave
                accessibleName: qsTr("Autosave interval")
                current: Preferences.autosaveMinutes
                model: [{ value: 1, text: qsTr("every minute") }, { value: 5, text: qsTr("every 5 minutes") }, { value: 10, text: qsTr("every 10 minutes") }]
                onActivated: value => Preferences.autosaveMinutes = value
            }

            Heading { text: qsTr("Look") }

            Label { text: qsTr("Design") }
            Segmented {
                accessibleName: qsTr("Design")
                current: Preferences.theme
                model: [{ value: "system", text: qsTr("Like the computer") }, { value: "light", text: qsTr("Light") }, { value: "dark", text: qsTr("Dark") }]
                onActivated: value => Preferences.theme = value
            }

            Label { text: qsTr("Accent colour") }
            Row {
                spacing: 8

                Repeater {
                    model: root.accentChoices

                    Rectangle {
                        required property string modelData

                        width: 28
                        height: 28
                        radius: 14
                        color: modelData
                        border.width: root.accentNow.toLowerCase() === modelData ? 3 : 1
                        border.color: root.accentNow.toLowerCase() === modelData ? Theme.fg : Theme.line
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData
                        MouseArea {
                            anchors.fill: parent
                            onClicked: Preferences.accentColor = parent.modelData === "#2563d9" ? "" : parent.modelData
                        }
                    }
                }

                TextField {
                    id: ownColour

                    width: 100
                    height: 28
                    placeholderText: qsTr("#rrggbb")
                    text: Preferences.accentColor
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.fg
                    background: Rectangle { radius: Theme.radius; color: Theme.field; border.width: 1; border.color: Theme.line }
                    onEditingFinished: if (/^#[0-9a-fA-F]{6}$/.test(text)) Preferences.accentColor = text
                    Accessible.name: qsTr("Own accent colour")
                }
            }

            Txt {
                visible: root.accentWarning !== ""
                width: parent.width
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                color: Theme.amberFg
                font.pixelSize: Theme.fontSizeSmall
                text: root.accentWarning === "red" ? qsTr("This colour looks like an error message (red). It works, but you might take buttons for warnings.")
                    : root.accentWarning === "yellow" ? qsTr("This colour looks like a warning (yellow). It works, but you might take buttons for warnings.")
                    : qsTr("This colour looks like “all fine” (green). It works, but you might take buttons for confirmations.")
            }

            Label { text: qsTr("Size of the interface") }
            Segmented {
                accessibleName: qsTr("Size of the interface")
                current: Preferences.uiScale
                model: [{ value: 80, text: "80 %" }, { value: 100, text: "100 %" }, { value: 125, text: "125 %" }, { value: 150, text: "150 %" }, { value: 200, text: "200 %" }]
                onActivated: value => Preferences.uiScale = value
            }

            Heading { text: qsTr("Working") }

            Label { text: qsTr("Mouse") }
            Segmented {
                accessibleName: qsTr("Mouse")
                current: Preferences.mouseStyle
                model: [{ value: "fission", text: "Fission359" }, { value: "fusion", text: "Fusion" }, { value: "tinkercad", text: "Tinkercad" }, { value: "blender", text: "Blender" }, { value: "sketchup", text: "SketchUp" }]
                onActivated: value => Preferences.mouseStyle = value
            }

            Label { text: qsTr("Handles") }
            Segmented {
                accessibleName: qsTr("Handles")
                current: Preferences.handleStyle
                model: [{ value: "axes", text: qsTr("Arrows along the axes") }, { value: "classic", text: qsTr("Classic (squares)") }]
                onActivated: value => Preferences.handleStyle = value
            }

            Heading { text: qsTr("Printer") }

            Row {
                spacing: 12

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.fg
                    text: DocumentModel.printerName !== "" ? DocumentModel.printerName : qsTr("My printer")
                }

                Btn {
                    text: qsTr("Choose another…")
                    onClicked: root.printerRequested()
                }
            }

            Row {
                topPadding: 12
                spacing: 8

                Btn {
                    text: qsTr("Back to the defaults")
                    onClicked: Preferences.resetAll()
                }

                Btn {
                    text: qsTr("Close")
                    variant: "primary"
                    onClicked: root.closeRequested()
                }
            }
        }
    }
}
