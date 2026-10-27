// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/recent_files.h"

#include <QFileInfo>

namespace fission::services {

namespace {
const QString key = QStringLiteral("recentFiles");

QString canonical(const QString& path) {
    return QFileInfo(path).absoluteFilePath();
}
} // namespace

RecentFiles::RecentFiles(const QString& settingsFile)
    : settings_(settingsFile.isEmpty()
                    ? std::make_unique<QSettings>(QSettings::IniFormat, QSettings::UserScope,
                                                  QStringLiteral("Fission359"),
                                                  QStringLiteral("Fission359"))
                    : std::make_unique<QSettings>(settingsFile, QSettings::IniFormat)) {}

QStringList RecentFiles::stored() const {
    return settings_->value(key).toStringList();
}

void RecentFiles::store(const QStringList& paths) {
    settings_->setValue(key, paths);
    settings_->sync();
}

QStringList RecentFiles::paths() const {
    QStringList existing;
    for (const QString& path : stored()) {
        if (QFileInfo::exists(path)) {
            existing.push_back(path);
        }
    }
    return existing;
}

void RecentFiles::add(const QString& path) {
    const QString entry = canonical(path);
    QStringList list = stored();
    list.removeAll(entry);
    list.prepend(entry);
    while (list.size() > maxCount) {
        list.removeLast();
    }
    store(list);
}

void RecentFiles::remove(const QString& path) {
    QStringList list = stored();
    list.removeAll(canonical(path));
    store(list);
}

void RecentFiles::clear() {
    store({});
}

} // namespace fission::services
