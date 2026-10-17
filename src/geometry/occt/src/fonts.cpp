// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/fonts.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>

#include <Font_FontMgr.hxx>
#include <Font_SystemFont.hxx>
#include <NCollection_Sequence.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_AsciiString.hxx>

namespace fission::geo::occt {

namespace {

// OpenCASCADE keeps the list of installed fonts in one shared object. It is only read here, but
// it fills itself on first use, so one thread at a time.
std::mutex& systemFontsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

} // namespace

FontBook::FontBook(const std::vector<std::string>& bundledFiles) {
    const std::lock_guard lock(systemFontsMutex());
    const occ::handle<Font_FontMgr> manager = Font_FontMgr::GetInstance();
    for (const std::string& file : bundledFiles) {
        NCollection_Sequence<occ::handle<Font_SystemFont>> found;
        try {
            if (!manager->CheckFont(found, TCollection_AsciiString(file.c_str()))) {
                continue;
            }
        } catch (const Standard_Failure&) {
            continue;
        }
        for (const occ::handle<Font_SystemFont>& font : found) {
            if (!font->HasFontAspect(Font_FontAspect_Regular)) {
                continue;
            }
            const std::string name = font->FontName().ToCString();
            bundled_.try_emplace(lower(name),
                                 Bundled{name,
                                         {font->FontPath(Font_FontAspect_Regular).ToCString(),
                                          font->FontFaceId(Font_FontAspect_Regular)}});
        }
    }
}

std::optional<FontBook::Face> FontBook::find(const std::string& family) const {
    if (const auto bundled = bundled_.find(lower(family)); bundled != bundled_.end()) {
        return bundled->second.face;
    }
    const std::lock_guard lock(systemFontsMutex());
    try {
        Font_FontAspect aspect = Font_FontAspect_Regular;
        const occ::handle<Font_SystemFont> font = Font_FontMgr::GetInstance()->FindFont(
            TCollection_AsciiString(family.c_str()), Font_StrictLevel_Strict, aspect,
            /*theDoFailMsg=*/false);
        if (font.IsNull() || !font->HasFontAspect(Font_FontAspect_Regular)) {
            return std::nullopt;
        }
        return Face{font->FontPath(Font_FontAspect_Regular).ToCString(),
                    font->FontFaceId(Font_FontAspect_Regular)};
    } catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::vector<std::string> FontBook::families() const {
    std::set<std::string> names;
    for (const auto& [key, font] : bundled_) {
        names.insert(font.name);
    }
    const std::lock_guard lock(systemFontsMutex());
    try {
        for (const occ::handle<Font_SystemFont>& font :
             Font_FontMgr::GetInstance()->GetAvailableFonts()) {
            if (font->HasFontAspect(Font_FontAspect_Regular)) {
                names.insert(font->FontName().ToCString());
            }
        }
    } catch (const Standard_Failure&) {
        // The list of installed fonts is a bonus; the bundled ones are still offered.
    }
    return {names.begin(), names.end()};
}

} // namespace fission::geo::occt
