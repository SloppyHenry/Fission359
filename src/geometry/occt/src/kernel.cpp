// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/kernel.h"

#include "fission/geo/occt/solids.h"

namespace fission::geo::occt {

namespace {

class OcctKernel final : public GeometryKernel {
public:
    NamedSolid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner) const override {
        return makeNamedBox(sizeX, sizeY, sizeZ, corner);
    }
    NamedSolid makeCylinder(double radius, double height, Point3d base) const override {
        return makeNamedCylinder(radius, height, base);
    }
    SolidReport measure(const Solid& solid) const override { return occt::measure(solid); }
    TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) const override {
        return occt::tessellate(solid, params);
    }
};

} // namespace

std::unique_ptr<GeometryKernel> makeKernel() {
    return std::make_unique<OcctKernel>();
}

} // namespace fission::geo::occt
