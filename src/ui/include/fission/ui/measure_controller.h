// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <memory>

#include "fission/geo/kernel.h"

namespace fission::ui {

class DocumentModel;
class SelectionController;
class Viewport3D;

/// The tool "Measure" and the facts about a body (AP-2.11). While it is on, what is chosen in the
/// 3D view (one or two faces, edges or corners) is measured: the number and its line are shown
/// in the view and the card holds the numbers, ready to copy. Body facts (volume, surface, box,
/// weight, filament) are asked for by the panel. A measure changes nothing in the document.
class MeasureController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// What was found, empty when nothing can be measured: { count, first, second (face | edge |
    /// corner), x, y, z (a corner), length, area, radius, diameter, distance, angle (only what
    /// applies), labelX, labelY (where the number sits, view pixels) }.
    Q_PROPERTY(QVariantMap result READ result NOTIFY resultChanged)
    /// The colour of the measure line, from the theme.
    Q_PROPERTY(QColor lineColour READ lineColour WRITE setLineColour NOTIFY lineColourChanged)

public:
    MeasureController(DocumentModel* model, SelectionController* selection,
                      std::shared_ptr<const geo::GeometryKernel> kernel, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);
    QVariantMap result() const { return m_result; }
    QColor lineColour() const { return m_colour; }
    void setLineColour(const QColor& colour);

    /// Measures the choice again (the camera moved, the document changed).
    Q_INVOKABLE void refresh();

    /// The facts about a body: { valid, volume (mm³), area (mm²), sizeX, sizeY, sizeZ (mm) }; just
    /// { valid: false } for a body that has no geometry now (hidden, a hole, a failed step).
    Q_INVOKABLE QVariantMap bodyFacts(const QString& bodyId) const;
    /// The materials, in the order shown: [{ id, density }].
    Q_INVOKABLE QVariantList materials() const;
    /// The most it weighs and the filament it takes: { grams, filamentMetres }. `density` in
    /// g/cm³, `filamentDiameter` in mm.
    Q_INVOKABLE QVariantMap weightOf(double volume, double density, double filamentDiameter) const;
    /// Puts text on the system clipboard (the card's "Copy").
    Q_INVOKABLE void copyToClipboard(const QString& text) const;

signals:
    void enabledChanged();
    void resultChanged();
    void lineColourChanged();

private:
    void measureChoice();

    QPointer<DocumentModel> m_model;
    QPointer<SelectionController> m_selection;
    QPointer<Viewport3D> m_viewport;
    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    bool m_enabled = false;
    QVariantMap m_result;
    QColor m_colour{0xB8, 0xBC, 0xC4};
    std::optional<std::array<geo::Point3d, 2>> m_span; ///< the line of `m_result`
};

} // namespace fission::ui
