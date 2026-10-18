// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The gizmo of the tool "Move" (AP-1.6a, docs/02-ux-konzept.md §2.2): arrows along X, Y and Z,
// squares for a plane, a dot in the middle for moving across the bed. The 3D view draws them
// (what a body hides is faint); this layer holds the letters, the places to grab and the value
// bubble. While a body moves (by the gizmo or by dragging the body itself), digits replace the
// distance: Tab goes on from X to Y to Z, Enter accepts, Esc puts it back where it was.
Item {
    id: root

    objectName: "gizmoLayer"
    readonly property bool typing: bubble.typing

    Binding {
        target: Gizmo
        property: "colours"
        value: ({ x: Theme.axisX, y: Theme.axisY, z: Theme.axisZ, ink: Theme.handleLine, fill: Theme.handle })
    }

    function axisColour(axis) {
        return axis === "x" ? Theme.axisX : axis === "y" ? Theme.axisY : axis === "z" ? Theme.axisZ : Theme.fg2
    }

    // A digit was typed while a body is being moved: typing takes over from the pointer.
    function startTyping(first) {
        if (!Gizmo.editing || bubble.typing || !Gizmo.typeInsteadOfDrag()) {
            return
        }
        const moving = Gizmo.moving
        bubble.axis = moving.indexOf("x") >= 0 || moving === "free" || moving === "body" ? 0
                    : moving.indexOf("y") >= 0 ? 1 : 2
        bubble.typing = true
        input.text = first
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
        bubble.accept()
    }

    // Developer option --simulate "type:..." while moving: the same calls the keys make.
    function simulate(tokens) {
        for (const token of tokens) {
            if (token === "tab") {
                bubble.step(1)
            } else if (token === "enter") {
                input.finish()
            } else if (token === "esc") {
                input.cancel()
            } else if (!bubble.typing) {
                startTyping(token)
            } else {
                input.text = token
                bubble.accept()
            }
        }
    }

    // The places to grab, one item for each of the seven parts. The list is made anew while
    // dragging but always holds all seven in the same order, so the delegates stay: a place to
    // grab that went away under the pointer would end the drag. (It did: an arrow that turned
    // towards the viewer while the body moved was taken out of the list, and the drag let go.)
    Repeater {
        model: Gizmo.parts.length

        delegate: Item {
            id: part

            required property int index
            readonly property var info: index < Gizmo.parts.length ? Gizmo.parts[index] : null
            readonly property bool shown: info !== null && info.shown
            readonly property bool arrow: shown && info.midX !== undefined
            readonly property bool pointed: head.containsMouse || shaft.containsMouse

            onPointedChanged: if (info) Gizmo.setPointed(info.part, pointed)

            anchors.fill: parent

            // The letter at the tip of an arrow.
            Txt {
                visible: part.arrow
                x: part.arrow ? part.info.labelX - width / 2 : 0
                y: part.arrow ? part.info.labelY - height / 2 : 0
                text: part.info ? part.info.part.toUpperCase() : ""
                color: root.axisColour(part.info ? part.info.axis : "")
                font.weight: Font.DemiBold
                font.pixelSize: Theme.fontSizeSmall
                opacity: Gizmo.editing && part.info && Gizmo.moving !== part.info.part ? 0.35 : 1.0
                elide: Text.ElideNone
            }

            GizmoGrab {
                id: head

                enabled: part.shown
                visible: part.shown
                partName: part.info ? part.info.part : ""
                centreX: part.info ? part.info.x : 0
                centreY: part.info ? part.info.y : 0
                size: part.arrow ? 32 : (part.info && part.info.part === "free" ? 26 : 24)
            }

            // The shaft of an arrow can be grabbed, too.
            GizmoGrab {
                id: shaft

                enabled: part.arrow
                visible: part.arrow
                partName: part.info ? part.info.part : ""
                centreX: part.arrow ? part.info.midX : 0
                centreY: part.arrow ? part.info.midY : 0
                size: 26
            }
        }
    }

    component GizmoGrab: Item {
        id: grab

        property string partName
        property real centreX
        property real centreY
        property real size: 20
        readonly property bool containsMouse: area.containsMouse

        x: centreX - width / 2
        y: centreY - height / 2
        width: size
        height: size

        MouseArea {
            id: area

            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeAllCursor
            Accessible.role: Accessible.Slider
            Accessible.name: grab.partName === "free" ? qsTr("Move across the bed")
                : grab.partName.length === 2 ? qsTr("Move in the %1 plane").arg(grab.partName.toUpperCase())
                : qsTr("Move along %1").arg(grab.partName.toUpperCase())

            onPressed: mouse => {
                const p = area.mapToItem(root, mouse.x, mouse.y)
                Gizmo.beginDrag(grab.partName, p.x, p.y)
            }
            onPositionChanged: mouse => {
                if (pressed && Gizmo.editing && !bubble.typing) {
                    const p = area.mapToItem(root, mouse.x, mouse.y)
                    Gizmo.dragTo(p.x, p.y)
                }
            }
            onReleased: {
                if (Gizmo.editing && !bubble.typing) {
                    Gizmo.endDrag()
                }
            }
            // The grab was taken away: the move keeps what it has (Esc is what takes back).
            onCanceled: {
                if (Gizmo.editing && !bubble.typing) {
                    Gizmo.endDrag()
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: Gizmo.editing && !bubble.typing
        onActivated: Gizmo.cancelDrag()
    }

    // The bubble beside the body: how far it has moved; the field when a distance is typed.
    Rectangle {
        id: bubble

        property bool typing: false
        property int axis: 0
        readonly property var names: ["X", "Y", "Z"]
        readonly property string moving: Gizmo.moving

        function moves(i) {
            const letter = ["x", "y", "z"][i]
            return moving.indexOf(letter) >= 0 || ((moving === "free" || moving === "body") && i < 2)
        }

        // The typed distance goes into the document at once (the body follows what is typed).
        function accept() {
            const value = Numbers.evaluate(input.text.trim(), "length")
            if (input.text.trim() !== "" && !isNaN(value)) {
                Gizmo.setOffset(axis, value)
            }
        }

        // Tab: the next axis, X → Y → Z.
        function step(by) {
            accept()
            axis = (axis + by + 3) % 3
            input.text = ""
            input.forceActiveFocus()
        }

        visible: Gizmo.editing && Gizmo.bubbleAt.x !== undefined
        x: visible ? Math.min(Math.max(Gizmo.bubbleAt.x, 4), root.width - width - 4) : 0
        y: visible ? Math.min(Math.max(Gizmo.bubbleAt.y, 4), root.height - height - 4) : 0
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
            spacing: 6

            // While dragging: the distance along each axis that moves.
            Repeater {
                model: 3

                delegate: Row {
                    id: entry

                    required property int index

                    visible: !bubble.typing && bubble.moves(index)
                    spacing: 4

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: bubble.names[entry.index]
                        color: root.axisColour(["x", "y", "z"][entry.index])
                        font.weight: Font.DemiBold
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideNone
                    }

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            const v = Gizmo.offset[entry.index]
                            return (v > 0 ? "+" : "") + Numbers.format(v, 2)
                        }
                        mono: true
                        color: Theme.fg
                        font.weight: Font.Medium
                        elide: Text.ElideNone
                    }
                }
            }

            // While typing: the axis and the field for its distance.
            Txt {
                visible: bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                text: bubble.names[bubble.axis]
                color: root.axisColour(["x", "y", "z"][bubble.axis])
                font.weight: Font.DemiBold
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }

            TextInput {
                id: input

                property bool pendingEdit: false

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
                Accessible.name: qsTr("Distance along %1").arg(bubble.names[bubble.axis])

                onTextEdited: {
                    pendingEdit = true
                    if (!isNaN(Numbers.evaluate(text, "length"))) {
                        bubble.accept()
                    }
                }
                Keys.onTabPressed: bubble.step(1)
                Keys.onBacktabPressed: bubble.step(-1)
                Keys.onReturnPressed: finish()
                Keys.onEnterPressed: finish()
                Keys.onEscapePressed: cancel()
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
                    bubble.typing = false
                    Gizmo.commitEdit()
                    pendingEdit = false
                    focus = false
                }

                function cancel() {
                    bubble.typing = false
                    Gizmo.cancelEdit()
                    pendingEdit = false
                    focus = false
                }
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: "mm"
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                mono: true
                elide: Text.ElideNone
            }
        }
    }
}
