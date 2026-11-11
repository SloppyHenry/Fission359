// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui
import Fission359.View

// The measures of the sketch (AP-2.2b): the labels ("30", "Ø 12", "R 5", "45°") at the anchor of
// each measure. A click on a label opens its expression as a number field: Enter changes the
// measure, Esc closes the field, Delete on an empty field removes the measure. A digit with
// chosen elements and no tool on (Main.qml) opens a field for a new measure of them.
Item {
    id: root

    objectName: "sketchMeasureLayer"
    property Viewport3D viewport
    // The id ("R…") of the measure whose label is a field now, "" when none.
    property string editing: ""
    // A field for a new measure of the chosen elements is open.
    property bool creating: false
    // A field has the keyboard (the digit shortcuts of Main.qml stay off then).
    readonly property bool typing: editing !== "" || creating

    // Only while a sketch is open.
    visible: Sketch.engaged

    // The glossary word for a measure kind, as the caption of a field.
    function kindWord(kind) {
        switch (kind) {
        case "length": return qsTr("Length")
        case "diameter": return qsTr("Diameter")
        case "radius": return qsTr("Radius")
        case "distance": return qsTr("Distance")
        case "angle": return qsTr("Angle")
        default: return ""
        }
    }

    // The kind for Numbers.evaluate: an angle is typed in degrees, the rest in millimetres.
    function inputKind(kind) {
        return kind === "angle" ? "angle" : "length"
    }

    // The word of a rule kind, on its button of the rule bar.
    function ruleWord(kind) {
        switch (kind) {
        case "horizontal": return qsTr("Horizontal")
        case "vertical": return qsTr("Vertical")
        case "fixed": return qsTr("Fixed")
        case "parallel": return qsTr("Parallel")
        case "perpendicular": return qsTr("Right angle")
        case "equal": return qsTr("Same size")
        case "concentric": return qsTr("Same centre")
        case "tangent": return qsTr("Touching")
        default: return ""
        }
    }

    // The tooltip of a rule kind: what the rule does.
    function ruleTip(kind) {
        switch (kind) {
        case "horizontal": return qsTr("Makes the line lie flat, left to right")
        case "vertical": return qsTr("Makes the line stand upright")
        case "fixed": return qsTr("Keeps it where it is")
        case "parallel": return qsTr("Makes the two lines run side by side")
        case "perpendicular": return qsTr("Makes the two lines meet at a right angle")
        case "equal": return qsTr("Makes both equally long or equally large")
        case "concentric": return qsTr("Gives both circles the same centre")
        case "tangent": return qsTr("Makes them touch without crossing")
        default: return ""
        }
    }

    // World → item pixels. Reading the camera values makes the bindings that call this follow it.
    function project(x, y, z) {
        if (!viewport) {
            return Qt.point(root.width / 2, root.height / 2)
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        return viewport.project(x, y, z)
    }

    // Where the field for a new measure opens: in the middle of the view (the chosen elements
    // are drawn highlighted, so the field needs no place of its own; a place next to them follows
    // with the measure handles of AP-2.2c).
    function anchorPoint() {
        return Qt.point(root.width / 2, root.height / 2)
    }

    // --- the labels of the measures ------------------------------------------------------

    // The field of the label being edited, so that closing can give the keyboard back.
    property var editField: null

    // A click on a label: its expression opens as a field, preselected.
    function openEdit(index) {
        const info = Sketch.measureLabels[index]
        if (!info) {
            return
        }
        cancelCreate()
        editing = info.id
        const part = chips.itemAt(index)
        const input = part ? part.field : null
        editField = input
        if (input) {
            input.invalid = false
            input.text = info.expression
            input.forceActiveFocus()
            input.selectAll()
        }
    }

    function closeEdit() {
        const wasEditing = editing !== ""
        editing = ""
        if (editField) {
            editField.focus = false
            editField = null
        }
        if (wasEditing) {
            root.forceActiveFocus() // the keyboard goes back to the layer (as in SketchLayer)
        }
    }

    // --- a new measure for the chosen elements -------------------------------------------

    // A digit was pressed with elements chosen and no tool on (Main.qml): the field opens with it.
    function startMeasure(first) {
        const kind = Sketch.measureKindForSelection()
        if (Sketch.toolActive || kind === "" || creating) {
            return
        }
        closeEdit()
        creator.kind = kind
        creating = true
        createInput.invalid = false
        createInput.text = first
        createInput.forceActiveFocus()
        createInput.cursorPosition = createInput.text.length
    }

    // Enter: the text is evaluated first (a wrong one stays open, with the reason under it).
    function acceptCreate() {
        if (!creating) {
            return
        }
        const text = createInput.text.trim()
        if (text === "") {
            cancelCreate()
            return
        }
        const kind = inputKind(creator.kind)
        if (isNaN(Numbers.evaluate(text, kind))) {
            createInput.errorInfo = Numbers.explain(text, kind)
            createInput.invalid = true
            return
        }
        if (Sketch.setMeasureForSelection(text)) {
            cancelCreate()
        } else {
            createInput.errorInfo = Numbers.explain(text, kind)
            createInput.invalid = true // the line under the field says why
        }
    }

    function cancelCreate() {
        const wasCreating = creating
        creating = false
        createInput.text = ""
        createInput.invalid = false
        createInput.focus = false
        if (wasCreating) {
            root.forceActiveFocus()
        }
    }

    function closeAll() {
        closeEdit()
        cancelCreate()
    }

    Connections {
        target: Sketch

        function onStateChanged() { if (!Sketch.engaged) root.closeAll() }
        function onToolChanged() { root.closeAll() }
        function onSelectionChanged() {
            if (Sketch.measureKindForSelection() === "") {
                root.cancelCreate()
            }
        }
    }

    Repeater {
        id: chips

        model: Sketch.measureLabels.length

        delegate: Item {
            id: part

            required property int index
            readonly property var info: index < Sketch.measureLabels.length ? Sketch.measureLabels[index] : null
            readonly property alias field: input
            readonly property bool open: part.info !== null && root.editing === part.info.id
            // Follows the camera: project() reads its values, so this is evaluated again.
            readonly property point at: part.info !== null
                ? root.project(part.info.x, part.info.y, part.info.z)
                : Qt.point(0, 0)

            anchors.fill: parent
            visible: part.info !== null

            Rectangle {
                id: chip

                x: Math.max(0, Math.min(root.width - width, part.at.x - width / 2))
                y: Math.max(0, Math.min(root.height - height, part.at.y - height / 2))
                width: chipRow.implicitWidth + 12
                height: 22
                radius: Theme.radius
                color: Theme.panel
                border.width: 1
                border.color: part.info && part.info.conflict ? Theme.bad
                    : part.open ? Theme.accentLine : Theme.line2
                z: part.open ? 10 : 1

                Row {
                    id: chipRow

                    anchors.centerIn: parent
                    spacing: 4

                    // The measure as it is; a conflict is shown in the warning colour.
                    Txt {
                        visible: !part.open
                        anchors.verticalCenter: parent.verticalCenter
                        text: part.info ? part.info.text : ""
                        mono: true
                        color: part.info && part.info.conflict ? Theme.bad : Theme.fg
                        font.weight: Font.Medium
                        font.pixelSize: Theme.fontSizeSmall
                        elide: Text.ElideNone
                    }

                    // The expression of the measure while the label is clicked.
                    ValueInput {
                        id: input

                        visible: part.open
                        anchors.verticalCenter: parent.verticalCenter
                        compact: true
                        kind: root.inputKind(part.info ? part.info.kind : "")
                        Accessible.name: root.kindWord(part.info ? part.info.kind : "")

                        Keys.onReturnPressed: finish()
                        Keys.onEnterPressed: finish()
                        Keys.onEscapePressed: root.closeEdit()
                        // Delete on the empty field removes the measure.
                        Keys.onDeletePressed: event => {
                            if (part.info !== null && input.text.trim() === "") {
                                Sketch.removeMeasure(part.info.id)
                                root.closeEdit()
                            } else {
                                event.accepted = false // the field deletes a character
                            }
                        }
                        // Backspace has no handler of its own in every Qt version: onPressed.
                        Keys.onPressed: event => {
                            if (event.key !== Qt.Key_Backspace) {
                                event.accepted = false
                                return
                            }
                            if (part.info !== null && input.text.trim() === "") {
                                Sketch.removeMeasure(part.info.id)
                                root.closeEdit()
                            } else {
                                event.accepted = false
                            }
                        }

                        function finish() {
                            if (!part.open) {
                                return
                            }
                            const text = input.text.trim()
                            if (text === "") {
                                root.closeEdit()
                                return
                            }
                            if (Sketch.changeMeasure(part.info.id, text)) {
                                root.closeEdit()
                            } else {
                                // Not taken: the field stays open and says why under it.
                                input.errorInfo = Numbers.explain(text, input.kind)
                                input.invalid = true
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !part.open
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: root.kindWord(part.info ? part.info.kind : "")
                    onClicked: root.openEdit(part.index)
                }
            }
        }
    }

    // The marks of the rules (AP-2.2c3): a small stroke symbol next to the element each rule
    // concerns. Hovering says the rule's name, a click on the symbol removes the rule.
    Repeater {
        id: ruleMarks

        model: Sketch.ruleMarks.length

        delegate: Item {
            id: mark

            required property int index
            readonly property var info: index < Sketch.ruleMarks.length ? Sketch.ruleMarks[index] : null
            // Follows the camera: project() reads its values, so this is evaluated again.
            readonly property point at: mark.info !== null
                ? root.project(mark.info.x, mark.info.y, mark.info.z)
                : Qt.point(0, 0)
            // "horizontal" → "ruleHorizontal", "pointOnCurve" → "rulePointOnCurve"
            readonly property string iconName: mark.info !== null
                ? "rule" + mark.info.kind.charAt(0).toUpperCase() + mark.info.kind.slice(1)
                : ""

            anchors.fill: parent
            visible: mark.info !== null

            Rectangle {
                id: badge

                x: Math.max(0, Math.min(root.width - width, mark.at.x + 10 + mark.info.slot * 22))
                y: Math.max(0, Math.min(root.height - height, mark.at.y + 8))
                width: 20
                height: 20
                radius: Theme.radius
                color: Theme.panel
                border.width: 1
                border.color: area.containsMouse ? Theme.accentLine : Theme.line2
                z: 2

                Icon {
                    anchors.centerIn: parent
                    name: mark.iconName
                    size: 14
                    color: Theme.fg
                }

                MouseArea {
                    id: area

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    Accessible.role: Accessible.Button
                    Accessible.name: root.ruleWord(mark.info ? mark.info.kind : "")
                    onClicked: Sketch.removeRule(mark.info.id)
                }

                ToolTip.visible: area.containsMouse
                ToolTip.text: root.ruleWord(mark.info ? mark.info.kind : "") + " – "
                              + qsTr("click to remove this rule")
                ToolTip.delay: 700
            }
        }
    }

    // The field for a new measure of the chosen elements, with the caption of its kind.
    Rectangle {
        id: creator

        property string kind: ""
        readonly property point at: root.anchorPoint()

        x: Math.max(0, Math.min(root.width - width, at.x - width / 2))
        y: Math.max(0, Math.min(root.height - height, at.y - height - 8))
        width: createRow.implicitWidth + 20
        height: 30
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: Theme.accentLine
        visible: root.creating
        z: 10

        Row {
            id: createRow

            anchors.centerIn: parent
            spacing: 8

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: root.kindWord(creator.kind)
                color: Theme.fg2
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }

            ValueInput {
                id: createInput

                compact: true
                kind: root.inputKind(creator.kind)
                Accessible.name: root.kindWord(creator.kind)

                Keys.onReturnPressed: root.acceptCreate()
                Keys.onEnterPressed: root.acceptCreate()
                Keys.onEscapePressed: root.cancelCreate()
                Keys.onDeletePressed: event => {
                    if (createInput.text.trim() === "") {
                        root.cancelCreate()
                    } else {
                        event.accepted = false
                    }
                }
                // Backspace has no handler of its own in every Qt version: onPressed.
                Keys.onPressed: event => {
                    if (event.key !== Qt.Key_Backspace) {
                        event.accepted = false
                        return
                    }
                    if (createInput.text.trim() === "") {
                        root.cancelCreate()
                    } else {
                        event.accepted = false
                    }
                }
            }
        }
    }

    // The rule bar: one button per rule that fits the chosen elements, at the bottom centre (no
    // tool on, nothing typed). Sketch.selectedElements is read so the bar follows a new choice.
    Rectangle {
        id: ruleBar

        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        width: ruleRow.implicitWidth + 12
        height: 40
        radius: Theme.radiusFloat
        color: Theme.raise
        border.width: 1
        border.color: Theme.line2
        visible: !Sketch.toolActive && !root.typing
                 && Sketch.selectedElements.length > 0
                 && Sketch.rulesForSelection().length > 0
        z: 10

        Row {
            id: ruleRow

            x: 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: {
                    Sketch.selectedElements // a new choice: the rules are looked up again
                    return Sketch.rulesForSelection()
                }

                delegate: Btn {
                    id: ruleButton

                    required property string modelData

                    small: true
                    text: root.ruleWord(modelData)
                    iconName: "rule" + modelData.charAt(0).toUpperCase() + modelData.slice(1)
                    ToolTip.visible: ruleButton.hovered
                    ToolTip.text: root.ruleTip(modelData)
                    ToolTip.delay: 700
                    onClicked: Sketch.addRuleForSelection(modelData)
                }
            }
        }
    }
}
