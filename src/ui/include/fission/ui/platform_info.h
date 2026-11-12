// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

namespace fission::ui {

/// The platform the program runs on, for QML (singleton "Platform"). `browser` is true only in
/// the WebAssembly build, where parts of the desktop interface are hidden (AP-6.7b3); the
/// answer itself comes from services::isBrowser(), the only allowed platform branch.
class PlatformInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool browser READ browser CONSTANT)

public:
    explicit PlatformInfo(QObject* parent = nullptr);

    bool browser() const;
};

} // namespace fission::ui
