// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/sketch_controller.h"

#include <QDebug>
#include <QVariantList>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "fission/core/selection.h"
#include "fission/render/scene_renderer.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

namespace {

constexpr double kCatchPixels = 10.0;  // how near the pointer must be to catch
constexpr double kLift = 0.05;         // mm: drawn a little in front of the plane (no flicker)
constexpr double kOutsideHalf = 100.0; // mm: half size of the grid on the plane of a face
constexpr float kGridWidth = 1.0F;
constexpr float kElementWidth = 2.5F;
constexpr float kGuideWidth = 1.25F;
constexpr double kDashPixels = 6.0;
constexpr int kMaxGridLines = 160; // per direction

QVector3D at(const core::SketchPlane& plane, double u, double v) {
    const geo::Point3d world = plane.toWorld({u, v});
    const geo::Point3d& n = plane.normal();
    return {static_cast<float>(world.x + n.x * kLift), static_cast<float>(world.y + n.y * kLift),
            static_cast<float>(world.z + n.z * kLift)};
}

void line(render::HandleGraphics& out, const core::SketchPlane& plane, core::SketchPoint from,
          core::SketchPoint to, float width, const QColor& colour) {
    out.lines.push_back({at(plane, from.u, from.v), at(plane, to.u, to.v), width, colour});
}

// A dashed line: strokes of `dash` mm with gaps of the same length.
void dashed(render::HandleGraphics& out, const core::SketchPlane& plane, core::SketchPoint from,
            core::SketchPoint to, double dash, float width, const QColor& colour) {
    const double length = std::hypot(to.u - from.u, to.v - from.v);
    if (!(length > 1e-9) || !(dash > 0.0)) {
        return;
    }
    const int strokes = std::max(1, static_cast<int>(std::ceil(length / (2.0 * dash))));
    for (int i = 0; i < strokes; ++i) {
        const double a = static_cast<double>(i) * 2.0 * dash / length;
        const double b = std::min(1.0, a + dash / length);
        if (a >= 1.0) {
            break;
        }
        line(out, plane, {from.u + (to.u - from.u) * a, from.v + (to.v - from.v) * a},
             {from.u + (to.u - from.u) * b, from.v + (to.v - from.v) * b}, width, colour);
    }
}

// A chain of short lines from `a0` to `a1` (radians, counter-clockwise) round a middle.
void arcLines(render::HandleGraphics& out, const core::SketchPlane& plane, core::SketchPoint middle,
              double radius, double a0, double a1, double pixelsPerMm, float width,
              const QColor& colour, bool isDashed, double dash) {
    const double span = a1 - a0;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(std::abs(span) * radius * pixelsPerMm / 6.0)), 12, 720);
    std::vector<core::SketchPoint> points;
    for (int i = 0; i <= segments; ++i) {
        const double a = a0 + span * static_cast<double>(i) / static_cast<double>(segments);
        points.push_back({middle.u + radius * std::cos(a), middle.v + radius * std::sin(a)});
    }
    if (!isDashed) {
        for (std::size_t i = 1; i < points.size(); ++i) {
            line(out, plane, points[i - 1], points[i], width, colour);
        }
        return;
    }
    // Dashes follow the length along the curve: every other piece of the chain.
    const double piece = std::max(
        1.0, dash * 2.0 / std::max(1e-9, std::abs(span) * radius / static_cast<double>(segments)));
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (static_cast<long>(std::floor(static_cast<double>(i) / piece)) % 2 == 0) {
            line(out, plane, points[i - 1], points[i], width, colour);
        }
    }
}

