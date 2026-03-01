#ifndef GLTF_POSTPROCESSOR_H
#define GLTF_POSTPROCESSOR_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QString>
#include <vector>

#include "GLLights.h"

class TriangleMesh; // Forward declaration
class GLMaterial;   // Forward declaration

/**
 * Post-process exported glTF JSON to add missing optional properties
 *
 * This ensures exported files are complete and robust, avoiding undefined
 * behavior in various glTF importers (especially Assimp in release builds).
 *
 * The processor adds:
 * - texCoord: 0 (default) to all texture references
 * - scale: 1.0 to normalTexture
 * - strength: 1.0 to occlusionTexture
 * - Explicit filter and wrap modes to all samplers
 * - Missing properties in KHR_texture_transform extensions (offset, rotation, scale)
 * - KHR_texture_transform to extensionsUsed if transforms are present
 */
class GltfPostProcessor
{
public:
    /**
     * Post-process a glTF JSON object to add missing optional properties
     *
     * @param gltfJson The glTF JSON object to process (modified in place)
     * @param logCallback Optional callback for logging (receives QString messages)
     * @return true if any modifications were made
     */
    static bool postProcessGltfJson(QJsonObject& gltfJson,
        std::function<void(const QString&)> logCallback = nullptr);

    /**
     * Post-process a glTF JSON object with actual texture transforms from source materials
     *
     * This version writes the actual transform values from the source materials,
     * not just identity defaults. This is necessary because Assimp's glTF exporter
     * doesn't convert AI_MATKEY_UVTRANSFORM to KHR_texture_transform.
     *
     * @param gltfJson The glTF JSON object to process (modified in place)
     * @param meshes Source mesh objects with material data
     * @param logCallback Optional callback for logging
     * @return true if any modifications were made
     */
    static bool postProcessGltfJsonWithMaterials(
        QJsonObject& gltfJson,
        const std::vector<TriangleMesh*>& meshes,
        const std::vector<GPULight>& lights,
        std::function<void(const QString&)> logCallback = nullptr,
        const QString& textureSubfolder = "textures",
        const QMap<QString, QString>& pathMapping = {});

    /**
     * Post-process a glTF file on disk
     *
     * @param filePath Path to the .gltf file (not .glb)
     * @param logCallback Optional callback for logging
     * @return true if successful
     */
    static bool postProcessGltfFile(const QString& filePath,
        std::function<void(const QString&)> logCallback = nullptr);

    /**
     * Post-process a glTF file with actual texture transform data from source materials
     *
     * @param filePath Path to the .gltf file
     * @param meshes Source mesh objects with material data
     * @param logCallback Optional callback for logging
     * @return true if successful
     */
    static bool postProcessGltfFileWithMaterials(
        const QString& filePath,
        const std::vector<TriangleMesh*>& meshes,
        const std::vector<GPULight>& lights,
        std::function<void(const QString&)> logCallback = nullptr,
        const QString& textureSubfolder = "textures",
        const QMap<QString, QString>& pathMapping = {});

    /**
     * Post-process a GLB file on disk
     *
     * Reads the GLB, extracts JSON, post-processes it, and rewrites the GLB
     *
     * @param filePath Path to the .glb file
     * @param logCallback Optional callback for logging
     * @return true if successful
     */
    static bool postProcessGlbFile(const QString& filePath,
        std::function<void(const QString&)> logCallback = nullptr);

    /**
     * Post-process a GLB file with actual texture transform data from source materials
     *
     * @param filePath Path to the .glb file
     * @param meshes Source mesh objects with material data
     * @param logCallback Optional callback for logging
     * @return true if successful
     */
    static bool postProcessGlbFileWithMaterials(
        const QString& filePath,
        const std::vector<TriangleMesh*>& meshes,
        const std::vector<GPULight>& lights,
        std::function<void(const QString&)> logCallback = nullptr,
        const QString& textureSubfolder = "textures",
        const QMap<QString, QString>& pathMapping = {});

	/**
	 * Write punctual light definitions from the source scene into the glTF JSON
	 *
	 * This adds a KHR_lights_punctual extension with light definitions based on the provided GPULight data.
	 */
    static bool writePunctualLights(
        QJsonObject& gltfJson,
        const std::vector<GPULight>& lights,
        std::function<void(const QString&)> logCallback = nullptr);

    static bool removeTangentAttributes(
        QJsonObject& gltfJson,
        std::function<void(const QString&)> logCallback = nullptr);

    /**
    * Return a normalized path for the GLB file, ensuring consistent formatting across platforms
     * This is important for the patchGlbImageNames function to correctly locate the GLB file
     * and update its embedded texture names.
     *
     * @param path The original file path to the GLB file
     * @return Normalized file path suitable for GLB patching
    */
    static QString normalisedGlbPath(const QString& path);

private:
    static bool fixTextureInfoWithTransforms(QJsonObject& parent, const QString& key);
    static bool fixNormalTextureInfo(QJsonObject& parent, const QString& key);
    static bool fixOcclusionTextureInfo(QJsonObject& parent, const QString& key);
    static bool fixNormalTextureScale(
        QJsonObject& gltfJson,
        const std::vector<TriangleMesh*>& meshes,
        std::function<void(const QString&)> logCallback);
    static bool fixSpecularExtension(
        QJsonObject& gltfJson,
        const std::vector<TriangleMesh*>& meshes,
        std::function<void(const QString&)> logCallback = nullptr);
    static bool fixMetallicFactor(
        QJsonObject& gltfJson,
        const std::vector<TriangleMesh*>& meshes,
        std::function<void(const QString&)> logCallback = nullptr);
    static bool fixSpecularTextures(
        QJsonObject& gltfJson,
        const std::vector<TriangleMesh*>& meshes,
        std::function<void(const QString&)> logCallback = nullptr);
    static int findOrCreateTexture(
        QJsonObject& gltfJson,
        const QString& imagePath,
        std::function<void(const QString&)> logCallback = nullptr);

    static void log(const QString& message, std::function<void(const QString&)> callback);

private:
    static QString _textureSubfolder;
    static QMap<QString, QString> _pathMapping;
};

#endif // GLTF_POSTPROCESSOR_H