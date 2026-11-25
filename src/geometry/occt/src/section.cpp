// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The cut face of a body along a plane (AP-2.11a).

#include "fission/geo/occt/section.h"

#include <algorithm>
#include <cmath>

#include "fission/geo/error.h"

#include "occt_build.h"
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>

namespace fission::geo::occt {

SectionMesh sectionMesh(const Solid& solid, Point3d planePoint, Point3d planeNormal,
                        double deflection) {
    const double length = std::sqrt(planeNormal.x * planeNormal.x + planeNormal.y * planeNormal.y +
                                    planeNormal.z * planeNormal.z);
    if (!isFinite(planePoint) || !isFinite(planeNormal) || !(length > 1e-12) ||
        !isPositiveFinite(deflection)) {
        throw GeometryError(GeometryErrorCode::invalidInput,
                            "section: the plane or the tolerance is not a usable number");
    }
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput, "not a body of this kernel");
    }
    const gp_Dir normal(planeNormal.x / length, planeNormal.y / length, planeNormal.z / length);
    SectionMesh section;
    try {
        Bnd_Box box;
        BRepBndLib::Add(data->shape(), box, /*useTriangulation=*/false);
        if (box.IsVoid()) {
            return section;
        }
        const gp_Pnt origin(planePoint.x, planePoint.y, planePoint.z);
        // A square on the plane, big enough to cover the body wherever the plane cuts it.
        const gp_Pnt lo = box.CornerMin();
        const gp_Pnt hi = box.CornerMax();
        const double reach = lo.Distance(hi) + origin.Distance(gp_Pnt((lo.XYZ() + hi.XYZ()) / 2.0));
        const double half = 2.0 * reach + 1.0;
        const gp_Pln plane(origin, normal);
        const TopoDS_Face sheet = BRepBuilderAPI_MakeFace(plane, -half, half, -half, half).Face();

        BRepAlgoAPI_Common common(data->shape(), sheet);
        if (!common.IsDone()) {
            throw GeometryError(GeometryErrorCode::kernelFailure, "the section could not be cut");
        }
        const TopoDS_Shape shape = BRepBuilderAPI_Copy(common.Shape(), /*copyGeom=*/false).Shape();
        BRepMesh_IncrementalMesh mesher(shape, deflection, /*isRelative=*/false, 0.5,
                                        /*isInParallel=*/false);
        for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
            const TopoDS_Face face = TopoDS::Face(it.Current());
            GProp_GProps props;
            BRepGProp::SurfaceProperties(face, props);
            section.area += props.Mass();
            TopLoc_Location location;
            const occ::handle<Poly_Triangulation>& mesh = BRep_Tool::Triangulation(face, location);
            if (mesh.IsNull()) {
                continue;
            }
            const gp_Trsf transform = location.Transformation();
            const auto base = static_cast<std::uint32_t>(section.positions.size());
            for (int i = 1; i <= mesh->NbNodes(); ++i) {
                const gp_Pnt p = mesh->Node(i).Transformed(transform);
                section.positions.push_back({p.X(), p.Y(), p.Z()});
            }
            for (int t = 1; t <= mesh->NbTriangles(); ++t) {
                int a = 0;
                int b = 0;
                int c = 0;
                mesh->Triangle(t).Get(a, b, c);
                const Point3d& pa = section.positions[base + static_cast<std::uint32_t>(a - 1)];
                const Point3d& pb = section.positions[base + static_cast<std::uint32_t>(b - 1)];
                const Point3d& pc = section.positions[base + static_cast<std::uint32_t>(c - 1)];
                const gp_Vec cross = gp_Vec(pb.x - pa.x, pb.y - pa.y, pb.z - pa.z)
                                         .Crossed(gp_Vec(pc.x - pa.x, pc.y - pa.y, pc.z - pa.z));
                if (cross.Dot(gp_Vec(normal)) < 0.0) {
                    std::swap(b, c);
                }
                for (const int index : {a, b, c}) {
                    section.indices.push_back(base + static_cast<std::uint32_t>(index - 1));
                }
            }
        }
    } catch (const GeometryError&) {
        throw;
    } catch (const Standard_Failure&) {
        // Empty means only "the plane misses the body"; a failed cut is an error the caller can
        // log (and show no cap for this body).
        throw GeometryError(GeometryErrorCode::kernelFailure, "the section could not be cut");
    }
    return section;
}

} // namespace fission::geo::occt
