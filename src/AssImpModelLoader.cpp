#include "AssImpModelLoader.h"

#include "IXCAFDocProcessor.hxx"
#include "XCAFDocProcessorFactory.hxx"
#include "XCAFSTEPProcessor.hxx"
#include "XCAFIGESProcessor.hxx"
#include "XCAFBREPProcessor.hxx"

#include "MainWindow.h"
#include "ModelViewer.h"
#include "TangentGenerator.h"
#include "Utils.h"
#include "UVGenerator.h"
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout>
#include <Quantity_ColorRGBA.hxx>
#include <cmath>
#include <unordered_set>

using namespace std;

namespace
{
glm::vec3 computeFallbackTangent(const glm::vec3& normal)
{
	const glm::vec3 safeNormal = glm::length(normal) > 0.0001f
		? glm::normalize(normal)
		: glm::vec3(0.0f, 1.0f, 0.0f);

	glm::vec3 referenceAxis = std::abs(safeNormal.y) < 0.999f
		? glm::vec3(0.0f, 1.0f, 0.0f)
		: glm::vec3(1.0f, 0.0f, 0.0f);

	glm::vec3 tangent = glm::cross(referenceAxis, safeNormal);
	if (glm::length(tangent) <= 0.0001f)
	{
		referenceAxis = glm::vec3(0.0f, 0.0f, 1.0f);
		tangent = glm::cross(referenceAxis, safeNormal);
	}

	return glm::normalize(tangent);
}

void assignFallbackTexCoords(Vertex& vertex)
{
	for (glm::vec2& texCoord : vertex.TexCoords)
	{
		texCoord = glm::vec2(0.0f);
	}
}

void assignFallbackTangentBasis(Vertex& vertex)
{
	const glm::vec3 safeNormal = glm::length(vertex.Normal) > 0.0001f
		? glm::normalize(vertex.Normal)
		: glm::vec3(0.0f, 1.0f, 0.0f);

	vertex.Tangent = computeFallbackTangent(safeNormal);
	vertex.Bitangent = glm::normalize(glm::cross(safeNormal, vertex.Tangent));
}
}


bool AssImpModelProgressHandler::Update(float percentage)
{
	emit fileReadProcessed(percentage);
	return !(_cancelFlag && *_cancelFlag);
}

/*  Functions   */
// Constructor, expects a filepath to a 3D model.
AssImpModelLoader::AssImpModelLoader() : QObject(),
	_importer(),
	_scene(nullptr),
	_errorMessage(""),
	_loadingCancelled(false),
	_selectedUVMethod(UVMethod::None),
	_autoScale(true),
	_autoOrient(true)
{
	_loadingCancelled = false;
	_progHandler = new AssImpModelProgressHandler();
	_progHandler->setCancelFlag(&_loadingCancelled);
	_importer.SetProgressHandler(_progHandler);
	connect(_progHandler, SIGNAL(fileReadProcessed(float)), this, SLOT(processFileReadProgress(float)));

	_autoScale = QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
		.value("assimpAutoScaleCheckBox", true).toBool();
	_autoOrient = QSettings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
		.value("assimpAutoOrientCheckBox", true).toBool();
}

AssImpModelLoader::~AssImpModelLoader()
{
	disconnect(_progHandler, SIGNAL(fileReadProcessed(float)), this, SLOT(processFileReadProgress(float)));
	//delete _progHandler; // causes crash
	_progHandler = nullptr;
}

void AssImpModelLoader::setImageTextureUploader(MaterialProcessor::ImageTextureUploadFn uploader)
{
	_materialProcessor.setImageTextureUploader(std::move(uploader));
}

void AssImpModelLoader::setKtx2TextureUploader(MaterialProcessor::Ktx2TextureUploadFn uploader)
{
	_materialProcessor.setKtx2TextureUploader(std::move(uploader));
}

void AssImpModelLoader::processFileReadProgress(float percentage)
{
	emit fileReadProcessed(percentage);
}

void AssImpModelLoader::cancelLoading()
{
	_loadingCancelled = true;
}

AssImpMeshDataBatch AssImpModelLoader::getMeshes() const
{
	return _meshes;
}

