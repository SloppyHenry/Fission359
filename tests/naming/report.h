// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Judging and counting for the naming tests (AP-0.6). Every resolved reference is compared with
// the geometric truth and counted. The hard measure of the spike is "silently wrong" = 0; the
// soft measures (how often the system finds the right element by itself) are only reported.
//
// The summary is printed at the end of the run. With FISSION_NAMING_REPORT=<file> it is also
// written as CSV (group, mode, counts) for docs/spikes/persistente-referenzen.md.

#pragma once

#include <string>

#include "naming.h"
#include "truth.h"

namespace fission::test::naming {

enum class Verdict {
    correct,         // found the one right element, without a warning
    correctWarned,   // lenient mode: the right element, but shown with "please check"
    unnecessaryHelp, // asked for help although there was one clear right element
    rightHelp,       // asked for help where no single right element exists (or it is ambiguous)
    silentlyWrong,   // took an element without warning, but it is not the right one: FORBIDDEN
    warnedWrong,     // lenient mode: wrong element, but with "please check"
};

const char* toString(Verdict verdict);

// `truth` holds all elements that geometrically are "the intended one". Exactly one element
// means there is a right answer; none (it vanished) or several (it was split) means the only
// honest answer is "needs help".
Verdict judge(const Resolution& resolution, const truth::Shapes& truth);

// Counts the verdict under `group` / `strictness` and returns it.
Verdict record(const std::string& group, Strictness strictness, const Resolution& resolution,
               const truth::Shapes& truth);

} // namespace fission::test::naming
