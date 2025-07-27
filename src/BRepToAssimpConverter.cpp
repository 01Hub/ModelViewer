#include "BRepToAssimpConverter.h"
#include "MainWindow.h"
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Poly_Triangulation.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Wire.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_ChildIterator.hxx>
#include <TDF_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <vector>
#include <XCAFDoc_DocumentTool.hxx>


ColorCache BRepToAssimpConverter::s_colorCache;

/**
 * Converts a vector of shapes (with name, transformation, and color) into an Assimp aiScene.
 *
 * This method iterates over each shape in the provided vector, applies the specified transformation,
 * and converts each shape into a sub-scene. The resulting meshes and nodes are aggregated into a single
 * aiScene with a root node named "Root".
 *
 * @param shapeTuples A vector of tuples containing shapes, names, transformations, and colors.
 * @return Pointer to the newly created aiScene containing all converted shapes,
 *         or nullptr if the input vector is empty.
 */
aiScene* BRepToAssimpConverter::convert(const std::vector<ShapeWithNameAndTrsf>& shapeTuples)
{
	if (shapeTuples.empty())
		return nullptr;

	aiScene* scene = new aiScene();
	scene->mRootNode = new aiNode();
	scene->mRootNode->mName = aiString("Root");

	std::vector<aiMesh*> meshList;
	std::vector<aiMaterial*> materialList;
	std::vector<aiNode*> childNodes;

	int meshIndex = 0;
	int totalCount = shapeTuples.size();

	for (const auto& tuple : shapeTuples)
	{
		int progress = (int)(((float)meshIndex / (float)totalCount) * 100);
		MainWindow::setProgressValue(progress);
		const TopoDS_Shape& shape = std::get<0>(tuple);
		const std::string& name = std::get<1>(tuple);
		const gp_Trsf& trsf = std::get<2>(tuple);
		const Quantity_Color& color = std::get<3>(tuple);

		// Apply transformation to the shape
		TopoDS_Shape transformedShape = shape;
		if (!trsf.Form() == gp_Identity)
		{
			BRepBuilderAPI_Transform trsfBuilder(shape, trsf, true);
			transformedShape = trsfBuilder.Shape();
		}

		// Convert the shape into a subscene with name
		aiScene* subScene = convert(transformedShape, color, meshIndex, name); // <== this version must accept name

		if (subScene && subScene->mNumMeshes > 0)
		{
			unsigned int meshBase = static_cast<unsigned int>(meshList.size());
			unsigned int materialBase = static_cast<unsigned int>(materialList.size());

			// Append meshes
			for (unsigned int m = 0; m < subScene->mNumMeshes; ++m)
				meshList.push_back(subScene->mMeshes[m]);

			for (unsigned int i = 0; i < subScene->mNumMaterials; ++i)
			{
				materialList.push_back(subScene->mMaterials[i]);
			}


			// Apply color and fix mMaterialIndex
			for (unsigned int m = meshBase; m < meshBase + subScene->mNumMeshes; ++m)
			{
				aiMesh* mesh = meshList[m];

				// Correct the material index!
				// Find which material this mesh should use
				unsigned int originalMatIndex = mesh->mMaterialIndex;
				mesh->mMaterialIndex = materialBase + originalMatIndex; // Correct material index

				// Only modify the material if it's within bounds
				if (materialBase + originalMatIndex < materialList.size())
				{
					aiMaterial* material = materialList[materialBase + originalMatIndex];

					// Set color only if no texture
					if (material->GetTextureCount(aiTextureType_DIFFUSE) == 0)
					{
						// Diffuse color (from STEP model)
						aiColor3D diffuseColor(color.Red(), color.Green(), color.Blue());
						material->AddProperty(&diffuseColor, 1, AI_MATKEY_COLOR_DIFFUSE);

						// Ambient color: dimmed diffuse (30%)
						aiColor3D ambientColor = diffuseColor * 0.3f;
						material->AddProperty(&ambientColor, 1, AI_MATKEY_COLOR_AMBIENT);

						// Specular color: bright white
						aiColor3D specularColor(0.8f, 0.8f, 1.0f);
						material->AddProperty(&specularColor, 1, AI_MATKEY_COLOR_SPECULAR);

						// Shininess
						float shininess = 24.0f;
						material->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);

					}
				}
			}

			if (subScene->mRootNode)
			{
				aiNode* nodeCopy = cloneNodeDeep(subScene->mRootNode);

				// Adjust mesh indices
				for (unsigned int j = 0; j < nodeCopy->mNumMeshes; ++j)
					nodeCopy->mMeshes[j] += meshBase;

				// Optionally override node name to shape name
				if (!name.empty())
					nodeCopy->mName = aiString(name);

				childNodes.push_back(nodeCopy);
			}

			// Clean up subscene
			subScene->mMeshes = nullptr;
			subScene->mMaterials = nullptr;
			subScene->mRootNode = nullptr;
			subScene->mNumMeshes = 0;
			subScene->mNumMaterials = 0;
			delete subScene;
		}
	}

	// Attach all child nodes to the root node
	scene->mRootNode->mNumChildren = static_cast<unsigned int>(childNodes.size());
	if (!childNodes.empty())
	{
		scene->mRootNode->mChildren = new aiNode * [childNodes.size()];
		std::copy(childNodes.begin(), childNodes.end(), scene->mRootNode->mChildren);
	}

	// Finalize the scene
	scene->mNumMeshes = static_cast<unsigned int>(meshList.size());
	scene->mMeshes = new aiMesh * [scene->mNumMeshes];
	std::copy(meshList.begin(), meshList.end(), scene->mMeshes);

	scene->mNumMaterials = static_cast<unsigned int>(materialList.size());
	scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
	std::copy(materialList.begin(), materialList.end(), scene->mMaterials);

	return scene;
}

/**
 * @brief Converts a TopoDS_Shape (from OpenCASCADE) into an aiScene (from Assimp).
 *
 * This function traverses the input shape to extract solids or faces and converts them into Assimp meshes.
 * Each mesh is added to the scene with a corresponding node. The meshIndex is used to uniquely name and index meshes.
 *
 * @param shape The input TopoDS_Shape to convert.
 * @param meshIndex Reference to an integer used to assign unique indices to generated meshes. It is incremented as meshes are created.
 * @return aiScene* Pointer to the newly created aiScene containing the converted meshes and nodes.
 */
