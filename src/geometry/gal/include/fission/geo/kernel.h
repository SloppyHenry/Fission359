// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "fission/geo/error.h"
#include "fission/geo/mesh.h"
#include "fission/geo/primitives.h"
#include "fission/geo/rotation.h"
#include "fission/geo/signature.h"
#include "fission/geo/solid.h"
#include "fission/geo/types.h"

namespace fission::geo {

/// A face together with the role its creator gave it ("top", "side"). `faceId` counts like
/// `TriangleMesh::faceIds` (1-based, in the order of the body's faces).
struct NamedFace {
    std::uint32_t faceId = 0;
    std::string role;
};

/// A body and the roles of its faces. The naming contract (ADR-0003): an operation names **every**
/// face it creates, so `faces` has exactly one entry per face of `solid`.
///
/// `history` tells what the operation did to the names of its inputs' faces. It is empty for
/// operations that make a body from nothing and for pure moves (every face stays as it was).
struct NamedSolid {
    Solid solid;
    std::vector<NamedFace> faces;
    ElementHistory history = {};
};

/// How bodies are combined (AP-1.7, ADR-0011).
enum class BooleanOp {
    merge,    ///< The target and the tools as one body.
    subtract, ///< The target without the places the tools take.
    intersect ///< Only what target and tools have in common.
};

/// An edge of a body to round or bevel: its number in the body (as in the tessellation) and the
/// name it is known by, which goes into the name of the face made there (may be empty).
struct BlendEdge {
    std::uint32_t id = 0;
    std::string name;
};

enum class BlendKind {
    round,  ///< A quarter-circle strip (Abrunden).
    chamfer ///< A flat cut (Abschrägen).
};

/// How big the rounding or bevel is (mm). A chamfer with `size2` > 0 is cut `size` into the one
/// face and `size2` into the other; 0 means the same on both.
struct BlendSpec {
    BlendKind kind = BlendKind::round;
    double size = 1.0;
    double size2 = 0.0;
};

/// A place on a face and the way the face looks out of the body there (unit vector): where the
/// grip for pushing the face stands (AP-2.4a).
struct FaceAnchor {
    Point3d point;
    Point3d way;
};

/// What features may ask of the geometry kernel (docs/03-architektur.md §4). The core and the
/// feature types only know this interface; the OpenCASCADE implementation is handed in by the
/// application, tests may hand in another one.
///
/// All functions are safe to call from several threads at once and throw `GeometryError` for
/// input the kernel cannot work with or results that fail validation.
class GeometryKernel {
public:
    GeometryKernel() = default;
    GeometryKernel(const GeometryKernel&) = delete;
    GeometryKernel& operator=(const GeometryKernel&) = delete;
    virtual ~GeometryKernel() = default;

    /// Box with the given edge lengths (mm); `corner` has the smallest coordinates.
    /// `cornerRadius` rounds the four vertical edges (0 = sharp, at most half the shorter side).
    /// Faces: bottom, top, front (−Y), back (+Y), left (−X), right (+X), and with rounded
    /// corners cornerFrontLeft, cornerFrontRight, cornerBackLeft, cornerBackRight.
    virtual NamedSolid makeBox(double sizeX, double sizeY, double sizeZ, Point3d corner,
                               double cornerRadius) const = 0;

    /// Cylinder standing on `base` (centre of its bottom), axis +Z.
    /// Faces: bottom, top, side.
    virtual NamedSolid makeCylinder(double radius, double height, Point3d base) const = 0;

    /// One of the other basic shapes (see primitives.h); `base` is the middle of its underside.
    virtual NamedSolid makePrimitive(const PrimitiveSpec& spec, Point3d base) const = 0;

    /// The same body turned by `angles` (degrees, see rotation.h) around `centre`. A pure move:
    /// the faces stay the faces they were and keep their roles (a "top" stays the top, whichever
    /// way it faces now), in the same order. Throws `GeometryError` for angles or a centre that
    /// are not numbers. The default refuses: a kernel that builds bodies must also turn them.
    virtual NamedSolid rotate(const NamedSolid& solid, Point3d centre, const Angles& angles) const {
        (void)solid;
        (void)centre;
        (void)angles;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot turn bodies");
    }

    /// The same body moved by `delta` (mm). A pure move, like `rotate`: the faces stay the faces
    /// they were, with their roles, in the same order. The default refuses.
    virtual NamedSolid translate(const NamedSolid& solid, Point3d delta) const {
        (void)solid;
        (void)delta;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot move bodies");
    }

    /// Combines `target` with `tools` exactly (no tolerance), then joins faces that lie in one
    /// surface. Faces keep the roles they had in their input; where one input face is cut into
    /// several pieces, the pieces get the role with `#1`, `#2` ... appended (never one name twice).
    /// A face that comes from a tool and now bounds the cut keeps the tool's role. Equal roles
    /// from different inputs that are joined into one face leave the role of the earlier input
    /// (target first). The result is always **one** body (a compound if it falls apart).
    /// `subtract` with tools that miss the target gives the target back unchanged. Throws
    /// `GeometryError` with `emptyResult` if nothing is left. The default refuses.
    /// `fuzzy` (mm, 0 = exact) lets faces that lie closer than that count as one: the way out
    /// for bodies whose faces lie almost on each other, offered to the person, never used unasked.
    virtual NamedSolid boolean(BooleanOp op, const NamedSolid& target,
                               const std::vector<NamedSolid>& tools, double fuzzy) const {
        (void)op;
        (void)target;
        (void)tools;
        (void)fuzzy;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot combine bodies");
    }
    NamedSolid boolean(BooleanOp op, const NamedSolid& target,
                       const std::vector<NamedSolid>& tools) const {
        return boolean(op, target, tools, 0.0);
    }

