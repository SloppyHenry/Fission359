// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/io/drawing.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>
#include <variant>

#include "atomic_file.h"

namespace fission::io {

namespace {

using namespace fission::geo;

// A number in plain notation with at most 6 decimals ("12.5", "-0.001"), the same on every
// computer (no locale), without "-0".
std::string number(double value) {
    if (std::abs(value) < 5e-7) {
        value = 0.0;
    }
    char buffer[40];
    const auto result =
        std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::fixed, 6);
    std::string text(buffer, result.ptr);
    if (text.find('.') != std::string::npos) {
        while (text.back() == '0') {
            text.pop_back();
        }
        if (text.back() == '.') {
            text.pop_back();
        }
    }
    return text;
}

constexpr double degreesToRadians = 0.017453292519943295;

Point2d onCircle(const OutlineArc& arc, double angleDegrees) {
    const double angle = angleDegrees * degreesToRadians;
    return {arc.centre.x + arc.radius * std::cos(angle),
            arc.centre.y + arc.radius * std::sin(angle)};
}

struct Box {
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    void add(const Point2d& p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    void add(const Point2d& centre, double radius) {
        add({centre.x - radius, centre.y - radius});
        add({centre.x + radius, centre.y + radius});
    }
};

// The box round all shapes. An arc counts with its ends and the extreme points it passes.
Box boxOf(const Outline& outline) {
    Box box;
    for (const OutlineShape& shape : outline.shapes) {
        if (const auto* line = std::get_if<OutlineLine>(&shape)) {
            box.add(line->from);
            box.add(line->to);
        } else if (const auto* circle = std::get_if<OutlineCircle>(&shape)) {
            box.add(circle->centre, circle->radius);
        } else if (const auto* arc = std::get_if<OutlineArc>(&shape)) {
            box.add(onCircle(*arc, arc->startAngle));
            box.add(onCircle(*arc, arc->endAngle));
            for (int quarter = 0; quarter < 8; ++quarter) {
                const double angle = 90.0 * quarter;
                if (angle > arc->startAngle && angle < arc->endAngle) {
                    box.add(onCircle(*arc, angle));
                }
            }
        } else if (const auto* poly = std::get_if<OutlinePolyline>(&shape)) {
            for (const Point2d& p : poly->points) {
                box.add(p);
            }
        }
    }
    if (box.minX > box.maxX) {
        box = Box{0.0, 0.0, 0.0, 0.0};
    }
    return box;
}

std::vector<std::uint8_t> bytesOf(const std::string& text) {
    return {text.begin(), text.end()};
}

} // namespace

std::string svgText(const Outline& outline) {
    const Box box = boxOf(outline);
    const double width = box.maxX - box.minX;
    const double height = box.maxY - box.minY;
    // The drawing starts at (0, 0) in the top left; y runs down in SVG, up in the outline.
    const auto x = [&](double value) { return number(value - box.minX); };
    const auto y = [&](double value) { return number(box.maxY - value); };

    std::string svg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg += "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + number(width) + "mm\" height=\"" +
           number(height) + "mm\" viewBox=\"0 0 " + number(width) + " " + number(height) + "\">\n";
    svg += "<g fill=\"none\" stroke=\"#000000\" stroke-width=\"0.1\">\n";
    for (const OutlineShape& shape : outline.shapes) {
        if (const auto* line = std::get_if<OutlineLine>(&shape)) {
            svg += "<line x1=\"" + x(line->from.x) + "\" y1=\"" + y(line->from.y) + "\" x2=\"" +
                   x(line->to.x) + "\" y2=\"" + y(line->to.y) + "\"/>\n";
        } else if (const auto* circle = std::get_if<OutlineCircle>(&shape)) {
            svg += "<circle cx=\"" + x(circle->centre.x) + "\" cy=\"" + y(circle->centre.y) +
                   "\" r=\"" + number(circle->radius) + "\"/>\n";
        } else if (const auto* arc = std::get_if<OutlineArc>(&shape)) {
            const Point2d from = onCircle(*arc, arc->startAngle);
            const Point2d to = onCircle(*arc, arc->endAngle);
            const bool large = arc->endAngle - arc->startAngle > 180.0;
            // Counter-clockwise as drawn on the page is sweep flag 0.
            svg += "<path d=\"M " + x(from.x) + " " + y(from.y) + " A " + number(arc->radius) +
                   " " + number(arc->radius) + " 0 " + (large ? "1" : "0") + " 0 " + x(to.x) + " " +
                   y(to.y) + "\"/>\n";
        } else if (const auto* poly = std::get_if<OutlinePolyline>(&shape)) {
            svg += "<polyline points=\"";
            for (std::size_t i = 0; i < poly->points.size(); ++i) {
                svg += (i == 0 ? "" : " ") + x(poly->points[i].x) + "," + y(poly->points[i].y);
            }
            svg += "\"/>\n";
        }
    }
    svg += "</g>\n</svg>\n";
    return svg;
}

std::string dxfText(const Outline& outline) {
    std::string dxf;
    const auto pair = [&](int code, const std::string& value) {
        dxf += std::to_string(code) + "\n" + value + "\n";
    };
    const auto point = [&](int first, const Point2d& p) {
        pair(first, number(p.x));
        pair(first + 10, number(p.y));
    };
    pair(0, "SECTION");
    pair(2, "HEADER");
    pair(9, "$ACADVER");
    pair(1, "AC1009");
    pair(0, "ENDSEC");
    pair(0, "SECTION");
    pair(2, "TABLES");
    pair(0, "TABLE");
    pair(2, "LAYER");
    pair(70, "1");
    pair(0, "LAYER");
    pair(2, "0");
    pair(70, "0");
    pair(62, "7");
    pair(6, "CONTINUOUS");
    pair(0, "ENDTAB");
    pair(0, "ENDSEC");
    pair(0, "SECTION");
    pair(2, "ENTITIES");
    for (const OutlineShape& shape : outline.shapes) {
        if (const auto* line = std::get_if<OutlineLine>(&shape)) {
            pair(0, "LINE");
            pair(8, "0");
            point(10, line->from);
            point(11, line->to);
        } else if (const auto* circle = std::get_if<OutlineCircle>(&shape)) {
            pair(0, "CIRCLE");
            pair(8, "0");
            point(10, circle->centre);
            pair(40, number(circle->radius));
        } else if (const auto* arc = std::get_if<OutlineArc>(&shape)) {
            pair(0, "ARC");
            pair(8, "0");
            point(10, arc->centre);
            pair(40, number(arc->radius));
            pair(50, number(std::fmod(arc->startAngle, 360.0)));
            pair(51, number(std::fmod(arc->endAngle, 360.0)));
        } else if (const auto* poly = std::get_if<OutlinePolyline>(&shape)) {
            pair(0, "POLYLINE");
            pair(8, "0");
            pair(66, "1");
            pair(70, "0");
            for (const Point2d& p : poly->points) {
                pair(0, "VERTEX");
                pair(8, "0");
                point(10, p);
            }
            pair(0, "SEQEND");
            pair(8, "0");
        }
    }
    pair(0, "ENDSEC");
    pair(0, "EOF");
    return dxf;
}

bool exportSvg(const Outline& outline, const std::filesystem::path& path) {
    return writeFileAtomically(path, bytesOf(svgText(outline)));
}

bool exportDxf(const Outline& outline, const std::filesystem::path& path) {
    return writeFileAtomically(path, bytesOf(dxfText(outline)));
}

} // namespace fission::io
