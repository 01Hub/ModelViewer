#include "BRepToAssimpConverter.h"
#include "MainWindow.h"
#include "XCAFSTEPProcessor.hxx"
#include <assimp/scene.h>
#include <BinDrivers.hxx>
#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <DE_ShapeFixParameters.hxx>
#include <IMeshTools_Parameters.hxx>
#include <QFileInfo>
#include <QString>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Tool.hxx>
#include <TDocStd_Document.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFReadProgressIndicator.hxx>

aiScene* XCAFSTEPProcessor::processFile(const std::string& path)
{
	XCAFDocProcessor::initializeDocumentProcessing();
	return processSTEPFile(path);
}

aiScene* XCAFSTEPProcessor::processSTEPFile(const std::string& path)
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
		if (MainWindow::isFileLoadCancelRequested())
		{
			return nullptr;
		}

		qWarning("Failed to read STEP file: %s", e.what());
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
		qWarning("No shapes found in STEP file");
		return nullptr;
	}

	// Pre-tessellate all free shapes in parallel using a single BRepMesh_IncrementalMesh call.
	// Collecting them into one compound lets OCCT distribute face meshing across all available
	// CPU cores (InParallel = true), which is significantly faster than the per-face sequential
	// meshing that convertFaceGroupToMesh() would otherwise do.
	// Deflection = 0.05 with Relative = true means 5 % of each face's bounding-box diagonal,
	// matching the adaptive quality used by convertFaceGroupToMesh().
	{
		MainWindow::showStatusMessage(tr("Pre-tessellating geometry (parallel)..."));

		TopoDS_Compound compound;
		BRep_Builder builder;
		builder.MakeCompound(compound);

		for (Standard_Integer i = 1; i <= labels.Length(); ++i)
		{
			TopoDS_Shape shape;
			if (shapeTool->GetShape(labels.Value(i), shape) && !shape.IsNull())
				builder.Add(compound, shape);
		}

		IMeshTools_Parameters meshParams;
		meshParams.Deflection             = BRepToAssimpConverter::resolveDeflectionFraction(); // user-configurable, default 5 %
		meshParams.Angle                  = 0.3;    // angular deflection in radians
		meshParams.Relative               = true;   // deflection is relative to each face's bbox
		meshParams.InParallel             = true;   // use all available CPU cores
		meshParams.AllowQualityDecrease   = true;   // avoid stalling on difficult faces

		BRepMesh_IncrementalMesh(compound, meshParams);
	}

#ifdef __DEBUG__
	auto startTraverse = std::chrono::high_resolution_clock::now();
#endif

	// Get the document name to set the root node name using file information
	QFileInfo fi(QString::fromStdString(path));

	// Create the main Assimp scene
	aiScene* scene = new aiScene();
	scene->mRootNode = new aiNode();
	std::string docName = fi.baseName().toStdString();
	std::string rootNodeName = docName + "_STEP";
	scene->mRootNode->mName = aiString(rootNodeName.c_str());

	int meshIndex = 0; // Tracks the mesh indices for the scene

	// Count total leaf parts across ALL free shape labels (the original code only counted
	// labels.Value(1), which gave a wrong total when a file has multiple top-level shapes).
	int totalMeshes = 0;
	for (Standard_Integer i = 1; i <= labels.Length(); ++i)
		totalMeshes += countMeshes(shapeTool, labels.Value(i));
	int processedMeshes = 0;

	// Pre-allocate the root node's children array to the exact number of free shape
	// labels so traverseXCAFAssembly can use direct index assignment instead of
	// calling realloc() for every top-level child it attaches.
	scene->mRootNode->mChildren    = labels.Length() > 0 ? new aiNode*[labels.Length()] : nullptr;
	scene->mRootNode->mNumChildren = 0;

	MainWindow::showStatusMessage(tr("Traversing assembly and building scene..."));

	for (Standard_Integer i = 1; i <= labels.Length(); ++i)
	{
		traverseXCAFAssembly(shapeTool, colorTool, labels.Value(i), TopLoc_Location(), scene->mRootNode, scene, meshIndex, processedMeshes, totalMeshes);
	}

#ifdef __DEBUG__
	auto endTraverse = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationTraverse = endTraverse - startTraverse;
	std::cout << "TraverseXCAFAssembly and scene creation took " << durationTraverse.count() << " seconds\n";
#endif

	// Finalize the scene
	if (scene->mNumMeshes == 0)
	{
		qWarning("No meshes were generated during scene creation");
		delete scene;
		return nullptr;
	}

#ifdef __DEBUG__
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

// Read s STEP file
void XCAFSTEPProcessor::readSTEPFile(const std::string & filename, Handle(TDocStd_Document) & doc)
{
	STEPCAFControl_Reader reader;

	// Disable XCAF sub-systems that are irrelevant for pure 3-D visualization.
	// Each disabled mode skips a separate traversal/annotation pass during Transfer(),
	// which can meaningfully reduce load time on large assemblies.
	reader.SetGDTMode(false);   // Geometric Dimensioning & Tolerancing annotations
	reader.SetSHUOMode(false);  // Super-imposed Higher-Usage Occurrence relationships
	reader.SetPropsMode(false); // Validation properties (mass, surface area, etc.)
	reader.SetViewMode(false);  // Saved camera views embedded in the STEP file

	// Phase D: Tune the OCCT 7.9.x shape-fix pipeline via DE_ShapeFixParameters.
	// By default OCCT runs FixShellOrientationMode = FixOrNot, meaning it will attempt
	// to reconcile shell normal orientations on every imported solid.  For well-authored
	// STEP files from professional CAD tools this is unnecessary overhead; we disable it
	// explicitly.  Other expensive topology-repair passes that have no visual benefit
	// for a viewer (self-intersection detection, face splitting) are also suppressed.
	// All other modes are left at their OCCT defaults so legitimate healing still runs.
	{
		using FM = DE_ShapeFixParameters::FixMode;
		DE_ShapeFixParameters fixParams;
		fixParams.FixShellOrientationMode  = FM::NotFix; // skip expensive shell normal reconciliation
		fixParams.FixSplitFaceMode         = FM::NotFix; // skip topology splitting (viewer does not need it)
		fixParams.FixSelfIntersectionMode  = FM::NotFix; // skip costly self-intersection detection
		reader.SetShapeFixParameters(fixParams);
	}

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
		if (MainWindow::isFileLoadCancelRequested())
		{
			throw std::runtime_error("Model loading cancelled by user.");
		}
		throw std::runtime_error("Cannot read STEP file");
	}
	MainWindow::resetProgressBar();

#ifdef __DEBUG__
	auto endCount = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationCount = endCount - startCount;
	std::cout << "ReadFile took " << durationCount.count() << " seconds\n";


	auto startTraverse = std::chrono::high_resolution_clock::now();
#endif

	MainWindow::showStatusMessage(tr("Transfering shapes..."));

	if (!reader.Transfer(doc, transferScope.Next()))
	{
		if (MainWindow::isFileLoadCancelRequested())
		{
			throw std::runtime_error("Model loading cancelled by user.");
		}
		throw std::runtime_error("Cannot transfer STEP data to XCAF document");
	}
#ifdef __DEBUG__
	auto endTraverse = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> durationTraverse = endTraverse - startTraverse;
	std::cout << "\nTransfer took " << durationTraverse.count() << " seconds\n";
#endif
}

