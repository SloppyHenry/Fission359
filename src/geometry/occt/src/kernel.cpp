// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/geo/occt/kernel.h"

#include "fission/geo/occt/exchange.h"
#include "fission/geo/occt/measuring.h"
#include "fission/geo/occt/sketch_curve.h"
#include "fission/geo/occt/sketch_edge.h"
#include "fission/geo/occt/sketch_intersections.h"
#include "fission/geo/occt/sketch_regions.h"
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
    NamedSolid transformed(const NamedSolid& solid, const Transform3& transform) const override {
        return transformNamed(solid, transform);
    }
    NamedSolid boolean(BooleanOp op, const NamedSolid& target, const std::vector<NamedSolid>& tools,
                       double fuzzy) const override {
        return booleanNamed(op, target, tools, fuzzy);
    }
    NamedSolid blendEdges(const NamedSolid& solid, const std::vector<BlendEdge>& edges,
                          const BlendSpec& spec, const std::string& rolePrefix) const override {
        return blendEdgesNamed(solid, edges, spec, rolePrefix);
    }
    NamedSolid offsetFace(const NamedSolid& solid, std::uint32_t faceId,
                          double distance) const override {
        return offsetFaceNamed(solid, faceId, distance);
    }
    FaceAnchor faceAnchor(const Solid& solid, std::uint32_t faceId) const override {
        return faceAnchorOf(solid, faceId);
    }
    FlatFace flatFace(const Solid& solid, std::uint32_t faceId) const override {
        return flatFaceOf(solid, faceId);
    }
    NamedSolid removeFaces(const NamedSolid& solid,
                           const std::vector<std::uint32_t>& faceIds) const override {
        return removeFacesNamed(solid, faceIds);
    }
    NamedSolid offsetFaces(const NamedSolid& solid, const std::vector<std::uint32_t>& faceIds,
                           double distance) const override {
        return offsetFacesNamed(solid, faceIds, distance);
    }
    NamedSolid tiltFace(const NamedSolid& solid, std::uint32_t faceId, Point3d pivot, Point3d axis,
                        double degrees) const override {
        return tiltFaceNamed(solid, faceId, pivot, axis, degrees);
    }
    NamedSolid hollow(const NamedSolid& solid, const std::vector<std::uint32_t>& openFaces,
                      double wall, bool outside, const std::string& rolePrefix) const override {
        return hollowNamed(solid, openFaces, wall, outside, rolePrefix);
    }
    NamedSolid draftFaces(const NamedSolid& solid, const std::vector<std::uint32_t>& faceIds,
                          Point3d neutralPoint, Point3d pull, double degrees) const override {
        return draftFacesNamed(solid, faceIds, neutralPoint, pull, degrees);
    }
    std::optional<double> plainCut(const Solid& solid, Point3d way, double near) const override {
        return plainCutOf(solid, way, near);
    }
    NamedSolid stretch(const NamedSolid& solid, Point3d way, double position,
                       double distance) const override {
        return stretchNamed(solid, way, position, distance);
    }
    std::vector<std::uint32_t> tangentChain(const Solid& solid, std::uint32_t edge) const override {
        return tangentChainOf(solid, edge);
    }
    std::vector<std::string> fontFamilies() const override { return fonts_.families(); }
    std::vector<FontStyle> fontStyles(const std::string& family) const override {
        return fonts_.styles(family);
    }
    ImportResult importFile(ImportFormat format, const std::vector<std::uint8_t>& bytes,
                            const ImportOptions& options) const override {
        return occt::importFile(format, bytes, options);
    }
    std::string exportStep(const std::vector<ExportBody>& bodies) const override {
        return occt::exportStep(bodies);
    }
    Measurement measureElements(const std::vector<ElementRef>& elements) const override {
        return occt::measureElements(elements);
    }
    Outline faceOutline(const Solid& solid, std::uint32_t faceId) const override {
        return occt::faceOutline(solid, faceId);
    }
    SketchEdgeShape projectEdgeToSketch(const Solid& solid, std::uint32_t edgeId,
                                        const SketchFrame& frame) const override {
        return occt::projectEdgeToSketch(solid, edgeId, frame);
    }
    SketchSplineCurve sketchSpline(const std::vector<Point2d>& through,
                                   double tolerance) const override {
        return occt::sketchSpline(through, tolerance);
    }
    std::vector<std::vector<SketchCrossing>>
    sketchIntersections(const std::vector<SketchCurve>& curves) const override {
        return occt::sketchIntersections(curves);
    }
    SketchRegionResult sketchRegions(const std::vector<SketchCurve>& curves) const override {
        return occt::sketchRegions(curves);
    }
    NamedSolid extrudeRegion(const std::vector<SketchCurve>& curves, Point2d inside,
                             const SketchFrame& frame, const ExtrudeSpec& spec) const override {
        return occt::extrudeRegion(curves, inside, frame, spec);
    }
    std::vector<SketchCrossing>
    sketchCarrierCrossings(const SketchCurve& carrier,
                           const std::vector<SketchCurve>& others) const override {
        return occt::sketchCarrierCrossings(carrier, others);
    }
    SolidReport measure(const Solid& solid) const override { return occt::measure(solid); }
    Bounds bounds(const Solid& solid) const override { return occt::bounds(solid); }
    Bounds exactBounds(const Solid& solid) const override { return occt::exactBounds(solid); }
    std::vector<SnapPointGal> snapPoints(const Solid& solid) const override {
        return occt::snapPoints(solid);
    }
    TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) const override {
        return occt::tessellate(solid, params);
    }

    BodySignatures signatures(const Solid& solid) const override { return occt::signatures(solid); }

private:
    FontBook fonts_;
};

} // namespace

std::unique_ptr<GeometryKernel> makeKernel(const KernelOptions& options) {
    return std::make_unique<OcctKernel>(options);
}

} // namespace fission::geo::occt
