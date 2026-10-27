// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string_view>

namespace fission::services {

/// Where the log files live: ~/Library/Logs/Fission359 (macOS) or $XDG_STATE_HOME/fission359
/// (Linux). The environment variable FISSION_LOG_DIR overrides it (tests).
std::filesystem::path logDirectory();

/// Starts logging to `logDirectory()/fission359.log` (1 MiB, three files in rotation) and to the
/// terminal, and sends Qt's own messages there as well. Returns the log file. Safe to call once.
std::filesystem::path startLogging(std::string_view program);

} // namespace fission::services
