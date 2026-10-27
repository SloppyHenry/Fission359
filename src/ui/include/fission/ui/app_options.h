// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace fission::ui {

/// Start-up choices that QML reads (QML singleton "AppOptions"): set on the command line or by
/// the environment, later also by the settings.
class AppOptions : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool focusMode MEMBER focusMode NOTIFY changed)
    Q_PROPERTY(bool reduceMotion MEMBER reduceMotion NOTIFY changed)
    Q_PROPERTY(int openMenu MEMBER openMenu NOTIFY changed)
    Q_PROPERTY(bool showHistory MEMBER showHistory NOTIFY changed)
    Q_PROPERTY(bool showShapes MEMBER showShapes NOTIFY changed)
    Q_PROPERTY(QString camera MEMBER camera NOTIFY changed)
    Q_PROPERTY(QString rightTab MEMBER rightTab NOTIFY changed)
    Q_PROPERTY(bool softShadows MEMBER softShadows NOTIFY changed)
    Q_PROPERTY(bool parked MEMBER parked NOTIFY changed)
    /// The first-start assistant is to be shown (the person has not seen it yet).
    Q_PROPERTY(bool firstRun MEMBER firstRun NOTIFY changed)
    /// The start page is shown when the program starts (no file was asked for).
    Q_PROPERTY(bool startPage MEMBER startPage NOTIFY changed)

public:
    explicit AppOptions(QObject* parent = nullptr) : QObject(parent) {}

    bool focusMode = false;
    bool reduceMotion = false;
    int openMenu = -1; ///< developer option: open this menu (index) at start, for screenshots
    /// developer option: make a few changes, undo one, and open "Recent actions" (screenshots)
    bool showHistory = false;
    /// developer option: open the shape gallery at start (screenshots)
    bool showShapes = false;
    /// developer option "yaw,pitch,distance" (degrees, degrees, mm): start with this camera
    QString camera;
    /// developer option: start with this tab of the right side bar ("object" or "printer")
    QString rightTab;
    /// developer option --no-soft-shadows switches the occlusion off for comparison images
    bool softShadows = true;
    /// developer option (environment FISSION_PARK_WINDOW): the window opens far outside the screen
    /// and never takes the focus, so that the window tests on a Mac do not pop up in front of
    /// the person who is working there
    bool parked = false;
    bool firstRun = false;
    bool startPage = false;

signals:
    void changed();
};

} // namespace fission::ui
