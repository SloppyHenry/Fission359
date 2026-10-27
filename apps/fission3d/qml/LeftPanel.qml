// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// Left side bar: bodies and library on top, the recipe below.
Rectangle {
    id: root

    property string documentName: ""
    property string tab: "bodies"      // bodies | library
    property var bodies: []            // [{ name, color, visible, chosen }]
    property bool bedVisible: true
    property string bedText: ""
    property var recipe: []            // [{ text, state }]  state: ok | warning | fails | off | pending
    property int currentStep: -1       // the step whose values are shown (highlighted)
    signal stepRequested(int index)
    // The recipe as the person edits it (AP-1.9).
    signal stepHovered(int index, bool on)
    signal stepRenamed(int index, string text)
    signal stepDeleteRequested(int index)
    signal stepHideRequested(int index, bool hide)
    signal deleteConfirmed
    signal hideInstead
    signal deleteCancelled
    // What deleting would break, as a question in the panel: { text, canHide } or null.
    property var deletion: null
    // The time marker: the number of steps that count (-1: all of them).
    property int marker: -1
    signal markerRequested(int count)
    property bool markerHeld: false

    // The gap between rows (0 … number of steps) a marker would stand in if it were let go
    // `dy` pixels from where `line` is.
    function gapFor(line, dy) {
        const y = line.mapToItem(rows, 0, line.height / 2).y + dy
        let gap = 0
        for (let i = 0; i < root.recipe.length; ++i) {
            const row = stepRepeater.itemAt(i)
            if (row && row.mapToItem(rows, 0, row.height / 2).y < y) {
                gap = i + 1
            }
        }
        return gap
    }

    function markerDragged(line, dy) {
        const gap = gapFor(line, dy)
        const row = gap < root.recipe.length ? stepRepeater.itemAt(gap) : null
        ghost.y = (row ? row.mapToItem(recipeBox, 0, 0).y : rows.mapToItem(recipeBox, 0, rows.height).y) - 1
        ghost.visible = true
    }

    function markerReleased(line, dy) {
        ghost.visible = false
        const gap = gapFor(line, dy)
        root.markerRequested(gap >= root.recipe.length ? -1 : gap)
    }
    signal tabRequested(string tab)
    signal addRequested
    signal bodyVisibilityRequested(int index, bool visible)
    signal bodyChosen(int index, bool extend)
    signal bodyRenamed(int index, string text)
    signal bodyHovered(int index, bool on)
    signal bodyColorChosen(int index, string color)
    signal bedVisibilityRequested(bool visible)

    color: Theme.panel
    clip: true

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.line
    }

    // --- tabs ---------------------------------------------------------------------------
    Item {
        id: tabs

        width: parent.width
        height: 46

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 6
            height: parent.height
            spacing: 2

            Repeater {
                model: [
                    { value: "bodies", text: qsTr("Bodies") },
                    { value: "library", text: qsTr("Library") }
                ]

                delegate: AbstractButton {
                    id: tabButton

                    required property var modelData
                    readonly property bool selected: root.tab === modelData.value

                    height: 46
                    implicitWidth: label.implicitWidth + 20
                    hoverEnabled: true
                    Accessible.name: modelData.text
                    onClicked: root.tabRequested(modelData.value)

                    contentItem: Txt {
                        id: label

                        text: tabButton.modelData.text
                        color: tabButton.selected ? Theme.fg : Theme.fg3
                        font.weight: tabButton.selected ? Font.DemiBold : Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideNone
                    }

                    background: Rectangle {
                        color: "transparent"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 2
                            color: tabButton.selected ? Theme.fg : "transparent"
                        }

                        Focusring {
                            visible: tabButton.visualFocus
                        }
                    }
                }
            }
        }

        IconBtn {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            buttonSize: 26
            iconName: "plus"
            tip: qsTr("Add shape")
            onClicked: root.addRequested()
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.line
        }
    }

    // --- body list ----------------------------------------------------------------------
    Flickable {
        id: listArea

        anchors.top: tabs.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: recipeBox.top
        contentHeight: listColumn.implicitHeight + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: listColumn

            x: 10
            y: 10
            width: parent.width - 20
            spacing: 10

            // search field
            Rectangle {
                width: parent.width
                height: 32
                radius: 7
                color: Theme.field
                border.width: 1
                border.color: search.activeFocus ? Theme.accentLine : Theme.line

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "search"
                        color: Theme.fg4
                    }

                    TextInput {
                        id: search

                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 24
                        color: Theme.fg
                        font.family: Theme.fontFamilies[0]
                        font.pixelSize: Theme.fontSize
                        clip: true
                        Accessible.name: qsTr("Search bodies")

                        Txt {
                            visible: search.text === "" && !search.activeFocus
                            text: root.tab === "bodies" ? qsTr("Search bodies") : qsTr("Search building blocks and motifs")
                            color: Theme.fg4
                        }
                    }
                }
            }

            // bodies
            Column {
                visible: root.tab === "bodies"
                width: parent.width
                spacing: 1

                TreeRow {
                    visible: root.bodies.filter(body => !body.consumed).length > 0
                    group: true
                    iconName: "folder"
                    text: root.documentName
                    count: qsTr("%n body(ies)", "", root.bodies.filter(body => !body.consumed).length)
                    hasEye: false
                }

                Repeater {
                    // The rows stay when only a value of a body changes (a choice changes
                    // `chosen`): a row that is rebuilt between the two clicks of a double click
                    // could not be renamed. So the model is the number of bodies.
                    model: root.bodies.length

                    delegate: TreeRow {
                        required property int index
                        readonly property var modelData: index < root.bodies.length ? root.bodies[index] : ({})

                        objectName: "bodyRow_" + index
                        // A body that is part of a result is not a body of its own (hidden, its
                        // position in the list stays the position in the document).
                        visible: !modelData.consumed
                        child: true
                        cut: modelData.cut === true
                        selected: modelData.chosen
                        swatch: modelData.color
                        hole: modelData.hole === true
                        badge: modelData.hole === true ? qsTr("Hole") : ""
                        renamable: true
                        colorEditable: true
                        text: modelData.name
                        eyeOn: modelData.visible
                        eyeLabel: qsTr("Visibility: %1").arg(modelData.name)
                        onEyeToggled: root.bodyVisibilityRequested(index, !modelData.visible)
                        onClicked: extend => root.bodyChosen(index, extend)
                        onRenamed: text => root.bodyRenamed(index, text)
                        onHovered: on => root.bodyHovered(index, on)
                        onSwatchClicked: (x, y) => {
                            colorPopup.body = index
                            const spot = mapToItem(root, x, y)
                            colorPopup.x = Math.max(8, Math.min(spot.x - 20, root.width - colorPopup.width - 8))
                            colorPopup.y = spot.y + 4
                            colorPopup.open()
                        }
                    }
                }

                Txt {
                    visible: root.bodies.filter(body => !body.consumed).length === 0
                    width: parent.width
                    leftPadding: 8
                    rightPadding: 8
                    topPadding: 6
                    text: qsTr("No bodies yet. Get a shape from “Shapes” in the tool bar or with %1.")
                          .arg(Qt.platform.os === "osx" ? "⌘K" : qsTr("Ctrl+K"))
                    color: Theme.fg4
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    lineHeight: 1.25
                }

                TreeRow {
                    iconName: "printer"
                    text: qsTr("Print bed")
                    count: root.bedText
                    eyeOn: root.bedVisible
                    eyeLabel: qsTr("Show print bed")
                    onEyeToggled: root.bedVisibilityRequested(!root.bedVisible)
                }
            }

            // library (placeholder)
            Column {
                visible: root.tab === "library"
                width: parent.width
                spacing: 6

                Txt {
                    text: qsTr("Building blocks")
                    color: Theme.fg3
                    font.weight: Font.DemiBold
                    font.pixelSize: Theme.fontSizeSmall
                }

                Repeater {
                    model: [qsTr("Box with lid"), qsTr("Hinge"), qsTr("Snap fit"), qsTr("Hanging loop"),
                            qsTr("Gear"), qsTr("Screw hole")]

                    delegate: LibraryCard {
                        required property string modelData

                        width: listColumn.width
                        text: modelData
                    }
                }

                Txt {
                    topPadding: 8
                    text: qsTr("Motifs")
                    color: Theme.fg3
                    font.weight: Font.DemiBold
                    font.pixelSize: Theme.fontSizeSmall
                }

                Grid {
                    columns: 2
                    spacing: 6
                    width: parent.width

                    Repeater {
                        model: [qsTr("Fir tree"), qsTr("Star"), qsTr("Snowflake"), qsTr("Heart")]

                        delegate: LibraryCard {
                            required property string modelData

                            width: (listColumn.width - 6) / 2
                            text: modelData
                        }
                    }
                }
            }
        }
    }

    // --- recipe -------------------------------------------------------------------------
    Item {
        id: recipeBox

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Math.min(parent.height * 0.46, recipeHeader.height + recipeBody.implicitHeight)

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.line
        }

        Rectangle {
            id: ghost

            visible: false
            x: 8
            width: parent.width - 16
            height: 2
            radius: 1
            color: Theme.accent
            opacity: 0.7
            z: 3
        }

        Item {
            id: recipeHeader

            width: parent.width
            height: 40

            Txt {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Recipe")
                color: Theme.fg
                font.weight: Font.DemiBold
                elide: Text.ElideNone
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: kbd.implicitWidth + 12
                height: 20
                radius: 4
                color: Theme.field
                border.width: 1
                border.color: Theme.line2

                Txt {
                    id: kbd

                    anchors.centerIn: parent
                    text: Qt.platform.os === "osx" ? "⌘Z" : qsTr("Ctrl+Z")
                    mono: true
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.Medium
                    elide: Text.ElideNone
                }
            }
        }

        // The steps scroll when there are more than fit; the header stays.
        Flickable {
            id: recipeScroll

            anchors.top: recipeHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            contentHeight: recipeBody.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            interactive: !root.markerHeld

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }

            Column {
                id: recipeBody

                width: recipeScroll.width

            // What deleting asks: the steps that would need help, with the ways out.
            Rectangle {
                visible: root.deletion !== null
                x: 8
                width: parent.width - 16
                height: card.implicitHeight + 20
                radius: 8
                color: Theme.amberSoft
                border.width: 1
                border.color: Theme.amberLine

                Column {
                    id: card

                    x: 10
                    y: 10
                    width: parent.width - 20
                    spacing: 8

                    Txt {
                        width: parent.width
                        text: root.deletion !== null ? root.deletion.text : ""
                        color: Theme.fg
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                    }

                    Flow {
                        width: parent.width
                        spacing: 6

                        Btn {
                            objectName: "deleteAnyway"
                            small: true
                            variant: "primary"
                            text: qsTr("Delete anyway")
                            onClicked: root.deleteConfirmed()
                        }

                        Btn {
                            objectName: "hideInstead"
                            small: true
                            visible: root.deletion !== null && root.deletion.canHide
                            text: qsTr("Only hide")
                            onClicked: root.hideInstead()
                        }

                        Btn {
                            objectName: "deleteCancel"
                            small: true
                            text: qsTr("Cancel")
                            onClicked: root.deleteCancelled()
                        }
                    }
                }
            }

            Column {
                id: rows

                x: 8
                width: parent.width - 16

                Repeater {
                    id: stepRepeater

                    model: root.recipe
                    // A new step shows at the bottom of the list.
                    onCountChanged: Qt.callLater(() => {
                        recipeScroll.contentY = Math.max(0, recipeScroll.contentHeight - recipeScroll.height)
                    })

                    delegate: Column {
                        id: stepItem

                        required property var modelData
                        required property int index

                        width: rows.width

                        MarkerLine {
                            id: markerHere

                            objectName: "marker"
                            visible: stepItem.index === root.marker
                            height: visible ? 28 : 0
                            atEnd: false
                            onDragMoved: dy => root.markerDragged(markerHere, dy)
                            onReleased: dy => root.markerReleased(markerHere, dy)
                            onReset: root.markerRequested(-1)
                            onHeldChanged: root.markerHeld = held
                        }

                        RecipeRow {
                            id: stepRow

                            objectName: "recipeRow_" + stepItem.index
                            position: stepItem.index
                            iconName: stepItem.modelData.icon
                            sentence: stepItem.modelData.text
                            label: stepItem.modelData.label
                            state: stepItem.modelData.state
                            problem: stepItem.modelData.problem
                            problemActions: stepItem.modelData.problemActions
                            onEditRequested: root.stepRequested(stepItem.index)
                            onHideRequested: root.stepHideRequested(stepItem.index, true)
                            selected: stepItem.index === root.currentStep
                            beyondMarker: root.marker >= 0 && stepItem.index >= root.marker
                            onClicked: root.stepRequested(stepItem.index)
                            onHovered: on => root.stepHovered(stepItem.index, on)
                            onRenamed: text => root.stepRenamed(stepItem.index, text)
                            onDeleteRequested: root.stepDeleteRequested(stepItem.index)
                            onMenuRequested: (x, y) => {
                                rowMenu.step = stepItem.index
                                const spot = stepRow.mapToItem(root, x, y)
                                rowMenu.x = Math.min(spot.x, root.width - rowMenu.width - 8)
                                rowMenu.y = Math.max(8, spot.y - rowMenu.height / 2)
                                rowMenu.open()
                            }
                        }

                        function startRename() { stepRow.startRename() }
                    }
                }
            }

            // the time marker at the end: everything counts
            MarkerLine {
                id: markerEnd

                objectName: "markerEnd"
                x: 8
                width: parent.width - 16
                visible: root.marker < 0 || root.marker >= root.recipe.length
                height: 28
                atEnd: true
                onDragMoved: dy => root.markerDragged(markerEnd, dy)
                onReleased: dy => root.markerReleased(markerEnd, dy)
                onReset: root.markerRequested(-1)
                onHeldChanged: root.markerHeld = held
            }

            Txt {
                width: parent.width
                leftPadding: 14
                rightPadding: 14
                topPadding: 6
                bottomPadding: 12
                text: qsTr("Every change appears here as a sentence.")
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
            }
        }
    }

    RowMenu {
        id: rowMenu

        property int step: -1
        readonly property bool hidden: step >= 0 && step < root.recipe.length && root.recipe[step].state === "off"

        entries: [
            { id: "rename", text: qsTr("Rename"), icon: "pencil" },
            { id: "hide", text: hidden ? qsTr("Show") : qsTr("Hide"), icon: hidden ? "eye" : "eyeoff" },
            { id: "delete", text: qsTr("Delete"), icon: "x", danger: true }
        ]
        onChosen: id => {
            if (id === "rename") {
                stepRepeater.itemAt(step)?.startRename()
            } else if (id === "hide") {
                root.stepHideRequested(step, !hidden)
            } else {
                root.stepDeleteRequested(step)
            }
        }
    }

    // The colours of one body, next to its row: not a dialog, a click beside closes it.
    Popup {
        id: colorPopup

        property int body: -1

        width: root.width - 16
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.radiusFloat
            color: Theme.raise
            border.width: 1
            border.color: Theme.line2
        }

        contentItem: ColorField {
            current: colorPopup.body >= 0 && colorPopup.body < root.bodies.length ? root.bodies[colorPopup.body].color : ""
            onChosen: color => {
                root.bodyColorChosen(colorPopup.body, color)
                colorPopup.close()
            }
        }
    }
}
