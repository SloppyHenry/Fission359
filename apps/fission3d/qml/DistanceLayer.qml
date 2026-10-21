// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui
import "clearplace.js" as ClearPlace

// The distance measures of the tool "Move" (AP-1.6d, docs/02-ux-konzept.md §2.3): beside the
// bands that the 3D view draws from the chosen body to its neighbours and the edge of the bed,
// the number of each. A click makes it a field: type `5` and press Enter, and the body stands
// 5 mm from there. Tab goes on to the next band, Esc leaves everything as it was.
Item {
    id: root

    objectName: "distanceLayer"
    // What floats over the view (tool palette, view cube …): the numbers keep out from under it.
    property var obstacles: []
    // The number that is a field now (place in Distances.labels), or -1.
    property int editing: -1
    readonly property bool typing: editing >= 0 && input.activeFocus

    Binding {
        target: Distances
        property: "colours"
        value: ({ line: Theme.fg2 })
    }

    // Nothing to type into while a body is being moved or a size is typed.
    readonly property bool idle: !Gizmo.editing && !Handles.editing && !Rotate.editing && !Scale.editing

    function open(index) {
        if (!root.idle || index < 0 || index >= Distances.labels.length) {
            return
        }
        editing = index
        input.refused = false
        input.text = Numbers.format(Distances.labels[index].value, 3)
        input.pendingEdit = false
        input.forceActiveFocus()
        input.selectAll()
    }

    function close() {
        editing = -1
        input.focus = false
    }

    // The number typed, put into the document: the body moves so that the band measures that.
    function apply() {
        if (editing < 0) {
            return true
        }
        const value = Numbers.evaluate(input.text.trim(), "length")
        return !isNaN(value) && Distances.setDistance(editing, value)
    }

    onIdleChanged: if (!idle) close()
    Connections {
        target: Distances
        function onEnabledChanged() { if (!Distances.enabled) root.close() }
    }

    Repeater {
        model: Distances.labels.length

        delegate: Item {
            id: part

            required property int index
            readonly property var info: index < Distances.labels.length ? Distances.labels[index] : null
            readonly property bool open: root.editing === index
            readonly property bool pointed: area.containsMouse

            onPointedChanged: Distances.setPointed(index, pointed)

            anchors.fill: parent
            visible: info !== null

            Rectangle {
                id: chip

                readonly property point at: part.info
                    ? ClearPlace.place(part.info.x, part.info.y, width, height, root.obstacles,
                                       root.width, root.height)
                    : Qt.point(0, 0)

                x: at.x - width / 2
                y: at.y - height / 2
                width: chipRow.implicitWidth + 14
                height: 22
                radius: 11
                color: Theme.panel
                border.width: 1
                border.color: part.open ? (input.refused ? Theme.bad : Theme.accentLine)
                                        : (part.pointed ? Theme.fg3 : Theme.line2)
                opacity: Gizmo.editing ? 0.8 : 1.0
                z: part.open ? 10 : 1

                Row {
                    id: chipRow

                    anchors.centerIn: parent
                    spacing: 4

                    // The number: what it is while it is shown, the field when it is clicked.
                    Txt {
                        visible: !part.open
                        anchors.verticalCenter: parent.verticalCenter
                        text: part.info ? Numbers.format(part.info.value, 2) : ""
                        mono: true
                        color: Theme.fg
                        font.weight: Font.Medium
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideNone
                    }

                    TextInput {
                        id: input

                        property bool refused: false
                        property bool pendingEdit: false

                        visible: part.open
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(36, contentWidth + 6)
                        color: Theme.fg
                        selectionColor: Theme.accent
                        selectedTextColor: Theme.accentFg
                        font.family: Theme.monoFamilies[0]
                        font.pixelSize: Theme.fontSizeSmall
                        font.weight: Font.Medium
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        Accessible.name: qsTr("Distance")

                        onTextEdited: {
                            pendingEdit = true
                            refused = false
                        }
                        Keys.onReturnPressed: finish()
                        Keys.onEnterPressed: finish()
                        Keys.onEscapePressed: root.close()
                        Keys.onTabPressed: next(1)
                        Keys.onBacktabPressed: next(-1)
                        onActiveFocusChanged: {
                            // Clicking elsewhere accepts what was typed, if anything was.
                            if (!activeFocus && root.editing === part.index) {
                                if (pendingEdit) {
                                    root.apply()
                                }
                                root.editing = -1
                            }
                        }

                        function finish() {
                            if (root.editing < 0) {
                                return
                            }
                            if (!pendingEdit || root.apply()) {
                                root.close()
                            } else {
                                refused = true // not a number, or one that cannot be a distance
                            }
                        }

                        function next(by) {
                            const count = Distances.labels.length
                            if (root.editing < 0 || count === 0) {
                                return
                            }
                            if (pendingEdit && !root.apply()) {
                                refused = true
                                return
                            }
                            root.open((root.editing + by + count) % count)
                        }
                    }

                    Txt {
                        visible: part.open
                        anchors.verticalCenter: parent.verticalCenter
                        text: "mm"
                        color: Theme.fg4
                        font.pixelSize: Theme.fontSizeSmall
                        mono: true
                        elide: Text.ElideNone
                    }
                }

                MouseArea {
                    id: area

                    anchors.fill: parent
                    enabled: !part.open && root.idle
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Distance")
                    onClicked: root.open(part.index)
                }
            }
        }
    }
}
