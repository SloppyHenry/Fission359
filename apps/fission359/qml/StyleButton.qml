// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// A small on/off button with one letter, for "Bold" and "Italic" (like in a text program).
AbstractButton {
    id: control

    property string letter: ""
    property bool bold: false
    property bool italic: false
    property string tip: ""

    checkable: true
    implicitWidth: 38
    implicitHeight: Theme.controlHeight
    hoverEnabled: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: tip
    Accessible.checked: checked
    ToolTip.visible: hovered && tip !== ""
    ToolTip.text: tip
    ToolTip.delay: 700

    contentItem: Txt {
        text: control.letter
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.bold: control.bold
        font.italic: control.italic
        font.pixelSize: Theme.fontSizeTitle
        color: !control.enabled ? Theme.fg4 : control.checked ? Theme.accentFg : Theme.fg
        elide: Text.ElideNone
    }

    background: Rectangle {
        radius: Theme.radius
        color: control.checked ? Theme.accent : control.hovered ? Theme.field2 : Theme.field
        border.width: 1
        border.color: control.checked ? Theme.accent : Theme.line
        opacity: control.enabled ? 1.0 : 0.5

        Focusring {
            visible: control.visualFocus
        }
    }
}
