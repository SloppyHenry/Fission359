// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// A small card above the 3D view that says what happened and offers the way on (rule 5: errors
// are help). It never blocks the work behind it, so it is no dialog. Used for files that could
// not be saved or opened and for the rescued work after a crash.
//
//   card.show("What happened", [{ id: "retry", text: "Try again", primary: true }, ...])
Float {
    id: root

    property string text: ""
    property var actions: []
    property var payload: null   // whatever the caller wants back with the chosen action

    signal chosen(string id, var payload)

    function show(message, buttons, data) {
        text = message
        actions = buttons
        payload = data === undefined ? null : data
        visible = true
    }

    function dismiss() {
        visible = false
    }

    visible: false
    width: Math.min(560, parent.width - 32)
    height: content.implicitHeight + 28
    Accessible.role: Accessible.AlertMessage
    Accessible.name: text

    Column {
        id: content

        x: 16
        y: 14
        width: parent.width - 32
        spacing: 12

        Txt {
            width: parent.width
            text: root.text
            color: Theme.fg
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Row {
            spacing: 8

            Repeater {
                model: root.actions

                Btn {
                    required property var modelData

                    text: modelData.text
                    variant: modelData.primary === true ? "primary" : "default"
                    onClicked: {
                        const data = root.payload
                        root.visible = false
                        root.chosen(modelData.id, data)
                    }
                }
            }
        }
    }
}
