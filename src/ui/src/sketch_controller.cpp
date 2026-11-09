// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/sketch_controller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
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
constexpr float kDraftWidth = 3.5F; // the element being pulled is stronger
constexpr float kGuideWidth = 1.25F;
constexpr double kDashPixels = 6.0;
constexpr double kArcDragPixels = 6.0; // pulling further than this from a line end makes an arc
constexpr int kMaxGridLines = 160;     // per direction
constexpr double kEditPixels = 12.0;   // how near the pointer must be to a piece or an end to trim

QVector3D at(const core::SketchPlane& plane, double u, double v) {
    const geo::Point3d world = plane.toWorld({u, v});
    const geo::Point3d& n = plane.normal();
    return {static_cast<float>(world.x + n.x * kLift), static_cast<float>(world.y + n.y * kLift),
            static_cast<float>(world.z + n.z * kLift)};
}

// More dashes than this on one line are not visible any more; such a line is drawn solid.
constexpr double kMaxDashes = 2000.0;

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

// A chain of lines through `points`. Dashed: strokes of `dash` mm with gaps of the same length,
// counted along the whole chain (not afresh on every short line).
void chain(render::HandleGraphics& out, const core::SketchPlane& plane,
           const std::vector<core::SketchPoint>& points, float width, const QColor& colour,
           bool isDashed, double dash) {
    double travelled = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const core::SketchPoint& a = points[i - 1];
        const core::SketchPoint& b = points[i];
        if (!isDashed) {
            line(out, plane, a, b, width, colour);
            continue;
        }
        const double length = std::hypot(b.u - a.u, b.v - a.v);
        if (!(length > 1e-9)) {
            continue;
        }
        // Too fine to dash (or no dash length at all): drawn solid.
        if (!(dash > 0.0) || length / (2.0 * dash) > kMaxDashes) {
            line(out, plane, a, b, width, colour);
            travelled += length;
            continue;
        }
        const auto along = [&](double at) {
            return core::SketchPoint{a.u + (b.u - a.u) * at / length,
                                     a.v + (b.v - a.v) * at / length};
        };
        // Stroke k covers [2k·dash, 2k·dash + dash] of the whole chain. Counting strokes (instead
        // of stepping by the remaining phase) always moves on: rounding cannot stall the loop.
        const double begin = travelled;
        const double end = travelled + length;
        for (double k = std::floor(begin / (2.0 * dash)); k * 2.0 * dash < end; k += 1.0) {
            const double from = std::max(begin, k * 2.0 * dash);
            const double to = std::min(end, k * 2.0 * dash + dash);
            if (to > from) {
                line(out, plane, along(from - begin), along(to - begin), width, colour);
            }
        }
        travelled = end;
    }
}

// `curve`: the line chain of a spline as the kernel calculated it (null: the points themselves).
void drawElement(render::HandleGraphics& out, const core::SketchPlane& plane,
                 const core::SketchElement& element, double pixelsPerMm, const QColor& colour,
                 float width = kElementWidth,
                 const std::vector<core::SketchPoint>* curve = nullptr) {
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
        dot.radius = width > kElementWidth ? 4.5F : 3.5F;
        dot.fill = colour;
        dot.ring = colour;
        dot.ringWidth = 0.0F;
        out.dots.push_back(dot);
        break;
    }
    case core::SketchElementKind::line:
        if (element.construction) {
            dashed(out, plane, p[0], p[1], dash, width, shown);
        } else {
            line(out, plane, p[0], p[1], width, shown);
        }
        break;
    case core::SketchElementKind::circle: {
        const double radius = std::hypot(p[1].u - p[0].u, p[1].v - p[0].v);
        arcLines(out, plane, p[0], radius, 0.0, 2.0 * std::numbers::pi, pixelsPerMm, width, shown,
                 element.construction, dash);
        break;
    }
    case core::SketchElementKind::arc: {
        const double radius = std::hypot(p[1].u - p[0].u, p[1].v - p[0].v);
        const double a0 = std::atan2(p[1].v - p[0].v, p[1].u - p[0].u);
        double a1 = std::atan2(p[2].v - p[0].v, p[2].u - p[0].u);
        if (a1 <= a0) {
            a1 += 2.0 * std::numbers::pi;
        }
        arcLines(out, plane, p[0], radius, a0, a1, pixelsPerMm, width, shown, element.construction,
                 dash);
        break;
    }
    case core::SketchElementKind::spline:
        chain(out, plane, curve != nullptr && curve->size() >= 2 ? *curve : p, width, shown,
              element.construction, dash);
        break;
    }
}

// Alt is held: the shape being drawn is an aid. (The tests run without a window system.)
bool altHeld() {
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr &&
           (QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier) != 0;
}

