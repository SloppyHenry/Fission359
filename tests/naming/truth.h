// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Ground truth for the naming tests (AP-0.6). Everything here is a pure geometric query on a
// shape: "which faces lie in this plane with this outward normal?". It never looks at names,
// so it can judge the naming system independently. The tests state the intended target
// analytically from the model parameters (a box is 40 wide, so its +X face lies at x = 40).

#pragma once

#include <vector>

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace fission::test::naming::truth {

using Shapes = std::vector<TopoDS_Shape>;

constexpr double kTolerance = 1e-4; // mm

// Planar faces that lie in the plane through `point` and whose outward normal is `outward`.
Shapes planarFaces(const TopoDS_Shape& shape, const gp_Pnt& point, const gp_Dir& outward);

// Straight edges whose two end points both lie in both planes (the edge "between" two faces).
Shapes straightEdgesBetween(const TopoDS_Shape& shape, const gp_Pnt& point1, const gp_Dir& normal1,
                            const gp_Pnt& point2, const gp_Dir& normal2);

// Cylindrical faces around the axis through `point` along `axis` with the given radius.
Shapes cylinderFaces(const TopoDS_Shape& shape, const gp_Pnt& point, const gp_Dir& axis,
                     double radius);

// Circular edges with the given center, axis direction and radius.
Shapes circleEdges(const TopoDS_Shape& shape, const gp_Pnt& center, const gp_Dir& axis,
                   double radius);

bool contains(const Shapes& shapes, const TopoDS_Shape& candidate);

} // namespace fission::test::naming::truth
