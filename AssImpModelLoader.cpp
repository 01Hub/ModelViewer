#include "AssImpModelLoader.h"
#include "BRepToAssimpConverter.h"
#include "ConsoleProgressIndicator.hxx"
#include "MainWindow.h"
#include "Utils.h"
#include <IGESCAFControl_Reader.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <Quantity_ColorRGBA.hxx>
#include <STEPControl_Reader.hxx>
#include <TopoDS_Iterator.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc.hxx>
#include <XSControl_WorkSession.hxx>
#include <BinDrivers.hxx>

using namespace std;


bool AssImpModelProgressHandler::Update(float percentage)
{
	emit fileReadProcessed(percentage);
	return true;
}

/*  Functions   */
// Constructor, expects a filepath to a 3D model.
AssImpModelLoader::AssImpModelLoader(QOpenGLShaderProgram* prog) : QObject(), _prog(prog)
{
	initializeOpenGLFunctions();
	_loadingCancelled = false;
	_progHandler = new AssImpModelProgressHandler();
	_importer.SetProgressHandler(_progHandler);
	connect(_progHandler, SIGNAL(fileReadProcessed(float)), this, SLOT(processFileReadProgress(float)));
}

AssImpModelLoader::~AssImpModelLoader()
{
	disconnect(_progHandler, SIGNAL(fileReadProcessed(float)), this, SLOT(processFileReadProgress(float)));
	//delete _progHandler; // causes crash
	_progHandler = nullptr;
}

void AssImpModelLoader::processFileReadProgress(float percentage)
{
	emit fileReadProcessed(percentage);
}

void AssImpModelLoader::cancelLoading()
{
	_loadingCancelled = true;
}

vector<AssImpMesh*> AssImpModelLoader::getMeshes() const
{
	return _meshes;
}

/*  Functions   */
// Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
void AssImpModelLoader::loadModel(string path)
{
	_loadingCancelled = false;
	_path = std::string(path);
	_meshes.clear();
	_loadedTextures.clear();

	const aiScene* scene = nullptr;

	QFileInfo fi(path.c_str());

#ifdef __DEBUG__
	std::cout << "\n--------------------------------------------------" << std::endl;
	std::cout << "Starting to load model: " << path.c_str() << std::endl;
	std::cout << "File size: " << fi.size() / 1024.0f << " KB" << std::endl;
#endif

	if (fi.suffix().toLower() == "step" || fi.suffix().toLower() == "stp")
	{
		scene = processSTEPFile(path);
	}
	else if (fi.suffix().toLower() == "iges" || fi.suffix().toLower() == "igs")
    {
		scene = processIGESFile(path);
	}
	else // all Assimp models
	{
		// Read file via ASSIMP
		_importer.SetPropertyFloat("PP_GSN_MAX_SMOOTHING_ANGLE", 15);
		scene = _importer.ReadFile(path, aiProcess_CalcTangentSpace |
			aiProcess_GenSmoothNormals |
			aiProcess_FixInfacingNormals |
			aiProcess_JoinIdenticalVertices |
			aiProcess_OptimizeMeshes |
			aiProcess_Triangulate |
			aiProcess_GenUVCoords |
			aiProcess_SortByPType);
	}

	// Check for errors
	if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
	{
		_errorMessage = _importer.GetErrorString();
		cout << "ERROR::ASSIMP:: " << _importer.GetErrorString() << endl;
		return;
	}
	// Retrieve the directory path of the filepath
	this->directory = path.substr(0, path.find_last_of('/'));

	// Process ASSIMP's root node recursively
	this->processNode(0, scene->mRootNode, scene);
}