const char* toolWord(interaction::SketchToolKind kind) {
    switch (kind) {
    case interaction::SketchToolKind::line:
        return "line";
    case interaction::SketchToolKind::rectangle:
        return "rectangle";
    case interaction::SketchToolKind::circle:
        return "circle";
    case interaction::SketchToolKind::point:
        return "point";
    case interaction::SketchToolKind::arc:
        return "arc";
    case interaction::SketchToolKind::slot:
        return "slot";
    case interaction::SketchToolKind::polygon:
        return "polygon";
    case interaction::SketchToolKind::spline:
        return "spline";
    case interaction::SketchToolKind::trim:
        return "trim";
    case interaction::SketchToolKind::extend:
        return "extend";
    case interaction::SketchToolKind::offset:
        return "offset";
    case interaction::SketchToolKind::fillet:
        return "fillet";
    case interaction::SketchToolKind::mirror:
        return "mirror";
    case interaction::SketchToolKind::pattern:
        return "pattern";
    case interaction::SketchToolKind::takeOver:
        return "takeover";
    case interaction::SketchToolKind::none:
        break;
    }
    return "";
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
    : QObject(parent), m_model(model), m_selection(selection), m_regionWorker(this) {
    connect(&m_regionWorker, &SketchRegionWorker::changed, this,
            &SketchController::onRegionsChanged);
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
    connect(viewport, &Viewport3D::leftClicked, this, &SketchController::onClicked);
    connect(viewport, &Viewport3D::leftDoubleClicked, this, &SketchController::onDoubleClicked);
    connect(viewport, &Viewport3D::leftPressed, this, &SketchController::onPressed);
    connect(viewport, &Viewport3D::leftDragged, this, &SketchController::onDragged);
    connect(viewport, &Viewport3D::leftReleased, this, &SketchController::onReleased);
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
    m_tool.select(interaction::SketchToolKind::none);
    m_tool.setHelperLine(false);
    forgetEdit();
    m_selectedRegion.clear();
    m_hoverRegion.clear();
    m_regionsFailedSaid = false;
    m_regionWorker.reset();
    m_tool.setPolygonSides(interaction::kSketchDefaultSides); // the last number lasts one sketch
    updateDraft();
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
    m_tool.select(interaction::SketchToolKind::none);
    m_tool.setHelperLine(false);
    forgetEdit();
    m_trimmer.reset();
    m_selectedRegion.clear();
    m_hoverRegion.clear();
    m_regionsFailedSaid = false;
    m_regionWorker.reset();
    emit regionsChanged();
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics({}, Viewport3D::HandleSource::sketch);
    }
    if (m_selection != nullptr) {
        m_selection->overrideViewportFilter(std::nullopt);
    }
    if (m_viewport != nullptr) {
        m_viewport->setPickableBodies(std::nullopt);
    }
    if (!m_blocked.empty()) {
        m_blocked.clear();
        emit blockedChanged();
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
    updateDraft();
    emit toolChanged();
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
    // First the element being pulled, then the tool, then the chosen region, then the sketch
    // (decision 10, 13 and 53).
    if (!toolActive() && !m_selectedRegion.empty()) {
        clearRegionSelection();
        return;
    }
    if (toolActive()) {
        if (m_stroking) { // a stroke of trimming is dropped first
            forgetEdit();
            refresh();
            return;
        }
        if (copyTool() && (m_patTyped[0] || m_patTyped[1] || m_second)) {
            // first the typed numbers, then the second step goes back to choosing (decision 41)
            if (m_patTyped[0] || m_patTyped[1]) {
                m_patTyped[0].reset();
                m_patTyped[1].reset();
            } else {
                m_second = false;
                m_anchor.reset();
            }
            m_preview.clear();
            if (m_rawPointer) {
                updateEditHover(*m_rawPointer);
            }
            updateDraft();
            return;
        }
        if ((offsetTool() && m_chain) || m_typedValue) { // a picked chain, a typed number
            if (m_typedValue) {                          // first the typed number, then the chain
                m_typedValue.reset();
            } else {
                m_chain.reset();
            }
            m_preview.clear();
            if (m_rawPointer) {
                updateEditHover(*m_rawPointer);
            }
            updateDraft();
            return;
        }
        if (!m_tool.cancel()) {
            chooseTool(QString());
        }
        updateDraft();
        return;
    }
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
    m_regionWorker.request(m_model->sketchKernelShared(), m_elements);
    m_trimmer.reset(); // the pieces depend on the elements
    if (!m_blocked.empty()) {
        m_blocked.clear();
        emit blockedChanged();
    }
    if (takeOverActive()) {
        applyPickFilter(); // the bodies in front of the sketch may have changed
    }
    m_hoverPiece.reset();
    m_hoverExtension.reset();
    m_chain.reset();
    m_hoverCorner.reset();
    m_pressedCorner.reset();
    m_hoverIds.clear();
    m_preview.clear();
    std::erase_if(m_picked, [this](const std::string& id) {
        return std::ranges::none_of(m_elements,
                                    [&](const core::SketchElement& e) { return e.id == id; });
    });
    if (m_picked.empty()) { // what was chosen is gone (undone)
        m_second = false;
        m_anchor.reset();
    }
    refresh();
    if (changed) {
        emit stateChanged();
    }
}

void SketchController::onPicked(int intent, bool, const QString& body, int kind, int number) {
    if (m_mode == Mode::sketching && takeOverActive() &&
        static_cast<Viewport3D::PickIntent>(intent) == Viewport3D::PickIntent::click) {
        if (!body.isEmpty()) {
            takeOver(body, kind, number);
        }
        if (m_selection != nullptr) {
            m_selection->clear(); // the click took the edge over; it is not "chosen" as well
        }
        return;
    }
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
    const bool curveDrawn =
        toolActive() && m_tool.kind() == interaction::SketchToolKind::spline && m_tool.busy();
    if (curveDrawn || (copyTool() && m_anchor)) {
        // the points of the curve being drawn catch too (the first one closes the curve), and the
        // start of a pattern (its direction lines up like that of a line)
        std::vector<core::SketchElement> withPending = m_elements;
        std::vector<core::SketchPoint> pending;
        if (curveDrawn) {
            pending = m_tool.placedPoints();
        } else {
            pending.push_back(*m_anchor);
        }
        for (const core::SketchPoint& point : pending) {
            core::SketchElement mark;
            mark.kind = core::SketchElementKind::point;
            mark.points = {point};
            withPending.push_back(std::move(mark));
        }
        m_caught = interaction::sketchSnap(*onPlane, withPending, settings);
    } else {
        m_caught = interaction::sketchSnap(*onPlane, m_elements, settings);
    }

    QVariantMap map;
    map[QStringLiteral("valid")] = true;
    map[QStringLiteral("u")] = m_caught->position.u;
    map[QStringLiteral("v")] = m_caught->position.v;
    map[QStringLiteral("kind")] = kindName(m_caught->kind);
    map[QStringLiteral("element")] = QString::fromStdString(m_caught->element);
    map[QStringLiteral("guides")] = static_cast<int>(m_caught->guides.size());
    m_snapMap = map;
    m_tool.moveTo(m_caught->position, altHeld());
    if (editingTool()) {
        updateEditHover(*onPlane);
    }
    if (!toolActive()) {
        hoverRegionAt({onPlane->u, onPlane->v});
    }
    updateDraft(); // draws, too
    emit snapChanged();
}

void SketchController::pointerGone() {
    m_rawPointer.reset();
    if (!m_hoverRegion.empty()) {
        m_hoverRegion.clear();
        emit regionsChanged();
        refresh();
    }
    if (m_hoverPiece || m_hoverExtension || !m_hoverIds.empty() || m_hoverCorner ||
        (!m_chain && !m_preview.empty())) {
        m_hoverPiece.reset();
        m_hoverExtension.reset();
        m_hoverIds.clear();
        if (!m_pressedCorner) {
            m_hoverCorner.reset();
        }
        if (!m_chain) {
            m_preview.clear();
        }
        updateDraft();
    }
    if (!m_caught && m_snapMap.isEmpty()) {
        return;
    }
    m_caught.reset();
    m_snapMap = {};
    refresh();
    emit snapChanged();
}

// The curve of a spline as the kernel calculates it; the last ones are kept, so that a redraw
// (the camera moves) does not ask the kernel again. Null for other elements, and for a curve the
// kernel refuses (then the points themselves are drawn).
const std::vector<core::SketchPoint>*
SketchController::curveOf(const core::SketchElement& element) {
    constexpr std::size_t kKept = 64;
    if (element.kind != core::SketchElementKind::spline || m_model == nullptr ||
        !core::sketchPointsUsable(element.kind, element.points)) {
        return nullptr;
    }
    for (const CachedCurve& cached : m_curves) {
        if (cached.through == element.points) {
            return cached.line.empty() ? nullptr : &cached.line;
        }
    }
    if (m_curves.size() >= kKept) {
        m_curves.erase(m_curves.begin());
    }
    CachedCurve made;
    made.through = element.points;
    if (auto line = m_model->sketchSplineLine(element.points)) {
        made.line = std::move(*line);
    }
    m_curves.push_back(std::move(made));
    return m_curves.back().line.empty() ? nullptr : &m_curves.back().line;
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
        drawRegions(graphics, 0.03);

        for (const core::SketchElement& element : m_elements) {
            drawElement(graphics, m_plane, element, ppm,
                        element.link ? m_linkedColour : m_elementColour, kElementWidth,
                        curveOf(element));
        }
        if (toolActive()) {
            for (const core::SketchElement& element : m_draft.elements) {
                drawElement(graphics, m_plane, element, ppm, m_draftColour, kDraftWidth,
                            curveOf(element));
            }
        }
        if (toolActive() && m_trimmer) {
            // what trimming would take away: the pieces a stroke went over and the one under the
            // pointer
            std::vector<interaction::SketchPiece> shown = m_marked;
            if (m_hoverPiece) {
                shown.push_back(*m_hoverPiece);
            }
            for (const interaction::SketchPiece& piece : shown) {
                const std::vector<core::SketchPoint> outline = m_trimmer->outline(piece);
                for (std::size_t i = 1; i < outline.size(); ++i) {
                    line(graphics, m_plane, outline[i - 1], outline[i], kDraftWidth, m_trimColour);
                }
            }
        }
        if (toolActive() && m_hoverExtension) {
            const std::vector<core::SketchPoint>& way = m_hoverExtension->preview;
            const double dash = kDashPixels / std::max(ppm, 1e-6);
            chain(graphics, m_plane, way, kDraftWidth, m_draftColour, true, dash);
            render::HandleDot target;
            target.centre = at(m_plane, m_hoverExtension->target.u, m_hoverExtension->target.v);
            target.radius = 4.5F;
            target.fill = m_draftColour;
            target.ring = QColor(255, 255, 255, 230);
            target.ringWidth = 1.5F;
            graphics.dots.push_back(target);
        }
        if (offsetTool() || cornerTool() || copyTool()) {
            // what is under the pointer or picked, and what the edit would add
            std::vector<std::string> shown = m_hoverIds;
            shown.insert(shown.end(), m_picked.begin(), m_picked.end());
            if (m_chain) {
                for (const interaction::SketchChainSegment& seg : m_chain->segments) {
                    shown.push_back(seg.element);
                }
            }
            for (const core::SketchElement& element : m_elements) {
                if (std::ranges::find(shown, element.id) != shown.end()) {
                    drawElement(graphics, m_plane, element, ppm, m_draftColour, kDraftWidth,
                                curveOf(element));
                }
            }
            for (const core::SketchElement& element : m_preview) {
                drawElement(graphics, m_plane, element, ppm, m_draftColour, kDraftWidth, nullptr);
            }
            if (copyTool() && m_anchor) {
                render::HandleDot start;
                start.centre = at(m_plane, m_anchor->u, m_anchor->v);
                start.radius = 4.5F;
                start.fill = m_draftColour;
                start.ring = QColor(255, 255, 255, 230);
                start.ringWidth = 1.5F;
                graphics.dots.push_back(start);
            }
            const auto& corner = m_pressedCorner ? m_pressedCorner : m_hoverCorner;
            if (cornerTool() && corner) {
                render::HandleDot mark;
                mark.centre = at(m_plane, corner->at.u, corner->at.v);
                mark.radius = 4.5F;
                mark.fill = m_draftColour;
                mark.ring = QColor(255, 255, 255, 230);
                mark.ringWidth = 1.5F;
                graphics.dots.push_back(mark);
            }
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

// --- regions (AP-2.1g) -------------------------------------------------------------------

void SketchController::drawRegions(render::HandleGraphics& out, double lift) const {
    const interaction::SketchRegionSet& set = m_regionWorker.regions();
    const geo::Point3d& n = m_plane.normal();
    const auto place = [&](const geo::Point2d& p) {
        const geo::Point3d world = m_plane.toWorld({p.x, p.y});
        return QVector3D(static_cast<float>(world.x + n.x * lift),
                         static_cast<float>(world.y + n.y * lift),
                         static_cast<float>(world.z + n.z * lift));
    };
    for (const interaction::SketchRegionInfo& info : set.regions) {
        // light shading; the one under the pointer stronger, the chosen one strongest (decision 53)
        int alpha = 34;
        if (info.id == m_selectedRegion) {
            alpha = 120;
        } else if (info.id == m_hoverRegion && !toolActive()) {
            alpha = 76;
        }
        QColor fill = m_regionColour;
        fill.setAlpha(alpha);
        const std::vector<geo::Point2d>& t = info.shape.triangles;
        for (std::size_t i = 0; i + 2 < t.size(); i += 3) {
            render::HandleTriangle triangle;
            triangle.corners[0] = place(t[i]);
            triangle.corners[1] = place(t[i + 1]);
            triangle.corners[2] = place(t[i + 2]);
            triangle.fill = fill;
            out.fills.push_back(triangle);
        }
    }
}

void SketchController::onRegionsChanged() {
    if (m_mode != Mode::sketching) {
        return;
    }
    const interaction::SketchRegionSet& set = m_regionWorker.regions();
    const auto has = [&set](const std::string& id) {
        return std::ranges::any_of(
            set.regions, [&](const interaction::SketchRegionInfo& info) { return info.id == id; });
    };
    if (!m_selectedRegion.empty() && !has(m_selectedRegion)) {
        m_selectedRegion.clear(); // the region is gone (an element was removed or moved away)
    }
    if (!m_hoverRegion.empty() && !has(m_hoverRegion)) {
        m_hoverRegion.clear();
    }
    if (m_regionWorker.failed() || set.rejected > 0) {
        if (!m_regionsFailedSaid) {
            m_regionsFailedSaid = true;
            emit notice(QStringLiteral("regionsFailed"));
        }
    } else {
        m_regionsFailedSaid = false;
    }
    emit regionsChanged();
    refresh();
}

void SketchController::hoverRegionAt(const core::SketchPoint& at) {
    const interaction::SketchRegionSet& set = m_regionWorker.regions();
    const int index = interaction::regionAt(set, at);
    const std::string id = index >= 0 ? set.regions[static_cast<std::size_t>(index)].id : "";
    if (id != m_hoverRegion) {
        m_hoverRegion = id;
        emit regionsChanged();
    }
}

void SketchController::clickRegion(const interaction::PlaneRay& ray, double pixelsPerMillimetre) {
    pointAt(ray, pixelsPerMillimetre);
    const auto onPlane = interaction::planePoint(ray, m_plane);
    std::string chosen;
    if (onPlane) {
        const interaction::SketchRegionSet& set = m_regionWorker.regions();
        const int index = interaction::regionAt(set, {onPlane->u, onPlane->v});
        if (index >= 0) {
            chosen = set.regions[static_cast<std::size_t>(index)].id;
        }
    }
    if (chosen != m_selectedRegion) { // a click beside every region lets go
        m_selectedRegion = chosen;
        emit regionsChanged();
        refresh();
    }
}

void SketchController::clearRegionSelection() {
    if (!m_selectedRegion.empty()) {
        m_selectedRegion.clear();
        emit regionsChanged();
        refresh();
    }
}

// --- drawing (AP-2.1c1) -----------------------------------------------------------------

QString SketchController::toolName() const {
    return QString::fromLatin1(toolWord(m_tool.kind()));
}

void SketchController::setHelperLine(bool on) {
    if (on == m_tool.helperLine()) {
        return;
    }
    m_tool.setHelperLine(on);
    emit toolChanged();
    updateDraft();
}

void SketchController::toggleHelperLine() {
    if (m_mode == Mode::sketching) {
        setHelperLine(!m_tool.helperLine());
    }
}

void SketchController::chooseTool(const QString& name, int variant) {
    if (m_mode != Mode::sketching) {
        return;
    }
    using interaction::SketchToolKind;
    SketchToolKind kind = SketchToolKind::none;
    for (const SketchToolKind candidate :
         {SketchToolKind::line, SketchToolKind::rectangle, SketchToolKind::circle,
          SketchToolKind::point, SketchToolKind::arc, SketchToolKind::slot, SketchToolKind::polygon,
          SketchToolKind::spline, SketchToolKind::trim, SketchToolKind::extend,
          SketchToolKind::offset, SketchToolKind::fillet, SketchToolKind::mirror,
          SketchToolKind::pattern, SketchToolKind::takeOver}) {
        if (name == QLatin1String(toolWord(candidate))) {
            kind = candidate;
        }
    }
    if (name == QLatin1String("chamfer")) { // the chamfer is the second way of the corner tool
        kind = SketchToolKind::fillet;
        variant = 1;
    }
    int& remembered = m_variants[static_cast<std::size_t>(kind)];
    if (variant >= 0) {
        remembered = variant == 0 ? 0 : 1;
    }
    m_tool.select(kind, remembered);
    forgetEdit();
    applyPickFilter();
    if (!m_blocked.empty()) {
        m_blocked.clear();
        emit blockedChanged();
    }
    emit toolChanged();
    updateDraft();
}

// The tool "takeover" lets the pointer find edges and faces of the bodies in front of the sketch;
// every other tool lets it find nothing but the plane.
void SketchController::applyPickFilter() {
    if (m_selection == nullptr || m_mode != Mode::sketching) {
        return;
    }
    core::SelectionFilter filter;
    filter.bodies = filter.faces = filter.edges = filter.vertices = false;
    std::optional<QStringList> only;
    if (takeOverActive() && m_model != nullptr) {
        filter.faces = filter.edges = true;
        only = m_model->takeableBodies(m_step);
    }
    m_selection->overrideViewportFilter(filter);
    if (m_viewport != nullptr) {
        m_viewport->setPickableBodies(std::move(only));
    }
}

void SketchController::onClicked(double x, double y) {
    if (m_mode != Mode::sketching || !m_planeReady || m_viewport == nullptr) {
        return;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (!ray) {
        return;
    }
    clickAt({{ray->origin.x(), ray->origin.y(), ray->origin.z()},
             {ray->direction.x(), ray->direction.y(), ray->direction.z()}},
            m_viewport->pixelsPerMillimetre(), altHeld());
}

void SketchController::onDoubleClicked(double, double) {
    doubleClick();
}

void SketchController::clickAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre,
                               bool alt) {
    if (m_mode == Mode::sketching && m_planeReady && !toolActive()) {
        clickRegion(ray, pixelsPerMillimetre); // no tool: a click chooses a region (decision 53)
        return;
    }
    if (!toolActive() || !m_planeReady) {
        return;
    }
    pointAt(ray, pixelsPerMillimetre);
    if (!m_caught) {
        return; // not over the plane
    }
    if (editingTool()) { // a click on a piece or an end (a stroke was dropped at the release)
        switch (m_tool.kind()) {
        case interaction::SketchToolKind::trim:
            commitTrim();
            break;
        case interaction::SketchToolKind::offset:
            commitOffset();
            break;
        case interaction::SketchToolKind::fillet:
            commitCorner();
            break;
        case interaction::SketchToolKind::mirror:
        case interaction::SketchToolKind::pattern:
            commitCopy();
            break;
        default:
            commitExtend();
            break;
        }
        return;
    }
    applyStep(m_tool.click(m_caught->position, alt));
}

void SketchController::onPressed(double x, double y) {
    if (m_mode != Mode::sketching || !m_planeReady || m_viewport == nullptr || !toolActive()) {
        return;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (ray) {
        pressAt({{ray->origin.x(), ray->origin.y(), ray->origin.z()},
                 {ray->direction.x(), ray->direction.y(), ray->direction.z()}},
                m_viewport->pixelsPerMillimetre());
    }
}

void SketchController::onDragged(double x, double y) {
    onPointerMoved(x, y);
}

void SketchController::onReleased(double x, double y, bool dragged) {
    if (m_viewport == nullptr) {
        return;
    }
    const auto ray = m_viewport->rayAt(x, y);
    if (ray) {
        releaseAt({{ray->origin.x(), ray->origin.y(), ray->origin.z()},
                   {ray->direction.x(), ray->direction.y(), ray->direction.z()}},
                  m_viewport->pixelsPerMillimetre(), altHeld(), dragged);
    }
    m_viewport->setDrawDrag(false);
}

void SketchController::pressAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre) {
    if (!toolActive() || !m_planeReady) {
        return;
    }
    pointAt(ray, pixelsPerMillimetre);
    if (!m_caught) {
        return;
    }
    if (cornerTool()) { // pressed on a corner and pulled: the radius follows (decision 39)
        m_pressedCorner = m_hoverCorner && !m_hoverCorner->curve ? m_hoverCorner : std::nullopt;
        m_pressAt = m_rawPointer;
        m_cornerDragged = false;
        if (m_viewport != nullptr) {
            m_viewport->setDrawDrag(m_pressedCorner.has_value());
        }
        return;
    }
    if (editingTool()) {
        // trimming: the press starts a stroke over pieces; the release sets it (decision 34)
        const bool trim = m_tool.kind() == interaction::SketchToolKind::trim;
        m_stroking = trim;
        m_marked.clear();
        if (trim && m_hoverPiece) {
            m_marked.push_back(*m_hoverPiece);
        }
        if (m_viewport != nullptr) {
            m_viewport->setDrawDrag(trim);
        }
        refresh();
        return;
    }
    const double ppm = std::max(pixelsPerMillimetre, 1e-6);
    const bool armed = m_tool.press(m_caught->position, kCatchPixels / ppm, kArcDragPixels / ppm);
    if (m_viewport != nullptr) {
        m_viewport->setDrawDrag(armed);
    }
}

void SketchController::dragTo(const interaction::PlaneRay& ray, double pixelsPerMillimetre) {
    if (!toolActive() || !m_planeReady) {
        return;
    }
    pointAt(ray, pixelsPerMillimetre);
}

void SketchController::releaseAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre,
                                 bool alt, bool dragged) {
    if (!toolActive() || !m_planeReady) {
        return;
    }
    pointAt(ray, pixelsPerMillimetre);
    if (cornerTool()) {
        m_cornerDragged = dragged && m_pressedCorner.has_value();
        if (m_cornerDragged) {
            commitCorner();
        }
        m_pressedCorner.reset();
        m_pressAt.reset();
        m_cornerDragged = false;
        if (m_rawPointer) {
            updateEditHover(*m_rawPointer);
        }
        updateDraft();
        return;
    }
    if (editingTool()) {
        if (m_stroking && dragged) {
            commitTrim();
        }
        m_stroking = false; // a stroke that was only a click: the click that follows trims
        m_marked.clear();
        refresh();
        return;
    }
    if (!m_caught) {
        return;
    }
    applyStep(m_tool.release(m_caught->position, alt));
}

void SketchController::doubleClick() {
    if (!toolActive()) {
        return;
    }
    applyStep(m_tool.doubleClick());
}

void SketchController::removeLastPoint() {
    if (!drawing() || !m_tool.undoPoint()) {
        return;
    }
    updateDraft();
}

void SketchController::setTyped(int field, double value) {
    if (patternReadout()) { // count, then distance or angle (AP-2.1e3)
        if (field == 0 || field == 1) {
            if (!std::isfinite(value)) {
                m_patTyped[field].reset();
            } else {
                m_patTyped[field] = field == 1 && !m_tool.variant() ? std::abs(value) : value;
            }
            if (m_rawPointer) {
                updateEditHover(*m_rawPointer);
            }
            updateDraft();
        }
        return;
    }
    if (editDrafting()) { // the distance or radius of an offset or a corner
        if (field == 0) {
            m_typedValue = std::isfinite(value) && value != 0.0
                               ? std::optional<double>(std::abs(value))
                               : std::nullopt;
            if (m_rawPointer) {
                updateEditHover(*m_rawPointer);
            }
            updateDraft();
        }
        return;
    }
    if (!drawing()) {
        return;
    }
    const auto result =
        m_tool.setTyped(field, std::isfinite(value) ? std::optional<double>(value) : std::nullopt);
    if (result == interaction::SketchToolNotice::badSides) {
        emit notice(QStringLiteral("badSides"));
    }
    updateDraft();
}

void SketchController::clearTyped() {
    if (m_patTyped[0] || m_patTyped[1]) {
        m_patTyped[0].reset();
        m_patTyped[1].reset();
        if (m_rawPointer && editingTool()) {
            updateEditHover(*m_rawPointer);
        }
        updateDraft();
    }
    if (m_typedValue) {
        m_typedValue.reset();
        if (m_rawPointer && editingTool()) {
            updateEditHover(*m_rawPointer);
        }
        updateDraft();
    }
    m_tool.setTyped(0, std::nullopt);
    m_tool.setTyped(1, std::nullopt);
    m_tool.setTyped(2, std::nullopt);
    updateDraft();
}

void SketchController::acceptTyped() {
    if (copyTool()) {
        if (!m_second) {
            proceed();
        } else if (patternReadout()) {
            commitCopy();
        }
        return;
    }
    if (editDrafting()) {
        if (offsetTool()) {
            commitOffset();
        } else {
            commitCorner();
        }
        return;
    }
    if (!drawing()) {
        return;
    }
    const std::size_t placedBefore = m_tool.placedCount();
    applyStep(m_tool.accept(altHeld()));
    if (m_tool.placedCount() > placedBefore) {
        emit elementDone(); // the second middle of a slot: the typing of its length is over
    }
}

void SketchController::applyStep(const interaction::SketchToolStep& step) {
    for (const interaction::SketchBatch& batch : step.done) {
        // the recipe names what was drawn: a lone arc is an arc, also when a line tool drew it
        const bool loneArc =
            batch.size() == 1 && batch.front().kind == core::SketchElementKind::arc;
        const QString kind = loneArc ? QStringLiteral("arc") : toolName();
        if (m_model != nullptr && m_model->addSketchElements(m_step, batch, kind)) {
            m_elements = m_model->sketchElements(m_step);
        } else if (std::ranges::any_of(batch, [](const core::SketchElement& element) {
                       return element.kind == core::SketchElementKind::spline;
                   })) {
            emit notice(QStringLiteral("curveFailed")); // the kernel found no curve through them
        }
    }
    using interaction::SketchToolNotice;
    if (step.notice == SketchToolNotice::collinear) {
        emit notice(QStringLiteral("collinear"));
    } else if (step.notice == SketchToolNotice::tooSmall) {
        emit notice(QStringLiteral("tooSmall"));
    } else if (step.notice == SketchToolNotice::badSides) {
        emit notice(QStringLiteral("badSides"));
    }
    updateDraft();
    if (!step.done.empty()) {
        emit elementDone();
    }
}

// --- trimming and extending (AP-2.1e1) --------------------------------------------------

bool SketchController::editingTool() const noexcept {
    return toolActive() && (m_tool.kind() == interaction::SketchToolKind::trim ||
                            m_tool.kind() == interaction::SketchToolKind::extend ||
                            m_tool.kind() == interaction::SketchToolKind::offset ||
                            m_tool.kind() == interaction::SketchToolKind::fillet ||
                            m_tool.kind() == interaction::SketchToolKind::mirror ||
                            m_tool.kind() == interaction::SketchToolKind::pattern);
}

const interaction::SketchTrimmer* SketchController::trimmer() {
    if (!m_trimmer && m_model != nullptr) {
        m_trimmer.emplace(m_model->sketchKernel(), m_elements);
    }
    return m_trimmer ? &*m_trimmer : nullptr;
}

void SketchController::forgetEdit() {
    m_chain.reset();
    m_hoverCorner.reset();
    m_pressedCorner.reset();
    m_pressAt.reset();
    m_typedValue.reset();
    m_hoverIds.clear();
    m_preview.clear();
    m_stroking = false;
    m_marked.clear();
    m_hoverPiece.reset();
    m_hoverExtension.reset();
    forgetCopy();
}

void SketchController::forgetCopy() {
    m_picked.clear();
    m_second = false;
    m_anchor.reset();
    m_patTyped[0].reset();
    m_patTyped[1].reset();
}

void SketchController::updateEditHover(const core::SketchPoint& raw) {
    const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
    if (offsetTool()) {
        updateOffsetHover(raw);
        m_rawPointer = raw;
        return;
    }
    if (cornerTool()) {
        updateCornerHover(raw);
        m_rawPointer = raw;
        return;
    }
    if (copyTool()) {
        m_rawPointer = raw;
        updateCopyHover(raw);
        return;
    }
    if (m_tool.kind() == interaction::SketchToolKind::trim) {
        const interaction::SketchTrimmer* pieces = trimmer();
        if (pieces == nullptr) {
            return;
        }
        m_hoverPiece = pieces->pieceAt(
            raw, reach, m_stroking && !m_marked.empty() ? m_marked.back().element : std::string());
        if (m_stroking) {
            if (m_rawPointer) {
                markAlong(*m_rawPointer, raw);
            }
            if (m_hoverPiece && std::ranges::find(m_marked, *m_hoverPiece) == m_marked.end()) {
                m_marked.push_back(*m_hoverPiece);
            }
        }
    } else if (m_model != nullptr) {
        m_hoverExtension =
            interaction::findExtension(m_model->sketchKernel(), m_elements, raw, reach);
    }
    m_rawPointer = raw;
}

// A stroke goes over every piece between two positions of the pointer, also when the pointer
// jumped over some.
void SketchController::markAlong(const core::SketchPoint& from, const core::SketchPoint& to) {
    const interaction::SketchTrimmer* pieces = trimmer();
    if (pieces == nullptr) {
        return;
    }
    const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
    const double length = std::hypot(to.u - from.u, to.v - from.v);
    const int steps = std::clamp(static_cast<int>(std::ceil(length / (reach * 0.5))), 1, 400);
    for (int i = 1; i <= steps; ++i) {
        const double t = static_cast<double>(i) / steps;
        const auto piece = pieces->pieceAt(
            {from.u + (to.u - from.u) * t, from.v + (to.v - from.v) * t}, reach * 0.5);
        if (piece && std::ranges::find(m_marked, *piece) == m_marked.end()) {
            m_marked.push_back(*piece);
        }
    }
}

void SketchController::commitTrim() {
    const interaction::SketchTrimmer* pieces = trimmer();
    if (pieces == nullptr) {
        return;
    }
    std::vector<interaction::SketchPiece> taken = m_marked;
    if (m_hoverPiece && std::ranges::find(taken, *m_hoverPiece) == taken.end()) {
        taken.push_back(*m_hoverPiece);
    }
    m_stroking = false;
    m_marked.clear();
    if (!pieces->usable()) {
        emit notice(QStringLiteral("editFailed"));
        return;
    }
    if (taken.empty()) {
        return;
    }
    applyEdit(pieces->trim(taken), QStringLiteral("trimSketch"));
}

void SketchController::commitExtend() {
    if (m_model == nullptr || !m_rawPointer) {
        return;
    }
    const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
    bool usable = true;
    const auto found = interaction::findExtension(m_model->sketchKernel(), m_elements,
                                                  *m_rawPointer, reach, &usable);
    if (!usable) {
        emit notice(QStringLiteral("editFailed"));
    } else if (!found) {
        emit notice(QStringLiteral("nothingToExtend"));
    } else {
        applyEdit(interaction::extensionEdit(m_elements, *found), QStringLiteral("extendSketch"));
    }
}

bool SketchController::applyEdit(const interaction::SketchEdit& edit, const QString& action) {
    bool applied = false;
    // An element that follows an edge of a body is not changed (AP-2.1f, decision 48): nothing is
    // done and the hint offers to let go of the link.
    if (const std::vector<std::string> linked = linkedIn(edit); !linked.empty()) {
        m_blocked = linked;
        emit blockedChanged();
        say(QStringLiteral("linked"));
        m_stroking = false;
        m_marked.clear();
        m_hoverPiece.reset();
        updateDraft();
        return false;
    }
    if (edit.notice == interaction::SketchEditNotice::curvePartial) {
        emit notice(QStringLiteral("curvePartial"));
    }
    if (!edit.empty() && m_model != nullptr && m_model->applySketchEdit(m_step, edit, action)) {
        applied = true;
        m_elements = m_model->sketchElements(m_step);
        m_trimmer.reset();
        m_hoverPiece.reset();
        m_hoverExtension.reset();
        m_chain.reset();
        m_hoverCorner.reset();
        m_hoverIds.clear();
        m_preview.clear();
        m_typedValue.reset();
        forgetCopy(); // after a mirroring or a pattern the choosing begins again
        if (m_rawPointer) {
            updateEditHover(*m_rawPointer);
        }
        updateDraft();
        emit elementDone();
    }
    return applied;
}

// --- offsetting and corners (AP-2.1e2) --------------------------------------------------

std::vector<std::string> SketchController::linkedIn(const interaction::SketchEdit& edit) const {
    std::vector<std::string> ids;
    const auto note = [&](const std::string& id) {
        const auto found = std::ranges::find_if(
            m_elements, [&](const core::SketchElement& e) { return e.id == id; });
        if (found != m_elements.end() && found->link && std::ranges::find(ids, id) == ids.end()) {
            ids.push_back(id);
        }
    };
    for (const core::SketchPointMove& move : edit.moves) {
        note(move.element);
    }
    for (const std::string& removed : edit.removed) {
        note(removed);
    }
    return ids;
}

bool SketchController::takeOver(const QString& body, int kind, int number) {
    if (!takeOverActive() || !m_planeReady || m_model == nullptr) {
        return false;
    }
    const bool face = kind == static_cast<int>(core::SelectableKind::face);
    if (!face && kind != static_cast<int>(core::SelectableKind::edge)) {
        return false;
    }
    const auto result = m_model->takeOverEdges(m_step, body, face, number);
    if (!result) {
        say(QStringLiteral("cannotTake"));
        return false;
    }
    if (result->taken == 0) {
        say(QString::fromLatin1(result->already > 0 ? "edgeTaken" : "cannotTake"));
        return false;
    }
    m_elements = m_model->sketchElements(m_step);
    m_trimmer.reset();
    updateDraft();
    emit elementDone();
    if (result->skipped > 0) {
        say(QStringLiteral("edgesSome"));
    }
    return true;
}

bool SketchController::takeOverNamed(const QString& name) {
    if (!takeOverActive() || !m_planeReady || m_model == nullptr) {
        return false;
    }
    // the same path as a click, by name
    const std::size_t before = m_elements.size();
    const auto result = m_model->takeOverNamed(m_step, name);
    if (!result || result->taken == 0) {
        say(QString::fromLatin1(result && result->already > 0 ? "edgeTaken" : "cannotTake"));
        return false;
    }
    m_elements = m_model->sketchElements(m_step);
    m_trimmer.reset();
    updateDraft();
    emit elementDone();
    if (result->skipped > 0) {
        say(QStringLiteral("edgesSome"));
    }
    return m_elements.size() > before;
}

bool SketchController::releaseLinks() {
    if (m_blocked.empty() || m_model == nullptr) {
        return false;
    }
    const std::vector<std::string> ids = std::move(m_blocked);
    m_blocked.clear();
    emit blockedChanged();
    const bool done = m_model->unlinkSketchElements(m_step, ids);
    if (done) {
        m_elements = m_model->sketchElements(m_step);
        m_trimmer.reset();
        updateDraft();
    }
    return done;
}

void SketchController::say(const QString& code, double number) {
    m_noticeNumber = number;
    emit noticeNumberChanged();
    emit notice(code);
}

// Before a chain is picked: the chain under the pointer shows. After: the offset follows the
// pointer (the side it is on, the distance from the chain), or the typed distance.
void SketchController::updateOffsetHover(const core::SketchPoint& raw) {
    m_preview.clear();
    m_hoverIds.clear();
    if (!m_chain) {
        const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
        if (const auto id = interaction::elementNear(m_elements, raw, reach)) {
            if (const auto chain = interaction::findChain(m_elements, *id)) {
                if (chain->curve) {
                    m_hoverIds.push_back(*id);
                }
                for (const interaction::SketchChainSegment& seg : chain->segments) {
                    m_hoverIds.push_back(seg.element);
                }
            }
        }
        return;
    }
    const double side = interaction::chainSignedDistance(*m_chain, raw);
    const double sign = side < 0.0 ? -1.0 : 1.0;
    const double distance = m_typedValue ? *m_typedValue : std::abs(side);
    m_editValue = distance;
    if (m_model != nullptr && distance >= interaction::kSketchMinimumSize) {
        m_preview =
            interaction::offsetChain(m_model->sketchKernel(), *m_chain, sign * distance).edit.added;
    }
}

double SketchController::cornerValue(const interaction::SketchCorner& corner,
                                     const core::SketchPoint& raw) {
    if (m_typedValue) {
        return *m_typedValue;
    }
    if (m_pressedCorner && m_pressAt) {
        const double moved = std::hypot(raw.u - m_pressAt->u, raw.v - m_pressAt->v) *
                             std::max(m_pixelsPerMillimetre, 1e-6);
        if (m_cornerDragged || moved > 6.0) {
            return chamfering() ? interaction::cornerDistanceAt(m_elements, corner, raw)
                                : interaction::cornerRadiusAt(m_elements, corner, raw);
        }
    }
    return chamfering() ? m_lastChamfer : m_lastRadius;
}

void SketchController::updateCornerHover(const core::SketchPoint& raw) {
    m_preview.clear();
    if (!m_pressedCorner) {
        const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
        auto found = interaction::findCorner(m_elements, raw, reach);
        const bool same = found && m_hoverCorner && found->first == m_hoverCorner->first &&
                          found->second == m_hoverCorner->second &&
                          found->firstAtEnd == m_hoverCorner->firstAtEnd &&
                          found->secondAtEnd == m_hoverCorner->secondAtEnd;
        if (!same) {
            m_typedValue.reset(); // a number is for one corner
        }
        m_hoverCorner = found;
    }
    const auto& corner = m_pressedCorner ? m_pressedCorner : m_hoverCorner;
    if (!corner || corner->curve) {
        return;
    }
    const double value = cornerValue(*corner, raw);
    m_editValue = value;
    const auto result = chamfering() ? interaction::chamferCorner(m_elements, *corner, value)
                                     : interaction::roundCorner(m_elements, *corner, value);
    if (result.notice == interaction::SketchCornerNotice::none) {
        m_preview = result.edit.added;
    }
}

void SketchController::commitOffset() {
    if (m_model == nullptr || !m_rawPointer) {
        return;
    }
    if (!m_chain) { // the first click picks the chain
        const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
        const auto id = interaction::elementNear(m_elements, *m_rawPointer, reach);
        const auto chain = id ? interaction::findChain(m_elements, *id) : std::nullopt;
        if (!chain) {
            return;
        }
        if (chain->curve) {
            say(QStringLiteral("curveOffset"));
            return;
        }
        m_chain = chain;
        m_typedValue.reset();
        updateEditHover(*m_rawPointer);
        updateDraft();
        return;
    }
    const double side = interaction::chainSignedDistance(*m_chain, *m_rawPointer);
    const double distance =
        (side < 0.0 ? -1.0 : 1.0) * (m_typedValue ? *m_typedValue : std::abs(side));
    if (std::abs(distance) < interaction::kSketchMinimumSize) {
        return; // a click on the chain itself
    }
    const auto result = interaction::offsetChain(m_model->sketchKernel(), *m_chain, distance);
    switch (result.notice) {
    case interaction::SketchOffsetNotice::none:
        applyEdit(result.edit, QStringLiteral("offsetSketch"));
        break;
    case interaction::SketchOffsetNotice::curve:
        say(QStringLiteral("curveOffset"));
        break;
    case interaction::SketchOffsetNotice::tooBig:
        say(QStringLiteral("offsetTooBig"));
        break;
    case interaction::SketchOffsetNotice::crossing:
        say(QStringLiteral("offsetCrossing"));
        break;
    case interaction::SketchOffsetNotice::failed:
        say(QStringLiteral("editFailed"));
        break;
    }
}

void SketchController::commitCorner() {
    if (m_model == nullptr || !m_rawPointer) {
        return;
    }
    const auto& corner = m_pressedCorner ? m_pressedCorner : m_hoverCorner;
    if (!corner) {
        say(QStringLiteral("noCorner"));
        return;
    }
    if (corner->curve) {
        say(QStringLiteral("curveCorner"));
        return;
    }
    const double value = cornerValue(*corner, *m_rawPointer);
    const bool chamfer = chamfering();
    const auto result = chamfer ? interaction::chamferCorner(m_elements, *corner, value)
                                : interaction::roundCorner(m_elements, *corner, value);
    switch (result.notice) {
    case interaction::SketchCornerNotice::none:
        if (applyEdit(result.edit,
                      chamfer ? QStringLiteral("chamferCorner") : QStringLiteral("roundCorner"))) {
            (chamfer ? m_lastChamfer : m_lastRadius) = value;
        }
        break;
    case interaction::SketchCornerNotice::curve:
        say(QStringLiteral("curveCorner"));
        break;
    case interaction::SketchCornerNotice::noCorner:
        say(QStringLiteral("notACorner"));
        break;
    case interaction::SketchCornerNotice::tooSmall:
        say(QStringLiteral("tooSmall"));
        break;
    case interaction::SketchCornerNotice::tooBig:
        say(chamfer ? QStringLiteral("chamferTooBig") : QStringLiteral("cornerTooBig"),
            result.limit);
        break;
    case interaction::SketchCornerNotice::failed:
        say(QStringLiteral("editFailed"));
        break;
    }
}

// --- mirroring and patterns (AP-2.1e3) --------------------------------------------------

// The elements one click chooses: the whole chain of a line, arc or circle; a curve or a point
// alone.
std::vector<std::string> SketchController::chainIds(const std::string& id) const {
    std::vector<std::string> ids;
    if (const auto chain = interaction::findChain(m_elements, id)) {
        for (const interaction::SketchChainSegment& seg : chain->segments) {
            ids.push_back(seg.element);
        }
    }
    if (ids.empty()) {
        ids.push_back(id);
    }
    return ids;
}

std::optional<std::string> SketchController::axisNear(const core::SketchPoint& raw) const {
    const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
    const auto id = interaction::elementNear(m_elements, raw, reach);
    if (!id) {
        return std::nullopt;
    }
    const auto element =
        std::ranges::find_if(m_elements, [&](const core::SketchElement& e) { return e.id == *id; });
    if (element == m_elements.end() || element->kind != core::SketchElementKind::line) {
        return std::nullopt;
    }
    return id;
}

// The pattern the pointer and the typed numbers make (the pointer is where it catches). Also sets
// the numbers shown in the bubble.
interaction::SketchCopyResult SketchController::patternResult(const core::SketchPoint& pointer) {
    const std::size_t way = m_tool.variant() == 0 ? 0 : 1;
    const double typedCount = m_patTyped[0] ? std::clamp(std::round(*m_patTyped[0]), -1.0, 1000.0)
                                            : static_cast<double>(m_patCount[way]);
    const int count = static_cast<int>(typedCount);
    m_shownCount = count;
    if (way == 0) {
        core::SketchPoint step{pointer.u - m_anchor->u, pointer.v - m_anchor->v};
        const double length = std::hypot(step.u, step.v);
        double shown = length;
        if (m_patTyped[1]) {
            shown = *m_patTyped[1];
            if (length > 1e-9) {
                step = {step.u / length * shown, step.v / length * shown};
            } else {
                step = {0.0, 0.0};
            }
        }
        m_editValue = shown;
        return interaction::linearPattern(m_elements, m_picked, step, count);
    }
    const double angle = m_patTyped[1] ? *m_patTyped[1] : 360.0;
    m_editValue = angle;
    return interaction::circularPattern(m_elements, m_picked, *m_anchor, count, angle);
}

// Choosing: the chain under the pointer shows. Second step: the axis under the pointer and what
// would be made, or the pattern as the pointer and the numbers make it.
void SketchController::updateCopyHover(const core::SketchPoint& raw) {
    m_preview.clear();
    m_hoverIds.clear();
    if (!m_second) {
        const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
        if (const auto id = interaction::pickElementNear(m_elements, raw, reach)) {
            m_hoverIds = chainIds(*id);
        }
        return;
    }
    if (m_tool.kind() == interaction::SketchToolKind::mirror) {
        if (const auto axis = axisNear(raw)) {
            m_hoverIds.push_back(*axis);
            const auto result = interaction::mirrorElements(m_elements, m_picked, *axis);
            if (result.notice == interaction::SketchCopyNotice::none) {
                m_preview = result.edit.added;
            }
        }
        return;
    }
    if (m_anchor) {
        const core::SketchPoint pointer = m_caught ? m_caught->position : raw;
        const auto result = patternResult(pointer);
        if (result.notice == interaction::SketchCopyNotice::none) {
            m_preview = result.edit.added;
        }
    }
}

void SketchController::proceed() {
    if (!copyTool() || m_second || m_picked.empty()) {
        return;
    }
    m_second = true;
    m_anchor.reset();
    m_patTyped[0].reset();
    m_patTyped[1].reset();
    if (m_rawPointer) {
        updateEditHover(*m_rawPointer);
    }
    updateDraft();
}

void SketchController::applyCopy(const interaction::SketchCopyResult& result,
                                 const QString& action) {
    using interaction::SketchCopyNotice;
    switch (result.notice) {
    case SketchCopyNotice::none: {
        const bool pattern = patternTool();
        const std::size_t way = m_tool.variant() == 0 ? 0 : 1;
        const int count = m_shownCount;
        if (applyEdit(result.edit, action) && pattern) {
            m_patCount[way] = count; // the count used last
        }
        break;
    }
    case SketchCopyNotice::noAxis:
        say(QStringLiteral("mirrorAxis"));
        break;
    case SketchCopyNotice::tooMany:
        say(QStringLiteral("patternTooMany"));
        break;
    case SketchCopyNotice::nothing:
        say(QStringLiteral("patternNothing"));
        break;
    case SketchCopyNotice::nothingNew:
    case SketchCopyNotice::noSelection:
        say(QStringLiteral("copiesNone"));
        break;
    }
}

void SketchController::commitCopy() {
    if (m_model == nullptr || !m_rawPointer) {
        return;
    }
    if (!m_second) { // choosing: a click takes a chain in or out
        const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
        const auto id = interaction::pickElementNear(m_elements, *m_rawPointer, reach);
        if (!id) {
            return;
        }
        const std::vector<std::string> ids = chainIds(*id);
        if (std::ranges::find(m_picked, *id) != m_picked.end()) {
            std::erase_if(m_picked, [&](const std::string& other) {
                return std::ranges::find(ids, other) != ids.end();
            });
        } else {
            for (const std::string& other : ids) {
                if (std::ranges::find(m_picked, other) == m_picked.end()) {
                    m_picked.push_back(other);
                }
            }
        }
        updateEditHover(*m_rawPointer);
        updateDraft();
        return;
    }
    if (m_tool.kind() == interaction::SketchToolKind::mirror) {
        const auto axis = axisNear(*m_rawPointer);
        if (!axis) {
            say(QStringLiteral("mirrorAxis"));
            return;
        }
        applyCopy(interaction::mirrorElements(m_elements, m_picked, *axis),
                  QStringLiteral("mirrorSketch"));
        return;
    }
    const core::SketchPoint pointer = m_caught ? m_caught->position : *m_rawPointer;
    if (!m_anchor) { // the first click of the second step: the start of the row, the middle
        m_anchor = pointer;
        updateEditHover(*m_rawPointer);
        updateDraft();
        return;
    }
    applyCopy(patternResult(pointer), QStringLiteral("patternSketch"));
}

// The element being pulled: its numbers for the view, and the drawing.
void SketchController::updateDraft() {
    m_draft = toolActive() ? m_tool.draft() : interaction::SketchDraft{};
    QVariantMap map;
    map[QStringLiteral("valid")] = false;
    if (toolActive() && m_tool.busy() && m_planeReady) {
        QVariantList fields;
        for (const interaction::SketchReadoutField& field : m_draft.fields) {
            QVariantMap entry;
            entry[QStringLiteral("name")] = QString::fromStdString(field.name);
            entry[QStringLiteral("value")] = field.value;
            entry[QStringLiteral("typed")] = field.typed;
            fields << entry;
        }
        map[QStringLiteral("valid")] = true;
        map[QStringLiteral("fields")] = fields;
        map[QStringLiteral("u")] = m_draft.at.u;
        map[QStringLiteral("v")] = m_draft.at.v;
        if (m_viewport != nullptr) {
            const geo::Point3d world = m_plane.toWorld(m_draft.at);
            const QPointF screen = m_viewport->project(world.x, world.y, world.z);
            map[QStringLiteral("x")] = screen.x();
            map[QStringLiteral("y")] = screen.y();
        }
    }
    if (editDrafting() && m_planeReady) {
        QVariantList fields;
        QVariantMap entry;
        if (patternReadout()) {
            entry[QStringLiteral("name")] = QStringLiteral("count");
            entry[QStringLiteral("value")] = m_shownCount;
            entry[QStringLiteral("typed")] = m_patTyped[0].has_value();
            fields << entry;
            entry[QStringLiteral("name")] =
                m_tool.variant() == 0 ? QStringLiteral("distance") : QStringLiteral("angle");
            entry[QStringLiteral("value")] = m_editValue;
            entry[QStringLiteral("typed")] = m_patTyped[1].has_value();
        } else {
            entry[QStringLiteral("name")] = cornerTool() && !chamfering()
                                                ? QStringLiteral("radius")
                                                : QStringLiteral("distance");
            entry[QStringLiteral("value")] = m_editValue;
            entry[QStringLiteral("typed")] = m_typedValue.has_value();
        }
        fields << entry;
        const core::SketchPoint where = m_rawPointer ? *m_rawPointer : core::SketchPoint{};
        map[QStringLiteral("valid")] = true;
        map[QStringLiteral("fields")] = fields;
        map[QStringLiteral("u")] = where.u;
        map[QStringLiteral("v")] = where.v;
        if (m_viewport != nullptr) {
            const geo::Point3d world = m_plane.toWorld(where);
            const QPointF screen = m_viewport->project(world.x, world.y, world.z);
            map[QStringLiteral("x")] = screen.x();
            map[QStringLiteral("y")] = screen.y();
        }
    }
    m_readout = map;
    refresh();
    emit draftChanged();
}

} // namespace fission::ui
