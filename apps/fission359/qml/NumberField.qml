// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

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
    // Dragging the label changes the value (1 px = 0,1; Shift ×10, Alt ×0,1). A drag is one
    // change: the owner opens a transaction on `dragStarted` and closes it on `dragFinished`.
    property bool draggable: false
    readonly property bool editing: input.activeFocus
    property var errorInfo: ({}) // why the typed text is wrong, from Numbers.explain
    signal edited(real newValue)
    signal textCommitted(string text) // what was typed, when it differs from what was shown
    signal dragStarted()
    signal dragFinished(bool accepted)

    implicitWidth: 96
    implicitHeight: column.implicitHeight

    // The simulation of the window tests uses this to put the cursor in a field.
    function takeFocus() {
        input.forceActiveFocus()
    }

    function shownText() {
        return root.expression !== "" ? root.expression : Numbers.format(root.value, root.decimals)
    }

    function refresh() {
        input.text = shownText()
        input.pendingEdit = false
        // A narrow field (the three of the position) shows the start of a long value: the sign
        // of "-23,17" must not scroll out of sight, so that it reads as 23,17.
        if (!input.activeFocus) {
            input.cursorPosition = 0
        }
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
            id: labelText

            visible: root.label !== ""
            width: parent.width
            text: root.label
            color: Theme.fg4
            font.pixelSize: Theme.fontSizeSmall

            MouseArea {
                anchors.fill: parent
                enabled: root.draggable && !root.readOnly && !input.pendingEdit
                cursorShape: enabled ? Qt.SizeHorCursor : Qt.ArrowCursor
                onPressed: mouse => { forceActiveFocus(); root.beginDrag(mouse.x) }
                onPositionChanged: mouse => root.dragTo(mouse.x, mouse.modifiers)
                onReleased: root.endDrag(true)
                onCanceled: root.endDrag(false)
                Keys.onEscapePressed: root.endDrag(false)
            }
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

                    MouseArea {
                        anchors.fill: parent
                        enabled: root.draggable && !root.readOnly && !input.pendingEdit
                        cursorShape: enabled ? Qt.SizeHorCursor : Qt.ArrowCursor
                        onPressed: mouse => { forceActiveFocus(); root.beginDrag(mapToItem(root, mouse.x, 0).x) }
                        onPositionChanged: mouse =>
                            root.dragTo(mapToItem(root, mouse.x, 0).x, mouse.modifiers)
                        onReleased: root.endDrag(true)
                        onCanceled: root.endDrag(false)
                        Keys.onEscapePressed: root.endDrag(false)
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
                        root.errorInfo = ({})
                        root.refresh()
                        focus = false
                    }
                    onTextEdited: {
                        root.invalid = false
                        root.errorInfo = ({})
                        pendingEdit = true
                    }
                    // Option+arrow and Cmd+arrow are cursor moves of a text field (to the end of
                    // the line): claim them, or the field never sees the key.
                    Keys.onShortcutOverride: event => {
                        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                            event.accepted = true
                        }
                    }
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                            root.step(event.key === Qt.Key_Up ? 1 : -1, event.modifiers)
                            event.accepted = true
                        }
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
            text: root.invalid ? root.explanation() : root.note
            color: root.invalid ? Theme.bad : Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone

            // "Did you mean ...?" is a button: a click puts the known name in place.
            MouseArea {
                anchors.fill: parent
                enabled: root.invalid && root.suggestion() !== ""
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.applySuggestion()
            }
        }
    }

    // The wrong part of the text gets a red line under it.
    Rectangle {
        visible: root.invalid && root.errorInfo.code !== undefined && underlineEnd > underlineStart
        readonly property real underlineStart: root.errorInfo.code === undefined ? 0
            : input.positionToRectangle(root.errorInfo.start).x
        readonly property real underlineEnd: root.errorInfo.code === undefined ? 0
            : input.positionToRectangle(Math.max(root.errorInfo.end, root.errorInfo.start + 1)).x
        x: input.mapToItem(root, underlineStart, 0).x
        y: input.mapToItem(root, 0, input.height).y - 3
        width: Math.max(underlineEnd - underlineStart, 4)
        height: 2
        radius: 1
        color: Theme.bad
    }

    // --- arrow keys -----------------------------------------------------------------------

    // One step with an arrow key: 1, with Shift 10, with Alt (or Cmd) 0,1 (mm or degrees). A formula
    // stays a formula (`Breite/2` becomes `(Breite/2) + 1`).
    function step(direction, modifiers) {
        if (root.readOnly) {
            return
        }
        const size = (modifiers & Qt.ShiftModifier) ? 10 : (modifiers & (Qt.AltModifier | Qt.ControlModifier)) ? 0.1 : 1
        const moved = Numbers.adjusted(input.text, root.kind, direction * size)
        if (moved === "") {
            return // the text is not a formula (yet): the line below the field says why
        }
        input.text = moved
        input.pendingEdit = true
        commit()
        input.selectAll()
    }

    // --- dragging the label ---------------------------------------------------------------

    property real dragOrigin: 0
    property string dragText: ""
    property bool dragging: false

    function beginDrag(x) {
        root.dragOrigin = x
        root.dragText = shownText()
        root.dragging = true
        root.dragStarted()
    }

    function dragTo(x, modifiers) {
        if (!root.dragging) {
            return
        }
        const per = (modifiers & Qt.ShiftModifier) ? 1 : (modifiers & Qt.AltModifier) ? 0.01 : 0.1
        const delta = Math.round((x - root.dragOrigin) * per * 100) / 100
        const moved = Numbers.adjusted(root.dragText, root.kind, delta)
        if (moved !== "") {
            input.text = moved
            input.pendingEdit = true
            commit()
        }
    }

    function endDrag(accepted) {
        if (!root.dragging) {
            return
        }
        root.dragging = false
        root.dragFinished(accepted)
    }

    // --- what is wrong --------------------------------------------------------------------

    function suggestion() {
        return root.errorInfo.suggestion === undefined ? "" : root.errorInfo.suggestion
    }

    function applySuggestion() {
        const info = root.errorInfo
        if (info.suggestion === undefined || info.suggestion === "") {
            return
        }
        input.text = input.text.substring(0, info.start) + info.suggestion
                   + input.text.substring(info.end)
        input.pendingEdit = true
        commit()
    }

    function explanation() {
        const info = root.errorInfo
        const word = info.subject === undefined ? "" : info.subject
        const guess = suggestion()
        switch (info.code) {
        case "unknownName":
            return guess !== "" ? qsTr("I don't know “%1”. Did you mean “%2”?").arg(word).arg(guess)
                                : qsTr("I don't know “%1”.").arg(word)
        case "unknownFunction":
            return guess !== "" ? qsTr("I don't know the function “%1”. Did you mean “%2”?").arg(word).arg(guess)
                                : qsTr("I don't know the function “%1”.").arg(word)
        case "unknownUnit":
            return guess !== "" ? qsTr("I don't know the unit “%1”. Did you mean “%2”?").arg(word).arg(guess)
                                : qsTr("I don't know the unit “%1”.").arg(word)
        case "missingClosingBracket":
            return qsTr("A closing bracket is missing.")
        case "unexpectedEnd":
            return qsTr("The formula ends too early.")
        case "unexpectedCharacter":
            return qsTr("This character does not belong here.")
        case "divisionByZero":
            return qsTr("Dividing by zero is not possible.")
        case "incompatibleQuantities":
            return qsTr("Lengths and angles cannot be added.")
        case "wrongKind":
            return root.kind === "angle" ? qsTr("An angle belongs here, not a length.")
                 : root.kind === "plain" ? qsTr("A plain number belongs here.")
                 : qsTr("A length belongs here, not an angle.")
        case "unitNotAllowed":
            return qsTr("This number already has a unit.")
        case "wrongArgumentCount":
            return qsTr("This function has the wrong number of values.")
        case "outOfRange":
            return qsTr("This cannot be calculated (for example the root of a negative number).")
        case "notFinite":
            return qsTr("The result is too large.")
        default:
            return qsTr("That is not a number or formula.")
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
                root.errorInfo = Numbers.explain(input.text, root.kind)
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
