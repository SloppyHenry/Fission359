// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.View

Window {
    id: root

    width: 1280
    height: 800
    visible: true
    color: "#2b2d31"
    title: qsTr("Fission3D")

    Viewport3D {
        objectName: "viewport"
        anchors.fill: parent
    }
}
