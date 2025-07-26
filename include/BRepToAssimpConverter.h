#pragma once

#include <Quantity_Color.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_HSequenceOfShape.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <assimp/scene.h>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <map>

using ShapeWithNameAndTrsf = std::tuple<TopoDS_Shape, std::string, TopLoc_Location, Quantity_Color>;

struct QuantityColorComparator
{
    constexpr static double EPSILON = 1e-6;
    bool operator()(const Quantity_Color& lhs, const Quantity_Color& rhs) const
    {
        if (std::abs(lhs.Red() - rhs.Red()) > EPSILON)
            return lhs.Red() < rhs.Red();
        if (std::abs(lhs.Green() - rhs.Green()) > EPSILON)
            return lhs.Green() < rhs.Green();
        return lhs.Blue() < rhs.Blue();
    }
};


class BRepToAssimpConverter
{
public:    
    static aiScene* convert( const std::vector<ShapeWithNameAndTrsf>& shapeTuples);
    static aiScene* convert(const TopoDS_Shape& shape, const Quantity_Color& color, int& index, const std::string& name = "");
    static aiMesh* convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex);
    static aiNode* cloneNodeDeep(const aiNode* src);

    static aiScene* convert(
        const TopoDS_Shape& shape,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        int& meshIndex,
        const std::string& name);

    static std::vector<aiMesh*> convertFaceGroupToMeshes(
        const TopTools_IndexedMapOfShape& faceGroup,
        int& meshIndex,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        std::map<Quantity_Color, unsigned int, QuantityColorComparator>& materialMap,
        std::vector<aiMaterial*>& materials);

    static bool GetComprehensiveColor(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TopoDS_Shape& shape,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        Quantity_Color& outColor);

    static std::string ColorTypeToString(XCAFDoc_ColorType type);

    static bool SearchChildLabelsForColor(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TDF_Label& parentLabel,
        Quantity_Color& outColor);

    static bool SearchParentLabelsForColor(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TDF_Label& startLabel,
        Quantity_Color& outColor);

    static bool SearchAllColorsForAssociation(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TopoDS_Shape& shape,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        Quantity_Color& outColor);

    static bool SearchShapeLabelForTargetWithColor(
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TDF_Label& shapeLabel,
        const TopoDS_Shape& targetShape,
        Quantity_Color& outColor);

    static std::string ColorToString(const Quantity_Color& color);
    static std::string ShapeTypeToString(TopAbs_ShapeEnum shapeType);

    static bool ColorsEqual(const Quantity_Color& color1, const Quantity_Color& color2, double tolerance = 0.001);

    static std::string GetLabelPath(const TDF_Label& label);

    static bool FindColorInXCAFDocument(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const TopoDS_Shape& targetShape,
        Quantity_Color& outColor);

    static bool SearchShapeLabelForTarget(
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TDF_Label& shapeLabel,
        const TopoDS_Shape& targetShape,
        Quantity_Color& outColor);

    static bool ContainsShape(const TopoDS_Shape& compound, const TopoDS_Shape& target);

    static bool SearchLabelHierarchyForColor(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TDF_Label& startLabel,
        Quantity_Color& outColor);


private:
    static Standard_Real computeDeflectionFromBBox(const TopTools_IndexedMapOfShape& faceGroup, Standard_Real percent = 0.01);
    static TopoDS_Face healAndTriangulateFace(const TopoDS_Face& inputFace,
                                   double deflection = 0.01,
                                   double angularDeflection = 0.5,
                                   double fixTolerance = 1e-6);
};
