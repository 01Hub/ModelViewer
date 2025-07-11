#include "AssImpModelLoader.h"
#include "BRepToAssimpConverter.h"
#include "MainWindow.h"
#include "ModelViewer.h"
#include "MeshAnalyzer.h"
#include "TangentGenerator.h"
#include "Utils.h"
#include "UVGenerator.h"
#include "XCAFReadProgressIndicator.hxx"
#include <BinDrivers.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <IGESCAFControl_Reader.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Reader.hxx>
#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLayout>
#include <Quantity_ColorRGBA.hxx>
#include <STEPControl_Reader.hxx>
#include <TopoDS_Iterator.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc.hxx>
#include <XSControl_WorkSession.hxx>

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

	if (fi.suffix().toLower() == "step" || fi.suffix().toLower() == "stp")
	{
		_scene = processSTEPFile(path);
	}
	else if (fi.suffix().toLower() == "iges" || fi.suffix().toLower() == "igs")
    {
		_scene = processIGESFile(path);
	}
	else if (fi.suffix().toLower() == "brep" || fi.suffix().toLower() == "rle")
	{
		_scene = processBREPFile(path);
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

	SceneMeshInfo stats = collectSceneMeshInfo(_scene);

	if (modelHasMissingUVs)
	{								
		QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());				
		bool remember = settings.value("RememberUVMethod", false).toBool();		
		if (stats.totalTriangles > 100000 && _selectedUVMethod == UVMethod::AngleBasedSmartUV && remember)
		{
			QMessageBox msgBox;
			msgBox.setWindowTitle("Performance Warning!");
			msgBox.setText("The model contains more than 100000 triangles and the current method of UV generation is \"Smart UV\" which is time consuming.\nDo you want to continue generating the UV?");
			msgBox.setIcon(QMessageBox::Question);

			// Add custom buttons
			QPushButton* yesButton = msgBox.addButton(QMessageBox::Yes);
			QPushButton* noButton = msgBox.addButton(QMessageBox::No);
			QPushButton* changeSettingsButton = msgBox.addButton("Change Settings", QMessageBox::ActionRole);

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
	int batchSize = std::clamp(stats.meshCount / 10, 5, 100);
	_batchSize = batchSize;
	qDebug() << "Batch size = " << _batchSize;

	// Process ASSIMP's root node recursively
	int nodeNum = 0;
	this->processNode(nodeNum, _scene->mRootNode, _scene);

	// Flush any remaining meshes in batch
	if (!_currentBatch.empty())
	{
		emit meshBatchReady(std::move(_currentBatch));
		_currentBatch.clear();
	}

	//emit nodeProcessed(nodeNum, _scene->mNumMeshes);

	if (_progressiveLoading)
		emit loadingFinished(true, _scene);
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
		traverseXCAFAssembly(shapeTool, colorTool, labels.Value(i), TopLoc_Location(), shapes, colors, names);
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

aiScene* AssImpModelLoader::processBREPFile(const std::string& path)
{
	// Create XCAF Application and document
	Handle(TDocStd_Document) doc;
	Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
	BinDrivers::DefineFormat(app);
	app->NewDocument("BinOcaf", doc);

	MainWindow::showIndeterminateProgressBar();

	try
	{
		// Read BREP file directly into a shape
		TopoDS_Shape shape;
		BRep_Builder builder;

		if (!BRepTools::Read(shape, path.c_str(), builder))
		{
			qCritical("Failed to read BREP file: %s", path.c_str());
			MainWindow::resetProgressBar();
			app->Close(doc);
			return nullptr;
		}

		// Check if shape is valid
		if (shape.IsNull())
		{
			qCritical("Read BREP file but shape is null: %s", path.c_str());
			MainWindow::resetProgressBar();
			app->Close(doc);
			return nullptr;
		}

		MainWindow::resetProgressBar();
		MainWindow::showStatusMessage("Transferring shapes...");

		// Access shape tool and color tool - ensure they're properly initialized
		Handle(XCAFDoc_ShapeTool) shapeTool = XCAFDoc_DocumentTool::ShapeTool(doc->Main());
		Handle(XCAFDoc_ColorTool) colorTool = XCAFDoc_DocumentTool::ColorTool(doc->Main());

		// Verify tools are valid
		if (shapeTool.IsNull() || colorTool.IsNull())
		{
			qCritical("Failed to initialize XCAF tools for BREP file: %s", path.c_str());
			MainWindow::resetProgressBar();
			app->Close(doc);
			return nullptr;
		}

		// Add the shape to XCAF document
		TDF_Label rootLabel = shapeTool->AddShape(shape, Standard_False);

		// Verify the root label is valid
		if (rootLabel.IsNull())
		{
			qCritical("Failed to add shape to XCAF document: %s", path.c_str());
			MainWindow::resetProgressBar();
			app->Close(doc);
			return nullptr;
		}			
		
		// Single model mode - treat entire shape as one unit
		MainWindow::showStatusMessage("Loading as single model...");

		// Set name and color for the single shape
		try
		{
			Handle(TDataStd_Name) nameAttr = TDataStd_Name::Set(rootLabel,
				TCollection_ExtendedString("BREPModel"));

			if (!colorTool->IsSet(rootLabel, XCAFDoc_ColorGen))
			{
				Quantity_Color defaultColor(0.7, 0.7, 0.7, Quantity_TOC_RGB);
				colorTool->SetColor(rootLabel, defaultColor, XCAFDoc_ColorGen);
			}
		}
		catch (const Standard_Failure& e)
		{
			qWarning("Warning: Could not set name/color for shape: %s", e.GetMessageString());
		}

		// Create single shape tuple
		std::vector<ShapeWithNameAndTrsf> shapeTuples;
		shapeTuples.emplace_back(shape, "BREPModel", TopLoc_Location(),
			Quantity_Color(0.7, 0.7, 0.7, Quantity_TOC_RGB));

		// Convert to Assimp scene
		MainWindow::showStatusMessage("Converting shape to mesh...");
		return BRepToAssimpConverter::convert(shapeTuples);
		

#ifdef __DEBUG__
		QFileInfo fi(QString::fromStdString(path));
		QString savePath = "/home/sharjith/" + fi.baseName() + ".cbf";
		PCDM_StoreStatus sstatus = app->SaveAs(doc, TCollection_ExtendedString(savePath.toStdString().c_str()));
		if (sstatus != PCDM_SS_OK)
		{
			qDebug() << "Error saving the document";
		}
#endif

		app->Close(doc);
	}
	catch (const Standard_Failure& e)
	{
		qCritical("OpenCASCADE exception while reading BREP file: %s", e.GetMessageString());
		MainWindow::resetProgressBar();
		app->Close(doc);
		return nullptr;
	}
	catch (...)
	{
		qCritical("Unknown exception while reading BREP file: %s", path.c_str());
		MainWindow::resetProgressBar();
		app->Close(doc);
		return nullptr;
	}
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

	Handle(XCAFReadProgressIndicator) progress = new XCAFReadProgressIndicator();
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
		traverseXCAFAssembly(shapeTool, colorTool, labels.Value(i), TopLoc_Location(), shapes, colors, names);
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

	Handle(XCAFReadProgressIndicator) progress = new XCAFReadProgressIndicator();
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
void AssImpModelLoader::traverseXCAFAssembly(
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
			traverseXCAFAssembly(shapeTool, colorTool, comps.Value(i), parentLoc, outShapes, outColors, outNames);
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
			traverseXCAFAssembly(shapeTool, colorTool, comps.Value(i), loc, outShapes, outColors, outNames);
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
void AssImpModelLoader::processNode(int& nodeCounter, aiNode* node, const aiScene* scene)
{
	if (_loadingCancelled)
	{
		emit loadingCancelled();
		return;
	}

	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		AssImpMesh* myMesh = processMesh(mesh, scene, i, scene->mNumMeshes);

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
		processNode(nodeCounter, node->mChildren[i], scene);

		if (nodeCounter % 20 == 0)
		{
			emit nodeProcessed(nodeCounter, node->mNumChildren);
		}
	}
}



AssImpMesh* AssImpModelLoader::processMesh(aiMesh* mesh, const aiScene* scene, const int& meshIndex, const int& totalMeshes)
{
	// Data to fill
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	vector<Texture> textures;
	
	bool needsUVGeneration = false;

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
			needsUVGeneration = true;
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
	if (needsUVGeneration && _selectedUVMethod != UVMethod::None)
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

		// ADS Maps
		_materialProcessor.setADSTextureMaps(material, textures);

		// PBR Maps
		_materialProcessor.setPBRTextureMaps(material, textures);

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

	if (!scene || !scene->HasMeshes())
		return info;

	info.meshCount = static_cast<int>(scene->mNumMeshes);

	for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		int numFaces = static_cast<int>(mesh->mNumFaces);
		int numVerts = static_cast<int>(mesh->mNumVertices);

		info.totalVertices += numVerts;
		info.totalTriangles += numFaces;

		if (numFaces > info.largestMeshTriangles)
		{
			info.largestMeshTriangles = numFaces;
			info.largestMeshName = mesh->mName.C_Str();
		}
	}

	return info;
}
