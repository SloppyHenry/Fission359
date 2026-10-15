// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The one number field of the program (master prompt, rule 2): typing replaces the value,
// formulas, units and both decimal separators work. `value` is in millimetres (or degrees).
Item {
    id: root

    property string label: ""
    property real value: 0
    property string unit: "mm"
    property string kind: "length" // length | angle | plain
    property string axis: ""       // "", "x", "y" or "z": a coloured letter in front
    property int decimals: 3
    property string note: ""
    property string expression: ""  // the formula the value came from ("" = typed as a plain number)
    property bool invalid: false
    property bool readOnly: false
    readonly property bool editing: input.activeFocus
    signal edited(real newValue)
    signal textCommitted(string text) // what was typed, when it differs from what was shown

    implicitWidth: 96
    implicitHeight: column.implicitHeight

    function shownText() {
        return root.expression !== "" ? root.expression : Numbers.format(root.value, root.decimals)
    }

    function refresh() {
        input.text = shownText()
        input.pendingEdit = false
    }

    // A value that changes from outside (undo, a formula) shows at once, unless the person is
    // in the middle of typing here.
    onExpressionChanged: if (!input.pendingEdit) refresh()

    onValueChanged: if (!input.pendingEdit) refresh()
    Component.onCompleted: refresh()
    Column {
        id: column

        width: parent.width
        spacing: 5

        Txt {
            visible: root.label !== ""
            width: parent.width
            text: root.label
            color: Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
        }

        Rectangle {
            id: box

            width: parent.width
            height: Theme.controlHeight
            radius: Theme.radius
            color: Theme.field
            border.width: 1
            border.color: root.invalid ? Theme.bad : input.activeFocus ? Theme.accentLine : Theme.line

            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: Theme.radius + 3
                color: "transparent"
                border.width: 3
                border.color: root.invalid ? Theme.badSoft
                            : Qt.rgba(Theme.accentLine.r, Theme.accentLine.g, Theme.accentLine.b, 0.22)
                visible: input.activeFocus || root.invalid
                z: -1
            }

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Rectangle {
                    visible: root.axis !== ""
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    radius: 4
                    color: root.axis === "x" ? Theme.axisX : root.axis === "y" ? Theme.axisY : Theme.axisZ

                    Txt {
                        anchors.centerIn: parent
                        text: root.axis.toUpperCase()
                        mono: true
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }

                TextInput {
                    id: input

                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - (root.axis !== "" ? 24 : 0) - unitText.width - 6
                    clip: true
                    readOnly: root.readOnly
                    selectByMouse: true
                    color: Theme.fg
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.accentFg
                    font.family: Theme.monoFamilies[0]
                    font.pixelSize: Theme.fontSize
                    font.weight: Font.Medium
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    // True from the first key typed until the value is accepted or dropped. The
                    // window leaves Ctrl+Z to the field only while this is true; the field's own
                    // undo list is not used for that (it keeps its entries after Enter).
                    property bool pendingEdit: false
                    Accessible.name: root.label !== "" ? root.label : root.axis.toUpperCase()

                    onActiveFocusChanged: {
                        if (activeFocus) {
                            selectAll() // typing replaces the value
                        } else {
                            root.commit()
                        }
                    }
                    onAccepted: root.commit()
                    Keys.onEscapePressed: {
                        root.invalid = false
                        root.refresh()
                        focus = false
                    }
                    onTextEdited: {
                        root.invalid = false
                        pendingEdit = true
                    }
                }

                Txt {
                    id: unitText

                    anchors.verticalCenter: parent.verticalCenter
                    text: root.unit
                    mono: true
                    color: Theme.fg4
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        Txt {
            visible: root.note !== "" || root.invalid
            width: parent.width
            text: root.invalid ? qsTr("That is not a number or formula.") : root.note
            color: root.invalid ? Theme.bad : Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }

    function commit() {
        if (root.readOnly) {
            return
        }
        const result = Numbers.evaluate(input.text, root.kind)
        if (isNaN(result)) {
            if (input.text.trim() === "") {
                root.invalid = false
                refresh()
            } else {
                root.invalid = true
            }
            return
        }
        root.invalid = false
        const typed = input.text.trim()
        const changedText = typed !== shownText()
        if (result !== root.value) {
            root.value = result
            root.edited(result)
        }
        if (changedText) {
            root.textCommitted(typed)
        }
        refresh()
    }
}
