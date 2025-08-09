#include "AssImpModelLoader.h"

#include "IXCAFDocProcessor.hxx"
#include "XCAFDocProcessorFactory.hxx"
#include "XCAFSTEPProcessor.hxx"
#include "XCAFIGESProcessor.hxx"
#include "XCAFBREPProcessor.hxx"

#include "MainWindow.h"
#include "ModelViewer.h"
#include "MeshAnalyzer.h"
#include "TangentGenerator.h"
#include "Utils.h"
#include "UVGenerator.h"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout>
#include <Quantity_ColorRGBA.hxx>


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
void AssImpModelLoader::loadModel(string path, const bool& progressiveLoading)
{
	_progressiveLoading = progressiveLoading;
	_loadingCancelled = false;
	_path = std::string(path);
	_meshes.clear();	
	_materialProcessor.clearLoadedTextures();

	_importer.FreeScene(); // Free any previously loaded scene
	_scene = nullptr; // Reset scene pointer
		
	QFileInfo fi(path.c_str());

#ifdef __DEBUG__
	std::cout << "\n--------------------------------------------------" << std::endl;
	std::cout << "Starting to load model: " << path.c_str() << std::endl;
	std::cout << "File size: " << fi.size() / 1024.0f << " KB" << std::endl;
#endif

	// Check if the file is a supported XCAF document type
	std::unique_ptr<IXCAFDocProcessor> processor = XCAFDocProcessorFactory::createProcessor(fi.suffix().toLower().toStdString());
	if (processor)
	{
		_scene = processor->processFile(path);
	}
	else // all Assimp models
	{
		// Read file via ASSIMP
		_importer.SetPropertyFloat("PP_GSN_MAX_SMOOTHING_ANGLE", 15);
		_scene = _importer.ReadFile(path, aiProcess_CalcTangentSpace |
			aiProcess_GenSmoothNormals |
			aiProcess_FixInfacingNormals |
			aiProcess_JoinIdenticalVertices |
			aiProcess_OptimizeMeshes |
			aiProcess_Triangulate |
			aiProcess_GenUVCoords |
			aiProcess_SortByPType);
	}

	// Check for errors
	if (!_scene || _scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !_scene->mRootNode) // if is Not Zero
	{
		_errorMessage = _importer.GetErrorString();
		cout << "ERROR::ASSIMP:: " << _importer.GetErrorString() << endl;
		return;
	}

	bool modelHasMissingUVs = false;
	for (unsigned int i = 0; i < _scene->mNumMeshes; ++i)
	{
		if (_scene->mMeshes[i]->mTextureCoords[0] == nullptr)
		{
			modelHasMissingUVs = true;
			break;
		}
	}

	_sceneStats = collectSceneMeshInfo(_scene);

	if (modelHasMissingUVs)
	{								
		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());				
		bool remember = settings.value("RememberUVMethod", false).toBool();		
		if (_sceneStats.totalTriangles > 100000 && _selectedUVMethod == UVMethod::AngleBasedSmartUV && remember)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle(tr("Performance Warning!"));
			msgBox.setText(tr("The model contains more than 100000 triangles and the current method of UV generation is \"Smart UV\" which is time consuming.\nDo you want to continue generating the UV?"));
			msgBox.setIcon(QMessageBox::Question);

			// Add custom buttons
			QPushButton* yesButton = msgBox.addButton(QMessageBox::Yes);
			QPushButton* noButton = msgBox.addButton(QMessageBox::No);
			QPushButton* changeSettingsButton = msgBox.addButton(tr("Change Settings"), QMessageBox::ActionRole);

			// Set default button
			msgBox.setDefaultButton(QMessageBox::Yes);

			// Execute and check result
			msgBox.exec();

			if (msgBox.clickedButton() == noButton)
			{
				qDebug() << "User chose not to generate UVs, using None method.";
				_selectedUVMethod = UVMethod::None;
			}
			else if (msgBox.clickedButton() == changeSettingsButton)
			{				
				_selectedUVMethod = ModelViewer::askUserForUVMethod(qApp->activeWindow()).method;
			}
		}			
	}
	else
	{
		qDebug() << "Model has no missing UVs, using None method.";
		_selectedUVMethod = UVMethod::None; // No UVs needed, reset to None
	}


	// Retrieve the directory path of the filepath
	this->_texturePath = path.substr(0, path.find_last_of('/'));

	// Set batch size based on number of meshes;
	int batchSize = std::clamp(_sceneStats.meshCount / 10, 5, 100);
	_batchSize = batchSize;
	qDebug() << "Batch size = " << _batchSize;

	// Process ASSIMP's root node recursively	
	this->processNode(0, _scene->mRootNode, _scene, aiMatrix4x4());

	// Flush any remaining meshes in batch
	if (!_currentBatch.empty())
	{
		emit meshBatchReady(std::move(_currentBatch));
		_currentBatch.clear();
	}

	if (_progressiveLoading)
		emit loadingFinished(true, _scene);
}


// Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
void AssImpModelLoader::processNode(int nodeCounter, aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform)
{
	if (_loadingCancelled)
	{
		emit loadingCancelled();
		return;
	}

	// Compute global transformation matrix for the current node
	aiMatrix4x4 globalTransform = parentTransform * node->mTransformation;

	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		AssImpMesh* myMesh = processMesh(mesh, scene, i, scene->mNumMeshes, globalTransform);

		_meshes.push_back(myMesh);            // full mesh store


		if (_progressiveLoading)
		{
			_currentBatch.push_back(myMesh);      // batch collection
			if (_currentBatch.size() >= _batchSize)
			{
				emit meshBatchReady(std::move(_currentBatch));
				_currentBatch.clear();
			}
		}
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		if (_loadingCancelled)
		{
			emit loadingCancelled();
			return;
		}

		++nodeCounter;
		processNode(nodeCounter, node->mChildren[i], scene, globalTransform);

		if (!_needsUVGeneration && nodeCounter % 20 == 0)
		{
			emit nodeProcessed(i+1, node->mNumChildren, _sceneStats.meshCount, _needsUVGeneration && _selectedUVMethod != UVMethod::None);
		}
		else
			emit nodeProcessed(i+1, node->mNumChildren, _sceneStats.meshCount, _needsUVGeneration && _selectedUVMethod != UVMethod::None);
	}
}


