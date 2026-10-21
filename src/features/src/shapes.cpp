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
        geo::NamedSolid made =
            turnedBody(input, context, context.kernel.makePrimitive(spec(input.params), base),
                       bodyCentre(input.params));
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    std::vector<ParamSpec> parameters() const final { return withPlace(sizes(), prismatic()); }

    std::optional<ResolvedParams> throughParams(const ResolvedParams& params,
                                                double reach) const final {
        return prismatic() ? prismaticThrough(params, reach) : std::nullopt;
    }

    int version() const final { return 1; }

protected:
    virtual std::vector<ParamSpec> sizes() const = 0;
    /// Runs straight along its axis, so a hole of this shape can go all the way through.
    virtual bool prismatic() const { return false; }
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
    geo::Point3d bodyCentre(const ResolvedParams& p) const override {
        return {valueOf(p, "x"), valueOf(p, "y"), valueOf(p, "z") + valueOf(p, "radius")};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double r = valueOf(p, "radius");
        std::vector<HandleSpec> out;
        addRing(out, "radius", HandleKind::radius, valueOf(p, "x"), valueOf(p, "y"),
                valueOf(p, "z") + r, r, r);
        for (HandleSpec& handle : out) {
            handle.rise = 1.0; // the sphere stands on its plane: its equator rises as it grows
        }
        return out;
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
    geo::Point3d bodyCentre(const ResolvedParams& p) const override {
        return {valueOf(p, "x"), valueOf(p, "y"), valueOf(p, "z") + valueOf(p, "radius") / 2.0};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double r = valueOf(p, "radius");
        std::vector<HandleSpec> out;
        addRing(out, "radius", HandleKind::radius, valueOf(p, "x"), valueOf(p, "y"),
                valueOf(p, "z"), r, r);
        return out;
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
        const double top = valueOf(p, "topRadius");
        std::vector<HandleSpec> out;
        addRing(out, "radius", HandleKind::radius, x, y, z, valueOf(p, "radius"),
                valueOf(p, "radius"));
        if (top > 0.0) { // a point has nothing to pull on; the height handle sits there
            addRing(out, "topRadius", HandleKind::radius, x, y, z + h, top, top);
        }
        out.push_back(makeHandle("height", HandleKind::distance, {x, y, z + h}, {0, 0, 1}, h));
        return out;
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
    geo::Point3d bodyCentre(const ResolvedParams& p) const override {
        return {valueOf(p, "x"), valueOf(p, "y"), valueOf(p, "z") + valueOf(p, "tubeRadius")};
    }
    std::vector<HandleSpec> handles(const ResolvedParams& p) const override {
        const double x = valueOf(p, "x");
        const double y = valueOf(p, "y");
        const double z = valueOf(p, "z");
        const double big = valueOf(p, "radius");
        const double t = valueOf(p, "tubeRadius");
        std::vector<HandleSpec> out;
        addRing(out, "radius", HandleKind::radius, x, y, z + t, big, big);
        const std::size_t tube = out.size();
        addRing(out, "tubeRadius", HandleKind::distance, x, y, z + t, big + t, t);
        markRole(out, tube, HandleRole::detail);
        for (std::size_t i = tube; i < out.size(); ++i) {
            out[i].rise = 1.0; // a thicker tube lifts the ring off its plane
        }
        return out;
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
    bool prismatic() const override { return true; }
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
        const double wall = valueOf(p, "wall");
        std::vector<HandleSpec> out;
        addRing(out, "radius", HandleKind::radius, x, y, z + h / 2.0, r, r);
        // On the inner rim at the top: pulled towards the middle the wall gets thicker.
        const std::size_t walls = out.size();
        addRing(out, "wall", HandleKind::distance, x, y, z + h, r - wall, wall, /*inward=*/true);
        markRole(out, walls, HandleRole::detail);
        out.push_back(makeHandle("height", HandleKind::distance, {x, y, z + h}, {0, 0, 1}, h));
        return out;
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
    bool prismatic() const override { return true; }
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
        const int sides = static_cast<int>(std::lround(valueOf(p, "sides")));
        std::vector<HandleSpec> out;
        // At every corner: the one facing the viewer is shown. The corners are where the kernel
        // puts them (a flat side faces front).
        addRing(out, "radius", HandleKind::radius, x, y, z + h / 2.0, r, r, false, sides,
                -kPi / 2.0 - kPi / sides);
        out.push_back(makeHandle("height", HandleKind::distance, {x, y, z + h}, {0, 0, 1}, h));
        return out;
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
    bool prismatic() const override { return true; }
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
        const int points = static_cast<int>(std::lround(valueOf(p, "points")));
        std::vector<HandleSpec> out;
        // The tips, and the notches between them (half a step further round).
        addRing(out, "radius", HandleKind::radius, x, y, z + h / 2.0, valueOf(p, "radius"),
                valueOf(p, "radius"), false, points, kPi / 2.0);
        const std::size_t notches = out.size();
        addRing(out, "innerRadius", HandleKind::radius, x, y, z + h / 2.0,
                valueOf(p, "innerRadius"), valueOf(p, "innerRadius"), false, points,
                kPi / 2.0 + kPi / points);
        markRole(out, notches, HandleRole::detail);
        out.push_back(makeHandle("height", HandleKind::distance, {x, y, z + h}, {0, 0, 1}, h));
        return out;
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
        const double w = valueOf(p, "width");
        const double d = valueOf(p, "depth");
        const double h = valueOf(p, "height");
        std::vector<HandleSpec> out;
        addBothEnds(out, "width", {x, y, z}, {1, 0, 0}, w / 2.0, w, 0.5);
        addBothEnds(out, "depth", {x, y, z}, {0, 1, 0}, d / 2.0, d, 0.5);
        out.push_back(makeHandle("height", HandleKind::distance, {x, y, z + h}, {0, 0, 1}, h));
        return out;
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
        const double w = valueOf(p, "width");
        const double d = valueOf(p, "depth");
        const double h = valueOf(p, "height");
        std::vector<HandleSpec> out;
        addBothEnds(out, "width", {x, y, z}, {1, 0, 0}, w / 2.0, w, 0.5);
        addBothEnds(out, "depth", {x, y, z}, {0, 1, 0}, d / 2.0, d, 0.5);
        out.push_back(
            makeHandle("height", HandleKind::distance, {x, y + d / 2.0, z + h}, {0, 0, 1}, h));
        return out;
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
