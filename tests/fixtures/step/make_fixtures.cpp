// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Writes the files of this folder (see README.md). Not part of the build: compile it by hand
// against the OpenCASCADE modules of the project and run it with the target folder as argument.

#include <iostream>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <IGESControl_Writer.hxx>
#include <Interface_Static.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>
int main(int, char** argv) {
    std::string dir = argv[1];
    auto doc = occ::handle<TDocStd_Document>(new TDocStd_Document("MDTV-XCAF"));
    XCAFDoc_DocumentTool::Set(doc->Main());
    auto st = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    auto ct = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    auto write = [&](const std::string& name, const char* unit) {
        if (unit)
            Interface_Static::SetCVal("write.step.unit", unit);
        STEPCAFControl_Writer w;
        w.SetColorMode(true);
        w.SetNameMode(true);
        w.Transfer(doc);
        w.Write((dir + "/" + name).c_str());
        Interface_Static::SetCVal("write.step.unit", "MM");
    };
    // 1. one block with a hole, named and coloured
    TopoDS_Shape block = BRepPrimAPI_MakeBox(20, 10, 5).Shape();
    TopoDS_Shape pin =
        BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10, 5, -1), gp_Dir(0, 0, 1)), 2, 7).Shape();
    TopoDS_Shape holed = BRepAlgoAPI_Cut(block, pin).Shape();
    auto l = st->AddShape(holed, false);
    TDataStd_Name::Set(l, TCollection_ExtendedString("Block with hole"));
    ct->SetColor(l, Quantity_Color(0.8, 0.2, 0.1, Quantity_TOC_sRGB), XCAFDoc_ColorGen);
    write("block-with-hole.step", nullptr);
    // 2. one-inch cube written in inches
    doc = occ::handle<TDocStd_Document>(new TDocStd_Document("MDTV-XCAF"));
    XCAFDoc_DocumentTool::Set(doc->Main());
    st = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    ct = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    st->AddShape(BRepPrimAPI_MakeBox(25.4, 25.4, 25.4).Shape(), false);
    write("cube-one-inch.step", "IN");
    // 3. assembly: two parts, the second moved
    doc = occ::handle<TDocStd_Document>(new TDocStd_Document("MDTV-XCAF"));
    XCAFDoc_DocumentTool::Set(doc->Main());
    st = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    ct = XCAFDoc_DocumentTool::ColorTool(doc->Main());
    auto asmL = st->NewShape();
    TDataStd_Name::Set(asmL, TCollection_ExtendedString("Assembly"));
    auto a = st->AddShape(BRepPrimAPI_MakeBox(10, 10, 10).Shape(), false);
    TDataStd_Name::Set(a, TCollection_ExtendedString("Cube"));
    auto b = st->AddShape(BRepPrimAPI_MakeCylinder(4, 12).Shape(), false);
    TDataStd_Name::Set(b, TCollection_ExtendedString("Rod"));
    ct->SetColor(b, Quantity_Color(0.1, 0.3, 0.9, Quantity_TOC_sRGB), XCAFDoc_ColorGen);
    gp_Trsf mv;
    mv.SetTranslation(gp_Vec(30, 0, 0));
    st->AddComponent(asmL, a, TopLoc_Location());
    st->AddComponent(asmL, b, TopLoc_Location(mv));
    st->UpdateAssemblies();
    write("assembly-two-parts.step", nullptr);
    // 4. only a loose face (no solid)
    doc = occ::handle<TDocStd_Document>(new TDocStd_Document("MDTV-XCAF"));
    XCAFDoc_DocumentTool::Set(doc->Main());
    st = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
    st->AddShape(BRepBuilderAPI_MakeFace(gp_Pln(), 0, 10, 0, 10).Shape(), false);
    write("sheet-only.step", nullptr);
    // 5. IGES: the block with a hole
    {
        IGESControl_Writer iw("MM", 0);
        iw.AddShape(holed);
        iw.ComputeModel();
        iw.Write((dir + "/block-faces-only.iges").c_str());
    }
    {
        IGESControl_Writer iw("MM", 1);
        iw.AddShape(holed);
        iw.ComputeModel();
        iw.Write((dir + "/block-with-hole.iges").c_str());
    }
    std::cout << "ok\n";
}
