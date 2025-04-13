#pragma once
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <assimp/scene.h>

class BRepToAssimpConverter {
public:
    static aiScene* Convert(const TopoDS_Shape& shape);

private:
    static void convertSolidToMesh(const TopoDS_Solid& solid, aiMesh*& mesh, int meshIndex);
    static aiMesh* convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex);
};
