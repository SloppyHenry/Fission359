// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui
import "clearplace.js" as ClearPlace

// The rings of the tool "Rotate" (AP-1.6b, docs/02-ux-konzept.md §2.2): one ring for each axis of
// the bed round the middle of the chosen body. The 3D view draws them (what a body hides is
// faint); this layer holds the places to grab (only on the line of each ring) and the value
// bubble. While a body turns, digits replace the angle: Tab goes on from X to Y to Z, Enter
// accepts, Esc puts it back.
Item {
    id: root

    objectName: "rotateLayer"
    readonly property bool typing: bubble.typing
    // What floats over the view (tool palette, view cube …): the bubble keeps out from under it.
    property var obstacles: []

    Binding {
        target: Rotate
        property: "colours"
        value: ({ x: Theme.axisX, y: Theme.axisY, z: Theme.axisZ, accent: Theme.accent })
    }

    function axisColour(axis) {
        return axis === "x" ? Theme.axisX : axis === "y" ? Theme.axisY : axis === "z" ? Theme.axisZ : Theme.fg2
    }

    // A digit was typed while a body is being turned: typing takes over from the pointer.
    function startTyping(first) {
        if (!Rotate.editing || bubble.typing || !Rotate.typeInsteadOfDrag()) {
            return
        }
        const turning = Rotate.turning
        bubble.axis = turning.indexOf("x") >= 0 ? 0 : turning.indexOf("y") >= 0 ? 1 : 2
        bubble.typing = true
        input.text = first
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
        bubble.accept()
    }

    // Developer option --simulate "type:..." while turning: the same calls the keys make.
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

    // One place to grab for each ring, shaped like the ring (containmentMask): the rest of the
    // view, and the body in the middle of the rings, stays free for the pointer. The three
    // always exist, so the one that is held never goes away under the pointer.
    Repeater {
        model: 3

        delegate: Item {
            id: ring

            required property int index
            readonly property string partName: ["x", "y", "z"][index]

            anchors.fill: parent

            MouseArea {
                id: area

                anchors.fill: parent
                containmentMask: Rotate.mask(ring.index)
                enabled: Rotate.enabled
                hoverEnabled: true
                preventStealing: true
                cursorShape: Qt.PointingHandCursor
                Accessible.role: Accessible.Slider
                Accessible.name: qsTr("Turn around %1").arg(ring.partName.toUpperCase())

                onContainsMouseChanged: Rotate.setPointed(ring.partName, containsMouse)
                onPressed: mouse => Rotate.beginDrag(ring.partName, mouse.x, mouse.y)
                onPositionChanged: mouse => {
                    if (pressed && Rotate.editing && !bubble.typing) {
                        Rotate.dragTo(mouse.x, mouse.y)
                    }
                }
                onReleased: {
                    if (Rotate.editing && !bubble.typing) {
                        Rotate.endDrag()
                    }
                }
                // The grab was taken away: the turn keeps what it has (Esc is what takes back).
                onCanceled: {
                    if (Rotate.editing && !bubble.typing) {
                        Rotate.endDrag()
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: Rotate.editing && !bubble.typing
        onActivated: Rotate.cancelDrag()
    }

    // The bubble beside the rings: how far the body has turned; the field when an angle is typed.
    Rectangle {
        id: bubble

        property bool typing: false
        property int axis: 0
        readonly property var names: ["X", "Y", "Z"]
        readonly property string turning: Rotate.turning

        function turns(i) {
            return turning.indexOf(["x", "y", "z"][i]) >= 0
        }

        // The typed angle goes into the document at once (the body follows what is typed).
        function accept() {
            const value = Numbers.evaluate(input.text.trim(), "angle")
            if (input.text.trim() !== "" && !isNaN(value)) {
                Rotate.setTurn(axis, value)
            }
        }

        // Tab: the next ring, X → Y → Z.
        function step(by) {
            if (!input.check()) {
                return
            }
            accept()
            axis = (axis + by + 3) % 3
            input.text = ""
            input.forceActiveFocus()
        }

        visible: Rotate.editing && Rotate.bubbleAt.x !== undefined
        readonly property point at: visible
            ? ClearPlace.place(Rotate.bubbleAt.x + width / 2, Rotate.bubbleAt.y + height / 2, width,
                               height, root.obstacles, root.width, root.height)
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

        onVisibleChanged: if (!visible) typing = false

        Row {
            id: bubbleRow

            anchors.centerIn: parent
            spacing: 6

            // While dragging: the angle about each axis that turns.
            Repeater {
                model: 3

                delegate: Row {
                    id: entry

                    required property int index

                    visible: !bubble.typing && bubble.turns(index)
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
                            const v = Rotate.turn[entry.index]
                            return (v > 0 ? "+" : "") + Numbers.format(v, 1) + "°"
                        }
                        mono: true
                        color: Theme.fg
                        font.weight: Font.Medium
                        elide: Text.ElideNone
                    }
                }
            }

            // While typing: the axis and the field for its angle.
            Txt {
                visible: bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                text: bubble.names[bubble.axis]
                color: root.axisColour(["x", "y", "z"][bubble.axis])
                font.weight: Font.DemiBold
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }

            ValueInput {
                id: input

                kind: "angle"
                visible: bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                Accessible.name: qsTr("Angle around %1").arg(bubble.names[bubble.axis])

                onTextEdited: {
                    if (!isNaN(Numbers.evaluate(text, "angle"))) {
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
                        // Clicking elsewhere accepts what was typed, if it is a number.
                        if (check()) {
                            finish()
                        } else {
                            cancel()
                        }
                    }
                }

                function finish() {
                    if (!bubble.typing || !check()) {
                        return // a wrong text stays in the field, with the reason under it
                    }
                    bubble.accept()
                    bubble.typing = false
                    Rotate.commitEdit()
                    focus = false
                }

                function cancel() {
                    bubble.typing = false
                    Rotate.cancelEdit()
                    focus = false
                }
            }

            Txt {
                visible: bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                text: "°"
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                mono: true
                elide: Text.ElideNone
            }
        }
    }
}
