// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission3D.Ui

// The size handles of the chosen shape, drawn over the 3D view (handle style "Axes", AP-1.5):
// a dot for every size, with an arrow along the way it moves. Drag a dot, or type: digits replace
// the value, Tab goes to the next size, Enter accepts, Esc leaves everything as it was. While a
// value changes, a bubble at the dot shows it (docs/02-ux-konzept.md §2.2).
Item {
    id: root

    // The sentence for a value ("Width"): the window knows the words.
    property var labelFor: name => name
    // Unit of a value, shown behind it.
    property string unit: "mm"
    readonly property bool typing: bubble.typing
    objectName: "handleLayer"
    readonly property bool busy: Handles.editing

    function startTyping(first) {
        if (Handles.handles.length === 0) {
            return
        }
        if (Handles.editing) {
            // A handle is being dragged: typing takes over from the pointer.
            if (!bubble.typing && bubble.index >= 0 && Handles.typeInsteadOfDrag()) {
                bubble.continueAsTyping(first)
            }
            return
        }
        bubble.begin(0, first)
    }

    // Developer option --simulate "type:50,tab,20,tab,4,enter": the same calls the keys make.
    function simulate(tokens) {
        for (const token of tokens) {
            if (token === "tab") {
                bubble.step(1)
            } else if (token === "backtab") {
                bubble.step(-1)
            } else if (token === "enter") {
                input.finish()
            } else if (token === "esc") {
                Handles.cancelEdit()
                bubble.hide()
            } else if (!bubble.typing) {
                startTyping(token)
            } else {
                input.text = token
                bubble.accept()
            }
        }
    }

    function currentHandle() {
        return bubble.index >= 0 && bubble.index < Handles.handles.length ? Handles.handles[bubble.index] : null
    }

    // One measure band with its label and its handle for every value. The delegates stay: the list
    // of handles is made anew whenever a value changes, and a handle that was rebuilt with it
    // would be gone from under the pointer in the middle of a drag. The model is only the number
    // of handles; each one reads its own entry.
    Repeater {
        model: Handles.handles.length

        delegate: Item {
            id: part

            required property int index
            readonly property var handle: index < Handles.handles.length ? Handles.handles[index] : null
            readonly property bool active: Handles.editing && bubble.index === index
            readonly property color tone: !handle ? Theme.accentLine
                : handle.axis === "x" ? Theme.axisX
                : handle.axis === "y" ? Theme.axisY
                : handle.axis === "z" ? Theme.axisZ : Theme.accentLine
            // A held handle stays in the view, even when the shape is pulled far beyond it: a
            // handle that disappears under the pointer ends the drag (the pointer loses it).
            readonly property real dotX: handle ? (active ? Math.min(Math.max(handle.x, 10), root.width - 10) : handle.x) : 0
            readonly property real dotY: handle ? (active ? Math.min(Math.max(handle.y, 10), root.height - 10) : handle.y) : 0
            readonly property real fromX: handle ? handle.fromX : 0
            readonly property real fromY: handle ? handle.fromY : 0
            readonly property real dirX: handle ? handle.dirX : 0
            readonly property real dirY: handle ? handle.dirY : 0
            readonly property real normalX: -dirY
            readonly property real normalY: dirX
            readonly property real length: Math.hypot(dotX - fromX, dotY - fromY)
            // The label is where the controller made room for it.
            readonly property real chipX: handle ? Math.min(Math.max(handle.chipX, 60), root.width - 60) : 0
            readonly property real chipY: handle ? Math.min(Math.max(handle.chipY, 16), root.height - 16) : 0

            width: root.width
            height: root.height
            visible: handle !== null && (active || (dotX > -40 && dotX < root.width + 40
                                                    && dotY > -40 && dotY < root.height + 40))

            // The band, as in a drawing: a line beside the shape, lines from its ends to the
            // points it measures, a stroke across each end.
            Shape {
                anchors.fill: parent
                visible: part.handle !== null

                ShapePath {
                    strokeColor: part.tone
                    strokeWidth: 1
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [3, 3]
                    fillColor: "transparent"
                    startX: part.handle ? part.handle.startPointX : 0
                    startY: part.handle ? part.handle.startPointY : 0
                    PathLine { x: part.fromX; y: part.fromY }
                    PathMove { x: part.handle ? part.handle.pointX : 0; y: part.handle ? part.handle.pointY : 0 }
                    PathLine { x: part.dotX; y: part.dotY }
                }

                ShapePath {
                    strokeColor: part.tone
                    strokeWidth: 2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: part.fromX
                    startY: part.fromY
                    PathLine { x: part.dotX; y: part.dotY }
                    PathMove { x: part.fromX + part.normalX * 5; y: part.fromY + part.normalY * 5 }
                    PathLine { x: part.fromX - part.normalX * 5; y: part.fromY - part.normalY * 5 }
                }
            }

            // The arrow tip beyond the handle.
            Shape {
                anchors.fill: parent

                ShapePath {
                    strokeColor: part.tone
                    strokeWidth: 2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: part.dotX + part.dirX * 10 - part.dirY * 5
                    startY: part.dotY + part.dirY * 10 + part.dirX * 5
                    PathLine { x: part.dotX + part.dirX * 16; y: part.dotY + part.dirY * 16 }
                    PathLine { x: part.dotX + part.dirX * 10 + part.dirY * 5; y: part.dotY + part.dirY * 10 - part.dirX * 5 }
                }
            }

            // The label: what it is and how much, at the band. A click types a value.
            Rectangle {
                id: chip

                x: part.chipX - width / 2
                y: part.chipY - height / 2
                width: chipRow.implicitWidth + 14
                height: 22
                radius: 11
                color: part.active ? part.tone : Theme.panel
                border.width: 1
                border.color: part.tone
                opacity: Handles.editing && !part.active ? 0.55 : 1.0
                visible: part.handle !== null && !(bubble.typing && bubble.index === part.index)

                Row {
                    id: chipRow

                    anchors.centerIn: parent
                    spacing: 4

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: part.handle ? root.labelFor(part.handle.parameter) : ""
                        color: part.active ? Theme.accentFg : Theme.fg3
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideNone
                    }

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: part.handle ? Numbers.format(part.handle.value, 2) : ""
                        color: part.active ? Theme.accentFg : Theme.fg
                        font.weight: Font.Medium
                        mono: true
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideNone
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !Handles.editing
                    cursorShape: Qt.IBeamCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: part.handle ? root.labelFor(part.handle.parameter) : ""
                    onClicked: bubble.begin(part.index, "")
                }
            }

            // The handle itself.
            Item {
                id: dot

                x: part.dotX - width / 2
                y: part.dotY - height / 2
                width: 24
                height: 24

                Rectangle {
                    anchors.centerIn: parent
                    width: area.containsMouse || part.active ? 18 : 14
                    height: width
                    radius: width / 2
                    color: part.active ? part.tone : Theme.handle
                    border.width: 2
                    border.color: part.tone
                }

                MouseArea {
                    id: area

                    anchors.fill: parent
                    hoverEnabled: true
                    preventStealing: true
                    cursorShape: Qt.SizeAllCursor
                    Accessible.role: Accessible.Slider
                    Accessible.name: part.handle ? root.labelFor(part.handle.parameter) : ""

                    onPressed: mouse => {
                        // The pointer position in the view, not in the dot: the dot moves with
                        // the shape while it is dragged.
                        const p = area.mapToItem(root, mouse.x, mouse.y)
                        if (Handles.beginDrag(part.index, p.x, p.y)) {
                            bubble.showDrag(part.index)
                        }
                    }
                    onPositionChanged: mouse => {
                        if (pressed) {
                            const p = area.mapToItem(root, mouse.x, mouse.y)
                            bubble.dragValue = Handles.dragTo(p.x, p.y)
                        }
                    }
                    onReleased: {
                        if (Handles.editing && !bubble.typing) {
                            Handles.endDrag()
                            bubble.hide()
                        }
                    }
                    // The grab was taken away (a gesture of the system, a window that lost the
                    // focus): the drag ends with what it has, it is not thrown away. Esc is what
                    // takes back.
                    onCanceled: {
                        if (Handles.editing && !bubble.typing) {
                            Handles.endDrag()
                            bubble.hide()
                        }
                    }
                }
            }

            Shortcut {
                sequence: "Escape"
                enabled: part.active && !bubble.typing
                onActivated: {
                    Handles.cancelDrag()
                    bubble.hide()
                }
            }
        }
    }

    // The bubble: the value of the handle that is dragged or typed, at the handle.
    Rectangle {
        id: bubble

        property int index: -1
        property bool typing: false
        property real dragValue: 0
        readonly property var handle: index >= 0 && index < Handles.handles.length ? Handles.handles[index] : null

        function showDrag(i) {
            index = i
            typing = false
            dragValue = Handles.handles[i].value
            visible = false // the label of the handle shows the value while it is pulled
        }

        function begin(i, first) {
            if (!Handles.beginEdit(i)) {
                return
            }
            index = i
            typing = true
            visible = true
            input.text = first
            input.forceActiveFocus()
            input.cursorPosition = input.text.length
            accept() // the first digit is already a value
        }

        function continueAsTyping(first) {
            typing = true
            input.text = first
            input.forceActiveFocus()
            input.cursorPosition = input.text.length
            accept()
        }

        function hide() {
            visible = false
            typing = false
            index = -1
        }

        // Moves typing to another value: Tab goes on, Shift+Tab goes back.
        function step(by) {
            accept()
            const count = Handles.handles.length
            index = (index + by + count) % count
            input.text = ""
            input.forceActiveFocus()
        }

        // The typed value goes into the document at once (the shape follows what is typed).
        function accept() {
            if (input.text.trim() !== "" && handle) {
                Handles.setValue(index, input.text.trim())
            }
        }

        visible: false
        x: handle ? Math.min(Math.max(handle.x + 16, 4), root.width - width - 4) : 0
        y: handle ? Math.min(Math.max(handle.y - height - 12, 4), root.height - height - 4) : 0
        width: bubbleRow.implicitWidth + 20
        height: 30
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: typing ? Theme.accentLine : Theme.line2
        z: 10

        Row {
            id: bubbleRow

            anchors.centerIn: parent
            spacing: 6

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: bubble.handle ? root.labelFor(bubble.handle.parameter) : ""
                color: Theme.fg3
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }

            // While dragging: the value. While typing: the field.
            Txt {
                visible: !bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                text: Numbers.format(bubble.dragValue, 2)
                mono: true
                color: Theme.fg
                font.weight: Font.Medium
                elide: Text.ElideNone
            }

            TextInput {
                id: input

                visible: bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(40, contentWidth + 6)
                color: Theme.fg
                selectionColor: Theme.accent
                selectedTextColor: Theme.accentFg
                font.family: Theme.monoFamilies[0]
                font.pixelSize: Theme.fontSize
                font.weight: Font.Medium
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                property bool pendingEdit: false
                Accessible.name: bubble.handle ? root.labelFor(bubble.handle.parameter) : ""

                onTextEdited: {
                    pendingEdit = true
                    const kind = "length"
                    if (!isNaN(Numbers.evaluate(text, kind))) {
                        bubble.accept()
                    }
                }
                Keys.onTabPressed: bubble.step(1)
                Keys.onBacktabPressed: bubble.step(-1)
                Keys.onReturnPressed: finish()
                Keys.onEnterPressed: finish()
                Keys.onEscapePressed: {
                    Handles.cancelEdit()
                    bubble.hide()
                    focus = false
                }
                onActiveFocusChanged: {
                    if (!activeFocus && bubble.typing) {
                        finish() // clicking elsewhere accepts what was typed
                    }
                }

                function finish() {
                    if (!bubble.typing) {
                        return
                    }
                    bubble.accept()
                    Handles.commitEdit()
                    bubble.hide()
                    focus = false
                }
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: root.unit
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                mono: true
                elide: Text.ElideNone
            }
        }
    }
}
