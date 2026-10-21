// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/kernel.h"

#include "fission/geo/occt/solids.h"

namespace fission::geo::occt {

namespace {

class OcctKernel final : public GeometryKernel {
public:
    explicit OcctKernel(const KernelOptions& options) : fonts_(options.fontFiles) {}

    NamedSolid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner,
                       double cornerRadius) const override {
        return makeNamedBox(sizeX, sizeY, sizeZ, corner, cornerRadius);
    }
    NamedSolid makePrimitive(const PrimitiveSpec& spec, Point3d base) const override {
        return makeNamedPrimitive(spec, base, fonts_);
    }
    NamedSolid makeCylinder(double radius, double height, Point3d base) const override {
        return makeNamedCylinder(radius, height, base);
    }
    NamedSolid rotate(const NamedSolid& solid, Point3d centre,
                      const Angles& angles) const override {
        return rotateNamed(solid, centre, angles);
    }
    NamedSolid translate(const NamedSolid& solid, Point3d delta) const override {
        return translateNamed(solid, delta);
    }
    NamedSolid boolean(BooleanOp op, const NamedSolid& target, const std::vector<NamedSolid>& tools,
                       double fuzzy) const override {
        return booleanNamed(op, target, tools, fuzzy);
    }
    std::vector<std::string> fontFamilies() const override { return fonts_.families(); }
    std::vector<FontStyle> fontStyles(const std::string& family) const override {
        return fonts_.styles(family);
    }
    SolidReport measure(const Solid& solid) const override { return occt::measure(solid); }
    Bounds bounds(const Solid& solid) const override { return occt::bounds(solid); }
    Bounds exactBounds(const Solid& solid) const override { return occt::exactBounds(solid); }
    TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) const override {
        return occt::tessellate(solid, params);
    }

private:
    FontBook fonts_;
};

} // namespace

std::unique_ptr<GeometryKernel> makeKernel(const KernelOptions& options) {
    return std::make_unique<OcctKernel>(options);
}

} // namespace fission::geo::occt
