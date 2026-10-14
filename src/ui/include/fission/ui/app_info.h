// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

namespace fission::ui {

/// Facts about the running program for the "About" window and the log (version, libraries,
/// graphics backend, start time). The app fills it in; QML only reads.
class AppInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version MEMBER m_version NOTIFY changed)
    Q_PROPERTY(QString qtVersion MEMBER m_qtVersion NOTIFY changed)
    Q_PROPERTY(QString kernelVersion MEMBER m_kernelVersion NOTIFY changed)
    Q_PROPERTY(QString graphicsBackend MEMBER m_graphicsBackend NOTIFY changed)
    Q_PROPERTY(QString logFile MEMBER m_logFile NOTIFY changed)
    Q_PROPERTY(int startupMilliseconds MEMBER m_startupMilliseconds NOTIFY changed)
    Q_PROPERTY(QString license READ license CONSTANT)

public:
    explicit AppInfo(QObject* parent = nullptr) : QObject(parent) {}

    void setVersion(const QString& version) { set(m_version, version); }
    void setQtVersion(const QString& version) { set(m_qtVersion, version); }
    void setKernelVersion(const QString& version) { set(m_kernelVersion, version); }
    void setGraphicsBackend(const QString& backend) { set(m_graphicsBackend, backend); }
    void setLogFile(const QString& path) { set(m_logFile, path); }
    void setStartupMilliseconds(int milliseconds) {
        m_startupMilliseconds = milliseconds;
        emit changed();
    }
    QString license() const { return QStringLiteral("GPL-3.0-or-later"); }

signals:
    void changed();

private:
    void set(QString& member, const QString& value) {
        if (member != value) {
            member = value;
            emit changed();
        }
    }

    QString m_version;
    QString m_qtVersion;
    QString m_kernelVersion;
    QString m_graphicsBackend;
    QString m_logFile;
    int m_startupMilliseconds = -1;
};

} // namespace fission::ui
