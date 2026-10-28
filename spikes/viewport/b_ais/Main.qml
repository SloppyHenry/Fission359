// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import ViewportB

Window {
    id: root
    width: 1280
    height: 800
    visible: true
    // While measuring, stay in front: macOS pauses drawing of covered windows.
    title: "Fission359 viewport spike B (OpenCASCADE AIS)"
    color: "#131519"

    AisViewport {
        id: viewport
        anchors.fill: parent

        PinchHandler {
            id: pinch
            target: null
            property real lastScale: 1
            onActiveChanged: lastScale = 1
            onActiveScaleChanged: {
                viewport.zoomBy(lastScale / activeScale)
                lastScale = activeScale
            }
        }
    }

    Rectangle {
        anchors { left: parent.left; top: parent.top; margins: 12 }
        width: stats.implicitWidth + 24
        height: stats.implicitHeight + 16
        radius: 8
        color: "#cc1b1e24"
        border.color: "#33ffffff"

        Column {
            id: stats
            anchors { left: parent.left; top: parent.top; margins: 12; topMargin: 8 }
            spacing: 2
            Text { color: "#e8eaed"; font.pixelSize: 14; font.bold: true
                   text: "B: " + viewport.backend }
            Text { color: "#c4c7cc"; font.pixelSize: 13
                   text: viewport.triangles.toLocaleString(Qt.locale("de_DE"), "f", 0) + " Dreiecke, "
                         + viewport.faceCount + " Flächen" }
            Text { color: "#c4c7cc"; font.pixelSize: 13
                   text: viewport.fps.toFixed(0) + " fps"
                         }
            Text { color: "#c4c7cc"; font.pixelSize: 13
                   text: "Auswahl-Latenz " + viewport.pickLatencyMs.toFixed(1) + " ms" }
            Text { color: "#c4c7cc"; font.pixelSize: 13
                   text: "Fläche unter Zeiger: " + (viewport.hoverFace > 0 ? viewport.hoverFace : "–")
                         + "   ausgewählt: " + (viewport.selectedFace > 0 ? viewport.selectedFace : "–") }
        }
    }

    Text {
        anchors { left: parent.left; bottom: parent.bottom; margins: 12 }
        color: "#9aa0a6"; font.pixelSize: 12
        text: "Rechte Taste: drehen · mittlere Taste (oder Shift+Linke): verschieben · Rad: Zoom · "
              + "Trackpad: zwei Finger drehen, Shift verschiebt, Pinch zoomt · Klick: Fläche wählen"
    }
}
