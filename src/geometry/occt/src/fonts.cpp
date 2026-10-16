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

constexpr FontStyle kStyles[] = {FontStyle::regular, FontStyle::bold, FontStyle::italic,
                                 FontStyle::boldItalic};

Font_FontAspect aspectOf(FontStyle style) {
    switch (style) {
    case FontStyle::bold:
        return Font_FontAspect_Bold;
    case FontStyle::italic:
        return Font_FontAspect_Italic;
    case FontStyle::boldItalic:
        return Font_FontAspect_BoldItalic;
    case FontStyle::regular:
        break;
    }
    return Font_FontAspect_Regular;
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
            const std::string name = font->FontName().ToCString();
            Bundled& entry = bundled_.try_emplace(lower(name), Bundled{name, {}}).first->second;
            for (const FontStyle style : kStyles) {
                const Font_FontAspect aspect = aspectOf(style);
                if (font->HasFontAspect(aspect)) {
                    entry.faces.try_emplace(
                        style, Face{font->FontPath(aspect).ToCString(), font->FontFaceId(aspect)});
                }
            }
        }
    }
}

std::optional<FontBook::Face> FontBook::find(const std::string& family, FontStyle style) const {
    if (const auto bundled = bundled_.find(lower(family)); bundled != bundled_.end()) {
        if (const auto face = bundled->second.faces.find(style);
            face != bundled->second.faces.end()) {
            return face->second;
        }
    }
    const std::lock_guard lock(systemFontsMutex());
    try {
        Font_FontAspect aspect = Font_FontAspect_Regular;
        const occ::handle<Font_SystemFont> font = Font_FontMgr::GetInstance()->FindFont(
            TCollection_AsciiString(family.c_str()), Font_StrictLevel_Strict, aspect,
            /*theDoFailMsg=*/false);
        if (font.IsNull() || !font->HasFontAspect(aspectOf(style))) {
            return std::nullopt;
        }
        return Face{font->FontPath(aspectOf(style)).ToCString(), font->FontFaceId(aspectOf(style))};
    } catch (const Standard_Failure&) {
        return std::nullopt;
    }
}

std::vector<FontStyle> FontBook::styles(const std::string& family) const {
    std::vector<FontStyle> result;
    for (const FontStyle style : kStyles) {
        if (find(family, style)) {
            result.push_back(style);
        }
    }
    return result;
}

bool FontBook::hasFamily(const std::string& family) const {
    return !styles(family).empty();
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
