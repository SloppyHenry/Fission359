// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui
import "clearplace.js" as ClearPlace

// The size handles of the chosen shape (handle style "Axes", AP-1.5): a dot on the face that a
// size moves, and a measure band beside the body with the label on it, like in a drawing. The
// dots, bands and arrows are drawn in the 3D view itself (what a body hides of them is faint);
// this layer holds their labels and the places to grab. Drag a dot, or type: digits replace the value, Tab goes to the next size,
// Enter accepts, Esc leaves everything as it was. While a value is typed, its label turns into
// the field (docs/02-ux-konzept.md §2.2).
Item {
    id: root

    // The sentence for a value ("Width"): the window knows the words.
    property var labelFor: name => name
    // Unit of a value, shown behind it.
    property string unit: "mm"
    // What floats over the view (tool palette, view cube …): labels keep out from under it.
    property var obstacles: []
    readonly property bool typing: bubble.typing
    objectName: "handleLayer"
    readonly property bool busy: Handles.editing

    // The colours of what the 3D view draws, from the theme.
    Binding {
        target: Handles
        property: "colours"
        value: ({
            x: Theme.axisX, y: Theme.axisY, z: Theme.axisZ,
            plain: Theme.fg2, ink: Theme.handleLine, fill: Theme.handle
        })
    }

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

    // One handle for every value: the place to grab its dot, and its label. The delegates stay:
    // the list of handles is made anew whenever a value changes, and a handle that was rebuilt
    // with it would be gone from under the pointer in the middle of a drag. The model is only the number of handles; each one reads its own entry.
    Repeater {
        model: Handles.handles.length

        delegate: Item {
            id: part

            required property int index
            readonly property var handle: index < Handles.handles.length ? Handles.handles[index] : null
            readonly property bool active: Handles.editing && bubble.index === index
            readonly property bool pointed: area.containsMouse || chipArea.containsMouse
            readonly property bool detail: handle !== null && handle.role !== "size"
            // Labels in the colour of the axis; values without an axis plain.
            readonly property color tone: !handle ? Theme.fg2
                : handle.axis === "x" ? Theme.axisX
                : handle.axis === "y" ? Theme.axisY
                : handle.axis === "z" ? Theme.axisZ : Theme.fg2
            // A held handle stays in the view, even when the shape is pulled far beyond it: a
            // handle that disappears under the pointer ends the drag (the pointer loses it).
            readonly property real dotX: handle ? (active ? Math.min(Math.max(handle.x, 10), root.width - 10) : handle.x) : 0
            readonly property real dotY: handle ? (active ? Math.min(Math.max(handle.y, 10), root.height - 10) : handle.y) : 0
            // In the view and not under what floats over it; a label beside its dot (no band) may
            // go to the other side of the dot instead.
            readonly property point chipAt: handle
                ? ClearPlace.place(handle.chipX, handle.chipY, chip.width, chip.height, root.obstacles,
                                   root.width, root.height,
                                   handle.band ? undefined : { x: handle.x, y: handle.y })
                : Qt.point(0, 0)
            readonly property real chipX: chipAt.x
            readonly property real chipY: chipAt.y
            // Quiet until wanted: a small body shows its dots only, a detail shows its label when
            // the pointer is on it. While another value changes, the rest steps back, and so does
            // a label whose band is behind a body.
            readonly property bool showLabel: handle !== null && (pointed || active || (!detail && !Handles.compact))
            readonly property real calm: (Handles.editing && !active ? 0.35 : 1.0) * (handle && handle.hidden ? 0.45 : 1.0)

            onPointedChanged: Handles.setPointed(index, pointed)

            width: root.width
            height: root.height
            // On a small body the details step aside (typing and the panel still reach them):
            // their dots would cover the ones of the sizes.
            visible: handle !== null && (active || (!(Handles.compact && detail)
                                                    && dotX > -40 && dotX < root.width + 40
                                                    && dotY > -40 && dotY < root.height + 40))

            // The label: what it is and how much, on its band. A click types a value.
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
                opacity: part.calm
                visible: part.showLabel && !(bubble.typing && bubble.index === part.index)

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
                    id: chipArea

                    anchors.fill: parent
                    enabled: !Handles.editing
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: part.handle ? root.labelFor(part.handle.parameter) : ""
                    onClicked: bubble.begin(part.index, "")
                }
            }

            // Where the dot is grabbed (the 3D view draws it).
            Item {
                id: dot

                x: part.dotX - width / 2
                y: part.dotY - height / 2
                width: 24
                height: 24

                MouseArea {
                    id: area

                    anchors.fill: parent
                    // Only the tool "Size" has dots; with "Move" the body is grabbed here.
                    enabled: Handles.mode === "size"
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

    // The bubble: the field for the value that is typed, where its label was.
    Rectangle {
        id: bubble

        property int index: -1
        property bool typing: false

        onIndexChanged: Handles.setCurrent(index)
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
        // Where the label of the value is: the label turns into the field.
        readonly property point at: handle
            ? ClearPlace.place(handle.chipX, handle.chipY, width, height, root.obstacles,
                               root.width, root.height,
                               handle.band ? undefined : { x: handle.x, y: handle.y })
            : Qt.point(0, 0)
        x: at.x - width / 2
        y: at.y - height / 2
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
