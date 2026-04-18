#include "AssImpMeshExporter.h"
#include "TriangleMesh.h"
#include "AssImpMesh.h"
#include "GLMaterial.h"
#include "GltfPostProcessor.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QMatrix4x4>
#include <algorithm>
#include <memory>
#include <set>

AssImpMeshExporter::AssImpMeshExporter(QObject* parent)
    : QObject(parent)
{
}

aiReturn AssImpMeshExporter::exportMeshes(
    const aiScene* scene,
    const std::vector<TriangleMesh*>& meshes,
    const QString& exportPath,
    const ExportSettings& settings)
{
    _currentSettings = settings;
    _lastEmbeddedIndexMapping.clear();

    logMessage(QString("=== AssImpMeshExporter::exportMeshes ==="));
    logMessage(QString("Target: %1").arg(exportPath));
    logMessage(QString("Output directory: %1").arg(settings.outputDirectory));

    if (meshes.empty())
    {
        logError("No meshes to export");
        return aiReturn_FAILURE;
    }

    // ===== STEP 1: Package textures =====
    logMessage("Step 1: Packaging textures...");

    // Check if this is a GLB export (for cleanup later)
    QFileInfo exportFileInfo(exportPath);
    QString ext = exportFileInfo.suffix().toLower();
    bool isGLB = (ext == "glb" || ext == "gltf-binary");
    bool isGLTF = (ext == "gltf");
    QString textureSubfolder = exportFileInfo.baseName() + "_textures";
    QString textureBaseDir = isGLB ? QDir::tempPath() : settings.outputDirectory;

    if (settings.copyTextures)
    {
        // Extract embedded textures if source is GLB
        if ((isGLB || isGLTF) && scene && scene->mNumTextures > 0)
        {
            logMessage("  Extracting embedded textures from source GLB...");
            QMap<QString, QString> embeddedMapping = extractEmbeddedTextures(scene, textureBaseDir, textureSubfolder);

            // Inject the embedded texture mappings into texturePackage
            // This bypasses the resolveTexture mechanism for GLB textures
            for (auto it = embeddedMapping.begin(); it != embeddedMapping.end(); ++it)
            {
                _lastTexturePackage.pathMapping[it.key()] = it.value();
            }

            logMessage(QString("  -> Injected %1 embedded texture mappings").arg(embeddedMapping.size()));
        }

        logMessage("Step 1b: Packaging textures...");
        _lastTexturePackage = _textureManager.packageTextures(
            meshes,
            textureBaseDir,
            textureSubfolder);

        // Re-inject embedded mappings (in case packageTextures cleared the mapping)
        if (scene && scene->mNumTextures > 0)
        {
            QMap<QString, QString> embeddedMapping = extractEmbeddedTextures(scene, textureBaseDir, textureSubfolder);
            for (auto it = embeddedMapping.begin(); it != embeddedMapping.end(); ++it)
            {
                _lastTexturePackage.pathMapping[it.key()] = it.value();
            }
        }

        logMessage(QString("  -> Total texture mappings: %1").arg(_lastTexturePackage.pathMapping.size()));


        logMessage(QString("  -> Packaged %1 unique textures")
            .arg(_lastTexturePackage.textures.size()));

        if (_lastTexturePackage.totalSize > 0)
        {
            logMessage(QString("  -> Total texture size: %1 MB")
                .arg(_lastTexturePackage.totalSize / (1024.0 * 1024.0), 0, 'f', 2));
        }

        if (_lastTexturePackage.duplicatesRemoved > 0)
        {
            logMessage(QString("  -> Removed %1 duplicate textures")
                .arg(_lastTexturePackage.duplicatesRemoved));
        }

        if (isGLB)
        {
            logMessage("  -> Note: Textures will be embedded in GLB and folder will be cleaned up");
        }
    }
    else
    {
        logMessage("  -> Texture copying disabled");
    }

    // ===== STEP 2: Create Assimp structures =====
    logMessage("Step 2: Creating Assimp structures...");

    std::vector<aiMesh*> aiMeshes;
    std::vector<aiMaterial*> aiMaterials;
    std::vector<QMatrix4x4> transforms;

    for (const auto* mesh : meshes)
    {
        if (!mesh) continue;

        // Skip invisible meshes
        /*if (!mesh->isVisible())
        {
            logMessage(QString("  -> Skipping invisible mesh: %1").arg(mesh->getName()));
            continue;
        }*/

        // Extract vertex and index data
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        // Try to cast to AssImpMesh for direct access
        if (auto assimpMesh = dynamic_cast<const AssImpMesh*>(mesh))
        {
            vertices = assimpMesh->vertices();
            indices = assimpMesh->indices();
        }
        else
        {
            logWarning(QString("Non-AssImpMesh encountered: %1 - limited support")
                .arg(mesh->getName()));
            // Could implement fallback here if needed
            continue;
        }

        if (vertices.empty() || indices.empty())
        {
            logWarning(QString("  -> Skipping empty mesh: %1")
                .arg(mesh->getName()));
            continue;
        }

        // Create Assimp mesh
        aiMesh* aiMesh = createMesh(vertices, indices, mesh->getName().toStdString());
        if (!aiMesh)
        {
            logError(QString("Failed to create Assimp mesh: %1").arg(mesh->getName()));
            continue;
        }

        // Get material from mesh
        GLMaterial meshMaterial = mesh->getMaterial();

        // Create material
        aiMaterial* aiMat = createMaterial(meshMaterial, _lastTexturePackage, exportPath);
        if (!aiMat)
        {
            logError(QString("Failed to create material for: %1").arg(mesh->getName()));
            delete aiMesh;
            continue;
        }

        aiMesh->mMaterialIndex = static_cast<unsigned int>(aiMaterials.size());

        aiMeshes.push_back(aiMesh);
        aiMaterials.push_back(aiMat);
        transforms.push_back(mesh->getTransformation());

        logMessage(QString("  -> Mesh added: %1 (%2 vertices, %3 indices)")
            .arg(mesh->getName())
            .arg(vertices.size())
            .arg(indices.size()));
    }

    if (aiMeshes.empty())
    {
        logError("No valid meshes created");
        return aiReturn_FAILURE;
    }

    logMessage(QString("  -> Total: %1 meshes, %2 materials")
        .arg(aiMeshes.size())
        .arg(aiMaterials.size()));

    // ===== STEP 3: Create scene hierarchy =====
    logMessage("Step 3: Creating scene hierarchy...");

    std::unique_ptr<aiScene> newScene(createScene(aiMeshes, aiMaterials, transforms));
    if (!newScene)
    {
        logError("Failed to create Assimp scene");
        return aiReturn_FAILURE;
    }

    logMessage("  -> Scene hierarchy created");

    // ===== STEP 4: Export via Assimp =====
    logMessage("Step 4: Exporting to file...");

    Assimp::Exporter exporter;
    const aiExportFormatDesc* format = findExportFormat(exportPath.toStdString(), exporter);

    if (!format)
    {
        logError(QString("Unsupported export format: %1")
            .arg(QFileInfo(exportPath).suffix()));
        return aiReturn_FAILURE;
    }

    logMessage(QString("  -> Using format: %1 (%2)")
        .arg(QString::fromLocal8Bit(format->description))
        .arg(QString::fromLocal8Bit(format->id)));

    aiReturn result = exporter.Export(newScene.get(), format->id, exportPath.toStdString().c_str());

    if (result != aiReturn_SUCCESS)
    {
        logError(QString("Assimp export failed: %1")
            .arg(QString::fromLocal8Bit(exporter.GetErrorString())));
        return result;
    }

    logMessage(QString("Export successful!"));
    logMessage(QString("  -> File: %1").arg(exportPath));
    logMessage(QString("  -> Meshes: %1").arg(aiMeshes.size()));
    logMessage(QString("  -> Materials: %1").arg(aiMaterials.size()));
    logMessage(QString("  -> Textures: %1").arg(_lastTexturePackage.textures.size()));

    // ===== STEP 5: Post-process to add lights and fix materials =====
    logMessage("Step 5: Post-processing exported file...");

    auto logCallback = [this](const QString& msg) { logMessage(msg); };

    if (isGLB)
    {
        if (GltfPostProcessor::postProcessGlbFileWithMaterials(
            exportPath, meshes, settings.lights, logCallback, textureSubfolder, _lastTexturePackage.pathMapping, _lastEmbeddedIndexMapping))
            logMessage("  -> Post-processing complete");
        else
            logWarning("  -> Post-processing failed (file may still be valid)");
    }
    else if (QFileInfo(exportPath).suffix().toLower() == "gltf")
    {
        if (GltfPostProcessor::postProcessGltfFileWithMaterials(
            exportPath, meshes, settings.lights, logCallback, textureSubfolder, _lastTexturePackage.pathMapping, _lastEmbeddedIndexMapping))
            logMessage("  -> Post-processing complete");
        else
            logWarning("  -> Post-processing failed (file may still be valid)");
    }

    // ===== STEP 6: Cleanup temp texture folder for GLB exports =====
    if (isGLB && settings.copyTextures && !_lastTexturePackage.textures.empty())
    {
        logMessage("Step 5: Cleaning up temp texture folder (textures embedded in GLB)...");

        QDir dir(_lastTexturePackage.textureDirectory);
        if (dir.removeRecursively())
        {
            logMessage(QString("  -> Removed temp textures folder: %1")
                .arg(_lastTexturePackage.textureDirectory));
        }
        else
        {
            logWarning(QString("  -> Could not remove temp textures folder: %1")
                .arg(_lastTexturePackage.textureDirectory));
        }
    }

    return aiReturn_SUCCESS;
}

/**
 * Enhanced exportScene method with material application
 *
 * This method now accepts the original mesh objects, applies their materials
 * to the Assimp scene meshes, and then exports the enriched scene.
 *
 * @param scene The Assimp scene to export
 * @param meshes The original ModelViewer mesh objects containing materials and properties
 * @param exportPath The destination file path
 * @return Assimp export result code
 */
