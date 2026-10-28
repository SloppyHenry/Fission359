// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "fission/core/document.h"

namespace fission::ui {

/// The MIME type under which copied bodies lie on the system clipboard.
inline constexpr const char* clipboardMimeType = "application/x-fission359";

/// What was copied of one body: the step that makes it and what the person did to the body itself
/// (name, colour, hole).
struct ClipboardPart {
    core::FeatureDraft draft;
    std::string name;
    core::Color color;
    bool hole = false;
};

/// The copied bodies in the format of the file: the text of a `document.json` that holds just
/// these steps and bodies. Another Fission359 (or this one, later) can paste it.
std::string clipboardToJson(const std::vector<ClipboardPart>& parts);

/// Reads what `clipboardToJson` wrote; empty for anything else (never throws).
std::vector<ClipboardPart> clipboardFromJson(std::string_view text);

} // namespace fission::ui
