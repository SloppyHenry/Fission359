// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fission/geo/mesh.h"
#include "fission/geo/types.h"

namespace fission::geo {

/// Why a geometry operation failed. The message catalog turns the code into text for people;
/// raw kernel messages never leave the geometry layer (master prompt, rule 5).
enum class GeometryErrorCode {
    invalidInput,     ///< A parameter is out of range (zero size, not a number, ...).
    invalidResult,    ///< The kernel produced a body that failed validation.
    kernelFailure,    ///< The kernel could not compute the operation.
    emptyResult,      ///< The operation left nothing (an overlap of bodies that do not overlap).
    noOverlap,        ///< A tool does not touch the body it should work on.
    textEmpty,        ///< A text without any visible character.
    glyphsMissing,    ///< The font has no character for some of the text; `subject()` lists them.
    fontNotFound,     ///< No font of that name on this computer; `subject()` is the name.
    fontStyleMissing, ///< The font exists, but not in the wanted style; `subject()` is its name.
    blendTooLarge, ///< Rounding or bevelling did not work with that size; `subject()` is the value.
    importUnreadable, ///< The bytes are not a STEP or IGES file this kernel can read.
    importEmpty,      ///< The file holds no solid body that could be used.
};

class GeometryError : public std::runtime_error {
public:
    /// `subject` is what the person needs to hear about (the characters a font lacks, the name of
    /// a missing font); `detail` is for the log only.
    GeometryError(GeometryErrorCode code, const std::string& detail, std::string subject = {})
        : std::runtime_error(detail), code_(code), subject_(std::move(subject)) {}

    GeometryErrorCode code() const noexcept { return code_; }
    const std::string& subject() const noexcept { return subject_; }

    /// What the message can say besides words: named numbers in mm ("maxRadius"), the place the
    /// problem is at, the faces or edges it concerns, and a mesh of the failed result.
    GeometryError& withNumber(std::string name, double value) {
        numbers_[std::move(name)] = value;
        return *this;
    }
    GeometryError& at(Point3d point) {
        where_ = point;
        return *this;
    }
    GeometryError& concerning(std::vector<std::string> elements) {
        elements_ = std::move(elements);
        return *this;
    }
    /// A rough mesh of the result that failed the check, for showing it, never for computing.
    GeometryError& withPreview(std::shared_ptr<const TriangleMesh> mesh) {
        preview_ = std::move(mesh);
        return *this;
    }

    const std::map<std::string, double>& numbers() const noexcept { return numbers_; }
    const std::optional<Point3d>& where() const noexcept { return where_; }
    const std::vector<std::string>& elements() const noexcept { return elements_; }
    const std::shared_ptr<const TriangleMesh>& preview() const noexcept { return preview_; }

private:
    GeometryErrorCode code_;
    std::string subject_;
    std::map<std::string, double> numbers_;
    std::optional<Point3d> where_;
    std::vector<std::string> elements_;
    std::shared_ptr<const TriangleMesh> preview_;
};

} // namespace fission::geo
