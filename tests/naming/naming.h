// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Prototype for AP-0.6 (persistent references / topological naming). This is a spike, not
// product code: AP-2.10 will build the real solution into the core. Like the other kernel
// tests it uses OpenCASCADE directly on purpose; the question under study is how well names
// survive changes, and for that the code has to see the kernel history itself.
//
// Concept (docs/03-architektur.md section 3.4):
//   1. Every feature names the faces it creates ("F3/hole/wall").
//   2. Names travel through booleans and fillets via the kernel history (Modified / Generated /
//      IsDeleted). Edges are named after the two faces they join ("edge[A|B]").
//   3. A reference also remembers what its target looked like (the signature). It is resolved in
//      three steps: name, then similarity, then "needs help". Nothing is ever guessed silently.

#pragma once

#include <string>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

namespace fission::test::naming {

enum class Kind { face, edge };

// What an adjacent face looked like (its name may be gone after a feature was replaced; the
// look still allows to recognise it).
struct NeighbourLook {
    std::string name;
    int geomType = -1;
    gp_Pnt centroid;
    gp_Dir direction{0.0, 0.0, 1.0};
    bool hasDirection = false;
    bool directed = false;
};

// What a referenced face or edge looked like when it was picked (and last resolved).
struct Signature {
    Kind kind = Kind::face;
    int geomType = -1; // GeomAbs_SurfaceType / GeomAbs_CurveType as number
    gp_Pnt centroid;   // center of mass
    gp_Dir direction{0.0, 0.0, 1.0};
    bool hasDirection = false; // plane normal, cylinder axis, line direction, circle axis
    bool directed = false;     // plane normals have a sign, axes and line directions do not
    double size = 0.0;         // area or length
    double radius = 0.0;       // cylinder / circle radius, 0 otherwise
    std::vector<std::string> neighbours;       // sorted names of the adjacent faces
    std::vector<NeighbourLook> neighbourLooks; // the same faces with their look
};

struct Reference {
    Kind kind = Kind::face;
    std::string name;
    Signature signature;
};

// One face or edge of a body together with its names (empty = not named).
struct Entry {
    TopoDS_Shape shape;
    std::vector<std::string> names; // sorted; the first one is the primary name
    const std::string& primary() const;
    bool hasName(const std::string& name) const;
    void addName(const std::string& name);
};

// An element that an earlier feature consumed (a rounded edge, a covered face). The body keeps
// what it looked like so that a reference to it is not mistaken for a look-alike that came
// from the same operation.
struct Tombstone {
    Kind kind = Kind::face;
    std::string name;
    Signature signature;
    std::string consumedBy; // feature id
};

// A solid together with the names of all its faces and edges.
class Body {
public:
    Body() = default;
    explicit Body(const TopoDS_Shape& shape);

    bool isNull() const { return shape_.IsNull(); }
    const TopoDS_Shape& shape() const { return shape_; }
    std::vector<Entry>& faces() { return faces_; }
    const std::vector<Entry>& faces() const { return faces_; }
    const std::vector<Entry>& edges() const { return edges_; }
    const std::vector<Entry>& entries(Kind kind) const;
    double diagonal() const { return diagonal_; }
    std::vector<Tombstone>& tombstones() { return tombstones_; }
    const std::vector<Tombstone>& tombstones() const { return tombstones_; }

    // Index of a face / edge of this body, or -1.
    int index(Kind kind, const TopoDS_Shape& sub) const;
    // Indices of all entries that carry `name`.
    std::vector<int> find(Kind kind, const std::string& name) const;

    // Names the edges after their two faces and gives still unnamed faces a name derived from
    // their named neighbours (`featureId/face[...]`). Call once after the history was applied.
    void deriveNames(const std::string& featureId);

    std::vector<std::string> neighbourNames(Kind kind, int index) const;
    std::vector<int> neighbourFaces(Kind kind, int index) const;
    Signature signatureOf(Kind kind, int index) const;

private:
    TopoDS_Shape shape_;
    std::vector<Entry> faces_;
    std::vector<Entry> edges_;
    std::vector<std::vector<int>> edgeFaces_; // per edge: indices of adjacent faces
    std::vector<std::vector<int>> faceEdges_; // per face: indices of its edges
    std::vector<Tombstone> tombstones_;
    double diagonal_ = 0.0;
};

// Creates the reference for a face / edge of `body` ("the user clicked it").
Reference makeReference(const Body& body, const TopoDS_Shape& sub, Kind kind);

// ---------------------------------------------------------------------------------------------
// Resolution

enum class Strictness {
    strict,  // only a clear, unique winner is taken over automatically (decision of the PO)
    lenient, // also takes a good match, but marks it "please check" (measured for AP-2.10)
};

enum class Outcome {
    found,     // the name led to exactly one plausible element
    fallback,  // the name was gone; the similarity search found a clear winner
    needsHelp, // not safe: the user has to pick again
};

struct Resolution {
    Outcome outcome = Outcome::needsHelp;
    bool checkPlease = false; // lenient mode only: a guess that is shown with a warning
    TopoDS_Shape shape;       // set for found / fallback
    int index = -1;           // index in the body for found / fallback
    double score = 0.0;       // similarity of the chosen element (fallback)
    std::string reason;       // short English explanation (for tests and the report)
};

Resolution resolve(const Body& body, const Reference& reference, Strictness strictness);

// Text listing every comparable candidate with its similarity score (for diagnosis).
std::string explain(const Body& body, const Reference& reference);

// After a successful resolution: the reference points to the current name and look.
void refresh(Reference& reference, const Body& body, const Resolution& resolution);

// Tunable limits of the similarity search. Values come from the measurements in
// docs/spikes/persistente-referenzen.md.
struct Limits {
    double directionGateDegrees = 15.0; // similarity search: direction must be this close
    double plausibleDegrees = 45.0;     // name hit: a bigger turn is suspicious
    double positionRadiusFactor = 0.25; // distance scale relative to the body diagonal
    double strictScore = 0.80;
    double strictMargin = 0.20;
    double lenientScore = 0.55;
    double lenientMargin = 0.08;
    double tombstoneSlack = 0.05;    // a consumed look-alike this close to the best is a rival
    bool useTombstones = true;       // consumed elements compete in the similarity search
    bool recogniseNeighbours = true; // match vanished neighbour names by their look
    double nearestAbsolute = 0.02;   // winner within this fraction of the diagonal of the old place
    double nearestRatio = 0.1;       // ... and at least this much closer than the runner-up
};
Limits& limits();

} // namespace fission::test::naming
