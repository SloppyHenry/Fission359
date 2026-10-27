// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <QTemporaryDir>

#include "fission/services/settings.h"
#include "fission/ui/preferences.h"

using fission::ui::Preferences;
using namespace fission::services;

namespace {
int changes = 0;
}

TEST_CASE("preferences keep what is set and tell about it", "[preferences]") {
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("s.ini"));
    {
        Settings settings(file);
        Preferences prefs(&settings, nullptr, nullptr);
        changes = 0;
        QObject::connect(&prefs, &Preferences::mouseStyleChanged, &prefs, [] { ++changes; });
        CHECK(prefs.mouseStyle() == QStringLiteral("fission"));
        prefs.setMouseStyle(QStringLiteral("blender"));
        prefs.setMouseStyle(QStringLiteral("blender"));  // the same again: no change to tell
        prefs.setMouseStyle(QStringLiteral("nonsense")); // refused
        CHECK(changes == 1);
        CHECK(prefs.mouseStyle() == QStringLiteral("blender"));
        prefs.setUiScale(150);
        prefs.setUiScale(9999); // refused
        CHECK(prefs.uiScale() == 150);
        prefs.setHandleStyle(QStringLiteral("classic"));
        prefs.setSoftShadows(false);
        prefs.setSnapStep(0.5);
    }
    Settings again(file);
    Preferences next(&again, nullptr, nullptr);
    CHECK(next.mouseStyle() == QStringLiteral("blender"));
    CHECK(next.uiScale() == 150);
    CHECK(next.handleStyle() == QStringLiteral("classic"));
    CHECK_FALSE(next.softShadows());
    CHECK(next.snapStep() == 0.5);

    next.resetAll();
    CHECK(next.mouseStyle() == QStringLiteral("fission"));
    CHECK(next.uiScale() == 100);
}

TEST_CASE("an accent colour close to a signal colour is warned about", "[preferences]") {
    QTemporaryDir folder;
    Settings settings(folder.filePath(QStringLiteral("s.ini")));
    const Preferences prefs(&settings, nullptr, nullptr);
    CHECK(prefs.accentWarning(QStringLiteral("#e02020")) == QStringLiteral("red"));
    CHECK(prefs.accentWarning(QStringLiteral("#f0c000")) == QStringLiteral("yellow"));
    CHECK(prefs.accentWarning(QStringLiteral("#20b040")) == QStringLiteral("green"));
    CHECK(prefs.accentWarning(QStringLiteral("#2563d9")).isEmpty()); // the program's blue
    CHECK(prefs.accentWarning(QStringLiteral("#8844cc")).isEmpty()); // violet
    CHECK(prefs.accentWarning(QStringLiteral("#808080")).isEmpty()); // grey is no signal
    CHECK(prefs.accentWarning(QStringLiteral("nonsense")).isEmpty());
}
