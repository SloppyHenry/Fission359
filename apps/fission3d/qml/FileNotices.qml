// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// The cards for problems with files and for exporting, and for work that was rescued after a
// crash. Problems are a card with the way on, never a dialog (rule 5); the words and buttons
// come from the message catalogue (`MessageCatalog.fileNotice`, `exportNotice`).
Item {
    id: root

    /// The message catalogue.
    property var catalog
    /// The item the cards hang under (the tabs).
    property Item below
    // Where the cards start: a little under the tabs (this item fills the window, so its
    // coordinates are those of the window).
    readonly property real cardTop: below ? below.y + below.height + 10 : 10
    /// A problem that is only a line of text.
    signal hintRequested(string text)
    /// "Save as…" was chosen on a card: the window opens its dialog.
    signal saveAsRequested()

    function showNotice(notice, path) {
        if (notice.card) {
            fileCard.show(notice.text, notice.buttons, path)
        } else {
            root.hintRequested(notice.text)
        }
    }

    Connections {
        target: Files

        function onRescuedWork(id, savedAt, originalPath) {
            const when = new Date(savedAt)
            const today = when.toDateString() === new Date().toDateString()
            let text = qsTr("We saved your work from %1.")
                .arg(today ? Qt.formatTime(when, "HH:mm") : Qt.formatDateTime(when, Locale.ShortFormat))
            if (originalPath !== "") {
                text += " " + qsTr("It belongs to “%1”.").arg(Files.baseName(originalPath))
            }
            rescueCard.show(text, [{ id: "restore", text: qsTr("Restore"), primary: true },
                                   { id: "discard", text: qsTr("Discard") }], id)
        }

        function onFileProblem(code, path) {
            root.showNotice(root.catalog.fileNotice(code), path)
        }
    }

    Connections {
        target: Exporter

        function onFinished(code, path, bodies) {
            if (code !== "") {
                root.showNotice(root.catalog.exportNotice(code))
            }
        }
    }

    NoticeCard {
        id: fileCard

        y: root.cardTop
        anchors.horizontalCenter: parent.horizontalCenter
        z: 0
        onChosen: (id, payload) => {
            if (id === "openanyway") {
                Files.openPath(payload, true)
            } else if (id === "saveas") {
                root.saveAsRequested()
            }
        }
    }

    NoticeCard {
        id: rescueCard

        y: root.cardTop
        anchors.horizontalCenter: parent.horizontalCenter
        z: 1
        onChosen: (id, payload) => {
            if (id === "restore") {
                Files.restoreRescued(payload)
            } else {
                Files.discardRescued(payload)
            }
        }
    }
}
