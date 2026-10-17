// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The shape gallery (docs/mockups/oberflaeche.html, "flyout"): big tiles with a picture of the
// real shape. Drag a tile into the 3D view (the shape follows the pointer and lands on the face
// under it), or click it (the shape goes to a free place on the bed). "As hole" makes the next
// shapes holes.
Float {
    id: root

    // The coordinates of this item's parent are the coordinates of the 3D view the shapes are
    // dragged into.
    property bool asHole: false
    readonly property alias catalog: shapeCatalog
    readonly property bool dragging: dragState.active
    signal closeRequested

    readonly property int gap: 6
    readonly property int columns: Math.max(1, Math.floor((width - 28 + gap) / (80 + gap)))
    readonly property real tileWidth: (width - 28 - (columns - 1) * gap) / columns
    readonly property int rows: Math.ceil(shapeCatalog.shapes.length / columns)

    height: 14 + header.height + 12 + rows * 98 + (rows - 1) * gap + 14
    z: 5

    ShapeCatalog {
        id: shapeCatalog
    }

    // What is going on while a tile is dragged.
    QtObject {
        id: dragState

        property bool active: false
        property bool cancelled: false
        property string type: ""
        property point at: Qt.point(0, 0) // in the coordinates of the 3D view
        property bool overView: false
    }

    // The picture that follows the pointer while it is not yet over the 3D view.
    Image {
        parent: root.parent
        visible: dragState.active && !dragState.overView && !dragState.cancelled
        x: dragState.at.x - width / 2
        y: dragState.at.y - height / 2
        width: 62
        height: 52
        opacity: 0.85
        z: 20
        sourceSize: Qt.size(124, 104)
        source: dragState.type !== "" ? "image://shapes/" + dragState.type + (root.asHole ? "/hole" : "") : ""
    }

    Shortcut {
        sequence: "Escape"
        enabled: dragState.active || Placement.active
        onActivated: {
            Placement.cancel()
            dragState.cancelled = true
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 14

        Item {
            id: header

            width: parent.width
            height: 30

            Txt {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Shapes")
                color: Theme.fg
                font.weight: Font.DemiBold
                font.pixelSize: Theme.fontSizeTitle
                elide: Text.ElideNone
            }

            Toggle {
                id: holeSwitch

                anchors.right: closeButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("As hole")
                checked: root.asHole
                onToggled: root.asHole = checked
            }

            IconBtn {
                id: closeButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                buttonSize: 26
                iconName: "x"
                tip: qsTr("Close")
                onClicked: root.closeRequested()
            }
        }

        Flow {
            y: header.height + 12
            width: parent.width
            spacing: root.gap

            Repeater {
                model: shapeCatalog.shapes

                delegate: FocusScope {
                    id: tile

                    required property var modelData

                    width: root.tileWidth
                    height: 98
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.name
                    Accessible.onPressAction: root.putOnBed(modelData.type)
                    Keys.onReturnPressed: root.putOnBed(modelData.type)
                    Keys.onEnterPressed: root.putOnBed(modelData.type)
                    Keys.onSpacePressed: root.putOnBed(modelData.type)

                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: area.pressed ? Theme.accentSoft : Theme.raise
                        border.width: 1
                        border.color: area.containsMouse || tile.activeFocus ? Theme.accentLine : Theme.line
                        opacity: dragState.active && dragState.type === tile.modelData.type ? 0.6 : 1.0

                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 8
                            width: 62
                            height: 52
                            sourceSize: Qt.size(124, 104)
                            asynchronous: true
                            source: "image://shapes/" + tile.modelData.type + (root.asHole ? "/hole" : "")
                        }

                        Txt {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 68
                            width: parent.width - 8
                            horizontalAlignment: Text.AlignHCenter
                            text: tile.modelData.name
                            color: area.containsMouse ? Theme.fg : Theme.fg2
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Focusring {
                            visible: tile.activeFocus
                            ringRadius: 8
                        }
                    }

                    MouseArea {
                        id: area

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        property point pressPoint

                        onPressed: mouse => {
                            tile.forceActiveFocus()
                            pressPoint = Qt.point(mouse.x, mouse.y)
                        }

                        onPositionChanged: mouse => {
                            if (!pressed) {
                                return
                            }
                            if (!dragState.active && Math.hypot(mouse.x - pressPoint.x, mouse.y - pressPoint.y) > 6) {
                                dragState.active = true
                                dragState.cancelled = false
                                dragState.type = tile.modelData.type
                            }
                            if (dragState.active) {
                                root.dragTo(area.mapToItem(root.parent, mouse.x, mouse.y))
                            }
                        }

                        onReleased: mouse => {
                            if (dragState.active) {
                                root.dropHere()
                            } else if (area.containsMouse) {
                                root.putOnBed(tile.modelData.type) // a click
                            }
                        }

                        onCanceled: {
                            Placement.cancel()
                            dragState.active = false
                        }
                    }

                    ToolTip {
                        visible: area.containsMouse && !dragState.active
                        delay: 800
                        x: 0
                        y: tile.height + 6
                        padding: 10

                        contentItem: Column {
                            spacing: 3
                            width: 230

                            Txt {
                                width: parent.width
                                text: tile.modelData.name
                                color: Theme.fg
                                font.weight: Font.DemiBold
                                elide: Text.ElideNone
                            }

                            Txt {
                                width: parent.width
                                text: shapeCatalog.dragTip + " " + shapeCatalog.clickTip
                                color: Theme.fg3
                                font.pixelSize: Theme.fontSizeSmall
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                            }
                        }

                        background: Rectangle {
                            radius: Theme.radiusFloat
                            color: Theme.panel
                            border.width: 1
                            border.color: Theme.line2
                        }
                    }
                }
            }
        }
    }

    // --- what the pointer does while a tile is dragged ---------------------------------------
    function dragTo(point) {
        dragState.at = point
        if (dragState.cancelled) {
            return
        }
        const view = root.parent
        const inFlyout = root.contains(root.mapFromItem(view, point.x, point.y))
        const inView = point.x >= 0 && point.y >= 0 && point.x < view.width && point.y < view.height
        dragState.overView = inView && !inFlyout
        if (!dragState.overView) {
            Placement.cancel() // back over the gallery or a side bar: take the shape back
            return
        }
        if (Placement.active) {
            Placement.moveTo(point.x, point.y)
        } else {
            Placement.begin(dragState.type, root.asHole, point.x, point.y)
        }
    }

    function dropHere() {
        if (Placement.active) {
            Placement.finish()
        }
        dragState.active = false
        dragState.cancelled = false
        dragState.overView = false
    }

    function putOnBed(type) {
        Placement.add(type, root.asHole)
    }
}
