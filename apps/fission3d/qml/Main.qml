// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import Fission3D.Ui
import Fission3D.View

Window {
    id: root

    readonly property bool isMac: Qt.platform.os === "osx"
    readonly property string primaryKey: isMac ? "⌘" : qsTr("Ctrl+")
    property string documentName: qsTr("Untitled")
    property string tool: "move"
    property string display: "color"
    property string leftTab: "bodies"
    property string rightTab: "object"
    property bool focusMode: AppOptions.focusMode
    property bool orthographic: false
    property string mouseStyle: "fission"
    property bool bedVisible: true
    property string transientHint: ""
    property string flyout: ""         // the open tool group of the palette: "" or "shapes"
    readonly property bool narrow: width < Theme.narrowBreakpoint
    readonly property bool veryNarrow: width < Theme.veryNarrowBreakpoint
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
    color: Theme.chrome
    title: qsTr("Fission3D")

    // ---- the menus: one model for the system menu bar and the in-window menus ------------
    readonly property var menuModel: [
        { title: qsTr("File"), items: [
            { id: "new", text: qsTr("New project"), shortcut: primaryKey + "N", nativeShortcut: "Ctrl+N", enabled: false },
            { id: "open", text: qsTr("Open…"), shortcut: primaryKey + "O", nativeShortcut: "Ctrl+O", enabled: false },
            { id: "save", text: qsTr("Save"), shortcut: primaryKey + "S", nativeShortcut: "Ctrl+S", enabled: false },
            { id: "saveas", text: qsTr("Save as…"), shortcut: isMac ? "⇧⌘S" : qsTr("Ctrl+Shift+S"), nativeShortcut: "Ctrl+Shift+S", enabled: false },
            { separator: true },
            { id: "export", text: qsTr("Export…"), shortcut: primaryKey + "E", nativeShortcut: "Ctrl+E", enabled: false },
            { separator: true },
            { id: "close", text: qsTr("Close"), shortcut: primaryKey + "W", nativeShortcut: "Ctrl+W", enabled: false },
            { id: "quit", text: qsTr("Quit Fission3D"), shortcut: primaryKey + "Q", nativeShortcut: "Ctrl+Q", role: "quit" }
        ] },
        { title: qsTr("Edit"), items: [
            { id: "undo", text: undoText, shortcut: primaryKey + "Z", nativeShortcut: "Ctrl+Z", enabled: root.undoAvailable },
            { id: "redo", text: redoText, shortcut: isMac ? "⇧⌘Z" : qsTr("Ctrl+Shift+Z"), nativeShortcut: "Ctrl+Shift+Z", enabled: root.redoAvailable },
            { id: "history", text: qsTr("Recent actions"), checked: root.historyOpen },
            { separator: true },
            { id: "cut", text: qsTr("Cut"), shortcut: primaryKey + "X", nativeShortcut: "Ctrl+X", enabled: false },
            { id: "copy", text: qsTr("Copy"), shortcut: primaryKey + "C", nativeShortcut: "Ctrl+C", enabled: false },
            { id: "paste", text: qsTr("Paste"), shortcut: primaryKey + "V", nativeShortcut: "Ctrl+V", enabled: false },
            { id: "delete", text: qsTr("Delete"), shortcut: isMac ? "⌫" : qsTr("Del"), enabled: false },
            { id: "selectall", text: qsTr("Select all"), shortcut: primaryKey + "A", nativeShortcut: "Ctrl+A", enabled: DocumentModel.bodies.length > 0 || root.typing },
            { id: "selectnone", text: qsTr("Select nothing"), shortcut: qsTr("Esc"), enabled: Selection.count > 0 },
            { separator: true },
            { id: "settings", text: qsTr("Settings…"), shortcut: primaryKey + ",", nativeShortcut: "Ctrl+,", role: "preferences" }
        ] },
        { title: qsTr("View"), items: [
            { id: "orthographic", text: qsTr("Orthographic"), shortcut: "P", checked: root.orthographic },
            { id: "bed", text: qsTr("Print bed"), checked: root.bedVisible },
            { id: "softshadows", text: qsTr("Soft shadows"), checked: viewport.ambientOcclusion },
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
            { id: "mouse.fission", text: qsTr("Mouse: like Fission3D"), checked: root.mouseStyle === "fission" },
            { id: "mouse.fusion", text: qsTr("Mouse: like Fusion"), checked: root.mouseStyle === "fusion" },
            { id: "mouse.tinkercad", text: qsTr("Mouse: like Tinkercad"), checked: root.mouseStyle === "tinkercad" },
            { id: "mouse.blender", text: qsTr("Mouse: like Blender"), checked: root.mouseStyle === "blender" },
            { id: "mouse.sketchup", text: qsTr("Mouse: like SketchUp"), checked: root.mouseStyle === "sketchup" }
        ] },
        { title: qsTr("Body"), items: [
            { id: "hole", text: qsTr("As hole"), checked: root.chosenAreHoles, enabled: Selection.count > 0 },
            { separator: true },
            { id: "merge", text: qsTr("Merge"), enabled: false },
            { id: "subtract", text: qsTr("Subtract"), enabled: false },
            { id: "overlap", text: qsTr("Keep overlap"), enabled: false },
            { id: "split", text: qsTr("Split"), enabled: false }
        ] },
        { title: qsTr("Window"), items: [
            { id: "minimize", text: qsTr("Minimize"), shortcut: primaryKey + "M", nativeShortcut: "Ctrl+M" },
            { id: "fullscreen", text: qsTr("Full screen"), shortcut: isMac ? "⌃⌘F" : "F11", checked: root.visibility === Window.FullScreen }
        ] },
        { title: qsTr("Help"), items: [
            { id: "search", text: qsTr("Command search"), shortcut: primaryKey + "K", nativeShortcut: "Ctrl+K" },
            { separator: true },
            { id: "about", text: qsTr("About Fission3D"), role: "about" }
        ] }
    ]

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

    // What the tool of the palette shows on the chosen body (AP-1.6, docs/08-gestaltung.md §5):
    // "Move" the gizmo and the measure bands, "Rotate" the rings, "Size" the dots for the sizes,
    // "Select" nothing.
    // A body can be dragged to move it in every tool but "Select".
    Binding { target: Handles; property: "mode"; value: root.tool === "size" ? "size" : root.tool === "move" ? "labels" : "off" }
    Binding { target: Gizmo; property: "enabled"; value: root.tool === "move" }
    Binding { target: Rotate; property: "enabled"; value: root.tool === "rotate" }

    function run(id) {
        switch (id) {
        case "quit": Qt.quit(); break
        case "about": aboutWindow.show(); aboutWindow.raise(); break
        case "settings": showHint(qsTr("Settings arrive in a later version.")); break
        case "search": showHint(qsTr("Command search arrives in a later version.")); break
        case "undo": DocumentModel.undo(); break
        case "redo": DocumentModel.redo(); break
        case "history": historyOpen = !historyOpen; break
        case "orthographic": orthographic = !orthographic; break
        case "bed": bedVisible = !bedVisible; break
        case "softshadows": viewport.ambientOcclusion = !viewport.ambientOcclusion; break
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
        case "hole": {
            const holeNow = !root.chosenAreHoles
            for (const index of root.chosenBodyIndexes) {
                DocumentModel.setBodyHole(index, holeNow)
            }
            break
        }
        default:
            if (id.indexOf("mouse.") === 0) {
                mouseStyle = id.substring(6)
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
        if (!AppOptions.softShadows) {
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
    Shortcut {
        sequence: "Escape"
        enabled: Selection.count > 0 && !root.typing && !Handles.editing && !Gizmo.editing
                 && !Rotate.editing
        onActivated: root.run("selectnone")
    }
    Shortcut { sequence: "P"; onActivated: root.run("orthographic") }
    Shortcut { sequence: "Home"; onActivated: root.run("view.home") }
    Shortcut { sequence: "1"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing; onActivated: root.run("view.front") }
    Shortcut { sequence: "2"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing; onActivated: root.run("view.back") }
    Shortcut { sequence: "3"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing; onActivated: root.run("view.left") }
    Shortcut { sequence: "4"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing; onActivated: root.run("view.right") }
    Shortcut { sequence: "5"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing; onActivated: root.run("view.top") }
    Shortcut { sequence: "6"; enabled: !Handles.armed && !Handles.editing && !Gizmo.editing && !Rotate.editing; onActivated: root.run("view.bottom") }
    // A shape that was just put down waits for its sizes: digits type them (they are the views
    // again as soon as something else is chosen). Tab or Enter start typing, too.
    Repeater {
        model: ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ",", "."]

        delegate: Item {
            id: digit

            required property string modelData

            Shortcut {
                sequence: digit.modelData
                enabled: (Handles.armed || Handles.editing || Gizmo.editing || Rotate.editing)
                         && !handleLayer.typing && !gizmoLayer.typing && !rotateLayer.typing
                         && !root.typing
                onActivated: Gizmo.editing ? gizmoLayer.startTyping(digit.modelData)
                             : Rotate.editing ? rotateLayer.startTyping(digit.modelData)
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
    // Undo and redo: on macOS the menu of the system carries them.
    Shortcut { sequence: "Ctrl+Z"; enabled: !root.isMac && root.undoAvailable; onActivated: root.run("undo") }
    Shortcut {
        sequences: ["Ctrl+Y", "Ctrl+Shift+Z"]
        enabled: !root.isMac && root.redoAvailable
        onActivated: root.run("redo")
    }

    // ---- layout (docs/08-gestaltung.md §1) ----------------------------------------------------
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

    DocTabs {
        id: docTabs

        anchors.top: menuStrip.bottom
        width: parent.width
        documentName: root.documentName
        canUndo: DocumentModel.canUndo
        canRedo: DocumentModel.canRedo
        undoTip: DocumentModel.canUndo ? qsTr("Undo: %1").arg(root.historyText(DocumentModel.nextUndo)) : ""
        redoTip: DocumentModel.canRedo ? qsTr("Redo: %1").arg(root.historyText(DocumentModel.nextRedo)) : ""
        onUndoRequested: root.run("undo")
        onRedoRequested: root.run("redo")
        onHomeRequested: root.showHint(qsTr("The start page arrives in a later version."))
        onNewDocumentRequested: root.showHint(qsTr("This arrives in a later version."))
        onCloseRequested: root.showHint(qsTr("Closing arrives together with saving."))
        onExportRequested: root.showHint(qsTr("Export arrives in a later version."))
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
            onStepRequested: index => DocumentModel.selectStep(DocumentModel.currentStep === index ? -1 : index)
            onBodyVisibilityRequested: (index, visible) => DocumentModel.setBodyVisible(index, visible)
            onBodyChosen: (index, extend) => Selection.chooseBody(index, extend)
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

            RotateLayer {
                id: rotateLayer

                anchors.fill: parent
                obstacles: viewOverlay.obstacles
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
                onFlyoutCloseRequested: root.flyout = ""
                onGroupRequested: group => {
                    if (group === "shapes") {
                        root.flyout = root.flyout === "shapes" ? "" : "shapes"
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
                viewport.frameAll()
            }
            stepTitle: root.currentStepTitle
            stepValues: root.stepValueList
            stepTexts: DocumentModel.stepTexts
            fonts: DocumentModel.fontFamilies
            fontStyles: DocumentModel.fontStyles(root.currentFontName)
            onTextCommitted: (name, text) => root.changeWords(name, text)
            onTabRequested: tab => root.rightTab = tab
            onValueCommitted: (name, text) => DocumentModel.setStepValue(DocumentModel.currentStep, name, text)
        }
    }

    StatusBar {
        id: statusBar

        anchors.bottom: parent.bottom
        width: parent.width
        selection: root.selectionText
        toolName: root.toolName(root.tool)
        hint: Placement.active ? qsTr("Let go to put the shape down here. Esc takes it back.")
            : Gizmo.editing ? qsTr("Type a distance to move exactly (Tab for the next direction), Enter accepts, Esc puts it back.")
            : Rotate.editing ? qsTr("Type an angle to turn exactly (Tab for the next ring), Enter accepts, Esc puts it back.")
            : Handles.editing ? qsTr("Enter accepts, Tab goes to the next size, Esc leaves everything as it was.")
            : Handles.armed ? qsTr("Type a size (Tab for the next one, Enter to finish) or drag the dots.")
            : root.transientHint !== "" && hintTimer.running ? root.transientHint
            : root.flyout === "shapes" ? qsTr("Drag a shape onto the print bed or onto a face, or click it. “As hole” makes it a hole.")
            : root.hoverText !== "" ? root.hoverText
            : Selection.count > 0 ? toolCatalog.find(root.tool).hint
            : root.navigationText
        unitText: qsTr("Unit: mm · Grid 1 mm")
    }

    AboutWindow {
        id: aboutWindow
    }

    // --- the document as the interface shows it (texts are composed here, see DocumentModel) ---
    ShapeCatalog {
        id: shapeCatalog
    }

    function typeLabel(type) {
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
        case "holeBody": return qsTr("%1 made a hole").arg(body)
        case "moveBody": return qsTr("%1 moved").arg(body)
        case "rotateBody": return qsTr("%1 turned").arg(body)
        case "solidBody": return qsTr("%1 made a part again").arg(body)
        case "setLabel": return qsTr("Step renamed")
        case "suppress": return qsTr("%1 step hidden").arg(typeLabel(step.type))
        case "unsuppress": return qsTr("%1 step shown").arg(typeLabel(step.type))
        case "nameBody": return qsTr("Body renamed to “%1”").arg(step.value)
        case "colorBody": return qsTr("Colour of %1 changed").arg(body)
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
    function problemText(problem, step) {
        if (!problem) {
            return ""
        }
        if (problem.detail === "unknownName") {
            return qsTr("I don’t know “%1”.").arg(problem.subject)
        }
        if (problem.detail === "brokenDependency") {
            return qsTr("“%1” has a problem itself.").arg(problem.subject)
        }
        if (problem.detail === "divisionByZero") {
            return qsTr("That divides by zero.")
        }
        if (problem.code === "textGlyphsMissing") {
            return qsTr("This font has no character for %1. Choose another font or remove it.").arg(problem.field)
        }
        if (problem.code === "fontStyleMissing") {
            return qsTr("“%1” has no %2 style. Choose another font or style.")
                .arg(problem.field).arg(styleLabel(step && step.texts ? step.texts.style : "regular"))
        }
        if (problem.code === "fontNotFound") {
            return qsTr("The font “%1” is not on this computer. Choose another font.").arg(problem.field)
        }
        if (problem.code === "parameterOutOfRange") {
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
        }
        if (problem.code === "inputFailed" || problem.code === "inputMissing") {
            return qsTr("An earlier step has a problem.")
        }
        if (problem.code === "parameterInvalid") {
            return qsTr("%1 cannot be calculated.").arg(valueLabel(problem.field))
        }
        return qsTr("This shape could not be built.")
    }

    readonly property var recipeList: DocumentModel.recipe.map(step => ({
        text: step.problem ? stepText(step) + " – " + problemText(step.problem, step) : stepText(step),
        state: step.state
    }))

    readonly property var chosenBodyIndexes: Selection.items.map(item => item.bodyIndex)
    // Every chosen body is a hole: "As hole" in the menu is ticked and a click turns it off.
    readonly property bool chosenAreHoles: chosenBodyIndexes.length > 0
        && chosenBodyIndexes.every(index => index >= 0 && index < DocumentModel.bodies.length && DocumentModel.bodies[index].hole)

    readonly property var bodyList: DocumentModel.bodies.map(body => ({
        name: body.name !== "" ? body.name : typeLabel(body.type) + " " + body.number,
        color: body.color,
        visible: body.visible,
        hole: body.hole,
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
        error: value.error
    }))
}
