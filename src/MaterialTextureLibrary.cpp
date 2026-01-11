#include "MaterialTextureLibrary.h"
#include "PathUtils.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

MaterialTextureLibrary& MaterialTextureLibrary::instance()
{
    static MaterialTextureLibrary inst;
    return inst;
}

MaterialTextureLibrary::MaterialTextureLibrary()
{
    QString appPath = PathUtils::getDataDirectory();
    QString root = appPath + "/textures/materials/";

    if (!QDir(root).exists())
    {
        qWarning() << "MaterialTextureLibrary: material root folder not found:" << root;
    }

    _materials = MaterialScanner::parseMaterialsFolder(root);
}

void MaterialTextureLibrary::reload(const QString& rootFolder)
{
    QMutexLocker locker(&_mutex);
    _materials.clear();
    _materials = MaterialScanner::parseMaterialsFolder(rootFolder);
    qDebug() << "MaterialTextureLibrary reloaded from" << rootFolder
        << "with" << _materials.size() << "materials";
}
