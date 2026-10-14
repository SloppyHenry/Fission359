// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Private to src/geometry/occt: the OpenCASCADE payload of a Solid.

#pragma once

#include <memory>
#include <utility>

#include "fission/geo/solid.h"

#include <TopoDS_Shape.hxx>

namespace fission::geo::occt {

class OcctSolidData final : public SolidData {
public:
    explicit OcctSolidData(TopoDS_Shape shape) : shape_(std::move(shape)) {}
    const TopoDS_Shape& shape() const noexcept { return shape_; }

private:
    TopoDS_Shape shape_;
};

inline Solid wrap(TopoDS_Shape shape) {
    return Solid(std::make_shared<const OcctSolidData>(std::move(shape)));
}

} // namespace fission::geo::occt
