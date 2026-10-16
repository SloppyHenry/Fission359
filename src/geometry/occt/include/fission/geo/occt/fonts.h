// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fission/geo/primitives.h"

namespace fission::geo::occt {

/// The fonts the kernel can letter with: the ones that ship with the program (font files handed
/// in by the application) and the ones installed on the computer.
///
/// A font is found by its family name ("IBM Plex Sans"), regular weight. A missing font is never
/// replaced by another one: the lettering reports it (rule 6).
class FontBook {
public:
    FontBook() = default;
    /// `bundledFiles` are paths of font files that come with the program. They win over system
    /// fonts of the same name, so a document looks the same on every computer.
    explicit FontBook(const std::vector<std::string>& bundledFiles);

    struct Face {
        std::string path;
        int faceId = 0;
    };

    /// Case does not matter. Nothing when the font does not exist or has no such style.
    std::optional<Face> find(const std::string& family, FontStyle style = FontStyle::regular) const;

    /// Does a font of that name exist (in any style)?
    bool hasFamily(const std::string& family) const;

    /// The styles of a font, in the order regular, bold, italic, boldItalic.
    std::vector<FontStyle> styles(const std::string& family) const;

    /// Family names to offer, sorted, without repeats: bundled and installed fonts.
    std::vector<std::string> families() const;

private:
    struct Bundled {
        std::string name;
        std::map<FontStyle, Face> faces;
    };
    std::map<std::string, Bundled> bundled_; ///< Keyed by the lower-case family name.
};

} // namespace fission::geo::occt