/*  Functions   */
// Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
void AssImpModelLoader::loadModel(string path, const bool& progressiveLoading)
{	
	_progressiveLoading = progressiveLoading;
	_loadingCancelled = false;
	_errorMessage.clear();
	_path = std::string(path);
	_meshes.clear();	
	_totalNodeCount = 0;
	_processedNodeCount = 0;
	_processedMeshCount = 0;

	_materialProcessor.clearGLBCaches();
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
			aiProcess_ImproveCacheLocality |
			aiProcess_Triangulate |
			aiProcess_GenUVCoords |
			aiProcess_SortByPType);
	}

	// Check for errors
	if (!_scene || _scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !_scene->mRootNode) // if is Not Zero
	{
		if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
		{
			_loadingCancelled = true;
			_errorMessage = "Model loading cancelled by user.";
			emit loadingCancelled();
			emit loadingFinished(false, nullptr);
			return;
		}

		_errorMessage = _importer.GetErrorString();
		cout << "ERROR::ASSIMP:: " << _importer.GetErrorString() << endl;
		emit loadingFinished(false, nullptr);
		return;
	}

	// If cancellation arrived during a long importer read, stop before any
	// post-read work (UV prompts, scene analysis, traversal) continues.
	if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
	{
		_loadingCancelled = true;
		_errorMessage = "Model loading cancelled by user.";
		emit loadingCancelled();
		emit loadingFinished(false, nullptr);
		return;
	}

	// === Parse glTF primitive modes and correct material structure ===
	QString qPath = QString::fromStdString(path);

	// CRITICAL: Capture original material indices BEFORE deduplication
	// This ensures we preserve the true glTF material indices for later export matching
	_meshIndexToOriginalMaterialIndex.clear();
	if (qPath.endsWith(".gltf", Qt::CaseInsensitive) || qPath.endsWith(".glb", Qt::CaseInsensitive))
	{
		// Save the original material indices before they get remapped
		for (unsigned int i = 0; i < _scene->mNumMeshes; ++i)
		{
			_meshIndexToOriginalMaterialIndex[i] = _scene->mMeshes[i]->mMaterialIndex;
		}

		// Update aiScene materials to match glTF structure (deduplicate, fix references)
		// This must happen BEFORE processing nodes so meshes get correct material assignments
		updateAiSceneWithGltfMaterials(qPath, const_cast<aiScene*>(_scene));

		// Parse primitive modes for rendering
		parseGltfPrimitiveModes(qPath);
	}

	_sceneStats = collectSceneMeshInfo(_scene);
	_totalNodeCount = countNodes(_scene->mRootNode);

	if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
	{
		_loadingCancelled = true;
		_errorMessage = "Model loading cancelled by user.";
		emit loadingCancelled();
		emit loadingFinished(false, nullptr);
		return;
	}

	// check if auto scaling is active and apply it
	applyCoordinateSystemTransformations(path);

	if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
	{
		_loadingCancelled = true;
		_errorMessage = "Model loading cancelled by user.";
		emit loadingCancelled();
		emit loadingFinished(false, nullptr);
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
	
	if (modelHasMissingUVs)
	{								
		if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
		{
			_loadingCancelled = true;
			_errorMessage = "Model loading cancelled by user.";
			emit loadingCancelled();
			emit loadingFinished(false, nullptr);
			return;
		}

		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());				
		bool remember = settings.value("RememberUVMethod", false).toBool();		
		if (_sceneStats.totalTriangles > 100000 && _selectedUVMethod == UVMethod::AngleBasedSmartUV && remember)
		{
			if (_uvDecisionCallback)
			{
				_selectedUVMethod = _uvDecisionCallback(_sceneStats.totalTriangles, _selectedUVMethod);
			}
			else
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
	}
	else
	{		
		_selectedUVMethod = UVMethod::None; // No UVs needed, reset to None
	}

	// Retrieve the directory path of the filepath
	this->_texturePath = path.substr(0, path.find_last_of('/'));

	// Set batch size based on number of meshes;
	int batchSize = std::clamp(_sceneStats.meshCount / 10, 5, 100);
	_batchSize = batchSize;

	if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
	{
		_loadingCancelled = true;
		_errorMessage = "Model loading cancelled by user.";
		emit loadingCancelled();
		emit loadingFinished(false, nullptr);
		return;
	}

	// Process ASSIMP's root node recursively	
	this->processNode(0, _scene->mRootNode, _scene, aiMatrix4x4());

	if (_loadingCancelled || MainWindow::isFileLoadCancelRequested())
	{
		_loadingCancelled = true;
		_errorMessage = "Model loading cancelled by user.";
		emit loadingFinished(false, _scene);
		return;
	}
	
	// Flush any remaining meshes in batch
	if (!_currentBatch.empty())
	{
		emit meshBatchReady(std::move(_currentBatch));
		_currentBatch.clear();
	}

	// === Parse KHR_lights_punctual extension ===
	std::vector<GPULight> parsedLights;
	QString gltfPath = QString::fromStdString(path);

	if (gltfPath.endsWith(".gltf", Qt::CaseInsensitive) || gltfPath.endsWith(".glb", Qt::CaseInsensitive))
	{
		parsedLights = _materialProcessor.parseKHRLightsPunctual(gltfPath);
		if (!parsedLights.empty())
		{
			qDebug() << "AssImpModelLoader: Loaded" << parsedLights.size() << "KHR lights with transforms";
		}
	}

	if (!parsedLights.empty() && (_autoScale || _autoOrient))
	{
		for (auto& light : parsedLights)
		{
			// Transform position (with translation)
			glm::vec4 transformedPos = _appliedTransform * glm::vec4(light.position, 1.0f);
			light.position = glm::vec3(transformedPos);

			// Transform direction (no translation)
			glm::vec4 transformedDir = _appliedTransform * glm::vec4(light.direction, 0.0f);
			light.direction = glm::normalize(glm::vec3(transformedDir));

			// Extract scale from transform matrix
			glm::vec3 scale(
				glm::length(glm::vec3(_appliedTransform[0])),
				glm::length(glm::vec3(_appliedTransform[1])),
				glm::length(glm::vec3(_appliedTransform[2]))
			);
			float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
			light.range *= avgScale;
		}

	}

	// Emit lights for GLWidget to handle
	emit lightsLoaded(parsedLights);
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
	++_processedNodeCount;

	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];		
		AssImpMeshData myMesh = processMesh(mesh, scene, node->mMeshes[i], scene->mNumMeshes, globalTransform, node->mName.C_Str());

		_meshes.push_back(myMesh);            // full mesh store
		++_processedMeshCount;
		emit nodeMeshProgressUpdated(
			_processedNodeCount,
			_totalNodeCount,
			_processedMeshCount,
			_sceneStats.meshCount,
			_needsUVGeneration && _selectedUVMethod != UVMethod::None);


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
	}

	const bool uvProcessed = _needsUVGeneration && _selectedUVMethod != UVMethod::None;
	emit nodeMeshProgressUpdated(_processedNodeCount, _totalNodeCount, _processedMeshCount, _sceneStats.meshCount, uvProcessed);
}