aiScene* AssImpModelLoader::processSTEPFile(const std::string& path)
{
	// Create XCAF Application and document
	Handle(TDocStd_Document) doc;
	Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
	BinDrivers::DefineFormat(app);
	app->NewDocument("BinOcaf", doc);

	try
	{
		readSTEPFile(path.c_str(), doc);
	}
	catch (const std::exception& e)
	{
		qCritical("Failed to read STEP file: %s", e.what());
		return nullptr;
	}

	// Get the shape tool from the document
	Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
	Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());


	// Get the shapes from the STEP file
	TDF_LabelSequence labels;
	shapeTool->GetFreeShapes(labels);

	if (labels.IsEmpty())
	{
		qCritical("No shapes found in STEP file");
		return nullptr;
	}

	// Traverse the assembly structure and extract shapes and names
	std::vector<TopoDS_Shape> shapes;
	std::vector<std::string> names;
	std::vector<Quantity_Color> colors;

#ifdef __DEBUG__
	auto startTraverse = std::chrono::high_resolution_clock::now();
#endif
	MainWindow::showStatusMessage("Traversing assembly...");
	for (Standard_Integer i = 1; i <= labels.Length(); ++i)
	{
		traverseSTEPAssembly(shapeTool, colorTool, labels.Value(i), TopLoc_Location(), shapes, colors, names);
	}
#ifdef __DEBUG__
	auto endTraverse = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationTraverse = endTraverse - startTraverse;
	std::cout << "TraverseSTEPAssembly took " << durationTraverse.count() << " seconds\n";
#endif

	// Add shapes and names to the tuple vector
	std::vector<ShapeWithNameAndTrsf> shapeTuples;
	for (int i = 0; i < shapes.size(); i++)
	{
		shapeTuples.emplace_back(shapes[i], names[i], TopLoc_Location(), colors[i]);
	}

	// Convert the shapes to Assimp scene
	MainWindow::showStatusMessage("Converting shapes to mesh...");
#ifdef __DEBUG__
	auto startConversion = std::chrono::high_resolution_clock::now();
#endif
	aiScene* scene = BRepToAssimpConverter::convert(shapeTuples);
#ifdef __DEBUG__
	auto endConversion = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationConversion = endConversion - startConversion;
	std::cout << "BRepToAssimpConverter::convert took " << durationConversion.count() << " seconds\n";

	std::cout << "End of loading STEP file" << std::endl;
	std::cout << "--------------------------------------------------" << std::endl;

	QString savePath = "/home/sharjith/" + fi.baseName() + ".cbf";
	PCDM_StoreStatus sstatus = app->SaveAs(doc, TCollection_ExtendedString(savePath.toStdString().c_str()));
	if (sstatus != PCDM_SS_OK)
	{
		app->Close(doc);
		qDebug() << "Error saving the document";
	}
#endif

	app->Close(doc);

	return scene;
}

aiScene* AssImpModelLoader::processIGESFile(const std::string& path)
{
	// Create XCAF Application and document
	Handle(TDocStd_Document) doc;
	Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
	BinDrivers::DefineFormat(app);
	app->NewDocument("BinOcaf", doc);

	// Initialize IGES controller inside session
	IGESControl_Controller::Init();

	MainWindow::showIndeterminateProgressBar();

	Handle(ConsoleProgressIndicator) progress = new ConsoleProgressIndicator();
	Message_ProgressRange rootRange = progress->Start();

	Message_ProgressScope transferScope(rootRange, "IGES Transfer", -1);

	// Read IGES file into XCAF document
	IGESCAFControl_Reader cafReader;
	if (cafReader.ReadFile(path.c_str()) != IFSelect_RetDone)
	{
		qCritical("Failed to read IGES file: %s", path.c_str());
		MainWindow::resetProgressBar();
		return nullptr;
	}
	MainWindow::resetProgressBar();

	MainWindow::showStatusMessage("Transfering shapes...");

	cafReader.Transfer(doc, transferScope.Next());

	// Access shape tool and color tool
	Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
	Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());

	// Collect free shapes (top-level shapes)
	TDF_LabelSequence labels;
	shapeTool->GetFreeShapes(labels);

	// Traverse the assembly structure and extract shapes and names
	std::vector<TopoDS_Shape> shapes;
	std::vector<std::string> names;
	std::vector<Quantity_Color> colors;

	MainWindow::showStatusMessage("Traversing assembly...");

	for (Standard_Integer i = 1; i <= labels.Length(); ++i)
	{
		traverseSTEPAssembly(shapeTool, colorTool, labels.Value(i), TopLoc_Location(), shapes, colors, names);
	}

	// Add shapes and names to the tuple vector
	std::vector<ShapeWithNameAndTrsf> shapeTuples;
	for (int i = 0; i < shapes.size(); i++)
	{
		shapeTuples.emplace_back(shapes[i], names[i], TopLoc_Location(), colors[i]);
	}

	// Convert to Assimp scene
	MainWindow::showStatusMessage("Converting shapes to mesh...");
	aiScene* scene = BRepToAssimpConverter::convert(shapeTuples);

