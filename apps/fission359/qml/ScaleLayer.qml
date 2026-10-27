// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui
import "clearplace.js" as ClearPlace

// The corner handles of the tool "Size" (AP-1.6b2, docs/02-ux-konzept.md §7.2): a square on each
// top corner of the box round the chosen body. Pulling one makes the body larger or smaller in all
// directions alike; the corner across at the bottom stays. The 3D view draws the squares and the
// dashed box (what a body hides is faint); this layer holds the places to grab and the value
// bubble. While a corner is pulled, digits replace the percentage: Enter accepts, Esc puts it back.
Item {
    id: root

    objectName: "scaleLayer"
    readonly property bool typing: bubble.typing
    // What floats over the view (tool palette, view cube …): the bubble keeps out from under it.
    property var obstacles: []

    Binding {
        target: Scale
        property: "colours"
        value: ({ handle: Theme.handle, ink: Theme.handleLine, line: Theme.fg3, accent: Theme.accent })
    }

    // A digit was typed while a corner is pulled: typing takes over from the pointer.
    function startTyping(first) {
        if (!Scale.editing || bubble.typing || !Scale.typeInsteadOfDrag()) {
            return
        }
        bubble.typing = true
        input.text = first
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
        bubble.accept()
    }

    // Developer option --simulate "type:..." while scaling: the same calls the keys make.
    function simulate(tokens) {
        for (const token of tokens) {
            if (token === "enter") {
                input.finish()
            } else if (token === "esc") {
                input.cancel()
            } else if (token === "tab") {
                // Only one value here: Tab accepts what is typed, like a number field.
                bubble.accept()
            } else if (!bubble.typing) {
                startTyping(token)
            } else {
                input.text = token
                bubble.accept()
            }
        }
    }

    // One place to grab for each corner. The four always exist, so the one that is held never
    // goes away under the pointer.
    Repeater {
        model: 4

        delegate: MouseArea {
            id: area

            required property int index
            readonly property var corner: Scale.corners.length > index ? Scale.corners[index] : null

            x: corner && corner.shown ? corner.x - width / 2 : -100
            y: corner && corner.shown ? corner.y - height / 2 : -100
            width: 24
            height: 24
            enabled: Scale.enabled && corner !== null && corner.shown
            hoverEnabled: true
            preventStealing: true
            cursorShape: index % 2 === 0 ? Qt.SizeBDiagCursor : Qt.SizeFDiagCursor
            Accessible.role: Accessible.Slider
            Accessible.name: qsTr("Scale evenly")

            onContainsMouseChanged: Scale.setPointed(index, containsMouse)
            onPressed: mouse => {
                const at = mapToItem(root, mouse.x, mouse.y)
                Scale.beginDrag(index, at.x, at.y)
            }
            onPositionChanged: mouse => {
                if (pressed && Scale.editing && !bubble.typing) {
                    const at = mapToItem(root, mouse.x, mouse.y)
                    Scale.dragTo(at.x, at.y)
                }
            }
            onReleased: {
                if (Scale.editing && !bubble.typing) {
                    Scale.endDrag()
                }
            }
            // The grab was taken away: the drag keeps what it has (Esc is what takes back).
            onCanceled: {
                if (Scale.editing && !bubble.typing) {
                    Scale.endDrag()
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: Scale.editing && !bubble.typing
        onActivated: Scale.cancelDrag()
    }

    // The bubble beside the corner held: how large the body is now; the field when a percentage
    // is typed.
    Rectangle {
        id: bubble

        property bool typing: false

        // The typed percentage goes into the document at once (the body follows what is typed).
        // "150 %" is understood as well as "150".
        function accept() {
            const text = input.text.replace("%", "").trim()
            const value = Numbers.evaluate(text, "plain")
            input.refused = text !== "" && (isNaN(value) || !Scale.setPercent(value))
        }

        visible: Scale.editing && Scale.bubbleAt.x !== undefined
        // Right of the corner held; left of it where the view ends first (it must not cover the
        // corner the pointer is on).
        readonly property real wantedX: {
            const b = Scale.bubbleAt
            if (b.x === undefined) {
                return 0
            }
            return b.x + width + 4 <= root.width ? b.x + width / 2
                                                 : 2 * b.cornerX - b.x - width / 2
        }
        readonly property point at: visible
            ? ClearPlace.place(wantedX, Scale.bubbleAt.y + height / 2, width, height, root.obstacles,
                               root.width, root.height,
                               Qt.point(Scale.bubbleAt.cornerX, Scale.bubbleAt.cornerY))
            : Qt.point(0, 0)
        x: at.x - width / 2
        y: at.y - height / 2
        width: bubbleRow.implicitWidth + 20
        height: 30
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: typing ? (input.refused ? Theme.bad : Theme.accentLine) : Theme.line2
        z: 10

        onVisibleChanged: if (!visible) typing = false

        Row {
            id: bubbleRow

            anchors.centerIn: parent
            spacing: 8

            // While dragging: the percentage; while typing: the field for it.
            Txt {
                visible: !bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                // Whole percent, or one decimal where the corner caught on another body (137.4 %).
                readonly property real percent: Scale.factor * 100
                text: qsTr("%1 %").arg(Numbers.format(percent, Math.abs(percent - Math.round(percent)) < 0.05 ? 0 : 1))
                mono: true
                color: Theme.fg
                font.weight: Font.Medium
                elide: Text.ElideNone
            }

            TextInput {
                id: input

                property bool refused: false

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
                Accessible.name: qsTr("Size in percent")

                onTextEdited: bubble.accept()
                Keys.onTabPressed: bubble.accept()
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
                    refused = false
                    Scale.commitEdit()
                    focus = false
                }

                function cancel() {
                    bubble.typing = false
                    refused = false
                    Scale.cancelEdit()
                    focus = false
                }
            }

            Txt {
                visible: bubble.typing
                anchors.verticalCenter: parent.verticalCenter
                text: "%"
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                mono: true
                elide: Text.ElideNone
            }

            // The size of the box round the body that comes out of it.
            Txt {
                anchors.verticalCenter: parent.verticalCenter
                readonly property var size: Scale.size
                text: size.length === 3
                    ? qsTr("%1 × %2 × %3 mm").arg(Numbers.format(size[0], 1))
                                             .arg(Numbers.format(size[1], 1))
                                             .arg(Numbers.format(size[2], 1))
                    : ""
                color: Theme.fg3
                font.pixelSize: Theme.fontSizeSmall
                mono: true
                elide: Text.ElideNone
            }
        }
    }
}
