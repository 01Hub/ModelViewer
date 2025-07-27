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

struct ShapeHasher
{
    std::size_t operator()(const TopoDS_Shape& shape) const
    {
        if (shape.IsNull()) return 0;

        // Use TShape pointer as hash - this is stable and unique per shape
        const TopoDS_TShape* tshape = shape.TShape().get();
        std::size_t h1 = std::hash<const void*>{}(tshape);

        // Combine with orientation for complete uniqueness
        std::size_t h2 = std::hash<int>{}(static_cast<int>(shape.Orientation()));

        // Combine hashes
        return h1 ^ (h2 << 1);
    }
};

// Custom equality function for TopoDS_Shape
struct ShapeEqual
{
    bool operator()(const TopoDS_Shape& lhs, const TopoDS_Shape& rhs) const
    {
        return lhs.IsSame(rhs);
    }
};

// Simple cache to avoid repeated expensive lookups
class ColorCache
{
private:
    std::unordered_map<TopoDS_Shape, Quantity_Color, ShapeHasher, ShapeEqual> shapeColorCache;
    std::unordered_map<std::string, Quantity_Color> labelColorCache;

public:
    bool GetCachedColor(const TopoDS_Shape& shape, Quantity_Color& color)
    {
        if (shape.IsNull()) return false;
        auto it = shapeColorCache.find(shape);
        if (it != shapeColorCache.end())
        {
            color = it->second;
            return true;
        }
        return false;
    }

    void CacheColor(const TopoDS_Shape& shape, const Quantity_Color& color)
    {
        if (!shape.IsNull())
        {
            shapeColorCache[shape] = color;
        }
    }

    bool GetCachedLabelColor(const std::string& labelPath, Quantity_Color& color)
    {
        auto it = labelColorCache.find(labelPath);
        if (it != labelColorCache.end())
        {
            color = it->second;
            return true;
        }
        return false;
    }

    void CacheLabelColor(const std::string& labelPath, const Quantity_Color& color)
    {
        labelColorCache[labelPath] = color;
    }

    void Clear()
    {
        shapeColorCache.clear();
        labelColorCache.clear();
    }
};

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

using ShapeWithNameAndTrsf = std::tuple<TopoDS_Shape, std::string, TopLoc_Location, Quantity_Color>;

class BRepToAssimpConverter
{
public:    
    static aiScene* convert( const std::vector<ShapeWithNameAndTrsf>& shapeTuples);
    static aiScene* convert(const TopoDS_Shape& shape, const Quantity_Color& color, int& index, const std::string& name = "");    
    static aiScene* convert(
        const TopoDS_Shape& shape,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        int& meshIndex,
        const std::string& name);

    static aiNode* cloneNodeDeep(const aiNode* src);

    static void ClearColorCache();


private:
   
    static aiMesh* convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex);

    static std::vector<aiMesh*> convertFaceGroupToMeshesWithCache(
        const TopTools_IndexedMapOfShape& faceGroup,
        int& meshIndex,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        std::map<Quantity_Color, unsigned int, QuantityColorComparator>& materialMap,
        std::vector<aiMaterial*>& materials);

    static bool GetComprehensiveColorWithCache(
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const TopoDS_Shape& shape,
        const TDF_Label& defLabel,
        const TDF_Label& instanceLabel,
        Quantity_Color& outColor);

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

    static Standard_Real computeDeflectionFromBBox(const TopTools_IndexedMapOfShape& faceGroup, Standard_Real percent = 0.01);
    static TopoDS_Face healAndTriangulateFace(const TopoDS_Face& inputFace,
        double deflection = 0.01,
        double angularDeflection = 0.5,
        double fixTolerance = 1e-6);

    private:
        static ColorCache s_colorCache;
};
