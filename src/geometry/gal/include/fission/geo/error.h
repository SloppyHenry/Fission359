// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace fission::geo {

/// Why a geometry operation failed. The message catalog turns the code into text for people;
/// raw kernel messages never leave the geometry layer (master prompt, rule 5).
enum class GeometryErrorCode {
    invalidInput,  ///< A parameter is out of range (zero size, not a number, ...).
    invalidResult, ///< The kernel produced a body that failed validation.
    kernelFailure, ///< The kernel could not compute the operation.
    textEmpty,     ///< A text without any visible character.
    glyphsMissing, ///< The font has no character for some of the text; `subject()` lists them.
    fontNotFound,  ///< No font of that name on this computer; `subject()` is the name.
};

class GeometryError : public std::runtime_error {
public:
    /// `subject` is what the person needs to hear about (the characters a font lacks, the name of
    /// a missing font); `detail` is for the log only.
    GeometryError(GeometryErrorCode code, const std::string& detail, std::string subject = {})
        : std::runtime_error(detail), code_(code), subject_(std::move(subject)) {}

    GeometryErrorCode code() const noexcept { return code_; }
    const std::string& subject() const noexcept { return subject_; }

private:
    GeometryErrorCode code_;
    std::string subject_;
};

} // namespace fission::geo
