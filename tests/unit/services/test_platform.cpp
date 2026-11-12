// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "fission/services/platform.h"

#ifndef __EMSCRIPTEN__
static_assert(!fission::services::isBrowser(),
              "the desktop build must not report itself as the browser");
#endif

TEST_CASE("platform: the desktop is not the browser", "[services]") {
#ifndef __EMSCRIPTEN__
    CHECK_FALSE(fission::services::isBrowser());
#endif
}