int AssImpModelLoader::countNodes(const aiNode* node) const
{
	if (!node)
		return 0;

	int count = 1;
	for (unsigned int i = 0; i < node->mNumChildren; ++i)
	{
		count += countNodes(node->mChildren[i]);
	}

	return count;
}


AssImpMeshData AssImpModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, const int& meshIndex, const int& totalMeshes, const aiMatrix4x4& transform, const char* nodeName)
{
	// Data to fill
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	vector<GLMaterial::Texture> textures;
	
	_needsUVGeneration = false;

	bool isNonTrianglePrimitive = false;
	if (_gltfMeshPrimitiveModes.find(meshIndex) != _gltfMeshPrimitiveModes.end())
	{
		GLenum mode = _gltfMeshPrimitiveModes[meshIndex];
		isNonTrianglePrimitive = (mode != GL_TRIANGLES &&
			mode != GL_TRIANGLE_STRIP &&
			mode != GL_TRIANGLE_FAN);
	}

	// Walk through each of the mesh's vertices
	int step = 0;
	unsigned int nbVertices = mesh->mNumVertices;

	// Check if we need to generate normals
	bool hasNormals = mesh->mNormals != nullptr;
	bool canGenerateNormals = HasSurfaceGeometry(mesh);
	std::vector<glm::vec3> generatedNormals;

	if (!hasNormals && canGenerateNormals)
	{
		GenerateFaceNormals(mesh, generatedNormals);
		printf("Generated normals for mesh with %u vertices and %u faces\n",
			mesh->mNumVertices, mesh->mNumFaces);
	}

	bool hasNegativeScale = false;
	for (unsigned int i = 0; i < nbVertices; i++)
	{
		step++;
		Vertex vertex{};
		assignFallbackTexCoords(vertex);

		// Compute the normal matrix as the inverse transpose of the transformation matrix
		aiMatrix3x3 normalMatrix = aiMatrix3x3(transform);
		normalMatrix = normalMatrix.Inverse().Transpose();

		// Detect negative scale by computing determinant of the 3x3 transform
		glm::mat3 glmTransform = glm::mat3(
			transform.a1, transform.a2, transform.a3,
			transform.b1, transform.b2, transform.b3,
			transform.c1, transform.c2, transform.c3
		);
		float determinant = glm::determinant(glmTransform);
		hasNegativeScale = determinant < 0.0f;

		// Transform Position
		aiVector3D pos = mesh->mVertices[i];
		aiVector3D transformedPos = transform * pos;
		vertex.Position = glm::vec3(transformedPos.x, transformedPos.y, transformedPos.z);

		// Transform Normals - improved logic
		if (hasNormals)
		{
			// Use existing normals from the mesh
			aiVector3D normal = mesh->mNormals[i];
			aiVector3D transformedNormal = normalMatrix * normal;
			transformedNormal.Normalize();

			// Flip normal if negative scale detected
			if (hasNegativeScale)
			{
				transformedNormal = -transformedNormal;
			}

			vertex.Normal = glm::vec3(transformedNormal.x, transformedNormal.y, transformedNormal.z);
		}
		else if (!generatedNormals.empty())
		{
			// Use generated face normals
			glm::vec3 normal = generatedNormals[i];
			aiVector3D aiNormal(normal.x, normal.y, normal.z);
			aiVector3D transformedNormal = normalMatrix * aiNormal;
			transformedNormal.Normalize();
			// Flip normal if negative scale detected
			if (hasNegativeScale)
			{
				transformedNormal = -transformedNormal;
			}
			vertex.Normal = glm::vec3(transformedNormal.x, transformedNormal.y, transformedNormal.z);
		}
		else
		{
			if (isNonTrianglePrimitive)
			{
				// Points, lines, etc. - use null normal to indicate "no lighting"
				vertex.Normal = glm::vec3(0.0f, 0.0f, 0.0f);				
			}
			else
			{
				// Fallback for points, lines, or invalid geometry
				// Use transformed up vector instead of position
				aiVector3D upVector(0.0f, 1.0f, 0.0f);
				aiVector3D transformedUp = normalMatrix * upVector;
				transformedUp.Normalize();
				// Flip normal if negative scale detected
				if (hasNegativeScale)
				{
					transformedUp = -transformedUp;
				}
				vertex.Normal = glm::vec3(transformedUp.x, transformedUp.y, transformedUp.z);
			}
		}

		// Texture Coordinates - Extract ALL available sets (0-3)
		bool hasAnyTexCoords = false;
		for (int texCoordSet = 0; texCoordSet < 4; texCoordSet++)
		{
			if (mesh->mTextureCoords[texCoordSet])
			{
				glm::vec2 vec;
				vec.x = mesh->mTextureCoords[texCoordSet][i].x;
				vec.y = mesh->mTextureCoords[texCoordSet][i].y;
				vertex.TexCoords[texCoordSet] = vec;
				hasAnyTexCoords = true;
			}
			else
			{
				// Initialize unused sets to zero (for safety)
				vertex.TexCoords[texCoordSet] = glm::vec2(0.0f);
			}
		}

		if (hasAnyTexCoords)
		{
			assignFallbackTangentBasis(vertex);

			// Tangent (only process if we have texCoords)
			if (mesh->mTangents)
			{
				aiVector3D tangent = mesh->mTangents[i];
				aiVector3D transformedTangent = normalMatrix * tangent;
				transformedTangent.Normalize();
				// Flip tangent if negative scale detected
				if (hasNegativeScale)
				{
					transformedTangent = -transformedTangent;
				}
				vertex.Tangent = glm::vec3(transformedTangent.x, transformedTangent.y, transformedTangent.z);
			}

			// Bitangent
			if (mesh->mBitangents)
			{
				aiVector3D bitangent = mesh->mBitangents[i];
				aiVector3D transformedBitangent = normalMatrix * bitangent;
				transformedBitangent.Normalize();
				// Flip bitangent if negative scale detected
				if (hasNegativeScale)
				{
					transformedBitangent = -transformedBitangent;
				}
				vertex.Bitangent = glm::vec3(transformedBitangent.x, transformedBitangent.y, transformedBitangent.z);
			}
		}
		else
		{
			// Keep a deterministic fallback UV/tangent basis so later texture
			// application samples a constant texel instead of reading garbage.
			assignFallbackTangentBasis(vertex);
			_needsUVGeneration = true;
		}

		// Vertex Color
		if (mesh->HasVertexColors(0))
		{
			aiColor4D color = mesh->mColors[0][i];
			vertex.Color = glm::vec4(color.r, color.g, color.b, color.a);
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
				
		generateUVsForMesh(analysis, mesh, vertices, indices);
	}

	// Determine the correct material index for this mesh.
	// For glTF files: use the PRE-DEDUPLICATION index saved before updateAiSceneWithGltfMaterials()
	// This is critical because deduplication remaps mesh->mMaterialIndex
	// For other formats: use the current index (not remapped)
	int originalMaterialIndex = mesh->mMaterialIndex;

	qDebug() << "[IMPORT] processMesh[" << meshIndex << "] nodeName=" << nodeName
	         << "- mesh->mMaterialIndex (raw)=" << mesh->mMaterialIndex;

	// If this is from a glTF file, look up the true original index
	// We need to find which aiMesh index this is to look it up in our saved mapping
	for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx)
	{
		if (scene->mMeshes[meshIdx] == mesh)
		{
			// Found this mesh's index in the scene
			auto it = _meshIndexToOriginalMaterialIndex.find(meshIdx);
			if (it != _meshIndexToOriginalMaterialIndex.end())
			{
				originalMaterialIndex = it->second;
				qDebug() << "  [IMPORT-glTF] Using PRE-DEDUP material index" << originalMaterialIndex
					     << "for aiMesh[" << meshIdx << "] (remapped was:" << mesh->mMaterialIndex << ")";
			}
			break;
		}
	}

	// Process materials
	GLMaterial mat = GLMaterial::DEFAULT_MAT();
	//if (mesh->mMaterialIndex != 0)

	// DEBUG: Log the material index assignment
	qDebug() << "[IMPORT] processMesh[" << meshIndex << "] FINAL"
	         << "nodeName=" << nodeName
	         << "mNumVertices=" << mesh->mNumVertices
	         << "mNumFaces=" << mesh->mNumFaces
	         << "originalMaterialIndex=" << originalMaterialIndex
	         << "scene->mNumMaterials=" << scene->mNumMaterials;

	// CRITICAL FIX: Use originalMaterialIndex to load material properties, not mesh->mMaterialIndex
	// When Assimp remaps materials, mesh->mMaterialIndex may point to wrong material slot.
	// We must use the pre-dedup originalMaterialIndex to access the correct material properties.
	int materialIndexToUse = originalMaterialIndex;
	if (materialIndexToUse < scene->mNumMaterials)
	{
		aiMaterial* material = scene->mMaterials[materialIndexToUse];
		// We assume a convention for sampler names in the shaders. Each diffuse texture should be named
		// as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
		// Same applies to other texture as the following list summarizes:
		// Diffuse: texture_diffuseN
		// Specular: texture_specularN
		// Normal: texture_normalN

		_materialProcessor.setFolderPath(this->_texturePath);

		// Determine file type
		bool isGlb = (_path.find(".glb") != std::string::npos);
		bool isGltf = (_path.find(".gltf") != std::string::npos && !isGlb);  // exclude .glb

		if (isGltf || isGlb)
		{
			_materialProcessor.processGltf2CoreAndExtensions(
				QString::fromStdString(_path),
				scene,
				QString::fromUtf8(nodeName),
				mesh,
				originalMaterialIndex,
				mat,
				textures);
			
			// Ensure scene textures are valid for deep copy / merging
			_materialProcessor.ensureAssimpSceneTexturesValid(
				const_cast<aiScene*>(scene),
				QString::fromStdString(_path));
			
			// Scale parameters based on model scale
			mat.setThicknessFactor(mat.thicknessFactor()* _appliedScale);
			mat.setAttenuationDistance(mat.attenuationDistance()* _appliedScale);
			mat.setIsGLTFMaterial(true);
			qDebug() << "GLTF Material Loaded";
		}
		else
		{
			//Set color and material
			_materialProcessor.processAssimpColorAndMaterial(material, mat);
			// ADS and PBR Maps from Assimp
			_materialProcessor.processAssimpTextureMaps(material, textures, mat);
		}
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

	// Material and textures details
	qDebug() << "Mesh with material: " << meshName << " processed.";
	std::cout << mat;	

	AssImpMeshData meshData;
	meshData.name = meshName;
	meshData.vertices = std::move(vertices);
	meshData.indices = std::move(indices);
	meshData.textures = std::move(textures);
	meshData.material = mat;
	meshData.hasNegativeScale = hasNegativeScale;
	meshData.sceneIndex = meshIndex;
	meshData.originalMaterialIndex = originalMaterialIndex;

	qDebug() << "[IMPORT-STORED] MeshData for" << meshName
	         << "sceneIndex=" << meshIndex
	         << "originalMaterialIndex=" << originalMaterialIndex
	         << "materialName=" << mat.name();

	if (_gltfMeshPrimitiveModes.find(meshIndex) != _gltfMeshPrimitiveModes.end())
	{
		meshData.primitiveMode = _gltfMeshPrimitiveModes[meshIndex];
		qDebug() << "Set primitive mode for mesh" << meshIndex << "to" << meshData.primitiveMode;
	}

	return meshData;
}

void AssImpModelLoader::generateUVsForMesh(MeshAnalysis::AnalysisResult& analysis, aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<std::seed_seq::result_type>& indices)
{
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
		UVGenerator::generatePlanar(vertices, indices, uvconfig);
		break;
	case UVMethod::Cylindrical:
		UVGenerator::generateCylindrical(vertices, indices, uvconfig);
		break;
	case UVMethod::Spherical:
		UVGenerator::generateSpherical(vertices, indices, uvconfig);
		break;
	case UVMethod::AngleBased:
		UVGenerator::generateAngleBased(vertices, indices, uvconfig);
		break;
	case UVMethod::Hybrid:
		UVGenerator::generateHybrid(vertices, indices);
		break;
	case UVMethod::AngleBasedSmartUV:
		UVGenerator::generateAngleBasedSmartUV(vertices, indices, uvconfig);
		break;
	case UVMethod::None: // fall through
	default:
		break; // skip UV generation
	}

	// MikkTSpace tangents
	TangentGenerator::generateMikkTSpaceTangentsForMesh(vertices, indices);
}