aiScene* BRepToAssimpConverter::convert(const TopoDS_Shape& shape, const Quantity_Color& color, int& meshIndex, const std::string& name)
{
	// Create a new Assimp scene and its root node
	auto* scene = new aiScene();
	scene->mRootNode = new aiNode();
	scene->mRootNode->mName = aiString(name); // Assign the name to the root node

	// Vectors to hold generated meshes
	std::vector<aiMesh*> meshes;

	// Explorer to find solids within the shape
	TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
	if (!solidExplorer.More())
	{
		// No solids found in the shape, fallback to extracting faces directly

		// Indexed map to collect faces
		TopTools_IndexedMapOfShape faceGroup;

		// Explorer to iterate over faces in the shape
		TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
		for (; faceExplorer.More(); faceExplorer.Next())
		{
			faceGroup.Add(faceExplorer.Current());
		}

		// Convert the collected faces into a single mesh
		aiMesh* mesh = convertFaceGroupToMesh(faceGroup, meshIndex);
		if (mesh)
		{
			// Add the mesh to the list
			meshes.push_back(mesh);

			// Name the mesh directly
			mesh->mName = name;

			// Increment mesh index for next mesh
			++meshIndex;
		}
	}
	else
	{
		// Solids found, process each solid separately
		for (; solidExplorer.More(); solidExplorer.Next())
		{
			// Extract the current solid
			TopoDS_Solid solid = TopoDS::Solid(solidExplorer.Current());

			// Indexed map to collect faces of the solid
			TopTools_IndexedMapOfShape faceGroup;

			// Explorer to iterate over faces in the solid
			TopExp_Explorer faceExplorer(solid, TopAbs_FACE);
			for (; faceExplorer.More(); faceExplorer.Next())
			{
				faceGroup.Add(faceExplorer.Current());
			}

			// Convert the collected faces into a mesh
			aiMesh* mesh = convertFaceGroupToMesh(faceGroup, meshIndex);
			if (mesh)
			{
				// Add the mesh to the list
				meshes.push_back(mesh);

				// Name the mesh directly
				mesh->mName = name;

				// Increment mesh index for next mesh
				++meshIndex;
			}
		}
	}

	// Assign the collected meshes to the scene
	scene->mNumMeshes = meshes.size();
	scene->mMeshes = new aiMesh * [scene->mNumMeshes];
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		scene->mMeshes[i] = meshes[i];
	}

	// Assign the meshes directly to the root node
	scene->mRootNode->mNumMeshes = meshes.size();
	scene->mRootNode->mMeshes = new unsigned int[meshes.size()];
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		scene->mRootNode->mMeshes[i] = i; // Index corresponds to the mesh in `scene->mMeshes`
	}

	// Create a default material for the scene
	scene->mMaterials = new aiMaterial * [1];
	aiColor3D aiColor(color.Red(), color.Green(), color.Blue());
	aiMaterial* material = new aiMaterial;
	material->AddProperty(&aiColor, 1, AI_MATKEY_COLOR_DIFFUSE);
	scene->mMaterials[0] = material;
	scene->mNumMaterials = 1;

	// Return the constructed scene
	return scene;
}


// Improved convert for reading step colours
// This version uses XCAFDoc_ColorTool and XCAFDoc_ShapeTool to extract colors
// and handles the case where the shape is a solid or a face group.
// Colors are cached to avoid redundant lookups.
aiScene* BRepToAssimpConverter::convert(
	const TopoDS_Shape& shape,
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const TDF_Label& defLabel,
	const TDF_Label& instanceLabel,
	int& meshIndex,
	const std::string& name)
{
	auto* scene = new aiScene();
	scene->mRootNode = new aiNode();

	std::vector<aiMesh*> meshes;
	std::vector<aiMaterial*> materials;
	std::map<Quantity_Color, unsigned int, QuantityColorComparator> materialMap;

	TopExp_Explorer solidExplorer(shape, TopAbs_SOLID);
	if (!solidExplorer.More())
	{
		TopTools_IndexedMapOfShape faceGroup;
		TopExp_Explorer faceExplorer(shape, TopAbs_FACE);
		for (; faceExplorer.More(); faceExplorer.Next())
		{
			faceGroup.Add(faceExplorer.Current());
		}

		auto faceMeshes = convertFaceGroupToMeshesWithCache(faceGroup, meshIndex, colorTool, shapeTool, defLabel, instanceLabel, materialMap, materials);
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
			auto faceMeshes = convertFaceGroupToMeshesWithCache(faceGroup, meshIndex, colorTool, shapeTool, defLabel, instanceLabel, materialMap, materials);
			meshes.insert(meshes.end(), faceMeshes.begin(), faceMeshes.end());
		}
	}

	// Extract actual part name
	std::string actualPartName = name;
	Handle(TDataStd_Name) nameAttr;
	if (defLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr))
	{
		std::string defLabelName = TCollection_AsciiString(nameAttr->Get()).ToCString();
		if (!defLabelName.empty() && defLabelName != "Unnamed")
		{
			actualPartName = defLabelName;
		}
	}

	if (actualPartName == name || actualPartName.empty() || actualPartName == "Unnamed")
	{
		if (instanceLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr))
		{
			std::string instanceLabelName = TCollection_AsciiString(nameAttr->Get()).ToCString();
			if (!instanceLabelName.empty() && instanceLabelName != "Unnamed")
			{
				actualPartName = instanceLabelName;
			}
		}
	}

	// Fix mesh names
	for (size_t i = 0; i < meshes.size(); ++i)
	{
		if (meshes.size() == 1)
		{
			meshes[i]->mName = aiString(actualPartName);
		}
		else
		{
			meshes[i]->mName = aiString(actualPartName + "_" + std::to_string(i + 1));
		}
	}

	scene->mRootNode->mName = aiString(actualPartName);

	// Set up scene
	scene->mNumMeshes = meshes.size();
	scene->mMeshes = new aiMesh * [meshes.size()];
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
	scene->mMaterials = new aiMaterial * [materials.size()];
	for (size_t i = 0; i < materials.size(); ++i)
	{
		scene->mMaterials[i] = materials[i];
	}

	return scene;
}

// Utility method to clear caches (call this when processing a new document)
void BRepToAssimpConverter::ClearColorCache()
{
	s_colorCache.Clear();
}

