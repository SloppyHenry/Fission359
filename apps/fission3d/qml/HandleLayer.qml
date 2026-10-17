// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Shapes
import Fission3D.Ui

// The size handles of the chosen shape, drawn over the 3D view (handle style "Axes", AP-1.5):
// a dot on the face that a size moves, and a measure band beside the body with the label on it,
// like in a drawing. Drag a dot, or type: digits replace the value, Tab goes to the next size,
// Enter accepts, Esc leaves everything as it was. While a value is typed, its label turns into
// the field (docs/02-ux-konzept.md §2.2).
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

    // One handle for every value: its dot on the shape, its measure band beside the body and its
    // label. The delegates stay: the list of handles is made anew whenever a value changes, and a
    // handle that was rebuilt with it would be gone from under the pointer in the middle of a
    // drag. The model is only the number of handles; each one reads its own entry.
    Repeater {
        model: Handles.handles.length

        delegate: Item {
            id: part

            required property int index
            readonly property var handle: index < Handles.handles.length ? Handles.handles[index] : null
            readonly property bool active: Handles.editing && bubble.index === index
            readonly property bool pointed: area.containsMouse || chipArea.containsMouse
            readonly property bool plain: !handle || handle.axis === "none"
            readonly property bool detail: handle !== null && handle.role !== "size"
            // Lines and labels in the colour of the axis; values without an axis plain.
            readonly property color tone: !handle ? Theme.fg2
                : handle.axis === "x" ? Theme.axisX
                : handle.axis === "y" ? Theme.axisY
                : handle.axis === "z" ? Theme.axisZ : Theme.fg2
            // What is drawn on the body (dot, arrow): plain ones dark, so they show on a light body.
            readonly property color ink: plain ? Theme.handleLine : tone
            // A held handle stays in the view, even when the shape is pulled far beyond it: a
            // handle that disappears under the pointer ends the drag (the pointer loses it).
            readonly property real dotX: handle ? (active ? Math.min(Math.max(handle.x, 10), root.width - 10) : handle.x) : 0
            readonly property real dotY: handle ? (active ? Math.min(Math.max(handle.y, 10), root.height - 10) : handle.y) : 0
            readonly property real dirX: handle ? handle.dirX : 0
            readonly property real dirY: handle ? handle.dirY : 0
            readonly property real chipX: handle ? Math.min(Math.max(handle.chipX, 60), root.width - 60) : 0
            readonly property real chipY: handle ? Math.min(Math.max(handle.chipY, 16), root.height - 16) : 0
            // Quiet until wanted: a small body shows its dots only, a detail shows its label when
            // the pointer is on it. While another value changes, the rest steps back.
            readonly property bool showBand: handle !== null && handle.band && (!Handles.compact || pointed || active)
            readonly property bool showLabel: handle !== null && (pointed || active || (!detail && !Handles.compact))
            readonly property bool showLeader: handle !== null && !handle.band && (pointed || active)
            readonly property real calm: Handles.editing && !active ? 0.35 : 1.0

            width: root.width
            height: root.height
            // On a small body the details step aside (typing and the panel still reach them):
            // their dots would cover the ones of the sizes.
            visible: handle !== null && (active || (!(Handles.compact && detail)
                                                    && dotX > -40 && dotX < root.width + 40
                                                    && dotY > -40 && dotY < root.height + 40))

            // The band, as in a drawing: thin lines from the points it measures, and the line
            // between them with a stroke across each end.
            Shape {
                anchors.fill: parent
                visible: part.showBand
                opacity: part.calm

                ShapePath {
                    strokeColor: part.tone
                    strokeWidth: 1
                    fillColor: "transparent"
                    startX: part.handle ? part.handle.edgeStartX : 0
                    startY: part.handle ? part.handle.edgeStartY : 0
                    PathLine { x: part.handle ? part.handle.bandStartX : 0; y: part.handle ? part.handle.bandStartY : 0 }
                    PathMove { x: part.handle ? part.handle.edgeEndX : 0; y: part.handle ? part.handle.edgeEndY : 0 }
                    PathLine { x: part.handle ? part.handle.bandEndX : 0; y: part.handle ? part.handle.bandEndY : 0 }
                }

                ShapePath {
                    id: band

                    readonly property real sx: part.handle ? part.handle.bandStartX : 0
                    readonly property real sy: part.handle ? part.handle.bandStartY : 0
                    readonly property real ex: part.handle ? part.handle.bandEndX : 0
                    readonly property real ey: part.handle ? part.handle.bandEndY : 0
                    readonly property real len: Math.max(Math.hypot(ex - sx, ey - sy), 1e-6)
                    // Across the band, for the strokes at its ends.
                    readonly property real nx: -(ey - sy) / len * 5
                    readonly property real ny: (ex - sx) / len * 5

                    strokeColor: part.tone
                    strokeWidth: 1.5
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: band.sx
                    startY: band.sy
                    PathLine { x: band.ex; y: band.ey }
                    PathMove { x: band.sx + band.nx; y: band.sy + band.ny }
                    PathLine { x: band.sx - band.nx; y: band.sy - band.ny }
                    PathMove { x: band.ex + band.nx; y: band.ey + band.ny }
                    PathLine { x: band.ex - band.nx; y: band.ey - band.ny }
                }
            }

            // A radius, a rounding, a wall: what it measures, on the shape, while the pointer is
            // on it. A light line under a dark one, so that it shows on the body and beside it.
            Shape {
                anchors.fill: parent
                visible: part.showLeader

                ShapePath {
                    strokeColor: Theme.handle
                    strokeWidth: 3.5
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    startX: part.handle ? part.handle.startX : 0
                    startY: part.handle ? part.handle.startY : 0
                    PathLine { x: part.dotX; y: part.dotY }
                }

                ShapePath {
                    strokeColor: part.ink
                    strokeWidth: 1.5
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [3, 2]
                    fillColor: "transparent"
                    startX: part.handle ? part.handle.startX : 0
                    startY: part.handle ? part.handle.startY : 0
                    PathLine { x: part.dotX; y: part.dotY }
                }
            }

            // The arrow beyond the dot: which way it goes, while the pointer is on it.
            Shape {
                anchors.fill: parent
                visible: part.pointed || part.active

                ShapePath {
                    strokeColor: Theme.handle
                    strokeWidth: 4.5
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: part.dotX + part.dirX * 13 - part.dirY * 5
                    startY: part.dotY + part.dirY * 13 + part.dirX * 5
                    PathLine { x: part.dotX + part.dirX * 19; y: part.dotY + part.dirY * 19 }
                    PathLine { x: part.dotX + part.dirX * 13 + part.dirY * 5; y: part.dotY + part.dirY * 13 - part.dirX * 5 }
                }

                ShapePath {
                    strokeColor: part.ink
                    strokeWidth: 2
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    startX: part.dotX + part.dirX * 13 - part.dirY * 5
                    startY: part.dotY + part.dirY * 13 + part.dirX * 5
                    PathLine { x: part.dotX + part.dirX * 19; y: part.dotY + part.dirY * 19 }
                    PathLine { x: part.dotX + part.dirX * 13 + part.dirY * 5; y: part.dotY + part.dirY * 13 - part.dirX * 5 }
                }
            }

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

            // The handle itself, on the face it moves.
            Item {
                id: dot

                x: part.dotX - width / 2
                y: part.dotY - height / 2
                width: 24
                height: 24

                Rectangle {
                    id: disc

                    readonly property real size: part.handle && part.handle.role === "rounding" ? 16
                        : part.detail ? 12 : 14

                    anchors.centerIn: parent
                    width: area.containsMouse || part.active ? size + 4 : size
                    height: width
                    radius: width / 2
                    color: part.active ? part.ink : Theme.handle
                    border.width: 2
                    border.color: part.ink
                }

                // A rounding has a quarter circle on its dot (docs/02-ux-konzept.md §2.2).
                Shape {
                    anchors.centerIn: parent
                    width: disc.width
                    height: disc.width
                    visible: part.handle !== null && part.handle.role === "rounding"

                    ShapePath {
                        strokeColor: part.active ? Theme.handle : part.ink
                        strokeWidth: 1.5
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        startX: disc.width * 0.3
                        startY: disc.width * 0.72
                        PathArc {
                            x: disc.width * 0.72
                            y: disc.width * 0.3
                            radiusX: disc.width * 0.42
                            radiusY: disc.width * 0.42
                        }
                    }
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

    // The bubble: the field for the value that is typed, where its label was.
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
        // Where the label of the value is: the label turns into the field.
        x: handle ? Math.min(Math.max(handle.chipX - width / 2, 4), root.width - width - 4) : 0
        y: handle ? Math.min(Math.max(handle.chipY - height / 2, 4), root.height - height - 4) : 0
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
