#include "AssImpMeshExporter.h"
#include "TriangleMesh.h"
#include "AssImpMesh.h"
#include "GLMaterial.h"
#include "GltfPostProcessor.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMatrix4x4>
#include <algorithm>
#include <memory>
#include <set>

AssImpMeshExporter::AssImpMeshExporter(QObject* parent)
    : QObject(parent)
{
}

aiReturn AssImpMeshExporter::exportMeshes(
    const std::vector<TriangleMesh*>& meshes,
    const QString& exportPath,
    const ExportSettings& settings)
{
    _currentSettings = settings;

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

    if (settings.copyTextures)
    {
        _lastTexturePackage = _textureManager.packageTextures(
            meshes,
            settings.outputDirectory);

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

        // Create material
        aiMaterial* aiMat = createMaterial(mesh->getMaterial(), _lastTexturePackage, exportPath);
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

    std::unique_ptr<aiScene> scene(createScene(aiMeshes, aiMaterials, transforms));
    if (!scene)
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

    aiReturn result = exporter.Export(scene.get(), format->id, exportPath.toStdString().c_str());

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

    // ===== STEP 5: Cleanup texture folder for GLB exports =====
    if (isGLB && settings.copyTextures && !_lastTexturePackage.textures.empty())
    {
        logMessage("Step 5: Cleaning up texture folder (textures embedded in GLB)...");

        QString texturesDir = settings.outputDirectory + "/textures";
        QDir dir(texturesDir);

        if (dir.exists())
        {
            // Remove all files in the textures directory
            QStringList files = dir.entryList(QDir::Files);
            int removedCount = 0;

            for (const QString& file : files)
            {
                if (dir.remove(file))
                {
                    removedCount++;
                }
            }

            // Remove the empty textures directory
            if (dir.rmdir(texturesDir))
            {
                logMessage(QString("  -> Removed textures folder (%1 files cleaned up)")
                    .arg(removedCount));
            }
            else
            {
                logWarning("  -> Could not remove textures folder");
            }
        }
    }

    return aiReturn_SUCCESS;
}

aiReturn AssImpMeshExporter::exportScene(const aiScene* scene,
    const std::string& exportPath)
{
    logMessage(QString("Exporting Assimp scene to: %1")
        .arg(QString::fromStdString(exportPath)));

    Assimp::Exporter exporter;
    const aiExportFormatDesc* format = findExportFormat(exportPath, exporter);

    if (!format)
    {
        logError(QString("Unsupported export format: %1")
            .arg(QString::fromStdString(exportPath)));
        return aiReturn_FAILURE;
    }

    aiReturn result = exporter.Export(scene, format->id, exportPath.c_str());

    if (result != aiReturn_SUCCESS)
    {
        logError(QString("Export failed: %1")
            .arg(QString::fromLocal8Bit(exporter.GetErrorString())));
    }
    else
    {
        logMessage(QString("Export successful: %1")
            .arg(QString::fromStdString(exportPath)));
    }

    return result;
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

    // ===== STEP 1: Package textures =====
    if (settings.copyTextures && !meshes.empty())
    {
        logMessage("Step 1: Packaging textures...");

        _lastTexturePackage = _textureManager.packageTextures(
            meshes,
            settings.outputDirectory);

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

    // ===== STEP 2: Apply materials to scene =====
    logMessage("Step 2: Applying materials to scene...");
    applyMaterialsToScene(scene, meshes, QString::fromStdString(exportPath));

    // ===== STEP 3: Embed textures in scene (CRITICAL FOR GLB) =====
    logMessage("Step 3: Embedding textures in scene...");

    // Only embed for GLB export (ext already determined in STEP 1)
    if (isGLB)
    {
        embedTexturesInScene(scene, _lastTexturePackage);
    }
    else
    {
        logMessage("  -> Skipping texture embedding for non-binary format");
    }

    // ===== STEP 4: Export =====
    logMessage("Step 4: Exporting scene...");

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

    // ===== STEP 5: Post-process glTF/GLB to add missing optional properties and write transforms =====
    logMessage("Step 5: Post-processing exported file with material transforms...");

    auto logCallback = [this](const QString& msg) {
        logMessage(msg);
        };

    if (isGLB)
    {
        if (GltfPostProcessor::postProcessGlbFileWithMaterials(exportFilePath, meshes, logCallback))
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
        if (GltfPostProcessor::postProcessGltfFileWithMaterials(exportFilePath, meshes, logCallback))
        {
            logMessage("  -> Post-processing complete");
        }
        else
        {
            logWarning("  -> Post-processing failed (file may still be valid)");
        }
    }

    // ===== STEP 6: Cleanup texture folder for GLB exports =====
    // NOTE: Only delete textures folder if we're ONLY exporting GLB
    // If a .gltf file might also exist, keep the textures folder
    if (isGLB && settings.copyTextures && !_lastTexturePackage.textures.empty())
    {
        // Check if a .gltf file exists alongside this .glb
        QString gltfPath = exportFilePath;
        gltfPath.replace(".glb", ".gltf");

        bool hasGltfFile = QFileInfo(gltfPath).exists();

        if (!hasGltfFile)
        {
            // Safe to delete - only GLB exists
            logMessage("Step 6: Cleaning up texture folder (textures embedded in GLB, no .gltf file)...");

            QString texturesDir = settings.outputDirectory + "/textures";
            QDir dir(texturesDir);

            if (dir.exists())
            {
                // Remove all files in the textures directory
                QStringList files = dir.entryList(QDir::Files);
                int removedCount = 0;

                for (const QString& file : files)
                {
                    if (dir.remove(file))
                    {
                        removedCount++;
                    }
                }

                // Remove the empty textures directory
                if (dir.rmdir(texturesDir))
                {
                    logMessage(QString("  -> Removed textures folder (%1 files cleaned up)")
                        .arg(removedCount));
                }
                else
                {
                    logWarning("  -> Could not remove textures folder");
                }
            }
        }
        else
        {
            logMessage("Step 6: Keeping texture folder (needed for existing .gltf file)");
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

    // ===== COLOR PROPERTIES =====
    {
        aiColor3D albedo(
            static_cast<float>(material.albedoColor().x()),
            static_cast<float>(material.albedoColor().y()),
            static_cast<float>(material.albedoColor().z()));
        aiMat->AddProperty(&albedo, 1, AI_MATKEY_COLOR_DIFFUSE);
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
            logMessage(QString("     -> alphaMode: MASK, alphaCutoff: %1").arg(alphaCutoff));
        }
        else if (blendMode == GLMaterial::BlendMode::Alpha)
        {
            alphaModeStr.Set("BLEND");
            logMessage(QString("     -> alphaMode: BLEND"));
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
        if (twoSided)
        {
            logMessage(QString("     -> doubleSided: true"));
        }
    }

    // ===== PHASE 2: COMMON EXTENSIONS =====

    // KHR_materials_ior
    {
        float ior = material.ior();
        // Only write if different from default (1.5)
        if (std::abs(ior - 1.5f) > 0.001f)
        {
            aiMat->AddProperty(&ior, 1, "$mat.gltf.ior", 0, 0);
            logMessage(QString("     -> ior: %1").arg(ior));
        }
    }

    // KHR_materials_emissive_strength
    {
        float emissiveStrength = material.emissiveStrength();
        if (std::abs(emissiveStrength - 1.0f) > 0.001f)
        {
            aiMat->AddProperty(&emissiveStrength, 1, "$mat.gltf.emissiveStrength", 0, 0);
            logMessage(QString("     -> emissiveStrength: %1").arg(emissiveStrength));
        }
    }


    // ===== PHASE 3: ADVANCED EXTENSIONS =====

    // KHR_materials_clearcoat
    {
        float clearcoat = material.clearcoat();
        if (clearcoat > 0.0f)
        {
            aiMat->AddProperty(&clearcoat, 1, "$mat.gltf.clearcoat.factor", 0, 0);

            float clearcoatRoughness = material.clearcoatRoughness();
            aiMat->AddProperty(&clearcoatRoughness, 1, "$mat.gltf.clearcoat.roughnessFactor", 0, 0);

            logMessage(QString("     -> clearcoat: %1, roughness: %2")
                .arg(clearcoat).arg(clearcoatRoughness));
        }
    }

    // KHR_materials_sheen
    {
        QVector3D sheenColor = material.sheenColor();
        if (sheenColor.length() > 0.0f)
        {
            aiColor3D color(sheenColor.x(), sheenColor.y(), sheenColor.z());
            aiMat->AddProperty(&color, 1, "$mat.gltf.sheen.sheenColorFactor", 0, 0);

            float sheenRoughness = material.sheenRoughness();
            aiMat->AddProperty(&sheenRoughness, 1, "$mat.gltf.sheen.sheenRoughnessFactor", 0, 0);

            logMessage(QString("     -> sheen: [%1, %2, %3], roughness: %4")
                .arg(sheenColor.x()).arg(sheenColor.y()).arg(sheenColor.z())
                .arg(sheenRoughness));
        }
    }

    // KHR_materials_iridescence
    {
        float iridescence = material.iridescenceFactor();
        if (iridescence > 0.0f)
        {
            aiMat->AddProperty(&iridescence, 1, "$mat.gltf.iridescence.iridescenceFactor", 0, 0);

            float iridescenceIor = material.iridescenceIor();
            aiMat->AddProperty(&iridescenceIor, 1, "$mat.gltf.iridescence.iridescenceIor", 0, 0);

            float thicknessMin = material.iridescenceThicknessMin();
            float thicknessMax = material.iridescenceThicknessMax();
            aiMat->AddProperty(&thicknessMin, 1, "$mat.gltf.iridescence.iridescenceThicknessMinimum", 0, 0);
            aiMat->AddProperty(&thicknessMax, 1, "$mat.gltf.iridescence.iridescenceThicknessMaximum", 0, 0);

            logMessage(QString("     -> iridescence: %1, ior: %2, thickness: [%3, %4]")
                .arg(iridescence).arg(iridescenceIor).arg(thicknessMin).arg(thicknessMax));
        }
    }

    // ===== PHASE 4: SPECIALIZED EXTENSIONS =====

    // KHR_materials_volume
    {
        float thickness = material.thicknessFactor();
        if (thickness > 0.0f)
        {
            aiMat->AddProperty(&thickness, 1, "$mat.gltf.volume.thicknessFactor", 0, 0);

            float attenuationDist = material.attenuationDistance();
            aiMat->AddProperty(&attenuationDist, 1, "$mat.gltf.volume.attenuationDistance", 0, 0);

            QVector3D attenuationColor = material.attenuationColor();
            aiColor3D attenColor(attenuationColor.x(), attenuationColor.y(), attenuationColor.z());
            aiMat->AddProperty(&attenColor, 1, "$mat.gltf.volume.attenuationColor", 0, 0);

            logMessage(QString("     -> volume: thickness=%1, attenuationDist=%2")
                .arg(thickness).arg(attenuationDist));
        }
    }

    // KHR_materials_specular
    {
        float specularFactor = material.specularFactor();
        QVector3D specularColorFactor = material.specularColorFactor();

        // Check if we have specular textures
        const auto& specularFactorTex = material.texture(GLMaterial::TextureType::SpecularFactor);
        const auto& specularColorTex = material.texture(GLMaterial::TextureType::SpecularColor);
        bool hasSpecularFactorTex = !specularFactorTex.path.empty();
        bool hasSpecularColorTex = !specularColorTex.path.empty();

        bool hasSpecular = (std::abs(specularFactor - 1.0f) > 0.001f) ||
            (specularColorFactor != QVector3D(1.0f, 1.0f, 1.0f)) ||
            hasSpecularFactorTex || hasSpecularColorTex;

        if (hasSpecular)
        {
            aiMat->AddProperty(&specularFactor, 1, "$mat.gltf.specular.specularFactor", 0, 0);

            aiColor3D color(specularColorFactor.x(), specularColorFactor.y(), specularColorFactor.z());
            aiMat->AddProperty(&color, 1, "$mat.gltf.specular.specularColorFactor", 0, 0);

            // Export specularTexture if present
            if (hasSpecularFactorTex)
            {
                QString texPath = QString::fromStdString(specularFactorTex.path);
                aiString aiTexPath(texPath.toStdString());
                aiMat->AddProperty(&aiTexPath, "$mat.gltf.specular.specularTexture", 0, 0);
                logMessage(QString("     -> specularTexture: %1").arg(texPath));
            }

            // Export specularColorTexture if present
            if (hasSpecularColorTex)
            {
                QString texPath = QString::fromStdString(specularColorTex.path);
                aiString aiTexPath(texPath.toStdString());
                aiMat->AddProperty(&aiTexPath, "$mat.gltf.specular.specularColorTexture", 0, 0);
                logMessage(QString("     -> specularColorTexture: %1").arg(texPath));
            }

            logMessage(QString("     -> specular: factor=%1, color=[%2,%3,%4]")
                .arg(specularFactor)
                .arg(specularColorFactor.x())
                .arg(specularColorFactor.y())
                .arg(specularColorFactor.z()));
        }
    }

    // KHR_materials_anisotropy
    {
        float anisotropyStrength = material.anisotropyStrength();
        if (anisotropyStrength > 0.0f)
        {
            aiMat->AddProperty(&anisotropyStrength, 1, "$mat.gltf.anisotropy.anisotropyStrength", 0, 0);

            float anisotropyRotation = material.anisotropyRotation();
            aiMat->AddProperty(&anisotropyRotation, 1, "$mat.gltf.anisotropy.anisotropyRotation", 0, 0);

            logMessage(QString("     -> anisotropy: strength=%1, rotation=%2")
                .arg(anisotropyStrength).arg(anisotropyRotation));
        }
    }

    // KHR_materials_dispersion
    {
        float dispersion = material.dispersion();
        if (dispersion > 0.0f)
        {
            aiMat->AddProperty(&dispersion, 1, "$mat.gltf.dispersion", 0, 0);
            logMessage(QString("     -> dispersion: %1").arg(dispersion));
        }
    }

    // KHR_materials_unlit
    {
        bool unlit = material.isUnlit();
        if (unlit)
        {
            int unlitInt = 1;
            aiMat->AddProperty(&unlitInt, 1, "$mat.gltf.unlit", 0, 0);
            logMessage(QString("     -> unlit: true"));
        }
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

    // Build texture mappings based on format
    std::vector<std::pair<GLMaterial::TextureType, aiTextureType>> textureMappings;

    if (isGLTF)
    {
        // glTF-specific mappings
        textureMappings = {
            {GLMaterial::TextureType::Albedo, aiTextureType_BASE_COLOR},
            // Note: Metallic and Roughness are handled specially for glTF (combined texture)
            {GLMaterial::TextureType::Normal, aiTextureType_NORMALS},
            {GLMaterial::TextureType::AmbientOcclusion, aiTextureType_LIGHTMAP},
            {GLMaterial::TextureType::Emissive, aiTextureType_EMISSIVE},
            {GLMaterial::TextureType::Transmission, aiTextureType_TRANSMISSION},
            // Note: Opacity is NOT a separate texture in glTF - it's in baseColorTexture alpha
            {GLMaterial::TextureType::Height, aiTextureType_HEIGHT},
            {GLMaterial::TextureType::ClearcoatColor, aiTextureType_CLEARCOAT},
            {GLMaterial::TextureType::ClearcoatRoughness, aiTextureType_CLEARCOAT},
            {GLMaterial::TextureType::ClearcoatNormal, aiTextureType_CLEARCOAT},
            {GLMaterial::TextureType::SheenColor, aiTextureType_SHEEN},
            {GLMaterial::TextureType::SheenRoughness, aiTextureType_SHEEN},
            {GLMaterial::TextureType::SpecularFactor, aiTextureType_UNKNOWN},
            {GLMaterial::TextureType::SpecularColor, aiTextureType_UNKNOWN}, 
        };
    }
    else
    {
        // Other formats (OBJ, FBX, etc.) - use all textures including separate metallic/roughness/opacity
        textureMappings = {
            {GLMaterial::TextureType::Albedo, aiTextureType_BASE_COLOR},
            {GLMaterial::TextureType::Metallic, aiTextureType_METALNESS},
            {GLMaterial::TextureType::Roughness, aiTextureType_DIFFUSE_ROUGHNESS},
            {GLMaterial::TextureType::Normal, aiTextureType_NORMALS},
            {GLMaterial::TextureType::AmbientOcclusion, aiTextureType_LIGHTMAP},
            {GLMaterial::TextureType::Emissive, aiTextureType_EMISSIVE},
            {GLMaterial::TextureType::Transmission, aiTextureType_TRANSMISSION},
            {GLMaterial::TextureType::Opacity, aiTextureType_OPACITY},  // Keep for non-glTF formats
            {GLMaterial::TextureType::Height, aiTextureType_HEIGHT},
            {GLMaterial::TextureType::ClearcoatColor, aiTextureType_CLEARCOAT},
            {GLMaterial::TextureType::ClearcoatRoughness, aiTextureType_CLEARCOAT},
            {GLMaterial::TextureType::ClearcoatNormal, aiTextureType_CLEARCOAT},
            {GLMaterial::TextureType::SheenColor, aiTextureType_SHEEN},
            {GLMaterial::TextureType::SheenRoughness, aiTextureType_SHEEN},
        };
    }

    for (const auto& [modelViewerType, assimpType] : textureMappings)
    {
        const auto& tex = material.texture(modelViewerType);
        if (tex.path.empty())
        {
            continue;
        }

        QString originalPath = QString::fromStdString(tex.path);
        auto it = texturePackage.pathMapping.find(originalPath);

        if (it == texturePackage.pathMapping.end())
        {
            logWarning(QString("Texture not found in package: %1").arg(originalPath));
            continue;
        }

        QString texturePath;

        if (useEmbeddedTextures)
        {
            // For embedded textures, use the "*index" notation
            // This will be resolved to the actual aiTexture in mTextures array
            // For now, just use the relative path - Assimp will handle embedding
            texturePath = it.value();
        }
        else
        {
            // For external textures, use relative path
            texturePath = it.value();
        }

        texturePath = texturePath.replace("\\", "/");

        aiString aiPath(texturePath.toStdString());
        aiMat->AddProperty(&aiPath, AI_MATKEY_TEXTURE(assimpType, 0));

        // CRITICAL: Set texture coordinate set index (defaults to 0)
        // Without this, release builds may use uninitialized memory
        int uvIndex = tex.texCoordIndex;
        aiMat->AddProperty(&uvIndex, 1, AI_MATKEY_UVWSRC(assimpType, 0));

        // Set texture mapping mode to wrap (defaults)
        int mappingModeU = aiTextureMapMode_Wrap;
        int mappingModeV = aiTextureMapMode_Wrap;
        aiMat->AddProperty(&mappingModeU, 1, AI_MATKEY_MAPPINGMODE_U(assimpType, 0));
        aiMat->AddProperty(&mappingModeV, 1, AI_MATKEY_MAPPINGMODE_V(assimpType, 0));

        // Set UV transform (scale, offset, rotation)
        // This is critical for glTF export with KHR_texture_transform
        aiUVTransform uvTransform;
        uvTransform.mTranslation = aiVector2D(tex.offset.x, tex.offset.y);
        uvTransform.mScaling = aiVector2D(tex.scale.x, tex.scale.y);
        uvTransform.mRotation = tex.rotation;
        aiMat->AddProperty(&uvTransform, 1, AI_MATKEY_UVTRANSFORM(assimpType, 0));

        logMessage(QString("     -> %1: %2 (UV: scale=[%3,%4] offset=[%5,%6] rotation=%7)")
            .arg(GLMaterial::textureTypeToString(modelViewerType))
            .arg(texturePath)
            .arg(tex.scale.x)
            .arg(tex.scale.y)
            .arg(tex.offset.x)
            .arg(tex.offset.y)
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
void AssImpMeshExporter::embedTexturesInScene(
    aiScene* scene,
    const TexturePackage& texturePackage)
{
    if (!scene)
    {
        logError("Scene is null");
        return;
    }

    logMessage("Step: Embedding textures into scene...");

    std::vector<aiTexture*> textures;
    std::set<QString> processedPaths;  // Avoid duplicates

    // Define texture types to check
    const aiTextureType texTypes[] = {
        aiTextureType_BASE_COLOR,
        aiTextureType_NORMALS,
        aiTextureType_METALNESS,
        aiTextureType_DIFFUSE_ROUGHNESS,
        aiTextureType_LIGHTMAP,
        aiTextureType_EMISSIVE,
        aiTextureType_TRANSMISSION,
        aiTextureType_OPACITY,
        aiTextureType_HEIGHT,
        aiTextureType_CLEARCOAT,
        aiTextureType_SHEEN,
		aiTextureType_SPECULAR,
    };

    // Iterate through all materials and collect unique texture paths
    for (unsigned int matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx)
    {
        aiMaterial* mat = scene->mMaterials[matIdx];
        if (!mat) continue;

        for (aiTextureType texType : texTypes)
        {
            aiString texPath;
            if (mat->GetTexture(texType, 0, &texPath) == aiReturn_SUCCESS)
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
                QString candidate = _currentSettings.outputDirectory + "/textures/" + fi.fileName();
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