// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui
import Fission359.View

// The label on a chosen hole: "Cuts out: Box 1". A click opens the list of the bodies it
// touches; ticking one on or off lets the hole cut it or not (rule 13, UX 7.1).
Item {
    id: root

    property Viewport3D viewport
    /// What `DocumentModel.cutLabel` says about the chosen hole (`valid: false` for none).
    property var info: ({ valid: false })
    /// The name of a body by its index in the document.
    property var nameOf: index => ""
    signal targetToggled(int bodyIndex, bool cut)

    readonly property var targets: info.valid ? info.targets : []
    readonly property var cutNames: targets.filter(t => t.cut).map(t => nameOf(t.index))
    property bool open: false

    // Follows the camera: read its values so that the binding is evaluated again.
    readonly property point at: {
        if (!viewport || !info.valid) {
            return Qt.point(0, 0)
        }
        const follow = viewport.cameraYaw + viewport.cameraPitch + viewport.pixelsPerMillimetre
        return viewport.project(info.x, info.y, info.z)
    }

    visible: info.valid
    onInfoChanged: if (!info.valid) open = false

    Rectangle {
        id: chip

        x: Math.max(4, Math.min(root.width - width - 4, root.at.x - width / 2))
        y: Math.max(4, root.at.y - height - 14)
        width: chipRow.implicitWidth + 16
        height: 24
        radius: 12
        color: Theme.panel
        border.width: 1
        border.color: root.open ? Theme.accentLine : (hover.hovered ? Theme.fg3 : Theme.line2)

        Row {
            id: chipRow

            anchors.centerIn: parent
            spacing: 5

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "hole"
                size: 13
                color: Theme.fg3
            }

            Txt {
                anchors.verticalCenter: parent.verticalCenter
                text: root.cutNames.length > 0
                      ? qsTr("Cuts out: %1").arg(root.cutNames.join(", "))
                      : qsTr("Cuts out nothing")
                color: Theme.fg
                font.pixelSize: Theme.fontSizeSmall
                elide: Text.ElideNone
            }
        }

        HoverHandler {
            id: hover
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: root.open = !root.open
        }

        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Choose what this hole cuts out")
    }

    Rectangle {
        visible: root.open
        x: chip.x
        y: chip.y - height - 4
        width: Math.max(chip.width, 180)
        height: list.implicitHeight + 8
        radius: Theme.radius
        color: Theme.panel
        border.width: 1
        border.color: Theme.line

        Column {
            id: list

            anchors.fill: parent
            anchors.margins: 4

            Repeater {
                model: root.targets

                delegate: Rectangle {
                    required property var modelData

                    width: list.width
                    height: 26
                    radius: Theme.radius
                    color: rowHover.hovered ? Theme.field : "transparent"

                    HoverHandler {
                        id: rowHover
                    }

                    TapHandler {
                        acceptedButtons: Qt.LeftButton
                        onTapped: root.targetToggled(modelData.index, !modelData.cut)
                    }

                    Rectangle {
                        id: box

                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                        height: 14
                        radius: 3
                        color: modelData.cut ? Theme.accent : "transparent"
                        border.width: 1
                        border.color: modelData.cut ? Theme.accent : Theme.fg3

                        Icon {
                            visible: modelData.cut
                            anchors.centerIn: parent
                            name: "ok"
                            size: 10
                            strokeWidth: 2.2
                            color: Theme.accentFg
                        }
                    }

                    Txt {
                        anchors.left: box.right
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.nameOf(modelData.index)
                        color: Theme.fg
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Accessible.role: Accessible.CheckBox
                    Accessible.checkable: true
                    Accessible.checked: modelData.cut
                    Accessible.name: root.nameOf(modelData.index)
                }
            }
        }
    }
}
