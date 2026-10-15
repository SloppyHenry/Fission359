// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>

#include <cstdint>
#include <map>
#include <atomic>
#include <memory>
#include <mutex>

#include "fission/core/document.h"
#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/geo/kernel.h"
#include "fission/render/scene.h"

namespace fission::ui {

class Numbers;

/// Connects the document of the core to the interface (QML): the recipe as sentences-in-waiting,
/// the bodies, the values of the chosen step and the scene for the 3D view.
///
/// The core calculates in a worker thread; this object receives the results on the interface
/// thread. It hands QML numbers and codes, never finished sentences: the texts (German and
/// English) are composed in QML with qsTr (rule 4).
///
/// Every change the interface makes is a command in the history (AP-1.2): the model holds a
/// `core::History`, so the document can only be looked at from outside. Undo, redo and the list
/// of recent actions are offered here; the sentences for the list are composed in QML.
class DocumentModel : public QObject {
    Q_OBJECT
    /// [{ index, type, state, values: [number], problem: { code, subject, field, detail } }]
    /// `state` is ok | warning | fails | off | pending.
    Q_PROPERTY(QVariantList recipe READ recipe NOTIFY recipeChanged)
    /// [{ index, type, number, name, color, visible }]; `name` is empty unless the person named it.
    Q_PROPERTY(QVariantList bodies READ bodies NOTIFY bodiesChanged)
    /// Index in the recipe of the step whose values are shown, or -1.
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)
    /// Values of the current step: [{ name, group, expression, value, kind, error }].
    /// `expression` is empty when the value was typed as a plain number.
    Q_PROPERTY(QVariantList stepValues READ stepValues NOTIFY stepValuesChanged)
    Q_PROPERTY(bool calculating READ calculating NOTIFY calculatingChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    /// What `undo` / `redo` would do next, as { key, type, number, field, value, name } (text
    /// values of a `CommandDescription`); empty when there is nothing to do.
    Q_PROPERTY(QVariantMap nextUndo READ nextUndo NOTIFY historyChanged)
    Q_PROPERTY(QVariantMap nextRedo READ nextRedo NOTIFY historyChanged)
    /// Recent actions, oldest first: [{ key, type, number, field, value, name, done }].
    /// `done` is false for the ones that were undone and can be redone.
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    /// Number of actions that are done (= the position in `history`).
    Q_PROPERTY(int historyPosition READ historyPosition NOTIFY historyChanged)
    /// The printer whose print bed is shown (id from the printer list).
    Q_PROPERTY(QString printerId READ printerId NOTIFY printerChanged)
    /// Brand and model; empty for the generic printer (the interface says "My printer").
    Q_PROPERTY(QString printerName READ printerName NOTIFY printerChanged)
    Q_PROPERTY(double bedX READ bedX NOTIFY printerChanged)
    Q_PROPERTY(double bedY READ bedY NOTIFY printerChanged)
    Q_PROPERTY(double bedZ READ bedZ NOTIFY printerChanged)
    Q_PROPERTY(double nozzle READ nozzle NOTIFY printerChanged)
    /// [{ id, name, bedX, bedY, bedZ, nozzle }]
    Q_PROPERTY(QVariantList printers READ printers CONSTANT)

public:
    /// `kernel` does the geometry work; the application hands in the OpenCASCADE one.
    explicit DocumentModel(std::shared_ptr<const geo::GeometryKernel> kernel,
                           QObject* parent = nullptr);
    ~DocumentModel() override;

    /// The number fields use the named measures of the document in their formulas.
    void setNumbers(Numbers* numbers) { m_numbers = numbers; }

    /// The one box the program starts with (replaces the demo body of AP-0.8).
    void addDemoBox();

    const core::Document& document() const noexcept { return m_history.document(); }
    std::shared_ptr<const core::RecomputeResult> result() const { return m_result; }
    std::shared_ptr<const render::RenderScene> scene() const { return m_scene; }

    /// Blocks until the calculation that is running is done and its result is shown
    /// (start of the program, tests).
    void waitForCalculation();

    QVariantList recipe() const { return m_recipe; }
    QVariantList bodies() const { return m_bodies; }
    int currentStep() const noexcept { return m_currentStep; }
    QVariantList stepValues() const { return m_stepValues; }
    bool calculating() const noexcept { return m_calculating; }
    bool canUndo() const noexcept { return m_history.canUndo(); }
    bool canRedo() const noexcept { return m_history.canRedo(); }
    QVariantMap nextUndo() const;
    QVariantMap nextRedo() const;
    QVariantList history() const;
    int historyPosition() const noexcept { return static_cast<int>(m_history.position()); }

    QString printerId() const { return m_printerId; }
    QString printerName() const;
    double bedX() const;
    double bedY() const;
    double bedZ() const;
    double nozzle() const;
    QVariantList printers() const;
    /// Chooses the printer; false for an id that is not in the list. Not part of the history:
    /// the printer is a setting of the program, not a change of the model.
    Q_INVOKABLE bool setPrinter(const QString& id);

    Q_INVOKABLE void selectStep(int index);
    /// Sets the value `name` of step `index` to what was typed (a number or a formula).
    Q_INVOKABLE bool setStepValue(int index, const QString& name, const QString& text);
    Q_INVOKABLE void setBodyVisible(int index, bool visible);

    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    /// Goes to the state after `position` actions (0 = the beginning of this session).
    Q_INVOKABLE bool goToHistory(int position);

    /// A change that takes many small steps (dragging a handle): everything between
    /// `beginChange` and `commitChange` is one step in the history. `field` names what changes.
    Q_INVOKABLE void beginChange(const QString& field);
    Q_INVOKABLE void commitChange();
    /// Takes back everything since `beginChange` (the person pressed Esc).
    Q_INVOKABLE void cancelChange();

    /// How finely bodies are tessellated for the screen (0 = basic, see render/detail.h). The
    /// view asks for more when the person zooms in; finer meshes are made in the background and
    /// replace the coarse ones when they are ready.
    Q_INVOKABLE void setDetailLevel(int level);
    int detailLevel() const { return m_detailLevel; }
    /// The level of the meshes that are on screen now (for tests and the log).
    int shownDetailLevel() const { return m_shownDetailLevel; }

signals:
    void recipeChanged();
    void bodiesChanged();
    void currentStepChanged();
    void stepValuesChanged();
    void calculatingChanged();
    void historyChanged();
    void printerChanged();
    /// A new scene for the 3D view.
    void sceneReady(std::shared_ptr<const fission::render::RenderScene> scene);

private:
    void onEngineEvent(const core::RecomputeEvent& event);
    void applyResult(std::shared_ptr<const core::RecomputeResult> result);
    void requestRecompute();
    /// After the document changed by a command, undo or redo: show it, and calculate again
    /// unless `geometryChanged` is false (only a look changed).
    void documentChanged(bool geometryChanged = true);
    int stepIndexOf(const core::FeatureId& id) const;
    void setCalculating(bool calculating);
    void rebuildLists();
    void rebuildScene();
    QVariantList buildStepValues() const;
    static std::uint64_t meshKey(const core::BodyResult& body) noexcept;

    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    core::FeatureRegistry m_registry;
    core::History m_history;
    std::shared_ptr<const core::RecomputeResult> m_result;
    std::shared_ptr<const render::RenderScene> m_scene;
    Numbers* m_numbers = nullptr;

    QVariantList m_recipe;
    QVariantList m_bodies;
    QVariantList m_stepValues;
    int m_currentStep = -1;
    core::FeatureId m_currentFeature; // the chosen step, by id so that undo cannot shift it
    bool m_calculating = false;

    // Tessellations by result, made on the worker thread, read when the scene is put together.
    std::mutex m_meshMutex;
    std::map<std::uint64_t, std::shared_ptr<const geo::TriangleMesh>> m_meshes;

    // Finer tessellations for the current zoom, by mesh key; made by a pool thread, used on the
    // interface thread only.
    void startDetailJob();
    void applyDetail(std::uint64_t generation, int level,
                     std::map<std::uint64_t, std::shared_ptr<const geo::TriangleMesh>> meshes);
    QString m_printerId{QStringLiteral("generic")};
    int m_detailLevel = 0;
    int m_shownDetailLevel = 0;
    std::uint64_t m_detailGeneration = 0;
    std::shared_ptr<std::atomic<std::uint64_t>> m_detailCurrent =
        std::make_shared<std::atomic<std::uint64_t>>(0);
    std::map<std::uint64_t, std::shared_ptr<const geo::TriangleMesh>> m_detailMeshes;
    QTimer m_detailTimer;
    QThreadPool m_detailPool;

    // Last: its thread must be stopped before the rest goes away.
    std::unique_ptr<core::RecomputeEngine> m_engine;
};

} // namespace fission::ui
