// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// One step of the recipe as a sentence: its position, a symbol for what it is, its name (if the
// person gave it one) and the sentence, and a badge when it is hidden, needs help or fails.
// Never colour alone: the badge has a symbol and a tooltip with words (docs/02-ux-konzept.md §14).
Rectangle {
    id: root

    property int position: 0           // 0-based place in the recipe
    property string iconName: "cube"
    property string sentence: ""
    property string label: ""          // the name the person gave the step, may be empty
    property string state: "ok"        // ok | warning | fails | off | pending
    property string problem: ""        // what went wrong, in words
    property bool selected: false
    property bool beyondMarker: false  // after the time marker: not calculated
    readonly property bool editing: nameInput.visible
    readonly property bool hidden: state === "off"
    signal clicked
    signal hovered(bool on)
    signal renamed(string text)
    signal menuRequested(real x, real y)
    signal deleteRequested

    function startRename() {
        nameInput.text = root.label
        nameInput.visible = true
        nameInput.forceActiveFocus()
        nameInput.selectAll()
    }

    readonly property string stateWord: state === "warning" ? qsTr("Needs help")
                                      : state === "fails" ? qsTr("Does not work")
                                      : state === "off" ? qsTr("Hidden") : ""
    property bool explained: false
    /// The buttons under the explanation of a problem: [{ id: "editValue" | "hideStep", label }].
    property var problemActions: []
    signal editRequested
    signal hideRequested

    width: parent ? parent.width : 260
    height: column.implicitHeight
    radius: 6
    color: selected ? Theme.accentSoft : state === "fails" ? Theme.badSoft
         : hover.hovered ? Theme.field : "transparent"
    opacity: hidden || beyondMarker ? 0.55 : 1.0
    activeFocusOnTab: true
    Accessible.name: sentence
    Accessible.role: Accessible.ListItem

    Keys.onPressed: event => {
        if (event.key === Qt.Key_F2) {
            root.startRename()
            event.accepted = true
        } else if (event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
            root.deleteRequested()
            event.accepted = true
        }
    }

    HoverHandler {
        id: hover

        onHoveredChanged: root.hovered(hovered)
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: {
            // The second click of a double click must not take the focus away from the name
            // that is being typed.
            if (!root.editing) {
                root.forceActiveFocus()
            }
            root.clicked()
        }
        onDoubleTapped: Qt.callLater(root.startRename)
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: point => {
            root.forceActiveFocus()
            root.menuRequested(point.position.x, point.position.y)
        }
    }

    Column {
        id: column

        width: parent.width

        Item {
            width: parent.width
            height: Math.max(30, texts.implicitHeight + 12)

            Txt {
                id: number

                x: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                text: String.fromCharCode(0x2460 + root.position)
                color: Theme.fg4
                mono: true
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }

            Icon {
                id: symbol

                anchors.left: number.right
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                name: root.iconName
                size: 14
                color: Theme.fg3
            }

            Column {
                id: texts

                anchors.left: symbol.right
                anchors.leftMargin: 8
                anchors.right: badge.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter

                TextInput {
                    id: nameInput

                    visible: false
                    width: parent.width
                    color: Theme.fg
                    font.family: Theme.fontFamilies[0]
                    font.pixelSize: Theme.fontSize
                    font.weight: Font.DemiBold
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.accentFg
                    clip: true
                    Accessible.name: qsTr("Name of the step")
                    onAccepted: {
                        visible = false
                        root.renamed(text)
                    }
                    Keys.onEscapePressed: visible = false
                    onActiveFocusChanged: if (!activeFocus && visible) { visible = false }
                }

                Txt {
                    visible: root.label !== "" && !nameInput.visible
                    width: parent.width
                    text: root.label
                    color: Theme.fg
                    font.weight: Font.DemiBold
                }

                Txt {
                    width: parent.width
                    text: root.sentence
                    color: root.label !== "" ? Theme.fg3 : Theme.fg
                    font.pixelSize: root.label !== "" ? Theme.fontSizeSmall : Theme.fontSize
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
            }

            // ok: a tick; hidden / needs help / fails: a symbol or a word, with a tooltip.
            AbstractButton {
                id: badge

                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: root.state === "off" ? badgeWord.implicitWidth + 4 : 22
                height: 22
                hoverEnabled: true
                enabled: root.state === "warning" || root.state === "fails"
                Accessible.name: root.stateWord
                onClicked: root.explained = !root.explained

                ToolTip.visible: hovered && root.stateWord !== ""
                ToolTip.delay: 400
                ToolTip.text: root.stateWord

                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        visible: root.state === "ok"
                        name: "ok"
                        size: 13
                        strokeWidth: 2.4
                        color: Theme.ok
                    }

                    Icon {
                        anchors.centerIn: parent
                        visible: root.state === "warning" || root.state === "fails"
                        name: root.state === "fails" ? "x" : "warn"
                        size: 13
                        strokeWidth: 2.4
                        color: root.state === "fails" ? Theme.bad : Theme.amberFg
                    }

                    Txt {
                        id: badgeWord

                        anchors.centerIn: parent
                        visible: root.state === "off"
                        text: qsTr("(hidden)")
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.fg3
                        elide: Text.ElideNone
                    }
                }

                background: Rectangle {
                    radius: 5
                    color: badge.hovered && badge.enabled ? Theme.field2 : "transparent"
                }
            }
        }

        // What went wrong, right under the step (docs/02-ux-konzept.md §14).
        Txt {
            visible: root.explained && root.problem !== ""
            width: parent.width
            leftPadding: 34
            rightPadding: 10
            bottomPadding: 8
            text: root.problem
            color: Theme.fg2
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        // The way out in one click (rule 5).
        Row {
            visible: root.explained && root.problem !== "" && root.problemActions.length > 0
            leftPadding: 34
            bottomPadding: 8
            spacing: 6

            Repeater {
                model: root.problemActions

                Btn {
                    required property var modelData

                    objectName: "problemAction_" + modelData.id
                    small: true
                    text: modelData.label
                    onClicked: modelData.id === "editValue" ? root.editRequested() : root.hideRequested()
                }
            }
        }
    }
}
