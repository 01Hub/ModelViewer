#include "XCAFDocProcessor.hxx"
#include <map>

// Traverse the STEP assembly structure and extract shapes and names
void XCAFDocProcessor::traverseXCAFAssembly(
	const Handle(XCAFDoc_ShapeTool)& shapeTool,
	const Handle(XCAFDoc_ColorTool)& colorTool,
	const TDF_Label& label,
	const TopLoc_Location& parentLoc,
	std::vector<TopoDS_Shape>& outShapes,
	std::vector<Quantity_Color>& outColors,
	std::vector<std::string>& outNames)
{
	// 1) Assembly?  Recurse its components (cycle-safe)
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
		// an instance of an assemblyrecurse into its real children
		TDF_LabelSequence comps;
		shapeTool->GetComponents(defLabel, comps);
		for (Standard_Integer i = 1; i <= comps.Length(); ++i)
		{
			traverseXCAFAssembly(shapeTool, colorTool, comps.Value(i), loc, outShapes, outColors, outNames);
		}
		return;
	}

	// 5) Now defLabel must be a true leaf part definition-grab its shape
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
bool XCAFDocProcessor::GetShapeColorFromShape(
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
