#include "XCAFIGESProcessor.hxx"

#include "MainWindow.h"
#include "BRepToAssimpConverter.h"
#include "XCAFApp_Application.hxx"
#include "XCAFReadProgressIndicator.hxx"
#include "IGESCAFControl_Reader.hxx"
#include "IGESControl_Controller.hxx"
#include "TDocStd_Document.hxx"
#include "TDF_LabelSequence.hxx"
#include "TDF_Tool.hxx"
#include "BinDrivers.hxx"
#include <QString>


#include <assimp/scene.h>

aiScene* XCAFIGESProcessor::processFile(const std::string& path)
{
	return processIGESFile(path);
}

aiScene* XCAFIGESProcessor::processIGESFile(const std::string& path)
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

	MainWindow::showStatusMessage(tr("Transfering shapes..."));

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

	MainWindow::showStatusMessage(tr("Traversing assembly..."));

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
	MainWindow::showStatusMessage(tr("Converting shapes to mesh..."));
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
