// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Lettering (AP-1.5b): size, place, naming, and the refusals (missing letters, missing fonts).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/occt/fonts.h"
#include "fission/geo/occt/solids.h"

using Catch::Approx;
using namespace fission::geo;

namespace {

// The font that ships with the tests (tests/fixtures/fonts), standing in for the bundled ones.
const occt::FontBook& testFonts() {
    static const occt::FontBook fonts(
        {std::string(FISSION_TEST_FIXTURES_DIR) + "/fonts/DejaVuSans.ttf"});
    return fonts;
}

constexpr const char* kFont = "DejaVu Sans";

NamedSolid letter(const std::string& text, double size = 10.0, double height = 2.0,
                  Point3d base = {}) {
    return occt::makeNamedText(TextSpec{text, kFont, size, height}, base, testFonts());
}

GeometryErrorCode codeOf(const std::string& text, const char* font = kFont) {
    try {
        occt::makeNamedText(TextSpec{text, font, 10.0, 2.0}, {}, testFonts());
    } catch (const GeometryError& error) {
        return error.code();
    }
    return GeometryErrorCode::kernelFailure; // not reached when the call throws
}

} // namespace

TEST_CASE("text: capital letters are as tall as the size says, the ink is centred",
          "[text][golden]") {
    const Point3d base{5.0, -3.0, 1.0};
    const NamedSolid made = letter("LENA", 12.0, 3.0, base);
    const SolidReport report = occt::measure(made.solid);

    CHECK(report.valid);
    CHECK(report.solids == 4); // four separate letters
    CHECK(report.volume > 0.0);
    // Flat capitals: the height of the lettering is the height of a capital letter.
    CHECK(report.boxMax.y - report.boxMin.y == Approx(12.0).epsilon(1e-3));
    CHECK((report.boxMin.x + report.boxMax.x) / 2.0 == Approx(base.x).margin(1e-3));
    CHECK((report.boxMin.y + report.boxMax.y) / 2.0 == Approx(base.y).margin(1e-3));
    CHECK(report.boxMin.z == Approx(1.0).margin(1e-6));
    CHECK(report.boxMax.z == Approx(4.0).margin(1e-6));
}

TEST_CASE("text: every face has a name, one letter after the other", "[text]") {
    const NamedSolid made = letter("Hi");
    const SolidReport report = occt::measure(made.solid);
    REQUIRE(static_cast<int>(made.faces.size()) == report.faces);

    std::set<std::string> roles;
    for (const NamedFace& face : made.faces) {
        roles.insert(face.role);
    }
    CHECK(roles.size() == made.faces.size()); // no name twice
    CHECK(roles.contains("glyph0Top"));
    CHECK(roles.contains("glyph0Bottom"));
    CHECK(roles.contains("glyph1Top")); // the "i" has a dot: a third piece
    CHECK(roles.contains("glyph2Top"));
}

TEST_CASE("text: the size and the height scale what they should", "[text][golden]") {
    const SolidReport small = occt::measure(letter("Ab", 10.0, 1.0).solid);
    const SolidReport tall = occt::measure(letter("Ab", 10.0, 2.0).solid);
    const SolidReport big = occt::measure(letter("Ab", 20.0, 1.0).solid);
    CHECK(tall.volume == Approx(2.0 * small.volume).epsilon(1e-6));
    CHECK(big.volume ==
          Approx(4.0 * small.volume).epsilon(1e-3)); // twice the size, four times the ink
    CHECK(big.boxMax.x - big.boxMin.x ==
          Approx(2.0 * (small.boxMax.x - small.boxMin.x)).epsilon(1e-3));
}

TEST_CASE("text: umlauts and sharp s work when the font has them", "[text]") {
    for (const char* text : {"Größe", "ÄÖÜ äöü ß", "Straße", "Café"}) {
        INFO(text);
        const SolidReport report = occt::measure(letter(text).solid);
        CHECK(report.valid);
        CHECK(report.volume > 0.0);
    }
}

