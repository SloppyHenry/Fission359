// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fission/geo/named_solid.h"
#include "fission/geo/solid.h"

namespace fission::geo {

/// A colour read from or written to a file (sRGB, 0..255).
struct FileColor {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    bool operator==(const FileColor&) const = default;
};

/// The file formats the kernel can read.
enum class ImportFormat { step, iges };

/// How to read a file (AP-2.13).
struct ImportOptions {
    /// Multiplies every length after reading, to put right a file whose unit was guessed wrong
    /// (25.4 for "it was inches"). 1 = as the file says.
    double lengthFactor = 1.0;
};

/// One body found in a file.
struct ImportedBody {
    NamedSolid body;                ///< Faces named `face1`, `face2`, ... in the body's order.
    std::string name;               ///< As in the file; empty if it has none.
    std::optional<FileColor> color; ///< The colour of the body, or of its first coloured face.
    bool repaired = false;          ///< Was invalid and had to be repaired: say so.
};

struct ImportResult {
    std::vector<ImportedBody> bodies;
    int skipped = 0;       ///< Things that are not solids, or could not be repaired.
    bool assembly = false; ///< The file was an assembly; its parts were read flat.
    /// The file does not say which unit it uses, millimetres were assumed (IGES only; every
    /// STEP file states its unit).
    bool unitAssumed = false;
};

/// A body to write into a file.
struct ExportBody {
    Solid solid;
    std::string name;               ///< Empty = unnamed.
    std::optional<FileColor> color; ///< Empty = no colour.
};

} // namespace fission::geo
