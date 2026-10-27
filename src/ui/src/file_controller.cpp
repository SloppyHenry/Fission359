// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/file_controller.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QTemporaryDir>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "fission/core/version.h"
#include "fission/io/fission_file.h"
#include "fission/services/autosave.h"
#include "fission/services/recent_files.h"
#include "fission/services/recovery_store.h"
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

FileController::FileController(DocumentModel* model, services::RecentFiles* recent,
                               services::RecoveryStore* recovery, services::Autosave* autosave,
                               QObject* parent)
    : QObject(parent), m_model(model), m_recent(recent), m_recovery(recovery),
      m_autosave(autosave) {
    m_tabs.emplace_back();       // the document that is shown at the start
    m_pool.setMaxThreadCount(1); // saves go one after the other, in the order they were asked
    connect(model, &DocumentModel::historyChanged, this, &FileController::refreshModified);
    if (m_autosave != nullptr) {
        // Every step that was made counts; undoing and redoing are not new work.
        connect(model, &DocumentModel::historyChanged, this, [this] {
            const int position = m_model->historyPosition();
            if (position > m_lastPosition) {
                m_autosave->commandDone();
            }
            m_lastPosition = position;
            emit tabsChanged();
        });
        connect(m_autosave, &services::Autosave::due, this, &FileController::writeRecovery);
        m_autosave->start();
    }
    connect(model, &DocumentModel::paletteChanged, this, &FileController::refreshModified);
}

FileController::~FileController() {
    m_pool.waitForDone();
}

QString FileController::fileName() const {
    return cur().path.isEmpty() ? QString() : QFileInfo(cur().path).completeBaseName();
}

QStringList FileController::recentFiles() const {
    return m_recent != nullptr ? m_recent->paths() : QStringList();
}

QVariantList FileController::recentTiles() const {
    QVariantList tiles;
    for (const QString& path : recentFiles()) {
        const std::vector<std::uint8_t> png = io::readPreview(toPath(path));
        QString preview;
        if (!png.empty()) {
            const QByteArray bytes(reinterpret_cast<const char*>(png.data()),
                                   static_cast<qsizetype>(png.size()));
            preview =
                QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
        }
        tiles.push_back(QVariantMap{{QStringLiteral("path"), path},
                                    {QStringLiteral("name"), QFileInfo(path).completeBaseName()},
                                    {QStringLiteral("preview"), preview}});
    }
    return tiles;
}

QStringList FileController::exampleNames() const {
    QStringList names;
    for (const QString& file :
         QDir(QStringLiteral(":/fission/examples"))
             .entryList({QStringLiteral("*.fission")}, QDir::Files, QDir::Name)) {
        names.push_back(QFileInfo(file).completeBaseName());
    }
    return names;
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
        emit tabsChanged();
    }
}

void FileController::setPath(const QString& path) {
    if (path == cur().path) {
        return;
    }
    cur().path = path;
    emit fileChanged();
    emit tabsChanged();
}

