// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <vector>

#include <lib3mf_implicit.hpp>

TEST_CASE("lib3mf writes and reads back a 3MF file", "[smoke][lib3mf]") {
    const auto wrapper = Lib3MF::CWrapper::loadLibrary();

    // A tetrahedron.
    const std::vector<Lib3MF::sPosition> vertices = {
        {{0.0f, 0.0f, 0.0f}}, {{10.0f, 0.0f, 0.0f}}, {{0.0f, 10.0f, 0.0f}}, {{0.0f, 0.0f, 10.0f}}};
    const std::vector<Lib3MF::sTriangle> triangles = {
        {{0, 2, 1}}, {{0, 1, 3}}, {{1, 2, 3}}, {{2, 0, 3}}};

    const auto path = std::filesystem::temp_directory_path() / "fission359-smoke.3mf";

    {
        auto model = wrapper->CreateModel();
        auto mesh = model->AddMeshObject();
        mesh->SetGeometry(vertices, triangles);
        model->QueryWriter("3mf")->WriteToFile(path.string());
    }

    auto model = wrapper->CreateModel();
    model->QueryReader("3mf")->ReadFromFile(path.string());
    auto meshes = model->GetMeshObjects();
    REQUIRE(meshes->Count() == 1);
    REQUIRE(meshes->MoveNext());
    CHECK(meshes->GetCurrentMeshObject()->GetTriangleCount() == 4);
    CHECK(meshes->GetCurrentMeshObject()->GetVertexCount() == 4);

    std::filesystem::remove(path);
}
