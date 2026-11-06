// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission359.Ui

// The tool "Measure" (AP-2.11): what the one or two chosen elements measure, in plain rows, with
// "Copy" for the main number. The line itself and its number are in the 3D view. Nothing here
// changes the model.
Float {
    id: root

    signal closeRequested

    readonly property var found: Measure.result
    readonly property bool hasResult: found.count !== undefined

    /// The rows to show: [{ label, text }], and the main number as text ("12,50 mm").
    readonly property var rows: {
        const r = found
        const list = []
        if (r.count === undefined) {
            return list
        }
        const mm = value => Numbers.format(value, 2) + " mm"
        if (r.x !== undefined) {
            list.push({ label: qsTr("Position"), text: "X " + mm(r.x) + "   Y " + mm(r.y) + "   Z " + mm(r.z) })
        }
        if (r.distance !== undefined) {
            list.push({ label: qsTr("Distance"), text: mm(r.distance) })
        }
        if (r.angle !== undefined) {
            list.push({ label: qsTr("Angle"), text: Numbers.format(r.angle, 2) + " °" })
        }
        if (r.length !== undefined) {
            list.push({ label: qsTr("Length"), text: mm(r.length) })
        }
        if (r.area !== undefined) {
            list.push({ label: qsTr("Area"), text: Numbers.format(r.area, 2) + " mm²" })
        }
        if (r.radius !== undefined) {
            list.push({ label: qsTr("Radius"), text: mm(r.radius) })
            list.push({ label: qsTr("Diameter"), text: mm(r.diameter) })
        }
        return list
    }
    readonly property string mainNumber: rows.length > 0
        ? (found.distance !== undefined && (found.distance > 1e-9 || found.angle === undefined) ? rows[found.x !== undefined ? 1 : 0].text
           : found.angle !== undefined ? Numbers.format(found.angle, 2) + " °"
           : rows[0].text.replace(/^\S+ /, ""))
        : ""
    readonly property string hintText: Measure.result.count === undefined
        ? qsTr("Click a face, an edge or a corner. A second one (Shift+click) gives the distance and the angle between them.")
        : qsTr("Shift+click adds the next one. Esc ends measuring.")

    width: 300
    height: content.implicitHeight + 32
    Accessible.name: qsTr("Measure")

    Column {
        id: content

        x: 16
        y: 16
        width: parent.width - 32
        spacing: 10

        Txt {
            text: qsTr("Measure")
            color: Theme.fg
            font.weight: Font.DemiBold
        }

        Txt {
            width: parent.width
            text: root.hintText
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Repeater {
            model: root.rows

            delegate: InfoRow {
                required property var modelData

                label: modelData.label
                value: modelData.text
            }
        }

        Row {
            spacing: 8

            Btn {
                text: qsTr("Copy")
                variant: "primary"
                enabled: root.mainNumber !== ""
                onClicked: Measure.copyToClipboard(root.mainNumber)
            }

            Btn {
                text: qsTr("Done")
                onClicked: root.closeRequested()
            }
        }
    }
}
