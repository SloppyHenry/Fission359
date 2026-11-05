// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// STEP and IGES files in and STEP out, with names and colours (AP-2.13). Goes through OCCT's
// XCAF document so that colours and names survive.

#include "fission/geo/occt/exchange.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <utility>

#include "fission/geo/error.h"

#include "occt_build.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_CurveType.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <IGESData_GlobalSection.hxx>
#include <IGESData_IGESModel.hxx>
#include <NCollection_Sequence.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ColorType.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

namespace fission::geo::occt {

namespace {

// OCCT's file readers keep their settings in process-wide tables. One file at a time is slow
// for nobody (a read takes milliseconds to seconds) and keeps the kernel safe to call from
// several threads.
std::mutex& exchangeLock() {
    static std::mutex lock;
    return lock;
}

// An XCAF document for one import or export. The document and its attributes point at each other
// (the owner attribute on the root label holds the document), so the handles alone never free it;
// the destructor drops the attributes and breaks that cycle.
class XcafDocument {
public:
    XcafDocument() : document_(new TDocStd_Document("MDTV-XCAF")) {
        XCAFDoc_DocumentTool::Set(document_->Main());
    }
    ~XcafDocument() { document_->Main().Root().ForgetAllAttributes(true); }
    XcafDocument(const XcafDocument&) = delete;
    XcafDocument& operator=(const XcafDocument&) = delete;

    const occ::handle<TDocStd_Document>& handle() const { return document_; }

private:
    occ::handle<TDocStd_Document> document_;
};

std::string nameOf(const TDF_Label& label) {
    occ::handle<TDataStd_Name> name;
    if (label.IsNull() || !label.FindAttribute(TDataStd_Name::GetID(), name)) {
        return {};
    }
    return TCollection_AsciiString(name->Get()).ToCString(); // UTF-8
}

std::uint8_t channel(double value) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(value * 255.0), 0L, 255L));
}

std::optional<FileColor> toFileColor(const Quantity_Color& color) {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    color.Values(red, green, blue, Quantity_TOC_sRGB);
    return FileColor{channel(red), channel(green), channel(blue)};
}

std::optional<FileColor> colorOf(const occ::handle<XCAFDoc_ColorTool>& colors,
                                 const TopoDS_Shape& solid) {
    Quantity_Color color;
    for (const XCAFDoc_ColorType type : {XCAFDoc_ColorSurf, XCAFDoc_ColorGen}) {
        if (colors->GetColor(solid, type, color)) {
            return toFileColor(color);
        }
    }
    for (TopExp_Explorer faces(solid, TopAbs_FACE); faces.More(); faces.Next()) {
        if (colors->GetColor(faces.Current(), XCAFDoc_ColorSurf, color)) {
            return toFileColor(color);
        }
    }
    return std::nullopt;
}

// Valid as is, or repaired (`repaired` set), or nothing.
std::optional<TopoDS_Shape> usable(const TopoDS_Shape& shape, bool& repaired) {
    if (BRepCheck_Analyzer(shape).IsValid()) {
        return shape;
    }
    ShapeFix_Shape fixer(shape);
    fixer.Perform();
    const TopoDS_Shape fixed = fixer.Shape();
    if (!fixed.IsNull() && BRepCheck_Analyzer(fixed).IsValid()) {
        repaired = true;
        return fixed;
    }
    return std::nullopt;
}

// Loose faces that close up into a watertight shell: a solid made of them (IGES is mostly
// surfaces). Only closed shells count, and only when the solid is valid; the rest stays out.
std::vector<TopoDS_Shape> solidsFromSurfaces(const TopoDS_Shape& shape) {
    std::vector<TopoDS_Shape> solids;
    BRepBuilderAPI_Sewing sewing(1e-4);
    sewing.Add(shape);
    sewing.Perform();
    const TopoDS_Shape sewn = sewing.SewedShape();
    if (sewn.IsNull()) {
        return solids;
    }
    for (TopExp_Explorer shells(sewn, TopAbs_SHELL); shells.More(); shells.Next()) {
        const TopoDS_Shell shell = TopoDS::Shell(shells.Current());
        if (!BRep_Tool::IsClosed(shell)) {
            continue;
        }
        TopoDS_Solid solid = BRepBuilderAPI_MakeSolid(shell).Solid();
        GProp_GProps properties;
        BRepGProp::VolumeProperties(solid, properties);
        if (properties.Mass() < 0.0) { // the faces looked inwards: turn the solid inside out
            solid.Reverse();
        }
        if (BRepCheck_Analyzer(solid).IsValid()) {
            solids.push_back(solid);
        }
    }
    return solids;
}