void drawElement(render::HandleGraphics& out, const core::SketchPlane& plane,
                 const core::SketchElement& element, double pixelsPerMm, const QColor& colour) {
    if (!core::sketchPointsUsable(element.kind, element.points)) {
        return;
    }
    const double dash = kDashPixels / std::max(pixelsPerMm, 1e-6);
    QColor shown = colour;
    if (element.construction) {
        shown.setAlpha(170);
    }
    const auto& p = element.points;
    switch (element.kind) {
    case core::SketchElementKind::point: {
        render::HandleDot dot;
        dot.centre = at(plane, p[0].u, p[0].v);
        dot.radius = 3.5F;
        dot.fill = colour;
        dot.ring = colour;
        dot.ringWidth = 0.0F;
        out.dots.push_back(dot);
        break;
    }
    case core::SketchElementKind::line:
        if (element.construction) {
            dashed(out, plane, p[0], p[1], dash, kElementWidth, shown);
        } else {
            line(out, plane, p[0], p[1], kElementWidth, shown);
        }
        break;
    case core::SketchElementKind::circle: {
        const double radius = std::hypot(p[1].u - p[0].u, p[1].v - p[0].v);
        arcLines(out, plane, p[0], radius, 0.0, 2.0 * std::numbers::pi, pixelsPerMm, kElementWidth,
                 shown, element.construction, dash);
        break;
    }
    case core::SketchElementKind::arc: {
        const double radius = std::hypot(p[1].u - p[0].u, p[1].v - p[0].v);
        const double a0 = std::atan2(p[1].v - p[0].v, p[1].u - p[0].u);
        double a1 = std::atan2(p[2].v - p[0].v, p[2].u - p[0].u);
        if (a1 <= a0) {
            a1 += 2.0 * std::numbers::pi;
        }
        arcLines(out, plane, p[0], radius, a0, a1, pixelsPerMm, kElementWidth, shown,
                 element.construction, dash);
        break;
    }
    case core::SketchElementKind::spline:
        // Until the curve is calculated (AP-2.1d) the chain of its points.
        for (std::size_t i = 1; i < p.size(); ++i) {
            if (element.construction) {
                dashed(out, plane, p[i - 1], p[i], dash, kElementWidth, shown);
            } else {
                line(out, plane, p[i - 1], p[i], kElementWidth, shown);
            }
        }
        break;
    }
}

QString kindName(interaction::SketchSnapKind kind) {
    switch (kind) {
    case interaction::SketchSnapKind::point:
        return QStringLiteral("point");
    case interaction::SketchSnapKind::middle:
        return QStringLiteral("middle");
    case interaction::SketchSnapKind::alignment:
        return QStringLiteral("alignment");
    case interaction::SketchSnapKind::grid:
        return QStringLiteral("grid");
    case interaction::SketchSnapKind::none:
        break;
    }
    return QStringLiteral("none");
}

} // namespace

SketchController::SketchController(DocumentModel* model, SelectionController* selection,
                                   QObject* parent)
    : QObject(parent), m_model(model), m_selection(selection) {
    connect(model, &DocumentModel::elementsChanged, this, &SketchController::onModelChanged);
    connect(model, &DocumentModel::recipeChanged, this, &SketchController::onModelChanged);
    connect(model, &DocumentModel::documentReplaced, this, [this] {
        if (m_mode != Mode::idle) {
            m_previousPose.reset(); // another document: the old view is not for it
            leave(false);
        }
    });
}

void SketchController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (viewport == nullptr) {
        return;
    }
    connect(viewport, &Viewport3D::picked, this, &SketchController::onPicked);
    connect(viewport, &Viewport3D::pointerMoved, this, &SketchController::onPointerMoved);
    connect(viewport, &Viewport3D::cameraChanged, this, [this] {
        if (m_mode == Mode::sketching) {
            refresh();
        }
    });
}

double SketchController::baseStep() const {
    return m_viewport != nullptr ? m_viewport->snapStep() : 1.0;
}

void SketchController::start() {
    if (m_mode != Mode::idle) {
        return;
    }
    m_mode = Mode::picking;
    if (m_selection != nullptr) {
        m_selection->clear();
        m_selection->overrideViewportFilter(
            core::SelectionFilter::only(core::SelectableKind::face));
    }
    emit stateChanged();
}

bool SketchController::pickFace(const QString& body, int number) {
    if (m_mode != Mode::picking || m_model == nullptr) {
        return false;
    }
    if (m_selection != nullptr) {
        m_selection->clear();
    }
    if (!m_model->faceIsFlat(body, number)) {
        emit notice(QStringLiteral("curvedFace"));
        return false;
    }
    const QString id = m_model->addSketch(body, number);
    if (id.isEmpty()) {
        emit notice(QStringLiteral("faceUnusable"));
        return false;
    }
    beginOn(id);
    return true;
}

bool SketchController::pickGround() {
    if (m_mode != Mode::picking || m_model == nullptr) {
        return false;
    }
    const QString id = m_model->addSketch(QString(), 0);
    if (id.isEmpty()) {
        return false;
    }
    beginOn(id);
    return true;
}

