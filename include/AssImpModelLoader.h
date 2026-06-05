#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "AssImpMesh.h"
#include "BoundingBox.h"
#include "MaterialProcessor.h"
#include "MeshAnalyzer.h"
#include "TriangleMesh.h"
#include "UVGenerator.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/ProgressHandler.hpp>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gp_Trsf.hxx>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <Quantity_Color.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Tool.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS_Shape.hxx>
#include <tuple>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_Location.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include "GLLights.h"
#include "GltfAnimationData.h"
#include "GltfCameraData.h"
#include "GltfVariantData.h"


class AssImpModelProgressHandler : public QObject, public Assimp::ProgressHandler
{
	Q_OBJECT
public:
	void setCancelFlag(const bool* cancelFlag) { _cancelFlag = cancelFlag; }
	virtual bool Update(float percentage);

	// Required by Qt system. TODO: Make sure it is fine
	inline void* operator new(size_t, void* ptr) noexcept
	{
		return ptr;
	}

	using Assimp::ProgressHandler::operator new;

private:
	const bool* _cancelFlag = nullptr;

signals:
	void fileReadProcessed(float percent);
};

enum class UVMethod
{
	None,
	Planar,
	Cylindrical,
	Spherical,
	AngleBased,
	Hybrid,
	AngleBasedSmartUV
};

struct SceneMeshInfo
{
	int totalVertices = 0;
	int totalTriangles = 0;
	int meshCount = 0;
	std::string largestMeshName;
	int largestMeshTriangles = 0;
	BoundingBox boundingBox;
	float maxDimension = 0.0f;
	float minDimension = 0.0f;
};


struct AssImpMeshData
{
	QString name;
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<GLMaterial::Texture> textures;
	GLMaterial material;
	GLenum primitiveMode = GL_TRIANGLES;
	bool hasNegativeScale = false;
	// Index of this mesh in aiScene::mMeshes[] at load time.
	// Preserved so the exporter can match surviving meshes back to aiMesh
	// entries without relying on name strings.
	int sceneIndex = -1;
	// Material index from aiMesh::mMaterialIndex at import time.
	// This is the authoritative mapping of which material applies to this mesh,
	// avoiding fragile name-based matching during export.
	int originalMaterialIndex = -1;

	// Absolute path of the file this mesh was loaded from.
	QString sourceFile;
	QString sourceNodeName;
	bool preserveNodeTransform = false;

	// KHR_materials_variants: which material index maps to which variants.
	// Empty when the source file has no variant extension.
	QVector<GltfVariantMapping> variantMappings;

	// Pre-built GLMaterial for every material index referenced by variants
	// (including the default material at key originalMaterialIndex).
	// Populated during processMesh() so variant switching requires no I/O.
	QMap<int, GLMaterial> allVariantMaterials;

	// Skinning support for animated glTF meshes.
	QVector<GltfSkinJoint> skinJoints;
	QVector<MorphTargetData> morphTargets;
	QVector<float> defaultMorphWeights;
};

using AssImpMeshDataBatch = std::vector<AssImpMeshData>;

Q_DECLARE_METATYPE(AssImpMeshData)
Q_DECLARE_METATYPE(AssImpMeshDataBatch)

class AssImpModelLoader : public QObject
{
	Q_OBJECT
public:
	/*  Functions   */
	using UVDecisionFn = std::function<UVMethod(int totalTriangles, UVMethod currentMethod)>;

	// Constructor, expects a filepath to a 3D model.
	AssImpModelLoader();

	~AssImpModelLoader();

	/*  Functions   */
	// Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
	void loadModel(std::string path, const bool& progressiveLoading = false);

	AssImpMeshDataBatch getMeshes() const;

	glm::mat4 getGlobalSceneTransform() const { return _appliedTransform; }

	QString getErrorMessage() const;

	void setUVGenerationMethod(const UVMethod& uvMethod) { _selectedUVMethod = uvMethod; }
	UVMethod getUVGenerationMethod() const { return _selectedUVMethod; }

	bool regenerateUVs(AssImpMesh* mesh, UVMethod method, const UVConfig& config);

	// Auto scale and orient the model to fit the scene's coordinate system
	void setAutoScaleActive(bool autoScale) { _autoScale = autoScale; }
	void setAutoOrientActive(bool autoOrient) { _autoOrient = autoOrient; }
	bool isAutoScaleActive(bool autoScale) const { return _autoScale; }
	bool isAutoOrientActive(bool autoOrient) const { return _autoOrient; }

	const aiScene* getScene() const { return _scene; }

	// Returns variant data parsed from KHR_materials_variants.
	// Empty when the loaded file has no variant extension.
	const GltfVariantData&    getVariantData()  const { return _variantData; }
	const GltfAnimationData&  getAnimationData() const { return _animationData; }
	const GltfCameraData&     getCameraData()    const { return _cameraData; }

