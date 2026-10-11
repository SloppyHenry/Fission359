// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Intentional layer violation: code that only links fission_core must not see Qt.
// The test layering_rejects_qt_in_core expects this file to FAIL to compile.

#include <QString>

#include "fission/core/version.h"

QString violation() {
    return QString::fromUtf8(fission::core::versionString().data());
}
