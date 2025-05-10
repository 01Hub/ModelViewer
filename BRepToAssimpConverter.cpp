#include "BRepToAssimpConverter.h"
#include "MainWindow.h"
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <cmath>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <vector>


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
		int progress = (int)(((float)meshIndex / (float)totalCount)*100);		
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

				// 🛠 Correct the material index!
				mesh->mMaterialIndex = materialBase; // (use the newly appended material)

				aiMaterial* material = materialList[materialBase];

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

	// Vectors to hold generated meshes and their corresponding nodes
	std::vector<aiMesh*> meshes;
	std::vector<aiNode*> meshNodes;

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

			// Create a node for this mesh
			aiNode* meshNode = new aiNode();

			// Assign the mesh index to the node
			meshNode->mMeshes = new unsigned int[1] {static_cast<unsigned int>(meshIndex)};
			meshNode->mNumMeshes = 1;

			// Set the parent of this node to the root node of the scene
			meshNode->mParent = scene->mRootNode;

			// Name the node and mesh using the mesh index
			meshNode->mName = name;// aiString("Mesh_" + std::to_string(meshIndex));
			mesh->mName = name;// aiString("Mesh_" + std::to_string(meshIndex));

			// Store the node
			meshNodes.push_back(meshNode);

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
			int faceCnt = 0;
			TopExp_Explorer faceExplorer(solid, TopAbs_FACE);
			for (; faceExplorer.More(); faceExplorer.Next())
			{
				faceGroup.Add(faceExplorer.Current());
				++faceCnt;
			}

#ifdef __DEBUG__
			std::cout << "Mesh index: " << meshIndex << std::endl;
			std::cout << "Solid has " << faceCnt << " faces." << std::endl;
#endif



			// Convert the collected faces into a mesh
			aiMesh* mesh = convertFaceGroupToMesh(faceGroup, meshIndex);
			if (mesh)
			{
				// Add the mesh to the list
				meshes.push_back(mesh);

				// Create a node for this mesh
				aiNode* meshNode = new aiNode();

				// Assign the mesh index to the node
				meshNode->mMeshes = new unsigned int[1] {static_cast<unsigned int>(meshIndex)};
				meshNode->mNumMeshes = 1;

				// Set the parent of this node to the root node of the scene
				meshNode->mParent = scene->mRootNode;

				// Name the node and mesh using the mesh index, indicating it comes from a solid
				meshNode->mName = name;// aiString("SolidMesh_" + std::to_string(meshIndex));
				mesh->mName = name;// aiString("SolidMesh_" + std::to_string(meshIndex));

				// Store the node
				meshNodes.push_back(meshNode);

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

	// Assign the mesh nodes as children of the root node
	scene->mRootNode->mNumChildren = meshNodes.size();
	scene->mRootNode->mChildren = new aiNode * [meshNodes.size()];
	for (size_t i = 0; i < meshNodes.size(); ++i)
	{
		scene->mRootNode->mChildren[i] = meshNodes[i];
	}

	// Create a default material for the scene
	scene->mMaterials = new aiMaterial * [1];
	aiColor3D aiColor(color.Red(), color.Green(), color.Blue());
	aiMaterial* material = new aiMaterial;
	material->AddProperty(&aiColor, 1, AI_MATKEY_COLOR_DIFFUSE);
	scene->mMaterials[0] = material;
	scene->mNumMaterials = 1;

	// Update material count
	++scene->mNumMaterials;

	// Return the constructed scene
	return scene;
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
#include <ShapeFix_Face.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepTools.hxx>
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

		BRepCheck_Analyzer analyzer(face);
		if (!analyzer.IsValid())
		{
			std::cout << "Face is invalid. Attempting to heal..." << std::endl;
			// Use ShapeFix_Face or other healing tools here
		}

		// Retrieve the triangulation and its location transformation on the face
		TopLoc_Location loc;
		Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
		if (triangulation.IsNull())
		{
			std::cout << "Failed to triangulate face " << f << " trying to heal..."<< std::endl;
			// Use ShapeFix_Face or other healing tools here
			ShapeFix_Face fixer;
			fixer.Init(face);
			fixer.Perform(); // Attempt to heal the face
			TopoDS_Face healedFace = fixer.Face();			
			if (!healedFace.IsNull())
			{				
				triangulation = BRep_Tool::Triangulation(healedFace, loc, Poly_MeshPurpose_AnyFallback);
				if (triangulation.IsNull())
				{
					Standard_Real increasedDeflection = deflection * 1.5; // Increase by 50%
					BRepMesh_IncrementalMesh mesher(healedFace, increasedDeflection);
					triangulation = BRep_Tool::Triangulation(healedFace, loc, Poly_MeshPurpose_AnyFallback);
					if (triangulation.IsNull())
					{
						std::cout << "Unable to triangulate, face will not be displayed..." << std::endl;
						continue; // Skip faces that failed to triangulate
					}
				}
				else
				{
					std::cout << "Face healed successfully..." << std::endl;
				}
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
		std::vector<aiVector3D> localUVs;

		// Create local vertex positions and UV texture coordinates from points
		for (int i = 0; i < nNodes; ++i)
		{
			const gp_Pnt& p = points[i];
			localVertices.emplace_back(p.X(), p.Y(), p.Z());
			// UV coordinates normalized based on XY bounding box of the face
			localUVs.emplace_back((p.X() - minX) / deltaX, (p.Y() - minY) / deltaY, 0.0f);
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
		texCoords.insert(texCoords.end(), localUVs.begin(), localUVs.end());
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
	mesh->mTextureCoords[0] = new aiVector3D[texCoords.size()];
	mesh->mNumUVComponents[0] = 2; // UV coordinates have two components (u,v)

	// Copy vertex positions, normals, and texture coordinates into the mesh arrays
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		mesh->mVertices[i] = vertices[i];
		mesh->mNormals[i] = normals[i];
		mesh->mTextureCoords[0][i] = texCoords[i];
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
	// Create a new node and copy basic properties
	aiNode* dest = new aiNode();
	dest->mName = src->mName;
	dest->mTransformation = src->mTransformation;
	dest->mNumMeshes = src->mNumMeshes;

	// Copy mesh indices if present
	if (src->mNumMeshes > 0)
	{
		dest->mMeshes = new unsigned int[src->mNumMeshes];
		std::copy(src->mMeshes, src->mMeshes + src->mNumMeshes, dest->mMeshes);
	}

	// Recursively clone child nodes
	dest->mNumChildren = src->mNumChildren;
	if (src->mNumChildren > 0)
	{
		dest->mChildren = new aiNode * [src->mNumChildren];
		for (unsigned int i = 0; i < src->mNumChildren; ++i)
		{
			dest->mChildren[i] = cloneNodeDeep(src->mChildren[i]);
			dest->mChildren[i]->mParent = dest;  // Set parent pointer in cloned child
		}
	}

	return dest;
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

