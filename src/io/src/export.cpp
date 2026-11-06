// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/io/export.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <exception>
#include <map>
#include <tuple>

#include "atomic_file.h"
#include <lib3mf_implicit.hpp>

namespace fission::io {

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr double degrees = 0.017453292519943295;

void putU32(Bytes& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void putFloat(Bytes& out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof bits == sizeof value);
    std::memcpy(&bits, &value, sizeof bits);
    putU32(out, bits); // little endian whatever the machine
}

struct MergedMesh {
    std::vector<geo::Point3f> vertices;
    std::vector<std::array<std::uint32_t, 3>> triangles;
};

// The mesh of the kernel repeats a corner for every face (one normal each): merge the corners
// that lie in the same place, as one closed mesh per body is wanted. Triangles that collapse
// by the merge have no area and are dropped.
MergedMesh mergeCorners(const geo::TriangleMesh& mesh) {
    MergedMesh merged;
    std::map<std::tuple<float, float, float>, std::uint32_t> known;
    std::vector<std::uint32_t> remap(mesh.positions.size());
    for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
        const geo::Point3f& point = mesh.positions[i];
        const auto [at, inserted] =
            known.try_emplace(std::make_tuple(point.x, point.y, point.z),
                              static_cast<std::uint32_t>(merged.vertices.size()));
        if (inserted) {
            merged.vertices.push_back(point);
        }
        remap[i] = at->second;
    }
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const std::uint32_t a = remap[mesh.indices[t]];
        const std::uint32_t b = remap[mesh.indices[t + 1]];
        const std::uint32_t c = remap[mesh.indices[t + 2]];
        if (a != b && b != c && a != c) {
            merged.triangles.push_back({a, b, c});
        }
    }
    return merged;
}

std::string plainNumber(float value) {
    char buffer[32];
    const auto result =
        std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::general, 7);
    return std::string(buffer, result.ptr);
}

// A name for one line of an OBJ/MTL file: no line breaks.
std::string oneLine(std::string name) {
    for (char& c : name) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return name;
}

} // namespace

geo::TessellationParams exportTessellation(ExportQuality quality) noexcept {
    switch (quality) {
    case ExportQuality::coarse:
        return {0.05, 10.0 * degrees};
    case ExportQuality::fine:
        return {0.005, 3.0 * degrees};
    case ExportQuality::normal:
        break;
    }
    return {0.01, 5.0 * degrees};
}

bool exportStl(const std::vector<MeshForExport>& bodies, const std::filesystem::path& path) {
    std::size_t triangles = 0;
    for (const MeshForExport& body : bodies) {
        triangles += body.mesh.triangleCount();
    }
    Bytes out;
    out.reserve(84 + triangles * 50);
    const char header[] = "Fission359 binary STL, millimetres";
    out.insert(out.end(), header, header + sizeof header - 1);
    out.resize(80, ' ');
    putU32(out, static_cast<std::uint32_t>(triangles));
    for (const MeshForExport& body : bodies) {
        const auto& p = body.mesh.positions;
        const auto& indices = body.mesh.indices;
        for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
            const geo::Point3f& a = p[indices[t]];
            const geo::Point3f& b = p[indices[t + 1]];
            const geo::Point3f& c = p[indices[t + 2]];
            // The face normal from the corners (counter-clockwise from outside), in double.
            const double ux = static_cast<double>(b.x) - a.x;
            const double uy = static_cast<double>(b.y) - a.y;
            const double uz = static_cast<double>(b.z) - a.z;
            const double vx = static_cast<double>(c.x) - a.x;
            const double vy = static_cast<double>(c.y) - a.y;
            const double vz = static_cast<double>(c.z) - a.z;
            double nx = uy * vz - uz * vy;
            double ny = uz * vx - ux * vz;
            double nz = ux * vy - uy * vx;
            const double length = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (length > 0.0) {
                nx /= length;
                ny /= length;
                nz /= length;
            }
            putFloat(out, static_cast<float>(nx));
            putFloat(out, static_cast<float>(ny));
            putFloat(out, static_cast<float>(nz));
            for (const geo::Point3f* corner : {&a, &b, &c}) {
                putFloat(out, corner->x);
                putFloat(out, corner->y);
                putFloat(out, corner->z);
            }
            out.push_back(0);
            out.push_back(0); // attribute byte count
        }
    }
    return writeFileAtomically(path, out);
}

