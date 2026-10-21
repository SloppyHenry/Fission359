// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "fission/core/command.h"
#include "fission/core/document.h"
#include "fission/core/element_names.h"
#include "fission/core/feature_type.h"
#include "fission/core/history.h"
#include "fission/core/recompute.h"
#include "fission/geo/kernel.h"
#include "fission/geo/seat.h"
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
    /// [{ index, type, number, name, color, visible, hole }]; `name` is empty unless the person
    /// named it. `hole` is true for a body marked "As hole".
    Q_PROPERTY(QVariantList bodies READ bodies NOTIFY bodiesChanged)
    /// Index in the recipe of the step whose values are shown, or -1.
    Q_PROPERTY(int currentStep READ currentStep NOTIFY currentStepChanged)
    /// Values of the current step: [{ name, group, expression, value, kind, error }].
    /// `expression` is empty when the value was typed as a plain number.
    Q_PROPERTY(QVariantList stepValues READ stepValues NOTIFY stepValuesChanged)
    /// Words of the current step (text, font): [{ name, value, kind }], `kind` is text | font.
    Q_PROPERTY(QVariantList stepTexts READ stepTexts NOTIFY stepValuesChanged)
    /// The handles of the current step in the world (mm): [{ parameter, kind, role, x, y, z, dx,
    /// dy, dz, sign, travel, rise, extent, value }]. `kind` is distance | radius, `role` is size |
    /// detail | rounding. Empty when the step has none or failed.
    Q_PROPERTY(QVariantList stepHandles READ stepHandles NOTIFY stepValuesChanged)
    /// The fonts lettering can use; filled in the background shortly after the start.
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies NOTIFY fontsChanged)
    /// A shape is being put down (dragged in): not yet a step in the history.
    Q_PROPERTY(bool placing READ placing NOTIFY placingChanged)
    /// The body of the shape being put down (empty when none).
    Q_PROPERTY(QString placingBodyId READ placingBodyId NOTIFY placingChanged)
    /// Something was copied and can be pasted.
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
    /// The height of the surface the last shape was put down on (0 = the print bed) and the
    /// body whose top it is (empty = the bed): the work plane (Arbeitsebene).
    Q_PROPERTY(double workplaneZ READ workplaneZ NOTIFY workplaneChanged)
    Q_PROPERTY(QString workplaneBodyId READ workplaneBodyId NOTIFY workplaneChanged)
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

    /// The names of the faces, edges and corners of every body, from the last result. They
    /// belong to the numbers of the meshes in `scene()` (the numbers do not change with the
    /// level of detail).
    using ElementNameMap = std::map<core::BodyId, core::ElementNames>;
    std::shared_ptr<const ElementNameMap> elementNames() const { return m_elementNames; }
    /// Id of the body at a position in `bodies`, or empty.
    Q_INVOKABLE QString bodyIdAt(int index) const;
    /// Position of a body in `bodies`, or -1.
    Q_INVOKABLE int bodyIndexOf(const QString& id) const;
    /// Position in the recipe of the step that made a body, or -1.
    Q_INVOKABLE int stepOfBody(const QString& id) const;
    /// Id of the first body a step made, or empty.
    Q_INVOKABLE QString bodyOfStep(int step) const;

    /// Blocks until the calculation that is running is done and its result is shown
    /// (start of the program, tests).
    void waitForCalculation();

    QVariantList recipe() const { return m_recipe; }
    QVariantList bodies() const { return m_bodies; }
    int currentStep() const noexcept { return m_currentStep; }
    QVariantList stepValues() const { return m_stepValues; }
    QVariantList stepTexts() const { return m_stepTexts; }
    QVariantList stepHandles() const { return m_stepHandles; }
    /// Would the current step accept `value` for `parameter` (checked without geometry)? A handle
    /// that is dragged stops where the shape stops being possible.
    bool acceptsValue(const QString& parameter, double value) const;
    QStringList fontFamilies() const { return m_fontFamilies; }
    bool placing() const noexcept { return m_placing; }
    QString placingBodyId() const;
    double workplaneZ() const noexcept { return m_workplaneZ; }
    QString workplaneBodyId() const { return m_workplaneBody; }
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
    /// Marks a body as a hole ("Als Loch") or takes the mark away.
    Q_INVOKABLE void setBodyHole(int index, bool hole);
    /// The styles a font has, as names ("regular", "bold", "italic", "boldItalic"); empty if the
    /// font is not on this computer.
    Q_INVOKABLE QStringList fontStyles(const QString& family) const;
    /// Sets a word of step `index` (the text of a lettering, the name of its font).
    Q_INVOKABLE bool setStepText(int index, const QString& name, const QString& text);

    // --- putting shapes down (AP-1.5) ---
    /// Starts putting a shape of `type` ("sphere", "box" ...) down at (x, y) on the plane z, as
    /// one undo step that is only finished by `finishPlacing`. Returns its step number, or -1
    /// for an unknown type. A shape that is already being put down is taken back first.
    Q_INVOKABLE int startPlacing(const QString& type, bool asHole, double x, double y, double z);
    /// Moves the shape being put down. `surfaceBody` is the body whose top it stands on (empty:
    /// the print bed).
    Q_INVOKABLE void placeAt(double x, double y, double z, const QString& surfaceBody);
    /// Moves the shape being put down onto a face that is not a flat top (AP-1.6b3): its
    /// underside lies on the face at `point`, whose outward direction is `normal`. The step gets
    /// the place and the three angles of the turn as plain values; the work plane stays as it
    /// was. A shape without a turn (or none being put down) is left alone.
    void placeOnFace(const geo::Point3d& point, const geo::Point3d& normal);
    /// Whether the shape being put down lies on a tilted face just now.
    bool placingOnFace() const noexcept { return m_placingOnFace; }
    /// Puts the shape down for good: one step in the history. The surface it stands on becomes
    /// the work plane. False if nothing was being put down.
    Q_INVOKABLE bool finishPlacing();
    /// Takes the shape back; nothing is left in the history.
    Q_INVOKABLE void cancelPlacing();
    /// A shape of `type` on the print bed at the first free place near the middle (a click on
    /// its tile), as one undo step. Returns its step number, or -1.
    Q_INVOKABLE int addShape(const QString& type, bool asHole);

    // --- changing bodies that are already there (AP-1.6e) ---
    // Each of these is one step in the history and writes only the values that change (a formula
    // in a value that stays is kept, ADR-0010). A body whose step has no place (later: results of
    // combining) is left out.
    /// The box that touches the body `bodyId` exactly (mm); nothing when it is not calculated.
    std::optional<geo::Bounds> bodyBounds(const QString& bodyId) const;
    /// Moves each body by its `delta` (mm). `key` names the step ("moveBody", "dropBody" ...).
    /// False when nothing moved.
    bool moveBodiesBy(const std::vector<std::pair<QString, std::array<double, 3>>>& moves,
                      const char* key);
    /// Lets the bodies fall on the print bed (or lifts them out of it): the lowest point of each
    /// ends at z = 0, its x and y stay. False when all of them stand there already.
    Q_INVOKABLE bool dropToBed(const QStringList& bodyIds);
    /// Lines the bodies up along `axis` (0 = x, 1 = y, 2 = z): `to` is min | centre | max.
    Q_INVOKABLE bool alignBodies(const QStringList& bodyIds, int axis, const QString& to);
    /// Lays the underside of an existing body on a face (AP-1.6b3 for a body that is already
    /// there): the same turn and foot as when it is put down there.
    bool seatBodyOnFace(const QString& bodyId, const geo::Point3d& point,
                        const geo::Point3d& normal);
    /// Copies the bodies for pasting. Only bodies of steps that work on nothing (shapes and
    /// lettering) can be copied; false (the clipboard stays as it was) if one cannot.
    Q_INVOKABLE bool copyBodies(const QStringList& bodyIds);
    /// Makes new bodies from the clipboard, 10 mm further to the side and to the back each time
    /// it is pasted, as one step. Returns the ids of the new bodies (empty: nothing to paste).
    Q_INVOKABLE QStringList pasteBodies();
    /// Copies and pastes in one go: the copy lies 10 mm to the side and to the back.
    Q_INVOKABLE QStringList duplicateBodies(const QStringList& bodyIds);
    /// Combines bodies in one step (ADR-0011): `mode` is merge | subtract | intersect, the first
    /// body is the target, the others are the tools. The result has the target's name and colour.
    /// Returns the id of the result; empty when it cannot be done (fewer than two bodies, a body
    /// that does not exist or is part of another result already).
    /// The label "cuts out: ..." of a hole (rule 13): `valid`, where it hangs (`x`, `y`, `z`: the
    /// middle of the top of the ghost) and `targets`, the bodies the hole touches, each as
    /// `{ index, cut }` (the index of the body it started as, and whether the hole cuts it).
    Q_INVOKABLE QVariantMap cutLabel(int bodyIndex) const;
    /// Lets the hole `holeIndex` cut the body `rootIndex` (or not), as one step. False when the
    /// body is no hole or the index is wrong.
    Q_INVOKABLE bool setCutTarget(int holeIndex, int rootIndex, bool cut);
    Q_INVOKABLE QString combineBodies(const QStringList& bodyIds, const QString& mode);
    bool canPaste() const noexcept { return !m_clipboard.empty(); }
    /// How far (mm) a copy lies from the original, in x and in y.
    static constexpr double kPasteOffset = 10.0;

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

    // --- moving bodies (AP-1.6, ADR-0010) ---
    /// Where the chosen step puts its body (its values x, y, z in mm), or nothing when the step
    /// has no position and cannot be moved.
    std::optional<std::array<double, 3>> currentPosition() const;
    /// Starts moving the body of the chosen step: everything until `commitChange` is one step
    /// in the history ("Box moved"); `cancelChange` takes it back. False when it cannot move.
    Q_INVOKABLE bool beginMove();
    /// Puts the body of the chosen step at a new place: the position values that are given
    /// become these numbers; the others stay as they are (a formula typed there is kept).
    /// False when the step cannot move.
    bool moveCurrent(const std::array<std::optional<double>, 3>& position);

    // --- turning bodies (AP-1.6b, ADR-0010) ---
    /// How the chosen step turns its body (its values rotX, rotY, rotZ in degrees), or nothing
    /// when the step has no turn.
    std::optional<geo::Angles> currentTurn() const;
    /// The point the body of the chosen step turns about (mm): the middle of its box as the step
    /// makes it, before the turn. Nothing when the step cannot be turned.
    std::optional<geo::Point3d> currentTurnCentre() const;
    /// Starts turning the body of the chosen step: everything until `commitChange` is one step in
    /// the history ("Box turned"); `cancelChange` takes it back. False when it cannot turn.
    Q_INVOKABLE bool beginTurn();
    /// Gives the body of the chosen step a new turn: the values that differ from the present ones
    /// become these numbers; those that do not stay as they are (a formula typed there is kept).
    bool turnCurrent(const geo::Angles& angles);

    // --- making bodies larger or smaller in all directions alike (AP-1.6b2) ---
    /// The box that touches the body of the chosen step (mm, exactly: a turned cylinder or a
    /// sphere touches it). Nothing when the step has no body, or its body is from an older result.
    std::optional<geo::Bounds> currentBodyBounds() const;
    /// The same for the shape that is being put down (AP-1.6c2: its size is needed to line it up
    /// with other bodies). Nothing while it has not been calculated yet.
    std::optional<geo::Bounds> placingBodyBounds() const;
    /// Starts making the body of the chosen step larger or smaller: everything until
    /// `commitChange` is one step in the history ("Box made larger to 150 %"); `cancelChange`
    /// takes it back. False when the step has no sizes or no place.
    Q_INVOKABLE bool beginScale();
    /// Would the shape still be possible with every size times `factor` (from where `beginScale`
    /// started; checked without geometry)? Sizes stay between `kSmallestSize` and `kLargestSize`.
    bool acceptsScale(double factor) const;
    /// Every size of the step times `factor` (from where `beginScale` started; a formula is kept
    /// as `(formula) * factor`), and the place moved so that the body grows about `anchor` (mm):
    /// that point stays where it is. A place value that does not move keeps what was typed.
    bool scaleCurrent(double factor, const geo::Point3d& anchor);

    static constexpr double kSmallestSize = 0.01;
    static constexpr double kLargestSize = 2000.0;

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
    void fontsChanged();
    void placingChanged();
    /// A shape was put down for good (dragged in or clicked): its step number and type.
    void shapePlaced(int step, const QString& type);
    void workplaneChanged();
    void clipboardChanged();
    void historyChanged();
    void printerChanged();
    /// A new result arrived: the names of the elements may be different now.
    void elementsChanged();
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
    /// Ends the open transaction. Cuts that holes need are updated first, so that they are part
    /// of the same step (ADR-0011).
    void finishTransaction();
    /// True when a body is marked as hole or a cut for a hole exists.
    bool hasCuttingParts() const;
    /// Runs one command and the cuts it needs as one step, described by `label`.
    bool executeWithCuts(std::unique_ptr<core::Command> command, core::CommandDescription label);
    void rebuildLists();
    void rebuildScene();
    QVariantList buildStepValues() const;
    QVariantList buildStepTexts() const;
    QVariantList buildStepHandles() const;
    bool setValueQuietly(const core::FeatureId& id, const char* name, double value);
    std::pair<double, double> freeSpot() const;
    static std::uint64_t meshKey(const core::BodyResult& body) noexcept;

    std::shared_ptr<const geo::GeometryKernel> m_kernel;
    core::FeatureRegistry m_registry;
    // The values of the step when making it larger or smaller began (`beginScale`).
    struct ScaleStart {
        core::FeatureId id;
        std::vector<std::pair<std::string, core::Value>> sizes; ///< as typed
        std::array<core::Value, 3> place;                       ///< x, y, z as typed
        std::array<double, 3> placeValue{};
        core::ResolvedParams params;
    };
    std::optional<ScaleStart> m_scale;
    core::History m_history;
    core::RecomputeCache m_cutCache; ///< for the cuts of holes (own memory: a separate calculation)
    std::shared_ptr<const core::RecomputeResult> m_result;
    std::optional<geo::Bounds> boundsOfFeature(const core::Feature& feature) const;
    /// The numbers x, y and z of a step, if it has a place. For a combined body the place of the
    /// first form it is made of (the target comes first).
    std::optional<std::array<double, 3>> positionOfFeature(const core::Feature& feature) const;
    /// The steps that put a body in its place: the step itself, or for a combined body all the
    /// steps of its parts, holes included (ADR-0011, they move together).
    std::optional<std::array<double, 3>> liveValues(const core::Feature& step) const;
    void collectForms(const core::Feature& step, std::vector<const core::Feature*>& out) const;
    /// Writes the turn and the foot of a seat into the step (one value after the other).
    void writeSeat(const core::FeatureId& id, const geo::Seat& seat);
    const core::Body* findBody(const QString& bodyId) const;

    // What was copied: the step that makes a body and what the person did to the body itself.
    struct ClipboardEntry {
        core::FeatureDraft draft;
        std::string name;
        core::Color color;
        bool hole = false;
    };
    std::vector<ClipboardEntry> m_clipboard;
    int m_pasted = 0; ///< how often this clipboard was pasted: the next copy lies further off
    std::shared_ptr<const render::RenderScene> m_scene;
    std::shared_ptr<const ElementNameMap> m_elementNames = std::make_shared<ElementNameMap>();
    Numbers* m_numbers = nullptr;

    QVariantList m_recipe;
    QVariantList m_bodies;
    QVariantList m_stepValues;
    QVariantList m_stepTexts;
    QVariantList m_stepHandles;
    QStringList m_fontFamilies;
    bool m_placing = false;
    core::FeatureId m_placingFeature;
    double m_placingZ = 0.0;
    QString m_placingSurface;
    geo::Point3d m_placingCentre; ///< middle of the shape's box minus its foot (before a turn)
    bool m_placingOnFace = false; ///< the shape lies on a tilted face (it has a turn)
    double m_workplaneZ = 0.0;
    QString m_workplaneBody;
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
