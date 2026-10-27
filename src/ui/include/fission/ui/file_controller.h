// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>

#include <cstdint>
#include <functional>

#include "fission/core/document.h"

namespace fission::services {
class RecentFiles;
class RecoveryStore;
class Autosave;
} // namespace fission::services

namespace fission::ui {

class DocumentModel;

/// Save, open, new project and the list of recent files (AP-1.10c). The file format itself is
/// `fission::io`; this class connects it to the document of the interface.
///
/// Saving runs in a background thread; the document keeps being edited meanwhile and counts as
/// "saved" afterwards only if nothing changed since the save began. Opening replaces the current
/// document (several documents at once come with AP-1.11).
///
/// Problems are reported as codes through `fileProblem`; the interface turns them into sentences
/// (rule 5), and no modal dialog is needed.
class FileController : public QObject {
    Q_OBJECT
    /// Full path of the file of this project, empty for a project never saved.
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    /// The file's name without ".fission"; empty for a project never saved.
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    /// There is something not saved (the dot in the window title).
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    /// Full paths, newest first, files that are gone left out.
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)

public:
    /// `recovery` and `autosave` are optional (without them nothing is saved on the side).
    FileController(DocumentModel* model, services::RecentFiles* recent,
                   services::RecoveryStore* recovery = nullptr,
                   services::Autosave* autosave = nullptr, QObject* parent = nullptr);
    ~FileController() override;

    QString filePath() const { return m_path; }
    QString fileName() const;
    bool modified() const noexcept { return m_modified; }
    bool saving() const noexcept { return m_saving; }
    QStringList recentFiles() const;

    /// Takes a picture of the 3D view for `preview.png` of the file (PNG bytes). Called on the
    /// interface thread when a save starts; without one, files get no preview.
    using PreviewGrabber = std::function<QByteArray()>;
    void setPreviewGrabber(PreviewGrabber grabber) { m_previewGrabber = std::move(grabber); }

    /// Saves to the file of this project. False when the project has no file yet (ask for a name
    /// and use `saveTo`) or a save is already running.
    Q_INVOKABLE bool save();
    /// Saves under this name (".fission" is added when missing); the project then belongs to this
    /// file. Starts the save and returns; `saveFinished` tells how it went.
    Q_INVOKABLE bool saveTo(const QUrl& url);
    /// Opens a file in place of the current document, after saving that one if it has a file.
    /// False when it could not be opened (`fileProblem` says why); the current document stays.
    /// `openDamaged`: open although a checksum is wrong (after the person chose "Open anyway").
    Q_INVOKABLE bool openFrom(const QUrl& url, bool openDamaged = false);
    /// Like `openFrom`, for a path as text (recent files, the command line).
    Q_INVOKABLE bool openPath(const QString& path, bool openDamaged = false) {
        return openFrom(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()), openDamaged);
    }
    /// An empty project in place of the current one, after saving that one if it has a file.
    Q_INVOKABLE bool newDocument();
    /// Makes the document safe to leave behind (new project, open, quit): a project with a file
    /// is saved, waiting for the write. False when work would be lost: a project that was never
    /// saved, or a file that could not be written.
    Q_INVOKABLE bool leave();
    /// Looks for work that was rescued from an earlier session and, if there is some, tells so
    /// with `rescuedWork` (the newest first).
    Q_INVOKABLE void checkRecovery();
    /// Opens the rescued work in place of the current document (after leaving that one safely).
    Q_INVOKABLE bool restoreRescued(const QString& id);
    Q_INVOKABLE void discardRescued(const QString& id);
    /// The end of the program: nothing the person made is lost, then the session is closed.
    void finishSession();
    Q_INVOKABLE QString baseName(const QString& path) const;
    Q_INVOKABLE void forgetRecent(const QString& path);

    /// Blocks until a running save is written and its result is taken in (quit, tests).
    void waitForSave();

signals:
    void fileChanged();
    void modifiedChanged();
    void savingChanged();
    void recentFilesChanged();
    void saveFinished(bool ok);
    /// A file could not be used. `code`: cannotWrite | cannotRead | notAFissionFile |
    /// documentUnreadable | damaged (the person may open it anyway) | newerFormat (opened as far
    /// as understood) | unsavedWork (a project that was never saved stands in the way).
    void fileProblem(const QString& code, const QString& path);
    void opened(const QString& path);
    /// Work of an earlier session was saved for the person: "We saved your work from 14:32."
    /// `originalPath` is empty for a project that never had a file.
    void rescuedWork(const QString& id, const QDateTime& savedAt, const QString& originalPath);

private:
    void refreshModified();
    /// Writes the project to the rescue folder in the background (only when something is not
    /// saved and changed since the last rescue file).
    void writeRecovery();
    bool writeRecoverySync();
    void finishSave(bool ok, const QString& path, std::uint64_t stamp);
    void setPath(const QString& path);

    DocumentModel* m_model;
    services::RecentFiles* m_recent;
    PreviewGrabber m_previewGrabber;
    QThreadPool m_pool;
    QString m_path;
    QString m_created; ///< ISO time of the first save, handed on when saving over the file
    bool m_modified = false;
    bool m_saving = false;
    services::RecoveryStore* m_recovery;
    services::Autosave* m_autosave;
    std::uint64_t m_recoveryStamp = 0; ///< the state the rescue file was written from
    bool m_recovering = false;
    int m_lastPosition = 0;
};

} // namespace fission::ui