#ifdef __DEBUG__
	QString savePath = "/home/sharjith/" + fi.baseName() + ".cbf";
	PCDM_StoreStatus sstatus = app->SaveAs(doc, TCollection_ExtendedString(savePath.toStdString().c_str()));
	if (sstatus != PCDM_SS_OK)
	{
		app->Close(doc);
		qDebug() << "Error saving the document";
	}
#endif

	app->Close(doc);

	return scene;
}

// Read s STEP file
void AssImpModelLoader::readSTEPFile(const std::string& filename, Handle(TDocStd_Document)& doc)
{	
	STEPCAFControl_Reader reader;
#ifdef __DEBUG__
	auto startCount = std::chrono::high_resolution_clock::now();
#endif
	MainWindow::showIndeterminateProgressBar();

	Handle(ConsoleProgressIndicator) progress = new ConsoleProgressIndicator();
	Message_ProgressRange rootRange = progress->Start();

	Message_ProgressScope transferScope(rootRange, "STEP Transfer", -1);

	if (!reader.ReadFile(filename.c_str()))
	{
		MainWindow::resetProgressBar();
		throw std::runtime_error("Cannot read STEP file");		
	}
	MainWindow::resetProgressBar();
	
#ifdef __DEBUG__
	auto endCount = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationCount = endCount - startCount;
	std::cout << "ReadFile took " << durationCount.count() << " seconds\n";


	auto startTraverse = std::chrono::high_resolution_clock::now();
#endif
	
    MainWindow::showStatusMessage("Transfering shapes...");
	
	if (!reader.Transfer(doc, transferScope.Next()))
	{
		throw std::runtime_error("Cannot transfer STEP data to XCAF document");
	}
#ifdef __DEBUG__
	auto endTraverse = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationTraverse = endTraverse - startTraverse;
	std::cout << "\nTransfer took " << durationTraverse.count() << " seconds\n";
#endif
}


