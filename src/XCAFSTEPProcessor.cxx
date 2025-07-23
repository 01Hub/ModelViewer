#include "XCAFSTEPProcessor.hxx"
#include "MainWindow.h"
#include "BRepToAssimpConverter.h"
#include <assimp/scene.h>
#include <QString>
#include <XCAFApp_Application.hxx>
#include <XCAFReadProgressIndicator.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDocStd_Document.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDF_Tool.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <BinDrivers.hxx>

aiScene* XCAFSTEPProcessor::processFile(const std::string& path)
{
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

#ifdef __DEBUG__
	auto startTraverse = std::chrono::high_resolution_clock::now();
#endif
	MainWindow::showStatusMessage(tr("Traversing assembly and building scene..."));

	// Create the main Assimp scene
	aiScene* scene = new aiScene();
	scene->mRootNode = new aiNode();
	scene->mRootNode->mName = aiString("Root");

	int meshIndex = 0; // Tracks the mesh indices for the scene

	// Traverse the assembly and build the Assimp scene hierarchy
	int totalMeshes = countMeshes(shapeTool, labels.Value(1)); // Assuming labels contains free shapes
	int processedMeshes = 0;

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
		qCritical("No meshes were generated during scene creation");
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

	MainWindow::showStatusMessage(tr("Transfering shapes..."));

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

