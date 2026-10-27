// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QTimer>

namespace fission::services {

/// Says when it is time to save for the person: every 60 seconds and after every 20th command,
/// whichever comes first (docs/arbeitspakete AP-1.10). It only counts and times; whoever listens
/// to `due` writes the file, in the background.
class Autosave : public QObject {
    Q_OBJECT

public:
    explicit Autosave(int intervalMilliseconds = 60000, int commandsPerSave = 20,
                      QObject* parent = nullptr);

    /// The person did one more thing (a step in the history).
    void commandDone();
    /// A save was made (or is not needed): counting starts again.
    void acknowledge();
    /// While stopped nothing becomes due (no project open, shutting down).
    void stop();
    void start();
    /// Turns the autosave off or on (a setting of the person).
    void setEnabled(bool enabled);
    /// Every this many minutes (1 to 60).
    void setIntervalMinutes(int minutes);

signals:
    void due();

private:
    QTimer timer_;
    int commandsPerSave_;
    int commands_ = 0;
    bool enabled_ = true;
};

} // namespace fission::services
