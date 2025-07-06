#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "AssImpMesh.h"
#include "TriangleMesh.h"
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
#include "MaterialProcessor.h"
#include "UVPromptDialog.h"


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
	Hybrid,
	AngleBasedSmartUV
};

struct UVDialogResult
{
	UVMethod method = UVMethod::None;	
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
	void loadModel(std::string path);

	std::vector<AssImpMesh*> getMeshes() const;

	QString getErrorMessage() const;

signals:
	void fileReadProcessed(float percent);
	void verticesProcessed(float percent);
	void nodeProcessed(int nodeNum, int totalNodes);
	void loadingCancelled();

public slots:
	void processFileReadProgress(float percentage);
	void cancelLoading();

private:	
	//using ShapeWithNameAndTrsf = std::tuple<TopoDS_Shape, std::string, gp_Trsf>;
	using ShapeWithNameAndTrsf = std::tuple<TopoDS_Shape, std::string, TopLoc_Location, Quantity_Color>;

	aiScene* processIGESFile(const std::string& path);
	aiScene* processSTEPFile(const std::string& path);
	aiScene* processBREPFile(const std::string& path);
	
	void readSTEPFile(const std::string& filename, Handle(TDocStd_Document)& doc);
	void traverseXCAFAssembly(
		const Handle(XCAFDoc_ShapeTool)& shapeTool,
		const Handle(XCAFDoc_ColorTool)& colorTool,
		const TDF_Label& label,
		const TopLoc_Location& parentLoc,
		std::vector<TopoDS_Shape>& outShapes,
		std::vector<Quantity_Color>& outColors,
		std::vector<std::string>& outNames);
	bool GetShapeColorFromShape(
		const Handle(XCAFDoc_ColorTool)& colorTool,
		const TopoDS_Shape& shape,
		Quantity_Color& outColor);

	// Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
	void processNode(int nodeNum, aiNode* node, const aiScene* scene);

	AssImpMesh* processMesh(aiMesh* mesh, const aiScene* scene);
	
	// For UV generation dialog user selection
	UVDialogResult askUserForUVMethod(const SceneUVPromptInfo& info, QWidget* parent);

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

	MaterialProcessor _materialProcessor; 

	UVMethod _selectedUVMethod = UVMethod::None;	
};
