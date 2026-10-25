// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui
import "clearplace.js" as ClearPlace

// The few actions that fit what is chosen, floating beside it in the 3D view (AP-1.8d,
// docs/02-ux-konzept.md §2.3). Every button asks for the same action as the menu does; actions
// that are not built yet are left out. It hides while something is dragged or typed.
Item {
    id: root

    objectName: "contextBar"
    property var viewport: null
    property var obstacles: []
    // What is chosen: body (one) | bodies (several) | face | edge | none
    property string kind: "none"
    property var bodyIds: []
    property bool allHoles: false
    // Colour of the chosen bodies ("#rrggbb"; empty if they differ).
    property string bodyColor: ""
    // True while a handle is pulled, a number is typed or a shape is put down.
    property bool busy: false

    signal actionRequested(string action)
    signal colorChosen(string color)

    // The actions for the chosen thing, in order: { id, text, icon }.
    readonly property var actions: {
        switch (kind) {
        case "body":
            return [
                { id: "hole", text: allHoles ? qsTr("As part") : qsTr("As hole"), icon: "hole" },
                { id: "color", text: qsTr("Colour"), icon: "palette" },
                { id: "duplicate", text: qsTr("Duplicate"), icon: "pattern" },
                { id: "dropToBed", text: qsTr("To the bed"), icon: "ground" }
            ]
        case "bodies":
            return [
                { id: "merge", text: qsTr("Merge"), icon: "combine" },
                { id: "subtract", text: qsTr("Cut out"), icon: "scissors" },
                { id: "color", text: qsTr("Colour"), icon: "palette" },
                { id: "dropToBed", text: qsTr("To the bed"), icon: "ground" }
            ]
        case "face":
            return [{ id: "view.lookat", text: qsTr("Look at face"), icon: "eye" }]
        default:
            return []
        }
    }

    // Where the chosen things are in the view: the middle of the top of their box, in pixels.
    property var anchorPoint: null
    function refresh() {
        anchorPoint = null
        if (viewport === null || bodyIds.length === 0) {
            return
        }
        const box = DocumentModel.boundsOfBodies(bodyIds)
        if (!box.valid) {
            return
        }
        let left = Infinity, right = -Infinity, top = Infinity, bottom = -Infinity
        for (const x of [box.min[0], box.max[0]]) {
            for (const y of [box.min[1], box.max[1]]) {
                for (const z of [box.min[2], box.max[2]]) {
                    const p = viewport.project(x, y, z)
                    left = Math.min(left, p.x); right = Math.max(right, p.x)
                    top = Math.min(top, p.y); bottom = Math.max(bottom, p.y)
                }
            }
        }
        anchorPoint = { x: (left + right) / 2, top: top, bottom: bottom }
    }

    onBodyIdsChanged: refresh()
    onViewportChanged: refresh()

    Connections {
        target: root.viewport
        ignoreUnknownSignals: true
        function onCameraChanged() { root.refresh() }
        function onSceneChanged() { root.refresh() }
    }

    Connections {
        target: DocumentModel
        function onBodiesChanged() { root.refresh() }
    }

    readonly property bool shown: !busy && actions.length > 0 && anchorPoint !== null && !colorPopup.opened

    Rectangle {
        id: bar

        // Above the box; below it when there is no room above.
        readonly property var spot: root.anchorPoint === null ? ({ x: 0, y: 0 }) : ClearPlace.place(
            root.anchorPoint.x,
            root.anchorPoint.top - height / 2 - 14 < height / 2 + 4
                ? root.anchorPoint.bottom + height / 2 + 14 : root.anchorPoint.top - height / 2 - 14,
            width, height, root.obstacles, root.width, root.height)

        visible: root.shown || colorPopup.opened
        x: spot.x - width / 2
        y: spot.y - height / 2
        width: row.implicitWidth + 12
        height: 40
        radius: Theme.radiusFloat
        color: Theme.raise
        border.width: 1
        border.color: Theme.line2

        Row {
            id: row

            x: 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: root.actions

                delegate: Btn {
                    required property var modelData

                    objectName: "context_" + modelData.id
                    small: true
                    text: modelData.text
                    iconName: modelData.icon
                    onClicked: {
                        if (modelData.id === "color") {
                            colorPopup.open()
                        } else {
                            root.actionRequested(modelData.id)
                        }
                    }
                }
            }
        }

        Popup {
            id: colorPopup

            y: parent.height + 6
            width: 260
            padding: 12
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            background: Rectangle {
                radius: Theme.radiusFloat
                color: Theme.raise
                border.width: 1
                border.color: Theme.line2
            }

            contentItem: ColorField {
                current: root.bodyColor
                onChosen: color => {
                    root.colorChosen(color)
                    colorPopup.close()
                }
            }
        }
    }
}
