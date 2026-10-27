// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

namespace fission::services {

/// Milliseconds since the operating system started this process (the user's wait begins there,
/// not at main()). Empty if the system does not tell.
std::optional<double> processAgeMilliseconds();

} // namespace fission::services
