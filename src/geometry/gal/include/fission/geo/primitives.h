// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <variant>

namespace fission::geo {

// The basic shapes (AP-1.5). Every shape stands on the plane z = base.z and is centred on
// (base.x, base.y) in plan view. Lengths are mm. Which faces a shape has, and what they are
// called, is documented per shape: the roles are part of the naming contract (ADR-0003).

/// Sphere resting on the plane. Faces: surface.
struct SphereSpec {
    double radius = 10.0;
};

/// Half a sphere, the flat side down. Faces: bottom, surface.
struct HemisphereSpec {
    double radius = 10.0;
};

/// Cone or truncated cone: `radius` at the bottom, `topRadius` at the top (0 = a point).
/// Faces: bottom, side, and top when `topRadius` > 0.
struct ConeSpec {
    double radius = 10.0;
    double topRadius = 0.0;
    double height = 20.0;
};

/// Ring: `radius` is the distance from the axis to the middle of the tube, `tubeRadius` the
/// radius of the tube (must be smaller). Resting on the plane. Faces: surface.
struct TorusSpec {
    double radius = 15.0;
    double tubeRadius = 5.0;
};

/// Pipe with a round hole: outer `radius`, wall thickness `wall` (smaller than the radius).
/// Faces: bottom, top, outer, inner.
struct TubeSpec {
    double radius = 10.0;
    double wall = 2.0;
    double height = 20.0;
};

/// Regular polygon pulled up. `radius` is the distance to the corners; one flat side faces front
/// (−Y). Faces: bottom, top, side1 … sideN (side1 faces front, then counter-clockwise from above).
struct PrismSpec {
    int sides = 6;
    double radius = 10.0;
    double height = 20.0;
};

/// Four-sided pyramid, the tip above the middle. Faces: bottom, front, back, left, right.
struct PyramidSpec {
    double sizeX = 20.0;
    double sizeY = 20.0;
    double height = 20.0;
};

/// Ramp: height 0 at the front (−Y), `height` at the back. Faces: bottom, back, slope, left, right.
struct WedgeSpec {
    double sizeX = 20.0;
    double sizeY = 20.0;
    double height = 20.0;
};

/// Star pulled up, one tip points to the back (+Y). Faces: bottom, top, side1 … side2N (counter-
/// clockwise from above, side1 is the edge that follows the back tip).
struct StarSpec {
    int points = 5;
    double radius = 20.0;     ///< To the tips.
    double innerRadius = 8.0; ///< To the notches between the tips.
    double height = 5.0;
};

/// How heavy and how slanted the letters of a font are.
enum class FontStyle { regular, bold, italic, boldItalic };

/// The name of a style in documents and in the interface code: "regular", "bold", "italic",
/// "boldItalic". The words people read come from the interface.
inline const char* fontStyleName(FontStyle style) {
    switch (style) {
    case FontStyle::bold:
        return "bold";
    case FontStyle::italic:
        return "italic";
    case FontStyle::boldItalic:
        return "boldItalic";
    case FontStyle::regular:
        break;
    }
    return "regular";
}

/// The style a name stands for; nothing for a name nobody knows.
inline std::optional<FontStyle> parseFontStyle(const std::string& name) {
    for (const FontStyle style :
         {FontStyle::regular, FontStyle::bold, FontStyle::italic, FontStyle::boldItalic}) {
        if (name == fontStyleName(style)) {
            return style;
        }
    }
    return std::nullopt;
}

/// Text pulled up from the plane. `text` is UTF-8 (new lines allowed), `font` a family name,
/// `size` the height of a capital letter, `height` how far the letters stand up. The text is
/// centred on `base` (by its ink, not by the line box). Faces: for every separate piece of
/// lettering n (counted from 0) glyph<n>Top, glyph<n>Bottom and glyph<n>Side<k>.
/// Throws textEmpty, glyphsMissing (never a stand-in character), fontNotFound or
/// fontStyleMissing (never another style).
struct TextSpec {
    std::string text = "Text";
    std::string font = "IBM Plex Sans";
    double size = 10.0;
    double height = 2.0;
    FontStyle style = FontStyle::regular;
};

using PrimitiveSpec = std::variant<SphereSpec, HemisphereSpec, ConeSpec, TorusSpec, TubeSpec,
                                   PrismSpec, PyramidSpec, WedgeSpec, StarSpec, TextSpec>;

} // namespace fission::geo
