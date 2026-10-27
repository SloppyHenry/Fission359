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
#include <QVariantList>

#include <cstdint>
#include <functional>
#include <vector>

#include "fission/core/document.h"
#include "fission/ui/document_model.h"

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
    /// The open documents: [{ name, modified, current }]; `name` is empty for a project never
    /// saved.
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int currentTab READ currentTab NOTIFY currentTabChanged)
    /// Full paths, newest first, files that are gone left out.
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)

public:
    /// `recovery` and `autosave` are optional (without them nothing is saved on the side).
    FileController(DocumentModel* model, services::RecentFiles* recent,
                   services::RecoveryStore* recovery = nullptr,
                   services::Autosave* autosave = nullptr, QObject* parent = nullptr);
    ~FileController() override;

    QString filePath() const { return m_tabs[static_cast<std::size_t>(m_current)].path; }
    QVariantList tabs() const;
    int currentTab() const noexcept { return m_current; }
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
    /// A new, empty project in a tab of its own.
    Q_INVOKABLE bool newDocument();
    /// Shows another open document (its undo list stays with it).
    Q_INVOKABLE bool switchTab(int index);
    /// Closes a document: a project with a file is saved first, one without is set aside as rescue
    /// work. Closing the last one leaves an empty project. False when the work could not be
    /// secured.
    Q_INVOKABLE bool closeTab(int index);
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
    /// Writes the rescue copy now and waits for it (what the autosave timer does; also for tests).
    /// False when it could not be written or there is no rescue folder.
    bool autosaveNow() { return writeRecoverySync(); }
    /// The end of the program: nothing the person made is lost, then the session is closed.
    void finishSession();
    /// What a bare number means in a new project: 1 for millimetres, 25.4 for inches (setting).
    void setNewDocumentUnit(double millimetres) { m_newDocumentUnit = millimetres; }
    /// The recent files as tiles for the start page: [{ path, name, preview }], `preview` is a
    /// picture as a data URL ("" when the file has none).
    Q_INVOKABLE QVariantList recentTiles() const;
    /// The examples that come with the program (names without ending, in a fixed order).
    Q_INVOKABLE QStringList exampleNames() const;
    /// Opens an example as a new, not yet saved project (saving asks for a name).
    Q_INVOKABLE bool openExample(const QString& name);
    Q_INVOKABLE QString baseName(const QString& path) const;
    Q_INVOKABLE void forgetRecent(const QString& path);

    /// Blocks until a running save is written and its result is taken in (quit, tests).
    void waitForSave();

signals:
    void fileChanged();
    void tabsChanged();
    void currentTabChanged();
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

    /// One open document. The shown one lives in the model (its `slot` is empty); the others are
    /// put aside in their slot.
    struct Tab {
        DocumentModel::DocumentSlot slot;
        QString path;
        QString created; ///< ISO time of the first save, handed on when saving over the file
        int recoverySlot = 0;
        std::uint64_t recoveryStamp = 0; ///< the state its rescue file was written from
    };
    Tab& cur();
    const Tab& cur() const;
    /// Puts a document on show: in place of an untouched empty one, or in a tab of its own (or
    /// shows the tab where the file is open already). `changed`: it differs from the file.
    bool placeDocument(core::Document document, const QString& path, const QString& created,
                       bool changed);

    DocumentModel* m_model;
    services::RecentFiles* m_recent;
    PreviewGrabber m_previewGrabber;
    QThreadPool m_pool;
    std::vector<Tab> m_tabs;
    int m_current = 0;
    int m_nextRecoverySlot = 1;
    bool m_modified = false;
    bool m_saving = false;
    services::RecoveryStore* m_recovery;
    services::Autosave* m_autosave;
    bool m_recovering = false;
    int m_lastPosition = 0;
    double m_newDocumentUnit = 1.0;
};

} // namespace fission::ui
