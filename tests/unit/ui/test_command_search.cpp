// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <QVariantMap>

#include "fission/ui/command_search.h"

using namespace fission::ui;

namespace {

QVariantMap command(const QString& id, const QString& name, const QStringList& words) {
    return {{"id", id}, {"name", name}, {"words", words}};
}

// A small catalogue like the real one: German and English words together.
QVariantList catalogue() {
    return {
        command("move", "Verschieben", {"bewegen", "schieben", "move", "shift", "position"}),
        command("size", "Größe",
                {"maße", "breite", "höhe", "skalieren", "size", "resize", "scale"}),
        command("rotate", "Drehen", {"rotieren", "kippen", "rotate", "turn", "spin"}),
        command("fillet", "Abrunden",
                {"runde ecken", "rundung", "kanten verrunden", "fillet", "round corners"}),
        command("hole", "Bohrung", {"bohren", "loch", "drill", "hole", "bore"}),
        command("shell", "Aushöhlen", {"hohl", "höhlen", "wand", "shell", "hollow out"}),
        command("text", "Text", {"namen", "name", "schrift", "beschriften", "lettering", "write"}),
        command("sphere", "Kugel", {"ball", "murmel", "sphere", "globe"}),
        command("cylinder", "Zylinder", {"rolle", "dose", "can", "rod", "cylinder"}),
    };
}

QString first(const QString& query) {
    const QStringList ids = CommandSearch().search(query, catalogue());
    return ids.isEmpty() ? QString() : ids.first();
}

} // namespace

TEST_CASE("the search finds what everyday words mean", "[command-search]") {
    CHECK(first("runde ecken") == "fillet");
    CHECK(first("bohren") == "hole");
    CHECK(first("hohl") == "shell");
    CHECK(first("namen") == "text");
}

TEST_CASE("typos, umlauts and capitals do not matter", "[command-search]") {
    CHECK(first("verschiben") == "move"); // a letter missing
    CHECK(first("VERSCHIEBEN") == "move");
    CHECK(first("grosse") == "size");      // "Größe" with ss
    CHECK(first("groesse") == "size");     // "oe" for ö is one letter off: found as a typo
    CHECK(first("hohlen") == "shell");     // ö typed as o
    CHECK(first("zylindr") == "cylinder"); // a letter missing at the end
}

TEST_CASE("English words find the commands in a German interface too", "[command-search]") {
    CHECK(first("hollow") == "shell");
    CHECK(first("drill") == "hole");
    CHECK(first("rotate") == "rotate");
    CHECK(first("round corners") == "fillet");
    CHECK(first("ball") == "sphere");
}

TEST_CASE("the beginning of a word is enough; all words must fit", "[command-search]") {
    CHECK(first("verschi") == "move");
    CHECK(first("kug") == "sphere");
    CHECK(CommandSearch().search("runde banane", catalogue()).isEmpty());
    CHECK(CommandSearch().search("xyzzy", catalogue()).isEmpty());
}

TEST_CASE("an empty text lists everything in the order of the catalogue", "[command-search]") {
    const QStringList all = CommandSearch().search("", catalogue());
    REQUIRE(all.size() == 9);
    CHECK(all.first() == "move");
    CHECK(all.last() == "cylinder");
}

TEST_CASE("the name counts more than a synonym", "[command-search]") {
    const QVariantList list{command("a", "Kugel", {"ball"}), command("b", "Ballon", {"kugel"})};
    // "kugel" is the name of a and only a synonym of b: a comes first.
    CHECK(CommandSearch().search("kugel", list).first() == "a");
}

TEST_CASE("the form of a text for comparing", "[command-search]") {
    CHECK(searchForm("  Größe, MASSE! ") == "grosse masse");
    CHECK(searchForm("Ähnlich ÖL Über") == "ahnlich ol uber");
    CHECK(searchForm("a-b_c") == "a b c");
}
