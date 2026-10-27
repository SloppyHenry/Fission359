// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtCore
import Fission359.Ui
import Fission359.View

Window {
    id: root

    readonly property bool isMac: Qt.platform.os === "osx"
    readonly property string primaryKey: isMac ? "⌘" : qsTr("Ctrl+")
    readonly property string documentName: Files.fileName !== "" ? Files.fileName : qsTr("Untitled")
    property string tool: "move"
    // "Lay on face": the next click on a face puts the chosen body on it (Esc leaves it).
    property bool seating: false
    // Developer option --simulate "filtermenu": opens the menu "Choose: ..." of the 3D view.
    property bool filterMenuOpen: false
    onFilterMenuOpenChanged: if (filterMenuOpen) viewOverlay.openFilterMenu()
    property string display: "color"
    property string leftTab: "bodies"
    property string rightTab: "object"
    property bool focusMode: AppOptions.focusMode
    property bool orthographic: false
    // What the person set up is kept in the settings (Preferences) and comes back at the next start.
    readonly property string mouseStyle: Preferences.mouseStyle
    property bool bedVisible: true
    property string transientHint: ""
    property string flyout: ""         // the open tool group of the palette: "" or "shapes"
    // The scale of the interface (settings): a factor on everything drawn.
    readonly property real uiFactor: Preferences.uiScale / 100
    readonly property real logicalWidth: width / uiFactor
    readonly property bool narrow: logicalWidth < Theme.narrowBreakpoint
    readonly property bool veryNarrow: logicalWidth < Theme.veryNarrowBreakpoint
    readonly property bool sidebarsShown: !focusMode && !veryNarrow
    readonly property int leftWidth: sidebarsShown ? (narrow ? Theme.leftPanelWidthNarrow : Theme.leftPanelWidth) : 0
    readonly property int rightWidth: sidebarsShown ? (narrow ? Theme.rightPanelWidthNarrow : Theme.rightPanelWidth) : 0
    readonly property string bedText: qsTr("%1 × %2").arg(DocumentModel.bedX).arg(DocumentModel.bedY)
    readonly property string printerTitle: DocumentModel.printerName !== ""
        ? DocumentModel.printerName : qsTr("My printer")

    width: 1280
    height: 800
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    // Developer option (environment FISSION_PARK_WINDOW, for the window tests): far outside the
    // screen and without the focus, so that it does not pop up in front of anyone.
    Binding { when: AppOptions.parked; target: root; property: "x"; value: -30000 }
    Binding { when: AppOptions.parked; target: root; property: "y"; value: -30000 }
    Binding {
        when: AppOptions.parked
        target: root
        property: "flags"
        value: Qt.Window | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnBottomHint
    }
    color: Theme.chrome
    // "Name.fission •": the dot says there is something not saved.
    title: (Files.fileName !== "" ? Files.fileName + ".fission" : root.documentName)
           + (Files.modified ? " •" : "") + " – " + qsTr("Fission359")

    // The files opened last, as entries of the File menu (a flat list under a heading).
    readonly property var recentItems: {
        const files = Files.recentFiles
        if (files.length === 0) {
            return []
        }
        const list = [{ separator: true }, { id: "recent.header", text: qsTr("Open recent"), enabled: false }]
        for (let i = 0; i < files.length; ++i) {
            list.push({ id: "recent." + i, text: "    " + Files.baseName(files[i]) })
        }
        return list
    }

    // ---- the menus: one model for the system menu bar and the in-window menus ------------
    readonly property var menuModel: [
        { title: qsTr("File"), items: [
            { id: "new", text: qsTr("New project"), shortcut: primaryKey + "N", nativeShortcut: "Ctrl+N" },
            { id: "open", text: qsTr("Open…"), shortcut: primaryKey + "O", nativeShortcut: "Ctrl+O" }
        ].concat(recentItems, [
            { separator: true },
            { id: "save", text: qsTr("Save"), shortcut: primaryKey + "S", nativeShortcut: "Ctrl+S", enabled: Files.modified || Files.fileName === "" },
            { id: "saveas", text: qsTr("Save as…"), shortcut: isMac ? "⇧⌘S" : qsTr("Ctrl+Shift+S"), nativeShortcut: "Ctrl+Shift+S" },
            { separator: true },
            { id: "export", text: qsTr("Export…"), shortcut: primaryKey + "E", nativeShortcut: "Ctrl+E" },
            { separator: true },
            { id: "close", text: qsTr("Close"), shortcut: primaryKey + "W", nativeShortcut: "Ctrl+W" },
            { id: "quit", text: qsTr("Quit Fission359"), shortcut: primaryKey + "Q", nativeShortcut: "Ctrl+Q", role: "quit" }
        ]) },
        { title: qsTr("Edit"), items: [
            { id: "undo", text: undoText, shortcut: primaryKey + "Z", nativeShortcut: "Ctrl+Z", enabled: root.undoAvailable },
            { id: "redo", text: redoText, shortcut: isMac ? "⇧⌘Z" : qsTr("Ctrl+Shift+Z"), nativeShortcut: "Ctrl+Shift+Z", enabled: root.redoAvailable },
            { id: "history", text: qsTr("Recent actions"), checked: root.historyOpen },
            { separator: true },
            { id: "cut", text: qsTr("Cut"), shortcut: primaryKey + "X", nativeShortcut: "Ctrl+X", enabled: Selection.count > 0 && !root.typing },
            { id: "copy", text: qsTr("Copy"), shortcut: primaryKey + "C", nativeShortcut: "Ctrl+C", enabled: Selection.count > 0 && !root.typing },
            { id: "paste", text: qsTr("Paste"), shortcut: primaryKey + "V", nativeShortcut: "Ctrl+V", enabled: DocumentModel.canPaste && !root.typing },
            { id: "duplicate", text: qsTr("Duplicate"), shortcut: primaryKey + "D", nativeShortcut: "Ctrl+D", enabled: Selection.count > 0 && !root.typing },
            { id: "delete", text: qsTr("Delete"), shortcut: isMac ? "⌫" : qsTr("Del"), enabled: Selection.count > 0 && !root.typing },
            { id: "selectall", text: qsTr("Select all"), shortcut: primaryKey + "A", nativeShortcut: "Ctrl+A", enabled: DocumentModel.bodies.length > 0 || root.typing },
            { id: "selectnone", text: qsTr("Select nothing"), shortcut: qsTr("Esc"), enabled: Selection.count > 0 },
            { separator: true },
            { id: "settings", text: qsTr("Settings…"), shortcut: primaryKey + ",", nativeShortcut: "Ctrl+,", role: "preferences" }
        ] },
        { title: qsTr("View"), items: [
            { id: "orthographic", text: qsTr("Orthographic"), shortcut: "P", checked: root.orthographic },
            { id: "bed", text: qsTr("Print bed"), checked: root.bedVisible },
            { id: "softshadows", text: qsTr("Soft shadows"), checked: viewport.ambientOcclusion },
            { id: "snapgrid", text: qsTr("Grid while dragging"), checked: viewport.snapGridShown },
            { id: "focus", text: qsTr("Focus"), shortcut: isMac ? "⇧F" : qsTr("Shift+F"), checked: root.focusMode },
            { separator: true },
            { id: "view.frame", text: qsTr("Show everything"), shortcut: "F" },
            { id: "view.lookat", text: qsTr("Look at face"), shortcut: "L", enabled: Selection.singleFaceChosen },
            { id: "view.home", text: qsTr("Home view"), shortcut: isMac ? "Home" : qsTr("Pos1") },
            { id: "view.front", text: qsTr("Front"), shortcut: "1" },
            { id: "view.back", text: qsTr("Back"), shortcut: "2" },
            { id: "view.left", text: qsTr("Left"), shortcut: "3" },
            { id: "view.right", text: qsTr("Right"), shortcut: "4" },
            { id: "view.top", text: qsTr("Top"), shortcut: "5" },
            { id: "view.bottom", text: qsTr("Bottom"), shortcut: "6" },
            { separator: true },
            { id: "mouse.fission", text: qsTr("Mouse: like Fission359"), checked: root.mouseStyle === "fission" },
            { id: "mouse.fusion", text: qsTr("Mouse: like Fusion"), checked: root.mouseStyle === "fusion" },
            { id: "mouse.tinkercad", text: qsTr("Mouse: like Tinkercad"), checked: root.mouseStyle === "tinkercad" },
            { id: "mouse.blender", text: qsTr("Mouse: like Blender"), checked: root.mouseStyle === "blender" },
            { id: "mouse.sketchup", text: qsTr("Mouse: like SketchUp"), checked: root.mouseStyle === "sketchup" }
        ] },
        { title: qsTr("Body"), items: [
            { id: "hole", text: qsTr("As hole"), checked: root.chosenAreHoles, enabled: Selection.count > 0 },
            { separator: true },
            { id: "dropToBed", text: qsTr("Drop to bed"), shortcut: "D", enabled: Selection.count > 0 },
            { id: "seatOnFace", text: qsTr("Lay on face"), enabled: Selection.count > 0 },
            { separator: true },
            { id: "merge", text: qsTr("Merge"), enabled: root.chosenBodies.length >= 2 },
            { id: "subtract", text: qsTr("Subtract"), enabled: root.chosenBodies.length >= 2 },
            { id: "overlap", text: qsTr("Keep overlap"), enabled: root.chosenBodies.length >= 2 },
            { id: "split", text: qsTr("Split"), enabled: root.chosenBodies.length === 1 }
        ] },
        { title: qsTr("Window"), items: [
            { id: "minimize", text: qsTr("Minimize"), shortcut: primaryKey + "M", nativeShortcut: "Ctrl+M" },
            { id: "fullscreen", text: qsTr("Full screen"), shortcut: isMac ? "⌃⌘F" : "F11", checked: root.visibility === Window.FullScreen }
        ] },
        { title: qsTr("Help"), items: [
            { id: "search", text: qsTr("Command search"), shortcut: primaryKey + "K", nativeShortcut: "Ctrl+K" },
            { separator: true },
            { id: "about", text: qsTr("About Fission359"), role: "about" }
        ] }
    ]

    // The one sentence of the hint line, whatever state the program is in. The first line that
    // applies wins: a drag or typing in progress, a shape being put down, a message, an open
    // tool group, what is under the pointer, the tool when something is chosen, and at rest what
    // to do next. Everything that wants to tell the person something goes through here (AP-1.12c).
    readonly property string currentHint: root.seating ? qsTr("Click the face the body should lie on. Esc cancels.")
            : Placement.active ? qsTr("Let go to put the shape down here; on a wall it lies on it (%1 keeps it upright). Esc takes it back.").arg(Qt.platform.os === "osx" ? "⌥" : "Alt")
            : Gizmo.editing ? qsTr("Type a distance to move exactly (Tab for the next direction), Enter accepts, Esc puts it back.")
            : Rotate.editing ? qsTr("Type an angle to turn exactly (Tab for the next ring), Enter accepts, Esc puts it back.")
            : Scale.editing ? qsTr("All sizes change alike; the corner across stays. Type a percentage, Enter accepts, Esc puts it back.")
            : Handles.editing ? qsTr("Enter accepts, Tab goes to the next size, Esc leaves everything as it was.")
            : Handles.armed ? qsTr("Type a size (Tab for the next one, Enter to finish) or drag the dots.")
            : root.transientHint !== "" && hintTimer.running ? root.transientHint
            : root.flyout === "combine" ? qsTr("Choose two or more bodies (Shift+click), then merge them, subtract the others from the first or keep only what they share. One body can be split in two.")
            : root.flyout === "shapes" ? qsTr("Drag a shape onto the print bed or onto a face, or click it; on the bed it snaps to corners and middles. Hold Shift to switch snapping off. “As hole” makes it a hole.")
            : root.hoverText !== "" ? root.hoverText
            : Selection.count > 0 ? toolCatalog.find(root.tool).hint
            : DocumentModel.bodies.length === 0 ? qsTr("Start with a shape: open “Shapes” in the toolbar, or press %1 and say what you want to build.").arg(primaryKey + "K")
            : root.navigationText

    // ---- undo and redo ---------------------------------------------------------------------
    // While a text field holds text that was typed and not yet accepted, Ctrl+Z is for that
    // text. A field says so with `pendingEdit` (see NumberField); once the value is accepted,
    // Ctrl+Z is for the document again, even if the cursor stays in the field.
    readonly property bool textHasHistory: {
        const item = root.activeFocusItem
        return item !== null && item !== undefined && item.pendingEdit === true
    }
    readonly property bool undoAvailable: DocumentModel.canUndo && !textHasHistory
    readonly property bool redoAvailable: DocumentModel.canRedo && !textHasHistory
    property bool historyOpen: false
    property bool exportOpen: false
    property bool settingsOpen: false
    property bool startOpen: AppOptions.startPage
    property bool searchOpen: false
    // A text field has the keyboard: keys such as Esc and Ctrl+A are for the text then.
    readonly property bool typing: root.activeFocusItem !== null && root.activeFocusItem !== undefined
                                   && root.activeFocusItem.cursorPosition !== undefined

    readonly property string undoText: DocumentModel.canUndo
        ? qsTr("Undo: %1").arg(historyText(DocumentModel.nextUndo)) : qsTr("Undo")
    readonly property string redoText: DocumentModel.canRedo
        ? qsTr("Redo: %1").arg(historyText(DocumentModel.nextRedo)) : qsTr("Redo")

    // The permanent line in the status bar: how the chosen mouse style moves the view.
    readonly property string navigationText: {
        switch (mouseStyle) {
        case "fusion":
            return qsTr("Drag with the middle mouse button to move the view, with Shift + middle button to turn it, scroll to zoom.")
        case "blender":
        case "sketchup":
            return qsTr("Drag with the middle mouse button to turn the view, with Shift + middle button to move it, scroll to zoom.")
        default:
            return qsTr("Drag with the right mouse button to turn the view, with the middle button to move it, scroll to zoom.")
        }
    }

    function showHint(text) {
        transientHint = text
        hintTimer.restart()
    }

    function toolName(id) {
        return toolCatalog.nameOf(id)
    }

    ToolCatalog {
        id: toolCatalog
    }

    CommandCatalog {
        id: commandCatalog

        tools: toolCatalog
        shapes: shapeCatalog
        isMac: root.isMac
        primaryKey: root.primaryKey
    }

    // Why a command cannot run now, in a sentence; empty when it can.
    function commandReason(command) {
        if (command.run.kind === "soon") {
            return qsTr("Arrives in a later version.")
        }
        switch (command.needs) {
        case "selection":
            if (chosenBodies.length < 1) { return qsTr("Choose a body first.") }
            break
        case "one":
            if (chosenBodies.length !== 1) { return qsTr("Choose one body first.") }
            break
        case "two":
            if (chosenBodies.length < 2) { return qsTr("Choose two bodies first (Shift+click).") }
            break
        }
        if (command.id === "menu.undo" && !undoAvailable) { return qsTr("Nothing to undo.") }
        if (command.id === "menu.redo" && !redoAvailable) { return qsTr("Nothing to redo.") }
        if (command.id === "menu.paste" && !DocumentModel.canPaste) { return qsTr("Copy something first.") }
        return ""
    }

    // The results for the command search: the ids the search ranks, with what the list shows.
    function searchResults(text) {
        const ids = CommandSearch.search(text, commandCatalog.searchables())
        return ids.slice(0, text === "" ? 8 : 30).map(id => {
            const command = commandCatalog.find(id)
            return { id: id, name: command.name, shortcut: command.shortcut || "",
                     group: command.group, reason: commandReason(command) }
        })
    }

    // The best match for a text (the tests use it); empty when nothing fits.
    function topCommand(text) {
        const ids = CommandSearch.search(text, commandCatalog.searchables())
        return ids.length > 0 ? ids[0] : ""
    }

    // Does what the command says. A command that cannot run says why instead.
    function runCommand(id) {
        const command = commandCatalog.find(id)
        if (command === null) {
            return false
        }
        const reason = commandReason(command)
        if (reason !== "") {
            showHint(command.name + ": " + reason)
            return false
        }
        switch (command.run.kind) {
        case "tool": tool = command.run.id; break
        case "shape": Placement.add(command.run.id, false); break
        case "menu": run(command.run.id); break
        }
        return true
    }

    // What the tool of the palette shows on the chosen body (AP-1.6, docs/08-gestaltung.md §5):
    // "Move" the gizmo and the measure bands, "Rotate" the rings, "Size" the dots for the sizes
    // and the corners that scale evenly,
    // "Select" nothing.
    // A body can be dragged to move it in every tool but "Select".
    Binding { target: Handles; property: "mode"; value: root.tool === "size" ? "size" : root.tool === "move" ? "labels" : "off" }
    Binding { target: Gizmo; property: "enabled"; value: root.tool === "move" }
    Binding { target: Distances; property: "enabled"; value: root.tool === "move" }
    Binding { target: Align; property: "enabled"; value: root.tool === "move" }
    Binding { target: Rotate; property: "enabled"; value: root.tool === "rotate" }
    Binding { target: Scale; property: "enabled"; value: root.tool === "size" }
    // Pulling a corner ends "type the sizes" right after putting a shape down: the digits are
    // the percentage now, and afterwards there is nothing waiting to be typed.
    Connections {
        target: Scale

        function onEditingChanged() {
            if (Scale.editing) {
                Handles.armed = false
            }
        }
    }

    // ---- deleting (AP-1.9): the steps that make the chosen bodies --------------------------------
    // What deleting would break, as a question in the recipe: { steps, dependents, text, canHide }.
    property var pendingDeletion: null

    function circled(position) {
        return String.fromCharCode(0x2460 + position)
    }

    // Asks first when other steps use what goes, or when more than the chosen step goes.
    function requestDeletion(plan, canHide) {
        if (plan.steps.length === 0) {
            return
        }
        if (plan.dependents.length === 0 && plan.steps.length === 1) {
            DocumentModel.deleteSteps(plan.steps)
            return
        }
        const list = plan.dependents.map(circled).join(" ")
        const text = plan.dependents.length > 0
            ? qsTr("%n step(s) use what you delete: %1. They will need help.", "", plan.dependents.length).arg(list)
            : qsTr("This also deletes %n shape(s) that are only there for it.", "", plan.steps.length - 1)
        pendingDeletion = { steps: plan.steps, dependents: plan.dependents, text: text,
                            canHide: canHide && plan.steps.length === 1 }
    }

    function deleteChosen() {
        if (chosenBodies.length > 0) {
            requestDeletion(DocumentModel.deletionOfBodies(chosenBodies), false)
        }
    }

    Connections {
        target: DocumentModel

        // The question is about a recipe that is not there any more.
        function onRecipeChanged() { root.pendingDeletion = null }
    }

    // Leaving the program: a project with a file is saved on the way out (nothing to ask).
    function quitProgram() {
        if (Files.leave()) {
            Qt.quit()
        }
    }

    onClosing: close => {
        if (!Files.leave()) {
            close.accepted = false // the reason is on the card; the work stays open
        }
    }

    function run(id) {
        if (id.startsWith("recent.")) {
            const files = Files.recentFiles
            const path = files[parseInt(id.substring(7))]
            if (path !== undefined && !Files.openPath(path)) {
                Files.forgetRecent(path)
            }
            return
        }
        switch (id) {
        case "quit": quitProgram(); break
        case "new": Files.newDocument(); break
        case "close": Files.closeTab(Files.currentTab); break
        case "open": openDialog.open(); break
        case "save":
            if (Files.fileName === "") {
                saveDialog.open()
            } else {
                Files.save()
            }
            break
        case "saveas": saveDialog.open(); break
        case "export": exportOpen = !exportOpen; break
        case "about": aboutWindow.show(); aboutWindow.raise(); break
        case "settings": settingsOpen = !settingsOpen; break
        case "search": searchOpen = !searchOpen; break
        case "undo": DocumentModel.undo(); break
        case "redo": DocumentModel.redo(); break
        case "history": historyOpen = !historyOpen; break
        case "orthographic": orthographic = !orthographic; break
        case "bed": bedVisible = !bedVisible; break
        case "softshadows":
            viewport.ambientOcclusion = !viewport.ambientOcclusion
            Preferences.softShadows = viewport.ambientOcclusion
            break
        case "snapgrid":
            viewport.snapGridShown = !viewport.snapGridShown
            Preferences.snapGridShown = viewport.snapGridShown
            break
        case "focus": focusMode = !focusMode; break
        case "minimize": root.showMinimized(); break
        case "fullscreen":
            root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
            break
        case "view.frame": viewport.frameAll(); break
        case "view.lookat": Selection.lookAtChosenFace(); break
        case "selectall":
            if (typing) {
                root.activeFocusItem.selectAll() // the text of a field, not the bodies
            } else {
                Selection.chooseAllBodies()
            }
            break
        case "selectnone": Selection.clear(); Handles.armed = false; break
        case "copy":
            if (!DocumentModel.copyBodies(chosenBodies)) {
                showHint(qsTr("This body cannot be copied yet."))
            }
            break
        case "paste": Selection.chooseBodies(DocumentModel.pasteBodies()); break
        case "cut":
            if (DocumentModel.copyBodies(chosenBodies)) {
                deleteChosen()
            } else {
                showHint(qsTr("This body cannot be copied yet."))
            }
            break
        case "delete": deleteChosen(); break
        case "duplicate": {
            const copies = DocumentModel.duplicateBodies(chosenBodies)
            if (copies.length > 0) {
                Selection.chooseBodies(copies)
            } else {
                showHint(qsTr("This body cannot be copied yet."))
            }
            break
        }
        case "dropToBed":
            if (!DocumentModel.dropToBed(chosenBodies)) {
                showHint(qsTr("Nothing to drop: the bodies already stand on the bed."))
            }
            break
        case "seatOnFace": seating = true; break
        case "split": splitChosen(); break
        case "merge":
        case "subtract":
        case "overlap": combine(id); break
        case "hole": {
            const holeNow = !root.chosenAreHoles
            for (const index of root.chosenBodyIndexes) {
                DocumentModel.setBodyHole(index, holeNow)
            }
            break
        }
        default:
            if (id.indexOf("mouse.") === 0) {
                Preferences.mouseStyle = id.substring(6)
            } else if (id.indexOf("view.") === 0) {
                viewport.setStandardView(id.substring(5))
            } else {
                showHint(qsTr("This arrives in a later version."))
            }
        }
    }

    readonly property string currentFontName: {
        const entry = DocumentModel.stepTexts.find(t => t.kind === "font")
        return entry ? entry.value : ""
    }
    readonly property string currentStyleName: {
        const entry = DocumentModel.stepTexts.find(t => t.kind === "style")
        return entry ? entry.value : "regular"
    }

    // A word of the chosen step was changed (text, font or style). A font that lacks the style in
    // use takes it back to regular in the same step, and says so.
    function splitChosen() {
        if (DocumentModel.splitBody(root.chosenBodies[0]) < 0) {
            showHint(qsTr("This body cannot be split: it may be part of another body."))
            return
        }
        root.flyout = ""
        root.tool = "size" // its handle, the dot on the plane, is shown by this tool
    }

    // Combines the chosen bodies: the one chosen first is changed, the others are the tools.
    function combine(way) {
        if (way === "split") {
            splitChosen()
            return
        }
        const made = DocumentModel.combineBodies(root.chosenBodies, way)
        if (made === "") {
            showHint(qsTr("Choose two or more bodies that are not part of another body yet."))
            return
        }
        root.flyout = ""
        Selection.chooseBodies([made])
    }

    function changeWords(name, text) {
        const step = DocumentModel.currentStep
        if (name === "font" && currentStyleName !== "regular"
                && DocumentModel.fontStyles(text).length > 0
                && DocumentModel.fontStyles(text).indexOf(currentStyleName) < 0) {
            DocumentModel.beginChange("font")
            DocumentModel.setStepText(step, "font", text)
            DocumentModel.setStepText(step, "style", "regular")
            DocumentModel.commitChange()
            showHint(qsTr("“%1” has no %2 style, so the lettering is regular again.")
                     .arg(text).arg(styleLabel(currentStyleName)))
            return
        }
        DocumentModel.setStepText(step, name, text)
    }

    function styleLabel(style) {
        switch (style) {
        case "bold": return qsTr("bold")
        case "italic": return qsTr("italic")
        case "boldItalic": return qsTr("bold italic")
        default: return qsTr("regular")
        }
    }

    // A shape that was just put down: lettering is typed right away.
    Connections {
        target: DocumentModel

        function onShapePlaced(step, type) {
            if (type === "text") {
                rightTab = "object"
                rightPanel.focusText()
            }
        }
    }

    Component.onCompleted: {
        if (AppOptions.showHistory) { // developer option, for screenshots
            DocumentModel.selectStep(0)
            DocumentModel.setStepValue(0, "width", "35")
            DocumentModel.setStepValue(0, "height", "2 cm + 5")
            DocumentModel.setBodyVisible(0, false)
            DocumentModel.setBodyVisible(0, true)
            DocumentModel.undo()
            historyOpen = true
        }
        if (AppOptions.showShapes) { // developer option, for screenshots
            flyout = "shapes"
        }
        if (AppOptions.camera !== "") { // developer option, for comparison images
            const v = AppOptions.camera.split(",").map(Number)
            if (v.length === 3 && v.every(x => !isNaN(x))) {
                viewport.setCamera(v[0], v[1], v[2])
            }
        }
        if (AppOptions.rightTab === "printer" || AppOptions.rightTab === "object") {
            rightTab = AppOptions.rightTab
        }
        viewport.snapGridShown = Preferences.snapGridShown
        viewport.fixedSnapStep = Preferences.snapStep
        if (!AppOptions.softShadows || !Preferences.softShadows) {
            viewport.ambientOcclusion = false
        }
        if (AppOptions.openMenu >= 0) {
            openMenuTimer.start()
        }
    }

    Timer {
        id: openMenuTimer

        interval: 300
        onTriggered: menuStrip.openMenu(AppOptions.openMenu)
    }

    Timer {
        id: hintTimer

        interval: 5000
    }

    Loader {
        active: root.isMac
        source: "NativeMenuBar.qml"
        onLoaded: {
            item.menuData = Qt.binding(() => root.menuModel)
            item.actionTriggered.connect(root.run)
        }
    }

    // ---- shortcuts (the menus of macOS bring their own) ------------------------------------
    Shortcut { sequence: "Shift+F"; onActivated: root.run("focus") }
    Shortcut { sequence: "F"; onActivated: root.run("view.frame") }
    Shortcut { sequence: "L"; enabled: Selection.singleFaceChosen; onActivated: root.run("view.lookat") }
    Shortcut { sequence: "Ctrl+A"; enabled: !root.isMac; onActivated: root.run("selectall") }
    Shortcut { sequence: "Ctrl+C"; enabled: !root.isMac && Selection.count > 0 && !root.typing; onActivated: root.run("copy") }
    Shortcut { sequence: "Ctrl+V"; enabled: !root.isMac && DocumentModel.canPaste && !root.typing; onActivated: root.run("paste") }
    Shortcut { sequence: "Delete"; enabled: Selection.count > 0 && !root.typing && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("delete") }
    Shortcut { sequence: "Backspace"; enabled: Selection.count > 0 && !root.typing && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("delete") }
    Shortcut { sequence: "Ctrl+X"; enabled: !root.isMac && Selection.count > 0 && !root.typing; onActivated: root.run("cut") }
    Shortcut { sequence: "Ctrl+D"; enabled: !root.isMac && Selection.count > 0 && !root.typing; onActivated: root.run("duplicate") }
    Shortcut { sequence: "D"; enabled: Selection.count > 0 && !root.typing && !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("dropToBed") }
    Shortcut {
        sequence: "Escape"
        enabled: root.seating
        onActivated: root.seating = false
    }
    Shortcut {
        sequence: "Escape"
        enabled: Selection.count > 0 && !root.typing && !Handles.editing && !Gizmo.editing
                 && !Rotate.editing && !Scale.editing && !root.seating
        onActivated: root.run("selectnone")
    }
    Shortcut { sequence: "P"; onActivated: root.run("orthographic") }
    Shortcut { sequence: "Home"; onActivated: root.run("view.home") }
    Shortcut { sequence: "1"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("view.front") }
    Shortcut { sequence: "2"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("view.back") }
    Shortcut { sequence: "3"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("view.left") }
    Shortcut { sequence: "4"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("view.right") }
    Shortcut { sequence: "5"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("view.top") }
    Shortcut { sequence: "6"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing && !Scale.editing; onActivated: root.run("view.bottom") }
    // A shape that was just put down waits for its sizes: digits type them (they are the views
    // again as soon as something else is chosen). Tab or Enter start typing, too.
    Repeater {
        model: ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ",", "."]

        delegate: Item {
            id: digit

            required property string modelData

            Shortcut {
                sequence: digit.modelData
                enabled: (Handles.armed || Handles.editing || Gizmo.editing || Rotate.editing
                          || Scale.editing)
                         && !handleLayer.typing && !gizmoLayer.typing && !rotateLayer.typing
                         && !scaleLayer.typing && !distanceLayer.typing && !root.typing
                onActivated: Gizmo.editing ? gizmoLayer.startTyping(digit.modelData)
                             : Rotate.editing ? rotateLayer.startTyping(digit.modelData)
                             : Scale.editing ? scaleLayer.startTyping(digit.modelData)
                                             : handleLayer.startTyping(digit.modelData)
            }
        }
    }
    Shortcut {
        sequences: ["Tab", "Return", "Enter"]
        enabled: Handles.armed && !Handles.editing && !root.typing
        onActivated: handleLayer.startTyping("")
    }
    // Arrow keys move the chosen body by 1 mm (with Shift 10 mm) along the axis of the bed that
    // is closest to the way on screen; Page up and Page down lift and lower it (like Tinkercad).
    readonly property bool nudgeKeys: Selection.count > 0 && root.tool !== "select" && !root.typing
                                      && !Gizmo.editing && !Handles.editing && !Rotate.editing
                                      && !Scale.editing
    Shortcut { sequence: "Left"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(-1, 0, 0, false) }
    Shortcut { sequence: "Right"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(1, 0, 0, false) }
    Shortcut { sequence: "Up"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, 1, 0, false) }
    Shortcut { sequence: "Down"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, -1, 0, false) }
    Shortcut { sequence: "PgUp"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, 0, 1, false) }
    Shortcut { sequence: "PgDown"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, 0, -1, false) }
    Shortcut { sequence: "Shift+Left"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(-1, 0, 0, true) }
    Shortcut { sequence: "Shift+Right"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(1, 0, 0, true) }
    Shortcut { sequence: "Shift+Up"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, 1, 0, true) }
    Shortcut { sequence: "Shift+Down"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, -1, 0, true) }
    Shortcut { sequence: "Shift+PgUp"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, 0, 1, true) }
    Shortcut { sequence: "Shift+PgDown"; enabled: root.nudgeKeys; onActivated: Gizmo.nudge(0, 0, -1, true) }
    Shortcut { sequence: "Ctrl+K"; onActivated: root.run("search") }
    Shortcut { sequence: "Ctrl+,"; onActivated: root.run("settings") }
    Shortcut { sequence: "Ctrl+Q"; enabled: !root.isMac; onActivated: root.run("quit") }
    Shortcut { sequence: "Ctrl+N"; enabled: !root.isMac; onActivated: root.run("new") }
    Shortcut { sequence: "Ctrl+O"; enabled: !root.isMac; onActivated: root.run("open") }
    Shortcut { sequence: "Ctrl+E"; enabled: !root.isMac; onActivated: root.run("export") }
    Shortcut { sequence: "Ctrl+W"; enabled: !root.isMac; onActivated: root.run("close") }
    Shortcut { sequence: "Ctrl+S"; enabled: !root.isMac; onActivated: root.run("save") }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !root.isMac; onActivated: root.run("saveas") }
    // Undo and redo: on macOS the menu of the system carries them.
    Shortcut { sequence: "Ctrl+Z"; enabled: !root.isMac && root.undoAvailable; onActivated: root.run("undo") }
    Shortcut {
        sequences: ["Ctrl+Y", "Ctrl+Shift+Z"]
        enabled: !root.isMac && root.redoAvailable
        onActivated: root.run("redo")
    }

    // ---- layout (docs/08-gestaltung.md §1) ----------------------------------------------------
    // Everything visible lives in `shell`, which is made larger or smaller as a whole when the
    // person sets another scale (80 to 200 %): no restart, and the layout works with the window
    // size divided by the scale.
    Item {
        id: shell

        width: root.width / root.uiFactor
        height: root.height / root.uiFactor
        scale: root.uiFactor
        transformOrigin: Item.TopLeft

    MenuStrip {
        id: menuStrip

        anchors.top: parent.top
        width: parent.width
        menus: root.menuModel
        inWindowMenus: !root.isMac
        printerText: qsTr("%1 · %2 mm").arg(root.printerTitle).arg(root.bedText)
        onActionTriggered: id => root.run(id)
        onSearchRequested: root.run("search")
        onSettingsRequested: root.run("settings")
        onPrinterRequested: {
            root.rightTab = "printer"
            root.focusMode = false
        }
    }

    FileDialog {
        id: openDialog

        title: qsTr("Open")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Fission359 files (*.fission)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: Files.openFrom(selectedFile)
    }

    FileDialog {
        id: saveDialog

        title: qsTr("Save")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "fission"
        nameFilters: [qsTr("Fission359 files (*.fission)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: Files.saveTo(selectedFile)
    }

    // The file name for an export is asked after the choices on the card, by the system.
    property string exportFormat: "3mf"
    property string exportQuality: "normal"
    property bool exportOnlyChosen: false

    FileDialog {
        id: exportDialog

        title: qsTr("Export")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.exportFormat
        nameFilters: root.exportFormat === "stl" ? [qsTr("STL files (*.stl)")] : [qsTr("3MF files (*.3mf)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onAccepted: {
            const names = {}
            for (let i = 0; i < DocumentModel.bodies.length; ++i) {
                names[DocumentModel.bodyIdAt(i)] = root.bodyName(i)
            }
            Exporter.start(root.exportFormat, root.exportQuality,
                           root.exportOnlyChosen ? root.chosenBodies : [], selectedFile, names)
        }
    }

    Connections {
        target: Exporter

        function onFinished(code, path, bodies) {
            switch (code) {
            case "":
                root.showHint(qsTr("%n body(ies) exported.", "", bodies))
                root.exportOpen = false
                break
            case "cancelled":
                root.showHint(qsTr("Export cancelled."))
                break
            case "nothing":
                root.showHint(qsTr("There is nothing to export: no visible body."))
                break
            case "needsHelp":
                fileCard.show(qsTr("A body has a problem in its recipe, so it is not exported: the file would not show what you see. Fix the steps marked with a warning first."),
                              [{ id: "cancel", text: qsTr("OK"), primary: true }])
                break
            default:
                fileCard.show(qsTr("The file could not be written. Is the folder writable and the disk not full?"),
                              [{ id: "cancel", text: qsTr("OK"), primary: true }])
                break
            }
        }
    }

    ExportCard {
        id: exportCard

        visible: root.exportOpen
        anchors.top: docTabs.bottom
        anchors.topMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 16
        z: 19
        anyChosen: root.chosenBodies.length > 0
        onCloseRequested: root.exportOpen = false
        onExportRequested: (format, quality, onlyChosen) => {
            root.exportFormat = format
            root.exportQuality = quality
            root.exportOnlyChosen = onlyChosen
            exportDialog.open()
        }
    }

    // What happens to the 3D view when a file replaces the document: nothing chosen any more.
    Connections {
        target: DocumentModel

        function onDocumentReplaced() {
            Selection.clear()
            Handles.armed = false
        }
    }

    // Problems with files are a card with the way on, never a dialog (rule 5).
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
            switch (code) {
            case "damaged":
                fileCard.show(qsTr("This file is damaged: it does not match what was saved. It can probably still be read."),
                              [{ id: "openanyway", text: qsTr("Open anyway"), primary: true },
                               { id: "cancel", text: qsTr("Cancel") }], path)
                break
            case "cannotWrite":
                fileCard.show(qsTr("The project could not be saved. Your work is still open. Is the folder writable and the disk not full?"),
                              [{ id: "saveas", text: qsTr("Save as…"), primary: true },
                               { id: "cancel", text: qsTr("OK") }])
                break
            case "newerFormat":
                fileCard.show(qsTr("This file was made with a newer Fission359 and is opened as far as this version understands it. To keep the original safe, save it under a new name."),
                              [{ id: "saveas", text: qsTr("Save as…"), primary: true },
                               { id: "cancel", text: qsTr("OK") }])
                break
            case "unsavedWork":
                fileCard.show(qsTr("This project has not been saved yet. Save it first, then your work is safe."),
                              [{ id: "saveas", text: qsTr("Save…"), primary: true },
                               { id: "cancel", text: qsTr("Not now") }])
                break
            case "notAFissionFile":
                root.showHint(qsTr("This is not a Fission359 file."))
                break
            case "documentUnreadable":
                root.showHint(qsTr("This file is damaged and could not be opened."))
                break
            default:
                root.showHint(qsTr("The file could not be read."))
                break
            }
        }
    }

    Connections {
        target: Files

        function onOpened() {
            root.startOpen = false
        }
    }

    StartPage {
        id: startPage

        visible: root.startOpen
        anchors.top: docTabs.bottom
        anchors.bottom: statusBar.top
        width: parent.width
        z: 15
        recent: Files.recentTiles()
        examples: Files.exampleNames()
        onVisibleChanged: if (visible) recent = Files.recentTiles()
        onCloseRequested: root.startOpen = false
        onNewRequested: if (Files.newDocument()) root.startOpen = false
        onOpenRequested: path => { if (Files.openPath(path)) root.startOpen = false }
        onExampleRequested: name => { if (Files.openExample(name)) root.startOpen = false }
    }

    FirstRunCard {
        id: firstRunCard

        visible: AppOptions.firstRun && !Preferences.firstRunDone
        anchors.top: docTabs.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        z: 23
    }

    CommandPalette {
        id: commandPalette

        visible: root.searchOpen
        anchors.top: docTabs.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        z: 24
        source: root.searchResults
        onVisibleChanged: if (visible) open()
        onCloseRequested: root.searchOpen = false
        onChosen: id => {
            root.searchOpen = false
            root.runCommand(id)
        }
    }

    SettingsPage {
        id: settingsPage

        visible: root.settingsOpen
        anchors.top: docTabs.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        z: 22
        onCloseRequested: root.settingsOpen = false
        onPrinterRequested: {
            root.settingsOpen = false
            root.rightTab = "printer"
            root.focusMode = false
        }
    }

    NoticeCard {
        id: fileCard

        anchors.top: docTabs.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        z: 20
        onChosen: (id, payload) => {
            if (id === "openanyway") {
                Files.openPath(payload, true)
            } else if (id === "saveas") {
                saveDialog.open()
            }
        }
    }

    NoticeCard {
        id: rescueCard

        anchors.top: docTabs.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        z: 21
        onChosen: (id, payload) => {
            if (id === "restore") {
                Files.restoreRescued(payload)
            } else {
                Files.discardRescued(payload)
            }
        }
    }

    DocTabs {
        id: docTabs

        anchors.top: menuStrip.bottom
        width: parent.width
        tabs: Files.tabs
        canUndo: DocumentModel.canUndo
        canRedo: DocumentModel.canRedo
        undoTip: DocumentModel.canUndo ? qsTr("Undo: %1").arg(root.historyText(DocumentModel.nextUndo)) : ""
        redoTip: DocumentModel.canRedo ? qsTr("Redo: %1").arg(root.historyText(DocumentModel.nextRedo)) : ""
        onUndoRequested: root.run("undo")
        onRedoRequested: root.run("redo")
        onHomeRequested: root.startOpen = !root.startOpen
        onNewDocumentRequested: root.run("new")
        onTabClicked: index => Files.switchTab(index)
        onCloseRequested: index => Files.closeTab(index)
        onExportRequested: root.run("export")
    }

    Item {
        id: middle

        anchors.top: docTabs.bottom
        anchors.bottom: statusBar.top
        width: parent.width

        LeftPanel {
            id: leftPanel

            width: root.leftWidth
            height: parent.height
            visible: root.leftWidth > 0
            documentName: root.documentName
            tab: root.leftTab
            bodies: root.bodyList
            bedVisible: root.bedVisible
            bedText: root.bedText
            recipe: root.recipeList
            currentStep: DocumentModel.currentStep
            onTabRequested: tab => root.leftTab = tab
            onAddRequested: root.flyout = "shapes"
            deletion: root.pendingDeletion
            onStepRequested: index => DocumentModel.selectStep(DocumentModel.currentStep === index ? -1 : index)
            onStepHovered: (index, on) => viewport.lightUpBodies(on ? [DocumentModel.bodyOfStep(index)] : [])
            onStepRenamed: (index, text) => DocumentModel.setStepLabel(index, text)
            onStepHideRequested: (index, hide) => DocumentModel.setStepSuppressed(index, hide)
            onStepDeleteRequested: index => root.requestDeletion(DocumentModel.deletionOfStep(index), true)
            onDeleteConfirmed: {
                const steps = root.pendingDeletion.steps
                root.pendingDeletion = null
                DocumentModel.deleteSteps(steps)
            }
            onHideInstead: {
                const steps = root.pendingDeletion.steps
                root.pendingDeletion = null
                DocumentModel.setStepSuppressed(steps[0], true)
            }
            onDeleteCancelled: root.pendingDeletion = null
            marker: DocumentModel.marker
            onMarkerRequested: count => DocumentModel.moveMarker(count)
            onBodyVisibilityRequested: (index, visible) => DocumentModel.setBodyVisible(index, visible)
            onBodyChosen: (index, extend) => Selection.chooseBody(index, extend)
            onBodyRenamed: (index, text) => DocumentModel.setBodyName(index, text)
            onBodyHovered: (index, on) => viewport.lightUpBodies(on ? [DocumentModel.bodyIdAt(index)] : [])
            onBodyColorChosen: (index, color) => DocumentModel.setBodiesColor([index], color)
            onBedVisibilityRequested: visible => root.bedVisible = visible
        }

        Item {
            id: viewArea

            x: root.leftWidth
            width: parent.width - root.leftWidth - root.rightWidth
            height: parent.height
            clip: true

            Viewport3D {
                id: viewport

                objectName: "viewport"
                anchors.fill: parent
                bedVisible: root.bedVisible
                orthographic: root.orthographic
                displayMode: root.display
                viewCenter: Theme.view
                viewEdge: Theme.view2
                bedColor: Theme.bed
                bedEdgeColor: Theme.bedEdge
                gridColor: Theme.grid
                gridMajorColor: Theme.gridMajor
                axisXColor: Theme.axisX
                axisYColor: Theme.axisY
                originColor: Theme.fg3
                edgeColor: Theme.edge
                shadowColor: Theme.shadow
                accentColor: Theme.accentLine
                animated: !AppOptions.reduceMotion
                mouseStyle: root.mouseStyle
                bodiesMovable: root.tool !== "select"
                onAmbientOcclusionTurnedOff: root.showHint(
                    qsTr("Soft shadows are off because the graphics card could not keep up. You can switch them on again in the View menu."))
            }

            // the frame being dragged to choose several things
            Rectangle {
                id: rubberBand

                property rect area: Qt.rect(0, 0, 0, 0)

                visible: area.width > 0 && area.height > 0
                x: area.x
                y: area.y
                width: area.width
                height: area.height
                color: Qt.rgba(Theme.accentLine.r, Theme.accentLine.g, Theme.accentLine.b, 0.12)
                border.width: 1
                border.color: Theme.accentLine
            }

            Connections {
                target: viewport

                function onRubberBandChanged(area) {
                    rubberBand.area = area
                }

                // A body pressed and dragged moves (every tool but "Select"). It is chosen first,
                // unless something of it is chosen already.
                function onBodyDragStarted(body, x, y) {
                    if (DocumentModel.bodyOfStep(DocumentModel.currentStep) !== body) {
                        Selection.chooseBody(DocumentModel.bodyIndexOf(body), false)
                    }
                    Gizmo.beginBodyDrag(x, y)
                }

                function onBodyDragMoved(x, y) {
                    if (Gizmo.editing && !gizmoLayer.typing) {
                        Gizmo.dragTo(x, y)
                    }
                }

                function onBodyDragFinished() {
                    if (Gizmo.editing && !gizmoLayer.typing) {
                        Gizmo.endDrag()
                    }
                }
            }

            HandleLayer {
                id: handleLayer

                anchors.fill: parent
                labelFor: root.valueLabel
                obstacles: viewOverlay.obstacles
            }

            GizmoLayer {
                id: gizmoLayer

                anchors.fill: parent
                obstacles: viewOverlay.obstacles
            }

            AlignLayer {
                id: alignLayer

                anchors.fill: parent
            }

            DistanceLayer {
                id: distanceLayer

                anchors.fill: parent
                obstacles: viewOverlay.obstacles
            }

            // The cutting plane of the chosen split, as a translucent sheet.
            SplitPlaneLayer {
                anchors.fill: parent
                viewport: viewport
                plane: root.splitPlaneInfo
            }

            // The label "Cuts out: …" on a chosen hole.
            CutLayer {
                anchors.fill: parent
                viewport: viewport
                info: root.cutLabelInfo
                nameOf: root.bodyName
                onTargetToggled: (bodyIndex, cut) => DocumentModel.setCutTarget(root.chosenHoleIndex, bodyIndex, cut)
            }

            // "Lay on face": the next click puts the chosen body on the face under the pointer.
            MouseArea {
                anchors.fill: parent
                enabled: root.seating
                visible: root.seating
                cursorShape: Qt.CrossCursor
                z: 5
                onClicked: mouse => {
                    if (Placement.seatChosen(mouse.x, mouse.y)) {
                        root.seating = false
                    } else {
                        root.showHint(qsTr("Click a face of the print bed or of another body."))
                    }
                }
            }

            RotateLayer {
                id: rotateLayer

                anchors.fill: parent
                obstacles: viewOverlay.obstacles
            }

            ScaleLayer {
                id: scaleLayer

                anchors.fill: parent
                obstacles: viewOverlay.obstacles
            }

            ContextBar {
                anchors.fill: parent
                viewport: viewport
                obstacles: viewOverlay.obstacles
                kind: root.contextKind
                bodyIds: root.chosenBodies
                allHoles: root.chosenAreHoles
                bodyColor: root.chosenColor
                busy: Handles.editing || Gizmo.editing || Rotate.editing || Scale.editing
                      || Placement.active || root.seating || root.typing
                onActionRequested: action => root.run(action)
                onColorChosen: color => DocumentModel.setBodiesColor(root.colorTargets, color)
            }

            BedLabels {
                anchors.fill: parent
                viewport: viewport
                printerText: menuStrip.printerText
            }

            ViewOverlay {
                id: viewOverlay

                anchors.fill: parent
                orthographic: root.orthographic
                display: root.display
                tool: root.tool
                focusMode: root.focusMode
                mouseStyle: root.mouseStyle
                filter: Selection.filter
                onFilterRequested: filter => Selection.filter = filter
                cameraYaw: viewport.cameraYaw
                cameraPitch: viewport.cameraPitch
                onOrthographicToggled: root.orthographic = !root.orthographic
                onDisplayRequested: mode => root.display = mode
                onToolRequested: tool => root.tool = tool
                flyout: root.flyout
                chosenCount: root.chosenBodies.length
                onCombineRequested: way => root.combine(way)
                onFlyoutCloseRequested: root.flyout = ""
                onGroupRequested: group => {
                    if (group === "shapes") {
                        root.flyout = root.flyout === "shapes" ? "" : "shapes"
                    } else if (group === "combine") {
                        root.flyout = root.flyout === "combine" ? "" : "combine"
                    } else {
                        root.flyout = ""
                        root.showHint(qsTr("This tool group arrives in a later version."))
                    }
                }
                onViewRequested: view => viewport.setStandardView(view)
                onDirectionRequested: (x, y, z) => viewport.setViewDirection(x, y, z)
                onFocusToggled: root.focusMode = !root.focusMode
                onNavigationHint: text => root.showHint(text)
                onOrbitDragged: (dx, dy) => viewport.orbitBy(dx, dy)
                onPanDragged: (dx, dy) => viewport.panBy(dx, dy)
                onZoomRequested: factor => viewport.zoomBy(factor)
            }

            HistoryPanel {
                x: 100
                y: 60
                visible: root.historyOpen
                entries: root.historyRows
                position: DocumentModel.historyPosition
                onGoToRequested: position => DocumentModel.goToHistory(position)
                onCloseRequested: root.historyOpen = false
            }
        }

        RightPanel {
            id: rightPanel

            x: parent.width - root.rightWidth
            width: root.rightWidth
            height: parent.height
            visible: root.rightWidth > 0
            tab: root.rightTab
            printerName: root.printerTitle
            printerId: DocumentModel.printerId
            printers: DocumentModel.printers
            bedText: qsTr("%1 × %2 mm").arg(root.bedText).arg(DocumentModel.bedZ)
            nozzleText: qsTr("%1 mm").arg(DocumentModel.nozzle)
            onPrinterChosen: id => {
                DocumentModel.setPrinter(id)
                Preferences.printerId = id
                viewport.frameAll()
            }
            stepTitle: root.currentStepTitle
            mode: root.panelMode
            bodyName: root.bodyName(root.panelBodyIndex)
            renamable: root.panelBodyIndex >= 0
            partText: Selection.items.length === 1 ? root.partName(Selection.items[0].kind, Selection.items[0].bodyIndex) : ""
            bodyCount: root.chosenBodies.length
            canCombine: root.chosenBodies.length >= 2
            bodyColor: root.chosenColor
            onColorChosen: color => DocumentModel.setBodiesColor(root.colorTargets, color)
            onNameCommitted: name => DocumentModel.setBodyName(root.panelBodyIndex, name)
            onCombineRequested: way => root.combine(way)
            stepValues: root.stepValueList
            isHole: root.currentStepIsHole
            stepTexts: DocumentModel.stepTexts
            fonts: DocumentModel.fontFamilies
            fontStyles: DocumentModel.fontStyles(root.currentFontName)
            onTextCommitted: (name, text) => root.changeWords(name, text)
            onTabRequested: tab => root.rightTab = tab
            onAxisChosen: axis => DocumentModel.setSplitAxis(DocumentModel.currentStep, axis)
            onValueCommitted: (name, text) => DocumentModel.setStepValue(DocumentModel.currentStep, name, text)
            onDragStarted: name => DocumentModel.beginChange(name)
            onDragFinished: accepted => accepted ? DocumentModel.commitChange() : DocumentModel.cancelChange()
        }
    }

    StatusBar {
        id: statusBar

        anchors.bottom: parent.bottom
        width: parent.width
        selection: root.selectionText
        toolName: root.toolName(root.tool)
        hint: root.currentHint
        unitText: qsTr("Unit: mm")
        gridStep: viewport.snapStep
        gridAutomatic: viewport.fixedSnapStep === 0
        onGridStepChosen: step => {
            viewport.fixedSnapStep = step
            Preferences.snapStep = step
        }
        onGridAutomaticChosen: {
            viewport.fixedSnapStep = 0
            Preferences.snapStep = 0
        }
    }

    AboutWindow {
        id: aboutWindow
    }

    // --- the document as the interface shows it (texts are composed here, see DocumentModel) ---
    ShapeCatalog {
        id: shapeCatalog
    }
    } // shell

    function typeLabel(type) {
        if (type === "combine") { return qsTr("Combine") }
        if (type === "split") { return qsTr("Split") }
        if (type === "place") { return qsTr("Placing") }
        const name = shapeCatalog.nameOf(type)
        return name !== "" ? name : qsTr("Body")
    }

    function valueLabel(name) {
        switch (name) {
        case "width": return qsTr("Width")
        case "depth": return qsTr("Depth")
        case "height": return qsTr("Height")
        case "radius": return qsTr("Radius")
        case "cornerRadius": return qsTr("Corner radius")
        case "font": return qsTr("Font")
        case "style": return qsTr("Style")
        case "topRadius": return qsTr("Top radius")
        case "tubeRadius": return qsTr("Tube radius")
        case "innerRadius": return qsTr("Inner radius")
        case "wall": return qsTr("Wall")
        case "sides": return qsTr("Sides")
        case "points": return qsTr("Points")
        case "size": return qsTr("Letter size")
        case "position": return qsTr("Position")
        default: return name
        }
    }

    function stepText(step) {
        const n = value => Numbers.format(value, 2)
        const v = step.values
        if (step.type === "box" && v.length === 3) {
            return qsTr("Box %1 × %2 × %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "cylinder" && v.length === 2) {
            return qsTr("Cylinder, radius %1 mm, height %2 mm").arg(n(v[0])).arg(n(v[1]))
        }
        if (step.type === "sphere" && v.length === 1) {
            return qsTr("Sphere, radius %1 mm").arg(n(v[0]))
        }
        if (step.type === "hemisphere" && v.length === 1) {
            return qsTr("Hemisphere, radius %1 mm").arg(n(v[0]))
        }
        if (step.type === "cone" && v.length === 3) {
            return v[1] > 0
                ? qsTr("Cone, radius %1 mm at the bottom, %2 mm at the top, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
                : qsTr("Cone, radius %1 mm, height %2 mm").arg(n(v[0])).arg(n(v[2]))
        }
        if (step.type === "torus" && v.length === 2) {
            return qsTr("Torus, ring radius %1 mm, tube radius %2 mm").arg(n(v[0])).arg(n(v[1]))
        }
        if (step.type === "tube" && v.length === 3) {
            return qsTr("Tube, radius %1 mm, wall %2 mm, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "prism" && v.length === 3) {
            return qsTr("Prism with %1 sides, radius %2 mm, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "pyramid" && v.length === 3) {
            return qsTr("Pyramid %1 × %2 × %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "wedge" && v.length === 3) {
            return qsTr("Wedge %1 × %2 × %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "star" && v.length === 3) {
            return qsTr("Star with %1 points, radius %2 mm, height %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "text" && v.length === 2) {
            const style = step.texts.style
            return style !== undefined && style !== "regular"
                ? qsTr("Text “%1” (%2), letters %3 mm, height %4 mm").arg(step.texts.text).arg(styleLabel(style)).arg(n(v[0])).arg(n(v[1]))
                : qsTr("Text “%1”, letters %2 mm, height %3 mm").arg(step.texts.text).arg(n(v[0])).arg(n(v[1]))
        }
        if (step.type === "split" && v.length === 2) {
            return qsTr("Split across %1 at %2 mm").arg(["X", "Y", "Z"][Math.round(v[0])]).arg(n(v[1]))
        }
        if (step.type === "place" && v.length === 3) {
            return qsTr("Part placed, moved by %1, %2, %3 mm").arg(n(v[0])).arg(n(v[1])).arg(n(v[2]))
        }
        if (step.type === "combine" && v.length === 1) {
            switch (step.texts.mode) {
            case "subtract": return qsTr("Cut out %n body(ies)", "", v[0])
            case "intersect": return qsTr("Overlap kept with %n body(ies)", "", v[0])
            default: return qsTr("Merged with %n body(ies)", "", v[0])
            }
        }
        // A kind of step this version does not know (a file from a later version).
        if (shapeCatalog.nameOf(step.type) === "" && ["combine", "split", "place"].indexOf(step.type) < 0) {
            return qsTr("Step %1 (unknown kind)").arg(step.index + 1)
        }
        return typeLabel(step.type)
    }

    // One short sentence for a step of the history ("Width changed to 35").
    function historyText(step) {
        if (!step || !step.key) {
            return ""
        }
        const body = step.name !== undefined && step.name !== "" ? step.name : typeLabel(step.type)
        switch (step.key) {
        case "addFeature": return qsTr("%1 added").arg(typeLabel(step.type))
        case "removeFeature": return qsTr("%1 removed").arg(typeLabel(step.type))
        case "setValue":
            return step.value !== undefined
                ? qsTr("%1 changed to %2").arg(valueLabel(step.field)).arg(step.value)
                : qsTr("%1 changed").arg(valueLabel(step.field))
        case "setText": return qsTr("Text of %1 changed").arg(body)
        case "cutTargets": return qsTr("What %1 cuts out changed").arg(body)
        case "holeBody": return qsTr("%1 made a hole").arg(body)
        case "moveBody": return qsTr("%1 moved").arg(body)
        case "rotateBody": return qsTr("%1 turned").arg(body)
        case "dropBody": return step.count === "1" ? qsTr("%1 dropped to the bed").arg(body) : qsTr("Bodies dropped to the bed")
        case "alignBodies": return qsTr("Bodies aligned")
        case "seatBody": return qsTr("%1 laid on a face").arg(body)
        case "splitBody": return qsTr("%1 split in two").arg(body)
        case "combineBodies":
            switch (step.mode) {
            case "subtract": return qsTr("Cut out %n body(ies)", "", Number(step.count) - 1)
            case "intersect": return qsTr("Overlap of %n body(ies) kept", "", Number(step.count))
            default: return qsTr("%n body(ies) merged", "", Number(step.count))
            }
        case "pasteBodies": return step.count === "1" ? qsTr("%1 pasted").arg(typeLabel(step.type)) : qsTr("Bodies pasted")
        case "scaleBody":
            return Number(step.value) < 100
                ? qsTr("%1 made smaller to %2 %").arg(body).arg(Numbers.format(Number(step.value), 1))
                : qsTr("%1 made larger to %2 %").arg(body).arg(Numbers.format(Number(step.value), 1))
        case "solidBody": return qsTr("%1 made a part again").arg(body)
        case "setLabel": return qsTr("Step renamed")
        case "setRollback": return step.count === "" ? qsTr("Time marker put back to now") : qsTr("Time marker moved")
        case "suppress": return qsTr("%1 step hidden").arg(typeLabel(step.type))
        case "unsuppress": return qsTr("%1 step shown").arg(typeLabel(step.type))
        case "nameBody": return qsTr("Body renamed to “%1”").arg(step.value)
        case "colorBody": return step.count !== undefined ? qsTr("Colour of %n body(ies) changed", "", Number(step.count)) : qsTr("Colour of %1 changed").arg(body)
        case "showBody": return qsTr("%1 shown").arg(body)
        case "hideBody": return qsTr("%1 hidden").arg(body)
        case "addMeasure": return qsTr("Named measure “%1” added").arg(step.name)
        case "removeMeasure": return qsTr("Named measure “%1” removed").arg(step.name)
        case "setMeasure": return qsTr("Named measure “%1” changed to %2").arg(step.name).arg(step.value)
        case "renameMeasure": return qsTr("Named measure “%1” renamed to “%2”").arg(step.name).arg(step.value)
        default: return qsTr("Several changes")
        }
    }

    readonly property var historyRows: DocumentModel.history.map(step => ({
        text: historyText(step),
        done: step.done
    }))

    // One short sentence for what went wrong (the full message catalogue comes later).
    MessageCatalog {
        id: messageCatalog

        valueLabel: root.valueLabel
        styleLabel: root.styleLabel
    }

    function problemText(problem, step) {
        return messageCatalog.text(problem, step)
    }

    function recipeIcon(type) {
        switch (type) {
        case "combine": return "combine"
        case "split": return "scissors"
        case "place": return "move"
        case "text": return "text"
        default: return "cube"
        }
    }

    readonly property var recipeList: DocumentModel.recipe.map(step => ({
        text: stepText(step),
        icon: recipeIcon(step.type),
        label: step.label,
        state: step.state,
        problem: step.problem ? problemText(step.problem, step) : "",
        problemActions: step.problem ? messageCatalog.actionsFor(step.problem) : []
    }))

    readonly property var chosenBodyIndexes: Selection.items.map(item => item.bodyIndex)
    // The ids of the chosen bodies (once each, also when faces of a body are chosen).
    readonly property var chosenBodies: Array.from(new Set(Selection.items.map(item => item.body)))
    // Every chosen body is a hole: "As hole" in the menu is ticked and a click turns it off.
    readonly property bool chosenAreHoles: chosenBodyIndexes.length > 0
        && chosenBodyIndexes.every(index => index >= 0 && index < DocumentModel.bodies.length && DocumentModel.bodies[index].hole)

    // The body of the chosen step is a hole (its panel offers "goes all the way through").
    readonly property bool currentStepIsHole: {
        const index = DocumentModel.bodies && DocumentModel.currentStep >= 0
            ? DocumentModel.bodyIndexOf(DocumentModel.bodyOfStep(DocumentModel.currentStep)) : -1
        return index >= 0 && DocumentModel.bodies[index].hole
    }

    // The cutting plane of the chosen split step (asked again when the values change).
    readonly property var splitPlaneInfo: DocumentModel.stepValues && DocumentModel.currentStep >= 0
        ? DocumentModel.splitPlane() : ({ valid: false })

    // The one chosen body, if it is a hole: the label "Cuts out: …" hangs on it.
    readonly property int chosenHoleIndex: {
        const indexes = chosenBodyIndexes
        if (indexes.length !== 1 || indexes[0] < 0 || indexes[0] >= DocumentModel.bodies.length) {
            return -1
        }
        return DocumentModel.bodies[indexes[0]].hole ? indexes[0] : -1
    }
    // Read the list of bodies so that the label is asked again when a body or a cut changes.
    readonly property var cutLabelInfo: DocumentModel.bodies && chosenHoleIndex >= 0
        ? DocumentModel.cutLabel(chosenHoleIndex) : ({ valid: false })

    readonly property var bodyList: DocumentModel.bodies.map(body => ({
        name: (body.name !== "" ? body.name : typeLabel(body.type) + " " + body.number)
              + (body.part > 0 ? qsTr(", part %1").arg(body.part) : ""),
        color: body.color,
        visible: body.visible,
        hole: body.hole,
        consumed: body.consumed,
        cut: body.cut,
        chosen: chosenBodyIndexes.indexOf(body.index) >= 0
    }))

    // --- what is chosen and what is under the pointer, in words ----------------------------
    function bodyName(index) {
        return index >= 0 && index < bodyList.length ? bodyList[index].name : ""
    }

    // "Face of Box 1": which part of which body.
    function partName(kind, bodyIndex) {
        const body = bodyName(bodyIndex)
        switch (kind) {
        case "face": return qsTr("Face of %1").arg(body)
        case "edge": return qsTr("Edge of %1").arg(body)
        case "vertex": return qsTr("Corner of %1").arg(body)
        default: return body
        }
    }

    readonly property string selectionText: {
        const items = Selection.items
        if (items.length === 0) {
            return ""
        }
        if (items.length === 1) {
            return qsTr("%1 selected").arg(partName(items[0].kind, items[0].bodyIndex))
        }
        return qsTr("%n thing(s) selected", "", items.length)
    }

    readonly property string hoverText: {
        const hover = Selection.hover
        if (hover.kind === undefined) {
            return ""
        }
        const what = partName(hover.kind, hover.bodyIndex)
        switch (hover.kind) {
        case "face": return root.tool !== "select"
            ? qsTr("%1 – click chooses it, double click the whole body, drag moves the body.").arg(what)
            : qsTr("%1 – click chooses it, double click chooses the whole body.").arg(what)
        case "body": return root.tool !== "select"
            ? qsTr("%1 – click chooses it, drag moves it.").arg(what)
            : qsTr("%1 – click chooses it.").arg(what)
        default: return qsTr("%1 – click chooses it.").arg(what)
        }
    }

    // Which set of sections the property panel shows (see RightPanel.mode).
    readonly property string panelMode: {
        if (root.chosenBodies.length > 1) {
            return "bodies"
        }
        const items = Selection.items
        if (items.length > 0 && items.every(item => item.kind !== "body") && items[0].bodyIndex >= 0) {
            return "part"
        }
        return DocumentModel.stepValues.length > 0 ? "body" : "none"
    }
    // What the context bar next to the selection is for: body | bodies | face | none.
    readonly property string contextKind: {
        const items = Selection.items
        if (items.length === 0) {
            return "none"
        }
        if (chosenBodies.length > 1) {
            return "bodies"
        }
        if (items.some(item => item.kind === "body")) {
            return "body"
        }
        return Selection.singleFaceChosen ? "face" : "none"
    }
    // The bodies a colour choice applies to (once each), and their colour if it is the same.
    readonly property var colorTargets: {
        const chosen = Array.from(new Set(chosenBodyIndexes.filter(index => index >= 0)))
        return chosen.length > 0 ? chosen : (panelBodyIndex >= 0 ? [panelBodyIndex] : [])
    }
    readonly property string chosenColor: {
        const colors = new Set(colorTargets.filter(index => index < DocumentModel.bodies.length)
                                           .map(index => DocumentModel.bodies[index].color))
        return colors.size === 1 ? Array.from(colors)[0] : ""
    }
    // The body the panel shows: the chosen one, else the one the chosen recipe step makes.
    readonly property int panelBodyIndex: {
        if (root.chosenBodies.length === 1 && chosenBodyIndexes.length > 0) {
            return chosenBodyIndexes[0]
        }
        return DocumentModel.bodies && DocumentModel.currentStep >= 0
            ? DocumentModel.bodyIndexOf(DocumentModel.bodyOfStep(DocumentModel.currentStep)) : -1
    }

    readonly property string currentStepTitle: {
        const index = DocumentModel.currentStep
        return index >= 0 && index < DocumentModel.recipe.length
               ? typeLabel(DocumentModel.recipe[index].type) : ""
    }

    readonly property var stepValueList: DocumentModel.stepValues.map(value => ({
        name: value.name,
        label: valueLabel(value.name),
        group: value.group,
        axis: value.axis,
        expression: value.expression,
        value: value.value,
        kind: value.kind,
        error: value.error,
        advanced: value.advanced
    }))
}
