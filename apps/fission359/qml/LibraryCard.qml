// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// A card of the library list (placeholder entries: building blocks and motifs).
AbstractButton {
    id: root

    implicitHeight: 38
    hoverEnabled: true
    Accessible.name: text

    contentItem: Txt {
        text: root.text
        color: Theme.fg
        verticalAlignment: Text.AlignVCenter
        leftPadding: 10
    }

    background: Rectangle {
        radius: 8
        color: Theme.raise
        border.width: 1
        border.color: root.hovered ? Theme.accentLine : Theme.line

        Focusring {
            visible: root.visualFocus
        }
    }
}
