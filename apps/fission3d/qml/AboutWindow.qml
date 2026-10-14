// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui

// "About Fission3D": version and what the program runs on (for bug reports).
Window {
    id: root

    width: 420
    height: 380
    minimumWidth: 420
    maximumWidth: 420
    color: Theme.panel
    title: qsTr("About Fission3D")
    flags: Qt.Dialog

    // The window is as high as its content (the texts differ by language), and not resizable.
    onVisibleChanged: if (visible) {
        const h = column.implicitHeight + 56
        root.minimumHeight = h
        root.maximumHeight = h
        root.height = h
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.close()
    }

    Column {
        id: column

        x: 28
        y: 28
        width: parent.width - 56
        spacing: 14

        Row {
            spacing: 14

            Logo {
                size: 48
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                Txt {
                    text: "Fission3D"
                    color: Theme.fg
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    elide: Text.ElideNone
                }

                Txt {
                    text: qsTr("Version %1").arg(AppInfo.version)
                    color: Theme.fg3
                    elide: Text.ElideNone
                }
            }
        }

        Txt {
            width: parent.width
            text: qsTr("Easy to use and precise: a 3D construction program for makers.")
            color: Theme.fg2
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Theme.line
        }

        Column {
            width: parent.width
            spacing: 6

            InfoRow {
                label: qsTr("Qt")
                value: AppInfo.qtVersion
                mono: true
            }

            InfoRow {
                label: qsTr("OpenCASCADE")
                value: AppInfo.kernelVersion
                mono: true
            }

            InfoRow {
                label: qsTr("Graphics")
                value: AppInfo.graphicsBackend
                mono: true
            }

            InfoRow {
                label: qsTr("Start time")
                value: AppInfo.startupMilliseconds >= 0 ? qsTr("%1 ms").arg(AppInfo.startupMilliseconds) : "–"
                mono: true
            }

            InfoRow {
                label: qsTr("License")
                value: AppInfo.license
                mono: true
            }
        }

        Txt {
            width: parent.width
            text: qsTr("Log file: %1").arg(AppInfo.logFile)
            color: Theme.fg4
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WrapAnywhere
            elide: Text.ElideNone
        }

        Row {
            anchors.right: parent.right

            Btn {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