// Traverse the STEP assembly structure and extract shapes and names
void AssImpModelLoader::traverseSTEPAssembly(
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& label,
	const TopLoc_Location& parentLoc,
	std::vector<TopoDS_Shape>& outShapes,
	std::vector<Quantity_Color>& outColors,
	std::vector<std::string>& outNames)
{
	// 1) Assembly?  Recurse its components (cycle‐safe)
	if (shapeTool->IsAssembly(label))
	{
		TDF_LabelSequence comps;
		shapeTool->GetComponents(label, comps);
		for (Standard_Integer i = 1; i <= comps.Length(); ++i)
		{
			traverseSTEPAssembly(shapeTool, colorTool, comps.Value(i), parentLoc, outShapes, outColors, outNames);
		}
		return;
	}

	// 2) Compute this instance's transform
	TopLoc_Location loc = parentLoc * shapeTool->GetLocation(label);

	// 3) If it's a reference, resolve to its definition label
	TDF_Label defLabel = label;
	if (shapeTool->IsReference(label))
	{
		TDF_Label tmp;
		if (shapeTool->GetReferredShape(label, tmp))
		{
			defLabel = tmp;
		}
	}

	// 4) If that definition is *also* an assembly, dive in (so we don't name a sub-assembly as if it were a leaf)
	if (shapeTool->IsAssembly(defLabel))
	{
		// an instance of an assembly—recurse into its real children
		TDF_LabelSequence comps;
		shapeTool->GetComponents(defLabel, comps);
		for (Standard_Integer i = 1; i <= comps.Length(); ++i)
		{
			traverseSTEPAssembly(shapeTool, colorTool, comps.Value(i), loc, outShapes, outColors, outNames);
		}
		return;
	}

	// 5) Now defLabel must be a true leaf part definition—grab its shape
	TopoDS_Shape shape = shapeTool->GetShape(defLabel);
	if (shape.IsNull()) return;

	shape.Move(loc);
	outShapes.push_back(shape);

	// 6) Extract the name from the *definition* label (defLabel), falling back to the instance label only if needed
	Handle(TDataStd_Name) nameAttr;
	std::string name = "Unnamed";
	if (defLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr))
	{
		name = TCollection_AsciiString(nameAttr->Get()).ToCString();
	}
	else if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr))
	{
		name = TCollection_AsciiString(nameAttr->Get()).ToCString();
	}

	// 7) Add instance number to the name (e.g., Wheel.1, Wheel.2)
	// --- Extract Instance Number from the label ---
	static std::map<std::string, int> nameCountMap;
	int instanceNum = 1; // Default value if no instance number is found
	// Attempt to extract instance number based on shapeTool's label information
	// Check if the name is already counted (for handling multiple instances)
	if (nameCountMap.find(name) != nameCountMap.end())
	{
		instanceNum = ++nameCountMap[name];
	}
	else
	{
		nameCountMap[name] = 1;
	}

	std::string instanceName = name + "." + std::to_string(instanceNum);
	outNames.push_back(instanceName);

	// 8) (Fixed) Get the color of the shape
	Quantity_Color color;
	bool hasColor = false;
	if (!colorTool.IsNull())
	{
		hasColor = GetShapeColorFromShape(colorTool, shape, color);
	}

	if (!hasColor)
	{
		color = Quantity_NOC_GRAY95; // fallback to default if no color found
	}

	outColors.push_back(color);
}

// --- Helper to get color from shape ---
bool AssImpModelLoader::GetShapeColorFromShape(
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


// Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
void AssImpModelLoader::processNode(int nodeNum, aiNode* node, const aiScene* scene)
{
	if (_loadingCancelled)
	{
		emit loadingCancelled();
		return;
	}
	// Process each mesh located at the current node
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		// The node object only contains indices to index the actual objects in the scene.
		// The scene contains all the data, node is just to keep stuff organized (like relations between nodes).
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		this->_meshes.push_back(this->processMesh(mesh, scene));
	}

	// After we've processed all of the meshes (if any) we then recursively process each of the children nodes
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		if (_loadingCancelled)
		{
			emit loadingCancelled();
			return;
		}
		this->processNode(++nodeNum, node->mChildren[i], scene);
		emit nodeProcessed(nodeNum, node->mNumChildren);
	}
}

