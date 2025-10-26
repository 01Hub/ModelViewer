#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "AssImpMesh.h"
#include "BoundingBox.h"
#include "GLTFMetadataExtractor.h"
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


class AssImpModelProgressHandler : public QObject, public Assimp::ProgressHandler
{
	Q_OBJECT
public:
	virtual bool Update(float percentage);

	// Required by Qt system. TODO: Make sure it is fine
	inline void* operator new(size_t, void* ptr) noexcept
	{
		return ptr;
	}

	using Assimp::ProgressHandler::operator new;

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
};


class AssImpModelLoader : public QObject, public QOpenGLFunctions_4_5_Core
{
	Q_OBJECT
public:
	/*  Functions   */
	// Constructor, expects a filepath to a 3D model.
	AssImpModelLoader(QOpenGLShaderProgram* prog);

	~AssImpModelLoader();

	/*  Functions   */
	// Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
	void loadModel(std::string path, const bool& progressiveLoading = false);

	std::vector<AssImpMesh*> getMeshes() const;

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

	void freeScene();

signals:
	void fileReadProcessed(float percent);
	void verticesProcessed(float percent);
	void nodeProcessed(int nodeNum, int totalNodes, int totalMeshes, bool uvProcessed);
	void meshBatchReady(std::vector<AssImpMesh*> batch);
	void loadingFinished(bool successFlag, const aiScene* scene);
	void loadingCancelled();

public slots:
	void processFileReadProgress(float percentage);
	void cancelLoading();

private:
	// Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
	void processNode(int nodeNum, aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform);

	AssImpMesh* processMesh(aiMesh* mesh, const aiScene* scene, const int& meshIndex, const int& totalMeshes, const aiMatrix4x4& transform);

	void generateUVsForMesh(MeshAnalysis::AnalysisResult& analysis, aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<std::seed_seq::result_type>& indices);

	void GenerateFaceNormals(aiMesh* mesh, std::vector<glm::vec3>& generatedNormals);
	bool HasSurfaceGeometry(aiMesh* mesh);


	static SceneMeshInfo collectSceneMeshInfo(const aiScene* scene);
	static glm::mat4 aiMatrixToGlm(const aiMatrix4x4& from);
	static aiMatrix4x4 glmToAiMatrix(const glm::mat4& mat);

	// Automatic orientation and scaling of the model to fit the scene's coordinate system.
	void applyCoordinateSystemTransformations(const bool rotate, const bool scale, const std::string& filePath);
	void applyTransformToNode(aiNode* node, const glm::mat4& transform);
	glm::mat4 getCoordinateSystemTransform(const aiScene* scene, const std::string& filePath);
	glm::mat4 getCoordinateSystemFromFileType(const std::string& fileExtension);
	float calculateConditionalScale(const float& maxDimension);
		
private:
	QOpenGLShaderProgram* _prog;
	std::string _path;
	/*  Model Data  */
	std::vector<AssImpMesh*> _meshes;
	std::string _texturePath;

	Assimp::Importer _importer;
	AssImpModelProgressHandler* _progHandler;
	QString _errorMessage;
	bool _loadingCancelled;
	
	std::vector<AssImpMesh*> _currentBatch;
	int _batchSize = 20;

	const aiScene* _scene = nullptr; // Holds the loaded scene
	
	SceneMeshInfo _sceneStats; // Holds statistics about the scene

	bool _progressiveLoading = false; // If true, emit progress signals during loading

	MaterialProcessor _materialProcessor; 

	UVMethod _selectedUVMethod = UVMethod::Hybrid;
	bool _needsUVGeneration = false;

	bool _autoScale = true; // Automatically scale the model to fit the scene's coordinate system
	bool _autoOrient = true; // Automatically orient the model to match the scene's coordinate system

	GLTFMetadataExtractor _glTFMetadataExtractor;
	std::string _currentGLTFPath;  // Store path for material processor
};
