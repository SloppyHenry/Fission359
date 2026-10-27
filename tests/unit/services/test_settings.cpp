// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <QFile>
#include <QTemporaryDir>

#include "fission/services/settings.h"

using namespace fission::services;

TEST_CASE("settings start at their defaults", "[settings]") {
    QTemporaryDir folder;
    const Settings settings(folder.filePath(QStringLiteral("s.ini")));
    CHECK(settings.value(keys::language) == QStringLiteral("system"));
    CHECK(settings.value(keys::theme) == QStringLiteral("dark"));
    CHECK(settings.value(keys::uiScale) == 100);
    CHECK(settings.value(keys::mouseStyle) == QStringLiteral("fission"));
    CHECK(settings.value(keys::handleStyle) == QStringLiteral("axes"));
    CHECK(settings.value(keys::units) == QStringLiteral("mm"));
    CHECK(settings.value(keys::decimals) == 3);
    CHECK(settings.value(keys::autosave));
    CHECK(settings.value(keys::autosaveMinutes) == 1);
    CHECK_FALSE(settings.value(keys::firstRunDone));
    CHECK(settings.value(keys::snapStep) == 0.0);
    CHECK(settings.value(keys::snapGridShown));
    CHECK(settings.value(keys::softShadows));
}

TEST_CASE("what is written is read back, also by a new start", "[settings]") {
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("s.ini"));
    {
        Settings settings(file);
        CHECK(settings.set(keys::language, QStringLiteral("de")));
        CHECK(settings.set(keys::theme, QStringLiteral("light")));
        CHECK(settings.set(keys::accentColor, QStringLiteral("#12ab34")));
        CHECK(settings.set(keys::uiScale, 150));
        CHECK(settings.set(keys::mouseStyle, QStringLiteral("blender")));
        CHECK(settings.set(keys::handleStyle, QStringLiteral("classic")));
        CHECK(settings.set(keys::printerId, QStringLiteral("bambu-a1")));
        CHECK(settings.set(keys::snapStep, 0.25));
        CHECK(settings.set(keys::snapGridShown, false));
        CHECK(settings.set(keys::softShadows, false));
        CHECK(settings.set(keys::units, QStringLiteral("in")));
        CHECK(settings.set(keys::decimals, 4));
        CHECK(settings.set(keys::autosave, false));
        CHECK(settings.set(keys::autosaveMinutes, 5));
        CHECK(settings.set(keys::firstRunDone, true));
    }
    const Settings again(file);
    CHECK(again.value(keys::language) == QStringLiteral("de"));
    CHECK(again.value(keys::theme) == QStringLiteral("light"));
    CHECK(again.value(keys::accentColor) == QStringLiteral("#12ab34"));
    CHECK(again.value(keys::uiScale) == 150);
    CHECK(again.value(keys::mouseStyle) == QStringLiteral("blender"));
    CHECK(again.value(keys::handleStyle) == QStringLiteral("classic"));
    CHECK(again.value(keys::printerId) == QStringLiteral("bambu-a1"));
    CHECK(again.value(keys::snapStep) == 0.25);
    CHECK_FALSE(again.value(keys::snapGridShown));
    CHECK_FALSE(again.value(keys::softShadows));
    CHECK(again.value(keys::units) == QStringLiteral("in"));
    CHECK(again.value(keys::decimals) == 4);
    CHECK_FALSE(again.value(keys::autosave));
    CHECK(again.value(keys::autosaveMinutes) == 5);
    CHECK(again.value(keys::firstRunDone));
}

TEST_CASE("an unusable value is refused, and an unusable stored one counts as not stored",
          "[settings]") {
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("s.ini"));
    Settings settings(file);
    CHECK_FALSE(settings.set(keys::theme, QStringLiteral("pink")));
    CHECK_FALSE(settings.set(keys::uiScale, 5000));
    CHECK_FALSE(settings.set(keys::decimals, 9));
    CHECK_FALSE(settings.set(keys::accentColor, QStringLiteral("nonsense")));
    CHECK(settings.value(keys::theme) == QStringLiteral("dark"));

    // A file edited by hand (or written by another version).
    QFile edited(file);
    REQUIRE(edited.open(QIODevice::WriteOnly | QIODevice::Truncate));
    edited.write("theme=pink\nuiScale=abc\nmouseStyle=sketchup\ndecimals=-3\n");
    edited.close();
    const Settings reread(file);
    CHECK(reread.value(keys::theme) == QStringLiteral("dark"));
    CHECK(reread.value(keys::uiScale) == 100);
    CHECK(reread.value(keys::mouseStyle) == QStringLiteral("sketchup"));
    CHECK(reread.value(keys::decimals) == 3);
}

TEST_CASE("reset goes back to the defaults", "[settings]") {
    QTemporaryDir folder;
    Settings settings(folder.filePath(QStringLiteral("s.ini")));
    REQUIRE(settings.set(keys::language, QStringLiteral("en")));
    settings.reset();
    CHECK(settings.value(keys::language) == QStringLiteral("system"));
}
