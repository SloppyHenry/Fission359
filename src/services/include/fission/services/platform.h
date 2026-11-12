// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace fission::services {

/// True when the program runs in the browser (the WebAssembly build). This header is the only
/// place outside src/render/ where the platform may be branched on (AP-6.7, Festlegung 5):
/// every other code asks here. On the desktop the answer is a constant false, so the compiler
/// removes the browser-only code there.
constexpr bool isBrowser() noexcept {
#ifdef __EMSCRIPTEN__
    return true;
#else
    return false;
#endif
}

} // namespace fission::services