// convertFaceGroupToMeshes logic with just caching added
std::vector<aiMesh*> BRepToAssimpConverter::convertFaceGroupToMeshesWithCache(
	const TopTools_IndexedMapOfShape& faceGroup,
	int& meshIndex,
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const TDF_Label& defLabel,
	const TDF_Label& instanceLabel,
	std::map<Quantity_Color, unsigned int, QuantityColorComparator>& materialMap,
	std::vector<aiMaterial*>& materials)
{
	std::map<Quantity_Color, std::vector<TopoDS_Face>, QuantityColorComparator> colorFaceGroups;

	// First, try to get a shape-level color for all faces
	Quantity_Color shapeColor;
	bool hasShapeColor = false;

	if (faceGroup.Extent() > 0)
	{
		// Build compound from all faces
		TopoDS_Compound compound;
		BRep_Builder builder;
		builder.MakeCompound(compound);

		for (int f = 1; f <= faceGroup.Extent(); ++f)
		{
			builder.Add(compound, faceGroup(f));
		}

		// Check cache first for compound
		if (!s_colorCache.GetCachedColor(compound, shapeColor))
		{
			// Comprehensive color search
			hasShapeColor = GetComprehensiveColor(colorTool, compound, defLabel, instanceLabel, shapeColor);

			// Alternative: try the complete document search
			if (!hasShapeColor && !shapeTool.IsNull())
			{
				hasShapeColor = FindColorInXCAFDocument(colorTool, shapeTool, compound, shapeColor);
			}

			// Cache the result
			if (hasShapeColor)
			{
				s_colorCache.CacheColor(compound, shapeColor);
			}
		}
		else
		{
			hasShapeColor = true;
		}
	}

	// Process each face with caching
	for (int f = 1; f <= faceGroup.Extent(); ++f)
	{
		TopoDS_Face face = TopoDS::Face(faceGroup(f));
		Quantity_Color faceColor;
		bool hasFaceColor = false;

		// Check cache first
		if (s_colorCache.GetCachedColor(face, faceColor))
		{
			hasFaceColor = true;
		}
		else
		{
			// Comprehensive color search
			if (!colorTool.IsNull())
			{
				hasFaceColor = GetComprehensiveColor(colorTool, face, defLabel, instanceLabel, faceColor);

				// Alternative: try complete document search for this face
				if (!hasFaceColor && !shapeTool.IsNull())
				{
					hasFaceColor = FindColorInXCAFDocument(colorTool, shapeTool, face, faceColor);
				}
			}

			// Fall back to shape color if no face-specific color
			if (!hasFaceColor && hasShapeColor)
			{
				faceColor = shapeColor;
				hasFaceColor = true;
			}

			// Final fallback to gray
			if (!hasFaceColor)
			{
				faceColor = Quantity_NOC_GRAY95;
			}

			// Cache the result
			s_colorCache.CacheColor(face, faceColor);
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

		aiMesh* mesh = convertFaceGroupToMesh(faceMap, meshIndex);
		if (!mesh) continue;

		mesh->mName = "Mesh_" + std::to_string(meshIndex);
		++meshIndex;

		if (materialMap.find(color) == materialMap.end())
		{
			aiMaterial* material = new aiMaterial();
			aiColor3D diffuseColor(color.Red(), color.Green(), color.Blue());
			material->AddProperty(&diffuseColor, 1, AI_MATKEY_COLOR_DIFFUSE);

			// Add ambient, specular, etc. 
			aiColor3D ambientColor = diffuseColor * 0.3f;
			material->AddProperty(&ambientColor, 1, AI_MATKEY_COLOR_AMBIENT);

			aiColor3D specularColor(0.8f, 0.8f, 1.0f);
			material->AddProperty(&specularColor, 1, AI_MATKEY_COLOR_SPECULAR);

			float shininess = 24.0f;
			material->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);

			materialMap[color] = materials.size();
			materials.push_back(material);
		}

		mesh->mMaterialIndex = materialMap[color];
		meshes.push_back(mesh);
	}

	return meshes;
}

/**
 * @brief Converts a group of CAD faces into an Assimp mesh.
 *
 * This function takes a group of OpenCascade faces, tessellates each face into triangles,
 * computes vertex positions, normals, and texture coordinates, and combines them into
 * a single aiMesh object usable by the Assimp library.
 *
 * The texture coordinates are generated by projecting vertices onto the XY-plane and
 * normalizing within the bounding box of each face's vertices to map UVs between 0 and 1.
 *
 * @param faceGroup A map of TopoDS_Shapes representing faces to convert into the mesh.
 * @param meshIndex An integer index that can be used to identify the created mesh (unused here).
 * @return aiMesh* Pointer to the generated Assimp mesh. Returns nullptr if no vertices are processed.
 */
