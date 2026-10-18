// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The document bar (52 px): start page, open documents, undo/redo and the export button.
Rectangle {
    id: root

    property string documentName: ""
    property string undoTip: ""
    property string redoTip: ""
    property bool canUndo: false
    property bool canRedo: false
    signal homeRequested
    signal newDocumentRequested
    signal closeRequested
    signal undoRequested
    signal redoRequested
    signal exportRequested

    height: Theme.tabsHeight
    color: Theme.bar

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.line
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        IconBtn {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "home"
            tip: qsTr("Start page")
            onClicked: root.homeRequested()
        }

        // The open document
        Rectangle {
            id: tab

            anchors.verticalCenter: parent.verticalCenter
            height: 36
            width: tabRow.implicitWidth + 22
            radius: 7
            color: Theme.panel
            border.width: 1
            border.color: Theme.line

            Row {
                id: tabRow

                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "cube"
                    color: Theme.fg3
                }

                Txt {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.documentName
                    color: Theme.fg
                    font.weight: Font.Medium
                    elide: Text.ElideNone
                }

                AbstractButton {
                    id: closeButton

                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 16
                    implicitHeight: 16
                    hoverEnabled: true
                    Accessible.name: qsTr("Close")
                    onClicked: root.closeRequested()

                    // Centred in a holder: the control stretches its content item, and the
                    // icon draws from its top-left corner.
                    contentItem: Item {
                        Icon {
                            anchors.centerIn: parent
                            name: "close"
                            size: 12
                            color: closeButton.hovered ? Theme.fg : Theme.fg4
                        }
                    }

                    background: Rectangle {
                        radius: 4
                        color: closeButton.hovered ? Theme.field : "transparent"
                    }
                }
            }
        }

        IconBtn {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "plus"
            tip: qsTr("New project")
            onClicked: root.newDocumentRequested()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        IconBtn {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "undo"
            enabled: root.canUndo
            tip: root.undoTip !== ""
                 ? (Qt.platform.os === "osx" ? qsTr("%1 (⌘Z)") : qsTr("%1 (Ctrl+Z)")).arg(root.undoTip)
                 : (Qt.platform.os === "osx" ? qsTr("Undo (⌘Z)") : qsTr("Undo (Ctrl+Z)"))
            onClicked: root.undoRequested()
        }

        IconBtn {
            anchors.verticalCenter: parent.verticalCenter
            iconName: "redo"
            enabled: root.canRedo
            tip: root.redoTip !== ""
                 ? (Qt.platform.os === "osx" ? qsTr("%1 (⇧⌘Z)") : qsTr("%1 (Ctrl+Shift+Z)")).arg(root.redoTip)
                 : (Qt.platform.os === "osx" ? qsTr("Redo (⇧⌘Z)") : qsTr("Redo (Ctrl+Shift+Z)"))
            onClicked: root.redoRequested()
        }

        Btn {
            anchors.verticalCenter: parent.verticalCenter
            variant: "primary"
            iconName: "upload"
            text: qsTr("Export")
            onClicked: root.exportRequested()
        }
    }
}
