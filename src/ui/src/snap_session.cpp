// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/snap_session.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointF>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <set>

#include "fission/ui/viewport_3d.h"

namespace fission::ui {

namespace {

// Sizes of the marks and the dashes, in pixels of the window.
constexpr float kMarkRadius = 6.5F;
constexpr float kMarkRing = 2.0F;
constexpr double kDash = 6.0;
constexpr double kGap = 4.0;
constexpr float kLineWidth = 1.5F;
constexpr int kMostDashes = 120;

bool g_suppressedForTests = false;

QVector3D toVector(const interaction::Vec3& p) {
    return {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
}

QColor withAlpha(QColor colour, float alpha) {
    colour.setAlphaF(colour.alphaF() * alpha);
    return colour;
}

void addMark(render::HandleGraphics& graphics, const interaction::SnapPoint& point,
             const QColor& colour) {
    render::HandleDot dot;
    dot.centre = toVector(point.position);
    dot.radius = kMarkRadius;
    dot.fill = withAlpha(colour, 0.25F);
    dot.ring = colour;
    dot.ringWidth = kMarkRing;
    // A corner is a square, the others are round; the middle of a circle or a face has a dot in
    // it as well.
    dot.square = point.kind == interaction::SnapKind::corner;
    graphics.dots.push_back(dot);
    if (point.kind == interaction::SnapKind::circleCentre ||
        point.kind == interaction::SnapKind::faceMiddle) {
        render::HandleDot inner = dot;
        inner.radius = 2.0F;
        inner.fill = colour;
        inner.ringWidth = 0.0F;
        graphics.dots.push_back(inner);
    }
}

} // namespace

bool snappingSuppressed() {
    // (The tests run without a window system: there is no keyboard to ask.)
    return g_suppressedForTests ||
           (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr &&
            (QGuiApplication::queryKeyboardModifiers() & Qt::ShiftModifier) != 0);
}

void setSnappingSuppressedForTests(bool suppressed) {
    g_suppressedForTests = suppressed;
}

double millimetresPerPixelAt(const Viewport3D& viewport, const QVector3D& point) {
    QVector3D sight = point - viewport.eyePosition();
    sight = sight.lengthSquared() > 1e-12F ? sight.normalized() : QVector3D(0, 0, -1);
    QVector3D across = QVector3D::crossProduct(sight, QVector3D(0, 0, 1));
    if (across.lengthSquared() < 1e-6F) {
        across = QVector3D::crossProduct(sight, QVector3D(1, 0, 0));
    }
    across.normalize();
    const QPointF here = viewport.project(point.x(), point.y(), point.z());
    const QVector3D next = point + across;
    const QPointF there = viewport.project(next.x(), next.y(), next.z());
    const double pixels = std::hypot(there.x() - here.x(), there.y() - here.y());
    return pixels > 1e-9 ? 1.0 / pixels : 1.0;
}

interaction::Vec3 snapReachAround(const Viewport3D& viewport, const QVector3D& around) {
    const QPointF here = viewport.project(around.x(), around.y(), around.z());
    const double across = 1.0 / millimetresPerPixelAt(viewport, around);
    interaction::Vec3 reach{};
    for (int axis = 0; axis < 3; ++axis) {
        QVector3D next = around;
        next[axis] += 1.0F;
        const QPointF there = viewport.project(next.x(), next.y(), next.z());
        const double pixels = std::hypot(there.x() - here.x(), there.y() - here.y());
        reach[static_cast<std::size_t>(axis)] =
            interaction::kSnapReachPixels / std::max(pixels, 0.35 * across);
    }
    return reach;
}

void SnapSession::collectTargets(const render::RenderScene& scene, const std::string& movingBody) {
    m_targets.clear();
    std::set<std::string> present;
    for (const render::RenderBody& body : scene.bodies) {
        present.insert(body.id);
        if (body.id == movingBody || !body.mesh || body.mesh->empty()) {
            continue;
        }
        Points& kept = m_cache[body.id];
        if (kept.version != body.meshVersion || kept.points.empty()) {
            kept.points = interaction::snapPointsOf(*body.mesh);
            kept.version = body.meshVersion;
        }
        m_targets.insert(m_targets.end(), kept.points.begin(), kept.points.end());
    }
    // Bodies that are gone do not stay in the cache.
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        it = present.count(it->first) != 0 ? std::next(it) : m_cache.erase(it);
    }
    const auto bed = interaction::snapPointsOfBed(scene.bed.sizeX, scene.bed.sizeY);
    m_targets.insert(m_targets.end(), bed.begin(), bed.end());
}

void SnapSession::begin(const render::RenderScene& scene, const std::string& movingBody,
                        const interaction::Vec3& low, const interaction::Vec3& high) {
    collectTargets(scene, movingBody);
    m_movers = interaction::snapPointsOfBox(
        {interaction::cleanedCoordinate(low[0]), interaction::cleanedCoordinate(low[1]),
         interaction::cleanedCoordinate(low[2])},
        {interaction::cleanedCoordinate(high[0]), interaction::cleanedCoordinate(high[1]),
         interaction::cleanedCoordinate(high[2])});
    m_active = true;
}

void SnapSession::beginWithMovers(const render::RenderScene& scene, const std::string& movingBody,
                                  std::vector<interaction::SnapPoint> movers) {
    collectTargets(scene, movingBody);
    m_movers = std::move(movers);
    m_active = true;
}

void SnapSession::end() {
    m_active = false;
    m_targets.clear();
    m_movers.clear();
}

interaction::SnapResult SnapSession::snap(const interaction::Vec3& offset,
                                          const std::array<bool, 3>& free,
                                          const interaction::Vec3& reach) const {
    if (!m_active) {
        interaction::SnapResult none;
        none.offset = offset;
        return none;
    }
    return interaction::snapOffset(m_movers, m_targets, offset, free, reach);
}

render::HandleGraphics SnapSession::marksOf(const interaction::SnapResult& result,
                                            const QColor& colour, double mmPerPixel) {
    render::HandleGraphics graphics;
    std::vector<interaction::SnapPoint> marked;
    const auto isMarked = [&marked](const interaction::SnapPoint& p) {
        return std::find(marked.begin(), marked.end(), p) != marked.end();
    };
    for (const auto& axis : result.axes) {
        if (!axis) {
            continue;
        }
        if (!isMarked(axis->target)) {
            marked.push_back(axis->target);
            addMark(graphics, axis->target, colour);
        }
        if (axis->onPoint()) {
            continue;
        }
        // The two only line up: the point of the body gets a small dot, a dashed line joins them.
        interaction::SnapPoint mover = axis->mover;
        if (!isMarked(mover)) {
            marked.push_back(mover);
            render::HandleDot dot;
            dot.centre = toVector(mover.position);
            dot.radius = 3.5F;
            dot.fill = colour;
            dot.ring = colour;
            dot.ringWidth = 0.0F;
            graphics.dots.push_back(dot);
        }
        const QVector3D from = toVector(mover.position);
        const QVector3D to = toVector(axis->target.position);
        const float length = (to - from).length();
        const double period = (kDash + kGap) * mmPerPixel;
        if (length < 1e-4F || period <= 0.0) {
            continue;
        }
        const QVector3D way = (to - from) / length;
        const int dashes = std::min(kMostDashes, static_cast<int>(std::ceil(length / period)));
        const double dash = std::min(kDash * mmPerPixel, static_cast<double>(length));
        const double step = dashes > 1 ? (length - dash) / (dashes - 1) : 0.0;
        for (int i = 0; i < dashes; ++i) {
            const QVector3D start = from + way * static_cast<float>(i * step);
            graphics.lines.push_back(
                {start, start + way * static_cast<float>(dash), kLineWidth, colour});
        }
    }
    return graphics;
}

} // namespace fission::ui
