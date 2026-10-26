// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/file_controller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "fission/core/version.h"
#include "fission/io/fission_file.h"
#include "fission/services/recent_files.h"
#include "fission/ui/document_model.h"

namespace fission::ui {

namespace {

constexpr auto extension = ".fission";

std::filesystem::path toPath(const QString& path) {
    return std::filesystem::path(path.toStdString());
}

QString codeOf(io::FileProblemCode code) {
    switch (code) {
    case io::FileProblemCode::cannotWrite:
        return QStringLiteral("cannotWrite");
    case io::FileProblemCode::cannotRead:
        return QStringLiteral("cannotRead");
    case io::FileProblemCode::notAFissionFile:
        return QStringLiteral("notAFissionFile");
    case io::FileProblemCode::damaged:
        return QStringLiteral("damaged");
    case io::FileProblemCode::documentUnreadable:
        return QStringLiteral("documentUnreadable");
    case io::FileProblemCode::newerFormat:
        return QStringLiteral("newerFormat");
    }
    return QStringLiteral("cannotRead");
}

} // namespace

FileController::FileController(DocumentModel* model, services::RecentFiles* recent, QObject* parent)
    : QObject(parent), m_model(model), m_recent(recent) {
    m_pool.setMaxThreadCount(1); // saves go one after the other, in the order they were asked
    connect(model, &DocumentModel::historyChanged, this, &FileController::refreshModified);
    connect(model, &DocumentModel::paletteChanged, this, &FileController::refreshModified);
}

FileController::~FileController() {
    m_pool.waitForDone();
}

QString FileController::fileName() const {
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).completeBaseName();
}

QStringList FileController::recentFiles() const {
    return m_recent != nullptr ? m_recent->paths() : QStringList();
}

QString FileController::baseName(const QString& path) const {
    return QFileInfo(path).completeBaseName();
}

void FileController::forgetRecent(const QString& path) {
    if (m_recent != nullptr) {
        m_recent->remove(path);
        emit recentFilesChanged();
    }
}

void FileController::refreshModified() {
    const bool now = m_model->hasUnsavedChanges();
    if (now != m_modified) {
        m_modified = now;
        emit modifiedChanged();
    }
}

void FileController::setPath(const QString& path) {
    if (path == m_path) {
        return;
    }
    m_path = path;
    emit fileChanged();
}

bool FileController::save() {
    if (m_path.isEmpty()) {
        return false;
    }
    return saveTo(QUrl::fromLocalFile(m_path));
}

bool FileController::saveTo(const QUrl& url) {
    QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (path.isEmpty() || m_saving) {
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += QLatin1String(extension);
    }

    // On the interface thread: everything that needs the document and the window.
    core::Document snapshot = m_model->documentForSaving();
    const std::uint64_t stamp = m_model->saveStamp();
    if (m_created.isEmpty()) {
        m_created = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    io::SaveExtras extras;
    extras.appVersion = std::string(core::versionString());
    extras.created = m_created.toStdString();
    if (m_previewGrabber) {
        const QByteArray png = m_previewGrabber();
        extras.previewPng.assign(png.begin(), png.end());
    }

    m_saving = true;
    emit savingChanged();
    m_pool.start([this, snapshot = std::move(snapshot), extras = std::move(extras), path, stamp] {
        const io::SaveResult result = io::save(snapshot, toPath(path), extras);
        QMetaObject::invokeMethod(
            this, [this, ok = result.ok, path, stamp] { finishSave(ok, path, stamp); },
            Qt::QueuedConnection);
    });
    return true;
}

void FileController::finishSave(bool ok, const QString& path, std::uint64_t stamp) {
    m_saving = false;
    emit savingChanged();
    if (!ok) {
        emit fileProblem(QStringLiteral("cannotWrite"), path);
        emit saveFinished(false);
        return;
    }
    setPath(path);
    // Saved only if nothing changed while the file was written; otherwise the project stays
    // marked as changed and the next save takes the newer state.
    if (m_model->saveStamp() == stamp) {
        m_model->markSaved();
    }
    if (m_recent != nullptr) {
        m_recent->add(path);
        emit recentFilesChanged();
    }
    refreshModified();
    emit saveFinished(true);
}

void FileController::waitForSave() {
    m_pool.waitForDone();
    QCoreApplication::sendPostedEvents(this, 0);
}

bool FileController::leave() {
    if (!m_model->hasUnsavedChanges()) {
        return true;
    }
    if (m_path.isEmpty()) {
        emit fileProblem(QStringLiteral("unsavedWork"), QString());
        return false;
    }
    waitForSave();
    if (!save()) {
        return false;
    }
    waitForSave();
    return !m_model->hasUnsavedChanges();
}

bool FileController::openFrom(const QUrl& url, bool openDamaged) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    io::OpenOptions options;
    options.openDamaged = openDamaged;
    io::OpenResult result = io::open(toPath(path), options);

    if (!result.document) {
        const auto damaged = std::find_if(result.problems.begin(), result.problems.end(),
                                          [](const io::FileProblem& problem) {
                                              return problem.code == io::FileProblemCode::damaged;
                                          });
        // A damaged file whose document can be read is offered ("Open anyway"), everything else
        // is plainly reported.
        if (damaged != result.problems.end() && damaged->readableAnyway) {
            emit fileProblem(QStringLiteral("damaged"), path);
        } else {
            const io::FileProblemCode code = result.problems.empty()
                                                 ? io::FileProblemCode::cannotRead
                                                 : result.problems.back().code;
            emit fileProblem(codeOf(code), path);
        }
        return false;
    }
    if (!leave()) {
        return false; // the work of the current project must not be lost; `leave` told why
    }
    // A file from a newer program holds things this one does not know and would leave out when
    // writing: it is opened, but saving goes to a new name, never over the original.
    const bool fromNewer = std::any_of(result.problems.begin(), result.problems.end(),
                                       [](const io::FileProblem& problem) {
                                           return problem.code == io::FileProblemCode::newerFormat;
                                       });
    m_created = fromNewer ? QString() : QString::fromStdString(result.info.created);
    m_model->replaceDocument(std::move(*result.document));
    setPath(fromNewer ? QString() : path);
    refreshModified();
    if (m_recent != nullptr && !fromNewer) {
        m_recent->add(path);
        emit recentFilesChanged();
    }
    for (const io::FileProblem& problem : result.problems) {
        if (problem.code == io::FileProblemCode::newerFormat ||
            problem.code == io::FileProblemCode::damaged) {
            emit fileProblem(codeOf(problem.code), path);
        }
    }
    emit opened(path);
    return true;
}

bool FileController::newDocument() {
    if (!leave()) {
        return false;
    }
    m_created.clear();
    m_model->replaceDocument(core::Document{});
    setPath(QString());
    refreshModified();
    return true;
}

} // namespace fission::ui
