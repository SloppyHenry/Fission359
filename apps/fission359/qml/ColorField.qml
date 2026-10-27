// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The colour of a body: the palette of the project as swatches (the filament colours, then the
// own colours). A click chooses a colour, a double click adjusts the swatch for the rest of the
// session of this project; "Own colour…" adds a new swatch. `current` is the colour now
// ("#rrggbb"), empty if the chosen bodies differ.
Item {
    id: root

    property string current: ""
    signal chosen(string color)

    function nameOf(key) {
        switch (key) {
        case "black": return qsTr("Black")
        case "white": return qsTr("White")
        case "grey": return qsTr("Grey")
        case "red": return qsTr("Red")
        case "blue": return qsTr("Blue")
        case "green": return qsTr("Green")
        case "yellow": return qsTr("Yellow")
        case "orange": return qsTr("Orange")
        case "silver": return qsTr("Silver")
        case "natural": return qsTr("Natural")
        case "turquoise": return qsTr("Turquoise")
        case "violet": return qsTr("Violet")
        case "pink": return qsTr("Pink")
        case "brown": return qsTr("Brown")
        default: return key
        }
    }

    function sameColor(a, b) {
        return a !== "" && b !== "" && Qt.colorEqual(a, b)
    }

    // Opens the editor for the swatch at `index`, or for a new one (-1).
    function edit(index) {
        ownPopup.editIndex = index
        ownPopup.picked = index >= 0 ? DocumentModel.palette[index].color
                                     : (root.current !== "" ? root.current : "#D93F36")
        ownPopup.open()
    }

    width: parent ? parent.width : 272
    implicitHeight: swatches.implicitHeight + 8 + ownButton.implicitHeight
    height: implicitHeight

    Flow {
        id: swatches

        width: parent.width
        spacing: 8

        Repeater {
            model: DocumentModel.palette

            delegate: Rectangle {
                id: swatch

                required property var modelData
                required property int index
                readonly property bool selected: root.sameColor(root.current, modelData.color)

                objectName: "swatch_" + modelData.key
                width: 28
                height: 28
                radius: 7
                color: modelData.color
                border.width: selected ? 2 : 1
                border.color: selected ? Theme.accent : Theme.edge
                Accessible.name: root.nameOf(modelData.nameKey)
                Accessible.role: Accessible.Button
                Accessible.onPressAction: root.chosen(modelData.color)

                // A click waits for the double click interval: a double click only adjusts.
                Timer {
                    id: clickTimer

                    interval: Qt.styleHints.mouseDoubleClickInterval
                    onTriggered: root.chosen(swatch.modelData.color)
                }

                MouseArea {
                    id: area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: clickTimer.restart()
                    onDoubleClicked: {
                        clickTimer.stop()
                        root.edit(swatch.index)
                    }
                }

                ToolTip.visible: area.containsMouse
                ToolTip.delay: 500
                ToolTip.text: root.nameOf(modelData.nameKey) + " · " + qsTr("Double-click to adjust")
            }
        }
    }

    Btn {
        id: ownButton

        objectName: "ownColour"
        anchors.top: swatches.bottom
        anchors.topMargin: 8
        small: true
        text: qsTr("Own colour…")
        onClicked: root.edit(-1)
    }

    // Not a dialog: a small popover that stays out of the way and closes with Esc or a click beside.
    Popup {
        id: ownPopup

        property int editIndex: -1 // the swatch that is adjusted, -1 for a new colour
        property color picked: "#D93F36"
        readonly property bool editingOwn: editIndex >= 0 && DocumentModel.palette[editIndex]?.custom === true

        y: ownButton.y + ownButton.height + 6
        width: parent.width
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onAboutToShow: {
            hue.value = Math.max(0, picked.hsvHue)
            sat.value = picked.hsvSaturation
            val.value = picked.hsvValue
        }

        background: Rectangle {
            radius: Theme.radiusFloat
            color: Theme.raise
            border.width: 1
            border.color: Theme.line2
        }

        contentItem: Column {
            spacing: 10

            Rectangle {
                width: parent.width
                height: 28
                radius: 7
                color: Qt.hsva(hue.value, sat.value, val.value, 1)
                border.width: 1
                border.color: Theme.edge
            }

            Repeater {
                model: [
                    { id: "hue", label: qsTr("Tone") },
                    { id: "sat", label: qsTr("Strength") },
                    { id: "val", label: qsTr("Brightness") }
                ]

                delegate: Row {
                    required property var modelData

                    spacing: 8

                    Txt {
                        width: 72
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: Theme.fg3
                    }

                    Slider {
                        id: slider

                        objectName: "own_" + modelData.id
                        width: ownPopup.availableWidth - 80
                        from: 0
                        to: 1
                        value: modelData.id === "hue" ? hue.value : modelData.id === "sat" ? sat.value : val.value
                        onMoved: {
                            if (modelData.id === "hue") hue.value = value
                            else if (modelData.id === "sat") sat.value = value
                            else val.value = value
                        }
                        Accessible.name: modelData.label
                        height: 22
                        hoverEnabled: true

                        // Slim like the other sliders of the interface: a thin rail, a small dot.
                        background: Rectangle {
                            x: slider.leftPadding
                            y: slider.topPadding + slider.availableHeight / 2 - height / 2
                            width: slider.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.field2

                            Rectangle {
                                width: slider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: Theme.accent
                            }
                        }

                        handle: Rectangle {
                            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                            y: slider.topPadding + slider.availableHeight / 2 - height / 2
                            implicitWidth: 14
                            implicitHeight: 14
                            radius: 7
                            color: slider.pressed ? Theme.accent : Theme.fg
                            border.width: slider.visualFocus ? 2 : 1
                            border.color: slider.visualFocus ? Theme.accentLine : Theme.line2
                        }
                    }
                }
            }

            // Wraps when the words are long (German): the buttons stay inside the frame.
            Flow {
                width: parent.width
                spacing: 8

                Btn {
                    objectName: "ownApply"
                    variant: "primary"
                    small: true
                    text: ownPopup.editIndex >= 0 ? qsTr("Keep this colour") : qsTr("Add this colour")
                    onClicked: {
                        const color = Qt.hsva(hue.value, sat.value, val.value, 1).toString()
                        if (ownPopup.editIndex >= 0) {
                            DocumentModel.setPaletteColor(ownPopup.editIndex, color)
                        } else {
                            DocumentModel.addPaletteColor(color)
                            root.chosen(color)
                        }
                        ownPopup.close()
                    }
                }

                Btn {
                    objectName: "ownReset"
                    visible: ownPopup.editIndex >= 0 && !ownPopup.editingOwn
                    small: true
                    text: qsTr("Back to standard")
                    onClicked: {
                        DocumentModel.resetPaletteColor(ownPopup.editIndex)
                        ownPopup.close()
                    }
                }

                Btn {
                    objectName: "ownRemove"
                    visible: ownPopup.editingOwn
                    small: true
                    text: qsTr("Remove")
                    onClicked: {
                        DocumentModel.removePaletteColor(ownPopup.editIndex)
                        ownPopup.close()
                    }
                }
            }
        }

        QtObject { id: hue; property real value: 0 }
        QtObject { id: sat; property real value: 1 }
        QtObject { id: val; property real value: 1 }
    }
}