void SketchController::beginOn(const QString& stepId) {
    m_step = stepId;
    m_mode = Mode::sketching;
    m_planeReady = false;
    m_cameraTurned = false;
    m_caught.reset();
    m_snapMap = {};
    if (m_viewport != nullptr && !m_previousPose) {
        m_previousPose = m_viewport->cameraPose();
    }
    if (m_selection != nullptr) {
        m_selection->clear();
        core::SelectionFilter nothing;
        nothing.bodies = nothing.faces = nothing.edges = nothing.vertices = false;
        m_selection->overrideViewportFilter(nothing);
    }
    m_model->setActiveSketch(stepId);
    emit stateChanged();
    onModelChanged(); // the plane may be there already (a sketch that is opened again)
}

void SketchController::leave(bool removeIfEmpty) {
    const QString leftStep = m_step;
    const bool wasSketching = m_mode == Mode::sketching;
    m_mode = Mode::idle;
    m_step.clear();
    m_planeReady = false;
    m_caught.reset();
    m_snapMap = {};
    m_gridStep = 0.0;
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics({}, Viewport3D::HandleSource::sketch);
    }
    if (m_selection != nullptr) {
        m_selection->overrideViewportFilter(std::nullopt);
    }
    if (m_model != nullptr) {
        m_model->setActiveSketch(QString()); // the other bodies are solid again
        if (removeIfEmpty && wasSketching) {
            m_model->removeSketchIfEmpty(leftStep);
        }
    }
    if (wasSketching && m_viewport != nullptr && m_previousPose) {
        m_viewport->goToPose(*m_previousPose);
    }
    m_previousPose.reset();
    emit stateChanged();
    emit snapChanged();
}

void SketchController::finish() {
    if (m_mode == Mode::idle) {
        return;
    }
    leave(true);
}

void SketchController::escape() {
    if (m_mode == Mode::idle) {
        return;
    }
    // A drawing tool would end first (AP-2.1c); with none on, Esc ends the sketch.
    finish();
}

bool SketchController::reopen(int stepIndex) {
    if (m_model == nullptr) {
        return false;
    }
    const QString id = m_model->stepIdAt(stepIndex);
    if (id.isEmpty() || (m_mode == Mode::sketching && id == m_step)) {
        return !id.isEmpty();
    }
    const auto result = m_model->sketchResult(id);
    if (!result || !result->plane) {
        emit notice(QStringLiteral("needsHelp"));
        return false;
    }
    if (m_mode == Mode::sketching) {
        const std::optional<render::CameraPose> keep = m_previousPose;
        leave(true);
        m_previousPose = keep; // coming back from the other sketch goes to where we started
    }
    beginOn(id);
    return true;
}

void SketchController::onModelChanged() {
    if (m_mode != Mode::sketching || m_model == nullptr) {
        return;
    }
    if (m_model->stepIndexOfId(m_step) < 0) {
        leave(false); // undone or deleted while sketching: nothing left to leave properly
        return;
    }
    const auto result = m_model->sketchResult(m_step);
    const bool ready = result && result->plane;
    if (ready) {
        m_plane = *result->plane;
        if (!m_cameraTurned && m_viewport != nullptr) {
            m_cameraTurned = true;
            const geo::Point3d& o = m_plane.origin();
            const geo::Point3d& n = m_plane.normal();
            m_viewport->lookAtPlane(QVector3D(static_cast<float>(n.x), static_cast<float>(n.y),
                                              static_cast<float>(n.z)),
                                    QVector3D(static_cast<float>(o.x), static_cast<float>(o.y),
                                              static_cast<float>(o.z)));
        }
    }
    const bool changed = ready != m_planeReady;
    m_planeReady = ready;
    m_elements = m_model->sketchElements(m_step);
    refresh();
    if (changed) {
        emit stateChanged();
    }
}

void SketchController::onPicked(int intent, bool, const QString& body, int kind, int number) {
    if (m_mode != Mode::picking ||
        static_cast<Viewport3D::PickIntent>(intent) != Viewport3D::PickIntent::click) {
        return;
    }
    // Seen in the log of a window test: what the pick pass answered to the click.
    qInfo().noquote() << QStringLiteral(
                             "sketch: click while choosing the plane: body \"%1\", kind %2, "
                             "number %3")
                             .arg(body)
                             .arg(kind)
                             .arg(number);
    if (body.isEmpty()) {
        if (!pickGround()) {
            qInfo() << "sketch: the ground plane could not be used";
        }
    } else if (kind == static_cast<int>(core::SelectableKind::face)) {
        pickFace(body, number);
    }
}