NamedSolid namedAsImported(const TopoDS_Shape& shape) {
    int next = 0;
    return nameFaces(shape, "import",
                     [&](const TopoDS_Face&) { return "face" + std::to_string(++next); });
}

// IGES: does the file state its unit? 1..11 are the units of the standard.
bool igesUnitStated(const occ::handle<Interface_InterfaceModel>& model) {
    const auto iges = occ::down_cast<IGESData_IGESModel>(model);
    if (iges.IsNull()) {
        return false;
    }
    const int flag = iges->GlobalSection().UnitFlag();
    return flag >= 1 && flag <= 11;
}

// The bytes in a file of their own that is removed again.
class TempFile {
public:
    explicit TempFile(const std::vector<std::uint8_t>& bytes) {
        static std::atomic<unsigned> counter{0};
        path_ = std::filesystem::temp_directory_path() /
                ("fission359-import-" + std::to_string(counter.fetch_add(1)) + "-" +
                 std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".iges");
        std::ofstream out(path_, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        ok_ = static_cast<bool>(out);
    }
    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    bool ok() const { return ok_; }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    bool ok_ = false;
};

} // namespace

ImportResult importFile(ImportFormat format, const std::vector<std::uint8_t>& bytes,
                        const ImportOptions& options) {
    if (!std::isfinite(options.lengthFactor) || options.lengthFactor <= 0.0) {
        throw GeometryError(GeometryErrorCode::invalidInput, "unit factor is not usable");
    }
    const std::lock_guard<std::mutex> guard(exchangeLock());
    ImportResult result;
    try {
        std::istringstream stream(std::string(bytes.begin(), bytes.end()));
        const XcafDocument owned;
        const occ::handle<TDocStd_Document>& document = owned.handle();
        if (format == ImportFormat::step) {
            STEPCAFControl_Reader reader;
            reader.SetColorMode(true);
            reader.SetNameMode(true);
            if (reader.ReadStream("file.step", stream) != IFSelect_RetDone ||
                !reader.Transfer(document)) {
                throw GeometryError(GeometryErrorCode::importUnreadable,
                                    "not a readable STEP file");
            }
        } else {
            // OCCT reads IGES only from a file.
            TempFile file(bytes);
            IGESCAFControl_Reader reader;
            reader.SetColorMode(true);
            reader.SetNameMode(true);
            if (!file.ok() || reader.ReadFile(file.path().string().c_str()) != IFSelect_RetDone) {
                throw GeometryError(GeometryErrorCode::importUnreadable,
                                    "not a readable IGES file");
            }
            result.unitAssumed = !igesUnitStated(reader.Model());
            if (!reader.Transfer(document)) {
                throw GeometryError(GeometryErrorCode::importUnreadable, "IGES file has no shapes");
            }
        }

        const occ::handle<XCAFDoc_ShapeTool> shapes =
            XCAFDoc_DocumentTool::ShapeTool(document->Main());
        const occ::handle<XCAFDoc_ColorTool> colors =
            XCAFDoc_DocumentTool::ColorTool(document->Main());
        gp_Trsf scale;
        scale.SetScale(gp_Pnt(0, 0, 0), options.lengthFactor);

        // Walks the tree of the document: an assembly hands its place down to its parts, so
        // that every part is read where the assembly puts it, with its own colour and name.
        const std::function<void(const TDF_Label&, const TopLoc_Location&, const std::string&,
                                 const std::optional<FileColor>&)>
            visit = [&](const TDF_Label& label, const TopLoc_Location& place,
                        const std::string& inheritedName,
                        const std::optional<FileColor>& inheritedColor) {
                std::string name = nameOf(label);
                std::optional<FileColor> color = inheritedColor;
                Quantity_Color own;
                for (const XCAFDoc_ColorType type : {XCAFDoc_ColorSurf, XCAFDoc_ColorGen}) {
                    if (!color && XCAFDoc_ColorTool::GetColor(label, type, own)) {
                        color = toFileColor(own);
                    }
                }
                if (XCAFDoc_ShapeTool::IsReference(label)) {
                    TDF_Label part;
                    if (XCAFDoc_ShapeTool::GetReferredShape(label, part)) {
                        visit(part, place * XCAFDoc_ShapeTool::GetLocation(label), name, color);
                    }
                    return;
                }
                if (XCAFDoc_ShapeTool::IsAssembly(label)) {
                    result.assembly = true;
                    NCollection_Sequence<TDF_Label> parts;
                    XCAFDoc_ShapeTool::GetComponents(label, parts, false);
                    for (const TDF_Label& part : parts) {
                        visit(part, place, {}, color);
                    }
                    return;
                }
                if (name.empty()) {
                    name = inheritedName;
                }
                const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(label);
                if (shape.IsNull()) {
                    ++result.skipped;
                    return;
                }
                std::vector<TopoDS_Shape> solids;
                for (TopExp_Explorer found(shape, TopAbs_SOLID); found.More(); found.Next()) {
                    solids.push_back(found.Current());
                }
                // Loose faces next to solids are not bodies.
                bool joined = false;
                if (!solids.empty()) {
                    if (TopExp_Explorer(shape, TopAbs_FACE, TopAbs_SOLID).More()) {
                        ++result.skipped;
                    }
                } else if (TopExp_Explorer(shape, TopAbs_FACE).More()) {
                    solids = solidsFromSurfaces(shape);
                    joined = !solids.empty();
                    if (!joined) {
                        ++result.skipped; // surfaces that do not close up
                    }
                }
                int index = 0;
                for (const TopoDS_Shape& solid : solids) {
                    ++index;
                    TopoDS_Shape placed = solid.Moved(place);
                    if (options.lengthFactor != 1.0) {
                        placed = BRepBuilderAPI_Transform(placed, scale, /*copy=*/true).Shape();
                    }
                    bool repaired = false;
                    const auto good = usable(placed, repaired);
                    if (!good) {
                        ++result.skipped;
                        continue;
                    }
                    ImportedBody body;
                    body.body = namedAsImported(*good);
                    const TDF_Label sub = shapes->FindShape(solid);
                    body.name = sub.IsNull() ? std::string() : nameOf(sub);
                    if (body.name.empty()) {
                        body.name = name;
                        if (!name.empty() && solids.size() > 1) {
                            body.name += " " + std::to_string(index);
                        }
                    }
                    body.color = colorOf(colors, solid);
                    if (!body.color) {
                        body.color = color;
                    }
                    body.repaired = repaired;
                    body.joined = joined;
                    result.bodies.push_back(std::move(body));
                }
            };

        NCollection_Sequence<TDF_Label> roots;
        shapes->GetFreeShapes(roots);
        for (const TDF_Label& root : roots) {
            visit(root, TopLoc_Location(), {}, std::nullopt);
        }
    } catch (const GeometryError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw GeometryError(GeometryErrorCode::importUnreadable,
                            std::string("OCCT failed reading a file: ") + failure.what());
    }
    if (result.bodies.empty()) {
        throw GeometryError(GeometryErrorCode::importEmpty, "the file holds no usable solid");
    }
    return result;
}

