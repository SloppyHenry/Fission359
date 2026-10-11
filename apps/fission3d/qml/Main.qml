// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Window {
    id: root

    width: 1280
    height: 800
    visible: true
    color: "#2b2d31"
    title: qsTr("Fission3D")

    Text {
        anchors.centerIn: parent
        text: root.title
        color: "#d8dae0"
        font.pixelSize: 32
    }
}
