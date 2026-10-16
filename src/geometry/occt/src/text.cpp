// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Lettering: text outlines from a font, pulled up into bodies (AP-1.5).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/fonts.h"
#include "fission/geo/occt/solids.h"

#include "occt_build.h"
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <Font_FTFont.hxx>
#include <Font_TextFormatter.hxx>
#include <NCollection_String.hxx>
#include <StdPrs_BRepFont.hxx>
#include <StdPrs_BRepTextBuilder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

// Decodes UTF-8; nothing for text that is not valid UTF-8.
std::optional<std::vector<char32_t>> decode(const std::string& text) {
    std::vector<char32_t> result;
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        char32_t code = lead;
        if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            code = lead & 0x07U;
        } else if (lead >= 0xE0) {
            length = 3;
            code = lead & 0x0FU;
        } else if (lead >= 0xC2) {
            length = 2;
            code = lead & 0x1FU;
        } else if (lead >= 0x80) {
            return std::nullopt;
        }
        if (lead >= 0xF5 || i + length > text.size()) {
            return std::nullopt;
        }
        for (std::size_t k = 1; k < length; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0U) != 0x80U) {
                return std::nullopt;
            }
            code = (code << 6U) | (next & 0x3FU);
        }
        result.push_back(code);
        i += length;
    }
    return result;
}

