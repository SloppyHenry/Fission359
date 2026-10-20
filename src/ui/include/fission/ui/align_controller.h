// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

#include "fission/interaction/distances.h"
#include "fission/ui/document_model.h"
#include "fission/ui/selection_controller.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// Aligning several bodies (AP-1.6e, as in Tinkercad): with two or more bodies chosen, the tool
/// "Move" shows nine points on the box round all of them, three for each direction (lowest side,
/// middle, highest side). The points sit on the edges of the box that face the viewer. A click on
/// one puts all chosen bodies flush there, or in the middle, along that direction; pointing at
/// one shows the plane they will line up on. One click is one step in the history ("Bodies
/// aligned"), and only the place values along that direction change.
///
/// The points and the plane are drawn in the scene with depth (`Viewport3D::setHandleGraphics`);
/// the window holds the places to click and the little labels.
class AlignController : public QObject {
    Q_OBJECT
    /// Shown at all: the tool "Move" is on and two or more bodies are chosen.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// The nine points, in the same order every time: [{ index, axis, to, x, y }] in view pixels;
    /// `axis` is x | y | z, `to` is min | centre | max. Empty when nothing is shown.
    Q_PROPERTY(QVariantList points READ points NOTIFY pointsChanged)
    /// The colours of the points, from the theme: { x, y, z, ink, fill }.
    Q_PROPERTY(QVariantMap colours READ colours WRITE setColours NOTIFY coloursChanged)

public:
    AlignController(DocumentModel* model, SelectionController* selection,
                    QObject* parent = nullptr);

    void attach(Viewport3D* viewport);

    bool enabled() const noexcept { return m_enabled; }
    void setEnabled(bool enabled);
    QVariantList points() const { return m_points; }
    QVariantMap colours() const { return m_colourMap; }
    void setColours(const QVariantMap& colours);

    Q_INVOKABLE void refresh();
    /// The pointer is on point `index` (`on`) or left it: the plane it lines up on shows.
    Q_INVOKABLE void setPointed(int index, bool on);
    /// Aligns the chosen bodies on point `index`. One step in the history. False when nothing
    /// moved (they line up there already).
    Q_INVOKABLE bool align(int index);
    /// The place in `points` of a direction (x | y | z) and a line (min | centre | max), or -1.
    Q_INVOKABLE int indexOf(const QString& axis, const QString& to) const;

    /// Size of a point (pixels).
    static constexpr float kPointRadius = 6.0F;

signals:
    void enabledChanged();
    void pointsChanged();
    void coloursChanged();

private:
    /// The chosen bodies that can be aligned, and the box round them all.
    struct Group {
        QStringList bodies;
        interaction::Box3 box;
    };
    std::optional<Group> group() const;

    QPointer<DocumentModel> m_model;
    QPointer<SelectionController> m_selection;
    QPointer<Viewport3D> m_viewport;
    bool m_enabled = false;
    QVariantList m_points;
    int m_pointed = -1;
    struct Colours {
        QColor x{0xE5, 0x53, 0x4B};
        QColor y{0x57, 0xAB, 0x5A};
        QColor z{0x5B, 0x97, 0xFF};
        QColor ink{0x16, 0x18, 0x1B};
        QColor fill{0xFF, 0xFF, 0xFF};
    } m_colours;
    QVariantMap m_colourMap;
};

} // namespace fission::ui
