// --- Helper to get color from label ---
bool XCAFDocProcessor::GetShapeColorFromLabel(
    const Handle(XCAFDoc_ColorTool)& colorTool,
    const TDF_Label& label,
    Quantity_Color& outColor)
{
    static const XCAFDoc_ColorType colorTypes[] = {
        XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
    };

    for (XCAFDoc_ColorType type : colorTypes)
    {
        if (colorTool->GetColor(label, type, outColor))
            return true;
    }
    return false;
}

// --- Helper to get color from shape ---
bool XCAFDocProcessor::GetShapeColorFromShape(
    const Handle(XCAFDoc_ColorTool)& colorTool,
    const TopoDS_Shape& shape,
    Quantity_Color& outColor)
{
    static const XCAFDoc_ColorType colorTypes[] = {
        XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
    };

    for (XCAFDoc_ColorType type : colorTypes)
    {
        if (colorTool->GetColor(shape, type, outColor))
            return true;
        if (colorTool->GetInstanceColor(shape, type, outColor))
            return true;
    }
    return false;
}

// --- Updated convertFaceGroupToMesh with face-level color support ---
std::vector<aiMesh*> BRepToAssimpConverter::convertFaceGroupToMeshes(
    const TopTools_IndexedMapOfShape& faceGroup,
    int& meshIndex,
    const Handle(XCAFDoc_ColorTool)& colorTool,
    std::map<Quantity_Color, unsigned int>& materialMap,
    std::vector<aiMaterial*>& materials)
{
    std::map<Quantity_Color, std::vector<TopoDS_Face>> colorFaceGroups;

    for (int f = 1; f <= faceGroup.Extent(); ++f)
    {
        TopoDS_Face face = TopoDS::Face(faceGroup(f));
        Quantity_Color faceColor;
        bool hasColor = false;

        if (!colorTool.IsNull())
        {
            hasColor = XCAFDocProcessor::GetShapeColorFromShape(colorTool, face, faceColor);
        }

        if (!hasColor)
        {
            faceColor = Quantity_NOC_GRAY95;
        }

        colorFaceGroups[faceColor].push_back(face);
    }

    std::vector<aiMesh*> meshes;

    for (const auto& entry : colorFaceGroups)
    {
        const Quantity_Color& color = entry.first;
        const std::vector<TopoDS_Face>& faces = entry.second;

        TopTools_IndexedMapOfShape faceMap;
        for (const TopoDS_Face& face : faces)
        {
            faceMap.Add(face);
        }

        aiMesh* mesh = convertSingleColorFaceGroup(faceMap, meshIndex);
        if (!mesh) continue;

        mesh->mName = "Mesh_" + std::to_string(meshIndex);
        ++meshIndex;

        if (materialMap.find(color) == materialMap.end())
        {
            aiMaterial* material = new aiMaterial();
            aiColor3D diffuseColor(color.Red(), color.Green(), color.Blue());
            material->AddProperty(&diffuseColor, 1, AI_MATKEY_COLOR_DIFFUSE);
            materialMap[color] = materials.size();
            materials.push_back(material);
        }

        mesh->mMaterialIndex = materialMap[color];
        meshes.push_back(mesh);
    }

    return meshes;
}

// --- Updated convert function ---
aiScene* BRepToAssimpConverter::convert(
    const TopoDS_Shape& shape,
    const Handle(XCAFDoc_ColorTool)& colorTool,
    int& meshIndex,
    const std::string& name)
{
    auto* scene = new aiScene();
    scene->mRootNode = new aiNode();
    scene->mRootNode->mName = aiString(name);

    std::vector<aiMesh*> meshes;
    std::vector<aiMaterial*> materials;
    std::map<Quantity_Color, unsigned int> materialMap;

    TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
    if (!solidExplorer.More())
    {
        TopTools_IndexedMapOfShape faceGroup;
        TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
        for (; faceExplorer.More(); faceExplorer.Next())
        {
            faceGroup.Add(faceExplorer.Current());
        }

        auto faceMeshes = convertFaceGroupToMeshes(faceGroup, meshIndex, colorTool, materialMap, materials);
        meshes.insert(meshes.end(), faceMeshes.begin(), faceMeshes.end());
    }
    else
    {
        for (; solidExplorer.More(); solidExplorer.Next())
        {
            TopoDS_Solid solid = TopoDS::Solid(solidExplorer.Current());
            TopTools_IndexedMapOfShape faceGroup;
            TopExp_Explorer faceExplorer(solid, TopAbs_FACE);
            for (; faceExplorer.More(); faceExplorer.Next())
            {
                faceGroup.Add(faceExplorer.Current());
            }

            auto faceMeshes = convertFaceGroupToMeshes(faceGroup, meshIndex, colorTool, materialMap, materials);
            meshes.insert(meshes.end(), faceMeshes.begin(), faceMeshes.end());
        }
    }

    scene->mNumMeshes = meshes.size();
    scene->mMeshes = new aiMesh*[meshes.size()];
    for (size_t i = 0; i < meshes.size(); ++i)
    {
        scene->mMeshes[i] = meshes[i];
    }

    scene->mRootNode->mNumMeshes = meshes.size();
    scene->mRootNode->mMeshes = new unsigned int[meshes.size()];
    for (size_t i = 0; i < meshes.size(); ++i)
    {
        scene->mRootNode->mMeshes[i] = i;
    }

    scene->mNumMaterials = materials.size();
    scene->mMaterials = new aiMaterial*[materials.size()];
    for (size_t i = 0; i < materials.size(); ++i)
    {
        scene->mMaterials[i] = materials[i];
    }

    return scene;
}
