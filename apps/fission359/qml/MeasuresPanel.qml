// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The named measures (AP-2.9), a fold-out area under the recipe: a table (name, formula, result,
// where it is used, a comment) and, as the other view, the measures that have a range as
// sliders: every model becomes a configurator. Renaming follows in all formulas as one step of
// the history; nothing here is a dialog.
Item {
    id: root

    /// Folded out (⌘M).
    property bool open: false
    /// "table" | "sliders"
    property string view: "table"
    /// The most height the area may take when open.
    property real maxHeight: 400
    /// The message catalogue, for the words of a mistake.
    property var catalog

    readonly property var measures: DocumentModel.measures
    readonly property int sliderCount: measures.filter(m => m.slider !== undefined).length
    // A measure asked to be deleted that something still uses: { name, text }, or null.
    property var removal: null
    // The name that did not work when renaming or adding: shown under the row.
    property string nameProblem: ""
    property string nameProblemFor: ""

    signal stepRequested(int index)
    signal openRequested(bool open)

    implicitHeight: open ? Math.min(maxHeight, header.height + body.contentHeight + 1) : header.height
    height: implicitHeight
    clip: true

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.line
    }

    // --- header: always there ------------------------------------------------------------
    Item {
        id: header

        width: parent.width
        height: 38

        MouseArea {
            anchors.fill: parent
            onClicked: root.openRequested(!root.open)
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Named measures")
            Accessible.checkable: true
            Accessible.checked: root.open
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            x: 14
            spacing: 8

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: root.open ? "▾" : "▸"
                color: Theme.fg3
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Measures")
                color: Theme.fg
                font.weight: Font.DemiBold
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: root.measures.length > 0 ? String(root.measures.length) : ""
                color: Theme.fg4
            }
        }

        Row {
            visible: root.open
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 12
            spacing: 8

            Segmented {
                anchors.verticalCenter: parent.verticalCenter
                implicitHeight: 28
                accessibleName: qsTr("View")
                current: root.view
                model: [{ value: "table", text: qsTr("Table") }, { value: "sliders", text: qsTr("Sliders") }]
                onActivated: value => root.view = value
            }

            IconBtn {
                anchors.verticalCenter: parent.verticalCenter
                iconName: "plus"
                tip: qsTr("New measure")
                framed: true
                onClicked: {
                    const name = DocumentModel.suggestMeasureName(qsTr("Measure"))
                    root.nameProblem = DocumentModel.addMeasure(name, "10", "length")
                    root.view = "table"
                    root.nameProblemFor = ""
                    nameFocus.restart()
                }
            }
        }
    }

    // After "+": the new row's name is ready to type over.
    Timer {
        id: nameFocus

        interval: 60
        onTriggered: {
            const last = rows.itemAt(rows.count - 1)
            if (last) {
                last.focusName()
            }
        }
    }

    Flickable {
        id: body

        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        visible: root.open
        contentHeight: tableColumn.implicitHeight + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        Column {
            id: tableColumn

            width: body.width

            Txt {
                visible: root.view === "table" && root.measures.length === 0
                width: parent.width
                leftPadding: 14
                rightPadding: 14
                topPadding: 4
                bottomPadding: 8
                text: qsTr("A measure is a number with a name, like Width. Use it in any field and change it in one place. Right click a field and choose “Save as named measure”.")
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }

            // What deleting a measure that is used asks: not a dialog, a card in the panel.
            Rectangle {
                visible: root.removal !== null
                x: 8
                width: parent.width - 16
                height: removalColumn.implicitHeight + 20
                radius: 8
                color: Theme.amberSoft
                border.width: 1
                border.color: Theme.amberLine

                Column {
                    id: removalColumn

                    x: 10
                    y: 10
                    width: parent.width - 20
                    spacing: 8

                    Txt {
                        width: parent.width
                        text: root.removal !== null ? root.removal.text : ""
                        color: Theme.fg
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                    }

                    Row {
                        spacing: 6

                        Btn {
                            text: qsTr("Delete anyway")
                            variant: "primary"
                            onClicked: {
                                DocumentModel.removeMeasure(root.removal.name)
                                root.removal = null
                            }
                        }

                        Btn {
                            text: qsTr("Keep")
                            onClicked: root.removal = null
                        }
                    }
                }
            }

            // --- the table ------------------------------------------------------------
            Repeater {
                id: rows

                model: root.view === "table" ? root.measures : []

                delegate: Item {
                    id: row

                    required property var modelData
                    required property int index

                    readonly property bool hasSlider: modelData.slider !== undefined
                    property bool more: false

                    function focusName() {
                        nameField.focusAndSelect()
                    }

                    width: tableColumn.width
                    height: rowColumn.implicitHeight + 16

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.line
                    }

                    Column {
                        id: rowColumn

                        x: 14
                        y: 8
                        width: parent.width - 28
                        spacing: 6

                        Row {
                            width: parent.width
                            spacing: 8

                            WordField {
                                id: nameField

                                width: parent.width * 0.42
                                value: row.modelData.name
                                invalid: root.nameProblemFor === row.modelData.name && root.nameProblem !== ""
                                onCommitted: text => {
                                    if (text === row.modelData.name) {
                                        return
                                    }
                                    const problem = DocumentModel.renameMeasure(row.modelData.name, text)
                                    root.nameProblem = problem
                                    root.nameProblemFor = problem === "" ? "" : row.modelData.name
                                    if (problem !== "") {
                                        nameField.refresh()
                                    }
                                }
                            }

                            NumberField {
                                id: formula

                                width: parent.width - nameField.width - parent.spacing
                                kind: row.modelData.kind
                                unit: row.modelData.unit
                                decimals: 2
                                value: row.modelData.value !== undefined ? row.modelData.value : 0
                                expression: row.modelData.expression
                                invalid: row.modelData.error !== undefined
                                onTextCommitted: text => DocumentModel.setMeasureExpression(row.modelData.name, text)
                            }
                        }

                        // The result when the formula is not just a number.
                        Txt {
                            visible: row.modelData.valid && row.modelData.expression.trim() !== Numbers.format(row.modelData.value, 2)
                            text: "= " + Numbers.format(row.modelData.value, 2) + (row.modelData.unit !== "" ? " " + row.modelData.unit : "")
                            color: Theme.fg3
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        Txt {
                            visible: root.nameProblemFor === row.modelData.name && root.nameProblem !== ""
                            width: parent.width
                            text: root.nameProblem === "duplicateName"
                                ? qsTr("Another measure has this name already.")
                                : qsTr("A name uses letters, digits and _ and starts with a letter.")
                            color: Theme.bad
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                        }

                        Txt {
                            visible: row.modelData.error !== undefined
                            width: parent.width
                            text: row.modelData.error !== undefined && root.catalog ? root.catalog.measureErrorText(row.modelData.error) : ""
                            color: Theme.bad
                            font.pixelSize: Theme.fontSizeSmall
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                        }

                        // Where it is used: a click goes to the step.
                        Flow {
                            visible: row.modelData.usedIn.length > 0 || row.modelData.usedByMeasures.length > 0
                            width: parent.width
                            spacing: 6

                            Txt {
                                text: qsTr("Used in")
                                color: Theme.fg4
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            Repeater {
                                model: row.modelData.usedIn

                                delegate: Rectangle {
                                    required property int modelData
                                    required property int index

                                    width: stepText.implicitWidth + 12
                                    height: 20
                                    radius: 10
                                    color: Theme.field

                                    Txt {
                                        id: stepText

                                        anchors.centerIn: parent
                                        text: qsTr("Step %1").arg(row.modelData.usedInNumbers[index])
                                        color: Theme.fg2
                                        font.pixelSize: Theme.fontSizeSmall
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.stepRequested(parent.modelData)
                                    }
                                }
                            }

                            Repeater {
                                model: row.modelData.usedByMeasures

                                delegate: Rectangle {
                                    required property string modelData

                                    width: measureText.implicitWidth + 12
                                    height: 20
                                    radius: 10
                                    color: Theme.field

                                    Txt {
                                        id: measureText

                                        anchors.centerIn: parent
                                        text: parent.modelData
                                        color: Theme.fg2
                                        font.pixelSize: Theme.fontSizeSmall
                                        mono: true
                                    }
                                }
                            }
                        }

                        Row {
                            spacing: 10

                            AbstractButton {
                                id: moreButton

                                height: 22
                                Accessible.name: qsTr("More")
                                Accessible.checkable: true
                                Accessible.checked: row.more
                                onClicked: row.more = !row.more
                                contentItem: Txt {
                                    text: (row.more ? "▾ " : "▸ ") + qsTr("More")
                                    color: Theme.fg3
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                background: Item {}
                            }

                            AbstractButton {
                                height: 22
                                Accessible.name: qsTr("Delete measure")
                                onClicked: {
                                    const uses = DocumentModel.measureUses(row.modelData.name)
                                    if (uses.steps.length === 0 && uses.measures.length === 0) {
                                        DocumentModel.removeMeasure(row.modelData.name)
                                        return
                                    }
                                    const where = []
                                    for (let i = 0; i < uses.numbers.length; ++i) {
                                        where.push(qsTr("step %1").arg(uses.numbers[i]))
                                    }
                                    for (let i = 0; i < uses.measures.length; ++i) {
                                        where.push(uses.measures[i])
                                    }
                                    root.removal = {
                                        name: row.modelData.name,
                                        text: qsTr("“%1” is used in %2. If you delete it, those places show a mistake until you give them another value.")
                                            .arg(row.modelData.name).arg(where.join(", "))
                                    }
                                }
                                contentItem: Txt {
                                    text: qsTr("Delete")
                                    color: Theme.bad
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                                background: Item {}
                            }
                        }

                        // "More": the comment and the range of the slider.
                        Column {
                            visible: row.more
                            width: parent.width
                            spacing: 6

                            WordField {
                                width: parent.width
                                label: qsTr("Comment")
                                value: row.modelData.comment
                                onCommitted: text => DocumentModel.setMeasureComment(row.modelData.name, text)
                            }

                            Toggle {
                                text: qsTr("Slider")
                                checked: row.hasSlider
                                onToggled: {
                                    if (checked) {
                                        const value = row.modelData.value !== undefined ? row.modelData.value : 10
                                        DocumentModel.setMeasureSlider(row.modelData.name, 0, Math.max(100, value * 2), 1)
                                    } else {
                                        DocumentModel.clearMeasureSlider(row.modelData.name)
                                    }
                                }
                            }

                            Row {
                                visible: row.hasSlider
                                width: parent.width
                                spacing: 8

                                Repeater {
                                    model: row.hasSlider ? [
                                        { key: "min", label: qsTr("Smallest") },
                                        { key: "max", label: qsTr("Largest") },
                                        { key: "step", label: qsTr("Step") }
                                    ] : []

                                    delegate: NumberField {
                                        required property var modelData

                                        width: (parent.width - 16) / 3
                                        label: modelData.label
                                        kind: "plain"
                                        unit: ""
                                        decimals: 2
                                        value: row.modelData.slider[modelData.key]
                                        onTextCommitted: text => {
                                            const typed = Numbers.evaluate(text, "plain")
                                            if (isNaN(typed)) {
                                                return
                                            }
                                            const range = {
                                                min: row.modelData.slider.min,
                                                max: row.modelData.slider.max,
                                                step: row.modelData.slider.step
                                            }
                                            range[modelData.key] = typed
                                            DocumentModel.setMeasureSlider(row.modelData.name, range.min, range.max, range.step)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // --- the sliders ----------------------------------------------------------
            Txt {
                visible: root.view === "sliders" && root.sliderCount === 0
                width: parent.width
                leftPadding: 14
                rightPadding: 14
                topPadding: 4
                bottomPadding: 8
                text: qsTr("No measure has a slider yet. In the table, open “More” of a measure and switch on “Slider”.")
                color: Theme.fg4
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }

            Repeater {
                model: root.view === "sliders" ? root.measures.filter(m => m.slider !== undefined) : []

                delegate: Item {
                    id: sliderRow

                    required property var modelData

                    width: tableColumn.width
                    height: sliderColumn.implicitHeight + 16

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.line
                    }

                    Column {
                        id: sliderColumn

                        x: 14
                        y: 8
                        width: parent.width - 28
                        spacing: 4

                        Row {
                            width: parent.width

                            Txt {
                                width: parent.width - valueText.implicitWidth
                                text: sliderRow.modelData.name
                                color: Theme.fg
                            }

                            Txt {
                                id: valueText

                                text: sliderRow.modelData.valid
                                    ? Numbers.format(sliderRow.modelData.value, 2) + (sliderRow.modelData.unit !== "" ? " " + sliderRow.modelData.unit : "")
                                    : "–"
                                mono: true
                                color: Theme.fg2
                            }
                        }

                        Slider {
                            id: slider

                            width: parent.width
                            from: sliderRow.modelData.slider.min
                            to: sliderRow.modelData.slider.max
                            stepSize: sliderRow.modelData.slider.step
                            snapMode: Slider.SnapAlways
                            value: sliderRow.modelData.valid ? sliderRow.modelData.value : sliderRow.modelData.slider.min
                            Accessible.name: sliderRow.modelData.name

                            // A drag is one step of the history; the model follows while you drag.
                            onPressedChanged: {
                                if (pressed) {
                                    DocumentModel.beginChange(sliderRow.modelData.name)
                                } else {
                                    DocumentModel.commitChange()
                                }
                            }
                            onMoved: DocumentModel.setMeasureExpression(sliderRow.modelData.name, String(Number(value.toFixed(6))))
                        }
                    }
                }
            }
        }
    }
}