aiReturn AssImpMeshExporter::exportScene(
    aiScene* scene,
    const std::vector<TriangleMesh*>& meshes,
    const std::string& exportPath)
{
    // Default settings with embedding enabled
    ExportSettings settings;
    settings.copyTextures = true;
    settings.outputDirectory = ".";  // Won't be used for embedded export

    return exportScene(scene, meshes, exportPath, settings);
}

/**
 * Overload: exportScene with texture packaging
 *
 * This version also handles texture packaging alongside material application,
 * useful for self-contained exports.
 */
aiReturn AssImpMeshExporter::exportScene(
    aiScene* scene,
    const std::vector<TriangleMesh*>& meshes,
    const std::string& exportPath,
    const ExportSettings& settings)
{
    logMessage(QString("=== AssImpMeshExporter::exportScene (GLB with Embedded Textures) ==="));
    logMessage(QString("Target: %1").arg(QString::fromStdString(exportPath)));
    logMessage(QString("Output directory: %1").arg(settings.outputDirectory));

    if (!scene)
    {
        logError("Scene pointer is null");
        return aiReturn_FAILURE;
    }

    if (scene->mNumMeshes == 0)
    {
        logError("Scene contains no meshes");
        return aiReturn_FAILURE;
    }

    _currentSettings = settings;

    // Check if this is a GLB export (for embedding and cleanup)
    QString exportFilePath = QString::fromStdString(exportPath);
    QFileInfo exportFileInfo(exportFilePath);
    QString ext = exportFileInfo.suffix().toLower();
    bool isGLB = (ext == "glb" || ext == "gltf-binary");
    bool isGLTF = (ext == "gltf");
    QString textureSubfolder = exportFileInfo.baseName() + "_textures";
    QString textureBaseDir = isGLB ? QDir::tempPath() : settings.outputDirectory;

    // ===== STEP 1: Package textures =====
    if (settings.copyTextures && !meshes.empty())
    {
        logMessage("Step 1: Packaging textures...");

        // Extract embedded textures if source is GLB
        if ((isGLB || isGLTF) && scene && scene->mNumTextures > 0)
        {
            logMessage("  Extracting embedded textures from source GLB...");
            QMap<QString, QString> embeddedMapping = extractEmbeddedTextures(scene, textureBaseDir, textureSubfolder);

            // Inject the embedded texture mappings into texturePackage
            // This bypasses the resolveTexture mechanism for GLB textures
            for (auto it = embeddedMapping.begin(); it != embeddedMapping.end(); ++it)
            {
                _lastTexturePackage.pathMapping[it.key()] = it.value();
            }

            logMessage(QString("  -> Injected %1 embedded texture mappings").arg(embeddedMapping.size()));
        }

        _lastTexturePackage = _textureManager.packageTextures(
            meshes,
            textureBaseDir,
            textureSubfolder);

        // Re-inject embedded mappings (in case packageTextures cleared the mapping)
        if (scene && scene->mNumTextures > 0)
        {
            QMap<QString, QString> embeddedMapping = extractEmbeddedTextures(scene, textureBaseDir, textureSubfolder);
            for (auto it = embeddedMapping.begin(); it != embeddedMapping.end(); ++it)
            {
                _lastTexturePackage.pathMapping[it.key()] = it.value();
            }
        }

        // Add normalised-path aliases so the GltfPostProcessor's normalisedGlbPath()
        // lookups ("glb://image_N") find the same entries as the full-path keys
        // ("glb://D:/path/model.glb::image_N") that packageTextures stored.
        // Without this, findOrCreateTexture() misses existing embedded textures and
        // creates duplicate URI-based image entries pointing to temp files.
        {
            QList<QPair<QString, QString>> aliases;
            for (auto it = _lastTexturePackage.pathMapping.constBegin();
                 it != _lastTexturePackage.pathMapping.constEnd(); ++it)
            {
                const QString normKey = GltfPostProcessor::normalisedGlbPath(it.key());
                if (normKey != it.key())
                {
                    aliases.append({normKey, it.value()});
                }
            }
            for (const auto& p : aliases)
            {
                const bool replacingExisting =
                    _lastTexturePackage.pathMapping.contains(p.first) &&
                    _lastTexturePackage.pathMapping.value(p.first) != p.second;
                _lastTexturePackage.pathMapping[p.first] = p.second;
            }
            if (!aliases.isEmpty())
                logMessage(QString("  -> Added %1 normalised-path alias(es) for GLB embedded textures")
                               .arg(aliases.size()));
        }

        logMessage(QString("  -> Total texture mappings: %1").arg(_lastTexturePackage.pathMapping.size()));

        logMessage(QString("  -> Packaged %1 unique textures")
            .arg(_lastTexturePackage.textures.size()));

        if (_lastTexturePackage.totalSize > 0)
        {
            logMessage(QString("  -> Total size: %1 MB")
                .arg(_lastTexturePackage.totalSize / (1024.0 * 1024.0), 0, 'f', 2));
        }

        if (isGLB)
        {
            logMessage("  -> Note: Textures will be embedded in GLB and folder will be cleaned up");
        }
    }
    else
    {
        logMessage("Step 1: Texture copying disabled");
    }

    // ===== STEP 2: Sync scene mesh count to surviving _meshStore entries =====
    // _globalScene is never updated when the user deletes meshes, so the deep
    // copy may contain stale aiMesh/aiNode entries.  Prune them here so that
    // scene->mMeshes and the meshes vector are in 1-to-1 correspondence before
    // applyMaterialsToScene() rebuilds the material array.
    logMessage("Step 2: Syncing scene to mesh store...");
    syncSceneToMeshStore(scene, meshes);

    // ===== STEP 3: Apply materials to scene =====
    // syncSceneToMeshStore produces scene->mMeshes[] in ASCENDING sceneIndex order.
    // _meshStore (and therefore `meshes`) is in traversal order, which may differ.
    // Sort a local copy by sceneIndex so the positional assignment in
    // applyMaterialsToScene correctly pairs each TriangleMesh with its aiMesh.
    logMessage("Step 3: Applying materials to scene...");
    {
        std::vector<TriangleMesh*> sortedMeshes = meshes;
        std::stable_sort(sortedMeshes.begin(), sortedMeshes.end(),
            [](const TriangleMesh* a, const TriangleMesh* b)
            {
                return a->getSceneIndex() < b->getSceneIndex();
            });
        applyMaterialsToScene(scene, sortedMeshes, QString::fromStdString(exportPath));
    }

    // ===== STEP 4: Embed textures in scene (CRITICAL FOR GLB) =====
    logMessage("Step 4: Embedding textures in scene...");

    QStringList embeddedTextureNames;
    // Only embed for GLB export (ext already determined in STEP 1)
    if (isGLB)
    {
        embeddedTextureNames = embedTexturesInScene(scene, _lastTexturePackage);
    }
    else
    {
        logMessage("  -> Skipping texture embedding for non-binary format");
    }

    // ===== STEP 5: Export =====
    logMessage("Step 5: Exporting scene...");

    Assimp::Exporter exporter;
    const aiExportFormatDesc* format = findExportFormat(exportPath, exporter);

    if (!format)
    {
        logError(QString("Unsupported export format: %1")
            .arg(QString::fromStdString(ext.toStdString())));
        return aiReturn_FAILURE;
    }

    logMessage(QString("  -> Format: %1 (%2)")
        .arg(QString::fromLocal8Bit(format->description))
        .arg(QString::fromLocal8Bit(format->id)));

    // Set export flags for GLB to embed textures
    aiReturn result;
    if (ext == "glb" || ext == "gltf-binary")
    {
        // Use glTF2 exporter with embedding
        logMessage("  -> Exporting with embedded textures...");
        result = exporter.Export(scene, "glb2", exportPath.c_str());
    }
    else
    {
        result = exporter.Export(scene, format->id, exportPath.c_str());
    }

    if (result != aiReturn_SUCCESS)
    {
        logError(QString("Export failed: %1")
            .arg(QString::fromLocal8Bit(exporter.GetErrorString())));
        return result;
    }

    logMessage(QString("Export successful!"));
    logMessage(QString("  -> File: %1").arg(QString::fromStdString(exportPath)));
    logMessage(QString("  -> Meshes: %1").arg(scene->mNumMeshes));
    logMessage(QString("  -> Materials: %1").arg(scene->mNumMaterials));
    logMessage(QString("  -> Embedded textures: %1").arg(scene->mNumTextures));

    // Patch image names into GLB JSON so post-processor can match them
    if (isGLB && !embeddedTextureNames.isEmpty())
        patchGlbImageNames(exportFilePath, embeddedTextureNames, scene,
            _lastTexturePackage.textureDirectory, &_lastEmbeddedIndexMapping);

    // ===== STEP 6: Post-process glTF/GLB to add missing optional properties and write transforms =====
    logMessage("Step 6: Post-processing exported file with material transforms...");

    auto logCallback = [this](const QString& msg) {
        logMessage(msg);
        };

    if (isGLB)
    {
        if (GltfPostProcessor::postProcessGlbFileWithMaterials(exportFilePath, meshes, settings.lights, logCallback, textureSubfolder, _lastTexturePackage.pathMapping, _lastEmbeddedIndexMapping))
        {
            logMessage("  -> Post-processing complete");
        }
        else
        {
            logWarning("  -> Post-processing failed (file may still be valid)");
        }
    }
    else if (ext == "gltf")
    {
        if (GltfPostProcessor::postProcessGltfFileWithMaterials(exportFilePath, meshes, settings.lights, logCallback, textureSubfolder, _lastTexturePackage.pathMapping, _lastEmbeddedIndexMapping))
        {
            logMessage("  -> Post-processing complete");
        }
        else
        {
            logWarning("  -> Post-processing failed (file may still be valid)");
        }
    }

    // ===== STEP 6: Cleanup temp texture folder for GLB exports =====
    if (isGLB && settings.copyTextures && !_lastTexturePackage.textures.empty())
    {
        logMessage("Step 6: Cleaning up temp texture folder (textures embedded in GLB)...");

        QDir dir(_lastTexturePackage.textureDirectory);
        if (dir.removeRecursively())
        {
            logMessage(QString("  -> Removed temp textures folder: %1")
                .arg(_lastTexturePackage.textureDirectory));
        }
        else
        {
            logWarning(QString("  -> Could not remove temp textures folder: %1")
                .arg(_lastTexturePackage.textureDirectory));
        }
    }

    return aiReturn_SUCCESS;
}

