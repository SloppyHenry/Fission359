// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/render/benchmark_scene.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace fission::render {

namespace {

// One tile: a grid of n x n cells, two triangles each, heights from a smooth function.
std::shared_ptr<const geo::TriangleMesh> makeTile(int cells, float size, int variant) {
    auto mesh = std::make_shared<geo::TriangleMesh>();
    const int side = cells + 1;
    mesh->positions.reserve(static_cast<std::size_t>(side) * static_cast<std::size_t>(side));
    mesh->normals.reserve(mesh->positions.capacity());
    const float step = size / static_cast<float>(cells);
    const float amplitude = 0.08F * size;
    const float frequency = 2.0F * std::numbers::pi_v<float> * (2.0F + static_cast<float>(variant % 3)) / size;
    const auto height = [&](float x, float y) {
        return 2.0F + amplitude * (std::sin(frequency * x) * std::cos(frequency * y) + 1.0F);
    };
    for (int j = 0; j < side; ++j) {
        for (int i = 0; i < side; ++i) {
            const float x = static_cast<float>(i) * step - 0.5F * size;
            const float y = static_cast<float>(j) * step - 0.5F * size;
            const float dx = amplitude * frequency * std::cos(frequency * x) * std::cos(frequency * y);
            const float dy = -amplitude * frequency * std::sin(frequency * x) * std::sin(frequency * y);
            const float length = std::sqrt(dx * dx + dy * dy + 1.0F);
            mesh->positions.push_back({x, y, height(x, y)});
            mesh->normals.push_back({-dx / length, -dy / length, 1.0F / length});
        }
    }
    mesh->indices.reserve(static_cast<std::size_t>(cells) * static_cast<std::size_t>(cells) * 6);
    for (int j = 0; j < cells; ++j) {
        for (int i = 0; i < cells; ++i) {
            const auto a = static_cast<std::uint32_t>(j * side + i);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(side);
            const std::uint32_t d = c + 1;
            mesh->indices.insert(mesh->indices.end(), {a, b, d, a, d, c});
        }
    }
    mesh->faceIds.assign(mesh->indices.size() / 3, 1);
    return mesh;
}

} // namespace

std::shared_ptr<const RenderScene> makeBenchmarkScene(std::size_t triangles, int bodies) {
    auto scene = std::make_shared<RenderScene>();
    bodies = std::max(bodies, 1);
    // cells per tile: 2 * cells^2 triangles
    const double perBody = static_cast<double>(triangles) / bodies;
    const int cells = std::max(2, static_cast<int>(std::lround(std::sqrt(perBody / 2.0))));
    const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(bodies))));
    const float pitch = std::min(scene->bed.sizeX, scene->bed.sizeY) / static_cast<float>(columns);
    const float size = 0.8F * pitch;
    for (int n = 0; n < bodies; ++n) {
        RenderBody body;
        body.id = "benchmark-" + std::to_string(n);
        body.meshVersion = static_cast<std::uint64_t>(n) + 1;
        body.mesh = makeTile(cells, size, n);
        // the tile is built around the origin: lay it out by moving its vertices
        auto moved = std::make_shared<geo::TriangleMesh>(*body.mesh);
        const float cx = (static_cast<float>(n % columns) + 0.5F) * pitch - 0.5F * pitch * static_cast<float>(columns);
        const float cy = (static_cast<float>(n / columns) + 0.5F) * pitch - 0.5F * pitch * static_cast<float>(columns);
        for (auto& p : moved->positions) {
            p.x += cx;
            p.y += cy;
        }
        body.mesh = std::move(moved);
        static constexpr unsigned char palette[4][3] = {
            {0xE0, 0x8E, 0x4B}, {0x5B, 0x9B, 0xD5}, {0x7C, 0xB3, 0x6B}, {0xD0, 0x6A, 0x8A}};
        const auto* rgb = palette[n % 4];
        body.color = QColor(rgb[0], rgb[1], rgb[2]);
        scene->bodies.push_back(std::move(body));
    }
    return scene;
}

} // namespace fission::render
