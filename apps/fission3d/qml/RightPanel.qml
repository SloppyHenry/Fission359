// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// Right side bar: properties of the selection ("Object") and the printer.
Rectangle {
    id: root

    property string stepTitle: ""
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
    signal textCommitted(string name, string text)
    function focusText() {
        wordRepeater.itemAt(0)?.focusAndSelect()
    }
    readonly property var sizeValues: stepValues.filter(v => v.group === "size")
    readonly property var positionValues: stepValues.filter(v => v.group === "position")
    readonly property var rotationValues: stepValues.filter(v => v.group === "rotation")
    property string tab: "object"      // object | printer
    property string printerName: ""
    property string bedText: ""
    property string nozzleText: ""
    property string printerId: ""
    property var printers: []          // [{ id, name, bedX, bedY, bedZ, nozzle }]
    signal tabRequested(string tab)
    signal printerChosen(string id)

    color: Theme.panel
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
                visible: root.tab === "object" && root.stepValues.length === 0
                iconName: "select"
                title: qsTr("Nothing selected")
                subtitle: qsTr("Click a body to change it.")
            }

            PanelHeader {
                visible: root.tab === "object" && root.stepValues.length > 0
                iconName: "cube"
                title: root.stepTitle
                subtitle: qsTr("Change a value or type a formula.")
            }

            Section {
                visible: root.tab === "object" && root.stepTexts.length > 0
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
                visible: root.tab === "object" && root.sizeValues.length > 0
                heading: qsTr("Size")

                Repeater {
                    model: root.sizeValues

                    delegate: NumberField {
                        id: sizeField

                        required property var modelData

                        width: parent.width
                        label: modelData.label
                        kind: modelData.kind
                        unit: modelData.kind === "angle" ? "°" : modelData.kind === "plain" ? "" : "mm"
                        invalid: modelData.error
                        onTextCommitted: text => root.valueCommitted(modelData.name, text)

                        Binding {
                            target: sizeField
                            property: "value"
                            value: sizeField.modelData.value
                        }

                        Binding {
                            target: sizeField
                            property: "expression"
                            value: sizeField.modelData.expression
                        }
                    }
                }
            }

            Section {
                visible: root.tab === "object" && root.positionValues.length > 0
                heading: qsTr("Position")

                Row {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: root.positionValues

                        delegate: NumberField {
                            id: positionField

                            required property var modelData

                            width: (parent.width - 16) / 3
                            axis: modelData.axis
                            kind: modelData.kind
                            unit: ""
                            invalid: modelData.error
                            onTextCommitted: text => root.valueCommitted(modelData.name, text)

                            Binding {
                                target: positionField
                                property: "value"
                                value: positionField.modelData.value
                            }

                            Binding {
                                target: positionField
                                property: "expression"
                                value: positionField.modelData.expression
                            }
                        }
                    }
                }
            }

            Section {
                visible: root.tab === "object" && root.rotationValues.length > 0
                heading: qsTr("Rotation")

                Row {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: root.rotationValues

                        delegate: NumberField {
                            id: rotationField

                            required property var modelData

                            width: (parent.width - 16) / 3
                            axis: modelData.axis
                            kind: modelData.kind
                            unit: "°"
                            invalid: modelData.error
                            onTextCommitted: text => root.valueCommitted(modelData.name, text)

                            Binding {
                                target: rotationField
                                property: "value"
                                value: rotationField.modelData.value
                            }

                            Binding {
                                target: rotationField
                                property: "expression"
                                value: rotationField.modelData.expression
                            }
                        }
                    }
                }
            }

            Section {
                visible: root.tab === "object" && root.stepValues.length === 0

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