bool FileController::save() {
    if (cur().path.isEmpty()) {
        return false;
    }
    return saveTo(QUrl::fromLocalFile(cur().path));
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
    if (cur().created.isEmpty()) {
        cur().created = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    }
    io::SaveExtras extras;
    extras.appVersion = std::string(core::versionString());
    extras.created = cur().created.toStdString();
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
    if (m_model->saveStamp() == stamp && m_recovery != nullptr) {
        m_recovery->removeSessionFiles(cur().recoverySlot); // the file itself is now the safe copy
        cur().recoveryStamp = 0;
    }
    if (m_autosave != nullptr) {
        m_autosave->acknowledge();
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
    if (cur().path.isEmpty()) {
        // No file to save into: the project is set aside as rescue work, which the next start
        // offers back ("We saved your work").
        if (m_recovery != nullptr && writeRecoverySync() &&
            m_recovery->keepSessionFiles(cur().recoverySlot)) {
            cur().recoveryStamp = 0;
            return true;
        }
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

void FileController::writeRecovery() {
    if (m_recovery == nullptr || m_recovering) {
        return;
    }
    if (!m_model->hasUnsavedChanges()) {
        m_recovery->removeSessionFiles(cur().recoverySlot); // nothing to rescue
        cur().recoveryStamp = 0;
        return;
    }
    const std::uint64_t stamp = m_model->saveStamp();
    if (stamp == cur().recoveryStamp) {
        return; // the rescue file already holds this state
    }
    core::Document snapshot = m_model->documentForSaving();
    io::SaveExtras extras;
    extras.appVersion = std::string(core::versionString());
    const int slot = cur().recoverySlot;
    const QString file = m_recovery->filePath(slot);
    const QString original = cur().path;
    m_recovering = true;
    m_pool.start([this, snapshot = std::move(snapshot), extras = std::move(extras), file, original,
                  stamp, slot] {
        const io::SaveResult result = io::save(snapshot, toPath(file), extras);
        QMetaObject::invokeMethod(
            this,
            [this, ok = result.ok, original, stamp, slot] {
                m_recovering = false;
                if (ok) {
                    m_recovery->writeInfo(original, slot);
                    cur().recoveryStamp = stamp;
                }
                if (m_autosave != nullptr) {
                    m_autosave->acknowledge();
                }
            },
            Qt::QueuedConnection);
    });
}

bool FileController::writeRecoverySync() {
    if (m_recovery == nullptr) {
        return false;
    }
    waitForSave();
    writeRecovery();
    waitForSave();
    return QFileInfo::exists(m_recovery->filePath(cur().recoverySlot));
}

void FileController::checkRecovery() {
    if (m_recovery == nullptr) {
        return;
    }
    const auto found = m_recovery->rescued();
    if (!found.empty()) {
        emit rescuedWork(found.front().id, found.front().savedAt, found.front().originalPath);
    }
}

void FileController::discardRescued(const QString& id) {
    if (m_recovery != nullptr) {
        m_recovery->discard(id);
        checkRecovery(); // there may be older ones
    }
}

} // namespace fission::ui

namespace fission::ui {

// --- tabs: several documents (AP-1.11e) ---------------------------------------------------------
// The shown document lives in the model; the others are put aside in their tab. Every tab has its
// own file, its own undo list and its own rescue file.

FileController::Tab& FileController::cur() {
    return m_tabs[static_cast<std::size_t>(m_current)];
}

const FileController::Tab& FileController::cur() const {
    return m_tabs[static_cast<std::size_t>(m_current)];
}

QVariantList FileController::tabs() const {
    QVariantList list;
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const Tab& tab = m_tabs[static_cast<std::size_t>(i)];
        const bool shown = i == m_current;
        list.push_back(QVariantMap{
            {QStringLiteral("name"),
             tab.path.isEmpty() ? QString() : QFileInfo(tab.path).completeBaseName()},
            {QStringLiteral("modified"), shown ? m_modified : tab.slot.hasUnsavedChanges()},
            {QStringLiteral("current"), shown}});
    }
    return list;
}

bool FileController::switchTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) {
        return false;
    }
    if (index == m_current) {
        return true;
    }
    waitForSave(); // a save that is running belongs to the document that is shown now
    if (m_model->hasUnsavedChanges() && m_recovery != nullptr) {
        writeRecoverySync(); // what is not saved yet is safe while the tab is put aside
    }
    Tab& incoming = m_tabs[static_cast<std::size_t>(index)];
    m_tabs[static_cast<std::size_t>(m_current)].slot = m_model->exchange(std::move(incoming.slot));
    m_current = index;
    m_lastPosition = m_model->historyPosition();
    refreshModified();
    emit fileChanged();
    emit tabsChanged();
    emit currentTabChanged();
    return true;
}

bool FileController::newDocument() {
    // A project that was never saved and not touched holds nothing of the person's: it is
    // replaced instead of piling up empty tabs.
    if (cur().path.isEmpty() && !m_model->hasUnsavedChanges()) {
        core::Document fresh;
        core::DocumentSettings settings = fresh.settings();
        settings.defaultUnitMillimetres = m_newDocumentUnit;
        fresh.setSettings(settings);
        cur().created.clear();
        cur().recoveryStamp = 0;
        m_model->replaceDocument(std::move(fresh));
        m_lastPosition = m_model->historyPosition();
        refreshModified();
        emit tabsChanged();
        return true;
    }
    Tab tab;
    tab.slot = m_model->emptySlot(m_newDocumentUnit);
    tab.recoverySlot = m_nextRecoverySlot++;
    m_tabs.push_back(std::move(tab));
    emit tabsChanged();
    return switchTab(static_cast<int>(m_tabs.size()) - 1);
}

bool FileController::closeTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) {
        return false;
    }
    if (!switchTab(index) || !leave()) {
        return false; // the work must not be lost; `leave` told why
    }
    m_model->markSaved(); // it is let go of: no rescue copy is to be made of it any more
    if (m_tabs.size() == 1) {
        // The last document: an empty one takes its place.
        cur().created.clear();
        cur().recoveryStamp = 0;
        core::Document fresh;
        core::DocumentSettings settings = fresh.settings();
        settings.defaultUnitMillimetres = m_newDocumentUnit;
        fresh.setSettings(settings);
        m_model->replaceDocument(std::move(fresh));
        setPath(QString());
        refreshModified();
        emit tabsChanged();
        return true;
    }
    const int other = index > 0 ? index - 1 : 1;
    switchTab(other);
    m_tabs.erase(m_tabs.begin() + index);
    if (index < m_current) {
        --m_current;
    }
    emit tabsChanged();
    emit currentTabChanged();
    return true;
}