std::string exportStep(const std::vector<ExportBody>& bodies) {
    const std::lock_guard<std::mutex> guard(exchangeLock());
    try {
        const XcafDocument owned;
        const occ::handle<TDocStd_Document>& document = owned.handle();
        const occ::handle<XCAFDoc_ShapeTool> shapes =
            XCAFDoc_DocumentTool::ShapeTool(document->Main());
        const occ::handle<XCAFDoc_ColorTool> colors =
            XCAFDoc_DocumentTool::ColorTool(document->Main());
        for (const ExportBody& body : bodies) {
            const auto* data = dynamic_cast<const OcctSolidData*>(body.solid.data());
            if (data == nullptr) {
                throw GeometryError(GeometryErrorCode::invalidInput, "not a body of this kernel");
            }
            // Bodies of equal shape (four bolts, a body and its copy) share their geometry; XCAF
            // would give them one entry, with the name and colour of the last. A copy of its own
            // keeps each body as it is.
            // The place the body was given goes into the geometry (a body that is only a moved
            // copy would become a reference to the original).
            const TopoDS_Shape& shape = data->shape();
            const TopoDS_Shape own =
                BRepBuilderAPI_Transform(shape.Located(TopLoc_Location()),
                                         shape.Location().Transformation(), /*copy=*/true)
                    .Shape();
            const TDF_Label label = shapes->AddShape(own, /*makeAssembly=*/false);
            if (!body.name.empty()) {
                TDataStd_Name::Set(label, TCollection_ExtendedString(body.name.c_str(), true));
            }
            if (body.color) {
                colors->SetColor(label,
                                 Quantity_Color(body.color->red / 255.0, body.color->green / 255.0,
                                                body.color->blue / 255.0, Quantity_TOC_sRGB),
                                 XCAFDoc_ColorGen);
            }
        }
        STEPCAFControl_Writer writer;
        writer.SetColorMode(true);
        writer.SetNameMode(true);
        std::ostringstream out;
        if (!writer.Transfer(document) || writer.WriteStream(out) != IFSelect_RetDone) {
            throw GeometryError(GeometryErrorCode::kernelFailure, "STEP writer failed");
        }
        return out.str();
    } catch (const GeometryError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw GeometryError(GeometryErrorCode::kernelFailure,
                            std::string("OCCT failed writing STEP: ") + failure.what());
    }
}