bool AssImpModelLoader::HasSurfaceGeometry(aiMesh* mesh)
{
	if (!mesh || mesh->mNumFaces == 0)
	{
		return false;
	}

	// Check if mesh has triangular faces
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		if (mesh->mFaces[i].mNumIndices >= 3)
		{
			return true;
		}
	}
	return false;
}

void AssImpModelLoader::GenerateFaceNormals(aiMesh* mesh, std::vector<glm::vec3>& generatedNormals)
{
	generatedNormals.clear();
	generatedNormals.resize(mesh->mNumVertices, glm::vec3(0.0f));

	// Only generate normals for meshes with triangular faces
	if (mesh->mNumFaces == 0)
	{
		// No faces - fill with default up normals
		std::fill(generatedNormals.begin(), generatedNormals.end(), glm::vec3(0.0f, 1.0f, 0.0f));
		return;
	}

	// Calculate normals from faces
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		const aiFace& face = mesh->mFaces[i];

		if (face.mNumIndices >= 3)
		{ 
			// Only process triangles/polygons
			// Get three vertices of the face
			aiVector3D v0 = mesh->mVertices[face.mIndices[0]];
			aiVector3D v1 = mesh->mVertices[face.mIndices[1]];
			aiVector3D v2 = mesh->mVertices[face.mIndices[2]];

			// Calculate face normal using cross product
			aiVector3D edge1 = v1 - v0;
			aiVector3D edge2 = v2 - v0;
			aiVector3D faceNormal = edge1 ^ edge2; // Cross product in Assimp

			// Check if normal is valid (non-zero length)
			float length = faceNormal.Length();
			if (length > 0.0001f)
			{
				faceNormal.Normalize();

				// Add this face normal to all vertices of the face (for smooth shading)
				for (unsigned int j = 0; j < face.mNumIndices; j++)
				{
					unsigned int vertexIndex = face.mIndices[j];
					if (vertexIndex < generatedNormals.size())
					{
						generatedNormals[vertexIndex] += glm::vec3(faceNormal.x, faceNormal.y, faceNormal.z);
					}
				}
			}
		}
	}

	// Normalize all accumulated normals
	for (auto& normal : generatedNormals)
	{
		float length = glm::length(normal);
		if (length > 0.0001f)
		{
			normal = glm::normalize(normal);
		}
		else
		{
			// Fallback for vertices not part of any valid face
			normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default up vector
		}
	}
}


