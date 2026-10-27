// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "fission/geo/kernel.h"

namespace fission::geo::occt {

struct KernelOptions {
    /// Font files that come with the program (for lettering). Fonts installed on the computer
    /// are found without being listed here.
    std::vector<std::string> fontFiles;
};

/// The OpenCASCADE implementation of the geometry kernel interface.
std::unique_ptr<GeometryKernel> makeKernel(const KernelOptions& options = {});

} // namespace fission::geo::occt