aiMesh* AssImpMeshExporter::createMesh(
    const std::vector<Vertex>& vertices,
    const std::vector<unsigned int>& indices,
    const std::string& name)
{
    auto mesh = new aiMesh();
    mesh->mName = aiString(name.c_str());
    mesh->mNumVertices = static_cast<unsigned int>(vertices.size());

    // Allocate vertex attributes
    mesh->mVertices = new aiVector3D[mesh->mNumVertices];
    mesh->mNormals = new aiVector3D[mesh->mNumVertices];
    mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
    mesh->mColors[0] = new aiColor4D[mesh->mNumVertices];

    // Copy vertex data
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        const auto& v = vertices[i];

        // Position
        mesh->mVertices[i] = aiVector3D(v.Position.x, v.Position.y, v.Position.z);

        // Normal
        mesh->mNormals[i] = aiVector3D(v.Normal.x, v.Normal.y, v.Normal.z);

        // UV coordinates (first set)
        mesh->mTextureCoords[0][i] = aiVector3D(v.TexCoords[0].x, v.TexCoords[0].y, 0.0f);

        // Vertex color (RGBA)
        mesh->mColors[0][i] = aiColor4D(v.Color.r, v.Color.g, v.Color.b, v.Color.a);
    }

    // Create faces from indices
    mesh->mNumFaces = static_cast<unsigned int>(indices.size() / 3);
    mesh->mFaces = new aiFace[mesh->mNumFaces];

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace& face = mesh->mFaces[i];
        face.mNumIndices = 3;
        face.mIndices = new unsigned int[3] {
            indices[i * 3],
                indices[i * 3 + 1],
                indices[i * 3 + 2]
            };
    }

    return mesh;
}

const aiExportFormatDesc* AssImpMeshExporter::findExportFormat(
    const std::string& filePath,
    Assimp::Exporter& exporter)
{
    // Extract file extension
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos)
    {
        return nullptr;
    }

    std::string ext = filePath.substr(dotPos + 1);

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Search for matching exporter
    for (unsigned int i = 0; i < exporter.GetExportFormatCount(); ++i)
    {
        const aiExportFormatDesc* fmt = exporter.GetExportFormatDescription(i);
        if (fmt && ext == fmt->fileExtension)
        {
            return fmt;
        }
    }

    return nullptr;
}