QString AssImpModelLoader::getErrorMessage() const
{
	return _errorMessage;
}

bool AssImpModelLoader::regenerateUVs(AssImpMesh* mesh,
	UVMethod method,
	const UVConfig& config)
{
	if (!mesh) return false;

	// Get current mesh data
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	mesh->getMeshData(vertices, indices);

	// Generate UVs and tangents
	switch (method)
	{
	case UVMethod::Planar:
		UVGenerator::generatePlanar(vertices, indices, config);
		break;
	case UVMethod::Cylindrical:
		UVGenerator::generateCylindrical(vertices, indices, config);
		break;
	case UVMethod::Spherical:
		UVGenerator::generateSpherical(vertices, indices, config);
		break;
	case UVMethod::AngleBased:
		UVGenerator::generateAngleBased(vertices, indices, config);
		break;
	case UVMethod::Hybrid:
		UVGenerator::generateHybrid(vertices, indices);
		break;
	case UVMethod::AngleBasedSmartUV:
		UVGenerator::generateAngleBasedSmartUV(vertices, indices, config);
		break;
	case UVMethod::None: // fall through
	default:
		break; // skip UV generation
	}

	// MikkTSpace tangents
	TangentGenerator::generateMikkTSpaceTangentsForMesh(vertices, indices);

	// Set data back to mesh (will call setupMesh internally)
	mesh->setMeshData(vertices, indices);

	return true;
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

	bool firstVertex = true;
	double minX = DBL_MAX, maxX = -DBL_MAX;
	double minY = DBL_MAX, maxY = -DBL_MAX;
	double minZ = DBL_MAX, maxZ = -DBL_MAX;

	float minMeshDimension = std::numeric_limits<float>::max();
	float maxMeshDimension = 0.0f;

	std::unordered_set<unsigned int> processedMeshes;

	std::function<void(const aiNode*, const glm::mat4&)> collectFromNode;
	collectFromNode = [&](const aiNode* node, const glm::mat4& parentTransform) {
		glm::mat4 nodeTransform = parentTransform * SceneUtils::aiMatrixToGlm(node->mTransformation);

		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
		{
			unsigned int meshIndex = node->mMeshes[i];
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

			// Track per-mesh bounding box
			double meshMinX = DBL_MAX, meshMaxX = -DBL_MAX;
			double meshMinY = DBL_MAX, meshMaxY = -DBL_MAX;
			double meshMinZ = DBL_MAX, meshMaxZ = -DBL_MAX;

			for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
			{
				glm::vec4 vertex = nodeTransform * glm::vec4(
					mesh->mVertices[j].x,
					mesh->mVertices[j].y,
					mesh->mVertices[j].z,
					1.0f
				);

				double x = static_cast<double>(vertex.x);
				double y = static_cast<double>(vertex.y);
				double z = static_cast<double>(vertex.z);

				// Update scene bounding box
				if (firstVertex)
				{
					minX = maxX = x;
					minY = maxY = y;
					minZ = maxZ = z;
					firstVertex = false;
				}
				else
				{
					minX = std::min(minX, x);
					maxX = std::max(maxX, x);
					minY = std::min(minY, y);
					maxY = std::max(maxY, y);
					minZ = std::min(minZ, z);
					maxZ = std::max(maxZ, z);
				}

				// Update mesh bounding box
				meshMinX = std::min(meshMinX, x);
				meshMaxX = std::max(meshMaxX, x);
				meshMinY = std::min(meshMinY, y);
				meshMaxY = std::max(meshMaxY, y);
				meshMinZ = std::min(meshMinZ, z);
				meshMaxZ = std::max(meshMaxZ, z);
			}

			// Calculate this mesh's dimensions
			double meshSizeX = meshMaxX - meshMinX;
			double meshSizeY = meshMaxY - meshMinY;
			double meshSizeZ = meshMaxZ - meshMinZ;
			float meshMaxDim = static_cast<float>(std::max({ meshSizeX, meshSizeY, meshSizeZ }));

			// Track smallest and largest across all meshes
			minMeshDimension = std::min(minMeshDimension, meshMaxDim);
			maxMeshDimension = std::max(maxMeshDimension, meshMaxDim);
		}

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			collectFromNode(node->mChildren[i], nodeTransform);
		}
		};

	collectFromNode(scene->mRootNode, glm::mat4(1.0f));

	if (!firstVertex)
	{
		info.boundingBox.setLimits(minX, minY, minZ, maxX, maxY, maxZ);
		info.minDimension = minMeshDimension;
		info.maxDimension = maxMeshDimension;
	}
	else
	{
		info.boundingBox.setLimits(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		info.minDimension = 0.0f;
		info.maxDimension = 0.0f;
	}

	return info;
}

