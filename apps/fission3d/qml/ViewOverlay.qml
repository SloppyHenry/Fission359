// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// Everything that floats above the 3D view: display bar, tool palette, view cube, navigation
// buttons (docs/08-gestaltung.md §5).
Item {
    id: root

    property bool orthographic: false
    property string display: "color"       // wire | solid | color
    property string tool: "move"
    property real cameraYaw: 0
    property real cameraPitch: 0
    property bool focusMode: false
    property string mouseStyle: "fission"  // fission | fusion | tinkercad | blender | sketchup
    /// Opens the menu of what can be chosen (the developer option --simulate "filtermenu").
    function openFilterMenu() {
        filterButton.clicked()
    }
    property string filter: "all"          // what can be chosen: all | body | face | edge | vertex
    property string flyout: ""             // the open tool group: "" or "shapes"
    signal flyoutCloseRequested
    signal filterRequested(string filter)
    signal orthographicToggled
    signal displayRequested(string mode)
    signal toolRequested(string tool)
    signal groupRequested(string group)
    signal viewRequested(string view)
    signal directionRequested(real x, real y, real z)
    signal focusToggled
    signal navigationHint(string hint)
    signal orbitDragged(real dx, real dy)
    signal panDragged(real dx, real dy)
    signal zoomRequested(real factor)

    // What floats over the 3D view, as rectangles in its coordinates: labels and value bubbles of
    // the handles keep out from under them (they once hid behind the tool palette).
    readonly property var obstacles: [
        Qt.rect(displayBar.x, displayBar.y, displayBar.width, displayBar.height),
        Qt.rect(filterBar.x, filterBar.y, filterBar.width, filterBar.height),
        Qt.rect(palette.x, palette.y, palette.width, palette.height),
        shapeFlyout.visible ? Qt.rect(shapeFlyout.x, shapeFlyout.y, shapeFlyout.width, shapeFlyout.height)
                            : Qt.rect(0, 0, 0, 0),
        Qt.rect(cubeColumn.x, cubeColumn.y, cubeColumn.width, cubeColumn.height),
        Qt.rect(navBar.x, navBar.y, navBar.width, navBar.height)
    ]

    // How the mouse does it depends on the style chosen in the View menu.
    readonly property string orbitHint: {
        switch (mouseStyle) {
        case "fusion": return qsTr("Orbit: Shift + middle mouse button – or drag this button")
        case "blender":
        case "sketchup": return qsTr("Orbit: drag with the middle mouse button – or drag this button")
        default: return qsTr("Orbit: drag with the right mouse button – or drag this button")
        }
    }
    readonly property string panHint: {
        switch (mouseStyle) {
        case "blender":
        case "sketchup": return qsTr("Pan: Shift + middle mouse button – or drag this button")
        default: return qsTr("Pan: drag with the middle mouse button – or drag this button")
        }
    }

    // --- display bar (top left) -------------------------------------------------------
    Row {
        id: displayBar

        x: 12
        y: 12
        spacing: 8

        Float {
            width: perspective.implicitWidth + 6
            height: 38

            AbstractButton {
                id: perspective

                x: 3
                y: 4
                implicitHeight: 30
                implicitWidth: perspectiveRow.implicitWidth + leftPadding + rightPadding
                horizontalPadding: 10
                hoverEnabled: true
                Accessible.name: root.orthographic ? qsTr("Orthographic") : qsTr("Perspective")
                onClicked: root.orthographicToggled()

                contentItem: Row {
                    id: perspectiveRow

                    spacing: 7

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.orthographic ? qsTr("Orthographic") : qsTr("Perspective")
                        color: perspective.hovered ? Theme.fg : Theme.fg3
                        font.pixelSize: Theme.fontSizeSmallPlus
                        elide: Text.ElideNone
                    }

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "chev"
                        size: 14
                        color: perspective.hovered ? Theme.fg : Theme.fg3
                    }
                }

                background: Rectangle {
                    radius: Theme.radius
                    color: "transparent"

                    Focusring {
                        visible: perspective.visualFocus
                    }
                }
            }
        }

        Float {
            width: displayRow.implicitWidth + 6
            height: 38

            Row {
                id: displayRow

                x: 3
                y: 4
                spacing: 2

                Repeater {
                    model: [
                        { value: "wire", text: qsTr("Wireframe"), icon: "wire" },
                        { value: "solid", text: qsTr("Solid"), icon: "solid" },
                        { value: "color", text: qsTr("Colours"), icon: "color" }
                    ]

                    delegate: AbstractButton {
                        id: displayButton

                        required property var modelData
                        readonly property bool selected: root.display === modelData.value

                        implicitHeight: 30
                        implicitWidth: displayContent.implicitWidth + leftPadding + rightPadding
                        horizontalPadding: 10
                        hoverEnabled: true
                        Accessible.name: modelData.text
                        Accessible.checkable: true
                        Accessible.checked: selected
                        onClicked: root.displayRequested(modelData.value)

                        contentItem: Row {
                            id: displayContent

                            spacing: 7

                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: displayButton.modelData.icon
                                size: 14
                                color: displayButton.selected || displayButton.hovered ? Theme.fg : Theme.fg3
                            }

                            Txt {
                                anchors.verticalCenter: parent.verticalCenter
                                text: displayButton.modelData.text
                                font.pixelSize: Theme.fontSizeSmallPlus
                                font.weight: displayButton.selected ? Font.DemiBold : Font.Normal
                                color: displayButton.selected || displayButton.hovered ? Theme.fg : Theme.fg3
                                elide: Text.ElideNone
                            }
                        }

                        background: Rectangle {
                            radius: Theme.radius
                            color: displayButton.selected ? Theme.field2 : "transparent"

                            Focusring {
                                visible: displayButton.visualFocus
                            }
                        }
                    }
                }
            }
        }
    }

    // --- what can be chosen (next to the display bar) -----------------------------------------
    readonly property var filterNames: ({
        all: qsTr("Everything"),
        body: qsTr("Bodies"),
        face: qsTr("Faces"),
        edge: qsTr("Edges"),
        vertex: qsTr("Corners")
    })

    Float {
        id: filterBar

        x: displayBar.x + displayBar.width + 8
        y: 12
        width: filterRow.implicitWidth + 26
        height: 38

        AbstractButton {
            id: filterButton

            x: 3
            y: 4
            implicitHeight: 30
            implicitWidth: filterRow.implicitWidth + leftPadding + rightPadding
            horizontalPadding: 10
            hoverEnabled: true
            Accessible.name: qsTr("Choose: %1").arg(root.filterNames[root.filter])
            onClicked: filterMenu.visible ? filterMenu.close() : filterMenu.popup(0, filterButton.height + 8)

            contentItem: Row {
                id: filterRow

                spacing: 7

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "select"
                    size: 14
                    color: filterButton.hovered ? Theme.fg : Theme.fg3
                }

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Choose: %1").arg(root.filterNames[root.filter])
                    color: filterButton.hovered ? Theme.fg : Theme.fg3
                    font.pixelSize: Theme.fontSizeSmallPlus
                    elide: Text.ElideNone
                }

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chev"
                    size: 14
                    color: filterButton.hovered ? Theme.fg : Theme.fg3
                }
            }

            background: Rectangle {
                radius: Theme.radius
                color: "transparent"

                Focusring {
                    visible: filterButton.visualFocus
                }
            }

            AppMenu {
                id: filterMenu

                items: ["all", "body", "face", "edge", "vertex"].map(id => ({
                    id: id, text: root.filterNames[id], checked: root.filter === id
                }))
                onActionTriggered: id => root.filterRequested(id)
            }
        }
    }

    // --- tool palette (left, 84 px) ------------------------------------------------------
    ToolCatalog {
        id: toolCatalog
    }

    Float {
        id: palette

        x: 12
        y: 60
        width: 84 // room for long German labels such as “Kombinieren”
        height: Math.min(paletteColumn.implicitHeight + 12, root.height - 72)

        Flickable {
            anchors.fill: parent
            anchors.margins: 6
            contentHeight: paletteColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: paletteColumn

                width: parent.width
                spacing: 2

                Repeater {
                    model: toolCatalog.tools

                    delegate: ToolButton3D {
                        required property var modelData

                        width: paletteColumn.width
                        text: modelData.name
                        iconName: modelData.icon
                        active: root.tool === modelData.id
                        tipText: modelData.tip
                        onClicked: root.toolRequested(modelData.id)
                    }
                }

                Item {
                    width: parent.width
                    height: 11

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.7
                        height: 1
                        color: Theme.line
                    }
                }

                Repeater {
                    id: groupRepeater

                    model: [
                        { id: "shapes", text: qsTr("Shapes"), icon: "cube",
                          tip: qsTr("Basic shapes, text and kits. With “As Hole” any shape becomes a hole.") },
                        { id: "sketch", text: qsTr("Sketch"), icon: "pencil",
                          tip: qsTr("2D drawing as a base, with measures and rules.") },
                        { id: "modify", text: qsTr("Modify"), icon: "round",
                          tip: qsTr("Pull, Round, Hollow and more.") },
                        { id: "combine", text: qsTr("Combine"), icon: "combine",
                          tip: qsTr("Merge, subtract or melt bodies together.") },
                        { id: "repeat", text: qsTr("Repeat"), icon: "pattern",
                          tip: qsTr("Arrange something several times or mirror it.") },
                        { id: "decorate", text: qsTr("Decorate"), icon: "sparkle",
                          tip: qsTr("Emboss, engrave, twist, generators.") },
                        { id: "check", text: qsTr("Check"), icon: "check",
                          tip: qsTr("Measure, section, print check, weight.") }
                    ]

                    delegate: ToolButton3D {
                        required property var modelData

                        width: paletteColumn.width
                        text: modelData.text
                        iconName: modelData.icon
                        expanded: root.flyout === modelData.id
                        tipText: modelData.tip
                        onClicked: root.groupRequested(modelData.id)
                    }
                }
            }
        }
    }

    // --- the shape gallery, next to its button in the palette ------------------------------------
    ShapeFlyout {
        id: shapeFlyout

        visible: root.flyout === "shapes"
        x: palette.x + palette.width + 10
        y: {
            const button = groupRepeater.itemAt(0)
            const wanted = button ? button.mapToItem(root, 0, 0).y - 6 : 60
            return Math.max(60, Math.min(wanted, root.height - height - 12))
        }
        width: Math.min(460, root.width - x - 130)
        onCloseRequested: root.flyoutCloseRequested()
    }

    // --- view cube and buttons (top right) -------------------------------------------------
    Column {
        id: cubeColumn

        x: root.width - 112 - 12
        y: 8
        spacing: 6

        ViewCube {
            yaw: root.cameraYaw
            pitch: root.cameraPitch
            onViewRequested: view => root.viewRequested(view)
            onDirectionRequested: (x, y, z) => root.directionRequested(x, y, z)
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6

            IconBtn {
                iconName: "home"
                tip: Qt.platform.os === "osx" ? qsTr("Home view (Home)") : qsTr("Home view (Pos1)")
                framed: true
                onClicked: root.viewRequested("home")
            }

            IconBtn {
                iconName: "max"
                tip: root.focusMode ? qsTr("Show side bars (Shift+F)") : qsTr("Focus: hide the side bars (Shift+F)")
                framed: true
                checkable: true
                checked: root.focusMode
                onClicked: root.focusToggled()
            }
        }
    }

    // --- navigation buttons (bottom right): show how the mouse does it, and work by dragging
    Float {
        id: navBar

        x: root.width - width - 12
        y: root.height - height - 14
        width: 42
        height: 3 * 34 + 2 * 2 + 8

        Column {
            x: 4
            y: 4
            spacing: 2

            NavButton {
                iconName: "orbit"
                label: qsTr("Orbit")
                hint: root.orbitHint
                onHintRequested: text => root.navigationHint(text)
                onDragged: (dx, dy) => root.orbitDragged(dx, dy)
            }

            NavButton {
                iconName: "hand"
                label: qsTr("Pan")
                hint: root.panHint
                onHintRequested: text => root.navigationHint(text)
                onDragged: (dx, dy) => root.panDragged(dx, dy)
            }

            NavButton {
                iconName: "zoom"
                label: qsTr("Zoom")
                hint: qsTr("Zoom: mouse wheel, towards the pointer – or drag this button")
                onHintRequested: text => root.navigationHint(text)
                onDragged: (dx, dy) => root.zoomRequested(Math.pow(1.01, dy))
            }
        }
    }
}