void AssImpMeshExporter::patchGlbImageNames(
    const QString& glbPath,
    const QStringList& orderedNames,
    const aiScene* scene,
    const QString& textureDirectory,
    QMap<QString, int>* embeddedIndexMapping)
{
    QFile file(glbPath);
    if (!file.open(QIODevice::ReadWrite))
    {
        logWarning("patchGlbImageNames: cannot open GLB for patching");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    // GLB header: magic(4) version(4) length(4)
    // Chunk 0:    chunkLength(4) chunkType(4=0x4E4F534A) chunkData
    if (data.size() < 28)
        return;

    quint32 chunk0Len = *reinterpret_cast<const quint32*>(data.constData() + 12);
    int jsonStart = 20;   // 12 (header) + 8 (chunk0 header)
    int jsonLen = static_cast<int>(chunk0Len);

    QByteArray jsonBytes = data.mid(jsonStart, jsonLen);
    // Strip padding nulls/spaces that GLB appends to 4-byte-align the chunk
    while (!jsonBytes.isEmpty() && (jsonBytes.back() == '\0' || jsonBytes.back() == ' '))
        jsonBytes.chop(1);

    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject())
    {
        logWarning("patchGlbImageNames: failed to parse GLB JSON");
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray images = root.value("images").toArray();
    QJsonArray textures = root.value("textures").toArray();
    QJsonArray materials = root.value("materials").toArray();

    // === CORRECT APPROACH: Derive image names from the Assimp-written JSON ===
    //
    // Problem: embedTexturesInScene() iterates texTypes[] in a fixed order (BASE_COLOR,
    // NORMALS, METALNESS, ...) and builds embeddedNames[] in that order. But Assimp's GLB2
    // writer serialises material texture slots in its own internal field order (e.g. METALNESS
    // before NORMALS), so the binary image slots do NOT match the embeddedNames[] indices.
    // Assigning embeddedNames[i] to binary slot i puts the wrong name on the wrong data,
    // which the post-processor then uses to misroute texture indices.
    //
    // Fix: Walk the Assimp-written JSON. For each material slot (normalTexture,
    // metallicRoughnessTexture, ...) we know both (a) the image index Assimp assigned and
    // (b) what source path the aiScene holds for that slot. From the source path we can
    // derive the correct filename. This builds an imageIndex->name map that is independent
    // of iteration order.

    // Map from well-known JSON slot key -> {aiTextureType, slot-index-in-type}
    struct SlotInfo { aiTextureType type; unsigned int idx; };

    // Standard PBR slots
    const QMap<QString, SlotInfo> slotMap = {
        { "baseColorTexture",         { aiTextureType_BASE_COLOR,        0 } },
        { "metallicRoughnessTexture", { aiTextureType_METALNESS,         0 } },
        { "normalTexture",            { aiTextureType_NORMALS,           0 } },
        { "occlusionTexture",         { aiTextureType_LIGHTMAP,          0 } },
        { "emissiveTexture",          { aiTextureType_EMISSIVE,          0 } },
    };
    // KHR extension texture slots
    const QMap<QString, SlotInfo> extSlotMap = {
        { "clearcoatTexture",               { aiTextureType_CLEARCOAT,     0 } },
        { "clearcoatRoughnessTexture",      { aiTextureType_CLEARCOAT,     1 } },
        { "clearcoatNormalTexture",         { aiTextureType_CLEARCOAT,     2 } },
        { "sheenColorTexture",              { aiTextureType_SHEEN,         0 } },
        { "sheenRoughnessTexture",          { aiTextureType_SHEEN,         1 } },
        { "transmissionTexture",            { aiTextureType_TRANSMISSION,  0 } },
        { "specularTexture",                { aiTextureType_UNKNOWN,       0 } },
        { "specularColorTexture",           { aiTextureType_UNKNOWN,       1 } },
        { "anisotropyTexture",              { aiTextureType_UNKNOWN,       2 } },
        { "thicknessTexture",               { aiTextureType_UNKNOWN,       3 } },
        { "diffuseTexture",                 { aiTextureType_DIFFUSE,       0 } },
        { "specularGlossinessTexture",      { aiTextureType_SPECULAR,      0 } },
        { "iridescenceTexture",             { aiTextureType_UNKNOWN,       4 } },
        { "iridescenceThicknessTexture",    { aiTextureType_UNKNOWN,       5 } },
        { "diffuseTransmissionTexture",     { aiTextureType_UNKNOWN,       6 } },
        { "diffuseTransmissionColorTexture",{ aiTextureType_UNKNOWN,       7 } },
    };

    // Helper: get the source path Assimp stored for a given material/type/slot
    auto getSourcePath = [&](unsigned int matIdx, aiTextureType type, unsigned int slotIdx) -> QString {
        if (!scene || matIdx >= scene->mNumMaterials) return {};
        aiString texPath;
        if (scene->mMaterials[matIdx]->GetTexture(type, slotIdx, &texPath) == aiReturn_SUCCESS)
            return QString::fromLocal8Bit(texPath.C_Str());
        return {};
        };

    // Walk every JSON material, resolve imageIndex -> filename from the aiScene
    QMap<int, QString> imageNameMap;
    QMap<QString, QString> packagedToOriginalPath;
    for (auto it = _lastTexturePackage.pathMapping.constBegin();
         it != _lastTexturePackage.pathMapping.constEnd(); ++it)
    {
        if (!it.value().isEmpty() && !packagedToOriginalPath.contains(it.value()))
            packagedToOriginalPath[it.value()] = it.key();
    }

    for (int mi = 0; mi < materials.size(); ++mi)
    {
        QJsonObject mat = materials[mi].toObject();

        // Helper: given a texture reference object in the JSON, find which image index
        // it points to and record its name from the aiScene.
        auto resolveSlot = [&](const QString& slotKey, const QJsonObject& texRef,
            bool isExtension) {
                int texIdx = texRef.value("index").toInt(-1);
                if (texIdx < 0 || texIdx >= textures.size()) return;

                int imgIdx = textures[texIdx].toObject().value("source").toInt(-1);
                if (imgIdx < 0 || imgIdx >= images.size()) return;

                if (imageNameMap.contains(imgIdx)) return; // already resolved (shared texture)

                const SlotInfo* info = nullptr;
                if (!isExtension)
                {
                    auto it = slotMap.find(slotKey);
                    if (it != slotMap.end()) info = &it.value();
                }
                else
                {
                    auto it = extSlotMap.find(slotKey);
                    if (it != extSlotMap.end()) info = &it.value();
                }
                if (!info) return;

                QString path = getSourcePath(static_cast<unsigned int>(mi), info->type, info->idx);
                if (path.isEmpty()) return;

                QString authoritativePath = packagedToOriginalPath.value(path, path);
                if (authoritativePath == path)
                {
                    const QString sourceFileName = QFileInfo(path).fileName();
                    for (auto it = packagedToOriginalPath.constBegin();
                         it != packagedToOriginalPath.constEnd(); ++it)
                    {
                        if (QFileInfo(it.key()).fileName().compare(sourceFileName, Qt::CaseInsensitive) == 0)
                        {
                            authoritativePath = it.value();
                            break;
                        }
                    }
                }

                QString name = QFileInfo(path).fileName();
                if (!name.isEmpty())
                {
                    imageNameMap[imgIdx] = name;
                    if (embeddedIndexMapping)
                    {
                        int authoritativeIndex = imgIdx;
                        const QString normalizedPath = GltfPostProcessor::normalisedGlbPath(authoritativePath);
                        if (normalizedPath.startsWith("glb://image_"))
                        {
                            bool ok = false;
                            const int parsedIndex = normalizedPath.mid(QString("glb://image_").size()).toInt(&ok);
                            if (ok)
                                authoritativeIndex = parsedIndex;
                        }

                        (*embeddedIndexMapping)[authoritativePath] = authoritativeIndex;
                        if (normalizedPath != authoritativePath && !embeddedIndexMapping->contains(normalizedPath))
                            (*embeddedIndexMapping)[normalizedPath] = authoritativeIndex;
                    }
                    logMessage(QString("  patchGlbImageNames: img[%1] <- '%2'  (slot %3)")
                        .arg(imgIdx).arg(name).arg(slotKey));
                }
            };

        // Standard PBR slots
        QJsonObject pbr = mat.value("pbrMetallicRoughness").toObject();
        for (const QString& key : { "baseColorTexture", "metallicRoughnessTexture" })
        {
            QJsonObject ref = pbr.value(key).toObject();
            if (!ref.isEmpty()) resolveSlot(key, ref, false);
        }
        for (const QString& key : { "normalTexture", "occlusionTexture", "emissiveTexture" })
        {
            QJsonObject ref = mat.value(key).toObject();
            if (!ref.isEmpty()) resolveSlot(key, ref, false);
        }

        // KHR extension slots
        QJsonObject exts = mat.value("extensions").toObject();
        for (const QString& extName : exts.keys())
        {
            QJsonObject extData = exts.value(extName).toObject();
            for (const QString& key : extSlotMap.keys())
            {
                QJsonObject ref = extData.value(key).toObject();
                if (!ref.isEmpty()) resolveSlot(key, ref, true);
            }
        }
    }

    // Fill in names for existing images not resolved via JSON slot lookup
    for (int i = 0; i < images.size() && i < orderedNames.size(); ++i)
    {
        if (!imageNameMap.contains(i))
        {
            logMessage(QString("  patchGlbImageNames: img[%1] <- '%2'  (fallback)")
                .arg(i).arg(orderedNames[i]));
            imageNameMap[i] = orderedNames[i];
        }
    }

    // For any textures in orderedNames that Assimp didn't write a JSON image entry for
    // (e.g. KHR_materials_volume thickness, KHR_materials_anisotropy — Assimp's GLB2 writer
    // has no native support for these extensions, so it drops them from JSON even though
    // they were embedded in mTextures[]).
    // We identify missing ones by comparing orderedNames against imageNameMap coverage,
    // then pull image bytes directly from scene->mTextures[] (authoritative, in-memory)
    // rather than disk files which may be stale or missing for glb://image_N sources.
    QByteArray extraBinData; // image bytes to append after the existing BIN chunk
    if (orderedNames.size() > images.size() && scene && scene->mNumTextures > 0)
    {
        QJsonArray bufferViews = root.value("bufferViews").toArray();

        // Find current end of BIN data — new images will start here
        int binChunkHeaderOffset = jsonStart + jsonLen;
        while (binChunkHeaderOffset % 4 != 0) binChunkHeaderOffset++;
        quint32 existingBinLen = (binChunkHeaderOffset + 8 <= data.size())
            ? *reinterpret_cast<const quint32*>(data.constData() + binChunkHeaderOffset)
            : 0;
        int nextByteOffset = static_cast<int>(existingBinLen);

        // Build set of base filenames already covered by JSON image entries
        QSet<QString> coveredNames;
        for (auto it = imageNameMap.begin(); it != imageNameMap.end(); ++it)
            coveredNames.insert(QFileInfo(it.value()).completeBaseName());

        for (int i = 0; i < orderedNames.size(); ++i)
        {
            const QString& name = orderedNames[i];
            QString baseName = QFileInfo(name).completeBaseName();

            // Skip if already covered by an existing JSON image entry
            if (coveredNames.contains(baseName)) continue;

            // Find the matching aiTexture by base filename (mFilename is the leaf name
            // set in createEmbeddedTexture, e.g. "image_1.jpeg" or "image_1.png")
            QByteArray imgData;
            QString imgName = name;
            for (unsigned int ti = 0; ti < scene->mNumTextures; ++ti)
            {
                const aiTexture* tex = scene->mTextures[ti];
                if (!tex || tex->mWidth == 0) continue;

                QString texBase = QFileInfo(QString::fromLocal8Bit(tex->mFilename.C_Str()))
                                      .completeBaseName();
                if (texBase.compare(baseName, Qt::CaseInsensitive) != 0) continue;

                if (tex->mHeight == 0)
                {
                    // Compressed image stored as raw bytes
                    imgData = QByteArray(reinterpret_cast<const char*>(tex->pcData),
                                         static_cast<int>(tex->mWidth));
                    // Derive name with the real extension from format hint
                    QString hint = QString::fromLocal8Bit(tex->achFormatHint).toLower();
                    if (!hint.isEmpty() && hint != "png")
                        imgName = baseName + "." + hint;
                    else
                        imgName = baseName + ".png";
                }
                break;
            }

            if (imgData.isEmpty())
            {
                // mTextures lookup failed — fall back to disk file
                QString filePath = textureDirectory + "/" + name;
                if (!QFile::exists(filePath))
                    filePath = textureDirectory + "/" + QFileInfo(name).fileName();
                if (QFile::exists(filePath))
                {
                    QFile f(filePath);
                    if (f.open(QIODevice::ReadOnly))
                    {
                        imgData = f.readAll();
                        f.close();
                        imgName = QFileInfo(name).fileName();
                    }
                }
            }

            if (imgData.isEmpty())
            {
                logWarning(QString("patchGlbImageNames: no data for extra image %1: %2")
                    .arg(i).arg(name));
                continue;
            }

            // Detect MIME type from magic bytes
            QString mimeType = "image/png";
            if (imgData.size() >= 2 &&
                static_cast<uchar>(imgData[0]) == 0xFF &&
                static_cast<uchar>(imgData[1]) == 0xD8)
                mimeType = "image/jpeg";

            // Pad to 4-byte alignment
            while (nextByteOffset % 4 != 0) { extraBinData.append('\0'); nextByteOffset++; }

            // Create bufferView
            QJsonObject newBv;
            newBv["buffer"]     = 0;
            newBv["byteOffset"] = nextByteOffset;
            newBv["byteLength"] = imgData.size();
            int newBvIdx = bufferViews.size();
            bufferViews.append(newBv);

            // Create image entry
            QJsonObject newImg;
            newImg["bufferView"] = newBvIdx;
            newImg["mimeType"]   = mimeType;
            newImg["name"]       = imgName;
            images.append(newImg);
            coveredNames.insert(QFileInfo(imgName).completeBaseName());

            extraBinData.append(imgData);
            nextByteOffset += imgData.size();

            logMessage(QString("  patchGlbImageNames: appended img[%1] bufferView[%2] "
                               "offset=%3 length=%4 name='%5' (from mTextures)")
                       .arg(i).arg(newBvIdx)
                       .arg(newBv["byteOffset"].toInt())
                       .arg(imgData.size())
                       .arg(imgName));
        }

        root["bufferViews"] = bufferViews;
    }


    // Apply names to the JSON images array
    bool patched = false;
    for (int i = 0; i < images.size(); ++i)
    {
        QJsonObject img = images[i].toObject();
        // Only patch URI-less (embedded) images
        if (!img.contains("uri") || img.value("uri").toString().isEmpty())
        {
            if (imageNameMap.contains(i))
            {
                img["name"] = imageNameMap[i];
                images[i] = img;
                patched = true;
            }
        }
    }

    if (!patched && extraBinData.isEmpty())
        return;

    root["images"] = images;
    doc.setObject(root);

    // Re-encode JSON and pad to 4-byte boundary
    QByteArray newJson = doc.toJson(QJsonDocument::Compact);
    while (newJson.size() % 4 != 0)
        newJson.append(' ');

    // Reconstruct GLB bytes
    QByteArray newData;
    newData.reserve(data.size() + newJson.size() - jsonLen + extraBinData.size());

    // Copy original header (12 bytes)
    newData.append(data.left(12));

    // Write new JSON chunk length + type + data
    quint32 newChunkLen = static_cast<quint32>(newJson.size());
    newData.append(reinterpret_cast<const char*>(&newChunkLen), 4);
    quint32 chunkType = 0x4E4F534Au;
    newData.append(reinterpret_cast<const char*>(&chunkType), 4);
    newData.append(newJson);

    // Locate the original BIN chunk
    int afterJson = jsonStart + jsonLen;
    while (afterJson % 4 != 0) afterJson++;

    if (!extraBinData.isEmpty() && afterJson + 8 <= data.size())
    {
        // Read existing BIN chunk header, append its data, then our extra data
        quint32 oldBinLen = *reinterpret_cast<const quint32*>(data.constData() + afterJson);
        quint32 binType = *reinterpret_cast<const quint32*>(data.constData() + afterJson + 4);

        quint32 newBinLen = oldBinLen + static_cast<quint32>(extraBinData.size());
        newData.append(reinterpret_cast<const char*>(&newBinLen), 4);
        newData.append(reinterpret_cast<const char*>(&binType), 4);
        newData.append(data.mid(afterJson + 8, static_cast<int>(oldBinLen)));
        newData.append(extraBinData);
    }
    else
    {
        // No extra data — just copy BIN chunk as-is
        newData.append(data.mid(afterJson));
    }

    // Fix total length in GLB header
    quint32 totalLen = static_cast<quint32>(newData.size());
    memcpy(newData.data() + 8, &totalLen, 4);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        logWarning("patchGlbImageNames: cannot write patched GLB");
        return;
    }
    file.write(newData);
    file.close();

    logMessage(QString("  -> Patched %1 image names in GLB JSON").arg(orderedNames.size()));
}

