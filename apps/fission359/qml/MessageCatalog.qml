// SPDX-FileCopyrightText: Fission359 contributors
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
// Every message answers three questions (UX §9): what happened (`title`), why and where (`text`,
// with the numbers of `problem.numbers` in it), and what to do (`actionsFor`). The buttons:
// "editValue" (shows the values of the step so that the wrong one can be changed), "hideStep"
// (switches the step off, the rest goes on), "pickAgain" (choose the face or edge again),
// "useMaximum" (the largest value that works), "makeFlush" (closes tiny gaps when combining) and
// "copyDetails" (for a bug report: only for faults of the program, never sent anywhere).
// The raw `detail` of a problem is used nowhere here but in "copyDetails" (checked by
// tests/unit/ui/test_messages.cpp).
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
        "combineNoOverlap": { actions: ["hideStep"] },
        "importFileUnreadable": { actions: ["hideStep"] },
        "importFileEmpty": { actions: ["hideStep"] },
        "referenceNeedsHelp": { actions: ["pickAgain", "hideStep"] }
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
        "notFlat": { card: false, buttons: [] },
        "cannotWrite": { card: true, buttons: ["ok"] }
    })

    /// A length in mm as people write it: "3,1" in German, "3.1" in English, no needless zeros.
    function mm(value) {
        const text = Number(value).toLocaleString(Qt.locale(), "f", 2)
        const point = Qt.locale().decimalPoint
        return text.indexOf(point) < 0 ? text : text.replace(new RegExp("0+$"), "").replace(new RegExp("\\" + point + "$"), "")
    }

    function buttonLabel(id) {
        switch (id) {
        case "inches": return qsTr("It was inches")
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
            return qsTr("This file was made with a newer Fission359 and is opened as far as this version understands it. To keep the original safe, save it under a new name.")
        case "unsavedWork":
            return qsTr("This project has not been saved yet. Save it first, then your work is safe.")
        case "notAFissionFile":
            return qsTr("This is not a Fission359 file.")
        case "documentUnreadable":
            return qsTr("This file is damaged and could not be opened.")
        case "cannotRead":
        default:
            return qsTr("The file could not be read.")
        }
    }

    /// The words for a mistake in the formula of a named measure: { code, subject, path }.
    function measureErrorText(error) {
        switch (error.code) {
        case "cycle":
            return qsTr("These measures use each other: %1.").arg(error.path.join(" → "))
        case "unknownName":
            return qsTr("I don’t know “%1”.").arg(error.subject)
        case "brokenDependency":
            return qsTr("“%1” has a problem itself.").arg(error.subject)
        case "divisionByZero":
            return qsTr("That divides by zero.")
        case "unknownUnit":
            return qsTr("I don’t know the unit “%1”.").arg(error.subject)
        case "wrongKind":
            return qsTr("The result is not the right kind of value (a length, an angle or a plain number).")
        default:
            return qsTr("This formula cannot be calculated.")
        }
    }

    /// What an import says when it did not work.
    readonly property var importEntries: ({
        "unsupported": { card: false, buttons: [] },
        "cannotRead": { card: false, buttons: [] },
        "unreadable": { card: false, buttons: [] },
        "empty": { card: false, buttons: [] }
    })

    /// { card, text, buttons } for an import that did not work (not for the code "").
    function importNotice(code) {
        const known = importEntries[code] !== undefined
        return { card: false, text: importText(known ? code : "unreadable"), buttons: [] }
    }

    function importText(code) {
        switch (code) {
        case "unsupported":
            return qsTr("This kind of file cannot be imported. Fission359 reads STEP and IGES files.")
        case "cannotRead":
            return qsTr("The file could not be read. Is it still there?")
        case "empty":
            return qsTr("This file has no solid body in it, only surfaces or lines. Fission359 can only use solid bodies.")
        case "unreadable":
        default:
            return qsTr("This file could not be read. Is it a STEP or IGES file that is not damaged?")
        }
    }

    /// { card, text, buttons } for a good import that has something to say (a repair, parts
    /// left out, an assembly, an unknown unit); `card` is false when it has nothing to say.
    function importedNotice(notes) {
        const lines = []
        if (notes.repaired > 0) {
            lines.push(qsTr("%n body(ies) had small flaws in the file and was repaired.", "", notes.repaired))
        }
        if (notes.joined > 0) {
            lines.push(qsTr("%n body(ies) was put together from loose surfaces.", "", notes.joined))
        }
        if (notes.skipped > 0) {
            lines.push(qsTr("%n thing(s) in the file are no solid bodies and were left out.", "", notes.skipped))
        }
        if (notes.assembly) {
            lines.push(qsTr("The file is an assembly. Its parts came in as separate bodies."))
        }
        if (notes.unitAssumed) {
            lines.push(qsTr("The file does not say which unit it uses. Millimetres were assumed."))
        }
        return {
            card: lines.length > 0,
            text: lines.join(" "),
            buttons: notes.unitAssumed
                ? [{ id: "inches", text: buttonLabel("inches"), primary: true }, { id: "cancel", text: buttonLabel("ok") }]
                : [{ id: "cancel", text: buttonLabel("ok"), primary: true }]
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
        case "notFlat":
            return qsTr("This face is curved, so it has no flat outline. Choose a flat face.")
        case "cannotWrite":
        default:
            return qsTr("The file could not be written. Is the folder writable and the disk not full?")
        }
    }

    /// The buttons for a problem of `step`: [{ id, label }].
    function actionsFor(problem, step) {
        if (!problem) {
            return []
        }
        const entry = entries[problem.code]
        const ids = entry ? entry.actions : ["hideStep"]
        const largest = problem.numbers ? problem.numbers.maxValue : undefined
        const extra = []
        if (largest !== undefined && largest > 0 && problem.field !== "") {
            extra.push("useMaximum")
        }
        if (problem.code === "kernelResultInvalid" && step && step.type === "combine"
                && !(step.texts && step.texts.fuzzy === "yes")) {
            extra.push("makeFlush")
        }
        if (isFault(problem)) {
            extra.push("copyDetails")
        }
        return extra.concat(ids).map(id => ({
            id: id,
            label: actionLabel(id, largest)
        }))
    }

    function actionLabel(id, largest) {
        switch (id) {
        case "editValue": return qsTr("Change value")
        case "pickAgain": return qsTr("Choose again")
        case "useMaximum": return qsTr("Use the largest value (%1 mm)").arg(mm(Math.floor(largest * 100) / 100))
        case "makeFlush": return qsTr("Make flush")
        case "copyDetails": return qsTr("Copy details for a bug report")
        default: return qsTr("Hide this step")
        }
    }

    /// A fault of the program rather than of the person: the code is one nobody can fix by
    /// changing a value, or one this catalogue does not know.
    function isFault(problem) {
        return !problem || entries[problem.code] === undefined
            || problem.code === "kernelFailed" || problem.code === "namingContractViolated"
    }

    /// What happened, in one short line.
    function title(problem, step) {
        if (!problem) {
            return ""
        }
        switch (problem.code) {
        case "referenceNeedsHelp":
            switch (problem.reason) {
            case "consumed":
                return problem.step !== undefined
                    ? qsTr("This does not exist after step %1.").arg(problem.step)
                    : qsTr("This was used up by an earlier step.")
            case "split": return qsTr("The surface was split.")
            case "turned": return qsTr("This has turned a lot.")
            case "swapped": return qsTr("Two equal ones changed places.")
            default: return qsTr("This has lost its reference.")
            }
        case "parameterOutOfRange":
        case "kernelRejected":
            return problem.numbers && problem.numbers.maxValue !== undefined && problem.field !== ""
                ? qsTr("%1 does not work here.").arg(valueLabel(problem.field))
                : qsTr("These values do not work.")
        case "parameterInvalid": return qsTr("A value cannot be calculated.")
        case "kernelResultInvalid": return qsTr("The result is not clean.")
        case "combineEmpty": return qsTr("Nothing is left.")
        case "combineNoOverlap": return qsTr("The cutter misses the body.")
        case "inputFailed":
        case "inputMissing": return qsTr("An earlier step has a problem.")
        case "textGlyphsMissing":
        case "fontNotFound":
        case "fontStyleMissing": return qsTr("The font does not fit.")
        case "unknownType": return qsTr("This step is not known here.")
        case "cancelled": return qsTr("Calculation replaced.")
        default: return qsTr("That did not work.")
        }
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
        const largest = problem.numbers ? problem.numbers.maxValue : undefined
        if (largest !== undefined && problem.field !== ""
                && (problem.code === "parameterOutOfRange" || problem.code === "kernelRejected")) {
            const given = problem.numbers.given
            return given !== undefined
                ? qsTr("%1 with %2 mm does not work here. The largest value that works is %3 mm.")
                      .arg(valueLabel(problem.field)).arg(mm(given)).arg(mm(largest))
                : qsTr("This value does not work here. The largest value that works is %1 mm.").arg(mm(largest))
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
        case "referenceNeedsHelp": {
            const what = problem.kind === "edge" ? qsTr("edge") : qsTr("face")
            switch (problem.reason) {
            case "split": return qsTr("The %1 was split into pieces, so I cannot tell which one you meant. Choose it again.").arg(what)
            case "consumed":
                return problem.step !== undefined
                    ? qsTr("This %1 does not exist any more after step %2. Choose another one.").arg(what).arg(problem.step)
                    : qsTr("An earlier step used up this %1. Choose another one.").arg(what)
            case "turned": return qsTr("This %1 has turned a lot. Choose it again to be sure.").arg(what)
            case "swapped": return qsTr("Two equal ones seem to have changed places. Choose the %1 you meant.").arg(what)
            default: return qsTr("I cannot find this %1 any more. Choose it again.").arg(what)
            }
        }
        case "combineNoOverlap":
            return qsTr("A cutter does not touch the body, so nothing is cut out. Move it onto the body.")
        case "kernelResultInvalid":
            if (step && step.type === "combine") {
                return qsTr("These bodies cannot be combined cleanly. Move one of them a little, or switch on “Make flush” in the panel of this step.")
            }
            return qsTr("The result is not a clean solid. Try changing a value a little.")
        case "combineEmpty":
            return qsTr("The bodies do not overlap, so nothing is left. Move them so that they touch.")
        case "importFileUnreadable":
            return qsTr("This file could not be read. Is it a STEP or IGES file that is not damaged?")
        case "importFileEmpty":
            return qsTr("This file has no solid body in it, only surfaces or lines. Fission359 can only use solid bodies.")
        case "inputFailed":
        case "inputMissing":
            return qsTr("An earlier step has a problem.")
        case "parameterInvalid":
            return qsTr("%1 cannot be calculated.").arg(valueLabel(problem.field))
        case "unknownType":
            return qsTr("This kind of step is not known to this version of the program.")
        case "cancelled":
            return qsTr("The calculation was replaced by a newer change.")
        case "kernelFailed":
        case "namingContractViolated":
        default:
            return qsTr("This is a fault in the program, not yours. If you copy the details, you can report it.")
        }
    }
}