namespace {

constexpr double kRadiansToDegrees = 57.29577951308232;

// The plane a face is drawn in: looking at the face from outside, y up.
struct DrawingPlane {
    gp_Pnt origin;
    gp_Dir x;
    gp_Dir y;

    Point2d project(const gp_Pnt& point) const {
        const gp_Vec offset(origin, point);
        return {offset.Dot(gp_Vec(x)), offset.Dot(gp_Vec(y))};
    }
};

double wrapDegrees(double angle) {
    angle = std::fmod(angle, 360.0);
    return angle < 0.0 ? angle + 360.0 : angle;
}

double angleAround(const Point2d& centre, const Point2d& point) {
    return wrapDegrees(std::atan2(point.y - centre.y, point.x - centre.x) * kRadiansToDegrees);
}

OutlineShape arcThrough(const Point2d& centre, double radius, const Point2d& from,
                        const Point2d& middle, const Point2d& to) {
    // Counter-clockwise from `a` to `b`, the one that holds `middle`.
    const double a = angleAround(centre, from);
    const double b = angleAround(centre, to);
    const double m = angleAround(centre, middle);
    const bool forward = wrapDegrees(m - a) < wrapDegrees(b - a);
    const double start = forward ? a : b;
    const double span = wrapDegrees((forward ? b : a) - start);
    return OutlineArc{centre, radius, start, start + (span > 0.0 ? span : 360.0)};
}

} // namespace

Outline faceOutline(const Solid& solid, std::uint32_t faceId) {
    const auto* data = dynamic_cast<const OcctSolidData*>(solid.data());
    if (data == nullptr) {
        throw GeometryError(GeometryErrorCode::invalidInput, "not a body of this kernel");
    }
    ShapeMap faces;
    TopExp::MapShapes(data->shape(), TopAbs_FACE, faces);
    if (faceId < 1 || faceId > static_cast<std::uint32_t>(faces.Extent())) {
        throw GeometryError(GeometryErrorCode::invalidInput, "no such face");
    }
    const TopoDS_Face face = TopoDS::Face(faces(static_cast<int>(faceId)));
    if (!isFlat(face)) {
        throw GeometryError(GeometryErrorCode::invalidInput, "the face is not flat");
    }

    // Looking at the face from outside: the first plane axis is x, the normal points at us.
    const BRepAdaptor_Surface surface(face);
    const gp_Pln plane = surface.Plane();
    const gp_Dir normal = outwardNormal(face);
    const gp_Dir xDirection = plane.Position().XDirection();
    const gp_Dir yDirection(gp_Vec(normal).Crossed(gp_Vec(xDirection)));
    DrawingPlane drawing{plane.Location(), xDirection, yDirection};

    Outline outline;
    for (TopExp_Explorer edges(face, TopAbs_EDGE); edges.More(); edges.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edges.Current());
        if (BRep_Tool::Degenerated(edge)) {
            continue;
        }
        const BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        const Point2d from = drawing.project(curve.Value(first));
        const Point2d to = drawing.project(curve.Value(last));
        switch (curve.GetType()) {
        case GeomAbs_Line:
            outline.shapes.push_back(OutlineLine{from, to});
            continue;
        case GeomAbs_Circle: {
            const Point2d centre = drawing.project(curve.Circle().Location());
            const double radius = curve.Circle().Radius();
            const bool whole = curve.IsClosed() || std::abs((last - first) - 2.0 * M_PI) < 1e-9;
            if (whole) {
                outline.shapes.push_back(OutlineCircle{centre, radius});
            } else {
                const Point2d middle = drawing.project(curve.Value(0.5 * (first + last)));
                outline.shapes.push_back(arcThrough(centre, radius, from, middle, to));
            }
            continue;
        }
        default:
            break;
        }
        // A spline, an ellipse ...: short lines within 0.005 mm.
        GCPnts_QuasiUniformDeflection sampler(curve, 0.005);
        OutlinePolyline line;
        if (sampler.IsDone()) {
            for (int i = 1; i <= sampler.NbPoints(); ++i) {
                line.points.push_back(drawing.project(sampler.Value(i)));
            }
        } else {
            line.points = {from, to};
        }
        outline.shapes.push_back(std::move(line));
    }
    return outline;
}

} // namespace fission::geo::occt
