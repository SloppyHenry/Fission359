// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The status line (28 px) with the hint line: says in one sentence what can be done now.
Rectangle {
    id: root

    property string selection: ""
    property string toolName: ""
    property string hint: ""
    property string unitText: ""
    /// The step of snapping in use (mm), and whether it follows the zoom.
    property real gridStep: 1
    property bool gridAutomatic: true
    /// The button "Continue" (mirror and pattern: from choosing to the second step, AP-2.1e3).
    property bool continueVisible: false
    signal continueClicked()
    /// The button "Let go of the link" (a sketch element that follows an edge was to be changed).
    property bool releaseVisible: false
    signal releaseClicked()
    signal gridStepChosen(real step)
    signal gridAutomaticChosen()

    height: Theme.statusHeight
    color: Theme.chrome

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.line
    }

    Row {
        id: leftRow

        anchors.left: parent.left
        anchors.leftMargin: Theme.space3
        anchors.right: rightRow.left
        anchors.rightMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space4
        clip: true

        Txt {
            visible: root.selection !== ""
            text: root.selection
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }

        Txt {
            text: qsTr("Tool: %1").arg(root.toolName)
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }

        Txt {
            // The hint is the long one: it gives way with "…" before the clip cuts it.
            width: Math.min(implicitWidth, Math.max(0, leftRow.width - x))
            text: root.hint
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideRight
            Accessible.role: Accessible.StaticText
            Accessible.name: root.hint
        }
    }

    Row {
        id: rightRow

        anchors.right: parent.right
        anchors.rightMargin: Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space4

        Btn {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.releaseVisible
            small: true
            variant: "primary"
            text: qsTr("Let go of the link")
            onClicked: root.releaseClicked()
        }

        Btn {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.continueVisible
            small: true
            variant: "primary"
            text: qsTr("Continue")
            onClicked: root.continueClicked()
        }

        Txt {
            anchors.verticalCenter: parent.verticalCenter
            text: root.unitText
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            elide: Text.ElideNone
        }

        // The precision of snapping.
        SnapControl {
            id: snapControl

            anchors.verticalCenter: parent.verticalCenter
            step: root.gridStep
            automatic: root.gridAutomatic
            onStepChosen: value => root.gridStepChosen(value)
            onAutomaticChosen: root.gridAutomaticChosen()
        }
    }
}
