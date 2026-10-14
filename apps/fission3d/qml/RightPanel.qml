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
    signal valueCommitted(string name, string text)
    readonly property var sizeValues: stepValues.filter(v => v.group === "size")
    readonly property var positionValues: stepValues.filter(v => v.group === "position")
    property string tab: "object"      // object | printer
    property string printerName: ""
    property string bedText: ""
    property string nozzleText: ""
    signal tabRequested(string tab)

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
                        unit: modelData.kind === "angle" ? "°" : "mm"
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
                            axis: modelData.name
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
                subtitle: qsTr("Printer profile · chosen at first start")
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
