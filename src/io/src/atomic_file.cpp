// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atomic_file.h"

#include <algorithm>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>

#include "fission/core/id.h"

namespace fission::io {

namespace {

using Bytes = std::vector<std::uint8_t>;

/// Makes the bytes of an open file really reach the disk (on macOS plain fsync does not flush
/// the drive's cache).
void flushToDisk(int descriptor) {
#ifdef __APPLE__
    if (fcntl(descriptor, F_FULLFSYNC) == 0) {
        return;
    }
#endif
    fsync(descriptor);
}

void flushFolder(const std::filesystem::path& folder) {
    const int descriptor = ::open(folder.c_str(), O_RDONLY);
    if (descriptor >= 0) {
        fsync(descriptor);
        ::close(descriptor);
    }
}

bool writeAll(int descriptor, const Bytes& data, std::size_t limit) {
    std::size_t done = 0;
    const std::size_t total = std::min(data.size(), limit);
    while (done < total) {
        const ssize_t written = ::write(descriptor, data.data() + done, total - done);
        if (written <= 0) {
            return false;
        }
        done += static_cast<std::size_t>(written);
    }
    return total == data.size();
}

} // namespace

bool writeFileAtomically(const std::filesystem::path& target, const Bytes& data,
                         std::optional<std::size_t> failAfterBytes) {
    core::IdSource ids;
    const std::filesystem::path folder =
        target.has_parent_path() ? target.parent_path() : std::filesystem::path(".");
    const std::filesystem::path temporary =
        folder / (target.filename().string() + ".tmp-" + ids.nextUuid().substr(0, 8));

    const int descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (descriptor < 0) {
        return false;
    }
    const auto abandon = [&]() {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    };
    const std::size_t limit = failAfterBytes.value_or(data.size());
    if (!writeAll(descriptor, data, limit)) {
        ::close(descriptor);
        return abandon();
    }
    flushToDisk(descriptor);
    if (::close(descriptor) != 0) {
        return abandon();
    }
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error) {
        return abandon();
    }
    flushFolder(folder);
    return true;
}

} // namespace fission::io