bool FileController::placeDocument(core::Document document, const QString& path,
                                   const QString& created, bool changed) {
    // Opening a file that is open already shows that tab.
    if (!path.isEmpty()) {
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
            if (m_tabs[static_cast<std::size_t>(i)].path == path && !changed) {
                return switchTab(i);
            }
        }
    }
    // A project that is empty and untouched (the one of the start) is replaced; otherwise the
    // document gets a tab of its own.
    const bool pristine = cur().path.isEmpty() && !m_model->hasUnsavedChanges();
    if (!pristine) {
        Tab tab;
        tab.slot = m_model->emptySlot(m_newDocumentUnit);
        tab.recoverySlot = m_nextRecoverySlot++;
        m_tabs.push_back(std::move(tab));
        if (!switchTab(static_cast<int>(m_tabs.size()) - 1)) {
            return false;
        }
    }
    cur().created = created;
    cur().recoveryStamp = 0;
    m_model->replaceDocument(std::move(document));
    if (changed) {
        m_model->markChanged(); // it is not what the file on disk holds
    }
    setPath(path);
    m_lastPosition = m_model->historyPosition();
    refreshModified();
    emit tabsChanged();
    return true;
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
    // A file from a newer program holds things this one does not know and would leave out when
    // writing: it is opened, but saving goes to a new name, never over the original.
    const bool fromNewer = std::any_of(result.problems.begin(), result.problems.end(),
                                       [](const io::FileProblem& problem) {
                                           return problem.code == io::FileProblemCode::newerFormat;
                                       });
    if (!placeDocument(std::move(*result.document), fromNewer ? QString() : path,
                       fromNewer ? QString() : QString::fromStdString(result.info.created),
                       false)) {
        return false;
    }
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

bool FileController::openExample(const QString& name) {
    // The io layer reads files, not program resources: the example goes to a temporary file.
    QFile resource(QStringLiteral(":/fission/examples/%1.fission").arg(name));
    QTemporaryDir folder;
    if (!resource.open(QIODevice::ReadOnly) || !folder.isValid()) {
        emit fileProblem(QStringLiteral("cannotRead"), name);
        return false;
    }
    const QString copy = folder.filePath(QStringLiteral("example.fission"));
    QFile out(copy);
    if (!out.open(QIODevice::WriteOnly) || out.write(resource.readAll()) < 0) {
        emit fileProblem(QStringLiteral("cannotRead"), name);
        return false;
    }
    out.close();
    io::OpenResult result = io::open(toPath(copy));
    if (!result.document) {
        emit fileProblem(QStringLiteral("documentUnreadable"), name);
        return false;
    }
    // A copy of its own: saving asks for a name.
    if (!placeDocument(std::move(*result.document), QString(), QString(), false)) {
        return false;
    }
    emit opened(QString());
    return true;
}

bool FileController::restoreRescued(const QString& id) {
    if (m_recovery == nullptr) {
        return false;
    }
    const auto found = m_recovery->rescued();
    const auto work = std::find_if(found.begin(), found.end(),
                                   [&](const services::RescuedWork& w) { return w.id == id; });
    if (work == found.end()) {
        return false;
    }
    io::OpenOptions options;
    options.openDamaged = true; // it is all there is; better than nothing
    io::OpenResult result = io::open(toPath(work->fissionPath), options);
    if (!result.document) {
        emit fileProblem(QStringLiteral("documentUnreadable"), work->fissionPath);
        m_recovery->discard(id);
        return false;
    }
    if (!placeDocument(std::move(*result.document), work->originalPath, QString(), true)) {
        return false;
    }
    // The rescue copy is only let go of once the work lies in this session's own rescue file.
    if (writeRecoverySync()) {
        m_recovery->discard(id);
    }
    emit opened(work->originalPath);
    checkRecovery();
    return true;
}

void FileController::finishSession() {
    if (m_autosave != nullptr) {
        m_autosave->stop();
    }
    waitForSave();
    // Every document: saved into its file, or set aside as rescue work.
    for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i) {
        const bool unsaved = i == m_current
                                 ? m_model->hasUnsavedChanges()
                                 : m_tabs[static_cast<std::size_t>(i)].slot.hasUnsavedChanges();
        if (!unsaved) {
            continue;
        }
        switchTab(i);
        if (!leave() && m_recovery != nullptr) {
            // The file could not be written: the work goes to the rescue folder instead.
            if (writeRecoverySync()) {
                m_recovery->keepSessionFiles(cur().recoverySlot);
            }
        }
    }
    if (m_recovery != nullptr) {
        m_recovery->endSession();
    }
}

} // namespace fission::ui
