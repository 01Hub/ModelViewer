#include "TextureLocationManager.h"
#include "TriangleMesh.h"
#include "GLMaterial.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>
#include <set>
#include <map>

TextureLocationManager::TextureLocationManager()
{
}

TextureMetadata TextureLocationManager::resolveTexture(const QString& path,
    const QString& baseDir)
{
    TextureMetadata meta;
    meta.originalPath = path;

    if (path.isEmpty())
    {
        return meta;  // Return empty metadata for empty path
    }

    // Step 1: Try path as provided
    {
        QFileInfo fi(path);
        if (fi.exists() && fi.isFile() && fi.isReadable())
        {
            meta.resolvedPath = fi.absoluteFilePath();
            meta.fileSize = static_cast<uint64_t>(fi.size());
            meta.hash = hashFile(meta.resolvedPath);
            qDebug() << "[TextureLocationManager] Resolved (direct):" << path
                << "->" << meta.resolvedPath;
            return meta;
        }
    }

    // Step 2: Try relative to baseDir (if provided)
    if (!baseDir.isEmpty())
    {
        QString relPath = baseDir + "/" + path;
        QFileInfo fi(relPath);
        if (fi.exists() && fi.isFile() && fi.isReadable())
        {
            meta.resolvedPath = fi.absoluteFilePath();
            meta.fileSize = static_cast<uint64_t>(fi.size());
            meta.hash = hashFile(meta.resolvedPath);
            qDebug() << "[TextureLocationManager] Resolved (relative to baseDir):"
                << path << "->" << meta.resolvedPath;
            return meta;
        }
    }

    // Step 3: Try relative to current working directory
    {
        QString cwdPath = QDir::currentPath() + "/" + path;
        QFileInfo fi(cwdPath);
        if (fi.exists() && fi.isFile() && fi.isReadable())
        {
            meta.resolvedPath = fi.absoluteFilePath();
            meta.fileSize = static_cast<uint64_t>(fi.size());
            meta.hash = hashFile(meta.resolvedPath);
            qDebug() << "[TextureLocationManager] Resolved (relative to CWD):"
                << path << "->" << meta.resolvedPath;
            return meta;
        }
    }

    // Not found - log warning but continue
    qWarning() << "[TextureLocationManager] Texture not found:" << path;
    return meta;  // Return with empty resolvedPath
}

