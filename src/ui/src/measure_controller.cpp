// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/ui/measure_controller.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <vector>

#include "fission/analysis/material.h"
#include "fission/core/selection.h"
#include "fission/geo/error.h"
#include "fission/render/scene_renderer.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/snap_session.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

namespace {

constexpr float kLineWidth = 1.5F; // pixels
constexpr float kTickWidth = 1.0F;
constexpr double kTickPixels = 5.0;
constexpr float kHeadLength = 9.0F;
constexpr float kHeadHalfWidth = 4.0F;

QString kindName(geo::ElementKind kind) {
    switch (kind) {
    case geo::ElementKind::corner:
        return QStringLiteral("corner");
    case geo::ElementKind::edge:
        return QStringLiteral("edge");
    case geo::ElementKind::face:
        break;
    }
    return QStringLiteral("face");
}

QVector3D toVector(const geo::Point3d& p) {
    return {static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)};
}

} // namespace

MeasureController::MeasureController(DocumentModel* model, SelectionController* selection,
                                     std::shared_ptr<const geo::GeometryKernel> kernel,
                                     QObject* parent)
    : QObject(parent), m_model(model), m_selection(selection), m_kernel(std::move(kernel)) {
    if (m_selection != nullptr) {
        connect(m_selection, &SelectionController::changed, this, &MeasureController::refresh);
    }
    if (m_model != nullptr) {
        connect(m_model, &DocumentModel::sceneReady, this, [this] { refresh(); });
    }
}

void MeasureController::attach(Viewport3D* viewport) {
    m_viewport = viewport;
    if (m_viewport != nullptr) {
        connect(m_viewport, &Viewport3D::cameraChanged, this, &MeasureController::refresh);
    }
    refresh();
}

void MeasureController::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    emit enabledChanged();
    refresh();
}

void MeasureController::setLineColour(const QColor& colour) {
    if (colour == m_colour) {
        return;
    }
    m_colour = colour;
    emit lineColourChanged();
    refresh();
}

void MeasureController::refresh() {
    measureChoice();
    // The line in the 3D view: between the two nearest points (or the two ends of an edge), with
    // a stroke across each end and an arrow head from each end inwards, like a measure in a
    // drawing.
    render::HandleGraphics graphics;
    if (m_viewport != nullptr && m_enabled && m_span) {
        const QVector3D from = toVector(m_span->at(0));
        const QVector3D to = toVector(m_span->at(1));
        if ((to - from).length() > 1e-6F) {
            const QVector3D middle = (from + to) / 2.0F;
            const double perPixel = millimetresPerPixelAt(*m_viewport, middle);
            QVector3D across = QVector3D::crossProduct(to - from, QVector3D(0, 0, 1));
            if (across.length() < 1e-6F) {
                across = QVector3D::crossProduct(to - from, QVector3D(0, 1, 0));
            }
            across.normalize();
            const QVector3D tick = across * static_cast<float>(kTickPixels * perPixel);
            graphics.lines.push_back({from, to, kLineWidth, m_colour});
            graphics.lines.push_back({from - tick, from + tick, kTickWidth, m_colour});
            graphics.lines.push_back({to - tick, to + tick, kTickWidth, m_colour});
            const QPointF a = m_viewport->project(from.x(), from.y(), from.z());
            const QPointF b = m_viewport->project(to.x(), to.y(), to.z());
            if (std::hypot(b.x() - a.x(), b.y() - a.y()) > 2.5 * kHeadLength) {
                const QVector3D way = (to - from).normalized();
                graphics.heads.push_back({from, -way, kHeadLength, kHeadHalfWidth, m_colour});
                graphics.heads.push_back({to, way, kHeadLength, kHeadHalfWidth, m_colour});
            }
        }
    }
    if (m_viewport != nullptr) {
        m_viewport->setHandleGraphics(std::move(graphics), Viewport3D::HandleSource::measure);
    }
}

