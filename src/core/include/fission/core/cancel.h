// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <memory>

namespace fission::core {

class CancelSource;

/// "Please stop": handed to long calculations, which look at it now and then and give up.
/// A default token never asks to stop. Cheap to copy; all copies see the same request.
///
/// (The standard `std::stop_token` would do, but libc++ on macOS still hides it behind an
/// experimental switch on the Xcode versions we support.)
class CancelToken {
public:
    CancelToken() = default;

    bool stop_requested() const noexcept { return flag_ != nullptr && flag_->load(); }

private:
    friend class CancelSource;
    explicit CancelToken(std::shared_ptr<std::atomic<bool>> flag) : flag_(std::move(flag)) {}

    std::shared_ptr<std::atomic<bool>> flag_;
};

/// The one who asks. Each source has its own request; tokens made from it share it.
class CancelSource {
public:
    CancelSource() : flag_(std::make_shared<std::atomic<bool>>(false)) {}

    void request_stop() noexcept { flag_->store(true); }
    bool stop_requested() const noexcept { return flag_->load(); }
    CancelToken token() const { return CancelToken(flag_); }

private:
    std::shared_ptr<std::atomic<bool>> flag_;
};

} // namespace fission::core