TEST_CASE("text: a second line makes the lettering taller", "[text]") {
    const SolidReport one = occt::measure(letter("AB").solid);
    const SolidReport two = occt::measure(letter("AB\nCD").solid);
    CHECK(two.valid);
    CHECK(two.boxMax.y - two.boxMin.y > 1.8 * (one.boxMax.y - one.boxMin.y));
    CHECK((two.boxMin.y + two.boxMax.y) / 2.0 == Approx(0.0).margin(1e-3));
}

TEST_CASE("text: letters the font lacks are refused and named, never replaced", "[text][golden]") {
    // A font that lacks the letter does not borrow it from another font: that would put a
    // different letter in the part than the one that was typed (rule 6).
    CHECK(codeOf("Hallo 漢字") == GeometryErrorCode::glyphsMissing);
    CHECK(codeOf("🦄") == GeometryErrorCode::glyphsMissing);

    try {
        occt::makeNamedText(TextSpec{"A漢B漢字", kFont, 10.0, 2.0}, {}, testFonts());
        FAIL("the text must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::glyphsMissing);
        CHECK(error.subject() == "漢字"); // each missing letter once, in the order of the text
    }
}

TEST_CASE("text: nothing to letter, or no such font", "[text]") {
    CHECK(codeOf("") == GeometryErrorCode::textEmpty);
    CHECK(codeOf("   ") == GeometryErrorCode::textEmpty);
    CHECK(codeOf("\n \n") == GeometryErrorCode::textEmpty);
    CHECK(codeOf("Text", "Gibt Es Nicht Sans") == GeometryErrorCode::fontNotFound);

    try {
        occt::makeNamedText(TextSpec{"Text", "Gibt Es Nicht Sans", 10.0, 2.0}, {}, testFonts());
        FAIL("the font does not exist");
    } catch (const GeometryError& error) {
        CHECK(error.subject() == "Gibt Es Nicht Sans");
    }
    CHECK(codeOf("Text\xFF") == GeometryErrorCode::invalidInput); // not UTF-8
    CHECK_THROWS_AS(occt::makeNamedText(TextSpec{"A", kFont, 0.0, 2.0}, {}, testFonts()),
                    GeometryError);
    CHECK_THROWS_AS(occt::makeNamedText(TextSpec{"A", kFont, 10.0, -1.0}, {}, testFonts()),
                    GeometryError);
}

TEST_CASE("text: the font book finds bundled fonts by name, whatever the case", "[text]") {
    CHECK(testFonts().find("DejaVu Sans").has_value());
    CHECK(testFonts().find("dejavu sans").has_value());
    CHECK_FALSE(testFonts().find("Gibt Es Nicht Sans").has_value());
    const auto families = testFonts().families();
    CHECK(std::find(families.begin(), families.end(), "DejaVu Sans") != families.end());
}

TEST_CASE("text: the font that ships with the program has umlauts and sharp s but no emoji",
          "[text]") {
    // The acceptance criterion of AP-1.5: umlauts and ß work, emoji are refused with their name.
    const occt::FontBook plex({std::string(FISSION_APP_FONTS_DIR) + "/IBMPlexSans-Regular.ttf",
                               std::string(FISSION_APP_FONTS_DIR) + "/IBMPlexMono-Regular.ttf"});
    REQUIRE(plex.find("IBM Plex Sans").has_value());
    REQUIRE(plex.find("IBM Plex Mono").has_value());

    const NamedSolid good =
        occt::makeNamedText(TextSpec{"Größe ÄÖÜ äöü ß", "IBM Plex Sans", 8.0, 1.5}, {}, plex);
    CHECK(occt::measure(good.solid).valid);

    try {
        occt::makeNamedText(TextSpec{"Hallo 😀 Welt 🎄", "IBM Plex Sans", 8.0, 1.5}, {}, plex);
        FAIL("emoji must be refused");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::glyphsMissing);
        CHECK(error.subject() == "😀🎄");
    }
}

