// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The drawing tools of the group "Sketch", next to its button in the palette (AP-2.1c1): Line,
// Rectangle, Circle, Point and the switch Helper line. A tool with several ways to draw it has a
// small arrow that opens a menu of them; the first way is the standard.
Float {
    id: root

    property var catalog: null
    property string tool: ""      // the tool that is on
    property int variant: 0       // its way
    property bool helper: false   // the switch "Helper line"
    signal toolChosen(string id, int variant)  // variant -1: the way used last
    signal helperToggled

    // The tool whose menu of ways is open.
    property string menuFor: ""
    property real menuY: 0

    width: 176
    height: column.implicitHeight + 16
    z: 5

    Column {
        id: column

        x: 8
        y: 8
        width: parent.width - 16
        spacing: 2

        Repeater {
            model: root.catalog ? root.catalog.tools.filter(tool => tool.listed !== false) : []

            delegate: AbstractButton {
                id: row

                required property var modelData
                readonly property bool isSwitch: modelData.id === "helper"
                readonly property bool on: isSwitch ? root.helper : root.tool === modelData.id

                objectName: "sketchTool_" + modelData.id
                width: column.width
                height: 34
                hoverEnabled: true
                text: modelData.name
                Accessible.name: modelData.name
                Accessible.checkable: true
                Accessible.checked: on
                onClicked: {
                    root.menuFor = ""
                    if (isSwitch) {
                        root.helperToggled()
                    } else {
                        root.toolChosen(modelData.id, -1)
                    }
                }

                TooltipCard {
                    visible: row.hovered && !row.pressed
                    x: row.width + 22
                    y: 0
                    title: row.modelData.name
                    sentence: row.modelData.tip
                    shortcut: row.modelData.key
                }

                background: Rectangle {
                    radius: 7
                    color: row.on ? Theme.accent : row.hovered ? Theme.field : "transparent"

                    Focusring {
                        visible: row.visualFocus
                    }
                }

                contentItem: Row {
                    spacing: 8
                    leftPadding: 8

                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: row.modelData.icon
                        size: 18
                        color: nameText.color
                    }

                    Txt {
                        id: nameText

                        anchors.verticalCenter: parent.verticalCenter
                        width: 78
                        text: row.modelData.name
                        color: row.on ? "#FFFFFF" : Theme.fg2
                        elide: Text.ElideRight
                    }

                    Txt {
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.key
                        mono: true
                        color: row.on ? "#FFFFFF" : Theme.fg4
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }

                // The arrow at the right: the ways to draw this tool.
                AbstractButton {
                    id: ways

                    visible: row.modelData.variants.length > 0
                    objectName: "sketchWays_" + row.modelData.id
                    x: row.width - width - 4
                    y: Math.round((row.height - height) / 2)
                    width: 24
                    height: 24
                    hoverEnabled: true
                    Accessible.name: qsTr("Ways to draw %1").arg(row.modelData.name)
                    onClicked: {
                        root.menuY = row.y + 8 + row.height
                        root.menuFor = root.menuFor === row.modelData.id ? "" : row.modelData.id
                    }

                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "chev"
                            size: 14
                            color: row.on ? "#FFFFFF" : Theme.fg3
                        }
                    }
                    background: Rectangle {
                        radius: 6
                        color: ways.hovered ? Theme.line : "transparent"
                    }
                }
            }
        }
    }

    // The menu of ways, under the row of its tool.
    Float {
        id: waysMenu

        readonly property var entry: root.catalog && root.menuFor !== "" ? root.catalog.find(root.menuFor) : null

        visible: entry !== null
        x: 24
        y: root.menuY
        width: 168
        height: waysColumn.implicitHeight + 12
        z: 6

        Column {
            id: waysColumn

            x: 6
            y: 6
            width: parent.width - 12

            Repeater {
                model: waysMenu.entry ? waysMenu.entry.variants : []

                delegate: AbstractButton {
                    id: way

                    required property int index
                    required property string modelData
                    readonly property bool current: root.tool === root.menuFor && root.variant === index

                    objectName: "sketchWay_" + root.menuFor + "_" + index
                    width: waysColumn.width
                    height: 30
                    hoverEnabled: true
                    text: modelData
                    Accessible.name: modelData
                    onClicked: {
                        const id = root.menuFor
                        root.menuFor = ""
                        root.toolChosen(id, index)
                    }

                    background: Rectangle {
                        radius: 6
                        color: way.current ? Theme.accentSoft : way.hovered ? Theme.field : "transparent"
                    }
                    contentItem: Txt {
                        leftPadding: 8
                        verticalAlignment: Text.AlignVCenter
                        text: way.modelData
                        color: way.current ? Theme.accentText : Theme.fg2
                    }
                }
            }
        }
    }
}
