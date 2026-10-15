// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/picking.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fission::render {

namespace {

QVector3D toVector(const geo::Point3f& p) {
    return {p.x, p.y, p.z};
}

struct TriangleHit {
    float t = 0.0F;
    float u = 0.0F;
    float v = 0.0F;
};

// Moeller-Trumbore. Back faces count too: from inside a hole you still see its wall.
std::optional<TriangleHit> intersect(const Ray& ray, const QVector3D& a, const QVector3D& b,
                                     const QVector3D& c) {
    const QVector3D edge1 = b - a;
    const QVector3D edge2 = c - a;
    const QVector3D p = QVector3D::crossProduct(ray.direction, edge2);
    const float determinant = QVector3D::dotProduct(edge1, p);
    if (std::abs(determinant) < 1e-12F) {
        return std::nullopt;
    }
    const float inverse = 1.0F / determinant;
    const QVector3D offset = ray.origin - a;
    const float u = QVector3D::dotProduct(offset, p) * inverse;
    if (u < 0.0F || u > 1.0F) {
        return std::nullopt;
    }
    const QVector3D q = QVector3D::crossProduct(offset, edge1);
    const float v = QVector3D::dotProduct(ray.direction, q) * inverse;
    if (v < 0.0F || u + v > 1.0F) {
        return std::nullopt;
    }
    const float t = QVector3D::dotProduct(edge2, q) * inverse;
    if (t <= 0.0F) {
        return std::nullopt;
    }
    return TriangleHit{t, u, v};
}

// Slab test against the body's box; skips most bodies without looking at a triangle.
bool hitsBox(const Ray& ray, const QVector3D& lo, const QVector3D& hi) {
    float near = 0.0F;
    float far = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = ray.origin[axis];
        const float direction = ray.direction[axis];
        if (std::abs(direction) < 1e-9F) {
            if (origin < lo[axis] || origin > hi[axis]) {
                return false;
            }
            continue;
        }
        float t0 = (lo[axis] - origin) / direction;
        float t1 = (hi[axis] - origin) / direction;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        near = std::max(near, t0);
        far = std::min(far, t1);
        if (near > far) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<SurfaceHit> pickSurface(const RenderScene& scene, const Ray& ray) {
    std::optional<SurfaceHit> best;

    for (std::size_t bodyIndex = 0; bodyIndex < scene.bodies.size(); ++bodyIndex) {
        const geo::TriangleMesh& mesh = scene.bodies[bodyIndex].geometry();
        if (mesh.empty()) {
            continue;
        }
        QVector3D lo(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
        QVector3D hi = -lo;
        for (const geo::Point3f& p : mesh.positions) {
            lo = QVector3D(std::min(lo.x(), p.x), std::min(lo.y(), p.y), std::min(lo.z(), p.z));
            hi = QVector3D(std::max(hi.x(), p.x), std::max(hi.y(), p.y), std::max(hi.z(), p.z));
        }
        if (!hitsBox(ray, lo, hi)) {
            continue;
        }
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const std::uint32_t ia = mesh.indices[i];
            const std::uint32_t ib = mesh.indices[i + 1];
            const std::uint32_t ic = mesh.indices[i + 2];
            const auto hit = intersect(ray, toVector(mesh.positions[ia]),
                                       toVector(mesh.positions[ib]), toVector(mesh.positions[ic]));
            if (!hit || (best && hit->t >= best->distance)) {
                continue;
            }
            // The smooth normal at the hit point: a cylinder wall is looked at squarely
            // wherever it was hit, not along the flat of one triangle.
            const float w = 1.0F - hit->u - hit->v;
            QVector3D normal = w * toVector(mesh.normals[ia]) +
                               hit->u * toVector(mesh.normals[ib]) +
                               hit->v * toVector(mesh.normals[ic]);
            normal = normal.lengthSquared() > 1e-12F ? normal.normalized() : QVector3D(0, 0, 1);
            SurfaceHit found;
            found.bodyIndex = static_cast<int>(bodyIndex);
            found.faceId = i / 3 < mesh.faceIds.size() ? mesh.faceIds[i / 3] : 0;
            found.point = ray.origin + ray.direction * hit->t;
            found.normal = normal;
            found.distance = hit->t;
            best = found;
        }
    }

    // The print bed is a plate whose top is the plane z = 0.
    if (std::abs(ray.direction.z()) > 1e-6F) {
        const float t = -ray.origin.z() / ray.direction.z();
        if (t > 0.0F && (!best || t < best->distance)) {
            const QVector3D point = ray.origin + ray.direction * t;
            if (std::abs(point.x()) <= 0.5F * scene.bed.sizeX &&
                std::abs(point.y()) <= 0.5F * scene.bed.sizeY) {
                SurfaceHit bed;
                bed.bodyIndex = -1;
                bed.point = point;
                bed.normal = QVector3D(0, 0, ray.direction.z() < 0.0F ? 1.0F : -1.0F);
                bed.distance = t;
                best = bed;
            }
        }
    }
    return best;
}

std::optional<FaceShape> faceShape(const geo::TriangleMesh& mesh, std::uint32_t faceId) {
    QVector3D weighted;
    float area = 0.0F;
    QVector3D lo(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max());
    QVector3D hi = -lo;
    bool any = false;
    for (std::size_t t = 0; t < mesh.faceIds.size(); ++t) {
        if (mesh.faceIds[t] != faceId || 3 * t + 2 >= mesh.indices.size()) {
            continue;
        }
        const QVector3D a = toVector(mesh.positions[mesh.indices[3 * t]]);
        const QVector3D b = toVector(mesh.positions[mesh.indices[3 * t + 1]]);
        const QVector3D c = toVector(mesh.positions[mesh.indices[3 * t + 2]]);
        const float triangleArea = 0.5F * QVector3D::crossProduct(b - a, c - a).length();
        weighted += triangleArea * (a + b + c) / 3.0F;
        area += triangleArea;
        for (const QVector3D& p : {a, b, c}) {
            lo = QVector3D(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));
            hi = QVector3D(std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z()));
        }
        any = true;
    }
    if (!any) {
        return std::nullopt;
    }
    FaceShape shape;
    // A degenerate face (zero area) falls back to the middle of its box.
    shape.center = area > 1e-9F ? weighted / area : 0.5F * (lo + hi);
    shape.radius = 0.5F * (hi - lo).length();
    return shape;
}

} // namespace fission::render
