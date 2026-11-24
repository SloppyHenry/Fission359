// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

#include "fission/geo/kernel.h"
#include "fission/ui/section_cap_worker.h"

namespace fission::ui {

class DocumentModel;
class Viewport3D;

/// The section view (AP-2.11): a plane cuts the visible bodies away for display. It is a view
/// setting only: no Command, no step of the recipe, no Undo, nothing is saved, and export,
/// measuring and volumes never see it. Turning it on puts the plane through the middle of the
/// box round all visible bodies, perpendicular to X, and cuts away the +X side.
class SectionController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// "x", "y" or "z": the axis the plane is perpendicular to.
    Q_PROPERTY(QString axis READ axis WRITE setAxis NOTIFY planeChanged)
    /// Where the plane cuts the axis (mm, world), kept within [rangeMin, rangeMax].
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY planeChanged)
    /// +1: the side of rising numbers is cut away, -1: the other one.
    Q_PROPERTY(int side READ side NOTIFY planeChanged)
    /// The extent of the visible bodies along the axis (mm).
    Q_PROPERTY(double rangeMin READ rangeMin NOTIFY planeChanged)
    Q_PROPERTY(double rangeMax READ rangeMax NOTIFY planeChanged)

public:
    SectionController(DocumentModel* model, std::shared_ptr<const geo::GeometryKernel> kernel,
                      QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool enabled() const noexcept { return m_enabled; }
    /// Turning on without a visible body does nothing and says `nothingToCut`.
    void setEnabled(bool enabled);
    QString axis() const { return m_axis; }
    void setAxis(const QString& axis);
    double position() const noexcept { return m_position; }
    void setPosition(double position);
    int side() const noexcept { return m_side; }
    double rangeMin() const noexcept { return m_min; }
    double rangeMax() const noexcept { return m_max; }

    /// Cuts away the other side of the plane.
    Q_INVOKABLE void flipSide();
    /// The index of the axis: 0, 1, 2.
    int axisIndex() const noexcept;
    /// For tests: waits for the cut faces to be worked out (they are, in the background).
    bool waitForCaps(int milliseconds) { return m_caps.waitUntilIdle(milliseconds); }
    /// The cut faces of the last finished calculation (null: none yet).
    const std::shared_ptr<const render::CapSet>& caps() const noexcept { return m_caps.caps(); }

signals:
    void enabledChanged();
    void planeChanged();
    /// The section was asked for, but there is no visible body.
    void nothingToCut();

private:
    /// Reads the box round the visible bodies. False if there is none.
    bool measureRange(double (&lo)[3], double (&hi)[3]) const;
    void refresh(bool centre);
    void apply();
    /// Asks for new cut faces; the old ones are not shown meanwhile.
    void requestCaps();

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    SectionCapWorker m_caps;
    bool m_enabled = false;
    QString m_axis = QStringLiteral("x");
    double m_position = 0.0;
    int m_side = 1;
    double m_min = 0.0;
    double m_max = 0.0;
    double m_centre[3] = {0.0, 0.0, 0.0}; ///< middle of the box round the bodies
};

} // namespace fission::ui
