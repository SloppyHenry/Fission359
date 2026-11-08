// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The numbers of the element being drawn in a sketch (AP-2.1c1): a small bubble at the pointer
// with the length (line), the width and height (rectangle) or the radius (circle). A digit (or
// minus, comma, point) turns it into number fields: typing replaces the value the pointer made
// (rule 2). Tab goes to the next number, Enter sets the element, Esc takes the typing back.
Item {
    id: root

    objectName: "sketchLayer"
    readonly property bool typing: bubble.typing
    // What floats over the view (tool palette, view cube ...): the bubble keeps out from under it.
    property var obstacles: []
    readonly property var fields: Sketch.readout.valid ? Sketch.readout.fields : []
    // How many numbers there are: the model of the Repeater, so that its fields (one of them has
    // the keyboard) are not made again each time a number changes.
    readonly property int fieldTotal: fields.length
    property int current: 0
    property var inputs: ({})

    function labelOf(name) {
        switch (name) {
        case "length": return qsTr("Length")
        case "angle": return qsTr("Angle")
        case "width": return qsTr("Width")
        case "height": return qsTr("Height")
        case "radius": return qsTr("Radius")
        default: return name
        }
    }

    function unitOf(name) {
        return name === "angle" ? "°" : qsTr("mm")
    }

    // A digit was pressed while an element is pulled: the number fields open with it. With nothing
    // to type (before the first click, three points of a circle) the key is simply taken: the digits
    // are not for the views while a tool is on.
    function startTyping(first) {
        if (bubble.typing || Sketch.fieldCount() === 0) {
            return
        }
        bubble.typing = true
        current = 0
        const input = inputs[0]
        if (input) {
            input.text = first
            input.forceActiveFocus()
            input.cursorPosition = input.text.length
        }
        apply(0)
    }

    function endTyping() {
        const wasTyping = bubble.typing
        bubble.typing = false
        for (const key in inputs) {
            if (inputs[key]) {
                inputs[key].text = ""
                inputs[key].invalid = false
                inputs[key].focus = false
            }
        }
        // The keyboard goes to the layer itself, which keeps no keys: a number field that is
        // hidden must not hold on to it (it would swallow Esc and make the Esc shortcut of the
        // sketch mode wait for "typing" to end).
        if (wasTyping) {
            root.forceActiveFocus()
        }
    }

    // The text of one number goes to the sketch at once (the element follows what is typed);
    // an empty field leaves that number to the pointer.
    function apply(index) {
        const input = inputs[index]
        if (!input) {
            return
        }
        const text = input.text.trim()
        if (text === "" || text === "-") {
            Sketch.setTyped(index, NaN)
            input.invalid = false
            return
        }
        const kind = fields[index] && fields[index].name === "angle" ? "angle" : "length"
        const value = Numbers.evaluate(text, kind)
        if (isNaN(value)) {
            input.errorInfo = Numbers.explain(text, kind)
            input.invalid = true
            Sketch.setTyped(index, NaN)
        } else {
            input.invalid = false
            Sketch.setTyped(index, value)
        }
    }

    function next() {
        const count = fields.length
        if (count < 2) {
            return
        }
        current = (current + 1) % count
        if (inputs[current]) {
            inputs[current].forceActiveFocus()
            inputs[current].selectAll()
        }
    }

    function accept() {
        for (let i = 0; i < fields.length; ++i) {
            if (inputs[i] && inputs[i].text.trim() !== "" && !inputs[i].check()) {
                return // the line under the field says what is wrong
            }
        }
        Sketch.acceptTyped()
    }

    function cancel() {
        Sketch.clearTyped()
        endTyping()
    }

    // Developer option --simulate "type:...": the same calls the keys make.
    function simulate(tokens) {
        for (const token of tokens) {
            if (token === "enter") {
                accept()
            } else if (token === "esc") {
                cancel()
            } else if (token === "tab") {
                next()
            } else if (!bubble.typing) {
                startTyping(token)
            } else if (inputs[current]) {
                inputs[current].text = token
                apply(current)
            }
        }
    }

    Connections {
        target: Sketch

        // The element is set (by Enter or by a click), or the tool changed: the typing is over.
        function onElementDone() { root.endTyping() }
        function onToolChanged() { root.endTyping() }
    }

    Rectangle {
        id: bubble

        property bool typing: false

        visible: Sketch.drawing && Sketch.readout.valid && root.fields.length > 0
        readonly property point wanted: Qt.point(
            Math.max(0, Math.min(root.width - width, (Sketch.readout.x || 0) + 16)),
            Math.max(0, Math.min(root.height - height, (Sketch.readout.y || 0) + 16)))
        x: wanted.x
        y: wanted.y
        width: bubbleRow.implicitWidth + 20
        height: 30
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: typing ? Theme.accentLine : Theme.line2
        z: 10

        onVisibleChanged: if (!visible) typing = false

        Row {
            id: bubbleRow

            anchors.centerIn: parent
            spacing: 10

            Repeater {
                model: root.fieldTotal

                delegate: Row {
                    id: entry

                    required property int index
                    readonly property var field: root.fields[index] !== undefined
                                                     ? root.fields[index]
                                                     : ({ name: "", value: 0, typed: false })

                    spacing: 4

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: bubble.typing
                        text: root.labelOf(entry.field.name)
                        color: entry.index === root.current ? Theme.fg2 : Theme.fg4
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideNone
                    }

                    // While the pointer decides: the number it makes.
                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !bubble.typing
                        text: Numbers.format(entry.field.value, entry.field.name === "angle" ? 1 : 2)
                              + (entry.field.name === "angle" ? " °" : " " + qsTr("mm"))
                        mono: true
                        color: entry.field.typed ? Theme.accentText : Theme.fg
                        font.weight: Font.Medium
                        elide: Text.ElideNone
                    }

                    // While typing: the field (empty = the pointer decides).
                    ValueInput {
                        id: input

                        visible: bubble.typing
                        anchors.verticalCenter: parent.verticalCenter
                        kind: entry.field.name === "angle" ? "angle" : "length"
                        font.pixelSize: Theme.fontSize
                        Accessible.name: root.labelOf(entry.field.name)
                        Component.onCompleted: root.inputs[entry.index] = input
                        Component.onDestruction: root.inputs[entry.index] = null

                        onTextEdited: root.apply(entry.index)
                        onActiveFocusChanged: if (activeFocus) root.current = entry.index
                        Keys.onTabPressed: root.next()
                        Keys.onReturnPressed: event => { if (bubble.typing) { root.accept() } else { event.accepted = false } }
                        Keys.onEnterPressed: event => { if (bubble.typing) { root.accept() } else { event.accepted = false } }
                        Keys.onEscapePressed: event => { if (bubble.typing) { root.cancel() } else { event.accepted = false } }
                    }

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: bubble.typing
                        text: root.unitOf(entry.field.name)
                        color: Theme.fg4
                        font.pixelSize: Theme.fontSizeSmall
                        mono: true
                        elide: Text.ElideNone
                    }
                }
            }
        }
    }
}