void AssImpModelLoader::applyCoordinateSystemTransformations(const std::string& path)
{
	if (_autoScale || _autoOrient)
	{
		_appliedTransform = glm::mat4(1.0f);

		// Apply only coordinate system conversion
		if (_autoOrient)
		{
			glm::mat4 coordTransform = getCoordinateSystemTransform(_scene, path);
			_appliedTransform = coordTransform;
		}

		// Apply scaling separately
		if (_autoScale)
		{
			_appliedScale = calculateConditionalScale(_sceneStats.minDimension, _sceneStats.maxDimension);
			_appliedTransform = glm::scale(_appliedTransform, glm::vec3(_appliedScale));
		}

		applyTransformToNode(_scene->mRootNode, _appliedTransform);
	}
}

void AssImpModelLoader::applyTransformToNode(aiNode* node, const glm::mat4& transform)
{
	if (!node) return;

	// Convert glm::mat4 to aiMatrix4x4
	aiMatrix4x4 aiTransform = SceneUtils::glmToAiMatrix(transform);
	
	// Apply transformation to the node
	node->mTransformation = aiTransform * node->mTransformation;
}

glm::mat4 AssImpModelLoader::getCoordinateSystemTransform(const aiScene* scene, const std::string& filePath)
{
	glm::mat4 transform = glm::mat4(1.0f);
	bool foundMetadata = false;
		
	// First: Try to get coordinate system from scene metadata
	if (scene && scene->mMetaData)
	{	
		// Try different possible metadata keys
		aiString upAxis;
		int upAxisInt;

		// String-based up axis
		if (scene->mMetaData->Get("UpAxis", upAxis) ||
			scene->mMetaData->Get("up_axis", upAxis) ||
			scene->mMetaData->Get("UP_AXIS", upAxis) ||
			scene->mMetaData->Get("CoordinateSystem", upAxis))
		{

			std::string upStr = upAxis.C_Str();
			std::transform(upStr.begin(), upStr.end(), upStr.begin(), ::tolower);

			if (upStr.find("y") != std::string::npos || upStr == "y_up")
			{
				transform = glm::rotate(glm::mat4(1.0f),
					glm::radians(90.0f),
					glm::vec3(1.0f, 0.0f, 0.0f));
				foundMetadata = true;
			}
			else if (upStr.find("z") != std::string::npos || upStr == "z_up")
			{
				transform = glm::mat4(1.0f); // Already Z-up
				foundMetadata = true;
			}			
		}
		// Integer-based up axis
		else if (scene->mMetaData->Get("UpAxis", upAxisInt) ||
			scene->mMetaData->Get("up_axis", upAxisInt))
		{
			switch (upAxisInt)
			{
			case 1: // Y-up
				transform = glm::rotate(glm::mat4(1.0f),
					glm::radians(90.0f),
					glm::vec3(1.0f, 0.0f, 0.0f));
				foundMetadata = true;
				break;
			case 2: // Z-up
				transform = glm::mat4(1.0f);
				foundMetadata = true;
				break;
			}
		}

#ifdef DEBUG
		if (foundMetadata)
		{
			printf("Found coordinate system metadata\n");
		}
		else
		{
			printf("No coordinate system metadata found\n");
		}
#endif
	}

	// Fallback to file extension if no metadata found
	if (!foundMetadata)
	{
		std::string extension = filePath.substr(filePath.find_last_of("."));
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		transform = getCoordinateSystemFromFileType(extension);
	}

	return transform;
}

