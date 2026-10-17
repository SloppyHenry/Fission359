// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QVariantList>
#include <QVector3D>

#include <cstdint>
#include <map>
#include <string>

#include "fission/ui/document_model.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// The size handles of the chosen shape (AP-1.5): points on the shape that change one value when
/// dragged (rule 1) and can be typed instead (rule 2). A handle changes the value, never scales the
/// geometry (rule 11): a box with rounded corners that is pulled wider keeps its 5 mm corners.
///
/// The controller turns what the shape says (`FeatureType::handles`, in mm) into screen points,
/// turns a drag of the pointer into a value, and changes the document through `DocumentModel`.
/// Everything between `beginEdit` and `commitEdit` (a drag, or typing with Tab from value to
/// value) is one step in the history; `cancelEdit` (Esc) takes it all back.
class HandleController : public QObject {
    Q_OBJECT
    /// The handles on screen, one for each value, in the order they are shown (view pixels):
    /// [{ index, candidate, parameter, kind, role, axis, x, y, startX, startY, band, bandStartX,
    /// bandStartY, bandEndX, bandEndY, edgeStartX, edgeStartY, edgeEndX, edgeEndY, chipX, chipY,
    /// dirX, dirY, value, pixelsPerMillimetre }].
    /// The dot (x, y) sits on the shape, on the face it moves; (startX, startY) is the other end
    /// of what it measures, and (dirX, dirY) the way it grows on screen (a unit vector). A main
    /// size along an axis has a measure band like in a drawing (`band`): it lies outside the
    /// body, on the bed in front of it or beside its outline, from bandStart to bandEnd, with
    /// lines from the points it measures (edgeStart, edgeEnd). `chipX`, `chipY` is where the
    /// label goes: on the band, or beside the dot when there is none. `role` is size | detail |
    /// rounding, `axis` is x | y | z | none (the colour of the handle). `candidate` is the place
    /// in `DocumentModel::stepHandles`. Empty when nothing is shown.
    Q_PROPERTY(QVariantList handles READ handles NOTIFY handlesChanged)
    /// A drag or typing is going on.
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    /// Digits start typing: a shape was just put down and nothing else was done since.
    Q_PROPERTY(bool armed READ armed WRITE setArmed NOTIFY armedChanged)
    /// The body is small on screen: only the dots are shown, a band and its label when the pointer
    /// is on the dot.
    Q_PROPERTY(bool compact READ compact NOTIFY compactChanged)

public:
    explicit HandleController(DocumentModel* model, QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    QVariantList handles() const { return m_handles; }
    bool editing() const noexcept { return m_editing; }
    bool armed() const noexcept { return m_armed; }
    bool compact() const noexcept { return m_compact; }
    void setArmed(bool armed);

    /// Positions are worked out again (the camera moved, the shape changed).
    Q_INVOKABLE void refresh();

    // --- dragging ---
    /// The pointer went down on handle `index` at (x, y) in view pixels.
    Q_INVOKABLE bool beginDrag(int index, double x, double y);
    /// The pointer moved. Returns the value the handle has now.
    Q_INVOKABLE double dragTo(double x, double y);
    Q_INVOKABLE bool endDrag();
    Q_INVOKABLE void cancelDrag();

    // --- typing ---
    /// A digit was typed while a handle is dragged: the drag turns into typing (rule 2: typing
    /// replaces dragging). The step in the history stays the same one. False if nothing is dragged.
    Q_INVOKABLE bool typeInsteadOfDrag();
    /// Starts changing values by typing (one step in the history until `commitEdit`).
    Q_INVOKABLE bool beginEdit(int index);
    /// Sets the value of handle `index` to what was typed (a number or a formula).
    Q_INVOKABLE bool setValue(int index, const QString& text);
    Q_INVOKABLE bool commitEdit();
    Q_INVOKABLE void cancelEdit();

    /// The step of a value: whole millimetres, finer when zoomed in, coarser when far away.
    static double snapStep(double pixelsPerMillimetre);
    /// No length of a shape is taken beyond this (mm): a slip of the hand must not make a body
    /// the size of a house.
    static constexpr double kLargestMillimetres = 2000.0;
    /// A handle whose axis is closer than this (sine of the angle, 0.5 = 30 degrees) to the line
    /// of sight cannot be dragged: the pointer would have to go ever so far for a little change.
    /// (Typing and turning the view still work.)
    static constexpr double kLeastSight = 0.5;
    /// Below this size on screen (diagonal of the body, pixels) the handles are `compact`.
    static constexpr double kCompactPixels = 120.0;

signals:
    void handlesChanged();
    void editingChanged();
    void armedChanged();
    void compactChanged();

private:
    void setEditing(bool editing);
    /// The box round the body of the current step as last calculated (mm); false if none.
    bool bodyBounds(QVector3D& low, QVector3D& high);

    // Room on screen (pixels): the size of a label, how far a band lies off the body beyond what
    // its label needs, how far a label sits off its dot, the shortest band worth drawing.
    static constexpr double kChipWidth = 76.0;
    static constexpr double kChipHeight = 24.0;
    static constexpr double kBandGap = 10.0;
    static constexpr double kDotGap = 16.0;
    static constexpr double kShortestBand = 14.0;

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    QVariantList m_handles;
    std::map<QString, int> m_chosen; ///< the place of each value while it is being changed
    bool m_editing = false;
    bool m_armed = false;
    bool m_compact = false;
    std::string m_boundsBody; ///< whose box `m_boundsLow`/`m_boundsHigh` are, and of which mesh
    std::uint64_t m_boundsVersion = 0;
    QVector3D m_boundsLow;
    QVector3D m_boundsHigh;
    int m_armedStep = -1;

    // The drag in progress. The pointer is a ray into the scene; it is laid on a plane that holds
    // the axis of the handle and faces the viewer, and the handle follows where the ray meets it.
    bool m_dragging = false;
    int m_dragIndex = -1;       ///< the handle as shown (position in `handles`)
    double m_dragExtent = 0.0;  ///< the length it measured when grabbed
    QVector3D m_dragOrigin;     ///< where the handle sat when it was grabbed (mm)
    QVector3D m_dragAxis;       ///< unit vector along which it moves
    QVector3D m_dragPlane;      ///< normal of the plane the pointer is laid on
    bool m_dragUsable = false;  ///< the axis is not (nearly) along the line of sight
    double m_dragGrabbed = 0.0; ///< where along the axis the pointer was when it grabbed
    double m_dragPixelsPerMillimetre = 1.0;
    double m_dragStartValue = 0.0;
    double m_dragSign = 1.0;
    double m_dragTravel = 1.0;
    double m_dragValue = 0.0;
    QString m_dragParameter;
};

} // namespace fission::ui
