// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Judging for the product suite of persistent references (AP-2.10d, docs/spikes/
// persistente-referenzen.md). The right answer is worked out from the geometry of the body
// (normals, planes, shapes), never from names: a name system that is wrong would otherwise
// agree with itself.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "refs_support.h"

namespace fission::test {

/// What the recompute made of one reference, compared with the one right element.
enum class Verdict {
    correct,       ///< Found, and it is the one right element.
    helpPlus,      ///< Asked for help where no single right element exists: the honest answer.
    helpMinus,     ///< Asked for help although there was one clear right element: unnecessary.
    silentlyWrong, ///< Took an element without a warning that is not the right one: FORBIDDEN.
};

struct Tally {
    int correct = 0;
    int helpPlus = 0;
    int helpMinus = 0;
    int wrong = 0;

    int total() const { return correct + helpPlus + helpMinus + wrong; }
    /// Of the references that had a right answer: how many were found without help.
    double automatic() const {
        const int withAnswer = correct + helpMinus + wrong;
        return withAnswer == 0 ? 1.0 : static_cast<double>(correct) / withAnswer;
    }
    void add(Verdict verdict) {
        switch (verdict) {
        case Verdict::correct:
            ++correct;
            break;
        case Verdict::helpPlus:
            ++helpPlus;
            break;
        case Verdict::helpMinus:
            ++helpMinus;
            break;
        case Verdict::silentlyWrong:
            ++wrong;
            break;
        }
    }
};

/// The numbers of the elements that geometrically are "the intended one": exactly one means
/// there is a right answer; none (it vanished) or several (it was split) means only "needs
/// help" is honest.
using Truth = std::vector<std::uint32_t>;

inline geo::Point3d turnedAboutZ(const geo::Point3d& v, double degrees) {
    return geo::rotatedDirection(v, {0.0, 0.0, degrees});
}

inline bool sameDirection(const geo::Point3d& a, const geo::Point3d& b) {
    const double cosine = a.x * b.x + a.y * b.y + a.z * b.z;
    return cosine > std::cos(1.0 * std::numbers::pi / 180.0); // within one degree
}

/// The flat faces looking along `normal`.
inline Truth facesLooking(const core::BodyLooks& looks, const geo::Point3d& normal) {
    Truth found;
    for (const core::NamedLook& face : looks.faces) {
        if (face.look.shape == core::ElementShape::plane &&
            sameDirection(face.look.direction, normal)) {
            found.push_back(face.id);
        }
    }
    return found;
}

/// The flat faces looking along `normal` that lie in the plane `normal · p = value`.
inline Truth facesLookingAt(const core::BodyLooks& looks, const geo::Point3d& normal,
                            double value) {
    Truth found;
    for (const std::uint32_t id : facesLooking(looks, normal)) {
        for (const core::NamedLook& face : looks.faces) {
            const geo::Point3d& c = face.look.centroid;
            if (face.id == id &&
                std::abs(c.x * normal.x + c.y * normal.y + c.z * normal.z - value) < 1e-6) {
                found.push_back(id);
            }
        }
    }
    return found;
}

/// The straight edges between a face looking along `a` and one looking along `b`.
inline Truth edgesBetween(const core::BodyLooks& looks, const geo::Point3d& a,
                          const geo::Point3d& b) {
    Truth found;
    for (const core::NamedLook& edge : looks.edges) {
        bool hasA = false;
        bool hasB = false;
        for (const core::NeighbourLook& side : edge.look.neighbourLooks) {
            hasA = hasA || sameDirection(side.direction, a);
            hasB = hasB || sameDirection(side.direction, b);
        }
        if (edge.look.shape == core::ElementShape::line && hasA && hasB) {
            found.push_back(edge.id);
        }
    }
    return found;
}

inline Truth facesOfShape(const core::BodyLooks& looks, core::ElementShape shape) {
    Truth found;
    for (const core::NamedLook& face : looks.faces) {
        if (face.look.shape == shape) {
            found.push_back(face.id);
        }
    }
    return found;
}

/// Judges the step `mark` in `result`. The recompute must have run the step or refused it for
/// its reference; anything else is a failure of the test itself.
inline Verdict judge(const RefWorld& world, const core::RecomputeResult& result,
                     const core::FeatureId& mark, const Truth& truth) {
    const core::FeatureState* state = result.state(mark);
    REQUIRE(state != nullptr);
    if (state->status == core::FeatureStatus::ok) {
        const auto found = world.mark->seenBy.find(mark.value);
        REQUIRE(found != world.mark->seenBy.end());
        const bool right = truth.size() == 1 && found->second.size() == 1 &&
                           found->second.front().id == truth.front();
        return right ? Verdict::correct : Verdict::silentlyWrong;
    }
    REQUIRE(state->status == core::FeatureStatus::warning);
    REQUIRE(state->issue.has_value());
    REQUIRE(state->issue->code == core::FeatureIssueCode::referenceNeedsHelp);
    return truth.size() == 1 ? Verdict::helpMinus : Verdict::helpPlus;
}

/// Appends the counts of one group to the file named by FISSION_NAMING_PRODUCT_REPORT (if set):
/// `scripts/test` sums the lines for its summary.
inline void report(const std::string& group, const Tally& tally) {
    const char* path = std::getenv("FISSION_NAMING_PRODUCT_REPORT");
    if (path == nullptr || *path == '\0') {
        return;
    }
    if (std::FILE* file = std::fopen(path, "a")) {
        std::fprintf(file, "%s %d %d %d %d\n", group.c_str(), tally.correct, tally.helpPlus,
                     tally.helpMinus, tally.wrong);
        std::fclose(file);
    }
}

} // namespace fission::test
