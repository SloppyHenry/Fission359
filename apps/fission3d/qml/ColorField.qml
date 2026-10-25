// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The colour of a body: the filament colours as swatches, and "Own colour…" for any other.
// `current` is the colour now ("#rrggbb"), empty if the chosen bodies differ.
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
        default: return qsTr("Natural")
        }
    }

    function sameColor(a, b) {
        return a !== "" && b !== "" && Qt.colorEqual(a, b)
    }

    // Opens the popover for an own colour (also called by the context bar).
    function openOwn() {
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
            model: Theme.filaments

            delegate: Rectangle {
                id: swatch

                required property var modelData
                readonly property bool selected: root.sameColor(root.current, modelData.color.toString())

                objectName: "swatch_" + modelData.key
                width: 28
                height: 28
                radius: 7
                color: modelData.color
                border.width: selected ? 2 : 1
                border.color: selected ? Theme.accent : Theme.edge
                Accessible.name: root.nameOf(modelData.key)
                Accessible.role: Accessible.Button
                Accessible.onPressAction: root.chosen(modelData.color.toString())

                MouseArea {
                    id: area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.chosen(swatch.modelData.color.toString())
                }

                ToolTip.visible: area.containsMouse
                ToolTip.delay: 500
                ToolTip.text: root.nameOf(modelData.key)
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
        onClicked: ownPopup.open()
    }

    // Not a dialog: a small popover that stays out of the way and closes with Esc or a click beside.
    Popup {
        id: ownPopup

        property color picked: root.current !== "" ? root.current : "#D93F36"

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
                    }
                }
            }

            Btn {
                objectName: "ownApply"
                variant: "primary"
                small: true
                text: qsTr("Use this colour")
                onClicked: {
                    root.chosen(Qt.hsva(hue.value, sat.value, val.value, 1).toString())
                    ownPopup.close()
                }
            }
        }

        QtObject { id: hue; property real value: 0 }
        QtObject { id: sat; property real value: 1 }
        QtObject { id: val; property real value: 1 }
    }
}
