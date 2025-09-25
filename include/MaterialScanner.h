// MaterialScanner.h
#pragma once

#include <QMap>
#include <QString>
#include <QComboBox>

using TextureMap = QMap<QString, QString>;         // key: "albedo","metallic","normal","ao" -> full path
using MaterialsMap = QMap<QString, TextureMap>;    // key: material folder name -> TextureMap

class MaterialScanner
{
public:
    // Scan rootFolder and return map of materials
    static MaterialsMap parseMaterialsFolder(const QString& rootFolder);

    // Populate combo box with keys of map. Keeps the same ordering as returned by parseMaterialsFolder.
    static void populateComboWithMaterials(QComboBox* combo, const MaterialsMap& materials);
};
