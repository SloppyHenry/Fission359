// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>
#include <string>

namespace fission::geo {

/// Why a geometry operation failed. The message catalog turns the code into text for people;
/// raw kernel messages never leave the geometry layer (master prompt, rule 5).
enum class GeometryErrorCode {
    invalidInput,  ///< A parameter is out of range (zero size, not a number, ...).
    invalidResult, ///< The kernel produced a body that failed validation.
    kernelFailure, ///< The kernel could not compute the operation.
};

class GeometryError : public std::runtime_error {
public:
    GeometryError(GeometryErrorCode code, const std::string& detail)
        : std::runtime_error(detail), code_(code) {}

    GeometryErrorCode code() const noexcept { return code_; }

private:
    GeometryErrorCode code_;
};

} // namespace fission::geo