aiMesh* BRepToAssimpConverter::convertFaceGroupToMesh(const TopTools_IndexedMapOfShape& faceGroup, int meshIndex)
{
	// Container vectors for the combined mesh data: vertices, normals, texture coordinates, and faces
	std::vector<aiVector3D> vertices, normals, texCoords;
	std::vector<aiFace> faces;
	int vertexOffset = 0; // Tracks global vertex index offset for face indices

	// Compute deflection for mesh tessellation based on the bounding box of the entire face group
	// Deflection controls the tessellation granularity (here 5% of bounding box diagonal)
	Standard_Real deflection = computeDeflectionFromBBox(faceGroup, 0.05);

	// Loop over each face in the input group
	for (int f = 1; f <= faceGroup.Extent(); ++f)
	{
		TopoDS_Face face = TopoDS::Face(faceGroup(f));

		BRepTools::Clean(face); // Clean the face to remove any artifacts

		// Generate mesh triangulation on the face with the computed deflection
		BRepMesh_IncrementalMesh mesher(face, deflection);

		// Retrieve the triangulation and its location transformation on the face
		TopLoc_Location loc;
		Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
		if (triangulation.IsNull())
		{
			//std::cout << "Failed to triangulate face " << f << " trying to heal..."<< std::endl;
			BRepCheck_Analyzer analyzer(face);
			if (!analyzer.IsValid())
			{
				//std::cout << "Face is invalid. Attempting to heal..." << std::endl;
				TopoDS_Face healedFace = healAndTriangulateFace(face);
				TopLoc_Location loc;
				triangulation = BRep_Tool::Triangulation(face, loc);
				if (triangulation.IsNull())
				{
					//std::cout << "Unable to heal face, skipping..." << std::endl;
					continue; // Skip faces that failed to heal
				}
				else
				{
					std::cout << "Face healed successfully..." << std::endl;
				}
			}
			else
			{
				//std::cout << "Face is valid, but triangulation failed..." << std::endl;
				continue; // Skip faces that failed to triangulate
			}
		}

		gp_Trsf trsf = loc.Transformation();

		int nNodes = triangulation->NbNodes();
		int nTriangles = triangulation->NbTriangles();

		// Initialize bounding box for UV coordinate computation
		double minX = 1e10, maxX = -1e10, minY = 1e10, maxY = -1e10;
		std::vector<gp_Pnt> points(nNodes);

		// Transform all nodes in the triangulation to global coordinates
		// and compute bounding box in the XY plane to generate UV map
		for (int i = 1; i <= nNodes; ++i)
		{
			gp_Pnt p = triangulation->Node(i).Transformed(trsf);
			points[i - 1] = p;
			minX = std::min(minX, p.X());
			maxX = std::max(maxX, p.X());
			minY = std::min(minY, p.Y());
			maxY = std::max(maxY, p.Y());
		}
		// Avoid division by zero in UV normalization
		double deltaX = (maxX - minX == 0) ? 1 : (maxX - minX);
		double deltaY = (maxY - minY == 0) ? 1 : (maxY - minY);

		// Local containers for the current face's vertex data
		std::vector<aiVector3D> localVertices;
		std::vector<aiVector3D> localNormals(nNodes);
		//std::vector<aiVector3D> localUVs;

		// Create local vertex positions and UV texture coordinates from points
		for (int i = 0; i < nNodes; ++i)
		{
			const gp_Pnt& p = points[i];
			localVertices.emplace_back(p.X(), p.Y(), p.Z());
			// UV coordinates normalized based on XY bounding box of the face
			//localUVs.emplace_back((p.X() - minX) / deltaX, (p.Y() - minY) / deltaY, 0.0f);
		}

		// Calculate normals per triangle and assign them to the vertices of the triangle
		for (int i = 1; i <= nTriangles; ++i)
		{
			Standard_Integer n1, n2, n3;
			triangulation->Triangle(i).Get(n1, n2, n3);
			// Convert 1-based indices to 0-based
			--n1; --n2; --n3;

			const aiVector3D& v0 = localVertices[n1];
			const aiVector3D& v1 = localVertices[n2];
			const aiVector3D& v2 = localVertices[n3];

			// Compute face normal using cross product of two edges
			aiVector3D normal = (v1 - v0) ^ (v2 - v0);
			normal.Normalize();

			// Assign the computed normal to each vertex of the triangle
			// This simple approach assigns face normals directly, no smoothing done here
			localNormals[n1] = normal;
			localNormals[n2] = normal;
			localNormals[n3] = normal;

			// Construct a face with indices adjusted for the global vertex offset
			aiFace face;
			face.mNumIndices = 3;
			face.mIndices = new unsigned int[3] {
				static_cast<unsigned int>(vertexOffset + n1),
					static_cast<unsigned int>(vertexOffset + n2),
					static_cast<unsigned int>(vertexOffset + n3)
				};
			faces.push_back(face);
		}

		// Append the local face data to the global containers
		vertices.insert(vertices.end(), localVertices.begin(), localVertices.end());
		//texCoords.insert(texCoords.end(), localUVs.begin(), localUVs.end());
		normals.insert(normals.end(), localNormals.begin(), localNormals.end());
		vertexOffset = vertices.size(); // Update vertex offset for next face
	}

	// Return nullptr if no vertices were created (empty mesh)
	if (vertices.empty()) return nullptr;

	// Create new Assimp mesh and allocate memory for vertex data
	aiMesh* mesh = new aiMesh();
	mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
	mesh->mNumVertices = vertices.size();
	mesh->mVertices = new aiVector3D[vertices.size()];
	mesh->mNormals = new aiVector3D[normals.size()];
	//mesh->mTextureCoords[0] = new aiVector3D[texCoords.size()];
	mesh->mNumUVComponents[0] = 2; // UV coordinates have two components (u,v)

	// Copy vertex positions, normals, and texture coordinates into the mesh arrays
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		mesh->mVertices[i] = vertices[i];
		mesh->mNormals[i] = normals[i];
		//mesh->mTextureCoords[0][i] = texCoords[i];
	}

	// Allocate and copy face (triangle) data
	mesh->mNumFaces = faces.size();
	mesh->mFaces = new aiFace[faces.size()];
	for (size_t i = 0; i < faces.size(); ++i)
	{
		mesh->mFaces[i] = faces[i];
	}

	// Default material index assigned to zero since no material handling here
	mesh->mMaterialIndex = 0;

	return mesh;
}