AssImpMesh* AssImpModelLoader::processMesh(aiMesh* mesh, const aiScene* scene)
{
	// Data to fill
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	vector<Texture> textures;

	// Walk through each of the mesh's vertices
	int step = 0;
	unsigned int nbVertices = mesh->mNumVertices;
	for (unsigned int i = 0; i < nbVertices; i++)
	{
		step++;
		Vertex vertex;
		glm::vec3 vector; // We declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.

		// Positions
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.Position = vector;

		// Normals
		vector.x = mesh->mNormals[i].x;
		vector.y = mesh->mNormals[i].y;
		vector.z = mesh->mNormals[i].z;
		vertex.Normal = vector;

		// Texture Coordinates
		if (mesh->mTextureCoords[0]) // Does the mesh contain texture coordinates?
		{
			glm::vec2 vec;
			// A vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
			// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.TexCoords = vec;

			// tangent
			if (mesh->mTangents)
			{
				vector.x = mesh->mTangents[i].x;
				vector.y = mesh->mTangents[i].y;
				vector.z = mesh->mTangents[i].z;
				vertex.Tangent = vector;
			}
			// bitangent
			if (mesh->mBitangents)
			{
				vector.x = mesh->mBitangents[i].x;
				vector.y = mesh->mBitangents[i].y;
				vector.z = mesh->mBitangents[i].z;
				vertex.Bitangent = vector;
			}
		}
		else
		{
			if (step == 1)
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);
			else if (step == 2)
				vertex.TexCoords = glm::vec2(1.0f, 0.0f);
			else
			{
				vertex.TexCoords = glm::vec2(0.5f, 1.0f);
				step = 0;
			}
		}

		// Vertex Color
		if (mesh->HasVertexColors(0))
		{
			aiColor4D color = mesh->mColors[0][i];
			vertex.Color = glm::vec4(color.r, color.g, color.b, color.a); // Add color
		}
		else
		{
			vertex.Color = glm::vec4(1.0f); // Default color (white)
		}

		vertices.push_back(vertex);

		if (i % 100000 == 0)
		{
			emit verticesProcessed(static_cast<float>(i) / nbVertices * 100.0f);
		}
	}

	// Now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		// Retrieve all indices of the face and store them in the indices vector
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}

	// Process materials
	GLMaterial mat = GLMaterial::DEFAULT_MAT();
	//if (mesh->mMaterialIndex != 0)
	if (mesh->mMaterialIndex < scene->mNumMaterials)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		// We assume a convention for sampler names in the shaders. Each diffuse texture should be named
		// as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
		// Same applies to other texture as the following list summarizes:
		// Diffuse: texture_diffuseN
		// Specular: texture_specularN
		// Normal: texture_normalN

		// ADS Maps
		setADSTextureMaps(material, textures);

		// PBR Maps
		setPBRTextureMaps(material, textures);

		//Set color and material
		setColorAndMaterial(material, mat);
	}

	// Return a mesh object created from the extracted mesh data
	return new AssImpMesh(_prog, QFileInfo(QString(_path.data())).baseName() + " (" + mesh->mName.C_Str() + ") ", vertices, indices, textures, mat);
}

void AssImpModelLoader::setColorAndMaterial(aiMaterial* material, GLMaterial& mat)
{
	aiColor3D color(0.f, 0.f, 0.f);
	float opacity = 1.0f;	
	if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, color))
	{
		mat.setAmbient(QVector3D(color.r, color.g, color.b));
	}
	else
	{
		mat.setAmbient(QVector3D(0.7f, 0.7f, 0.7f));
	}
	if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, color))
	{
		mat.setDiffuse(QVector3D(color.r, color.g, color.b));
		mat.setAlbedoColor(QVector3D(color.r, color.g, color.b));
	}
	else
	{
		mat.setDiffuse(QVector3D(0.7f, 0.7f, 0.7f));
		mat.setAlbedoColor(QVector3D(0.7f, 0.7f, 0.7f));
	}
	if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, color))
	{
		mat.setSpecular(QVector3D(color.r, color.g, color.b));
		bool grayScale = (color.r == color.g && color.g == color.b && color.r == color.b);
		float intensity = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
		if (grayScale)
			mat.setMetalness(intensity > 0.04f ? 0.04f : intensity); // limit to 4% for dielectrics
		else
			mat.setMetalness(intensity);
		mat.setRoughness(1.0f - intensity);

	}
	else
	{
		mat.setSpecular(QVector3D(0.7f, 0.7f, 0.7f));
		mat.setMetalness(0.0f);
		mat.setRoughness(1.0f);
	}
	if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_EMISSIVE, color))
	{
		mat.setEmissive(QVector3D(color.r, color.g, color.b));
	}
	else
	{
		mat.setEmissive(QVector3D(0.0f, 0.0f, 0.0f));
	}
	if (AI_SUCCESS == material->Get(AI_MATKEY_OPACITY, opacity) && opacity != 0)
	{
		std::cout << "Opacity: " << opacity << std::endl;
		mat.setOpacity(opacity);
	}
	else
	{
		mat.setOpacity(1.0f);
	}

}

