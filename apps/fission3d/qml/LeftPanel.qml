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
    signal tabRequested(string tab)
    signal addRequested
    signal bodyVisibilityRequested(int index, bool visible)
    signal bodyChosen(int index, bool extend)
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
                    model: root.bodies

                    delegate: TreeRow {
                        required property var modelData
                        required property int index

                        // A body that is part of a result is not a body of its own (hidden, its
                        // position in the list stays the position in the document).
                        visible: !modelData.consumed
                        child: true
                        cut: modelData.cut === true
                        selected: modelData.chosen
                        swatch: modelData.color
                        hole: modelData.hole === true
                        text: modelData.name
                        eyeOn: modelData.visible
                        eyeLabel: qsTr("Visibility: %1").arg(modelData.name)
                        onEyeToggled: root.bodyVisibilityRequested(index, !modelData.visible)
                        onClicked: extend => root.bodyChosen(index, extend)
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
        height: Math.min(parent.height * 0.46, recipeColumn.implicitHeight)

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.line
        }

        Column {
            id: recipeColumn

            width: parent.width

            Item {
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

            Column {
                x: 8
                width: parent.width - 16

                Repeater {
                    model: root.recipe

                    delegate: Rectangle {
                        required property var modelData
                        required property int index

                        width: parent.width
                        height: stepText.implicitHeight + 12
                        radius: 6
                        color: index === root.currentStep ? Theme.accentSoft
                             : modelData.state === "fails" ? Theme.badSoft : "transparent"
                        opacity: modelData.state === "off" ? 0.55 : 1.0

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.stepRequested(index)
                        }

                        Row {
                            anchors.fill: parent
                            anchors.margins: 6
                            anchors.leftMargin: 8
                            spacing: 8

                            Txt {
                                width: 18
                                text: String.fromCharCode(0x2460 + index)
                                color: Theme.fg4
                                mono: true
                                font.pixelSize: Theme.fontSizeSmall
                                elide: Text.ElideNone
                            }

                            Txt {
                                id: stepText

                                width: parent.width - 18 - 8 - 22
                                text: modelData.text
                                color: Theme.fg
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                            }

                            Icon {
                                visible: modelData.state === "ok"
                                name: "ok"
                                size: 13
                                strokeWidth: 2.4
                                color: Theme.ok
                            }

                            Icon {
                                visible: modelData.state === "warning" || modelData.state === "fails"
                                name: modelData.state === "fails" ? "x" : "warn"
                                size: 13
                                strokeWidth: 2.4
                                color: modelData.state === "fails" ? Theme.bad : Theme.fg3
                            }
                        }
                    }
                }
            }

            // the time marker: everything above is "now"
            Row {
                x: 8
                width: parent.width - 16
                height: 28
                spacing: 8

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: (parent.width - nowText.width - 16) / 2
                    height: 2
                    radius: 1
                    color: Theme.accentLine
                }

                Txt {
                    id: nowText

                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("now")
                    color: Theme.accentText
                    font.pixelSize: Theme.fontSizeSmall
                    font.weight: Font.DemiBold
                    elide: Text.ElideNone
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: (parent.width - nowText.width - 16) / 2
                    height: 2
                    radius: 1
                    color: Theme.accentLine
                }
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
