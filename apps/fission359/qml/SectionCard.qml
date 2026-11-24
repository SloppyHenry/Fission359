// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission359.Ui

// The section view (AP-2.11): a plane cuts the bodies away for display. The card chooses the
// axis, the place and the side. Nothing here changes the model or is saved.
Float {
    id: root

    signal closeRequested

    width: 300
    height: content.implicitHeight + 32
    Accessible.name: qsTr("Section view")

    Column {
        id: content

        x: 16
        y: 16
        width: parent.width - 32
        spacing: 10

        Txt {
            text: qsTr("Section view")
            color: Theme.fg
            font.weight: Font.DemiBold
        }

        Txt {
            width: parent.width
            text: qsTr("Drag the slider or type the place. Esc closes the section view; nothing in the project changes.")
            color: Theme.fg3
            font.pixelSize: Theme.fontSizeSmall
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }

        Segmented {
            objectName: "sectionAxis"
            accessibleName: qsTr("Axis")
            current: SectionView.axis
            model: [{ value: "x", text: "X" }, { value: "y", text: "Y" }, { value: "z", text: "Z" }]
            onActivated: value => SectionView.axis = value
        }

        NumberField {
            objectName: "sectionPosition"
            width: parent.width
            label: qsTr("Place")
            kind: "length"
            decimals: 2
            value: SectionView.position
            onTextCommitted: text => {
                const typed = Numbers.evaluate(text, "length")
                if (!isNaN(typed)) {
                    SectionView.position = typed
                }
            }
        }

        Slider {
            id: slider

            objectName: "sectionSlider"
            width: parent.width
            height: 22
            from: SectionView.rangeMin
            to: Math.max(SectionView.rangeMax, SectionView.rangeMin + 0.001)
            value: SectionView.position
            onMoved: SectionView.position = value
            Accessible.name: qsTr("Place")
            hoverEnabled: true

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 3
                radius: 2
                color: Theme.line2
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 14
                height: 14
                radius: 7
                color: Theme.accent
                border.width: 1
                border.color: Theme.accentLine
            }
        }

        Row {
            spacing: 8

            Btn {
                objectName: "sectionFlip"
                text: qsTr("Swap side")
                onClicked: SectionView.flipSide()
            }

            Btn {
                objectName: "sectionClose"
                text: qsTr("Close")
                variant: "primary"
                onClicked: root.closeRequested()
            }
        }
    }
}