void AssImpMeshExporter::logMessage(const QString& msg)
{
    if (_currentSettings.verbose)
    {
        qDebug() << msg;
    }
}

void AssImpMeshExporter::logWarning(const QString& msg)
{
    if (_currentSettings.verbose)
    {
        qWarning() << msg;
    }
}

void AssImpMeshExporter::logError(const QString& msg)
{
    qCritical() << msg;
}

// ===== FILE: AssImpMeshExporter_Part2.cpp =====
// This is the continuation of AssImpMeshExporter implementation
// Include this in the same compilation unit or link with Part 1
// Contains: Material creation, PBR properties, texture assignment, scene hierarchy

#include "AssImpMeshExporter.h"
#include "GLMaterial.h"
#include "TriangleMesh.h"

#include <QMatrix4x4>
#include <QDebug>

aiMaterial* AssImpMeshExporter::createMaterial(
    const GLMaterial& material,
    const TexturePackage& texturePackage,
    const QString& exportFileLocation)  // NEW parameter
{
    aiMaterial* aiMat = new aiMaterial();

    // ==== NAME =====
    aiString matName(material.name().toStdString());
    aiMat->AddProperty(&matName, AI_MATKEY_NAME);

    // ===== COLOR PROPERTIES =====
    {
        aiColor3D albedo(
            static_cast<float>(material.albedoColor().x()),
            static_cast<float>(material.albedoColor().y()),
            static_cast<float>(material.albedoColor().z()));
        // Write ONLY to AI_MATKEY_BASE_COLOR for glTF export to avoid conflicts with legacy ADS keys
        aiMat->AddProperty(&albedo, 1, AI_MATKEY_BASE_COLOR);
    }

    // ===== METALLIC & ROUGHNESS =====
    {
        float metallic = material.metalness();
        aiMat->AddProperty(&metallic, 1, AI_MATKEY_METALLIC_FACTOR);
    }

    {
        float roughness = material.roughness();
        aiMat->AddProperty(&roughness, 1, AI_MATKEY_ROUGHNESS_FACTOR);
    }

    // ===== TRANSPARENCY & IOR =====
    {
        float opacity = material.opacity();
        aiMat->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);
    }

    {
        float ior = material.ior();
        aiMat->AddProperty(&ior, 1, AI_MATKEY_REFRACTI);
    }

    // ===== TRANSMISSION =====
    if (material.transmission() > 0.0f)
    {
        float transmission = material.transmission();
        aiMat->AddProperty(&transmission, 1, AI_MATKEY_TRANSMISSION_FACTOR);
    }

    // ===== EMISSIVE =====
    {
        aiColor3D emissive(
            static_cast<float>(material.emissive().x()),
            static_cast<float>(material.emissive().y()),
            static_cast<float>(material.emissive().z()));
        aiMat->AddProperty(&emissive, 1, AI_MATKEY_COLOR_EMISSIVE);
    }

    // ===== PHASE 1: BASIC GLTF PROPERTIES =====

    // Alpha Mode
    {
        GLMaterial::BlendMode blendMode = material.blendMode();
        aiString alphaModeStr;

        if (blendMode == GLMaterial::BlendMode::Opaque)
        {
            alphaModeStr.Set("OPAQUE");
        }
        else if (blendMode == GLMaterial::BlendMode::Masked)
        {
            alphaModeStr.Set("MASK");
            float alphaCutoff = material.alphaThreshold();
            aiMat->AddProperty(&alphaCutoff, 1, "$mat.gltf.alphaCutoff", 0, 0);
        }
        else if (blendMode == GLMaterial::BlendMode::Alpha)
        {
            alphaModeStr.Set("BLEND");
        }

        aiMat->AddProperty(&alphaModeStr, "$mat.gltf.alphaMode", 0, 0);
    }

    // Double Sided
    {
        // Try to get the value - the exact method name may vary
        bool twoSided = material.twoSided();
        // Check if there's a method to get this - for now default to false
        // This needs the actual GLMaterial header to determine correct getter
        int twoSidedInt = twoSided ? 1 : 0;
        aiMat->AddProperty(&twoSidedInt, 1, "$mat.gltf.doubleSided", 0, 0);
    }

    // ===== PHASE 2: COMMON EXTENSIONS =====

    {
        float emissiveStrength = material.emissiveStrength();
        aiMat->AddProperty(&emissiveStrength, 1, AI_MATKEY_EMISSIVE_INTENSITY);
    }

    // ===== NORMAL SCALE =====
    {
        float normalScale = material.normalScale();
        aiMat->AddProperty(&normalScale, 1, AI_MATKEY_BUMPSCALING);
    }

    // ===== CLEARCOAT =====
    if (material.clearcoat() > 0.0f)
    {
        float clearcoat = material.clearcoat();
        aiMat->AddProperty(&clearcoat, 1, AI_MATKEY_CLEARCOAT_FACTOR);

        float clearcoatRoughness = material.clearcoatRoughness();
        aiMat->AddProperty(&clearcoatRoughness, 1, AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR);
    }

    // ===== SHEEN =====
    if (material.sheenColor().length() > 0.0f)
    {
        aiColor3D sheenColor(
            static_cast<float>(material.sheenColor().x()),
            static_cast<float>(material.sheenColor().y()),
            static_cast<float>(material.sheenColor().z()));
        aiMat->AddProperty(&sheenColor, 1, AI_MATKEY_SHEEN_COLOR_FACTOR);

        float sheenRoughness = material.sheenRoughness();
        aiMat->AddProperty(&sheenRoughness, 1, AI_MATKEY_SHEEN_ROUGHNESS_FACTOR);
    }

    // ===== LEGACY ADS =====
    {
        aiColor3D ambient(
            static_cast<float>(material.ambient().x()),
            static_cast<float>(material.ambient().y()),
            static_cast<float>(material.ambient().z()));
        aiMat->AddProperty(&ambient, 1, AI_MATKEY_COLOR_AMBIENT);
    }

    {
        aiColor3D diffuse(
            static_cast<float>(material.diffuse().x()),
            static_cast<float>(material.diffuse().y()),
            static_cast<float>(material.diffuse().z()));
        aiMat->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);
    }

    {
        aiColor3D specular(
            static_cast<float>(material.specular().x()),
            static_cast<float>(material.specular().y()),
            static_cast<float>(material.specular().z()));
        aiMat->AddProperty(&specular, 1, AI_MATKEY_COLOR_SPECULAR);
    }

    {
        float shininess = material.shininess();
        aiMat->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);
    }

    // ===== TEXTURES (NEW: Pass exportFileLocation) =====
    assignTexturesToMaterial(aiMat, material, texturePackage, true, exportFileLocation);

    return aiMat;
}

void AssImpMeshExporter::assignPBRProperties(
    aiMaterial* aiMat,
    const GLMaterial& material)
{
    // ===== BASE COLOR =====
    {
        aiColor3D albedo(
            static_cast<float>(material.albedoColor().x()),
            static_cast<float>(material.albedoColor().y()),
            static_cast<float>(material.albedoColor().z()));
        aiMat->AddProperty(&albedo, 1, AI_MATKEY_BASE_COLOR);
    }

    // ===== METALLIC & ROUGHNESS =====
    {
        float metallic = material.metalness();
        aiMat->AddProperty(&metallic, 1, AI_MATKEY_METALLIC_FACTOR);
    }

    {
        float roughness = material.roughness();
        aiMat->AddProperty(&roughness, 1, AI_MATKEY_ROUGHNESS_FACTOR);
    }

    // ===== TRANSPARENCY =====
    {
        float opacity = material.opacity();
        aiMat->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);
    }

    // ===== IOR (Index of Refraction) =====
    {
        float ior = material.ior();
        aiMat->AddProperty(&ior, 1, AI_MATKEY_REFRACTI);
    }

    // ===== TRANSMISSION (for glass and transparent materials) =====
    if (material.transmission() > 0.0f)
    {
        float transmission = material.transmission();
        aiMat->AddProperty(&transmission, 1, AI_MATKEY_TRANSMISSION_FACTOR);
    }

    // ===== EMISSIVE PROPERTIES =====
    {
        aiColor3D emissive(
            static_cast<float>(material.emissive().x()),
            static_cast<float>(material.emissive().y()),
            static_cast<float>(material.emissive().z()));
        aiMat->AddProperty(&emissive, 1, AI_MATKEY_COLOR_EMISSIVE);
    }

    {
        float emissiveStrength = material.emissiveStrength();
        aiMat->AddProperty(&emissiveStrength, 1, AI_MATKEY_EMISSIVE_INTENSITY);
    }

    // ===== NORMAL SCALE =====
    {
        float normalScale = material.normalScale();
        aiMat->AddProperty(&normalScale, 1, AI_MATKEY_BUMPSCALING);
    }

    // ===== OCCLUSION STRENGTH =====
    {
        float aoStrength = material.occlusionStrength();
        // Note: Assimp doesn't have a direct AO strength key, this is approximate
        //aiMat->AddProperty(&aoStrength, 1, AI_MATKEY_GLOBAL_BASE_COLOR_FACTOR);
    }

    // ===== CLEARCOAT EXTENSION =====
    if (material.clearcoat() > 0.0f)
    {
        float clearcoat = material.clearcoat();
        aiMat->AddProperty(&clearcoat, 1, AI_MATKEY_CLEARCOAT_FACTOR);

        float clearcoatRoughness = material.clearcoatRoughness();
        aiMat->AddProperty(&clearcoatRoughness, 1, AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR);
    }

    // ===== SHEEN EXTENSION =====
    if (material.sheenColor().length() > 0.0f)
    {
        aiColor3D sheenColor(
            static_cast<float>(material.sheenColor().x()),
            static_cast<float>(material.sheenColor().y()),
            static_cast<float>(material.sheenColor().z()));
        aiMat->AddProperty(&sheenColor, 1, AI_MATKEY_SHEEN_COLOR_FACTOR);

        float sheenRoughness = material.sheenRoughness();
        aiMat->AddProperty(&sheenRoughness, 1, AI_MATKEY_SHEEN_ROUGHNESS_FACTOR);
    }

    // ===== LEGACY ADS (for backward compatibility) =====
    {
        aiColor3D ambient(
            static_cast<float>(material.ambient().x()),
            static_cast<float>(material.ambient().y()),
            static_cast<float>(material.ambient().z()));
        aiMat->AddProperty(&ambient, 1, AI_MATKEY_COLOR_AMBIENT);
    }

    {
        aiColor3D diffuse(
            static_cast<float>(material.diffuse().x()),
            static_cast<float>(material.diffuse().y()),
            static_cast<float>(material.diffuse().z()));
        aiMat->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);
    }

    {
        aiColor3D specular(
            static_cast<float>(material.specular().x()),
            static_cast<float>(material.specular().y()),
            static_cast<float>(material.specular().z()));
        aiMat->AddProperty(&specular, 1, AI_MATKEY_COLOR_SPECULAR);
    }

    {
        float shininess = material.shininess();
        aiMat->AddProperty(&shininess, 1, AI_MATKEY_SHININESS);
    }
}