bool export3mf(const std::vector<MeshForExport>& bodies, const std::filesystem::path& path) {
    try {
        const auto wrapper = Lib3MF::CWrapper::loadLibrary();
        auto model = wrapper->CreateModel();
        model->SetUnit(Lib3MF::eModelUnit::MilliMeter);
        auto materials = model->AddBaseMaterialGroup();

        Lib3MF::sTransform identity{};
        for (int row = 0; row < 3; ++row) {
            identity.m_Fields[row][row] = 1.0F;
        }

        for (const MeshForExport& body : bodies) {
            const MergedMesh merged = mergeCorners(body.mesh);
            std::vector<Lib3MF::sPosition> vertices;
            for (const geo::Point3f& point : merged.vertices) {
                vertices.push_back({{point.x, point.y, point.z}});
            }
            std::vector<Lib3MF::sTriangle> triangles;
            for (const auto& triangle : merged.triangles) {
                triangles.push_back({{triangle[0], triangle[1], triangle[2]}});
            }

            auto object = model->AddMeshObject();
            object->SetName(body.name);
            object->SetGeometry(vertices, triangles);
            const auto property = materials->AddMaterial(
                body.name,
                wrapper->RGBAToColor(body.color.red, body.color.green, body.color.blue, 255));
            object->SetObjectLevelProperty(materials->GetUniqueResourceID(), property);
            model->AddBuildItem(object.get(), identity);
        }

        std::vector<Lib3MF_uint8> bytes;
        model->QueryWriter("3mf")->WriteToBuffer(bytes);
        return writeFileAtomically(path, bytes);
    } catch (const std::exception&) {
        return false; // the library says what went wrong in its own words: not for the person
    }
}

bool exportObj(const std::vector<MeshForExport>& bodies, const std::filesystem::path& path) {
    std::filesystem::path materialPath = path;
    materialPath.replace_extension(".mtl");
    std::string obj =
        "# Fission359, millimetres\nmtllib " + materialPath.filename().string() + "\n";
    std::string mtl = "# Fission359\n";
    std::uint32_t offset = 0; // OBJ numbers corners over the whole file, starting at 1
    int index = 0;
    for (const MeshForExport& body : bodies) {
        const MergedMesh merged = mergeCorners(body.mesh);
        const std::string material = "colour" + std::to_string(index++);
        mtl += "newmtl " + material + "\nKd " + plainNumber(body.color.red / 255.0F) + " " +
               plainNumber(body.color.green / 255.0F) + " " +
               plainNumber(body.color.blue / 255.0F) + "\n";
        obj += "o " + oneLine(body.name) + "\nusemtl " + material + "\n";
        for (const geo::Point3f& v : merged.vertices) {
            obj += "v " + plainNumber(v.x) + " " + plainNumber(v.y) + " " + plainNumber(v.z) + "\n";
        }
        for (const auto& t : merged.triangles) {
            obj += "f " + std::to_string(offset + t[0] + 1) + " " +
                   std::to_string(offset + t[1] + 1) + " " + std::to_string(offset + t[2] + 1) +
                   "\n";
        }
        offset += static_cast<std::uint32_t>(merged.vertices.size());
    }
    return writeTextFile(mtl, materialPath) && writeTextFile(obj, path);
}

bool writeTextFile(const std::string& text, const std::filesystem::path& path) {
    return writeFileAtomically(path, Bytes(text.begin(), text.end()));
}

} // namespace fission::io