// step colors support
std::vector<aiMesh*> BRepToAssimpConverter::convertFaceGroupToMeshes(const TopTools_IndexedMapOfShape& faceGroup, int& meshIndex, const Handle(XCAFDoc_ColorTool)& colorTool, const Handle(XCAFDoc_ShapeTool)& shapeTool, const TDF_Label& defLabel, const TDF_Label& instanceLabel, std::map<Quantity_Color, unsigned int, QuantityColorComparator>& materialMap, std::vector<aiMaterial*>& materials)
{
	std::map<Quantity_Color, std::vector<TopoDS_Face>, QuantityColorComparator> colorFaceGroups;

	// First, try to get a shape-level color for all faces
	Quantity_Color shapeColor;
	bool hasShapeColor = false;

	if (faceGroup.Extent() > 0)
	{
		// Build compound from all faces
		TopoDS_Compound compound;
		BRep_Builder builder;
		builder.MakeCompound(compound);

		for (int f = 1; f <= faceGroup.Extent(); ++f)
		{
			builder.Add(compound, faceGroup(f));
		}

		hasShapeColor = GetComprehensiveColor(colorTool, compound, defLabel, instanceLabel, shapeColor);

		// Alternative: try the complete document search
		if (!hasShapeColor && !shapeTool.IsNull())
		{
			hasShapeColor = FindColorInXCAFDocument(colorTool, shapeTool, compound, shapeColor);
		}
	}

	//std::cout << "Processing " << faceGroup.Extent() << " faces. Shape-level color found: "
		//<< (hasShapeColor ? "YES" : "NO") << std::endl;

	for (int f = 1; f <= faceGroup.Extent(); ++f)
	{
		TopoDS_Face face = TopoDS::Face(faceGroup(f));
		Quantity_Color faceColor;
		bool hasFaceColor = false;

		//std::cout << "Processing face " << f << "/" << faceGroup.Extent() << std::endl;

		// Try to get face-specific color first
		if (!colorTool.IsNull())
		{
			hasFaceColor = GetComprehensiveColor(colorTool, face, defLabel, instanceLabel, faceColor);

			// Alternative: try complete document search for this face
			if (!hasFaceColor && !shapeTool.IsNull())
			{
				hasFaceColor = FindColorInXCAFDocument(colorTool, shapeTool, face, faceColor);
			}
		}

		// Fall back to shape color if no face-specific color
		if (!hasFaceColor && hasShapeColor)
		{
			faceColor = shapeColor;
			hasFaceColor = true;
			//std::cout << "Using shape-level color for face " << f << std::endl;
		}

		// Final fallback to gray
		if (!hasFaceColor)
		{
			faceColor = Quantity_NOC_GRAY95;
			//std::cout << "Using default gray color for face " << f << std::endl;
		}

		colorFaceGroups[faceColor].push_back(face);
	}

	//std::cout << "Grouped faces into " << colorFaceGroups.size() << " color groups" << std::endl;

	std::vector<aiMesh*> meshes;

	for (const auto& entry : colorFaceGroups)
	{
		const Quantity_Color& color = entry.first;
		const std::vector<TopoDS_Face>& faces = entry.second;

		//std::cout << "Creating mesh for color (" << color.Red() << ", " << color.Green()
			//<< ", " << color.Blue() << ") with " << faces.size() << " faces" << std::endl;

		TopTools_IndexedMapOfShape faceMap;
		for (const TopoDS_Face& face : faces)
		{
			faceMap.Add(face);
		}

		aiMesh* mesh = BRepToAssimpConverter::convertFaceGroupToMesh(faceMap, meshIndex);
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


/**
 * Performs a deep clone of an Assimp aiNode and its entire subtree.
 *
 * This method recursively copies the given source node, including its name,
 * transformation matrix, mesh indices, and all child nodes, ensuring a complete
 * independent duplicate of the node hierarchy.
 *
 * @param src Pointer to the source aiNode to clone.
 * @return Pointer to the newly created aiNode which is a deep copy of src.
 */
aiNode* BRepToAssimpConverter::cloneNodeDeep(const aiNode* src)
{
	if (!src) return nullptr;

	aiNode* dest = new aiNode();

	dest->mName = src->mName;
	dest->mTransformation = src->mTransformation;
	dest->mParent = nullptr;

	// Clone mesh indices
	dest->mNumMeshes = src->mNumMeshes;
	dest->mMeshes = nullptr;
	if (src->mNumMeshes > 0 && src->mMeshes)
	{
		dest->mMeshes = new unsigned int[src->mNumMeshes];
		std::copy(src->mMeshes, src->mMeshes + src->mNumMeshes, dest->mMeshes);
	}

	// Clone metadata if present
	if (src->mMetaData)
	{
		dest->mMetaData = new aiMetadata(*src->mMetaData);
	}

	// Clone children
	dest->mNumChildren = src->mNumChildren;
	dest->mChildren = nullptr;
	if (src->mNumChildren > 0 && src->mChildren)
	{
		dest->mChildren = new aiNode * [src->mNumChildren];
		for (unsigned int i = 0; i < src->mNumChildren; ++i)
		{
			dest->mChildren[i] = cloneNodeDeep(src->mChildren[i]);
			if (dest->mChildren[i])
				dest->mChildren[i]->mParent = dest;
		}
	}

	return dest;
}


bool BRepToAssimpConverter::GetComprehensiveColor(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TopoDS_Shape& shape,
	const TDF_Label& defLabel,
	const TDF_Label& instanceLabel,
	Quantity_Color& outColor)
{
	if (colorTool.IsNull()) return false;

	// Priority order matching professional CAD viewers:
	// 1. Face-level surface colors (highest priority)
	// 2. Instance-level colors 
	// 3. Shape-level colors
	// 4. Definition-level colors
	// 5. Assembly-level inherited colors

	static const XCAFDoc_ColorType surfaceTypes[] = { XCAFDoc_ColorSurf };
	static const XCAFDoc_ColorType otherTypes[] = { XCAFDoc_ColorCurv, XCAFDoc_ColorGen };

	//std::cout << "=== Color Search Priority System ===" << std::endl;
	//std::cout << "Shape type: " << ShapeTypeToString(shape.ShapeType()) << std::endl;

	// PRIORITY 1: Face-level surface colors (HIGHEST)
	if (shape.ShapeType() == TopAbs_FACE)
	{
		// Try direct face surface color first
		if (colorTool->GetColor(shape, XCAFDoc_ColorSurf, outColor))
		{
			//std::cout << "P1: Found FACE surface color: " << ColorToString(outColor) << std::endl;
			return true;
		}

		// Try face instance color
		if (colorTool->GetInstanceColor(shape, XCAFDoc_ColorSurf, outColor))
		{
			//std::cout << "P1: Found FACE instance surface color: " << ColorToString(outColor) << std::endl;
			return true;
		}
	}

	// PRIORITY 2: Instance-level colors (for the specific instance)
	if (!instanceLabel.IsNull())
	{
		// Surface colors have priority over generic colors
		for (XCAFDoc_ColorType type : surfaceTypes)
		{
			if (colorTool->GetColor(instanceLabel, type, outColor))
			{
				//std::cout << "P2: Found instance surface color: " << ColorToString(outColor) << std::endl;
				return true;
			}
		}

		// Then try other types
		for (XCAFDoc_ColorType type : otherTypes)
		{
			if (colorTool->GetColor(instanceLabel, type, outColor))
			{
				//std::cout << "P2: Found instance " << ColorTypeToString(type) << " color: " << ColorToString(outColor) << std::endl;
				return true;
			}
		}
	}

	// PRIORITY 3: Shape-level colors (direct shape query)
	for (XCAFDoc_ColorType type : surfaceTypes)
	{
		if (colorTool->GetColor(shape, type, outColor))
		{
			//std::cout << "P3: Found shape surface color: " << ColorToString(outColor) << std::endl;
			return true;
		}
		if (colorTool->GetInstanceColor(shape, type, outColor))
		{
			//std::cout << "P3: Found shape instance surface color: " << ColorToString(outColor) << std::endl;
			return true;
		}
	}

	for (XCAFDoc_ColorType type : otherTypes)
	{
		if (colorTool->GetColor(shape, type, outColor))
		{
			//std::cout << "P3: Found shape " << ColorTypeToString(type) << " color: " << ColorToString(outColor) << std::endl;
			return true;
		}
		if (colorTool->GetInstanceColor(shape, type, outColor))
		{
			//std::cout << "P3: Found shape instance " << ColorTypeToString(type) << " color: " << ColorToString(outColor) << std::endl;
			return true;
		}
	}

	// PRIORITY 4: Definition-level colors
	if (!defLabel.IsNull())
	{
		for (XCAFDoc_ColorType type : surfaceTypes)
		{
			if (colorTool->GetColor(defLabel, type, outColor))
			{
				//std::cout << "P4: Found definition surface color: " << ColorToString(outColor) << std::endl;
				return true;
			}
		}

		for (XCAFDoc_ColorType type : otherTypes)
		{
			if (colorTool->GetColor(defLabel, type, outColor))
			{
				//std::cout << "P4: Found definition " << ColorTypeToString(type) << " color: " << ColorToString(outColor) << std::endl;
				return true;
			}
		}
	}

	// PRIORITY 5: Assembly-level inherited colors (search up hierarchy)
	if (!defLabel.IsNull())
	{
		if (SearchParentLabelsForColor(colorTool, defLabel, outColor))
		{
			//std::cout << "P5: Found inherited color from parent: " << ColorToString(outColor) << std::endl;
			return true;
		}
	}

	if (!instanceLabel.IsNull() && instanceLabel != defLabel)
	{
		if (SearchParentLabelsForColor(colorTool, instanceLabel, outColor))
		{
			//std::cout << "P5: Found inherited color from instance parent: " << ColorToString(outColor) << std::endl;
			return true;
		}
	}

	// PRIORITY 6: Reverse lookup through all colors (REAL API version)
	if (SearchAllColorsForAssociation(colorTool, shape, defLabel, instanceLabel, outColor))
	{
		//std::cout << "P6: Found color via comprehensive search: " << ColorToString(outColor) << std::endl;
		return true;
	}

	// PRIORITY 7: Child label search (sometimes colors are stored on sub-components)
	if (!defLabel.IsNull())
	{
		if (SearchChildLabelsForColor(colorTool, defLabel, outColor))
		{
			//std::cout << "P7: Found color in child labels: " << ColorToString(outColor) << std::endl;
			return true;
		}
	}

	//std::cout << "No color found through priority system" << std::endl;
	return false;
}


// Enhanced version of GetComprehensiveColor with caching for expensive operations
bool BRepToAssimpConverter::GetComprehensiveColorWithCache(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TopoDS_Shape& shape,
	const TDF_Label& defLabel,
	const TDF_Label& instanceLabel,
	Quantity_Color& outColor)
{
	if (colorTool.IsNull()) return false;

	// Check shape cache first
	if (s_colorCache.GetCachedColor(shape, outColor))
	{
		return true;
	}

	// Priority 1: Face-level surface colors 
	if (shape.ShapeType() == TopAbs_FACE)
	{
		if (colorTool->GetColor(shape, XCAFDoc_ColorSurf, outColor))
		{
			s_colorCache.CacheColor(shape, outColor);
			return true;
		}
		if (colorTool->GetInstanceColor(shape, XCAFDoc_ColorSurf, outColor))
		{
			s_colorCache.CacheColor(shape, outColor);
			return true;
		}
	}

	// Priority 2: Instance-level colors
	if (!instanceLabel.IsNull())
	{
		std::string instancePath = GetLabelPath(instanceLabel);
		if (s_colorCache.GetCachedLabelColor(instancePath, outColor))
		{
			s_colorCache.CacheColor(shape, outColor);
			return true;
		}

		static const XCAFDoc_ColorType surfaceTypes[] = { XCAFDoc_ColorSurf };
		static const XCAFDoc_ColorType otherTypes[] = { XCAFDoc_ColorCurv, XCAFDoc_ColorGen };

		for (XCAFDoc_ColorType type : surfaceTypes)
		{
			if (colorTool->GetColor(instanceLabel, type, outColor))
			{
				s_colorCache.CacheLabelColor(instancePath, outColor);
				s_colorCache.CacheColor(shape, outColor);
				return true;
			}
		}

		for (XCAFDoc_ColorType type : otherTypes)
		{
			if (colorTool->GetColor(instanceLabel, type, outColor))
			{
				s_colorCache.CacheLabelColor(instancePath, outColor);
				s_colorCache.CacheColor(shape, outColor);
				return true;
			}
		}
	}

	bool result = GetComprehensiveColor(colorTool, shape, defLabel, instanceLabel, outColor);
	if (result)
	{
		s_colorCache.CacheColor(shape, outColor);
	}

	return result;
}

// Utility method to convert color to string for debugging
std::string BRepToAssimpConverter::ColorTypeToString(XCAFDoc_ColorType type)
{
	switch (type)
	{
	case XCAFDoc_ColorSurf: return "Surface";
	case XCAFDoc_ColorCurv: return "Curve";
	case XCAFDoc_ColorGen: return "Generic";
	default: return "Unknown";
	}
}

// Searches child labels recursively for a color attribute
bool BRepToAssimpConverter::SearchChildLabelsForColor(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& parentLabel,
	Quantity_Color& outColor)
{
	static const XCAFDoc_ColorType colorTypes[] = {
		XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
	};

	// Check current label for color attributes
	for (XCAFDoc_ColorType type : colorTypes)
	{
		if (colorTool->GetColor(parentLabel, type, outColor))
		{
			//std::cout << "Found color in current label (" << ColorTypeToString(type) << "): "
			//	<< ColorToString(outColor) << std::endl;
			return true;
		}
	}

	// Recursively check children using TDF_ChildIterator
	TDF_ChildIterator childIter(parentLabel);
	for (; childIter.More(); childIter.Next())
	{
		TDF_Label childLabel = childIter.Value();

		//std::cout << "Searching child label: " << GetLabelPath(childLabel) << std::endl;

		if (SearchChildLabelsForColor(colorTool, childLabel, outColor))
			return true;
	}

	return false;
}

// Searches parent labels recursively for a color attribute
bool BRepToAssimpConverter::SearchParentLabelsForColor(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& startLabel,
	Quantity_Color& outColor)
{
	static const XCAFDoc_ColorType colorTypes[] = {
		XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
	};

	TDF_Label currentLabel = startLabel.Father();
	int level = 0;
	const int maxLevels = 10; // Prevent infinite loops

	while (!currentLabel.IsRoot() && level < maxLevels)
	{
		//std::cout << "  Checking parent level " << level << ": " << GetLabelPath(currentLabel) << std::endl;

		// Check for colors at this parent level
		for (XCAFDoc_ColorType type : colorTypes)
		{
			if (colorTool->GetColor(currentLabel, type, outColor))
			{
				//std::cout << "  Found parent color at level " << level << std::endl;
				return true;
			}
		}

		currentLabel = currentLabel.Father();
		level++;
	}

	return false;
}

// Searches all color labels in the document for an association with the given shape
bool BRepToAssimpConverter::SearchAllColorsForAssociation(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TopoDS_Shape& shape,
	const TDF_Label& defLabel,
	const TDF_Label& instanceLabel,
	Quantity_Color& outColor)
{
	// Get all color labels in the document
	TDF_LabelSequence colorLabels;
	colorTool->GetColors(colorLabels);

	if (colorLabels.IsEmpty())
		return false;

	//std::cout << "  Searching through " << colorLabels.Length() << " color labels" << std::endl;

	// Get shape tool for label-to-shape resolution
	Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(colorLabels.Value(1).Root());
	if (shapeTool.IsNull()) return false;

	for (Standard_Integer i = 1; i <= colorLabels.Length(); ++i)
	{
		TDF_Label colorLabel = colorLabels.Value(i);

		// Get the color from this label
		Quantity_Color tempColor;
		if (!colorTool->GetColor(colorLabel, tempColor)) continue;

		//std::cout << "  Checking color " << i << ": " << ColorToString(tempColor) << std::endl;

		// Method A: Check if this color label is associated with our shape
		// by testing if our labels have this color set
		static const XCAFDoc_ColorType colorTypes[] = {
			XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
		};

		// Check definition label association
		if (!defLabel.IsNull())
		{
			for (XCAFDoc_ColorType type : colorTypes)
			{
				if (colorTool->IsSet(defLabel, type))
				{
					Quantity_Color labelColor;
					if (colorTool->GetColor(defLabel, type, labelColor))
					{
						// Compare colors (with small tolerance for floating point)
						if (ColorsEqual(tempColor, labelColor))
						{
							outColor = tempColor;
							//std::cout << "    Found matching color via defLabel IsSet check" << std::endl;
							return true;
						}
					}
				}
			}
		}

		// Check instance label association
		if (!instanceLabel.IsNull() && instanceLabel != defLabel)
		{
			for (XCAFDoc_ColorType type : colorTypes)
			{
				if (colorTool->IsSet(instanceLabel, type))
				{
					Quantity_Color labelColor;
					if (colorTool->GetColor(instanceLabel, type, labelColor))
					{
						if (ColorsEqual(tempColor, labelColor))
						{
							outColor = tempColor;
							//std::cout << "    Found matching color via instanceLabel IsSet check" << std::endl;
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

// Searches a shape label recursively for a target shape and retrieves its color
bool BRepToAssimpConverter::SearchShapeLabelForTargetWithColor(
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& shapeLabel,
	const TopoDS_Shape& targetShape,
	Quantity_Color& outColor)
{
	// Get the shape from this label
	TopoDS_Shape labelShape = shapeTool->GetShape(shapeLabel);
	if (labelShape.IsNull()) return false;

	// Check if this shape contains our target shape
	if (labelShape.IsSame(targetShape) || ContainsShape(labelShape, targetShape))
	{
		// Try to get color from this label
		static const XCAFDoc_ColorType colorTypes[] = {
			XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
		};

		for (XCAFDoc_ColorType type : colorTypes)
		{
			if (colorTool->GetColor(shapeLabel, type, outColor))
			{
				//std::cout << "Found color for target shape via label " << GetLabelPath(shapeLabel)
				//	<< " (" << ColorTypeToString(type) << "): " << ColorToString(outColor) << std::endl;
				return true;
			}
		}

		// Also try getting color from the shape itself
		for (XCAFDoc_ColorType type : colorTypes)
		{
			if (colorTool->GetColor(labelShape, type, outColor))
			{
				//std::cout << "Found color for target shape via shape comparison (" << ColorTypeToString(type) << "): "
				//	<< ColorToString(outColor) << std::endl;
				return true;
			}
		}
	}

	// Recursively search components if this is an assembly
	if (shapeTool->IsAssembly(shapeLabel))
	{
		TDF_LabelSequence components;
		shapeTool->GetComponents(shapeLabel, components);

		for (Standard_Integer j = 1; j <= components.Length(); ++j)
		{
			if (SearchShapeLabelForTargetWithColor(shapeTool, colorTool, components.Value(j), targetShape, outColor))
			{
				return true;
			}
		}
	}

	return false;
}

//Utility method to convert a Quantity_Color to a string representation
std::string BRepToAssimpConverter::ColorToString(const Quantity_Color& color)
{
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(3)
		<< "(" << color.Red() << ", " << color.Green() << ", " << color.Blue() << ")";
	return oss.str();
}

// Utility method to convert a TopAbs_ShapeEnum to a string representation
std::string BRepToAssimpConverter::ShapeTypeToString(TopAbs_ShapeEnum shapeType)
{
	switch (shapeType)
	{
	case TopAbs_COMPOUND: return "COMPOUND";
	case TopAbs_COMPSOLID: return "COMPSOLID";
	case TopAbs_SOLID: return "SOLID";
	case TopAbs_SHELL: return "SHELL";
	case TopAbs_FACE: return "FACE";
	case TopAbs_WIRE: return "WIRE";
	case TopAbs_EDGE: return "EDGE";
	case TopAbs_VERTEX: return "VERTEX";
	default: return "UNKNOWN";
	}
}

// Utility method to compare two Quantity_Color objects with a tolerance
bool BRepToAssimpConverter::ColorsEqual(const Quantity_Color& color1, const Quantity_Color& color2, double tolerance)
{
	return (std::abs(color1.Red() - color2.Red()) < tolerance &&
		std::abs(color1.Green() - color2.Green()) < tolerance &&
		std::abs(color1.Blue() - color2.Blue()) < tolerance);
}

// Utility method to get the label path as a string
std::string BRepToAssimpConverter::GetLabelPath(const TDF_Label& label)
{
	if (label.IsNull()) return "NULL";

	TCollection_AsciiString labelPath;
	TDF_Tool::Entry(label, labelPath);
	return labelPath.ToCString();
}

// Searches for a color in the XCAF document by iterating through all free shapes
bool BRepToAssimpConverter::FindColorInXCAFDocument(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const TopoDS_Shape& targetShape,
	Quantity_Color& outColor)
{
	if (colorTool.IsNull() || shapeTool.IsNull()) return false;

	// Get all free shapes (top-level shapes) in the document
	TDF_LabelSequence freeShapes;
	shapeTool->GetFreeShapes(freeShapes);

	//std::cout << "Searching through " << freeShapes.Length() << " free shapes in XCAF document" << std::endl;

	for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i)
	{
		TDF_Label shapeLabel = freeShapes.Value(i);
		//std::cout << "Checking shape label: " << GetLabelPath(shapeLabel) << std::endl;

		// Check if this label has our target shape and get its color
		if (SearchShapeLabelForTargetWithColor(shapeTool, colorTool, shapeLabel, targetShape, outColor))
		{
			return true;
		}
	}

	return false;
}

// Searches a shape label recursively for a target shape and retrieves its color
bool BRepToAssimpConverter::SearchShapeLabelForTarget(
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& shapeLabel,
	const TopoDS_Shape& targetShape,
	Quantity_Color& outColor)
{
	// Get the shape from this label
	TopoDS_Shape labelShape = shapeTool->GetShape(shapeLabel);
	if (labelShape.IsNull()) return false;

	// Check if this shape contains our target shape
	if (labelShape.IsSame(targetShape) || ContainsShape(labelShape, targetShape))
	{
		// Try to get color from this label
		static const XCAFDoc_ColorType colorTypes[] = {
			XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
		};

		for (XCAFDoc_ColorType type : colorTypes)
		{
			if (colorTool->GetColor(shapeLabel, type, outColor))
			{
				//std::cout << "Found color for target shape via label " << GetLabelPath(shapeLabel)
				//	<< " (" << ColorTypeToString(type) << "): "
				//	<< outColor.Red() << ", " << outColor.Green() << ", " << outColor.Blue() << std::endl;
				return true;
			}
		}

		// Also try getting color from the shape itself
		for (XCAFDoc_ColorType type : colorTypes)
		{
			if (colorTool->GetColor(labelShape, type, outColor))
			{
				//std::cout << "Found color for target shape via shape comparison (" << ColorTypeToString(type) << "): "
				//	<< outColor.Red() << ", " << outColor.Green() << ", " << outColor.Blue() << std::endl;
				return true;
			}
		}
	}

	// Recursively search components if this is an assembly
	if (shapeTool->IsAssembly(shapeLabel))
	{
		TDF_LabelSequence components;
		shapeTool->GetComponents(shapeLabel, components);

		for (Standard_Integer j = 1; j <= components.Length(); ++j)
		{
			if (SearchShapeLabelForTarget(shapeTool, colorTool, components.Value(j), targetShape, outColor))
			{
				return true;
			}
		}
	}

	return false;
}

// Checks if a compound shape contains a specific target shape
bool BRepToAssimpConverter::ContainsShape(const TopoDS_Shape& compound, const TopoDS_Shape& target)
{
	if (compound.IsSame(target)) return true;

	// For compounds, check all sub-shapes
	for (TopoDS_Iterator it(compound); it.More(); it.Next())
	{
		if (it.Value().IsSame(target) || ContainsShape(it.Value(), target))
		{
			return true;
		}
	}

	// Also check using TopExp_Explorer for different shape types
	for (TopExp_Explorer exp(compound, target.ShapeType()); exp.More(); exp.Next())
	{
		if (exp.Current().IsSame(target))
		{
			return true;
		}
	}

	return false;
}

// Searches for a color in the label hierarchy, starting from a given label
bool BRepToAssimpConverter::SearchLabelHierarchyForColor(
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& startLabel,
	Quantity_Color& outColor)
{
	static const XCAFDoc_ColorType colorTypes[] = {
		XCAFDoc_ColorSurf, XCAFDoc_ColorCurv, XCAFDoc_ColorGen
	};

	// Search up the hierarchy (parents)
	TDF_Label currentLabel = startLabel;
	while (!currentLabel.IsRoot())
	{
		for (XCAFDoc_ColorType type : colorTypes)
		{
			if (colorTool->GetColor(currentLabel, type, outColor))
				return true;
		}
		currentLabel = currentLabel.Father();
	}

	// Search down the hierarchy (children)
	return SearchChildLabelsForColor(colorTool, startLabel, outColor);
}


/**
 * Computes an appropriate deflection value based on the bounding box diagonal of a group of faces.
 *
 * The deflection is calculated as a percentage of the diagonal length of the bounding box
 * that encloses all shapes in the provided face group. This value can be used to control
 * the level of detail or tessellation precision during shape conversion.
 *
 * @param faceGroup An indexed map containing the group of faces (shapes) to evaluate.
 * @param percent A multiplier (typically between 0 and 1) representing the fraction of the diagonal to use.
 * @return The computed deflection value as a Standard_Real.
 */
Standard_Real BRepToAssimpConverter::computeDeflectionFromBBox(const TopTools_IndexedMapOfShape& faceGroup, Standard_Real percent)
{
	Bnd_Box bbox;
	Standard_Real diag = 0.01; // Default deflection if no faces are present

	try
	{
		// Accumulate bounding box of all faces in the group
		for (int i = 1; i <= faceGroup.Extent(); ++i)
		{
			BRepBndLib::Add(faceGroup(i), bbox);
		}

		// Extract bounding box limits
		Standard_Real xmin, ymin, zmin, xmax, ymax, zmax;
		bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);

		// Compute diagonal length of bounding box
		gp_Pnt pMin(xmin, ymin, zmin);
		gp_Pnt pMax(xmax, ymax, zmax);
		diag = pMin.Distance(pMax);
	}
	catch (Standard_Failure& e)
	{
		// Handle exceptions gracefully, log if necessary
		std::cerr << "Error computing bounding box: " << e.GetMessageString() << std::endl;
	}

	// Return deflection as a fraction of the diagonal
	return percent * diag;
}

// Heals and triangulates a face, returning the healed face.
TopoDS_Face BRepToAssimpConverter::healAndTriangulateFace(const TopoDS_Face& inputFace,
	double deflection,
	double angularDeflection,
	double fixTolerance)
{
	TopoDS_Face healedFace;
	try
	{
		// 1. Validate original face
		BRepCheck_Analyzer analyzer(inputFace);
		if (!analyzer.IsValid())
		{
			std::cout << "[Heal] Input face is invalid" << std::endl;
			return inputFace;
		}

		// 2. Heal the face using ShapeFix_Face
		Handle(ShapeFix_Face) fixFace = new ShapeFix_Face(inputFace);
		fixFace->FixWireTool()->SetPrecision(fixTolerance);
		fixFace->FixOrientation();
		fixFace->FixAddNaturalBoundMode() = Standard_True;
		fixFace->Perform();
		healedFace = fixFace->Face();

		// 3. Optionally reset tolerance (if the face tolerance is very large)
		if (BRep_Tool::Tolerance(healedFace) > 1e-2)
		{
			BRep_Builder builder;
			builder.UpdateFace(healedFace, fixTolerance);
		}

		// 4. Clean any existing triangulation
		BRepTools::Clean(healedFace);

		// 5. Recompute triangulation
		BRepMesh_IncrementalMesh mesher(healedFace, deflection, Standard_True, angularDeflection);

		if (!mesher.IsDone())
		{
			std::cerr << "[Heal] Triangulation failed" << std::endl;
		}
		else
		{
			std::cout << "[Heal] Face healed and triangulated successfully" << std::endl;
		}
	}
	catch (Standard_Failure& e)
	{
		// Handle exceptions gracefully, log if necessary
		std::cerr << "Error healing face: " << e.GetMessageString() << std::endl;
	}

	return healedFace;
}