void AssImpMeshExporter::assignTexturesToMaterial(
    aiMaterial* aiMat,
    const GLMaterial& material,
    const TexturePackage& texturePackage,
    bool useEmbeddedTextures,
    const QString& exportFileLocation)
{
    logMessage(QString("  -> Assigning textures to material..."));

    // Detect export format
    QFileInfo fileInfo(exportFileLocation);
    QString ext = fileInfo.suffix().toLower();
    bool isGLTF = (ext == "gltf" || ext == "glb");

    // IMPORTANT: glTF texture handling notes:
    // 1. Metallic and Roughness MUST use the SAME texture (metallicRoughnessTexture)
    //    - Blue channel = metallic
    //    - Green channel = roughness
    // 2. Opacity/transparency is in the ALPHA channel of baseColorTexture, NOT a separate texture

    // Check if metallic and roughness use the same texture (for glTF)
    const auto& metallicTex = material.texture(GLMaterial::TextureType::Metallic);
    const auto& roughnessTex = material.texture(GLMaterial::TextureType::Roughness);
    bool hasMetallicRoughness = !metallicTex.path.empty() || !roughnessTex.path.empty();

    // For proper glTF export, metallic and roughness should point to the same texture
    // If they're different (which shouldn't happen for glTF), use metallic texture
    std::string metallicRoughnessPath = !metallicTex.path.empty() ? metallicTex.path : roughnessTex.path;

    // Build texture mappings based on format.
    // Each entry is {GLMaterial::TextureType, aiTextureType, slot-index}.
    // The slot index matters for types shared by multiple logical slots
    // (e.g. CLEARCOAT for clearcoat color/roughness/normal, UNKNOWN for specular/anisotropy).
    struct TexMapping { GLMaterial::TextureType mvType; aiTextureType aiType; unsigned int slot; };
    std::vector<TexMapping> textureMappings;

    if (isGLTF)
    {
        // glTF-specific mappings (Metallic/Roughness handled separately below)
        textureMappings = {
            {GLMaterial::TextureType::Albedo,             aiTextureType_BASE_COLOR,    0},
            {GLMaterial::TextureType::Normal,             aiTextureType_NORMALS,       0},
            {GLMaterial::TextureType::AmbientOcclusion,   aiTextureType_LIGHTMAP,      0},
            {GLMaterial::TextureType::Emissive,           aiTextureType_EMISSIVE,      0},
            {GLMaterial::TextureType::Transmission,       aiTextureType_TRANSMISSION,  0},
            {GLMaterial::TextureType::Height,             aiTextureType_HEIGHT,        0},
            {GLMaterial::TextureType::ClearcoatColor,     aiTextureType_CLEARCOAT,     0},
            {GLMaterial::TextureType::ClearcoatRoughness, aiTextureType_CLEARCOAT,     1},
            {GLMaterial::TextureType::ClearcoatNormal,    aiTextureType_CLEARCOAT,     2},
            {GLMaterial::TextureType::SheenColor,         aiTextureType_SHEEN,         0},
            {GLMaterial::TextureType::SheenRoughness,     aiTextureType_SHEEN,         1},
            {GLMaterial::TextureType::SpecularFactor,     aiTextureType_UNKNOWN,       0},
            {GLMaterial::TextureType::SpecularColor,      aiTextureType_UNKNOWN,       1},
            {GLMaterial::TextureType::Anisotropy,         aiTextureType_UNKNOWN,       2},
            {GLMaterial::TextureType::Thickness,          aiTextureType_UNKNOWN,       3},
            {GLMaterial::TextureType::Diffuse,            aiTextureType_DIFFUSE,       0},
            {GLMaterial::TextureType::SpecularGlossiness, aiTextureType_SPECULAR,      0},
            {GLMaterial::TextureType::Iridescence,        aiTextureType_UNKNOWN,       4},
            {GLMaterial::TextureType::IridescenceThickness, aiTextureType_UNKNOWN,     5},
            {GLMaterial::TextureType::DiffuseTransmission,      aiTextureType_UNKNOWN, 6},
            {GLMaterial::TextureType::DiffuseTransmissionColor, aiTextureType_UNKNOWN, 7},


        };
    }
    else
    {
        // Other formats (OBJ, FBX, etc.)
        textureMappings = {
            {GLMaterial::TextureType::Albedo,             aiTextureType_BASE_COLOR,        0},
            {GLMaterial::TextureType::Metallic,           aiTextureType_METALNESS,         0},
            {GLMaterial::TextureType::Roughness,          aiTextureType_DIFFUSE_ROUGHNESS, 0},
            {GLMaterial::TextureType::Normal,             aiTextureType_NORMALS,           0},
            {GLMaterial::TextureType::AmbientOcclusion,   aiTextureType_LIGHTMAP,          0},
            {GLMaterial::TextureType::Emissive,           aiTextureType_EMISSIVE,          0},
            {GLMaterial::TextureType::Transmission,       aiTextureType_TRANSMISSION,      0},
            {GLMaterial::TextureType::Opacity,            aiTextureType_OPACITY,           0},
            {GLMaterial::TextureType::Height,             aiTextureType_HEIGHT,            0},
            {GLMaterial::TextureType::ClearcoatColor,     aiTextureType_CLEARCOAT,         0},
            {GLMaterial::TextureType::ClearcoatRoughness, aiTextureType_CLEARCOAT,         1},
            {GLMaterial::TextureType::ClearcoatNormal,    aiTextureType_CLEARCOAT,         2},
            {GLMaterial::TextureType::SheenColor,         aiTextureType_SHEEN,             0},
            {GLMaterial::TextureType::SheenRoughness,     aiTextureType_SHEEN,             1},
            {GLMaterial::TextureType::Anisotropy,         aiTextureType_UNKNOWN,           2},
            {GLMaterial::TextureType::Thickness,          aiTextureType_UNKNOWN,           3},
        };
    }

    for (const auto& mapping : textureMappings)
    {
        const auto& tex = material.texture(mapping.mvType);
        if (tex.path.empty())
            continue;

        QString originalPath = QString::fromStdString(tex.path);

        // Look up with the original path first (handles full "glb://filepath::image_N"
        // URIs that packageTextures stores verbatim).  Fall back to the normalised form
        // "glb://image_N" for any legacy entries injected by extractEmbeddedTextures.
        auto it = texturePackage.pathMapping.find(originalPath);
        if (it == texturePackage.pathMapping.end())
            it = texturePackage.pathMapping.find(GltfPostProcessor::normalisedGlbPath(originalPath));

        if (it == texturePackage.pathMapping.end())
        {
            logWarning(QString("Texture not found in package: %1").arg(originalPath));
            continue;
        }

        QString texturePath = it.value();
        texturePath.replace("\\", "/");
        const unsigned int slot = mapping.slot;
        const aiTextureType aiType = mapping.aiType;

        aiString aiPath(texturePath.toStdString());
        aiMat->AddProperty(&aiPath, AI_MATKEY_TEXTURE(aiType, slot));

        int uvIndex = tex.texCoordIndex;
        aiMat->AddProperty(&uvIndex, 1, AI_MATKEY_UVWSRC(aiType, slot));

        int mappingModeU = aiTextureMapMode_Wrap;
        int mappingModeV = aiTextureMapMode_Wrap;
        aiMat->AddProperty(&mappingModeU, 1, AI_MATKEY_MAPPINGMODE_U(aiType, slot));
        aiMat->AddProperty(&mappingModeV, 1, AI_MATKEY_MAPPINGMODE_V(aiType, slot));

        aiUVTransform uvTransform;
        uvTransform.mTranslation = aiVector2D(tex.offset.x, tex.offset.y);
        uvTransform.mScaling = aiVector2D(tex.scale.x, tex.scale.y);
        uvTransform.mRotation = tex.rotation;
        aiMat->AddProperty(&uvTransform, 1, AI_MATKEY_UVTRANSFORM(aiType, slot));

        logMessage(QString("     -> %1: %2 (UV: scale=[%3,%4] offset=[%5,%6] rotation=%7)")
            .arg(GLMaterial::textureTypeToString(mapping.mvType))
            .arg(texturePath)
            .arg(tex.scale.x).arg(tex.scale.y)
            .arg(tex.offset.x).arg(tex.offset.y)
            .arg(tex.rotation));
    }

    // ===== METALLIC-ROUGHNESS COMBINED TEXTURE (glTF ONLY) =====
    // In glTF, metallic and roughness MUST be in the same texture
    // Blue channel = metallic, Green channel = roughness
    // For other formats, they were already handled as separate textures above
    if (isGLTF && hasMetallicRoughness)
    {
        QString originalPath = QString::fromStdString(metallicRoughnessPath);
        auto it = texturePackage.pathMapping.find(originalPath);
        if (it == texturePackage.pathMapping.end())
            it = texturePackage.pathMapping.find(GltfPostProcessor::normalisedGlbPath(originalPath));

        if (it != texturePackage.pathMapping.end())
        {
            QString texturePath = it.value();
            texturePath = texturePath.replace("\\", "/");

            // Add as BOTH metalness and roughness textures
            // Assimp should merge them into metallicRoughnessTexture
            aiString aiPath(texturePath.toStdString());

            // Use the metallic texture's properties (they should be the same for both)
            const auto& refTex = !metallicTex.path.empty() ? metallicTex : roughnessTex;

            // Add as metalness texture
            aiMat->AddProperty(&aiPath, AI_MATKEY_TEXTURE(aiTextureType_METALNESS, 0));
            int uvIndex = refTex.texCoordIndex;
            aiMat->AddProperty(&uvIndex, 1, AI_MATKEY_UVWSRC(aiTextureType_METALNESS, 0));

            // Add as roughness texture (same texture!)
            aiMat->AddProperty(&aiPath, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE_ROUGHNESS, 0));
            aiMat->AddProperty(&uvIndex, 1, AI_MATKEY_UVWSRC(aiTextureType_DIFFUSE_ROUGHNESS, 0));

            // UV transforms
            aiUVTransform uvTransform;
            uvTransform.mTranslation = aiVector2D(refTex.offset.x, refTex.offset.y);
            uvTransform.mScaling = aiVector2D(refTex.scale.x, refTex.scale.y);
            uvTransform.mRotation = refTex.rotation;
            aiMat->AddProperty(&uvTransform, 1, AI_MATKEY_UVTRANSFORM(aiTextureType_METALNESS, 0));
            aiMat->AddProperty(&uvTransform, 1, AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE_ROUGHNESS, 0));

            logMessage(QString("     -> metallicRoughnessTexture (glTF): %1").arg(texturePath));
        }
    }
}

