#pragma once

#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <assimp/scene.h>

using ShapeWithNameAndTrsf = std::tuple<TopoDS_Shape, std::string, TopLoc_Location, Quantity_Color>;

class BRepToAssimpConverter
{
public:    
    static aiScene* convert( const std::vector<ShapeWithNameAndTrsf>& shapeTuples);
    static aiScene* convert(const TopoDS_Shape& shape, const Quantity_Color& color, int& index, const std::string& name = "");
    static aiMesh* convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex);
    static aiNode* cloneNodeDeep(const aiNode* src);

private:
    static Standard_Real computeDeflectionFromBBox(const TopTools_IndexedMapOfShape& faceGroup, Standard_Real percent = 0.01);
    static TopoDS_Face healAndTriangulateFace(const TopoDS_Face& inputFace,
                                   double deflection = 0.01,
                                   double angularDeflection = 0.5,
                                   double fixTolerance = 1e-6);
};
