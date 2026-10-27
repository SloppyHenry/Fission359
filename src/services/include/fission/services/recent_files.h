// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>

#include <memory>

namespace fission::services {

/// The files opened or saved last, newest first (menu "File > Recent"). Kept in the settings of
/// the user; files that are gone are left out when the list is read.
class RecentFiles {
public:
    static constexpr int maxCount = 10;

    /// `settingsFile` empty: the user's settings of the program. A path: that INI file (tests).
    explicit RecentFiles(const QString& settingsFile = {});

    /// Existing files only, newest first.
    QStringList paths() const;
    /// Puts a file at the top (once, even when it was in the list).
    void add(const QString& path);
    void remove(const QString& path);
    void clear();

private:
    QStringList stored() const;
    void store(const QStringList& paths);

    std::unique_ptr<QSettings> settings_;
};

} // namespace fission::services