/**
 * Helper method: Apply materials from original meshes to Assimp scene meshes
 *
 * This method:
 * 1. Iterates through scene meshes and corresponding ModelViewer mesh objects
 * 2. Creates/updates Assimp materials from GLMaterial data
 * 3. Assigns textures and PBR properties
 * 4. Updates material indices in mesh references
 *
 * @param scene The Assimp scene whose materials will be updated
 * @param meshes The original ModelViewer mesh objects
 */
void AssImpMeshExporter::syncSceneToMeshStore(
    aiScene* scene,
    const std::vector<TriangleMesh*>& meshes)
{
    if (!scene || !scene->mRootNode || meshes.empty())
        return;

    // Nothing to do if counts already match
    if (scene->mNumMeshes == static_cast<unsigned int>(meshes.size()))
        return;

    logMessage(QString("syncSceneToMeshStore: scene has %1 meshes, meshStore has %2 — pruning stale entries")
        .arg(scene->mNumMeshes).arg(meshes.size()));

    // Build the set of original aiScene mesh indices that are still alive in _meshStore.
    // Each TriangleMesh carries the index it was assigned at load time via setSceneIndex(),
    // so this is an exact, name-independent match.
    QSet<int> survivingSceneIndices;
    for (const TriangleMesh* m : meshes)
    {
        int idx = m->getSceneIndex();
        if (idx >= 0)
            survivingSceneIndices.insert(idx);
    }

    // Build old-index → new-index map (-1 means the mesh was deleted)
    std::vector<int> oldToNew(scene->mNumMeshes, -1);
    std::vector<aiMesh*> keptMeshes;
    keptMeshes.reserve(meshes.size());

    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh* m = scene->mMeshes[i];
        if (m && survivingSceneIndices.contains(static_cast<int>(i)))
        {
            oldToNew[i] = static_cast<int>(keptMeshes.size());
            keptMeshes.push_back(m);
        }
        else
        {
            // Free the stale aiMesh — it was deep-copied so we own it
            delete m;
            scene->mMeshes[i] = nullptr;
        }
    }

    // Replace the mesh array with the pruned one
    delete[] scene->mMeshes;
    scene->mNumMeshes = static_cast<unsigned int>(keptMeshes.size());
    scene->mMeshes = new aiMesh*[scene->mNumMeshes];
    std::copy(keptMeshes.begin(), keptMeshes.end(), scene->mMeshes);

    // Remap mesh index references in every aiNode, dropping deleted indices
    std::function<void(aiNode*)> remapNode = [&](aiNode* node)
    {
        if (!node)
            return;

        std::vector<unsigned int> remapped;
        remapped.reserve(node->mNumMeshes);

        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            unsigned int oldIdx = node->mMeshes[i];
            if (oldIdx < oldToNew.size() && oldToNew[oldIdx] >= 0)
                remapped.push_back(static_cast<unsigned int>(oldToNew[oldIdx]));
            // else: index belonged to a deleted mesh — drop it silently
        }

        delete[] node->mMeshes;
        node->mNumMeshes = static_cast<unsigned int>(remapped.size());
        if (!remapped.empty())
        {
            node->mMeshes = new unsigned int[node->mNumMeshes];
            std::copy(remapped.begin(), remapped.end(), node->mMeshes);
        }
        else
        {
            node->mMeshes = nullptr;
        }

        for (unsigned int c = 0; c < node->mNumChildren; ++c)
            remapNode(node->mChildren[c]);
    };

    remapNode(scene->mRootNode);

    logMessage(QString("syncSceneToMeshStore: pruned to %1 meshes").arg(scene->mNumMeshes));
}

void AssImpMeshExporter::applyMaterialsToScene(
    aiScene* scene,
    const std::vector<TriangleMesh*>& meshes,
    const QString& exportFileLocation)  // NEW parameter
{
    if (!scene || meshes.empty())
    {
        logWarning("applyMaterialsToScene: Invalid input");
        return;
    }

    unsigned int materialCount = std::min(static_cast<unsigned int>(meshes.size()), scene->mNumMeshes);
    std::vector<aiMaterial*> newMaterials;
    newMaterials.reserve(materialCount);

    for (unsigned int i = 0; i < materialCount; ++i)
    {
        const TriangleMesh* mesh = meshes[i];
        if (!mesh)
        {
            logWarning(QString("Mesh at index %1 is null").arg(i));
            continue;
        }

        // Create material with export location context
        const GLMaterial& glMat = mesh->getMaterial();
        aiMaterial* aiMat = createMaterial(glMat, _lastTexturePackage, exportFileLocation);

        if (!aiMat)
        {
            logError(QString("Failed to create material for: %1").arg(mesh->getName()));
            continue;
        }

        newMaterials.push_back(aiMat);

        if (i < scene->mNumMeshes && scene->mMeshes[i])
        {
            scene->mMeshes[i]->mMaterialIndex = static_cast<unsigned int>(newMaterials.size() - 1);
            logMessage(QString("  -> Material applied: %1").arg(mesh->getName()));
        }
    }

    // Replace scene materials
    if (!newMaterials.empty())
    {
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
        {
            delete scene->mMaterials[i];
        }
        delete[] scene->mMaterials;

        scene->mNumMaterials = static_cast<unsigned int>(newMaterials.size());
        scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
        std::copy(newMaterials.begin(), newMaterials.end(), scene->mMaterials);

        logMessage(QString("  -> Scene updated: %1 materials").arg(scene->mNumMaterials));
    }
}

/**
 * Load image file and return as aiTexture with embedded data
 *
 * This creates an Assimp texture object that contains the actual image data,
 * ready to be embedded in the exported file.
 */
aiTexture* AssImpMeshExporter::createEmbeddedTexture(const QString& imagePath)
{
    QFileInfo fi(imagePath);

    if (!fi.exists() || !fi.isFile())
    {
        logWarning(QString("Texture file not found: %1").arg(imagePath));
        return nullptr;
    }

    // Read the file as binary data (keep original PNG/JPEG format)
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        logWarning(QString("Failed to open texture file: %1").arg(imagePath));
        return nullptr;
    }

    QByteArray fileData = file.readAll();
    file.close();

    if (fileData.isEmpty())
    {
        logWarning(QString("Texture file is empty: %1").arg(imagePath));
        return nullptr;
    }

    // Create Assimp texture for compressed data
    aiTexture* texture = new aiTexture();

    // Store filename
    texture->mFilename = aiString(fi.fileName().toStdString());

    // For compressed data: set mHeight = 0, mWidth = file size
    texture->mHeight = 0;  // Indicates compressed format
    texture->mWidth = static_cast<unsigned int>(fileData.size());

    // Allocate and copy compressed data
    texture->pcData = reinterpret_cast<aiTexel*>(new unsigned char[fileData.size()]);
    memcpy(texture->pcData, fileData.data(), fileData.size());

    // Set format hint based on file extension
    QString ext = fi.suffix().toLower();
    if (ext == "png")
    {
        texture->achFormatHint[0] = 'p';
        texture->achFormatHint[1] = 'n';
        texture->achFormatHint[2] = 'g';
        texture->achFormatHint[3] = '\0';
    }
    else if (ext == "jpg" || ext == "jpeg")
    {
        texture->achFormatHint[0] = 'j';
        texture->achFormatHint[1] = 'p';
        texture->achFormatHint[2] = 'g';
        texture->achFormatHint[3] = '\0';
    }

    logMessage(QString("  -> Embedded texture: %1 (%2 bytes)")
        .arg(fi.fileName())
        .arg(fileData.size()));

    return texture;
}

