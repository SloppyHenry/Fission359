// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// Right side bar: properties of the selection ("Object") and the printer.
Rectangle {
    id: root

    property string stepTitle: ""
    // What is chosen: none | body (one body, or a step picked in the recipe) | bodies | part
    // (a face, an edge or a corner of one body). Each has its own set of sections.
    property string mode: "none"
    property string bodyName: ""
    property bool renamable: false
    property string partText: ""        // "Face of Box 1"
    property int bodyCount: 0
    property bool canCombine: false
    signal nameCommitted(string name)
    signal combineRequested(string way)
    // The values of one step: shown for one body and for a part of it.
    readonly property bool showsStep: tab === "object" && (mode === "body" || mode === "part")
                                      && stepValues.length > 0
    // "More": what is rarely changed stays folded away until asked for.
    property bool moreOpen: false
    property var stepValues: []        // [{ name, label, group, expression, value, kind, error }]
    property var stepTexts: []         // [{ name, value, kind }] kind: text | font
    property var fonts: []             // names of the fonts lettering can use
    property var fontStyles: []        // the styles the chosen font has: regular | bold | italic | boldItalic
    readonly property string currentStyle: {
        const entry = stepTexts.find(t => t.kind === "style")
        return entry ? entry.value : "regular"
    }
    readonly property bool isBold: currentStyle === "bold" || currentStyle === "boldItalic"
    readonly property bool isItalic: currentStyle === "italic" || currentStyle === "boldItalic"
    function styleWith(bold, italic) {
        return bold ? (italic ? "boldItalic" : "bold") : (italic ? "italic" : "regular")
    }
    signal valueCommitted(string name, string text)
    // Dragging the name of a field changes its value as one step of the history.
    signal dragStarted(string name)
    signal dragFinished(bool accepted)
    signal textCommitted(string name, string text)
    function focusText() {
        wordRepeater.itemAt(0)?.focusAndSelect()
    }
    readonly property var sizeValues: stepValues.filter(v => v.group === "size" && !v.advanced)
    readonly property var advancedSizeValues: stepValues.filter(v => v.group === "size" && v.advanced)
    readonly property var positionValues: stepValues.filter(v => v.group === "position")
    readonly property var rotationValues: stepValues.filter(v => v.group === "rotation")
    // "Go all the way through": a switch for a hole whose shape runs straight (a value 0 or 1).
    readonly property var throughValue: stepValues.find(v => v.group === "through")
    property bool isHole: false
    // A split: the axis it cuts across (0 = x, 1 = y, 2 = z) and where.
    readonly property var axisValue: stepValues.find(v => v.group === "axis")
    readonly property var cutValues: stepValues.filter(v => v.group === "cut")
    signal axisChosen(int axis)
    // The fields stay when only their numbers change (a field being dragged or stepped with the
    // arrow keys must not be rebuilt under the hand): the repeaters get the names, which differ
    // only when the shape has other fields.
    function valueNamed(name) {
        return stepValues.find(v => v.name === name)
            ?? ({ name: name, label: "", kind: "length", axis: "", value: 0, expression: "", error: false })
    }
    readonly property string sizeKey: sizeValues.map(v => v.name).join("|")
    readonly property string advancedSizeKey: advancedSizeValues.map(v => v.name).join("|")
    readonly property var advancedSizeNames: advancedSizeKey === "" ? [] : advancedSizeKey.split("|")
    readonly property string positionKey: positionValues.map(v => v.name).join("|")
    readonly property string rotationKey: rotationValues.map(v => v.name).join("|")
    readonly property string cutKey: cutValues.map(v => v.name).join("|")
    readonly property var sizeNames: sizeKey === "" ? [] : sizeKey.split("|")
    readonly property var positionNames: positionKey === "" ? [] : positionKey.split("|")
    readonly property var rotationNames: rotationKey === "" ? [] : rotationKey.split("|")
    readonly property var cutNames: cutKey === "" ? [] : cutKey.split("|")
    // The switch "keep the tools" of a combine step.
    readonly property var keepToolsText: stepTexts.find(t => t.name === "keepTools")
    readonly property var fuzzyText: stepTexts.find(t => t.name === "fuzzy")
    readonly property bool subtracts: stepTexts.some(t => t.name === "mode" && t.value === "subtract")
    readonly property bool hasMore: advancedSizeValues.length > 0 || (isHole && throughValue !== undefined)
                                    || fuzzyText !== undefined
    property string tab: "object"      // object | printer
    property string printerName: ""
    property string bedText: ""
    property string nozzleText: ""
    property string printerId: ""
    property var printers: []          // [{ id, name, bedX, bedY, bedZ, nozzle }]
    signal tabRequested(string tab)
    signal printerChosen(string id)

    color: Theme.panel

    CombineCatalog {
        id: combineCatalog
    }
    clip: true

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.line
    }

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
                    { value: "object", text: qsTr("Object") },
                    { value: "printer", text: qsTr("Printer") }
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

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.line
        }
    }

    Flickable {
        anchors.top: tabs.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content

            width: parent.width

            // header: what is selected
            PanelHeader {
                visible: root.tab === "object" && root.mode === "none"
                iconName: "select"
                title: qsTr("Nothing selected")
                subtitle: qsTr("Click a body to change it.")
            }

            BodyHeader {
                visible: root.tab === "object" && (root.mode === "body" || root.mode === "part")
                name: root.renamable ? root.bodyName : root.stepTitle
                typeName: root.renamable ? root.stepTitle : ""
                renamable: root.renamable
                subtitle: root.mode === "part" ? root.partText : qsTr("Change a value or type a formula.")
                onRenamed: name => root.nameCommitted(name)
            }

            PanelHeader {
                visible: root.tab === "object" && root.mode === "bodies"
                iconName: "cube"
                title: qsTr("%n body(ies) selected", "", root.bodyCount)
                subtitle: qsTr("Changes here apply to all of them.")
            }

            Section {
                visible: root.tab === "object" && root.mode === "bodies" && root.canCombine
                heading: qsTr("Combine")

                Row {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: combineCatalog.ways.filter(way => way.needs === undefined)

                        delegate: ToolButton3D {
                            required property var modelData

                            text: modelData.label
                            width: (parent.width - 16) / 3
                            iconName: modelData.icon
                            onClicked: root.combineRequested(modelData.id)
                        }
                    }
                }
            }

            Section {
                visible: root.showsStep && root.axisValue !== undefined
                heading: qsTr("Cut")

                Segmented {
                    accessibleName: qsTr("Cut across")
                    current: root.axisValue !== undefined ? Math.round(root.axisValue.value) : 2
                    model: [{ value: 0, text: "X" }, { value: 1, text: "Y" }, { value: 2, text: "Z" }]
                    onActivated: value => root.axisChosen(value)
                }

                Repeater {
                    model: root.cutNames

                    delegate: NumberField {
                        id: cutField

                        required property string modelData
                        readonly property var entry: root.valueNamed(modelData)

                        objectName: "field_" + modelData
                        width: parent.width
                        label: entry.label
                        kind: entry.kind
                        unit: "mm"
                        invalid: entry.error
                        onTextCommitted: text => root.valueCommitted(modelData, text)
                        draggable: true
                        onDragStarted: root.dragStarted(modelData)
                        onDragFinished: accepted => root.dragFinished(accepted)

                        Binding {
                            target: cutField
                            property: "value"
                            value: cutField.entry.value
                        }

                        Binding {
                            target: cutField
                            property: "expression"
                            value: cutField.entry.expression
                        }
                    }
                }
            }

            Section {
                visible: root.showsStep && root.stepTexts.some(t => t.kind === "text" || t.kind === "font" || t.kind === "style")
                heading: qsTr("Text")

                Repeater {
                    id: wordRepeater

                    model: root.stepTexts.filter(t => t.kind === "text")

                    delegate: WordField {
                        required property var modelData

                        width: parent.width
                        label: qsTr("Words")
                        value: modelData.value
                        onCommitted: text => root.textCommitted(modelData.name, text)
                    }
                }

                Repeater {
                    model: root.stepTexts.filter(t => t.kind === "font")

                    delegate: FontField {
                        required property var modelData

                        width: parent.width
                        label: qsTr("Font")
                        value: modelData.value
                        fonts: root.fonts
                        onChosen: name => root.textCommitted(modelData.name, name)
                    }
                }

                // Bold and italic: a button is available when the font has the style that
                // pressing it would give.
                Row {
                    visible: root.stepTexts.some(t => t.kind === "style")
                    spacing: 8

                    StyleButton {
                        letter: "B"
                        bold: true
                        tip: qsTr("Bold")
                        checked: root.isBold
                        enabled: root.fontStyles.indexOf(root.styleWith(!root.isBold, root.isItalic)) >= 0
                        onClicked: root.textCommitted("style", root.styleWith(!root.isBold, root.isItalic))
                    }

                    StyleButton {
                        letter: "I"
                        italic: true
                        tip: qsTr("Italic")
                        checked: root.isItalic
                        enabled: root.fontStyles.indexOf(root.styleWith(root.isBold, !root.isItalic)) >= 0
                        onClicked: root.textCommitted("style", root.styleWith(root.isBold, !root.isItalic))
                    }
                }
            }

            Section {
                visible: root.showsStep && root.sizeValues.length > 0
                heading: qsTr("Size")

                Repeater {
                    model: root.sizeNames

                    delegate: NumberField {
                        id: sizeField

                        required property string modelData
                        readonly property var entry: root.valueNamed(modelData)

                        objectName: "field_" + modelData
                        width: parent.width
                        label: entry.label
                        kind: entry.kind
                        unit: entry.kind === "angle" ? "°" : entry.kind === "plain" ? "" : "mm"
                        invalid: entry.error
                        onTextCommitted: text => root.valueCommitted(modelData, text)
                        draggable: true
                        onDragStarted: root.dragStarted(modelData)
                        onDragFinished: accepted => root.dragFinished(accepted)

                        Binding {
                            target: sizeField
                            property: "value"
                            value: sizeField.entry.value
                        }

                        Binding {
                            target: sizeField
                            property: "expression"
                            value: sizeField.entry.expression
                        }
                    }
                }
            }

            Section {
                visible: root.showsStep && root.positionValues.length > 0
                heading: qsTr("Position")

                Row {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: root.positionNames

                        delegate: NumberField {
                            id: positionField

                            required property string modelData
                            readonly property var entry: root.valueNamed(modelData)

                            objectName: "field_" + modelData
                            width: (parent.width - 16) / 3
                            axis: entry.axis
                            kind: entry.kind
                            unit: ""
                            invalid: entry.error
                            onTextCommitted: text => root.valueCommitted(modelData, text)
                            draggable: true
                            onDragStarted: root.dragStarted(modelData)
                            onDragFinished: accepted => root.dragFinished(accepted)

                            Binding {
                                target: positionField
                                property: "value"
                                value: positionField.entry.value
                            }

                            Binding {
                                target: positionField
                                property: "expression"
                                value: positionField.entry.expression
                            }
                        }
                    }
                }
            }

            Section {
                visible: root.showsStep && root.rotationValues.length > 0
                heading: qsTr("Rotation")

                Row {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: root.rotationNames

                        delegate: NumberField {
                            id: rotationField

                            required property string modelData
                            readonly property var entry: root.valueNamed(modelData)

                            objectName: "field_" + modelData
                            width: (parent.width - 16) / 3
                            axis: entry.axis
                            kind: entry.kind
                            unit: "°"
                            invalid: entry.error
                            onTextCommitted: text => root.valueCommitted(modelData, text)
                            draggable: true
                            onDragStarted: root.dragStarted(modelData)
                            onDragFinished: accepted => root.dragFinished(accepted)

                            Binding {
                                target: rotationField
                                property: "value"
                                value: rotationField.entry.value
                            }

                            Binding {
                                target: rotationField
                                property: "expression"
                                value: rotationField.entry.expression
                            }
                        }
                    }
                }
            }

            // "▸ More": the rarely changed values of the step, folded away.
            Item {
                visible: root.showsStep && root.hasMore
                width: parent.width
                height: 40

                AbstractButton {
                    id: moreButton

                    objectName: "moreButton"
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    height: 30
                    implicitWidth: moreLabel.implicitWidth + 12
                    hoverEnabled: true
                    Accessible.name: qsTr("More")
                    Accessible.checkable: true
                    Accessible.checked: root.moreOpen
                    onClicked: root.moreOpen = !root.moreOpen

                    contentItem: Txt {
                        id: moreLabel

                        text: (root.moreOpen ? "▾ " : "▸ ") + qsTr("More")
                        color: moreButton.hovered ? Theme.fg : Theme.fg3
                        font.weight: Font.Medium
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        radius: 6
                        color: moreButton.hovered ? Theme.field : "transparent"

                        Focusring {
                            visible: moreButton.visualFocus
                        }
                    }
                }
            }

            Section {
                visible: root.showsStep && root.moreOpen && root.advancedSizeValues.length > 0

                Repeater {
                    model: root.advancedSizeNames

                    delegate: NumberField {
                        id: advancedField

                        required property string modelData
                        readonly property var entry: root.valueNamed(modelData)

                        objectName: "field_" + modelData
                        width: parent.width
                        label: entry.label
                        kind: entry.kind
                        unit: entry.kind === "angle" ? "°" : entry.kind === "plain" ? "" : "mm"
                        invalid: entry.error
                        onTextCommitted: text => root.valueCommitted(modelData, text)
                        draggable: true
                        onDragStarted: root.dragStarted(modelData)
                        onDragFinished: accepted => root.dragFinished(accepted)

                        Binding {
                            target: advancedField
                            property: "value"
                            value: advancedField.entry.value
                        }

                        Binding {
                            target: advancedField
                            property: "expression"
                            value: advancedField.entry.expression
                        }
                    }
                }
            }

            Section {
                visible: root.showsStep && root.moreOpen && root.isHole && root.throughValue !== undefined
                heading: qsTr("Hole")

                Toggle {
                    text: qsTr("Goes all the way through")
                    checked: root.throughValue !== undefined && root.throughValue.value >= 0.5
                    onToggled: root.valueCommitted("through", checked ? "1" : "0")
                }
            }

            Section {
                visible: root.showsStep && root.moreOpen && root.fuzzyText !== undefined
                heading: qsTr("Combining")

                Toggle {
                    visible: root.subtracts
                    text: qsTr("Keep the tools as bodies")
                    checked: root.keepToolsText !== undefined && root.keepToolsText.value === "yes"
                    onToggled: root.textCommitted("keepTools", checked ? "yes" : "no")
                }

                // The way out when faces lie almost on each other and the result is not clean.
                Toggle {
                    text: qsTr("Make flush (closes gaps under 0.001 mm)")
                    checked: root.fuzzyText !== undefined && root.fuzzyText.value === "yes"
                    onToggled: root.textCommitted("fuzzy", checked ? "yes" : "no")
                }
            }

            Section {
                visible: root.tab === "object" && root.mode === "none"

                Txt {
                    width: parent.width
                    text: qsTr("Get new shapes from “Shapes” in the tool bar on the left – or press %1 and describe what you want to build.")
                          .arg(Qt.platform.os === "osx" ? "⌘K" : qsTr("Ctrl+K"))
                    color: Theme.fg4
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    lineHeight: 1.25
                }
            }

            PanelHeader {
                visible: root.tab === "printer"
                iconName: "printer"
                title: root.printerName
                subtitle: qsTr("Printer profile")
            }

            Section {
                visible: root.tab === "printer"

                InfoRow {
                    label: qsTr("Print bed")
                    value: root.bedText
                    mono: true
                }

                InfoRow {
                    label: qsTr("Nozzle")
                    value: root.nozzleText
                    mono: true
                }
            }

            Section {
                visible: root.tab === "printer"
                heading: qsTr("Choose printer")

                Column {
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: root.printers

                        delegate: AbstractButton {
                            id: printerRow

                            required property var modelData
                            readonly property bool selected: root.printerId === modelData.id
                            readonly property string title: modelData.name !== "" ? modelData.name : qsTr("My printer")

                            width: parent.width
                            height: 34
                            hoverEnabled: true
                            Accessible.name: title + ", " + qsTr("%1 × %2 mm").arg(modelData.bedX).arg(modelData.bedY)
                            Accessible.checkable: true
                            Accessible.checked: selected
                            onClicked: root.printerChosen(modelData.id)

                            background: Rectangle {
                                radius: 8
                                color: printerRow.selected ? Theme.accentSoft : (printerRow.hovered ? Theme.field : "transparent")
                            }

                            contentItem: Item {
                                Txt {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: printerRow.title
                                    color: printerRow.selected ? Theme.accentText : Theme.fg
                                }

                                Txt {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("%1 × %2").arg(printerRow.modelData.bedX).arg(printerRow.modelData.bedY)
                                    color: Theme.fg4
                                    mono: true
                                    font.pixelSize: Theme.fontSizeSmall
                                }
                            }
                        }
                    }
                }
            }

            Section {
                visible: root.tab === "printer"
                heading: qsTr("Filaments")
                aside: qsTr("%n colour(s)", "", 4)

                Row {
                    spacing: 8

                    Repeater {
                        model: [
                            { name: qsTr("White"), color: "#ECE8E1" },
                            { name: qsTr("Red"), color: "#D93F36" },
                            { name: qsTr("Blue"), color: "#3F83D6" },
                            { name: qsTr("Yellow"), color: "#F2B93A" }
                        ]

                        delegate: Rectangle {
                            required property var modelData

                            width: 40
                            height: 40
                            radius: 8
                            color: modelData.color
                            border.width: 1
                            border.color: Theme.edge
                            Accessible.name: modelData.name + " (PLA)"
                            Accessible.role: Accessible.StaticText
                        }
                    }
                }
            }
        }
    }
}
