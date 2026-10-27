// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// The document bar (52 px): start page, open documents, undo/redo and the export button.
Rectangle {
    id: root

    property var tabs: []
    property string undoTip: ""
    property string redoTip: ""
    property bool canUndo: false
    property bool canRedo: false
    signal homeRequested
    signal newDocumentRequested
    signal closeRequested(int index)
    signal tabClicked(int index)
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

        // The open documents
        Repeater {
            model: root.tabs

            delegate: Rectangle {
                id: tab

                required property var modelData
                required property int index

                objectName: "tab_" + index
                anchors.verticalCenter: parent.verticalCenter
                height: 36
                width: tabRow.implicitWidth + 22
                radius: 7
                color: modelData.current ? Theme.panel : (tabArea.containsMouse ? Theme.field : "transparent")
                border.width: modelData.current ? 1 : 0
                border.color: Theme.line
                Accessible.role: Accessible.PageTab
                Accessible.name: tabText.text

                MouseArea {
                    id: tabArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.tabClicked(tab.index)
                }

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
                        id: tabText

                        anchors.verticalCenter: parent.verticalCenter
                        text: (tab.modelData.name !== "" ? tab.modelData.name : qsTr("Untitled"))
                              + (tab.modelData.modified ? " •" : "")
                        color: tab.modelData.current ? Theme.fg : Theme.fg3
                        font.weight: Font.Medium
                        elide: Text.ElideNone
                    }

                    AbstractButton {
                        id: closeButton

                        objectName: "tabClose_" + tab.index
                        anchors.verticalCenter: parent.verticalCenter
                        implicitWidth: 16
                        implicitHeight: 16
                        hoverEnabled: true
                        Accessible.name: qsTr("Close")
                        onClicked: root.closeRequested(tab.index)

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
        }

        IconBtn {
            anchors.verticalCenter: parent.verticalCenter
            objectName: "tabNew"
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