    /// Rounds or bevels `edges` of `solid`. Edges that run on tangentially are taken along. Faces
    /// keep their roles; each face made along an edge is named `rolePrefix` + "[" + the edge's
    /// name + "]" ("e" + number for an edge without a name), faces at corners get "#corner" and
    /// a number. Throws `GeometryError` with `blendTooLarge` (and the failed result as a preview,
    /// if there is one) when it does not work with that size, `invalidInput` for a size that is
    /// not a positive number or an edge that is not there. The default refuses.
    virtual NamedSolid blendEdges(const NamedSolid& solid, const std::vector<BlendEdge>& edges,
                                  const BlendSpec& spec, const std::string& rolePrefix) const {
        (void)solid;
        (void)edges;
        (void)spec;
        (void)rolePrefix;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot round edges");
    }

    /// The numbers of the edges that are rounded together with `edge`: the ones that run on from
    /// it without a corner (always includes `edge`). The default refuses.
    virtual std::vector<std::uint32_t> tangentChain(const Solid& solid, std::uint32_t edge) const {
        (void)solid;
        (void)edge;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot follow edges");
    }

    /// Moves one face by `distance` (mm) along the way it looks out of the body: positive makes
    /// the body larger there, negative smaller. A flat face moves flat (its neighbours must stand
    /// square on it), the wall of a round hole or pin gets a new radius (a hole's wall looks into
    /// the hole, so positive makes the hole narrower). The face keeps its role; faces that
    /// have to be made are named after it (`role` + "[push]"). Throws `GeometryError`:
    /// `invalidInput` for a distance that is zero or not a number or a radius that would drop to
    /// zero, `kernelFailure` for a face that cannot be moved. The default refuses.
    virtual NamedSolid offsetFace(const NamedSolid& solid, std::uint32_t faceId,
                                  double distance) const {
        (void)solid;
        (void)faceId;
        (void)distance;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot move faces");
    }

    /// Where to put the grip for `offsetFace`: a point on the face (its middle for a flat one, a
    /// point of the wall for a round one) and the way it looks out of the body. Throws
    /// `GeometryError` for a face that `offsetFace` cannot move. The default refuses.
    virtual FaceAnchor faceAnchor(const Solid& solid, std::uint32_t faceId) const {
        (void)solid;
        (void)faceId;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot move faces");
    }

    /// A place to cut `solid` for `stretch`: a coordinate along `way` (an axis direction, as for
    /// `stretch`) where the cut crosses nothing but flat faces along the way and round walls around
    /// it (no rounding, no hole across, no step). Of the free stretches the one closest to `near`
    /// wins, the place is its middle. None when `way` is no axis direction or there is no place.
    virtual std::optional<double> plainCut(const Solid& solid, Point3d way, double near) const {
        (void)solid;
        (void)way;
        (void)near;
        return std::nullopt;
    }

    /// Stretches the body: cut at `position` (a coordinate along `way`, a unit vector along x, y
    /// or z), the part beyond it moved by `distance` along `way` (negative: the stretch between
    /// the planes is taken out), the gap filled with the cross-section. Everything else stays
    /// exactly as it is. Faces keep their roles. Throws `GeometryError`: `invalidInput` with
    /// subject "position" when the cut would cross a rounding, a hole or a step (the person
    /// chooses the place), "distance" for zero or a body that would vanish, "face" for a way that
    /// is not an axis. The default refuses.
    virtual NamedSolid stretch(const NamedSolid& solid, Point3d way, double position,
                               double distance) const {
        (void)solid;
        (void)way;
        (void)position;
        (void)distance;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot stretch bodies");
    }

    /// The names of the fonts lettering can use (bundled and installed), sorted. Takes a moment
    /// the first time (the computer's fonts are read): ask from a background thread.
    virtual std::vector<std::string> fontFamilies() const { return {}; }

    /// The styles a font has (always in the order regular, bold, italic, boldItalic); empty if the
    /// font does not exist.
    virtual std::vector<FontStyle> fontStyles(const std::string& family) const {
        (void)family;
        return {};
    }

    virtual SolidReport measure(const Solid& solid) const = 0;

    /// The box round a body (mm), quickly: no volume, no validity check. For deciding how finely
    /// a big body is drawn. The default asks `measure`.
    virtual Bounds bounds(const Solid& solid) const {
        const SolidReport report = measure(solid);
        return {report.boxMin, report.boxMax};
    }

    /// The box round a body (mm), exactly: it touches the body (a turned cylinder, a sphere).
    /// What a body is made larger or smaller about (AP-1.6b2). Slower than `bounds`, faster than
    /// `measure`. The default asks `measure`.
    virtual Bounds exactBounds(const Solid& solid) const {
        const SolidReport report = measure(solid);
        return {report.boxMin, report.boxMax};
    }

    virtual TriangleMesh tessellate(const Solid& solid, const TessellationParams& params) const = 0;

    /// What every face and edge of a body looks like (ADR-0003, the base of persistent
    /// references). Costs about as much as a tessellation of the body's topology: ask only for
    /// bodies a reference points to. The default refuses.
    virtual BodySignatures signatures(const Solid& solid) const {
        (void)solid;
        throw GeometryError(GeometryErrorCode::kernelFailure, "this kernel cannot describe bodies");
    }
};

} // namespace fission::geo