void MeasureController::measureChoice() {
    QVariantMap result;
    m_span.reset();
    if (m_enabled && m_selection != nullptr && m_model != nullptr && m_kernel != nullptr) {
        // The last two elements chosen; a body as a whole is nothing to measure here.
        std::vector<geo::ElementRef> elements;
        for (const core::SelectionItem& item : m_selection->selection().items()) {
            if (item.kind == core::SelectableKind::body) {
                continue;
            }
            const auto bodies = m_model->exportBodies({QString::fromStdString(item.body.value)});
            if (bodies.empty()) {
                continue;
            }
            geo::ElementRef ref;
            ref.solid = bodies.front().solid;
            ref.kind = item.kind == core::SelectableKind::vertex ? geo::ElementKind::corner
                       : item.kind == core::SelectableKind::edge ? geo::ElementKind::edge
                                                                 : geo::ElementKind::face;
            ref.number = item.number;
            elements.push_back(std::move(ref));
        }
        if (elements.size() > 2) {
            elements.erase(elements.begin(), elements.end() - 2);
        }
        if (!elements.empty()) {
            try {
                const geo::Measurement found = m_kernel->measureElements(elements);
                result[QStringLiteral("count")] = static_cast<int>(elements.size());
                result[QStringLiteral("first")] = kindName(elements[0].kind);
                if (elements.size() > 1) {
                    result[QStringLiteral("second")] = kindName(elements[1].kind);
                }
                if (found.position) {
                    result[QStringLiteral("x")] = found.position->x;
                    result[QStringLiteral("y")] = found.position->y;
                    result[QStringLiteral("z")] = found.position->z;
                }
                if (found.length) {
                    result[QStringLiteral("length")] = *found.length;
                }
                if (found.area) {
                    result[QStringLiteral("area")] = *found.area;
                }
                if (found.radius) {
                    result[QStringLiteral("radius")] = *found.radius;
                    result[QStringLiteral("diameter")] = 2.0 * *found.radius;
                }
                if (found.distance) {
                    result[QStringLiteral("distance")] = *found.distance;
                }
                if (found.angle) {
                    result[QStringLiteral("angle")] = *found.angle;
                }
                m_span = found.span;
                if (m_span && m_viewport != nullptr) {
                    const QVector3D middle =
                        (toVector(m_span->at(0)) + toVector(m_span->at(1))) / 2.0F;
                    const QPointF screen = m_viewport->project(middle.x(), middle.y(), middle.z());
                    result[QStringLiteral("labelX")] = screen.x();
                    result[QStringLiteral("labelY")] = screen.y();
                }
            } catch (const geo::GeometryError&) {
                result.clear(); // nothing sensible: no kernel text reaches the person
                m_span.reset();
            }
        }
    }
    if (result != m_result) {
        m_result = result;
        emit resultChanged();
    }
}

QVariantMap MeasureController::bodyFacts(const QString& bodyId) const {
    QVariantMap facts;
    facts[QStringLiteral("valid")] = false;
    if (m_model == nullptr || m_kernel == nullptr) {
        return facts;
    }
    const auto bodies = m_model->exportBodies({bodyId});
    if (bodies.empty() || bodies.front().stale) {
        return facts;
    }
    try {
        const geo::SolidReport report = m_kernel->measure(bodies.front().solid);
        facts[QStringLiteral("valid")] = report.valid;
        facts[QStringLiteral("volume")] = report.volume;
        facts[QStringLiteral("area")] = report.area;
        facts[QStringLiteral("sizeX")] = report.boxMax.x - report.boxMin.x;
        facts[QStringLiteral("sizeY")] = report.boxMax.y - report.boxMin.y;
        facts[QStringLiteral("sizeZ")] = report.boxMax.z - report.boxMin.z;
    } catch (const geo::GeometryError&) {
        facts[QStringLiteral("valid")] = false;
    }
    return facts;
}

QVariantList MeasureController::materials() const {
    QVariantList list;
    for (const analysis::Material& material : analysis::materials()) {
        QVariantMap entry;
        entry[QStringLiteral("id")] =
            QString::fromUtf8(material.id.data(), static_cast<qsizetype>(material.id.size()));
        entry[QStringLiteral("density")] = material.density;
        list.push_back(entry);
    }
    return list;
}

QVariantMap MeasureController::weightOf(double volume, double density,
                                        double filamentDiameter) const {
    QVariantMap out;
    out[QStringLiteral("grams")] = analysis::massGrams(volume, density);
    out[QStringLiteral("filamentMetres")] =
        analysis::filamentLengthMm(volume, filamentDiameter) / 1000.0;
    return out;
}

void MeasureController::copyToClipboard(const QString& text) const {
    if (QGuiApplication::clipboard() != nullptr) {
        QGuiApplication::clipboard()->setText(text);
    }
}

} // namespace fission::ui
