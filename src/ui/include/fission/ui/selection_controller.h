// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "fission/core/selection.h"
#include "fission/ui/document_model.h"
#include "fission/ui/viewport_3d.h"

namespace fission::ui {

/// What the person has chosen in the 3D view and in the body list: the selection model of the
/// core (`core::Selection`) connected to the view and to the document.
///
/// The view tells what was clicked; this class decides what the choice becomes, keeps it up to
/// date when the document is recalculated (an element is found again by its name or let go,
/// never replaced by another one) and tells the view what to light up. A chosen body also
/// chooses its step in the recipe and the other way round, so that the panel on the right shows
/// the values of what is chosen. A choice is not a change of the document: it is not part of the
/// history.
class SelectionController : public QObject {
    Q_OBJECT
    /// Number of things chosen.
    Q_PROPERTY(int count READ count NOTIFY changed)
    /// What is chosen, in the order it was chosen: [{ body, bodyIndex, kind, number, name }];
    /// `kind` is body | face | edge | vertex, `bodyIndex` the position in `DocumentModel::bodies`.
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    /// What can be chosen: all | body | face | edge | vertex.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    /// What is under the pointer: { kind, bodyIndex, number }, or empty.
    Q_PROPERTY(QVariantMap hover READ hover NOTIFY hoverChanged)
    /// True if exactly one face is chosen (the view can look at it squarely).
    Q_PROPERTY(bool singleFaceChosen READ singleFaceChosen NOTIFY changed)

public:
    explicit SelectionController(DocumentModel* model, QObject* parent = nullptr);

    /// Connects the 3D view: its clicks choose, and it lights up what is chosen.
    void attach(Viewport3D* viewport);

    const core::Selection& selection() const noexcept { return m_selection; }
    int count() const noexcept { return static_cast<int>(m_selection.size()); }
    QVariantList items() const;
    QString filter() const;
    void setFilter(const QString& name);
    QVariantMap hover() const { return m_hover; }
    bool singleFaceChosen() const;

    /// Nothing chosen any more (Esc).
    Q_INVOKABLE void clear();
    /// A body of the list was clicked; with `extend` it is added or removed.
    Q_INVOKABLE void chooseBody(int bodyIndex, bool extend);
    /// Chooses every body that is shown (Select all).
    Q_INVOKABLE void chooseAllBodies();
    /// Turns the 3D view to look squarely at the chosen face.
    Q_INVOKABLE bool lookAtChosenFace();

signals:
    void changed();
    void filterChanged();
    void hoverChanged();

private:
    void onPicked(int intent, bool extend, const QString& body, int kind, int number);
    void onHover(const QString& body, int kind, int number);
    void onRubberBand(const QRectF& area, bool extend);
    void onRegionPicked(const QVariantList& elements, bool extend);
    void onElementsChanged();
    void onBodiesChanged();
    void onStepChanged();
    /// After every change of the choice: light it up, and follow it with the recipe's step.
    void choiceChanged();
    void showInView();
    std::optional<core::SelectionItem> itemFor(const QString& body, core::SelectableKind kind,
                                               std::uint32_t number) const;
    static QString kindName(core::SelectableKind kind);

    QPointer<DocumentModel> m_model;
    QPointer<Viewport3D> m_viewport;
    core::Selection m_selection;
    QVariantMap m_hover;
    bool m_followingStep = false; ///< the recipe's step is being set from here (no echo)
};

} // namespace fission::ui