void AssImpModelLoader::setPBRTextureMaps(aiMaterial* material, std::vector<Texture>& textures)
{
	// 1. albedo maps
	vector<Texture> albedoPBRMaps = loadMaterialTextures(material, aiTextureType_BASE_COLOR, "albedoMap");
	textures.insert(textures.end(), albedoPBRMaps.begin(), albedoPBRMaps.end());
	// 2. specular maps
	vector<Texture> metalicPBRMaps = loadMaterialTextures(material, aiTextureType_METALNESS, "metallicMap");
	textures.insert(textures.end(), metalicPBRMaps.begin(), metalicPBRMaps.end());
	// 3. roughness maps
	vector<Texture> roughnessPBRMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE_ROUGHNESS, "roughnessMap");
	textures.insert(textures.end(), roughnessPBRMaps.begin(), roughnessPBRMaps.end());
	// 4. emissive maps
	vector<Texture> emissivePBRMaps = loadMaterialTextures(material, aiTextureType_EMISSION_COLOR, "emissiveMap");
	textures.insert(textures.end(), emissivePBRMaps.begin(), emissivePBRMaps.end());
	// 5. normal maps
	std::vector<Texture> normalPBRMaps = loadMaterialTextures(material, aiTextureType_NORMAL_CAMERA, "normalMap");
	textures.insert(textures.end(), normalPBRMaps.begin(), normalPBRMaps.end());
	// 6. AO maps
	std::vector<Texture> aoPBRMaps = loadMaterialTextures(material, aiTextureType_AMBIENT_OCCLUSION, "aoMap");
	textures.insert(textures.end(), aoPBRMaps.begin(), aoPBRMaps.end());
}

void AssImpModelLoader::setADSTextureMaps(aiMaterial* material, std::vector<Texture>& textures)
{
	// 1. diffuse maps
	vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	// 2. specular maps
	vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
	textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
	// 3. emissive maps
	vector<Texture> emissiveMaps = loadMaterialTextures(material, aiTextureType_EMISSIVE, "texture_emissive");
	textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
	// 4. normal maps
	std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
	textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
	// 5. height maps
	std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_DISPLACEMENT, "texture_height");
	textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
	// 6. opacity maps
	std::vector<Texture> opacityMaps = loadMaterialTextures(material, aiTextureType_OPACITY, "texture_opacity");
	textures.insert(textures.end(), opacityMaps.begin(), opacityMaps.end());
}

// Checks all material textures of a given type and loads the textures if they're not loaded yet.
// The required info is returned as a Texture struct.
vector<Texture> AssImpModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName)
{
	vector<Texture> textures;

	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);

		// Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
		GLboolean skip = false;

		for (unsigned int j = 0; j < _loadedTextures.size(); j++)
		{
			if (_loadedTextures[j].path == str)
			{
				textures.push_back(_loadedTextures[j]);
				skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)
				break;
			}
		}

		if (!skip)
		{
			// If texture hasn't been loaded already, load it
			Texture texture;
			texture.id = textureFromFile(str.C_Str(), this->directory);
			texture.type = typeName;
			texture.path = str;
			textures.push_back(texture);

			this->_loadedTextures.push_back(texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
		}
	}

	return textures;
}

unsigned int AssImpModelLoader::textureFromFile(const char* path, std::string directory)
{
	//Generate texture ID and load texture data
	string filename = string(path);
	filename = directory + '/' + filename;
	unsigned int textureID;
	glGenTextures(1, &textureID);

	QImage texImage;

	if (!texImage.load(QString(filename.c_str())))
	{ // Load first image from file
		qWarning("AssImpModelLoader::textureFromFile - Could not read image file, using single-color instead.");
		QImage dummy(128, 128, QImage::Format_ARGB32);
		dummy.fill(Qt::white);
		texImage = dummy;
	}
	else
	{
		texImage = convertToGLFormat(texImage);
	}

	// Assign texture to ID
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImage.width(), texImage.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, texImage.bits());
	glGenerateMipmap(GL_TEXTURE_2D);

	// Parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureID;
}

QString AssImpModelLoader::getErrorMessage() const
{
	return _errorMessage;
}
