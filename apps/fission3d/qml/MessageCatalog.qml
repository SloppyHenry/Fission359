// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// What the program says when a step has a problem (rule 5: errors are help): for every code of
// the core (`FeatureIssueCode` in feature_type.h) what happened, where, and what can be done
// about it in one click. No raw kernel text ever reaches the person. `scripts/tools/
// check_messages.py` checks that every code has an entry; AP-2.14 grows the catalogue.
//
// The same goes for problems with files and with exporting (`fileEntries`, `exportEntries`, the
// codes of `FileController::fileProblem` and `ExportController::finished`): `fileNotice` and
// `exportNotice` give the sentence and the buttons of the card, or only the sentence for a hint.
//
// `actions` are the buttons under the explanation: "editValue" (shows the values of the step so
// that the wrong one can be changed) and "hideStep" (switches the step off, the rest goes on).
QtObject {
    id: root

    /// function (name) -> the words for a value ("Width"), set by the window.
    property var valueLabel: name => name
    /// function (style) -> the words for a font style, set by the window.
    property var styleLabel: style => style

    readonly property var entries: ({
        "unknownType": { actions: ["hideStep"] },
        "parameterInvalid": { actions: ["editValue", "hideStep"] },
        "parameterOutOfRange": { actions: ["editValue", "hideStep"] },
        "inputMissing": { actions: ["hideStep"] },
        "inputFailed": { actions: ["hideStep"] },
        "namingContractViolated": { actions: ["hideStep"] },
        "kernelRejected": { actions: ["editValue", "hideStep"] },
        "kernelResultInvalid": { actions: ["editValue", "hideStep"] },
        "kernelFailed": { actions: ["editValue", "hideStep"] },
        "cancelled": { actions: [] },
        "textGlyphsMissing": { actions: ["editValue", "hideStep"] },
        "fontNotFound": { actions: ["editValue", "hideStep"] },
        "fontStyleMissing": { actions: ["editValue", "hideStep"] },
        "combineEmpty": { actions: ["hideStep"] },
        "combineNoOverlap": { actions: ["hideStep"] }
    })

    /// Problems with files: "card" is shown as a card with buttons, else as a hint line. The
    /// buttons are ids of `buttonLabel`; the first is the main one.
    readonly property var fileEntries: ({
        "damaged": { card: true, buttons: ["openanyway", "cancel"] },
        "cannotWrite": { card: true, buttons: ["saveas", "ok"] },
        "newerFormat": { card: true, buttons: ["saveas", "ok"] },
        "unsavedWork": { card: true, buttons: ["save", "notnow"] },
        "notAFissionFile": { card: false, buttons: [] },
        "documentUnreadable": { card: false, buttons: [] },
        "cannotRead": { card: false, buttons: [] }
    })

    /// What the export says when it did not end well (the code "" is success, no problem).
    readonly property var exportEntries: ({
        "cancelled": { card: false, buttons: [] },
        "nothing": { card: false, buttons: [] },
        "needsHelp": { card: true, buttons: ["ok"] },
        "cannotWrite": { card: true, buttons: ["ok"] }
    })

    function buttonLabel(id) {
        switch (id) {
        case "openanyway": return qsTr("Open anyway")
        case "cancel": return qsTr("Cancel")
        case "saveas": return qsTr("Save as…")
        case "save": return qsTr("Save…")
        case "notnow": return qsTr("Not now")
        default: return qsTr("OK")
        }
    }

    // The id of a button is what the card reports back: "ok" and "notnow" only close it, "save"
    // asks for a name like "saveas".
    function buttonsOf(entry) {
        return entry.buttons.map((id, i) => ({
            id: id === "ok" || id === "notnow" ? "cancel" : id === "save" ? "saveas" : id,
            text: buttonLabel(id),
            primary: i === 0
        }))
    }

    /// { card, text, buttons } for a problem with a file; an unknown code reads like cannotRead.
    function fileNotice(code) {
        const known = fileEntries[code] !== undefined
        const entry = known ? fileEntries[code] : fileEntries["cannotRead"]
        return { card: entry.card, text: fileText(known ? code : "cannotRead"), buttons: buttonsOf(entry) }
    }

    function fileText(code) {
        switch (code) {
        case "damaged":
            return qsTr("This file is damaged: it does not match what was saved. It can probably still be read.")
        case "cannotWrite":
            return qsTr("The project could not be saved. Your work is still open. Is the folder writable and the disk not full?")
        case "newerFormat":
            return qsTr("This file was made with a newer Fission3D and is opened as far as this version understands it. To keep the original safe, save it under a new name.")
        case "unsavedWork":
            return qsTr("This project has not been saved yet. Save it first, then your work is safe.")
        case "notAFissionFile":
            return qsTr("This is not a Fission3D file.")
        case "documentUnreadable":
            return qsTr("This file is damaged and could not be opened.")
        case "cannotRead":
        default:
            return qsTr("The file could not be read.")
        }
    }

    /// { card, text, buttons } for an export that did not end well (not for the code "").
    function exportNotice(code) {
        const known = exportEntries[code] !== undefined
        const entry = known ? exportEntries[code] : exportEntries["cannotWrite"]
        return { card: entry.card, text: exportText(known ? code : "cannotWrite"), buttons: buttonsOf(entry) }
    }

    function exportText(code) {
        switch (code) {
        case "cancelled":
            return qsTr("Export cancelled.")
        case "nothing":
            return qsTr("There is nothing to export: no visible body.")
        case "needsHelp":
            return qsTr("A body has a problem in its recipe, so it is not exported: the file would not show what you see. Fix the steps marked with a warning first.")
        case "cannotWrite":
        default:
            return qsTr("The file could not be written. Is the folder writable and the disk not full?")
        }
    }

    /// The buttons for a problem: [{ id, label }].
    function actionsFor(problem) {
        if (!problem) {
            return []
        }
        const entry = entries[problem.code]
        const ids = entry ? entry.actions : ["hideStep"]
        return ids.map(id => ({
            id: id,
            label: id === "editValue" ? qsTr("Change value") : qsTr("Hide this step")
        }))
    }

    /// The sentence for a problem of a step: what happened, and what to do.
    function text(problem, step) {
        if (!problem) {
            return ""
        }
        // The reasons a formula is wrong come first: they say exactly where.
        if (problem.detail === "unknownName") {
            return qsTr("I don’t know “%1”.").arg(problem.subject)
        }
        if (problem.detail === "brokenDependency") {
            return qsTr("“%1” has a problem itself.").arg(problem.subject)
        }
        if (problem.detail === "divisionByZero") {
            return qsTr("That divides by zero.")
        }
        switch (problem.code) {
        case "textGlyphsMissing":
            return qsTr("This font has no character for %1. Choose another font or remove it.").arg(problem.field)
        case "fontStyleMissing":
            return qsTr("“%1” has no %2 style. Choose another font or style.")
                .arg(problem.field).arg(styleLabel(step && step.texts ? step.texts.style : "regular"))
        case "fontNotFound":
            return qsTr("The font “%1” is not on this computer. Choose another font.").arg(problem.field)
        case "parameterOutOfRange":
            switch (problem.field) {
            case "text": return qsTr("Type some text.")
            case "cornerRadius": return qsTr("The corners are too round for this size.")
            case "sides":
            case "points": return qsTr("Use a whole number from 3 to 64.")
            case "wall": return qsTr("The wall must be thinner than the radius.")
            case "tubeRadius": return qsTr("The tube must be thinner than the ring radius.")
            case "innerRadius": return qsTr("The notches must lie closer to the middle than the tips.")
            case "topRadius": return qsTr("The top radius cannot be negative.")
            default: return qsTr("%1 must be bigger than zero.").arg(valueLabel(problem.field))
            }
        case "kernelRejected":
            if (problem.field === "position") {
                return qsTr("The cutting plane does not touch the body. Move it into the body.")
            }
            return qsTr("The shape cannot be built with these values. Try changing a value a little.")
        case "combineNoOverlap":
            return qsTr("A cutter does not touch the body, so nothing is cut out. Move it onto the body.")
        case "kernelResultInvalid":
            if (step && step.type === "combine") {
                return qsTr("These bodies cannot be combined cleanly. Move one of them a little, or switch on “Make flush” in the panel of this step.")
            }
            return qsTr("The result is not a clean solid. Try changing a value a little.")
        case "combineEmpty":
            return qsTr("The bodies do not overlap, so nothing is left. Move them so that they touch.")
        case "inputFailed":
        case "inputMissing":
            return qsTr("An earlier step has a problem.")
        case "parameterInvalid":
            return qsTr("%1 cannot be calculated.").arg(valueLabel(problem.field))
        case "unknownType":
            return qsTr("This kind of step is not known to this version of the program.")
        case "cancelled":
            return qsTr("The calculation was replaced by a newer change.")
        default:
            return qsTr("This shape could not be built.")
        }
    }
}
