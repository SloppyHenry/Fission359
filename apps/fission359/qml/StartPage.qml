// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The start page (version 1): a new project, the files opened last, and examples. AP-5.4 grows
// it into "What do you want to build?"; it is its own file so that this can happen here.
Rectangle {
    id: root

    property var recent: []
    property var examples: []

    signal newRequested
    signal openRequested(string path)
    signal exampleRequested(string name)
    signal closeRequested

    color: Theme.chrome
    Accessible.name: qsTr("Start")

    function exampleTitle(name) {
        switch (name) {
        case "box-with-hole": return qsTr("Box with a hole")
        case "keychain": return qsTr("Keychain")
        case "star": return qsTr("Star")
        default: return name
        }
    }

    component Tile: Rectangle {
        id: tile

        property string title
        property string picture
        property string symbol
        signal clicked

        width: 190
        height: 170
        radius: Theme.radiusCard
        color: area.containsMouse ? Theme.field : Theme.panel
        border.width: 1
        border.color: Theme.line
        Accessible.role: Accessible.Button
        Accessible.name: title

        Rectangle {
            x: 10
            y: 10
            width: parent.width - 20
            height: 108
            radius: Theme.radius
            color: Theme.chrome

            Image {
                anchors.fill: parent
                visible: tile.picture !== ""
                source: tile.picture
                fillMode: Image.PreserveAspectCrop
            }

            Icon {
                anchors.centerIn: parent
                visible: tile.picture === ""
                name: tile.symbol !== "" ? tile.symbol : "cube"
                size: 36
                color: Theme.fg3
            }
        }

        Txt {
            x: 12
            y: 128
            width: parent.width - 24
            text: tile.title
            color: Theme.fg
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: area

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tile.clicked()
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight + 64
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Column {
            id: column

            x: 40
            y: 32
            width: parent.width - 80
            spacing: 18

            Item {
                width: parent.width
                height: 40

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Start")
                    color: Theme.fg
                    font.pixelSize: Theme.fontSizeLarge + 6
                    font.weight: Font.DemiBold
                }

                Btn {
                    objectName: "startClose"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Back to the project")
                    onClicked: root.closeRequested()
                }
            }

            Flow {
                width: parent.width
                spacing: 14

                Tile {
                    objectName: "startNew"
                    title: qsTr("New project")
                    symbol: "plus"
                    onClicked: root.newRequested()
                }
            }

            Txt {
                visible: root.recent.length > 0
                text: qsTr("Opened recently")
                color: Theme.fg
                font.weight: Font.DemiBold
            }

            Flow {
                visible: root.recent.length > 0
                width: parent.width
                spacing: 14

                Repeater {
                    model: root.recent

                    Tile {
                        required property var modelData
                        required property int index

                        objectName: "startRecent_" + index
                        title: modelData.name
                        picture: modelData.preview
                        onClicked: root.openRequested(modelData.path)
                    }
                }
            }

            Txt {
                visible: root.examples.length > 0
                text: qsTr("Examples")
                color: Theme.fg
                font.weight: Font.DemiBold
            }

            Flow {
                visible: root.examples.length > 0
                width: parent.width
                spacing: 14

                Repeater {
                    model: root.examples

                    Tile {
                        required property string modelData

                        objectName: "startExample_" + modelData
                        title: root.exampleTitle(modelData)
                        onClicked: root.exampleRequested(modelData)
                    }
                }
            }
        }
    }
}
