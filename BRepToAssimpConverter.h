#pragma once

#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <assimp/scene.h>

class BRepToAssimpConverter
{
public:
    static aiScene* convert(const Handle(TopTools_HSequenceOfShape)& shapeSeq);

private:
    static aiScene* convert(const TopoDS_Shape& shape, int& index);
    static aiMesh* convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex);
    static aiNode* cloneNodeDeep(const aiNode* src);
    static Standard_Real computeDeflectionFromBBox(const TopTools_IndexedMapOfShape& faceGroup, Standard_Real percent = 0.01);
};
