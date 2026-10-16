// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Finding the surface under a pixel by intersecting a ray with the display triangles. Used for
// the double click and Smart Zoom, which happen once per click. Hover and selection need the
// ID buffer instead (AP-1.4), because they run on every mouse move.

#pragma once

#include <QVector3D>

#include <cstdint>
#include <optional>
#include <string>

#include "fission/geo/mesh.h"
#include "fission/render/camera.h"
#include "fission/render/scene.h"

namespace fission::render {

struct SurfaceHit {
    int bodyIndex = -1; ///< -1: the print bed
    std::uint32_t faceId = 0;
    QVector3D point;
    QVector3D normal; ///< unit length, pointing out of the body
    float distance = 0.0F;
};

/// The nearest surface along `ray`: a body face, or else the top of the print bed. The body whose
/// `RenderBody::id` is `ignoreBody` is looked through (the shape being put down must not land on
/// itself).
std::optional<SurfaceHit> pickSurface(const RenderScene& scene, const Ray& ray,
                                      const std::string& ignoreBody = {});

/// Centre and radius of the sphere around one face of a body, and the way it faces (the mean of
/// its triangles' normals, pointing out of the body; for a curved face that is only a direction
/// to look from).
struct FaceShape {
    QVector3D center;
    float radius = 0.0F;
    QVector3D normal{0.0F, 0.0F, 1.0F};
};
std::optional<FaceShape> faceShape(const geo::TriangleMesh& mesh, std::uint32_t faceId);

} // namespace fission::render
