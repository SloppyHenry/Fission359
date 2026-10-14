// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
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
    property bool bedVisible: true
    property string transientHint: ""
    readonly property bool narrow: width < Theme.narrowBreakpoint
    readonly property bool veryNarrow: width < Theme.veryNarrowBreakpoint
    readonly property bool sidebarsShown: !focusMode && !veryNarrow
    readonly property int leftWidth: sidebarsShown ? (narrow ? Theme.leftPanelWidthNarrow : Theme.leftPanelWidth) : 0
    readonly property int rightWidth: sidebarsShown ? (narrow ? Theme.rightPanelWidthNarrow : Theme.rightPanelWidth) : 0
    readonly property string bedText: qsTr("%1 × %2").arg(viewport.bedSizeX).arg(viewport.bedSizeY)

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
            { id: "undo", text: qsTr("Undo"), shortcut: primaryKey + "Z", nativeShortcut: "Ctrl+Z", enabled: false },
            { id: "redo", text: qsTr("Redo"), shortcut: isMac ? "⇧⌘Z" : qsTr("Ctrl+Shift+Z"), nativeShortcut: "Ctrl+Shift+Z", enabled: false },
            { separator: true },
            { id: "cut", text: qsTr("Cut"), shortcut: primaryKey + "X", nativeShortcut: "Ctrl+X", enabled: false },
            { id: "copy", text: qsTr("Copy"), shortcut: primaryKey + "C", nativeShortcut: "Ctrl+C", enabled: false },
            { id: "paste", text: qsTr("Paste"), shortcut: primaryKey + "V", nativeShortcut: "Ctrl+V", enabled: false },
            { id: "delete", text: qsTr("Delete"), shortcut: isMac ? "⌫" : qsTr("Del"), enabled: false },
            { id: "selectall", text: qsTr("Select all"), shortcut: primaryKey + "A", nativeShortcut: "Ctrl+A", enabled: false },
            { separator: true },
            { id: "settings", text: qsTr("Settings…"), shortcut: primaryKey + ",", nativeShortcut: "Ctrl+,", role: "preferences" }
        ] },
        { title: qsTr("View"), items: [
            { id: "orthographic", text: qsTr("Orthographic"), shortcut: "P", checked: root.orthographic },
            { id: "bed", text: qsTr("Print bed"), checked: root.bedVisible },
            { id: "focus", text: qsTr("Focus"), shortcut: isMac ? "⇧F" : qsTr("Shift+F"), checked: root.focusMode },
            { separator: true },
            { id: "view.frame", text: qsTr("Show everything"), shortcut: "F" },
            { id: "view.home", text: qsTr("Home view"), shortcut: isMac ? "Home" : qsTr("Pos1") },
            { id: "view.front", text: qsTr("Front"), shortcut: "1" },
            { id: "view.back", text: qsTr("Back"), shortcut: "2" },
            { id: "view.left", text: qsTr("Left"), shortcut: "3" },
            { id: "view.right", text: qsTr("Right"), shortcut: "4" },
            { id: "view.top", text: qsTr("Top"), shortcut: "5" },
            { id: "view.bottom", text: qsTr("Bottom"), shortcut: "6" }
        ] },
        { title: qsTr("Body"), items: [
            { id: "hole", text: qsTr("As hole"), enabled: false },
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

    function showHint(text) {
        transientHint = text
        hintTimer.restart()
    }

    function toolName(id) {
        return { select: qsTr("Select"), move: qsTr("Move"), rotate: qsTr("Rotate"), size: qsTr("Size") }[id]
    }

    function run(id) {
        switch (id) {
        case "quit": Qt.quit(); break
        case "about": aboutWindow.show(); aboutWindow.raise(); break
        case "settings": showHint(qsTr("Settings arrive in a later version.")); break
        case "search": showHint(qsTr("Command search arrives in a later version.")); break
        case "orthographic": orthographic = !orthographic; break
        case "bed": bedVisible = !bedVisible; break
        case "focus": focusMode = !focusMode; break
        case "minimize": root.showMinimized(); break
        case "fullscreen":
            root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
            break
        case "view.frame": viewport.frameAll(); break
        default:
            if (id.indexOf("view.") === 0) {
                viewport.setStandardView(id.substring(5))
            } else {
                showHint(qsTr("This arrives in a later version."))
            }
        }
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
    Shortcut { sequence: "P"; onActivated: root.run("orthographic") }
    Shortcut { sequence: "Home"; onActivated: root.run("view.home") }
    Shortcut { sequence: "1"; onActivated: root.run("view.front") }
    Shortcut { sequence: "2"; onActivated: root.run("view.back") }
    Shortcut { sequence: "3"; onActivated: root.run("view.left") }
    Shortcut { sequence: "4"; onActivated: root.run("view.right") }
    Shortcut { sequence: "5"; onActivated: root.run("view.top") }
    Shortcut { sequence: "6"; onActivated: root.run("view.bottom") }
    Shortcut { sequence: "Ctrl+K"; onActivated: root.run("search") }
    Shortcut { sequence: "Ctrl+,"; onActivated: root.run("settings") }
    Shortcut { sequence: "Ctrl+Q"; enabled: !root.isMac; onActivated: root.run("quit") }

    // ---- layout (docs/08-gestaltung.md §1) ----------------------------------------------------
    MenuStrip {
        id: menuStrip

        anchors.top: parent.top
        width: parent.width
        menus: root.menuModel
        inWindowMenus: !root.isMac
        printerText: qsTr("My printer · %1 mm").arg(root.bedText)
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
            recipe: [{ text: qsTr("Box 20 × 20 × 20 mm"), state: "ok" }]
            onTabRequested: tab => root.leftTab = tab
            onAddRequested: root.showHint(qsTr("Shapes arrive in a later version."))
            onBodyVisibilityRequested: (index, visible) => {
                viewport.setBodyVisible(index, visible)
                root.bodyVisible = visible
            }
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
                animated: !AppOptions.reduceMotion
            }

            ViewOverlay {
                anchors.fill: parent
                orthographic: root.orthographic
                display: root.display
                tool: root.tool
                focusMode: root.focusMode
                cameraYaw: viewport.cameraYaw
                cameraPitch: viewport.cameraPitch
                onOrthographicToggled: root.orthographic = !root.orthographic
                onDisplayRequested: mode => root.display = mode
                onToolRequested: tool => root.tool = tool
                onGroupRequested: group => root.showHint(qsTr("This tool group arrives in a later version."))
                onViewRequested: view => viewport.setStandardView(view)
                onFocusToggled: root.focusMode = !root.focusMode
                onNavigationHint: text => root.showHint(text)
                onOrbitDragged: (dx, dy) => viewport.orbitBy(dx, dy)
                onPanDragged: (dx, dy) => viewport.panBy(dx, dy)
                onZoomRequested: factor => viewport.zoomBy(factor)
            }
        }

        RightPanel {
            id: rightPanel

            x: parent.width - root.rightWidth
            width: root.rightWidth
            height: parent.height
            visible: root.rightWidth > 0
            tab: root.rightTab
            printerName: qsTr("My printer")
            bedText: qsTr("%1 mm").arg(root.bedText + " × 256")
            nozzleText: qsTr("0.4 mm")
            onTabRequested: tab => root.rightTab = tab
        }
    }

    StatusBar {
        id: statusBar

        anchors.bottom: parent.bottom
        width: parent.width
        toolName: root.toolName(root.tool)
        hint: root.transientHint !== "" && hintTimer.running ? root.transientHint
            : qsTr("Drag with the right mouse button to turn the view, with the middle button to move it, scroll to zoom.")
        unitText: qsTr("Unit: mm · Grid 1 mm")
    }

    AboutWindow {
        id: aboutWindow
    }

    // the one demo body, see main.cpp
    property bool bodyVisible: true
    readonly property var bodyList: [{ name: qsTr("Box"), color: "#E9EAEC", visible: bodyVisible }]
}
