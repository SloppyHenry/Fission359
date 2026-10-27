// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/services/settings.h"

namespace fission::services {

Settings::Settings(const QString& file)
    : settings_(file.isEmpty() ? std::make_unique<QSettings>(
                                     QSettings::IniFormat, QSettings::UserScope,
                                     QStringLiteral("Fission3D"), QStringLiteral("Fission3D"))
                               : std::make_unique<QSettings>(file, QSettings::IniFormat)) {}

void Settings::reset() {
    settings_->clear();
    settings_->sync();
}

} // namespace fission::services
