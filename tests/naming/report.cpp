// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "report.h"

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <utility>

namespace fission::test::naming {

namespace {

struct Tally {
    int correctByName = 0;
    int correctBySimilarity = 0;
    int correctWarned = 0;
    int unnecessaryHelp = 0;
    int rightHelp = 0;
    int silentlyWrong = 0;
    int warnedWrong = 0;

    int total() const {
        return correctByName + correctBySimilarity + correctWarned + unnecessaryHelp + rightHelp +
               silentlyWrong + warnedWrong;
    }
};

using Key = std::pair<std::string, int>; // group, strictness

std::map<Key, Tally>& tallies() {
    static std::map<Key, Tally> all;
    return all;
}

const char* modeName(int strictness) {
    return strictness == static_cast<int>(Strictness::strict) ? "strict" : "lenient";
}

class SummaryListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunEnded(const Catch::TestRunStats& /*stats*/) override {
        if (tallies().empty()) {
            return;
        }
        std::printf("\nPersistent references: result per group\n");
        std::printf("%-34s %-8s %5s %6s %6s %6s %6s %6s %6s %6s\n", "group", "mode", "n", "name",
                    "sim.", "warn", "help+", "help-", "WRONG", "w.wrg");
        std::ofstream csv;
        if (const char* path = std::getenv("FISSION_NAMING_REPORT"); path && *path) {
            csv.open(path);
            csv << "group,mode,total,correct_by_name,correct_by_similarity,correct_warned,"
                   "right_help,unnecessary_help,silently_wrong,warned_wrong\n";
        }
        for (const auto& [key, t] : tallies()) {
            std::printf("%-34s %-8s %5d %6d %6d %6d %6d %6d %6d %6d\n", key.first.c_str(),
                        modeName(key.second), t.total(), t.correctByName, t.correctBySimilarity,
                        t.correctWarned, t.rightHelp, t.unnecessaryHelp, t.silentlyWrong,
                        t.warnedWrong);
            if (csv.is_open()) {
                csv << key.first << ',' << modeName(key.second) << ',' << t.total() << ','
                    << t.correctByName << ',' << t.correctBySimilarity << ',' << t.correctWarned
                    << ',' << t.rightHelp << ',' << t.unnecessaryHelp << ',' << t.silentlyWrong
                    << ',' << t.warnedWrong << '\n';
            }
        }
        std::printf(
            "name = right by name, sim. = right by similarity, warn = right but marked,\n"
            "help+ = rightly asked for help, help- = unnecessary help,\n"
            "WRONG = silently wrong (must be 0), w.wrg = wrong but marked (lenient only)\n");
    }
};

} // namespace

CATCH_REGISTER_LISTENER(SummaryListener)

const char* toString(Verdict verdict) {
    switch (verdict) {
    case Verdict::correct:
        return "correct";
    case Verdict::correctWarned:
        return "correct (warned)";
    case Verdict::unnecessaryHelp:
        return "unnecessary help";
    case Verdict::rightHelp:
        return "right help";
    case Verdict::silentlyWrong:
        return "SILENTLY WRONG";
    case Verdict::warnedWrong:
        return "wrong (warned)";
    }
    return "?";
}

Verdict judge(const Resolution& resolution, const truth::Shapes& truth) {
    const bool unique = truth.size() == 1;
    if (resolution.outcome == Outcome::needsHelp) {
        return unique ? Verdict::unnecessaryHelp : Verdict::rightHelp;
    }
    const bool right = unique && truth::contains(truth, resolution.shape);
    if (right) {
        return resolution.checkPlease ? Verdict::correctWarned : Verdict::correct;
    }
    return resolution.checkPlease ? Verdict::warnedWrong : Verdict::silentlyWrong;
}

Verdict record(const std::string& group, Strictness strictness, const Resolution& resolution,
               const truth::Shapes& truth) {
    const Verdict verdict = judge(resolution, truth);
    Tally& t = tallies()[{group, static_cast<int>(strictness)}];
    switch (verdict) {
    case Verdict::correct:
        (resolution.outcome == Outcome::found ? t.correctByName : t.correctBySimilarity)++;
        break;
    case Verdict::correctWarned:
        ++t.correctWarned;
        break;
    case Verdict::unnecessaryHelp:
        ++t.unnecessaryHelp;
        break;
    case Verdict::rightHelp:
        ++t.rightHelp;
        break;
    case Verdict::silentlyWrong:
        ++t.silentlyWrong;
        break;
    case Verdict::warnedWrong:
        ++t.warnedWrong;
        break;
    }
    return verdict;
}

} // namespace fission::test::naming