/**
 * Embed all textures from scene materials into aiScene->mTextures
 *
 * This walks through all materials, finds referenced texture files,
 * loads them, and attaches to the scene so they get embedded in export.
 */
QStringList AssImpMeshExporter::embedTexturesInScene(
    aiScene* scene,
    const TexturePackage& texturePackage)
{
    if (!scene)
    {
        logError("Scene is null");
        return {};
    }

    logMessage("Step: Embedding textures into scene...");

    std::vector<aiTexture*> textures;
    std::set<QString> processedPaths;  // Avoid duplicates
    QStringList embeddedNames;

    // Define texture types + slot indices to check.
    // Multi-slot types (CLEARCOAT, SHEEN, UNKNOWN) need each slot checked individually.
    const std::pair<aiTextureType, unsigned int> texSlots[] = {
        {aiTextureType_BASE_COLOR,        0},
        {aiTextureType_NORMALS,           0},
        {aiTextureType_METALNESS,         0},
        {aiTextureType_DIFFUSE_ROUGHNESS, 0},
        {aiTextureType_LIGHTMAP,          0},
        {aiTextureType_EMISSIVE,          0},
        {aiTextureType_TRANSMISSION,      0},
        {aiTextureType_OPACITY,           0},
        {aiTextureType_HEIGHT,            0},
        {aiTextureType_CLEARCOAT,         0},  // clearcoatTexture
        {aiTextureType_CLEARCOAT,         1},  // clearcoatRoughnessTexture
        {aiTextureType_CLEARCOAT,         2},  // clearcoatNormalTexture
        {aiTextureType_SHEEN,             0},  // sheenColorTexture
        {aiTextureType_SHEEN,             1},  // sheenRoughnessTexture
        {aiTextureType_UNKNOWN,           0},  // specularTexture
        {aiTextureType_UNKNOWN,           1},  // specularColorTexture
        {aiTextureType_UNKNOWN,           2},  // anisotropyTexture
        {aiTextureType_UNKNOWN,           3},  // thicknessTexture (KHR_materials_volume)
        {aiTextureType_SPECULAR,          0},  // specularGlossinessTexture
        {aiTextureType_DIFFUSE,           0},  // diffuseTexture (specGloss)
        {aiTextureType_UNKNOWN,           4},  // iridescenceTexture
        {aiTextureType_UNKNOWN,           5},  // iridescenceThicknessTexture
        {aiTextureType_UNKNOWN,           6},  // diffuseTransmissionTexture
        {aiTextureType_UNKNOWN,           7},  // diffuseTransmissionColorTexture
    };

    // Iterate through all materials and collect unique texture paths
    for (unsigned int matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx)
    {
        aiMaterial* mat = scene->mMaterials[matIdx];
        if (!mat) continue;

        for (const auto& [texType, slotIdx] : texSlots)
        {
            aiString texPath;
            if (mat->GetTexture(texType, slotIdx, &texPath) == aiReturn_SUCCESS)
            {
                QString path = QString::fromLocal8Bit(texPath.C_Str());

                // Skip if already processed
                if (processedPaths.count(path))
                {
                    continue;
                }
                processedPaths.insert(path);

                QString fullPath = path;

                // Try to find in output directory by filename
                QFileInfo fi(path);
                QString candidate = _lastTexturePackage.textureDirectory + "/" + fi.fileName();
                if (QFileInfo(candidate).exists())
                {
                    fullPath = candidate;
                }
                else
                {
                    // Fallback: assume it's a direct path
                    fullPath = path;
                }

                // Load and embed
                aiTexture* texture = createEmbeddedTexture(fullPath);
                if (texture)
                {
                    textures.push_back(texture);
                    embeddedNames.append(fi.fileName());
                }
            }
        }
    }

    // Attach textures to scene
    if (!textures.empty())
    {
        scene->mNumTextures = static_cast<unsigned int>(textures.size());
        scene->mTextures = new aiTexture * [scene->mNumTextures];
        std::copy(textures.begin(), textures.end(), scene->mTextures);

        logMessage(QString("  -> Embedded %1 textures in scene").arg(textures.size()));
    }

    return embeddedNames;
}

QMap<QString, QString> AssImpMeshExporter::extractEmbeddedTextures(
    const aiScene* scene,
    const QString& outputDirectory,
    const QString& textureSubfolder)
{
    QMap<QString, QString> textureMapping;  // glb://image_N -> <subfolder>/image_N.ext

    if (!scene || scene->mNumTextures == 0)
        return textureMapping;

    QString texDir = outputDirectory + "/" + textureSubfolder;
    QDir().mkpath(texDir);

    logMessage(QString("Extracting %1 embedded texture(s) from GLB...").arg(scene->mNumTextures));

    for (unsigned int i = 0; i < scene->mNumTextures; ++i)
    {
        aiTexture* tex = scene->mTextures[i];
        if (!tex) continue;

        // Get format
        QString format = QString::fromLocal8Bit(tex->achFormatHint);
        if (format.isEmpty()) format = "png";

        // Write with image_N naming to match MaterialProcessor's glb://image_N URIs
        QString filename = QString("image_%1.%2").arg(i).arg(format);
        QString outputPath = texDir + "/" + filename;

        // Check if already exists
        if (QFile::exists(outputPath))
        {
            logMessage(QString("  -> Skipping (already exists): %1").arg(filename));
            // Still add to mapping even if file exists
            QString glbUri = QString("glb://image_%1").arg(i);
            QString relativePath = QString("%1/%2").arg(textureSubfolder).arg(filename);
            textureMapping[glbUri] = relativePath;
            continue;
        }

        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly))
        {
            logWarning(QString("Failed to create texture file: %1").arg(outputPath));
            continue;
        }

        bool success = false;

        if (tex->mHeight == 0)
        {
            // Compressed
            file.write(reinterpret_cast<const char*>(tex->pcData), tex->mWidth);
            file.close();
            success = true;
            logMessage(QString("  -> Extracted: %1 (%2 bytes)").arg(filename).arg(tex->mWidth));
        }
        else
        {
            // Uncompressed RGBA
            file.close();
            QImage img(
                reinterpret_cast<const uchar*>(tex->pcData),
                tex->mWidth,
                tex->mHeight,
                tex->mWidth * 4,
                QImage::Format_RGBA8888
            );

            if (img.save(outputPath, "PNG"))
            {
                success = true;
                logMessage(QString("  -> Extracted: %1 (%2x%3 RGBA)").arg(filename).arg(tex->mWidth).arg(tex->mHeight));
            }
            else
            {
                logWarning(QString("Failed to save: %1").arg(filename));
                QFile::remove(outputPath);
            }
        }

        // Add to mapping if successful
        if (success)
        {
            QString glbUri = QString("glb://image_%1").arg(i);
            QString relativePath = QString("%1/%2").arg(textureSubfolder).arg(filename);
            textureMapping[glbUri] = relativePath;

            logMessage(QString("  -> Mapped: %1 -> %2").arg(glbUri).arg(relativePath));
        }
    }

    logMessage(QString("Extraction complete: %1 textures, %2 mappings")
        .arg(scene->mNumTextures).arg(textureMapping.size()));

    return textureMapping;
}

aiScene* AssImpMeshExporter::createScene(
    const std::vector<aiMesh*>& meshes,
    const std::vector<aiMaterial*>& materials,
    const std::vector<QMatrix4x4>& transforms)
{
    aiScene* scene = new aiScene();

    // Setup mesh array
    scene->mNumMeshes = static_cast<unsigned int>(meshes.size());
    scene->mMeshes = new aiMesh * [scene->mNumMeshes];
    std::copy(meshes.begin(), meshes.end(), scene->mMeshes);

    // Setup material array
    scene->mNumMaterials = static_cast<unsigned int>(materials.size());
    scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
    std::copy(materials.begin(), materials.end(), scene->mMaterials);

    // Create root node
    scene->mRootNode = new aiNode();
    scene->mRootNode->mName = aiString("RootNode");
    scene->mRootNode->mTransformation = aiMatrix4x4();

    // Create child nodes (one per mesh) with transforms
    scene->mRootNode->mNumChildren = static_cast<unsigned int>(meshes.size());
    scene->mRootNode->mChildren = new aiNode * [scene->mRootNode->mNumChildren];

    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        aiNode* childNode = new aiNode();

        // Determine node name
        std::string nodeName = "Mesh_" + std::to_string(i);
        if (meshes[i]->mName.length > 0)
        {
            nodeName = std::string(meshes[i]->mName.C_Str());
        }
        childNode->mName = aiString(nodeName);

        // Set parent relationship
        childNode->mParent = scene->mRootNode;

        // Assign mesh
        childNode->mNumMeshes = 1;
        childNode->mMeshes = new unsigned int[1];
        childNode->mMeshes[0] = i;

        // ===== APPLY TRANSFORMATION (NEW) =====
        if (i < transforms.size())
        {
            const auto& qmat = transforms[i];

            // Convert QMatrix4x4 to aiMatrix4x4
            // QMatrix4x4 is in column-major order
            childNode->mTransformation = aiMatrix4x4(
                qmat(0, 0), qmat(0, 1), qmat(0, 2), qmat(0, 3),
                qmat(1, 0), qmat(1, 1), qmat(1, 2), qmat(1, 3),
                qmat(2, 0), qmat(2, 1), qmat(2, 2), qmat(2, 3),
                qmat(3, 0), qmat(3, 1), qmat(3, 2), qmat(3, 3)
            );

            logMessage(QString("  -> Transform applied to: %1")
                .arg(QString::fromStdString(nodeName)));
        }
        else
        {
            childNode->mTransformation = aiMatrix4x4();
        }

        scene->mRootNode->mChildren[i] = childNode;
    }

    // Root node has no direct mesh assignments
    scene->mRootNode->mNumMeshes = 0;
    scene->mRootNode->mMeshes = nullptr;

    return scene;
}
