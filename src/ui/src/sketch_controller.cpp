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
    // First the element being pulled, then the tool, then the sketch (decision 10 and 13).
    if (toolActive()) {
        if (m_stroking) { // a stroke of trimming is dropped first
            forgetEdit();
            refresh();
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
    m_trimmer.reset(); // the pieces depend on the elements
    m_hoverPiece.reset();
    m_hoverExtension.reset();
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
    if (toolActive() && m_tool.kind() == interaction::SketchToolKind::spline && m_tool.busy()) {
        // the points of the curve being drawn catch too (the first one closes the curve)
        std::vector<core::SketchElement> withPending = m_elements;
        for (const core::SketchPoint& point : m_tool.placedPoints()) {
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
    updateDraft(); // draws, too
    emit snapChanged();
}

void SketchController::pointerGone() {
    m_rawPointer.reset();
    if (m_hoverPiece || m_hoverExtension) {
        m_hoverPiece.reset();
        m_hoverExtension.reset();
        refresh();
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

        for (const core::SketchElement& element : m_elements) {
            drawElement(graphics, m_plane, element, ppm, m_elementColour, kElementWidth,
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
          SketchToolKind::spline, SketchToolKind::trim, SketchToolKind::extend}) {
        if (name == QLatin1String(toolWord(candidate))) {
            kind = candidate;
        }
    }
    int& remembered = m_variants[static_cast<std::size_t>(kind)];
    if (variant >= 0) {
        remembered = variant == 0 ? 0 : 1;
    }
    m_tool.select(kind, remembered);
    forgetEdit();
    emit toolChanged();
    updateDraft();
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
    if (!toolActive() || !m_planeReady) {
        return;
    }
    pointAt(ray, pixelsPerMillimetre);
    if (!m_caught) {
        return; // not over the plane
    }
    if (editingTool()) { // a click on a piece or an end (a stroke was dropped at the release)
        if (m_tool.kind() == interaction::SketchToolKind::trim) {
            commitTrim();
        } else {
            commitExtend();
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
    m_tool.setTyped(0, std::nullopt);
    m_tool.setTyped(1, std::nullopt);
    m_tool.setTyped(2, std::nullopt);
    updateDraft();
}

void SketchController::acceptTyped() {
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
                            m_tool.kind() == interaction::SketchToolKind::extend);
}

const interaction::SketchTrimmer* SketchController::trimmer() {
    if (!m_trimmer && m_model != nullptr) {
        m_trimmer.emplace(m_model->sketchKernel(), m_elements);
    }
    return m_trimmer ? &*m_trimmer : nullptr;
}

void SketchController::forgetEdit() {
    m_stroking = false;
    m_marked.clear();
    m_hoverPiece.reset();
    m_hoverExtension.reset();
}

void SketchController::updateEditHover(const core::SketchPoint& raw) {
    const double reach = kEditPixels / std::max(m_pixelsPerMillimetre, 1e-6);
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

void SketchController::applyEdit(const interaction::SketchEdit& edit, const QString& action) {
    if (edit.notice == interaction::SketchEditNotice::curvePartial) {
        emit notice(QStringLiteral("curvePartial"));
    }
    if (!edit.empty() && m_model != nullptr && m_model->applySketchEdit(m_step, edit, action)) {
        m_elements = m_model->sketchElements(m_step);
        m_trimmer.reset();
        m_hoverPiece.reset();
        m_hoverExtension.reset();
        if (m_rawPointer) {
            updateEditHover(*m_rawPointer);
        }
        updateDraft();
        emit elementDone();
    }
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
    m_readout = map;
    refresh();
    emit draftChanged();
}

} // namespace fission::ui
