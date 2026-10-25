// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The head of the property panel for a body: icon, its name (click to rename), what kind of shape
// it is and a line saying what is chosen.
Item {
    id: root

    property string iconName: "cube"
    property string name: ""
    property string typeName: ""
    property string subtitle: ""
    property bool renamable: true
    signal renamed(string name)

    width: parent ? parent.width : 300
    height: 64

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.line
    }

    Rectangle {
        id: iconBox

        x: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 36
        radius: 8
        color: Theme.accentSoft

        Icon {
            anchors.centerIn: parent
            name: root.iconName
            color: Theme.accentText
        }
    }

    Column {
        anchors.left: iconBox.right
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter

        Item {
            width: parent.width
            height: nameInput.implicitHeight

            TextInput {
                id: nameInput

                objectName: "bodyName"
                width: Math.min(implicitWidth, parent.width - badge.width - 8)
                clip: true
                readOnly: !root.renamable
                selectByMouse: true
                color: Theme.fg
                selectionColor: Theme.accent
                selectedTextColor: Theme.accentFg
                font.family: Theme.fontFamilies[0]
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
                Accessible.name: qsTr("Name")
                Accessible.description: qsTr("Click to rename the body.")

                onActiveFocusChanged: {
                    if (activeFocus) {
                        selectAll()
                    } else {
                        root.commit()
                    }
                }
                onAccepted: focus = false
                Keys.onEscapePressed: {
                    text = root.name
                    focus = false
                }

                // Follows the document unless a name is being typed.
                Binding on text {
                    when: !nameInput.activeFocus
                    value: root.name
                }

                // A thin line under the name while it is pointed at or typed in: it can be changed.
                Rectangle {
                    anchors.top: parent.bottom
                    width: parent.width
                    height: 1
                    color: nameInput.activeFocus ? Theme.accent : Theme.line
                    visible: root.renamable && (nameInput.activeFocus || nameHover.hovered)
                }

                HoverHandler {
                    id: nameHover

                    cursorShape: root.renamable ? Qt.IBeamCursor : Qt.ArrowCursor
                }
            }

            // What kind of shape it is ("Box"), small beside the name.
            Rectangle {
                id: badge

                anchors.left: nameInput.right
                anchors.leftMargin: 8
                anchors.verticalCenter: nameInput.verticalCenter
                visible: root.typeName !== "" && root.typeName !== nameInput.text
                width: badgeText.implicitWidth + 12
                height: 18
                radius: 9
                color: Theme.field

                Txt {
                    id: badgeText

                    anchors.centerIn: parent
                    text: root.typeName
                    color: Theme.fg3
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }

        Txt {
            width: parent.width
            text: root.subtitle
            color: Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    function commit() {
        const typed = nameInput.text.trim()
        if (typed !== "" && typed !== root.name) {
            root.renamed(typed)
        } else {
            nameInput.text = root.name
        }
    }
}
