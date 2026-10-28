// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector3D>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "fission/geo/rotation.h"
#include "fission/interaction/drag.h"
#include "fission/ui/document_model.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

class RotateController;

/// The shape a ring has on the screen, for a mouse area that is only there where the ring is
/// (`containmentMask` of a QML item): the place counts when it is within a few pixels of the line.
class RingMask : public QObject {
    Q_OBJECT
public:
    RingMask(RotateController* owner, int ring);
    Q_INVOKABLE bool contains(const QPointF& point) const;

private:
    RotateController* m_owner;
    int m_ring;
};

/// The tool "Rotate" (AP-1.6b, handle style "Axes"): three rings round the middle of the chosen
/// body, one for each axis of the bed (X red, Y green, Z blue). Grab a ring and go round it; the
/// body turns about that axis, in steps of 15 degrees. Typing replaces dragging: the number is
/// how far to turn in degrees, Tab goes on from X to Y to Z, Enter accepts, Esc puts it back.
///
/// The turn is stored as three angles of the step that made the body (ADR-0010). A ring turns
/// the body about a fixed axis of the bed, so the three values are worked out again from the
/// result (`geo::anglesOf`); only the values that really change are written, and a turn about Z
/// of a body that is not tipped is exactly one value. One drag (or typing, or both) is one step
/// in the history, Esc takes it back.
///
/// Like the gizmo, the rings are drawn in the scene with depth (`Viewport3D::setHandleGraphics`);
/// the window holds the places to grab and the value bubble.
class RotateController : public QObject {
    Q_OBJECT
    /// Shown at all: the tool "Rotate" is on.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// The three rings, always in the order x, y, z (the window keeps one place to grab for each):
    /// [{ part, shown, axis, grabX, grabY }]. (grabX, grabY) is a point on the ring that is easy
    /// to see and take.
    Q_PROPERTY(QVariantList rings READ rings NOTIFY ringsChanged)
    /// A drag or typing is going on.
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    /// The axes the turn is about: "x", "y", "z", or several while typing ("xz"); empty when not
    /// turning.
    Q_PROPERTY(QString turning READ turning NOTIFY editingChanged)
    /// How far the body has turned since the drag began, [x, y, z] in degrees.
    Q_PROPERTY(QVariantList turn READ turn NOTIFY turnChanged)
    /// Where the value bubble goes, in view pixels { x, y }.
    Q_PROPERTY(QVariantMap bubbleAt READ bubbleAt NOTIFY ringsChanged)
    Q_PROPERTY(QVariantMap colours READ colours WRITE setColours NOTIFY coloursChanged)

public:
    explicit RotateController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);
    QVariantList rings() const { return m_rings; }
    bool editing() const noexcept { return m_editing; }
    QString turning() const { return m_turning; }
    QVariantList turn() const;
    QVariantMap bubbleAt() const { return m_bubbleAt; }
    QVariantMap colours() const { return m_colourMap; }
    void setColours(const QVariantMap& colours);

    /// The mask of ring 0, 1 or 2 (x, y, z) for a mouse area. Lives as long as the controller.
    Q_INVOKABLE QObject* mask(int ring);
    /// Whether a place (view pixels) is on a ring.
    bool ringContains(int ring, const QPointF& point) const;

    /// Where on the screen the ring `part` is at `degrees` round it (0 is the start of its frame,
    /// counter-clockwise seen from the tip of the axis), and the angle of the point `grabX/grabY`
    /// of `rings` in the same measure. For tests and the developer option `--simulate`.
    Q_INVOKABLE QPointF ringPoint(const QString& part, double degrees);
    Q_INVOKABLE double grabAngle(const QString& part);

    /// Positions are worked out again (the camera moved, the body changed).
    Q_INVOKABLE void refresh();
    /// The pointer is on a ring (`on`) or left it: the ring is drawn stronger.
    Q_INVOKABLE void setPointed(const QString& part, bool on);

    // --- dragging ---
    /// The pointer went down on ring `part` (x, y or z) at (x, y) in view pixels.
    Q_INVOKABLE bool beginDrag(const QString& part, double x, double y);
    Q_INVOKABLE bool dragTo(double x, double y);
    Q_INVOKABLE bool endDrag();
    Q_INVOKABLE void cancelDrag();

    // --- typing ---
    /// A digit was typed while dragging: the drag turns into typing, still the same step.
    Q_INVOKABLE bool typeInsteadOfDrag();
    /// How far to turn about `axis` (0 = x, 1 = y, 2 = z), in degrees from where the drag began.
    Q_INVOKABLE bool setTurn(int axis, double degrees);
    Q_INVOKABLE bool commitEdit();
    Q_INVOKABLE void cancelEdit();

    /// Rings are at least this many pixels wide (radius), and stand this far off a body.
    static constexpr double kLeastRingPixels = 64.0;
    static constexpr double kRingMargin = 16.0;
    /// How close to the line of a ring (pixels) counts as on it.
    static constexpr double kGrabPixels = 9.0;
    static constexpr int kRingSegments = 96;

signals:
    void enabledChanged();
    void ringsChanged();
    void editingChanged();
    void turnChanged();
    void coloursChanged();

private:
    struct Box {
        QVector3D low;
        QVector3D high;
    };
    std::optional<Box> bodyBox();
    double millimetresPerPixel(const QVector3D& point) const;
    /// The radius of the rings (mm) round `centre`.
    double ringRadius(const QVector3D& centre);
    /// Works the three angles out of the turn so far and gives them to the document.
    bool applyTurn();
    void setEditing(bool editing, const QString& turning);
    void finish();

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    bool m_enabled = false;
    QVariantList m_rings;
    QVariantMap m_bubbleAt;
    QString m_pointed;
    bool m_editing = false;
    bool m_dragging = false;
    QString m_turning;
    struct Colours {
        QColor x{0xE5, 0x53, 0x4B};
        QColor y{0x57, 0xAB, 0x5A};
        QColor z{0x5B, 0x97, 0xFF};
        QColor accent{0x4C, 0x8D, 0xFF};
    } m_colours;
    QVariantMap m_colourMap;

    // The box of the body, kept until its mesh changes.
    std::string m_boxBody;
    std::uint64_t m_boxVersion = 0;
    Box m_box;

    // Each ring as it is on the screen (view pixels), for hits.
    std::array<std::vector<QPointF>, 3> m_screen;
    std::array<RingMask*, 3>
        m_masks{}; ///< children of the controller (not for the script engine to delete)

    // The turn in progress.
    geo::Angles m_startAngles;       ///< the angle values when it began
    geo::Angles m_lastAngles;        ///< what was written last (closest representation is kept)
    QVector3D m_centre;              ///< the point turned about
    double m_radius = 0.0;           ///< of the rings, fixed while dragging
    std::array<double, 3> m_delta{}; ///< how far it has turned about each axis (degrees)
    int m_ring = -1;                 ///< the ring grabbed
    double m_grabbedAt = 0.0;        ///< where on that ring (radians), for the wedge
    std::optional<interaction::RingDrag> m_drag;
};

} // namespace fission::ui
