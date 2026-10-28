// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// Why a typed number or formula is wrong, in plain words (rule 5). One place for every field that
// takes a number: the number field of the panels and the value bubbles in the 3D view.
// `info` is the answer of `Numbers.explain`; `kind` is "length", "angle" or "plain".
QtObject {
    id: root

    function text(info, kind) {
        const word = info.subject === undefined ? "" : info.subject
        const guess = info.suggestion === undefined ? "" : info.suggestion
        switch (info.code) {
        case "unknownName":
            return guess !== "" ? qsTr("I don't know “%1”. Did you mean “%2”?").arg(word).arg(guess)
                                : qsTr("I don't know “%1”.").arg(word)
        case "unknownFunction":
            return guess !== "" ? qsTr("I don't know the function “%1”. Did you mean “%2”?").arg(word).arg(guess)
                                : qsTr("I don't know the function “%1”.").arg(word)
        case "unknownUnit":
            return guess !== "" ? qsTr("I don't know the unit “%1”. Did you mean “%2”?").arg(word).arg(guess)
                                : qsTr("I don't know the unit “%1”.").arg(word)
        case "missingClosingBracket":
            return qsTr("A closing bracket is missing.")
        case "unexpectedEnd":
            return qsTr("The formula ends too early.")
        case "unexpectedCharacter":
            return qsTr("This character does not belong here.")
        case "divisionByZero":
            return qsTr("Dividing by zero is not possible.")
        case "incompatibleQuantities":
            return qsTr("Lengths and angles cannot be added.")
        case "wrongKind":
            return kind === "angle" ? qsTr("An angle belongs here, not a length.")
                 : kind === "plain" ? qsTr("A plain number belongs here.")
                 : qsTr("A length belongs here, not an angle.")
        case "unitNotAllowed":
            return qsTr("This number already has a unit.")
        case "wrongArgumentCount":
            return qsTr("This function has the wrong number of values.")
        case "outOfRange":
            return qsTr("This cannot be calculated (for example the root of a negative number).")
        case "notFinite":
            return qsTr("The result is too large.")
        default:
            return qsTr("That is not a number or formula.")
        }
    }
}
