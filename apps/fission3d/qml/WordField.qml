// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// A field for words (the text of a lettering): the look of NumberField, no numbers. The words
// are accepted with Enter or when the field loses the keyboard; Esc drops what was typed.
Item {
    id: root

    property string label: ""
    property string value: ""
    property bool invalid: false
    readonly property alias input: input
    signal committed(string text)

    implicitWidth: 200
    implicitHeight: column.implicitHeight

    function focusAndSelect() {
        input.forceActiveFocus()
        input.selectAll()
    }

    function refresh() {
        input.text = root.value
        input.pendingEdit = false
    }

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
            width: parent.width
            height: Theme.controlHeight
            radius: Theme.radius
            color: Theme.field
            border.width: 1
            border.color: root.invalid ? Theme.bad : input.activeFocus ? Theme.accentLine : Theme.line

            TextInput {
                id: input

                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                verticalAlignment: TextInput.AlignVCenter
                clip: true
                selectByMouse: true
                color: Theme.fg
                selectionColor: Theme.accent
                selectedTextColor: Theme.accentFg
                font.family: Theme.fontFamilies[0]
                font.pixelSize: Theme.fontSize
                // True from the first key until the words are accepted or dropped (see NumberField).
                property bool pendingEdit: false
                Accessible.name: root.label

                onActiveFocusChanged: {
                    if (activeFocus) {
                        selectAll() // typing replaces the words
                    } else {
                        root.commit()
                    }
                }
                onAccepted: root.commit()
                onTextEdited: pendingEdit = true
                Keys.onEscapePressed: {
                    root.refresh()
                    focus = false
                }
            }
        }
    }

    function commit() {
        const typed = input.text
        input.pendingEdit = false
        if (typed !== root.value) {
            root.committed(typed)
        }
    }
}