glm::mat4 AssImpModelLoader::getCoordinateSystemFromFileType(const std::string& fileExtension)
{
	glm::mat4 transform = glm::mat4(1.0f);

	if (fileExtension == ".gltf" || fileExtension == ".glb")
	{
		// GLTF: Always Y-up by specification
		transform = glm::rotate(glm::mat4(1.0f),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else if (fileExtension == ".obj")
	{
		// OBJ: Usually Y-up (Wavefront OBJ spec)
		transform = glm::rotate(glm::mat4(1.0f),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else if (fileExtension == ".fbx")
	{
		// FBX: Usually Y-up (but can vary based on export settings)
		transform = glm::rotate(glm::mat4(1.0f),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else if (fileExtension == ".dae")
	{
		// Collada: Y-up by default
		transform = glm::rotate(glm::mat4(1.0f),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else if (fileExtension == ".blend")
	{
		// Blender: Y-up
		transform = glm::rotate(glm::mat4(1.0f),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else if (fileExtension == ".3ds" || fileExtension == ".max")
	{
		// 3ds Max files: Z-up (no conversion needed)
		transform = glm::mat4(1.0f);
	}
	else if (fileExtension == ".x3d")
	{
		// X3D: Y-up by specification
		transform = glm::rotate(glm::mat4(1.0f),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));
	}
	else if (fileExtension == ".ply")
	{
		// PLY: No standard, but commonly Z-up from scanning
		transform = glm::mat4(1.0f);
	}
	else if (fileExtension == ".stl")
	{
		// STL: No standard, varies by source
		// Default to no conversion, let user override
		transform = glm::mat4(1.0f);
	}
	else
	{
		// Unknown format: assume no conversion needed
		// Log this for debugging
#ifdef DEBUG
		printf("Unknown file format %s, assuming Z-up\n", fileExtension.c_str());
#endif
		transform = glm::mat4(1.0f);
	}

	return transform;
}

float AssImpModelLoader::calculateConditionalScale(const float& minDimension, const float& maxDimension)
{
	if (maxDimension < 1e-6f)
	{
		return 1.0f; // Avoid division by zero
	}

	constexpr float MIN_DIMENSION = 0.01f;
	constexpr float MAX_DIMENSION = 100000.0f;

	// Always scale up if minimum dimension is below threshold
	if (minDimension < MIN_DIMENSION)
	{
		double scale = static_cast<double>(MIN_DIMENSION) / static_cast<double>(minDimension);
		return static_cast<float>(scale);
	}

	// Scale down only if it doesn't push minimum dimension below threshold
	if (maxDimension > MAX_DIMENSION)
	{
		double proposedScale = static_cast<double>(MAX_DIMENSION) / static_cast<double>(maxDimension);
		double scaledMinDimension = static_cast<double>(minDimension) * proposedScale;

		// Only apply scale if minimum dimension stays within bounds
		if (scaledMinDimension >= MIN_DIMENSION)
		{
			return static_cast<float>(proposedScale);
		}
	}

	return 1.0f;
}

void AssImpModelLoader::parseGltfPrimitiveModes(const QString& gltfPath)
{
	_gltfMeshPrimitiveModes.clear();

	bool isGLB = gltfPath.endsWith(".glb", Qt::CaseInsensitive);
	bool isGLTF = gltfPath.endsWith(".gltf", Qt::CaseInsensitive);

	if (!isGLB && !isGLTF)
	{
		qWarning() << "parseGltfPrimitiveModes: Not a glTF file:" << gltfPath;
		return;
	}

	QJsonDocument doc;

	// ===== HANDLE GLB FILES =====
	if (isGLB)
	{
		std::vector<uint8_t> glbBinaryBuffer;
		QString jsonString = MaterialProcessor::extractJsonFromGLB(gltfPath, glbBinaryBuffer);

		if (jsonString.isEmpty())
		{
			qWarning() << "parseGltfPrimitiveModes: Failed to extract JSON from GLB:" << gltfPath;
			return;
		}

		QJsonParseError perr;
		doc = QJsonDocument::fromJson(jsonString.toUtf8(), &perr);
		if (perr.error != QJsonParseError::NoError)
		{
			qWarning() << "parseGltfPrimitiveModes: JSON parse error in GLB:" << perr.errorString();
			return;
		}
	}
	// ===== HANDLE GLTF FILES =====
	else
	{
		QFile file(gltfPath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			qWarning() << "Failed to open glTF file for primitive mode parsing:" << gltfPath;
			return;
		}

		doc = QJsonDocument::fromJson(file.readAll());
		file.close();

		if (!doc.isObject())
		{
			qWarning() << "Invalid glTF JSON structure";
			return;
		}
	}

	// ===== PARSE PRIMITIVE MODES (same for both GLTF and GLB) =====
	QJsonObject root = doc.object();
	QJsonArray meshesArray = root["meshes"].toArray();

	unsigned int meshIndex = 0;
	for (const QJsonValue& meshValue : meshesArray)
	{
		QJsonObject meshObj = meshValue.toObject();
		QJsonArray primitives = meshObj["primitives"].toArray();

		if (!primitives.isEmpty())
		{
			QJsonObject firstPrimitive = primitives[0].toObject();

			// glTF primitive modes:
			// 0 = POINTS,  1 = LINES,  2 = LINE_LOOP,  3 = LINE_STRIP
			// 4 = TRIANGLES,  5 = TRIANGLE_STRIP,  6 = TRIANGLE_FAN

			int mode = firstPrimitive["mode"].toInt(4);  // Default to TRIANGLES

			GLenum glMode = GL_TRIANGLES;
			switch (mode)
			{
			case 0: glMode = GL_POINTS; break;
			case 1: glMode = GL_LINES; break;
			case 2: glMode = GL_LINE_LOOP; break;
			case 3: glMode = GL_LINE_STRIP; break;
			case 4: glMode = GL_TRIANGLES; break;
			case 5: glMode = GL_TRIANGLE_STRIP; break;
			case 6: glMode = GL_TRIANGLE_FAN; break;
			default: glMode = GL_TRIANGLES; break;
			}

			_gltfMeshPrimitiveModes[meshIndex] = glMode;
		}

		++meshIndex;
	}
}

void AssImpModelLoader::updateAiSceneWithGltfMaterials(const QString& gltfPath, aiScene* scene)
{
	if (!scene || scene->mNumMaterials == 0)
		return;

	bool isGLB = gltfPath.endsWith(".glb", Qt::CaseInsensitive);
	bool isGLTF = gltfPath.endsWith(".gltf", Qt::CaseInsensitive);

	if (!isGLB && !isGLTF)
		return;

	QJsonDocument doc;

	// ===== READ GLTF JSON =====
	if (isGLB)
	{
		std::vector<uint8_t> glbBinaryBuffer;
		QString jsonString = MaterialProcessor::extractJsonFromGLB(gltfPath, glbBinaryBuffer);
		if (jsonString.isEmpty())
			return;

		doc = QJsonDocument::fromJson(jsonString.toUtf8());
	}
	else
	{
		QFile file(gltfPath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return;

		doc = QJsonDocument::fromJson(file.readAll());
		file.close();
	}

	if (!doc.isObject())
		return;

	QJsonObject root = doc.object();
	QJsonArray gltfMaterials = root["materials"].toArray();

	if (gltfMaterials.isEmpty())
		return;

	// ===== BUILD DEDUPLICATION MAP =====
	// Compare glTF materials to aiScene materials
	// Goal: identify which aiMaterials are duplicates and update mesh references

	std::map<int, int> aiMatToGltfMat;  // aiMaterial index -> glTF material index (primary)
	std::vector<bool> aiMatIsUsed(scene->mNumMaterials, false);

	// For each glTF material, find the corresponding aiMaterial
	// Assume they're in the same order (Assimp preserves glTF material order)
	for (unsigned int gltfIdx = 0; gltfIdx < gltfMaterials.size() && gltfIdx < scene->mNumMaterials; ++gltfIdx)
	{
		int aiIdx = static_cast<int>(gltfIdx);
		aiMatToGltfMat[aiIdx] = static_cast<int>(gltfIdx);
		aiMatIsUsed[aiIdx] = true;
	}

	// For remaining aiMaterials (beyond glTF count), deduplicate by content
	if (scene->mNumMaterials > static_cast<int>(gltfMaterials.size()))
	{
		for (unsigned int i = gltfMaterials.size(); i < scene->mNumMaterials; ++i)
		{
			// Find if this material is a duplicate of an earlier one
			bool isDuplicate = false;

			for (unsigned int j = 0; j < i && !isDuplicate; ++j)
			{
				// Simple check: compare material names (glTF uses names for dedup)
				if (scene->mMaterials[i]->GetName() == scene->mMaterials[j]->GetName())
				{
					aiMatToGltfMat[i] = aiMatToGltfMat[j];  // Point to the primary
					isDuplicate = true;
				}
			}

			if (!isDuplicate)
			{
				// Unique material, keep it
				aiMatToGltfMat[i] = static_cast<int>(i);
				aiMatIsUsed[i] = true;
			}
		}
	}

	// ===== UPDATE MESH MATERIAL REFERENCES =====
	// Redirect mesh primitives from duplicate materials to primary materials
	for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];
		int oldMatIdx = mesh->mMaterialIndex;

		if (oldMatIdx >= 0 && oldMatIdx < static_cast<int>(scene->mNumMaterials))
		{
			// Find the primary material for this mesh
			int primaryMatIdx = aiMatToGltfMat[oldMatIdx];
			mesh->mMaterialIndex = primaryMatIdx;
		}
	}

	qDebug() << "updateAiSceneWithGltfMaterials: Updated material references, scene has"
		<< scene->mNumMaterials << "materials from glTF with" << gltfMaterials.size() << "unique entries";
}
