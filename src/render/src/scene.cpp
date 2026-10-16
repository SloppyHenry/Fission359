// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/scene.h"

namespace fission::render {

const geo::TriangleMesh& RenderBody::geometry() const {
    static const geo::TriangleMesh empty;
    return mesh ? *mesh : empty;
}

} // namespace fission::render
