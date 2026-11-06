// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <optional>
#include <vector>

#include "fission/core/sketch.h"
#include "fission/interaction/sketch_snap.h"
#include "fission/render/camera.h"

namespace fission::ui {

class DocumentModel;
class SelectionController;
class Viewport3D;

/// The sketch mode (AP-2.1b, ADR-0013): choosing the plane to draw on, being in the sketch, and
/// leaving it again.
///
///  - `start()` (the button "Sketch"): the next click on a flat face, or on the bed, makes a new
///    sketch step on it. A click on a curved face makes no step and says why (`notice`).
///  - In the sketch: the view looks squarely at the plane, the other bodies are pale, a grid lies
///    on the plane and the pointer catches on the sketch (a marker shows where). The drawing
///    tools come with AP-2.1c.
///  - `finish()` (the button "Done", Esc): an empty sketch is removed again (a normal command), the
///    view returns to where it was.
/// The controller changes the document only through `DocumentModel` (commands).
class SketchController : public QObject {
    Q_OBJECT
    /// Waiting for the click that chooses the plane.
    Q_PROPERTY(bool picking READ picking NOTIFY stateChanged)
    /// In a sketch (the plane may still be being calculated, see `planeReady`).
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    /// Picking or in a sketch: Esc, the hint line and the view belong to the sketch mode.
    Q_PROPERTY(bool engaged READ engaged NOTIFY stateChanged)
    /// A drawing tool is on (none yet: AP-2.1c); Esc ends it before it ends the sketch.
    Q_PROPERTY(bool toolActive READ toolActive NOTIFY stateChanged)
    /// The plane of the sketch is known, grid and catching work.
    Q_PROPERTY(bool planeReady READ planeReady NOTIFY stateChanged)
    /// The id of the step being sketched in, empty when none.
    Q_PROPERTY(QString step READ step NOTIFY stateChanged)
    /// Where the pointer catches: { valid, u, v, kind (none | point | middle | alignment | grid),
    /// element, guides }; just { valid: false } when it is not over the plane.
    Q_PROPERTY(QVariantMap snap READ snap NOTIFY snapChanged)
    /// The distance (mm) of the grid lines now, 0 without a plane.
    Q_PROPERTY(double gridStep READ gridStep NOTIFY snapChanged)
    Q_PROPERTY(QColor gridColour MEMBER m_gridColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor gridMajorColour MEMBER m_gridMajorColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor elementColour MEMBER m_elementColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor guideColour MEMBER m_guideColour NOTIFY coloursChanged)
    Q_PROPERTY(QColor markerColour MEMBER m_markerColour NOTIFY coloursChanged)

public:
    SketchController(DocumentModel* model, SelectionController* selection,
                     QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool picking() const noexcept { return m_mode == Mode::picking; }
    bool active() const noexcept { return m_mode == Mode::sketching; }
    bool engaged() const noexcept { return m_mode != Mode::idle; }
    bool toolActive() const noexcept { return false; }
    bool planeReady() const noexcept { return m_mode == Mode::sketching && m_planeReady; }
    QString step() const { return m_step; }
    QVariantMap snap() const { return m_snapMap; }
    double gridStep() const noexcept { return m_gridStep; }

    /// Starts choosing the plane. Does nothing while a sketch mode is on.
    Q_INVOKABLE void start();
    /// The click on a flat face of a body (`body`, face `number`) or on the bed. Return true when a
    /// sketch step was made and the sketch mode is on.
    Q_INVOKABLE bool pickFace(const QString& body, int number);
    Q_INVOKABLE bool pickGround();
    /// "Done": ends the sketch mode. An empty sketch disappears.
    Q_INVOKABLE void finish();
    /// Esc: ends a tool, else the sketch, else the choosing of the plane.
    Q_INVOKABLE void escape();
    /// Double click on a sketch step in the recipe: opens the sketch mode on it. False (with a
    /// `notice`) when the step has no plane, e.g. because its face cannot be found any more.
    Q_INVOKABLE bool reopen(int stepIndex);

    /// The pointer is over the plane along this line of sight; `pixelsPerMillimetre` is the zoom.
    /// What the pointer catches shows as the marker and the aid lines. (The view calls this when
    /// the pointer moves; tests call it directly.)
    void pointAt(const interaction::PlaneRay& ray, double pixelsPerMillimetre);
    /// The pointer left the plane.
    void pointerGone();

signals:
    void stateChanged();
    void snapChanged();
    void coloursChanged();
    /// A sentence to tell the person, as a hint (never a dialog): "curvedFace" (sketches only go
    /// on flat faces), "faceUnusable" (this face cannot be drawn on), "needsHelp" (the sketch has
    /// no plane at the moment).
    void notice(const QString& code);

private:
    enum class Mode { idle, picking, sketching };

    void beginOn(const QString& stepId);
    void leave(bool removeIfEmpty);
    void onModelChanged();
    void onPicked(int intent, bool extend, const QString& body, int kind, int number);
    void onPointerMoved(double x, double y);
    void refresh();
    double baseStep() const;

    QPointer<DocumentModel> m_model;
    QPointer<SelectionController> m_selection;
    QPointer<Viewport3D> m_viewport;
    Mode m_mode = Mode::idle;
    QString m_step;
    bool m_planeReady = false;
    bool m_cameraTurned = false;
    core::SketchPlane m_plane;
    std::vector<core::SketchElement> m_elements;
    std::optional<render::CameraPose> m_previousPose;
    double m_gridStep = 0.0;
    double m_pixelsPerMillimetre = 2.0;
    std::optional<interaction::SketchSnap> m_caught;
    QVariantMap m_snapMap;

    QColor m_gridColour{0x3A, 0x3D, 0x43};
    QColor m_gridMajorColour{0x55, 0x59, 0x61};
    QColor m_elementColour{0xE8, 0xEA, 0xED};
    QColor m_guideColour{0x4C, 0x8D, 0xFF};
    QColor m_markerColour{0x4C, 0x8D, 0xFF};
};

} // namespace fission::ui
