// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QVariantList>

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

#include "fission/core/document.h"
#include "fission/core/feature_type.h"
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
/// Until commands exist (AP-1.2) the changes the interface makes (`setStepValue`,
/// `setBodyVisible`) go straight to the document; they are the places that become commands.
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

public:
    /// `kernel` does the geometry work; the application hands in the OpenCASCADE one.
    explicit DocumentModel(std::shared_ptr<const geo::GeometryKernel> kernel,
                           QObject* parent = nullptr);
    ~DocumentModel() override;

    /// The number fields use the named measures of the document in their formulas.
    void setNumbers(Numbers* numbers) { m_numbers = numbers; }

    /// The one box the program starts with (replaces the demo body of AP-0.8).
    void addDemoBox();

    const core::Document& document() const noexcept { return m_document; }
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

    Q_INVOKABLE void selectStep(int index);
    /// Sets the value `name` of step `index` to what was typed (a number or a formula).
    Q_INVOKABLE bool setStepValue(int index, const QString& name, const QString& text);
    Q_INVOKABLE void setBodyVisible(int index, bool visible);

signals:
    void recipeChanged();
    void bodiesChanged();
    void currentStepChanged();
    void stepValuesChanged();
    void calculatingChanged();
    /// A new scene for the 3D view.
    void sceneReady(std::shared_ptr<const fission::render::RenderScene> scene);

private:
    void onEngineEvent(const core::RecomputeEvent& event);
    void applyResult(std::shared_ptr<const core::RecomputeResult> result);
    void requestRecompute();
    void setCalculating(bool calculating);
    void rebuildLists();
    void rebuildScene();
    QVariantList buildStepValues() const;
    static std::uint64_t meshKey(const core::BodyResult& body) noexcept;

    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    core::FeatureRegistry m_registry;
    core::Document m_document;
    std::shared_ptr<const core::RecomputeResult> m_result;
    std::shared_ptr<const render::RenderScene> m_scene;
    Numbers* m_numbers = nullptr;

    QVariantList m_recipe;
    QVariantList m_bodies;
    QVariantList m_stepValues;
    int m_currentStep = -1;
    bool m_calculating = false;

    // Tessellations by result, made on the worker thread, read when the scene is put together.
    std::mutex m_meshMutex;
    std::map<std::uint64_t, geo::TriangleMesh> m_meshes;

    // Last: its thread must be stopped before the rest goes away.
    std::unique_ptr<core::RecomputeEngine> m_engine;
};

} // namespace fission::ui
