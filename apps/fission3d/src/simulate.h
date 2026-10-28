// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <functional>

class QQuickWindow;

namespace fission::ui {
class AlignController;
class DistanceController;
class DocumentModel;
class ExportController;
class FileController;
class GizmoController;
class HandleController;
class PlacementController;
class Preferences;
class RotateController;
class ScaleController;
class SelectionController;
class Viewport3D;
} // namespace fission::ui

/// Plays the actions of `--simulate "a:1;b:2"` through the real program, one after the other,
/// and calls `done` when the last one had time to show its result.
void playActions(QQuickWindow* window, fission::ui::Viewport3D* viewport,
                 fission::ui::PlacementController* placement, fission::ui::DocumentModel* model,
                 fission::ui::HandleController* handles, fission::ui::GizmoController* gizmo,
                 fission::ui::RotateController* rotate, fission::ui::ScaleController* scale,
                 fission::ui::DistanceController* distances, fission::ui::AlignController* align,
                 fission::ui::SelectionController* selection, fission::ui::FileController* files,
                 fission::ui::ExportController* exporter, fission::ui::Preferences* preferences,
                 const QString& actions, std::function<void()> done);

/// 0 while all checks of the actions held; else the code of the first one that failed (becomes
/// the exit code of the program, for the tests).
int simulationFailure();