std::string encode(char32_t code) {
    std::string out;
    if (code < 0x80) {
        out += static_cast<char>(code);
    } else if (code < 0x800) {
        out += static_cast<char>(0xC0U | (code >> 6U));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else if (code < 0x10000) {
        out += static_cast<char>(0xE0U | (code >> 12U));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    } else {
        out += static_cast<char>(0xF0U | (code >> 18U));
        out += static_cast<char>(0x80U | ((code >> 12U) & 0x3FU));
        out += static_cast<char>(0x80U | ((code >> 6U) & 0x3FU));
        out += static_cast<char>(0x80U | (code & 0x3FU));
    }
    return out;
}

bool isLayoutOnly(char32_t code) {
    return code == U'\n' || code == U'\r';
}

// Height of a capital letter in a font set to the nominal size 1 em. Fonts without an "H" fall
// back to the usual 70 %.
double capHeightPerEm(StdPrs_BRepFont& font) {
    const TopoDS_Shape glyph = font.RenderGlyph(U'H');
    if (!glyph.IsNull()) {
        Bnd_Box box;
        BRepBndLib::AddOptimal(glyph, box, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
        if (!box.IsVoid()) {
            const double height = box.CornerMax().Y() - box.CornerMin().Y();
            if (height > 1e-9) {
                return height;
            }
        }
    }
    return 0.7;
}

} // namespace

NamedSolid makeNamedText(const TextSpec& spec, Point3d base, const FontBook& fonts) {
    if (!isPositiveFinite(spec.size) || !isPositiveFinite(spec.height) || !isFinite(base)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "text: size and height must be positive numbers");
    }
    const std::optional<std::vector<char32_t>> letters = decode(spec.text);
    if (!letters) {
        throw GeometryError(GeometryErrorCode::invalidInput, "text: not valid UTF-8");
    }
    if (std::none_of(letters->begin(), letters->end(),
                     [](char32_t c) { return !isLayoutOnly(c) && c != U' ' && c != U'\t'; })) {
        throw GeometryError(GeometryErrorCode::textEmpty, "text: nothing to letter");
    }
    const std::optional<FontBook::Face> face = fonts.find(spec.font, spec.style);
    if (!face) {
        // The font may exist without the style: that is a different thing to tell the person.
        if (fonts.hasFamily(spec.font)) {
            throw GeometryError(GeometryErrorCode::fontStyleMissing,
                                std::string("text: no ") + fontStyleName(spec.style) +
                                    " style in " + spec.font,
                                spec.font);
        }
        throw GeometryError(GeometryErrorCode::fontNotFound, "text: font not found: " + spec.font,
                            spec.font);
    }

    try {
        // Size the font so that capital letters are `spec.size` tall. The font's own size is the
        // height of the em square, which is not what a person means by "10 mm text".
        StdPrs_BRepFont font;
        if (!font.Init(NCollection_String(face->path.c_str()), 1.0, face->faceId)) {
            throw GeometryError(GeometryErrorCode::fontNotFound,
                                "text: font file unusable: " + face->path, spec.font);
        }
        font.FTFont()->SetUseUnicodeSubsetFallback(false); // no stand-in letters from other fonts
        const double emSize = spec.size / capHeightPerEm(font);
        if (!font.Init(NCollection_String(face->path.c_str()), emSize, face->faceId)) {
            throw GeometryError(GeometryErrorCode::fontNotFound,
                                "text: font file unusable: " + face->path, spec.font);
        }
        font.FTFont()->SetUseUnicodeSubsetFallback(false);

        // A font that lacks a letter is an error that names the letters; it never swaps them.
        std::string missing;
        std::vector<char32_t> seen;
        for (const char32_t code : *letters) {
            if (isLayoutOnly(code) || code == U'\t' ||
                std::find(seen.begin(), seen.end(), code) != seen.end()) {
                continue;
            }
            seen.push_back(code);
            if (!font.FTFont()->HasSymbol(code)) {
                missing += encode(code);
            }
        }
        if (!missing.empty()) {
            throw GeometryError(GeometryErrorCode::glyphsMissing,
                                "text: the font has no character for " + missing, missing);
        }

        occ::handle<Font_TextFormatter> formatter = new Font_TextFormatter();
        formatter->SetupAlignment(Graphic3d_HTA_CENTER, Graphic3d_VTA_TOP);
        formatter->Append(NCollection_String(spec.text.c_str()), *font.FTFont());
        formatter->Format();
        StdPrs_BRepTextBuilder builder;
        const TopoDS_Shape outline = builder.Perform(
            font, formatter,
            gp_Ax3(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0)));
        if (outline.IsNull() || !TopExp_Explorer(outline, TopAbs_FACE).More()) {
            throw GeometryError(GeometryErrorCode::textEmpty, "text: no visible outline");
        }

        // Centre the ink on the base point, then pull the letters up.
        Bnd_Box ink;
        BRepBndLib::AddOptimal(outline, ink, /*useTriangulation=*/false,
                               /*useShapeTolerance=*/false);
        gp_Trsf place;
        place.SetTranslation(gp_Vec(base.x - (ink.CornerMin().X() + ink.CornerMax().X()) / 2.0,
                                    base.y - (ink.CornerMin().Y() + ink.CornerMax().Y()) / 2.0,
                                    base.z));
        const TopoDS_Shape flat = BRepBuilderAPI_Transform(outline, place, /*copy=*/true).Shape();
        const TopoDS_Shape shape =
            BRepPrimAPI_MakePrism(flat, gp_Vec(0.0, 0.0, spec.height)).Shape();

        // Name the faces piece by piece: the n-th separate piece of lettering has glyphNTop,
        // glyphNBottom and glyphNSideK (K counts its walls).
        std::vector<std::pair<std::string, TopoDS_Shape>> roles;
        int piece = 0;
        for (TopExp_Explorer solids(shape, TopAbs_SOLID); solids.More(); solids.Next(), ++piece) {
            ShapeMap faces;
            TopExp::MapShapes(solids.Current(), TopAbs_FACE, faces);
            int wall = 0;
            for (int i = 1; i <= faces.Extent(); ++i) {
                const TopoDS_Face wallOrCap = TopoDS::Face(faces(i));
                std::string role = "glyph" + std::to_string(piece);
                if (isFlat(wallOrCap) && outwardNormal(wallOrCap).Z() > 0.5) {
                    role += "Top";
                } else if (isFlat(wallOrCap) && outwardNormal(wallOrCap).Z() < -0.5) {
                    role += "Bottom";
                } else {
                    role += "Side" + std::to_string(++wall);
                }
                roles.emplace_back(std::move(role), faces(i));
            }
        }
        return withRoles(shape, "text", roles);
    } catch (const Standard_Failure&) {
        throw GeometryError(GeometryErrorCode::kernelFailure, "text: the kernel failed");
    }
}

} // namespace fission::geo::occt
