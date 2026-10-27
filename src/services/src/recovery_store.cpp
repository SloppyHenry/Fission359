// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/recovery_store.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <unistd.h>

namespace fission::services {

namespace {

bool processAlive(qint64 pid) {
    if (pid <= 0) {
        return false;
    }
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
}

} // namespace

RecoveryStore::RecoveryStore(const QString& folder)
    : folder_(folder.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
                                     QStringLiteral("/recovery")
                               : folder),
      session_(newId()) {}

QString RecoveryStore::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString RecoveryStore::pathOf(const QString& id) const {
    return folder_ + QLatin1Char('/') + id + QStringLiteral(".fission");
}

QString RecoveryStore::infoPathOf(const QString& id) const {
    return folder_ + QLatin1Char('/') + id + QStringLiteral(".json");
}

QString RecoveryStore::markerPathOf(const QString& id) const {
    return folder_ + QLatin1Char('/') + id + QStringLiteral(".running");
}

void RecoveryStore::beginSession() {
    QDir().mkpath(folder_);
    QFile marker(markerPathOf(session_));
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        marker.write(QByteArray::number(QCoreApplication::applicationPid()));
    }
}

void RecoveryStore::endSession() {
    QFile::remove(markerPathOf(session_));
    removeSessionFiles();
}

void RecoveryStore::removeSessionFiles() {
    QFile::remove(pathOf(session_));
    QFile::remove(infoPathOf(session_));
}

void RecoveryStore::writeInfo(const QString& originalPath) {
    QSaveFile info(infoPathOf(session_));
    if (info.open(QIODevice::WriteOnly)) {
        const QJsonObject object{
            {QStringLiteral("originalPath"), originalPath},
            {QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
        info.write(QJsonDocument(object).toJson());
        info.commit();
    }
    trim();
}

bool RecoveryStore::keepSessionFiles() {
    if (!QFileInfo::exists(pathOf(session_)) || !QFileInfo::exists(infoPathOf(session_))) {
        return false;
    }
    const QString kept = newId();
    const bool moved = QFile::rename(pathOf(session_), pathOf(kept)) &&
                       QFile::rename(infoPathOf(session_), infoPathOf(kept));
    if (!moved) {
        return false;
    }
    return true;
}

std::vector<RescuedWork> RecoveryStore::rescued() const {
    std::vector<RescuedWork> found;
    const QDir dir(folder_);
    for (const QFileInfo& entry : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        const QString id = entry.completeBaseName();
        if (id == session_ || !QFileInfo::exists(pathOf(id))) {
            continue;
        }
        // Another program that is still running owns its files.
        QFile marker(markerPathOf(id));
        if (marker.open(QIODevice::ReadOnly) &&
            processAlive(marker.readAll().trimmed().toLongLong())) {
            continue;
        }
        QFile info(entry.filePath());
        if (!info.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonObject object = QJsonDocument::fromJson(info.readAll()).object();
        RescuedWork work;
        work.id = id;
        work.fissionPath = pathOf(id);
        work.originalPath = object.value(QStringLiteral("originalPath")).toString();
        work.savedAt =
            QDateTime::fromString(object.value(QStringLiteral("savedAt")).toString(), Qt::ISODate)
                .toLocalTime();
        if (!work.savedAt.isValid()) {
            work.savedAt = entry.lastModified();
        }
        found.push_back(std::move(work));
    }
    std::sort(found.begin(), found.end(),
              [](const RescuedWork& a, const RescuedWork& b) { return a.savedAt > b.savedAt; });
    return found;
}

void RecoveryStore::discard(const QString& id) {
    QFile::remove(pathOf(id));
    QFile::remove(infoPathOf(id));
    QFile::remove(markerPathOf(id)); // a stale marker of a session that died
}

void RecoveryStore::trim() {
    std::vector<RescuedWork> all = rescued();
    // This session's file counts as well.
    const QFileInfo own(infoPathOf(session_));
    if (own.exists()) {
        RescuedWork mine;
        mine.id = session_;
        mine.savedAt = own.lastModified();
        all.push_back(mine);
        std::sort(all.begin(), all.end(),
                  [](const RescuedWork& a, const RescuedWork& b) { return a.savedAt > b.savedAt; });
    }
    for (std::size_t i = maxFiles; i < all.size(); ++i) {
        if (all[i].id != session_) {
            discard(all[i].id);
        }
    }
}

} // namespace fission::services
