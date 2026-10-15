// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

namespace fission::ui {

/// Start-up choices that QML reads (QML singleton "AppOptions"): set on the command line or by
/// the environment, later also by the settings.
class AppOptions : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool focusMode MEMBER focusMode NOTIFY changed)
    Q_PROPERTY(bool reduceMotion MEMBER reduceMotion NOTIFY changed)
    Q_PROPERTY(int openMenu MEMBER openMenu NOTIFY changed)
    Q_PROPERTY(bool showHistory MEMBER showHistory NOTIFY changed)

public:
    explicit AppOptions(QObject* parent = nullptr) : QObject(parent) {}

    bool focusMode = false;
    bool reduceMotion = false;
    int openMenu = -1; ///< developer option: open this menu (index) at start, for screenshots
    /// developer option: make a few changes, undo one, and open "Recent actions" (screenshots)
    bool showHistory = false;

signals:
    void changed();
};

} // namespace fission::ui