AssImpMesh* AssImpModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, const int& meshIndex, const int& totalMeshes, const aiMatrix4x4& transform)
{
	// Data to fill
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	vector<Texture> textures;
	
	_needsUVGeneration = false;

	// Walk through each of the mesh's vertices
	int step = 0;
	unsigned int nbVertices = mesh->mNumVertices;
	for (unsigned int i = 0; i < nbVertices; i++)
	{
		step++;
		Vertex vertex;
		
		// Compute the normal matrix as the inverse transpose of the transformation matrix
		aiMatrix3x3 normalMatrix = aiMatrix3x3(transform);
		normalMatrix = normalMatrix.Inverse().Transpose(); // Correctly compute the inverse transpose

		// Transform the vertex position by the mesh's transformation matrix
		// Transform Position
		aiVector3D pos = mesh->mVertices[i];
		aiVector3D transformedPos = transform * pos; // Transform position directly
		vertex.Position = glm::vec3(transformedPos.x, transformedPos.y, transformedPos.z);

		// Transform Normals
		aiVector3D normal = mesh->mNormals[i];
		aiVector3D transformedNormal = normalMatrix * normal; // Apply the inverse transpose to the normal
		transformedNormal.Normalize(); // Normalize the normal after transformation (important for scaling)
		vertex.Normal = glm::vec3(transformedNormal.x, transformedNormal.y, transformedNormal.z);

		// Texture Coordinates
		if (mesh->mTextureCoords[0]) // Does the mesh contain texture coordinates?
		{
			glm::vec2 vec;
			// A vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
			// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
			vec.x = mesh->mTextureCoords[0][i].x;
			vec.y = mesh->mTextureCoords[0][i].y;
			vertex.TexCoords = vec;

			// Tangent
			if (mesh->mTangents)
			{
				aiVector3D tangent = mesh->mTangents[i];
				aiVector3D transformedTangent = normalMatrix * tangent; // Transform tangent using the inverse transpose
				transformedTangent.Normalize(); // Normalize the tangent
				vertex.Tangent = glm::vec3(transformedTangent.x, transformedTangent.y, transformedTangent.z);
			}

			// Bitangent
			if (mesh->mBitangents)
			{
				aiVector3D bitangent = mesh->mBitangents[i];
				aiVector3D transformedBitangent = normalMatrix * bitangent; // Transform bitangent using the inverse transpose
				transformedBitangent.Normalize(); // Normalize the bitangent
				vertex.Bitangent = glm::vec3(transformedBitangent.x, transformedBitangent.y, transformedBitangent.z);
			}
		}
		else
		{			
			_needsUVGeneration = true;
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

	// If the mesh has no texture coordinates, we generate them now.
	if (_needsUVGeneration && _selectedUVMethod != UVMethod::None)
	{		
		// Generate UVs for the mesh
		MeshAnalysis::SamplingConfig config;
		config.maxSamples = 200;
		config.sphericalAspectRatio = 0.85f;
		auto analysis = MeshAnalyzer::analyzeMesh(mesh, config);

		// Choose UV config based on surface type
		UVConfig uvconfig;
		switch (analysis.surfaceType)
		{
		case MeshAnalysis::SurfaceType::SPHERICAL:
			uvconfig.sphericalScale = 1.0f;
			uvconfig.seamlessSpherical = true;
			break;

		case MeshAnalysis::SurfaceType::CYLINDRICAL:
			uvconfig.cylindricalScale = 1.0f;
			uvconfig.cylindricalOffset = 0.0f;
			break;

		case MeshAnalysis::SurfaceType::PLANAR:
			uvconfig.planarScale = glm::vec2(1.0f);
			break;

		case MeshAnalysis::SurfaceType::MIXED:
			break;
		}
		
		uvconfig.angleThreshold = 66.0f; // Similar to Blender's default
		uvconfig.enableRelaxation = true;

		// Generate UVs and tangents
		switch (_selectedUVMethod)
		{
		case UVMethod::Planar:
			UVGenerator::generatePlanar(mesh, vertices, indices, uvconfig);
			break;
		case UVMethod::Cylindrical:
			UVGenerator::generateCylindrical(mesh, vertices, indices, uvconfig);
			break;
		case UVMethod::Spherical:
			UVGenerator::generateSpherical(mesh, vertices, indices, uvconfig);
			break;
		case UVMethod::AngleBased:
			UVGenerator::generateAngleBased(mesh, vertices, indices, uvconfig);
			break;
		case UVMethod::Hybrid:
			UVGenerator::generateHybrid(mesh, vertices, indices);
			break;
		case UVMethod::AngleBasedSmartUV:
			UVGenerator::generateAngleBasedSmartUV(mesh, vertices, indices, uvconfig);
			break;
		case UVMethod::None: // fall through
		default:
			break; // skip UV generation
		}

		// MikkTSpace tangents
		TangentGenerator::generateMikkTSpaceTangentsForMesh(vertices, indices);	
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

		_materialProcessor.setFolderPath(this->_texturePath);

		// ADS and PBR Maps
		_materialProcessor.setTextureMaps(material, textures);

		//Set color and material
		_materialProcessor.setColorAndMaterial(material, mat);
	}

	// Return a mesh object created from the extracted mesh data
	QString meshName = QString::fromStdString(mesh->mName.C_Str());
	if(meshName.isEmpty())
	{
		meshName = QFileInfo(QString(_path.data())).baseName() + " (Unnamed Mesh)";
	}
	else
	{
		meshName = QFileInfo(QString(_path.data())).baseName() + " (" + mesh->mName.C_Str() + ")";
	}
	
	AssImpMesh* newMesh =  new AssImpMesh(_prog, meshName, vertices, indices, textures, mat);	
	return newMesh;
}


QString AssImpModelLoader::getErrorMessage() const
{
	return _errorMessage;
}

void AssImpModelLoader::freeScene()
{
	_importer.FreeScene();
	_scene = nullptr;
}

SceneMeshInfo AssImpModelLoader::collectSceneMeshInfo(const aiScene* scene)
{
	SceneMeshInfo info;

	if (!scene || !scene->HasMeshes() || !scene->mRootNode)
		return info;

	std::unordered_set<unsigned int> processedMeshes;

	std::function<void(const aiNode*)> collectFromNode;
	collectFromNode = [&](const aiNode* node) {
		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
		{
			unsigned int meshIndex = node->mMeshes[i];

			// Prevent double-counting if mesh is referenced by multiple nodes
			if (!processedMeshes.insert(meshIndex).second)
				continue;

			const aiMesh* mesh = scene->mMeshes[meshIndex];
			int numFaces = static_cast<int>(mesh->mNumFaces);
			int numVerts = static_cast<int>(mesh->mNumVertices);

			info.totalVertices += numVerts;
			info.totalTriangles += numFaces;
			info.meshCount++;

			if (numFaces > info.largestMeshTriangles)
			{
				info.largestMeshTriangles = numFaces;
				info.largestMeshName = mesh->mName.C_Str();
			}
		}

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			collectFromNode(node->mChildren[i]);
		}
		};

	collectFromNode(scene->mRootNode);

	return info;
}

