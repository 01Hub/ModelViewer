#include "ModelViewerApplication.h"
#include <assimp/Importer.hpp>

QStringList ModelViewerApplication::_supportedExtensions;

ModelViewerApplication::ModelViewerApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // Optionally initialize global state, logging, etc.
}

QStringList ModelViewerApplication::supportedImportExtensions()
{
    if(_supportedExtensions.isEmpty()) {
		initializeSupportedImportExtensions();
	}
	return _supportedExtensions;
}

void ModelViewerApplication::initializeSupportedImportExtensions()
{
	// 1. Get supported extensions from Assimp
	Assimp::Importer importer;
	std::string extList;
	importer.GetExtensionList(extList); // E.g. "*.obj;*.3ds;*.fbx;..."
	QString allExtensions = QString::fromStdString(extList).replace(';', ' ');

	// 2. Manually add STEP/IGES extensions (if not in Assimp list)
	if (!allExtensions.contains("*.step", Qt::CaseInsensitive)) {
		allExtensions += " *.step *.stp";
	}
	if (!allExtensions.contains("*.iges", Qt::CaseInsensitive)) {
		allExtensions += " *.iges *.igs";
	}
	if (!allExtensions.contains("*.brep", Qt::CaseInsensitive))
	{
		allExtensions += " *.brep *.rle";
	}

	// 3. All Supported filter
	QString allSupportedFilter = QString("All Supported Files (%1)").arg(allExtensions.trimmed());

	// 4. Common filters list
	QStringList commonFilters = {
		"Wavefront OBJ (*.obj)",
		"Autodesk 3DS (*.3ds)",
		"Collada DAE (*.dae)",
		"STL (*.stl)",
		"FBX (*.fbx)",
		"PLY (*.ply)",
		"DXF (*.dxf)",
		"GLTF (*.gltf *.glb)",
		"STEP (*.step *.stp)",
		"IGES (*.iges *.igs)",
		"BREP (*.brep *.rle)",
		"IFC (*.ifc)",
		"OFF (*.off)",
		"LWO (*.lwo *.lws)",
		"AC3D (*.ac *.ac3d *.acc)",
		"Blender (*.blend)",
		"Irrlicht (*.irr *.irrmesh)",
		"MD5 (*.md5mesh *.md5anim *.md5camera)"
	};

	// 5. Add all filters to dialog	
	_supportedExtensions << allSupportedFilter << commonFilters;
}