void SketchController::onPointerMoved(double x, double y) {
    if (m_mode != Mode::sketching || !m_planeReady || m_viewport == nullptr) {
        return;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        pointerGone();
        return;
    }
    pointAt({{ray->origin.x(), ray->origin.y(), ray->origin.z()},
             {ray->direction.x(), ray->direction.y(), ray->direction.z()}},
            m_viewport->pixelsPerMillimetre());
}

void SketchController::pointAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre) {
    if (m_mode != Mode::sketching || !m_planeReady) {
        return;
    }
    m_pixelsPerMillimetre = pixelsPerMillimetre;
    const auto onPlane = interaction::planePoint(ray, m_plane);
    if (!onPlane) {
        pointerGone();
        return;
    }
    interaction::SketchSnapSettings settings;
    settings.radiusPixels = kCatchPixels;
    settings.pixelsPerMillimetre = pixelsPerMillimetre;
    settings.gridStep = interaction::sketchGridStep(baseStep(), pixelsPerMillimetre);
    settings.enabled = !snappingSuppressed();
    m_caught = interaction::sketchSnap(*onPlane, m_elements, settings);

    QVariantMap map;
    map[QStringLiteral("valid")] = true;
    map[QStringLiteral("u")] = m_caught->position.u;
    map[QStringLiteral("v")] = m_caught->position.v;
    map[QStringLiteral("kind")] = kindName(m_caught->kind);
    map[QStringLiteral("element")] = QString::fromStdString(m_caught->element);
    map[QStringLiteral("guides")] = static_cast<int>(m_caught->guides.size());
    m_snapMap = map;
    refresh();
    emit snapChanged();
}

void SketchController::pointerGone() {
    if (!m_caught && m_snapMap.isEmpty()) {
        return;
    }
    m_caught.reset();
    m_snapMap = {};
    refresh();
    emit snapChanged();
}

void SketchController::refresh() {
    render::HandleGraphics graphics;
    if (m_mode == Mode::sketching && m_planeReady) {
        const double ppm =
            m_viewport != nullptr ? m_viewport->pixelsPerMillimetre() : m_pixelsPerMillimetre;
        const double spacing = interaction::sketchGridStep(baseStep(), ppm);
        m_gridStep = spacing;

        // The grid: the size of the bed on the ground plane, a fixed size elsewhere.
        const bool ground = m_plane.origin().z == 0.0 && m_plane.normal().z > 0.9999 &&
                            m_plane.origin().x == 0.0 && m_plane.origin().y == 0.0;
        double halfU = kOutsideHalf;
        double halfV = kOutsideHalf;
        if (ground && m_viewport != nullptr) {
            halfU = m_viewport->bedSizeX() / 2.0;
            halfV = m_viewport->bedSizeY() / 2.0;
        }
        if (spacing > 0.0) {
            const int countU = static_cast<int>(std::floor(halfU / spacing));
            const int countV = static_cast<int>(std::floor(halfV / spacing));
            if (countU <= kMaxGridLines && countV <= kMaxGridLines) {
                const double ordered = 10.0;
                for (int i = -countU; i <= countU; ++i) {
                    const bool major = i % static_cast<int>(ordered) == 0;
                    line(graphics, m_plane, {i * spacing, -countV * spacing},
                         {i * spacing, countV * spacing}, kGridWidth,
                         major ? m_gridMajorColour : m_gridColour);
                }
                for (int j = -countV; j <= countV; ++j) {
                    const bool major = j % static_cast<int>(ordered) == 0;
                    line(graphics, m_plane, {-countU * spacing, j * spacing},
                         {countU * spacing, j * spacing}, kGridWidth,
                         major ? m_gridMajorColour : m_gridColour);
                }
            }
        }
        graphics.hiddenAlpha = 0.5F;

        for (const core::SketchElement& element : m_elements) {
            drawElement(graphics, m_plane, element, ppm, m_elementColour);
        }
        if (m_caught) {
            const double dash = kDashPixels / std::max(ppm, 1e-6);
            for (const interaction::SketchGuide& guide : m_caught->guides) {
                dashed(graphics, m_plane, guide.from, guide.to, dash, kGuideWidth, m_guideColour);
            }
            render::HandleDot marker;
            marker.centre = at(m_plane, m_caught->position.u, m_caught->position.v);
            marker.radius = m_caught->kind == interaction::SketchSnapKind::none ? 3.5F : 5.0F;
            marker.fill = m_markerColour;
            marker.ring = QColor(255, 255, 255, 230);
            marker.ringWidth = 1.5F;
            graphics.dots.push_back(marker);
        }
    } else {
        m_gridStep = 0.0;
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::sketch);
    }
}

} // namespace fission::ui
