// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/platform_info.h"

#include "fission/services/platform.h"

namespace fission::ui {

PlatformInfo::PlatformInfo(QObject* parent) : QObject(parent) {}

bool PlatformInfo::browser() const {
    return services::isBrowser();
}

} // namespace fission::ui
