// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The basic shapes beyond box and cylinder (AP-1.5): sphere, hemisphere, cone, torus, tube, prism,
// pyramid, wedge, star. All stand on the plane z, centred on (x, y), like the box.

#include <cmath>
#include <memory>

#include "fission/features/builtin.h"
#include "fission/geo/primitives.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxCorners = 64;

ParamSpec length(const char* name, const char* initial) {
    return {name, QuantityKind::length, initial};
}

// A whole number of corners in 3 … 64; anything else is "out of range".
std::optional<FeatureIssue> requireCorners(const ResolvedParams& params, const char* name) {
    const double value = valueOf(params, name);
    if (!std::isfinite(value) || value != std::round(value) || value < 3.0 || value > kMaxCorners) {
        FeatureIssue issue;
        issue.code = FeatureIssueCode::parameterOutOfRange;
        issue.subject = name;
        return issue;
    }
    return std::nullopt;
}

std::optional<FeatureIssue> outOfRange(const char* name) {
    FeatureIssue issue;
    issue.code = FeatureIssueCode::parameterOutOfRange;
    issue.subject = name;
    return issue;
}

// What all of them share: the place (x, y, z) and the way a body is made from a description of the
// shape. The derived classes say which sizes the shape has.
class ShapeType : public FeatureType {
public:
    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const final {
        const geo::Point3d base{valueOf(input.params, "x"), valueOf(input.params, "y"),
                                valueOf(input.params, "z")};
        geo::NamedSolid made = context.kernel.makePrimitive(spec(input.params), base);
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    std::vector<ParamSpec> parameters() const final {
        std::vector<ParamSpec> all = sizes();
        all.push_back(length("x", "0"));
        all.push_back(length("y", "0"));
        all.push_back(length("z", "0"));
        return all;
    }

    int version() const final { return 1; }

protected:
    virtual std::vector<ParamSpec> sizes() const = 0;
    virtual geo::PrimitiveSpec spec(const ResolvedParams& params) const = 0;
};

class SphereType final : public ShapeType {
public:
    std::string_view id() const override { return "sphere"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        return requirePositive(p, {"radius"});
    }
    Description describe(const ResolvedParams& p) const override {
        return {"sphere", {p.at("radius")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double r = valueOf(p, "radius");
        return {{"radius",
                 HandleKind::radius,
                 "radius",
                 {valueOf(p, "x") + r, valueOf(p, "y"), valueOf(p, "z") + r},
                 {1, 0, 0},
                 1.0}};
    }

protected:
    std::vector<ParamSpec> sizes() const override { return {length("radius", "10")}; }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::SphereSpec{valueOf(p, "radius")};
    }
};

class HemisphereType final : public ShapeType {
public:
    std::string_view id() const override { return "hemisphere"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        return requirePositive(p, {"radius"});
    }
    Description describe(const ResolvedParams& p) const override {
        return {"hemisphere", {p.at("radius")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        return {{"radius",
                 HandleKind::radius,
                 "radius",
                 {valueOf(p, "x") + valueOf(p, "radius"), valueOf(p, "y"), valueOf(p, "z")},
                 {1, 0, 0},
                 1.0}};
    }

protected:
    std::vector<ParamSpec> sizes() const override { return {length("radius", "10")}; }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::HemisphereSpec{valueOf(p, "radius")};
    }
};

// radius = bottom, topRadius = top (0 = a point). The top handle sits on the rim of the top; for a
// pointed cone it coincides with the height handle (the view lets the height handle win).
class ConeType final : public ShapeType {
public:
    std::string_view id() const override { return "cone"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        if (auto issue = requirePositive(p, {"radius", "height"})) {
            return issue;
        }
        const double top = valueOf(p, "topRadius");
        if (!std::isfinite(top) || top < 0.0) {
            return outOfRange("topRadius");
        }
        return std::nullopt;
    }
    Description describe(const ResolvedParams& p) const override {
        return {"cone", {p.at("radius"), p.at("topRadius"), p.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double h = valueOf(p, "height");
        return {
            {"radius",
             HandleKind::radius,
             "radius",
             {x + valueOf(p, "radius"), y, z},
             {1, 0, 0},
             1.0},
            {"topRadius",
             HandleKind::radius,
             "topRadius",
             {x + valueOf(p, "topRadius"), y, z + h},
             {1, 0, 0},
             1.0},
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {length("radius", "10"), length("topRadius", "0"), length("height", "20")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::ConeSpec{valueOf(p, "radius"), valueOf(p, "topRadius"), valueOf(p, "height")};
    }
};

// radius = from the axis to the middle of the tube.
class TorusType final : public ShapeType {
public:
    std::string_view id() const override { return "torus"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        if (auto issue = requirePositive(p, {"radius", "tubeRadius"})) {
            return issue;
        }
        return valueOf(p, "tubeRadius") < valueOf(p, "radius") ? std::nullopt
                                                               : outOfRange("tubeRadius");
    }
    Description describe(const ResolvedParams& p) const override {
        return {"torus", {p.at("radius"), p.at("tubeRadius")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double t = valueOf(p, "tubeRadius");
        return {
            {"radius",
             HandleKind::radius,
             "radius",
             {x + valueOf(p, "radius"), y, z + t},
             {1, 0, 0},
             1.0},
            // On the outside of the tube, level with its middle: dragging out thickens the tube.
            {"tubeRadius",
             HandleKind::distance,
             "tubeRadius",
             {x + valueOf(p, "radius") + t, y, z + t},
             {1, 0, 0},
             1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {length("radius", "15"), length("tubeRadius", "5")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::TorusSpec{valueOf(p, "radius"), valueOf(p, "tubeRadius")};
    }
};

// radius = outside, wall = thickness of the wall.
class TubeType final : public ShapeType {
public:
    std::string_view id() const override { return "tube"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        if (auto issue = requirePositive(p, {"radius", "wall", "height"})) {
            return issue;
        }
        return valueOf(p, "wall") < valueOf(p, "radius") ? std::nullopt : outOfRange("wall");
    }
    Description describe(const ResolvedParams& p) const override {
        return {"tube", {p.at("radius"), p.at("wall"), p.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double r = valueOf(p, "radius");
        const double h = valueOf(p, "height");
        return {
            {"radius", HandleKind::radius, "radius", {x + r, y, z + h / 2.0}, {1, 0, 0}, 1.0},
            // On the inner rim of the top: dragging towards the middle makes the wall thicker.
            {"wall",
             HandleKind::distance,
             "wall",
             {x + r - valueOf(p, "wall"), y, z + h},
             {-1, 0, 0},
             1.0},
            {"height", HandleKind::distance, "height", {x, y + r, z + h}, {0, 0, 1}, 1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {length("radius", "10"), length("wall", "2"), length("height", "20")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::TubeSpec{valueOf(p, "radius"), valueOf(p, "wall"), valueOf(p, "height")};
    }
};

class PrismType final : public ShapeType {
public:
    std::string_view id() const override { return "prism"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        if (auto issue = requireCorners(p, "sides")) {
            return issue;
        }
        return requirePositive(p, {"radius", "height"});
    }
    Description describe(const ResolvedParams& p) const override {
        return {"prism", {p.at("sides"), p.at("radius"), p.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double h = valueOf(p, "height");
        const double r = valueOf(p, "radius");
        // The corner that ends the front side on the right: dragging it outwards along its own
        // direction makes the prism wider.
        const double angle = -kPi / 2.0 + kPi / std::round(valueOf(p, "sides"));
        const double dx = std::cos(angle);
        const double dy = std::sin(angle);
        return {
            {"radius",
             HandleKind::radius,
             "radius",
             {x + r * dx, y + r * dy, z + h / 2.0},
             {dx, dy, 0},
             1.0},
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {
            {"sides", QuantityKind::plain, "6"}, length("radius", "10"), length("height", "20")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::PrismSpec{static_cast<int>(std::lround(valueOf(p, "sides"))),
                              valueOf(p, "radius"), valueOf(p, "height")};
    }
};

class StarType final : public ShapeType {
public:
    std::string_view id() const override { return "star"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        if (auto issue = requireCorners(p, "points")) {
            return issue;
        }
        if (auto issue = requirePositive(p, {"radius", "innerRadius", "height"})) {
            return issue;
        }
        return valueOf(p, "innerRadius") < valueOf(p, "radius") ? std::nullopt
                                                                : outOfRange("innerRadius");
    }
    Description describe(const ResolvedParams& p) const override {
        return {"star", {p.at("points"), p.at("radius"), p.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double h = valueOf(p, "height");
        // The back tip, and the notch that follows it.
        const double notch = kPi / 2.0 + kPi / std::round(valueOf(p, "points"));
        const double dx = std::cos(notch);
        const double dy = std::sin(notch);
        const double inner = valueOf(p, "innerRadius");
        return {
            {"radius",
             HandleKind::radius,
             "radius",
             {x, y + valueOf(p, "radius"), z + h / 2.0},
             {0, 1, 0},
             1.0},
            {"innerRadius",
             HandleKind::radius,
             "innerRadius",
             {x + inner * dx, y + inner * dy, z + h / 2.0},
             {dx, dy, 0},
             1.0},
            {"height", HandleKind::distance, "height", {x, y, z + h}, {0, 0, 1}, 1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {{"points", QuantityKind::plain, "5"},
                length("radius", "20"),
                length("innerRadius", "8"),
                length("height", "5")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::StarSpec{static_cast<int>(std::lround(valueOf(p, "points"))),
                             valueOf(p, "radius"), valueOf(p, "innerRadius"), valueOf(p, "height")};
    }
};

// Pyramid and wedge: a base of width × depth (like the box) and a height.
class PyramidType final : public ShapeType {
public:
    std::string_view id() const override { return "pyramid"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        return requirePositive(p, {"width", "depth", "height"});
    }
    Description describe(const ResolvedParams& p) const override {
        return {"pyramid", {p.at("width"), p.at("depth"), p.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        return {
            {"width",
             HandleKind::distance,
             "width",
             {x + valueOf(p, "width") / 2.0, y, z},
             {1, 0, 0},
             1.0},
            {"depth",
             HandleKind::distance,
             "depth",
             {x, y + valueOf(p, "depth") / 2.0, z},
             {0, 1, 0},
             1.0},
            {"height",
             HandleKind::distance,
             "height",
             {x, y, z + valueOf(p, "height")},
             {0, 0, 1},
             1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {length("width", "20"), length("depth", "20"), length("height", "20")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::PyramidSpec{valueOf(p, "width"), valueOf(p, "depth"), valueOf(p, "height")};
    }
};

class WedgeType final : public ShapeType {
public:
    std::string_view id() const override { return "wedge"; }
    std::optional<FeatureIssue> validate(const ResolvedParams& p) const override {
        return requirePositive(p, {"width", "depth", "height"});
    }
    Description describe(const ResolvedParams& p) const override {
        return {"wedge", {p.at("width"), p.at("depth"), p.at("height")}};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double d = valueOf(p, "depth");
        const double h = valueOf(p, "height");
        return {
            {"width",
             HandleKind::distance,
             "width",
             {x + valueOf(p, "width") / 2.0, y, z + h / 4.0},
             {1, 0, 0},
             1.0},
            {"depth", HandleKind::distance, "depth", {x, y - d / 2.0, z}, {0, -1, 0}, 1.0},
            {"height", HandleKind::distance, "height", {x, y + d / 2.0, z + h}, {0, 0, 1}, 1.0},
        };
    }

protected:
    std::vector<ParamSpec> sizes() const override {
        return {length("width", "20"), length("depth", "20"), length("height", "20")};
    }
    geo::PrimitiveSpec spec(const ResolvedParams& p) const override {
        return geo::WedgeSpec{valueOf(p, "width"), valueOf(p, "depth"), valueOf(p, "height")};
    }
};

} // namespace

namespace detail {
void addShapeTypes(core::FeatureRegistry& registry) {
    registry.add(std::make_unique<SphereType>());
    registry.add(std::make_unique<HemisphereType>());
    registry.add(std::make_unique<ConeType>());
    registry.add(std::make_unique<TorusType>());
    registry.add(std::make_unique<TubeType>());
    registry.add(std::make_unique<PrismType>());
    registry.add(std::make_unique<PyramidType>());
    registry.add(std::make_unique<WedgeType>());
    registry.add(std::make_unique<StarType>());
}
} // namespace detail

} // namespace fission::features
