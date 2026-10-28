// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The precision of snapping, small enough for the status bar: "Auto" (the grid follows the zoom),
// a slider through the usual steps from 0.01 mm to 10 mm, and a field for any other number
// (formulas, units and both decimal separators work, as in every number field).
Row {
    id: root

    /// The step in use (mm), also while it is automatic.
    property real step: 1
    property bool automatic: true
    readonly property var notches: [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10]
    readonly property real leastStep: 0.01
    readonly property real largestStep: 100
    signal stepChosen(real step)
    signal automaticChosen()

    spacing: 8

    // Where on the slider (0 to 9) a step lies: three notches to a power of ten (1, 2, 5).
    function positionOf(value) {
        return Math.max(0, Math.min(notches.length - 1, 3 * Math.log(value / leastStep) / Math.LN10))
    }

    // "0,5", "0,05", "2,5", "10": up to three decimals, without trailing zeros.
    function stepText(value) {
        const point = Qt.locale().decimalPoint
        let text = Number(value).toLocaleString(Qt.locale(), "f", 3)
        if (text.indexOf(point) >= 0) {
            text = text.replace(/0+$/, "")
            if (text.endsWith(point)) {
                text = text.slice(0, -1)
            }
        }
        return text
    }

    Txt {
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Grid")
        color: Theme.fg3
        font.pixelSize: Theme.fontSizeSmall
        elide: Text.ElideNone
    }

    // "Auto": on while the grid follows the zoom. Clicking it when on keeps the step it has now.
    AbstractButton {
        id: auto

        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: autoText.implicitWidth + 14
        implicitHeight: 20
        hoverEnabled: true
        checkable: false
        Accessible.role: Accessible.CheckBox
        Accessible.name: qsTr("Grid follows the zoom")
        Accessible.checked: root.automatic
        onClicked: root.automatic ? root.stepChosen(root.step) : root.automaticChosen()

        contentItem: Txt {
            id: autoText

            text: qsTr("Auto")
            color: root.automatic ? Theme.accentText : Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideNone
        }

        background: Rectangle {
            radius: 5
            color: root.automatic ? Theme.accentSoft : (auto.hovered ? Theme.field : "transparent")
            border.width: 1
            border.color: root.automatic ? Theme.accentLine : Theme.line

            Focusring {
                visible: auto.visualFocus
            }
        }
    }

    Slider {
        id: slider

        anchors.verticalCenter: parent.verticalCenter
        width: 104
        height: 22
        from: 0
        to: root.notches.length - 1
        stepSize: 1
        snapMode: Slider.SnapAlways
        opacity: root.automatic ? 0.55 : 1.0
        hoverEnabled: true
        Accessible.name: qsTr("Precision of snapping")
        Accessible.description: root.stepText(root.step) + " mm"

        // Not a binding: the person's hand moves it, and it goes back to the step when let go.
        function follow() {
            if (!pressed) {
                value = root.positionOf(root.step)
            }
        }
        Component.onCompleted: follow()
        onPressedChanged: follow()
        onMoved: root.stepChosen(root.notches[Math.round(value)])

        Connections {
            target: root

            function onStepChanged() {
                slider.follow()
            }
        }

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

            Repeater {
                model: root.notches.length

                Rectangle {
                    required property int index

                    x: index / (root.notches.length - 1) * (parent.width - 1)
                    y: 5
                    width: 1
                    height: index % 3 === 0 ? 4 : 2
                    color: Theme.line2
                }
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

    // Any other number: Enter or leaving the field accepts it, Esc drops it.
    Rectangle {
        id: field

        anchors.verticalCenter: parent.verticalCenter
        width: 74
        height: 22
        radius: 5
        color: Theme.field
        border.width: 1
        border.color: input.activeFocus ? Theme.accentLine : Theme.line

        Row {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 4

            ValueInput {
                id: input

                kind: "length"
                compact: true
                problemAbove: true
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - unit.width - 4
                selectByMouse: true
                text: root.stepText(root.step)
                Accessible.name: qsTr("Grid step in millimetres")

                function commit() {
                    const result = Numbers.evaluate(text, "length")
                    if (!isNaN(result) && result > 0) {
                        const wanted = Math.max(root.leastStep, Math.min(root.largestStep, result))
                        if (wanted !== root.step || root.automatic) {
                            root.stepChosen(wanted)
                        }
                    }
                    text = Qt.binding(() => root.stepText(root.step))
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        selectAll() // typing replaces the value
                    } else {
                        commit()
                    }
                }
                onAccepted: {
                    if (!check()) {
                        return // stays in the field, the reason is shown above it
                    }
                    commit()
                    focus = false
                }
                Keys.onEscapePressed: {
                    text = Qt.binding(() => root.stepText(root.step))
                    focus = false
                }
            }

            Txt {
                id: unit

                anchors.verticalCenter: parent.verticalCenter
                text: "mm"
                mono: true
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }
        }
    }
}
