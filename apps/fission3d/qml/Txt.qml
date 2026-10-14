// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import Fission3D.Ui

// Text in the interface font; the default size and colour are those of body text.
Text {
    property bool mono: false

    color: Theme.fg2
    font.family: mono ? Theme.monoFamilies[0] : Theme.fontFamilies[0]
    font.pixelSize: Theme.fontSize
    renderType: Text.NativeRendering
    elide: Text.ElideRight
}