	void freeScene();
	void setImageTextureUploader(MaterialProcessor::ImageTextureUploadFn uploader);
	void setKtx2TextureUploader(MaterialProcessor::Ktx2TextureUploadFn uploader);
	void setUVDecisionCallback(UVDecisionFn callback) { _uvDecisionCallback = std::move(callback); }

signals:
	void fileReadProcessed(float percent);
	void verticesProcessed(float percent);
	void nodeMeshProgressUpdated(int processedNodes, int totalNodes, int processedMeshes, int totalMeshes, bool uvProcessed);
	void meshBatchReady(AssImpMeshDataBatch batch);
	void loadingFinished(bool successFlag, const aiScene* scene);
	void loadingCancelled();
	void lightsLoaded(const std::vector<GPULight>& lights);

public slots:
	void processFileReadProgress(float percentage);
	void cancelLoading();

private:
	// Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
	void processNode(int nodeNum, aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform);

	AssImpMeshData processMesh(aiMesh* mesh, const aiScene* scene, const int& meshIndex, const int& totalMeshes, const aiMatrix4x4& transform, const char* nodeName);

	int countNodes(const aiNode* node) const;

	void generateUVsForMesh(MeshAnalysis::AnalysisResult& analysis, aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<std::seed_seq::result_type>& indices);

	void GenerateFaceNormals(aiMesh* mesh, std::vector<glm::vec3>& generatedNormals);
	bool HasSurfaceGeometry(aiMesh* mesh);


	static SceneMeshInfo collectSceneMeshInfo(const aiScene* scene);
	
	// Automatic orientation and scaling of the model to fit the scene's coordinate system.
	void applyCoordinateSystemTransformations(const std::string& filePath);
	void applyTransformToNode(aiNode* node, const glm::mat4& transform);
	glm::mat4 getCoordinateSystemTransform(const aiScene* scene, const std::string& filePath);
	glm::mat4 getCoordinateSystemFromFileType(const std::string& fileExtension);
	float calculateConditionalScale(const float& minDimension, const float& maxDimension);
	// Parse glTF primitive modes and store them in the map
	void parseGltfPrimitiveModes(const QString& gltfPath);

	// Parse KHR_materials_variants extension and populate _variantData.
	// Must be called after updateAiSceneWithGltfMaterials() so material indices
	// in the JSON correspond 1:1 to aiScene::mMaterials[] entries.
	void parseGltfVariants(const QString& gltfPath);
	void parseSceneAnimations();
	void parseSceneCameras();

	// Update aiScene materials to match glTF structure (deduplicate, fix material assignments)
	void updateAiSceneWithGltfMaterials(const QString& gltfPath, aiScene* scene);

private:
	std::string _path;
	/*  Model Data  */
	AssImpMeshDataBatch _meshes;
	std::string _texturePath;

	Assimp::Importer _importer;
	AssImpModelProgressHandler* _progHandler;
	QString _errorMessage;
	bool _loadingCancelled;
	
	AssImpMeshDataBatch _currentBatch;
	int _batchSize = 20;

	const aiScene* _scene = nullptr; // Holds the loaded scene
	
	SceneMeshInfo _sceneStats; // Holds statistics about the scene
	int _totalNodeCount = 0;
	int _processedNodeCount = 0;
	int _processedMeshCount = 0;

	bool _progressiveLoading = false; // If true, emit progress signals during loading

	MaterialProcessor _materialProcessor; 

	UVMethod _selectedUVMethod = UVMethod::Hybrid;
	bool _needsUVGeneration = false;

	bool _autoScale = true; // Automatically scale the model to fit the scene's coordinate system
	bool _autoOrient = true; // Automatically orient the model to match the scene's coordinate system
	float _appliedScale = 1.0f;
	glm::mat4 _appliedTransform = glm::mat4(1.0f);

	// Map from mesh index to primitive mode (from glTF)
	std::unordered_map<unsigned int, GLenum> _gltfMeshPrimitiveModes;
	UVDecisionFn _uvDecisionCallback;

	// Map from aiMesh index to original material index (before deduplication)
	// Used to preserve true glTF material indices for correct export matching
	std::map<int, int> _meshIndexToOriginalMaterialIndex;

	// Map from Assimp compact material index → glTF material index.
	// Assimp loads materials in DFS traversal order (not glTF index order).
	// updateAiSceneWithGltfMaterials() builds this so processMesh() can convert
	// _meshIndexToOriginalMaterialIndex values (compact) into glTF-space indices
	// that are consistent with variant mapping materialIndex values.
	// Map from Assimp compact material index to glTF material index.
	// updateAiSceneWithGltfMaterials() rebuilds this from glTF primitive
	// provenance so processMesh() can convert pre-remap compact material slots
	// into authoritative glTF material indices. This keeps default materials,
	// variants, and export logic in the same index space even when Assimp merges
	// or reorders primitives.
	std::map<int, int> _aiMatToGltfMat;

	// Variant data parsed from KHR_materials_variants (empty for non-glTF files
	// or glTF files that do not carry the extension).
	GltfVariantData   _variantData;
	GltfAnimationData _animationData;
	GltfCameraData    _cameraData;
};
