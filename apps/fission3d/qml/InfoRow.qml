// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// A label and its value, in two columns (96 px for the label).
Item {
    id: root

    property string label
    property string value
    property bool mono: false

    width: parent ? parent.width : 200
    height: 24

    Txt {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 96
        text: root.label
        color: Theme.fg4
        font.pixelSize: Theme.fontSizeSmallPlus
    }

    Txt {
        anchors.left: parent.left
        anchors.leftMargin: 106
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: root.value
        mono: root.mono
        color: Theme.fg2
    }
}