TexturePackage TextureLocationManager::packageTextures(
    const std::vector<TriangleMesh*>& meshes,
    const QString& outputDirectory)
{
    TexturePackage package;
    package.textureDirectory = outputDirectory + "/textures";

    // Create textures directory
    QDir texDir(package.textureDirectory);
    if (!texDir.mkpath("."))
    {
        qCritical() << "[TextureLocationManager] Failed to create texture directory:"
            << package.textureDirectory;
        return package;
    }

    qDebug() << "[TextureLocationManager] Packaging textures to:"
        << package.textureDirectory;

    std::set<QString> processedPaths;      // Track processed original paths
    std::map<QByteArray, QString> hashMap; // Hash -> output filename mapping

    // Collect all textures from all meshes
    for (const auto* mesh : meshes)
    {
        if (!mesh) continue;

        const auto& material = mesh->getMaterial();

        // Iterate through all texture types (25 total)
        for (int i = 0; i < static_cast<int>(GLMaterial::TextureType::Count); ++i)
        {
            auto type = static_cast<GLMaterial::TextureType>(i);
            const auto& tex = material.texture(type);

            if (tex.path.empty()) continue;

            QString texPath = QString::fromStdString(tex.path);

            // Skip if already processed
            if (processedPaths.count(texPath))
            {
                qDebug() << "[TextureLocationManager] Already processed:" << texPath;
                continue;
            }

            // Resolve the texture path
            TextureMetadata meta = resolveTexture(texPath);

            if (meta.resolvedPath.isEmpty())
            {
                qWarning() << "[TextureLocationManager] Could not resolve texture:" << texPath;
                continue;
            }

            // Check for duplicates by content hash
            if (hashMap.count(meta.hash))
            {
                // This is a duplicate - reuse existing output name
                meta.outputName = hashMap[meta.hash];
                meta.relativePath = "textures/" + meta.outputName;
                package.duplicatesRemoved++;

                qDebug() << "[TextureLocationManager] Duplicate texture detected:"
                    << QFileInfo(meta.resolvedPath).fileName()
                    << "-> reusing" << meta.outputName;
            }
            else
            {
                // New unique texture - copy it to output directory
                QString originalName = QFileInfo(meta.resolvedPath).fileName();
                meta.outputName = generateUniqueFilename(
                    package.textureDirectory,
                    originalName);

                QString destPath = package.textureDirectory + "/" + meta.outputName;

                if (QFile::copy(meta.resolvedPath, destPath))
                {
                    meta.relativePath = "textures/" + meta.outputName;
                    package.totalSize += meta.fileSize;
                    hashMap[meta.hash] = meta.outputName;

                    qDebug() << "[TextureLocationManager] Texture copied:"
                        << originalName << "->" << meta.outputName
                        << QString("(%1 MB)").arg(meta.fileSize / (1024.0 * 1024.0), 0, 'f', 2);
                }
                else
                {
                    qWarning() << "[TextureLocationManager] Failed to copy texture:"
                        << meta.resolvedPath << "->" << destPath;
                    continue;
                }
            }

            // Record the mapping for material updates
            package.pathMapping[texPath] = meta.relativePath;
            package.textures.push_back(meta);
            processedPaths.insert(texPath);
        }
    }

    qDebug() << "[TextureLocationManager] Packaging complete:"
        << package.textures.size() << "unique textures,"
        << package.duplicatesRemoved << "duplicates removed,"
        << QString("%1 MB").arg(package.totalSize / (1024.0 * 1024.0), 0, 'f', 2);

    return package;
}

bool TextureLocationManager::areFilesIdentical(const QString& path1,
    const QString& path2)
{
    QByteArray hash1 = hashFile(path1);
    QByteArray hash2 = hashFile(path2);

    if (hash1.isEmpty() || hash2.isEmpty())
    {
        return false;  // If we can't hash, assume different
    }

    return hash1 == hash2;
}

QByteArray TextureLocationManager::hashFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "[TextureLocationManager] Cannot open file for hashing:" << path;
        return QByteArray();
    }

    QCryptographicHash hasher(QCryptographicHash::Sha256);

    // Hash in chunks for memory efficiency (1 MB chunks)
    const int chunkSize = 1024 * 1024;
    QByteArray buffer;

    while (!file.atEnd())
    {
        buffer = file.read(chunkSize);
        if (buffer.isEmpty()) break;
        hasher.addData(buffer);
    }

    file.close();
    return hasher.result();
}

QString TextureLocationManager::generateUniqueFilename(
    const QString& directory,
    const QString& originalFilename)
{
    QFileInfo fi(originalFilename);
    QString baseName = fi.baseName();
    QString suffix = fi.suffix();

    // Check if original name is available
    QString candidate = originalFilename;
    if (!QFileInfo(directory + "/" + candidate).exists())
    {
        return candidate;
    }

    // Handle collision with suffix
    int counter = 1;
    while (counter <= 100000)
    {
        if (suffix.isEmpty())
        {
            candidate = QString("%1_%2").arg(baseName).arg(counter);
        }
        else
        {
            candidate = QString("%1_%2.%3")
                .arg(baseName)
                .arg(counter)
                .arg(suffix);
        }

        if (!QFileInfo(directory + "/" + candidate).exists())
        {
            qDebug() << "[TextureLocationManager] Generated unique filename for collision:"
                << originalFilename << "->" << candidate;
            return candidate;
        }

        counter++;
    }

    // Fallback - should never reach here
    qCritical() << "[TextureLocationManager] Cannot generate unique filename after 100000 attempts:"
        << originalFilename;
    return originalFilename + ".collision";
}
