// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace fission::io {

/// Writes `data` to `target` so that the target is either the old file or the complete new one,
/// never something in between: a temporary file in the same folder is written, flushed to disk
/// and renamed over the target; the folder is flushed too. On any failure the temporary file is
/// removed and the target stays as it was. `failAfterBytes` (tests only) simulates a crash after
/// that many bytes.
bool writeFileAtomically(const std::filesystem::path& target, const std::vector<std::uint8_t>& data,
                         std::optional<std::size_t> failAfterBytes = std::nullopt);

} // namespace fission::io
