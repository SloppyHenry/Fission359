// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QString>

#include <vector>

namespace fission::services {

/// Work that was saved for the person without them asking: the autosave of a session that ended
/// without a clean exit, or of a project that was left behind without a file.
struct RescuedWork {
    QString id;
    QString fissionPath;  ///< The rescued project, a normal .fission file.
    QString originalPath; ///< The file it belonged to, empty for a project never saved.
    QDateTime savedAt;
};

/// The folder for rescue files (docs: AppDataLocation/recovery) and the bookkeeping around it.
/// It knows nothing of the file format: the caller writes the `.fission` file to `filePath()`.
///
/// A session (one run of the program) owns `<id>.fission` + `<id>.json` (when it was saved, the
/// file it belongs to) and a marker `<id>.running` that holds the process number. A marker whose
/// process is gone means the program did not end cleanly; its files are rescue work. A
/// clean exit removes the marker and the files, except for work that was set aside with
/// `keepSessionFiles` (a project without a file): that stays until the person decides.
/// At most `maxFiles` rescue files are kept, the oldest go first.
class RecoveryStore {
public:
    static constexpr int maxFiles = 10;

    /// `folder` empty: AppDataLocation/recovery.
    explicit RecoveryStore(const QString& folder = {});

    QString folder() const { return folder_; }
    QString sessionId() const { return session_; }

    /// Creates the folder and the marker of this session.
    void beginSession();
    /// Removes the marker of this session. Files that were not kept are removed as well.
    void endSession();

    /// Where the rescue file of a document of this session is written. Every open document (tab)
    /// has a slot of its own: 0, 1, 2, ...
    QString filePath(int slot = 0) const { return pathOf(idOf(slot)); }
    /// Notes what the file just written belongs to; also trims the folder to `maxFiles`.
    void writeInfo(const QString& originalPath, int slot = 0);
    /// Removes the rescue file of a document (it was saved, so it is not needed).
    void removeSessionFiles(int slot = 0);
    /// Makes the rescue file of a document a keepsake that survives a clean exit (the project is
    /// being left without a file). False when there was none.
    bool keepSessionFiles(int slot = 0);

    /// Rescue work of other sessions that ended badly or left something behind, newest first.
    std::vector<RescuedWork> rescued() const;
    void discard(const QString& id);

private:
    QString idOf(int slot) const;
    QString pathOf(const QString& id) const;
    QString infoPathOf(const QString& id) const;
    QString markerPathOf(const QString& id) const;
    void trim();
    static QString newId();

    QString folder_;
    QString session_;
};

} // namespace fission::services