namespace {

const occt::FontBook& plexBook() {
    static const std::string dir = FISSION_APP_FONTS_DIR;
    static const occt::FontBook book(
        {dir + "/IBMPlexSans-Regular.ttf", dir + "/IBMPlexSans-Bold.ttf",
         dir + "/IBMPlexSans-Italic.ttf", dir + "/IBMPlexSans-BoldItalic.ttf",
         dir + "/IBMPlexMono-Regular.ttf"});
    return book;
}

SolidReport lettered(const std::string& text, FontStyle style, const char* font = "IBM Plex Sans") {
    TextSpec spec{text, font, 10.0, 2.0};
    spec.style = style;
    return occt::measure(occt::makeNamedText(spec, {}, plexBook()).solid);
}

} // namespace

TEST_CASE("text: the font book knows which styles a font has", "[text]") {
    using S = FontStyle;
    CHECK(plexBook().styles("IBM Plex Sans") ==
          std::vector<S>{S::regular, S::bold, S::italic, S::boldItalic});
    CHECK(plexBook().styles("ibm plex sans").size() == 4); // case does not matter
    // The mono font only came with its regular style in this book.
    CHECK(plexBook().styles("IBM Plex Mono") == std::vector<S>{S::regular});
    CHECK(plexBook().styles("Gibt Es Nicht Sans").empty());
    CHECK(plexBook().hasFamily("IBM Plex Mono"));
    CHECK_FALSE(plexBook().hasFamily("Gibt Es Nicht Sans"));
    CHECK_FALSE(plexBook().find("IBM Plex Mono", S::bold).has_value());
}

TEST_CASE("text: bold is heavier and italic is slanted, the size stays", "[text][golden]") {
    const SolidReport regular = lettered("Hallo", FontStyle::regular);
    const SolidReport bold = lettered("Hallo", FontStyle::bold);
    const SolidReport italic = lettered("Hallo", FontStyle::italic);
    const SolidReport both = lettered("Hallo", FontStyle::boldItalic);
    for (const SolidReport* report : {&regular, &bold, &italic, &both}) {
        CHECK(report->valid);
        // A capital letter is as tall as the size says in every style.
        CHECK(report->boxMax.y - report->boxMin.y ==
              Approx(regular.boxMax.y - regular.boxMin.y).epsilon(0.05));
    }
    CHECK(bold.volume > regular.volume * 1.1); // more ink in the same height
    CHECK(both.volume > italic.volume * 1.1);
    // The strokes of an italic leaning right reach further to the right than upright ones: its
    // lettering is not the same width and not the same shape.
    CHECK(italic.boxMax.x - italic.boxMin.x !=
          Approx(regular.boxMax.x - regular.boxMin.x).epsilon(1e-3));
    CHECK(italic.volume != Approx(regular.volume).epsilon(1e-3));
}

TEST_CASE("text: a style the font does not have is refused and named, never replaced",
          "[text][golden]") {
    try {
        lettered("Hallo", FontStyle::bold, "IBM Plex Mono");
        FAIL("the mono font has no bold style in this book");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::fontStyleMissing);
        CHECK(error.subject() == "IBM Plex Mono");
    }
    // A font that does not exist at all stays a different thing.
    try {
        lettered("Hallo", FontStyle::bold, "Gibt Es Nicht Sans");
        FAIL("no such font");
    } catch (const GeometryError& error) {
        CHECK(error.code() == GeometryErrorCode::fontNotFound);
    }
    // The test font has only its regular style as well.
    TextSpec spec{"A", kFont, 10.0, 2.0};
    spec.style = FontStyle::italic;
    CHECK_THROWS_AS(occt::makeNamedText(spec, {}, testFonts()), GeometryError);
}

TEST_CASE("text: style names round trip", "[text]") {
    for (const FontStyle style :
         {FontStyle::regular, FontStyle::bold, FontStyle::italic, FontStyle::boldItalic}) {
        CHECK(parseFontStyle(fontStyleName(style)) == style);
    }
    CHECK_FALSE(parseFontStyle("heavy").has_value());
}
