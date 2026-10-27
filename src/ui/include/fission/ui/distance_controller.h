// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/geo/solid.h"
#include "fission/interaction/distances.h"
#include "fission/ui/document_model.h"
#include "fission/ui/gizmo_controller.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// The distance measures of the tool "Move" (AP-1.6d, docs/02-ux-konzept.md §2.3): measure bands
/// from the chosen body to the nearest body in each direction of the bed (left, right, front,
/// back) or, where there is none, to the edge of the bed, edge to edge. The numbers run along
/// while the body is moved; a click on one makes it a field: type `5` and the body stands 5 mm
/// from there. That is a move of the body, like any other: one step in the history ("Box
/// moved"), values changed and no more.
///
/// The bands are drawn in the scene with depth (`Viewport3D::setHandleGraphics`); the window holds
/// the numbers to click. A measure is not a bond: it moves the body once and does not hold the
/// distance when the other body changes.
class DistanceController : public QObject {
    Q_OBJECT
    /// Shown at all: the tool "Move" is on (and a body that can be moved is chosen).
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// The numbers to show, in view pixels: [{ index, side, x, y, value, toBed }]. `side` is left |
    /// right | front | back, (x, y) the middle of the band, `value` the distance in mm, `toBed`
    /// says it is measured to the edge of the bed. Empty when nothing is shown.
    Q_PROPERTY(QVariantList labels READ labels NOTIFY labelsChanged)
    /// The colour of the bands, from the theme: { line }.
    Q_PROPERTY(QVariantMap colours READ colours WRITE setColours NOTIFY coloursChanged)

public:
    explicit DistanceController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);
    /// The bands follow a body while the gizmo moves it, ahead of the calculation.
    void attachGizmo(GizmoController* gizmo);

    bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);
    QVariantList labels() const { return m_labels; }
    QVariantMap colours() const { return m_colourMap; }
    void setColours(const QVariantMap& colours);

    /// Positions are worked out again (the camera moved, the scene changed).
    Q_INVOKABLE void refresh();
    /// The pointer is on number `index` (`on`) or left it: its band is drawn stronger.
    Q_INVOKABLE void setPointed(int index, bool on);
    /// Moves the chosen body so that the measure `index` (place in `labels`) becomes
    /// `millimetres`. One step in the history. False (nothing changes) for a number below 0, a
    /// measure that is not there any more, or while the body is being moved or typed.
    Q_INVOKABLE bool setDistance(int index, double millimetres);
    /// The place in `labels` of the measure of a side (left | right | front | back), or -1.
    Q_INVOKABLE int indexOfSide(const QString& side) const;

    /// Half the stroke across the end of a band, and the arrow heads (pixels).
    static constexpr double kTickPixels = 5.0;
    static constexpr float kHeadLength = 9.0F;
    static constexpr float kHeadHalfWidth = 4.0F;

signals:
    void enabledChanged();
    void labelsChanged();
    void coloursChanged();

private:
    /// The box round the chosen body now (mm); empty if it cannot be moved or is not calculated.
    std::optional<interaction::Box3> moverBox();
    /// The boxes round the other bodies (visible solids, without the chosen one).
    std::vector<std::pair<std::string, interaction::Box3>> otherBoxes();
    /// The measures as they are now; empty when nothing is shown.
    std::vector<interaction::Gap> measure();

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    QPointer<GizmoController> m_gizmo;
    bool m_enabled = false;
    QVariantList m_labels;
    int m_pointed = -1;
    struct Colours {
        QColor line{0xB8, 0xBC, 0xC4};
    } m_colours;
    QVariantMap m_colourMap;
    std::vector<interaction::Gap> m_gaps; ///< what `m_labels` shows

    // The exact box round the chosen body, kept until the result of the calculation changes. While
    // the gizmo moves the body it stays as it was and the move so far is added to it.
    std::optional<geo::Bounds> m_bounds;
    std::shared_ptr<const void> m_boundsResult;
    int m_boundsStep = -1;
    bool m_following = false;

    struct Cached {
        std::uint64_t version = 0;
        interaction::Box3 box;
        bool valid = false;
    };
    std::map<std::string, Cached> m_boxes; ///< by body and mesh
};

} // namespace fission::ui
