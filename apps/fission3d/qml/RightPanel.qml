// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// Right side bar: properties of the selection ("Object") and the printer.
Rectangle {
    id: root

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
                visible: root.tab === "object"
                iconName: "select"
                title: qsTr("Nothing selected")
                subtitle: qsTr("Click a body to change it.")
            }

            Section {
                visible: root.tab === "object"

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
