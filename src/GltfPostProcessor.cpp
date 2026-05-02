#include "GltfPostProcessor.h"
#include "GLMaterial.h"
#include "TriangleMesh.h"
#include <QDebug>
#include <QJsonParseError>
#include <QMap>

#include <glm/gtc/quaternion.hpp>

// Default texture subfolder name - overridden per export by postProcessGltfJsonWithMaterials
QString GltfPostProcessor::_textureSubfolder = "textures";
QMap<QString, QString> GltfPostProcessor::_pathMapping;
QMap<QString, int> GltfPostProcessor::_embeddedIndexMapping;
QMap<int, int> GltfPostProcessor::_materialToSourceMeshIndex;

namespace
{
QString resolvePackagedPath(const QMap<QString, QString>& pathMapping, const QString& sourcePath)
{
    auto it = pathMapping.find(sourcePath);
    if (it != pathMapping.end() && !it.value().isEmpty())
        return it.value();

    QString normalisedPath = sourcePath;
    if (sourcePath.startsWith("glb://") && sourcePath.contains("::"))
        normalisedPath = "glb://" + sourcePath.mid(sourcePath.lastIndexOf("::") + 2);

    if (normalisedPath != sourcePath)
    {
        it = pathMapping.find(normalisedPath);
        if (it != pathMapping.end())
            return it.value();
    }

    return {};
}
}

void GltfPostProcessor::log(const QString& message, std::function<void(const QString&)> callback)
{
    if (callback)
    {
        callback(message);
    }
    else
    {
        qDebug() << message;
    }
}

const TriangleMesh* GltfPostProcessor::sourceMeshForMaterial(
    int materialIndex,
    const std::vector<TriangleMesh*>& meshes)
{
    auto mappedIt = _materialToSourceMeshIndex.find(materialIndex);
    if (mappedIt != _materialToSourceMeshIndex.end())
    {
        int meshIndex = mappedIt.value();
        if (meshIndex >= 0 && meshIndex < static_cast<int>(meshes.size()))
            return meshes[meshIndex];
    }

    if (materialIndex >= 0 && materialIndex < static_cast<int>(meshes.size()))
        return meshes[materialIndex];

    return nullptr;
}

bool GltfPostProcessor::postProcessGltfJson(QJsonObject& gltfJson,
    std::function<void(const QString&)> logCallback)
{
    bool modified = false;
    bool hasTextureTransforms = false;

    log("=== glTF Post-Processor ===", logCallback);

    // Helper to check if a texture has transforms
    auto hasTransforms = [](const QJsonObject& obj) -> bool {
        if (obj.contains("extensions"))
        {
            QJsonObject ext = obj["extensions"].toObject();
            return ext.contains("KHR_texture_transform");
        }
        return false;
        };

    // 1. Fix materials
    if (gltfJson.contains("materials"))
    {
        QJsonArray materials = gltfJson["materials"].toArray();
        bool materialsModified = false;

        for (int i = 0; i < materials.size(); ++i)
        {
            QJsonObject mat = materials[i].toObject();
            bool matModified = false;

            // Fix PBR metallic-roughness textures
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();

                // Check for transforms
                if (pbr.contains("baseColorTexture"))
                {
                    if (hasTransforms(pbr["baseColorTexture"].toObject()))
                        hasTextureTransforms = true;
                    if (fixTextureInfoWithTransforms(pbr, "baseColorTexture"))
                        matModified = true;
                }

                if (pbr.contains("metallicRoughnessTexture"))
                {
                    if (hasTransforms(pbr["metallicRoughnessTexture"].toObject()))
                        hasTextureTransforms = true;
                    if (fixTextureInfoWithTransforms(pbr, "metallicRoughnessTexture"))
                        matModified = true;
                }

                if (matModified)
                    mat["pbrMetallicRoughness"] = pbr;
            }

            // Fix normal texture (has scale + transforms)
            if (mat.contains("normalTexture"))
            {
                if (hasTransforms(mat["normalTexture"].toObject()))
                    hasTextureTransforms = true;
                if (fixNormalTextureInfo(mat, "normalTexture"))
                    matModified = true;
            }

            // Fix occlusion texture (has strength + transforms)
            if (mat.contains("occlusionTexture"))
            {
                if (hasTransforms(mat["occlusionTexture"].toObject()))
                    hasTextureTransforms = true;
                if (fixOcclusionTextureInfo(mat, "occlusionTexture"))
                    matModified = true;
            }

            // Fix emissive texture (transforms only)
            if (mat.contains("emissiveTexture"))
            {
                if (hasTransforms(mat["emissiveTexture"].toObject()))
                    hasTextureTransforms = true;
                if (fixTextureInfoWithTransforms(mat, "emissiveTexture"))
                    matModified = true;
            }

            // Fix extension textures if present
            if (mat.contains("extensions"))
            {
                QJsonObject extensions = mat["extensions"].toObject();
                bool extModified = false;

                // KHR_materials_clearcoat
                if (extensions.contains("KHR_materials_clearcoat"))
                {
                    QJsonObject clearcoat = extensions["KHR_materials_clearcoat"].toObject();

                    if (clearcoat.contains("clearcoatTexture"))
                    {
                        if (hasTransforms(clearcoat["clearcoatTexture"].toObject()))
                            hasTextureTransforms = true;
                        if (fixTextureInfoWithTransforms(clearcoat, "clearcoatTexture"))
                            extModified = true;
                    }

                    if (clearcoat.contains("clearcoatRoughnessTexture"))
                    {
                        if (hasTransforms(clearcoat["clearcoatRoughnessTexture"].toObject()))
                            hasTextureTransforms = true;
                        if (fixTextureInfoWithTransforms(clearcoat, "clearcoatRoughnessTexture"))
                            extModified = true;
                    }

                    if (clearcoat.contains("clearcoatNormalTexture"))
                    {
                        if (hasTransforms(clearcoat["clearcoatNormalTexture"].toObject()))
                            hasTextureTransforms = true;
                        if (fixTextureInfoWithTransforms(clearcoat, "clearcoatNormalTexture"))
                            extModified = true;
                    }

                    if (extModified)
                        extensions["KHR_materials_clearcoat"] = clearcoat;
                }

                // KHR_materials_transmission
                if (extensions.contains("KHR_materials_transmission"))
                {
                    QJsonObject transmission = extensions["KHR_materials_transmission"].toObject();

                    if (transmission.contains("transmissionTexture"))
                    {
                        if (hasTransforms(transmission["transmissionTexture"].toObject()))
                            hasTextureTransforms = true;
                        if (fixTextureInfoWithTransforms(transmission, "transmissionTexture"))
                            extModified = true;
                    }

                    if (extModified)
                        extensions["KHR_materials_transmission"] = transmission;
                }

                // KHR_materials_sheen
                if (extensions.contains("KHR_materials_sheen"))
                {
                    QJsonObject sheen = extensions["KHR_materials_sheen"].toObject();

                    if (sheen.contains("sheenColorTexture"))
                    {
                        if (hasTransforms(sheen["sheenColorTexture"].toObject()))
                            hasTextureTransforms = true;
                        if (fixTextureInfoWithTransforms(sheen, "sheenColorTexture"))
                            extModified = true;
                    }

                    if (sheen.contains("sheenRoughnessTexture"))
                    {
                        if (hasTransforms(sheen["sheenRoughnessTexture"].toObject()))
                            hasTextureTransforms = true;
                        if (fixTextureInfoWithTransforms(sheen, "sheenRoughnessTexture"))
                            extModified = true;
                    }

                    if (extModified)
                        extensions["KHR_materials_sheen"] = sheen;
                }

                if (extModified)
                {
                    mat["extensions"] = extensions;
                    matModified = true;
                }
            }

            if (matModified)
            {
                materials[i] = mat;
                materialsModified = true;
                log(QString("  Fixed material %1: %2").arg(i).arg(mat["name"].toString()), logCallback);
            }
        }

        if (materialsModified)
        {
            gltfJson["materials"] = materials;
            modified = true;
        }
    }

    // 1b. Ensure KHR_texture_transform is in extensionsUsed if we found transforms
    if (hasTextureTransforms)
    {
        QJsonArray extensionsUsed;
        if (gltfJson.contains("extensionsUsed"))
        {
            extensionsUsed = gltfJson["extensionsUsed"].toArray();
        }

        // Check if KHR_texture_transform is already listed
        bool hasExtension = false;
        for (const QJsonValue& val : extensionsUsed)
        {
            if (val.toString() == "KHR_texture_transform")
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension)
        {
            extensionsUsed.append("KHR_texture_transform");
            gltfJson["extensionsUsed"] = extensionsUsed;
            modified = true;
            log("  Added KHR_texture_transform to extensionsUsed", logCallback);
        }
    }

    // 2. Fix samplers - only fill in missing properties for existing samplers
    if (gltfJson.contains("samplers"))
    {
        QJsonArray samplers = gltfJson["samplers"].toArray();
        bool samplersModified = false;
        int fixedCount = 0;

        for (int i = 0; i < samplers.size(); ++i)
        {
            QJsonObject sampler = samplers[i].toObject();
            bool thisFixed = false;

            // Add missing filter properties
            if (!sampler.contains("magFilter"))
            {
                sampler["magFilter"] = 9729; // GL_LINEAR
                thisFixed = true;
            }

            if (!sampler.contains("minFilter"))
            {
                sampler["minFilter"] = 9987; // GL_LINEAR_MIPMAP_LINEAR
                thisFixed = true;
            }

            // Add missing wrap properties
            if (!sampler.contains("wrapS"))
            {
                sampler["wrapS"] = 10497; // GL_REPEAT
                thisFixed = true;
            }

            if (!sampler.contains("wrapT"))
            {
                sampler["wrapT"] = 10497; // GL_REPEAT
                thisFixed = true;
            }

            if (thisFixed)
            {
                samplers[i] = sampler;
                samplersModified = true;
                fixedCount++;
            }
        }

        if (samplersModified)
        {
            gltfJson["samplers"] = samplers;
            modified = true;
            log(QString("  Fixed %1 sampler(s) with missing properties").arg(fixedCount), logCallback);
        }
    }

    if (modified)
    {
        log("  Post-processing complete: Fixed missing properties", logCallback);
    }
    else
    {
        log("  No modifications needed", logCallback);
    }

    return modified;
}

bool GltfPostProcessor::postProcessGltfFile(const QString& filePath,
    std::function<void(const QString&)> logCallback)
{
    // Read the file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        log(QString("Failed to open glTF file for reading: %1").arg(filePath), logCallback);
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        log(QString("Failed to parse glTF JSON: %1").arg(parseError.errorString()), logCallback);
        return false;
    }

    if (!doc.isObject())
    {
        log("glTF JSON root is not an object", logCallback);
        return false;
    }

    // Post-process
    QJsonObject gltfJson = doc.object();
    bool modified = postProcessGltfJson(gltfJson, logCallback);

    if (!modified)
    {
        log(QString("No modifications needed for: %1").arg(filePath), logCallback);
        return true;
    }

    // Write back
    doc.setObject(gltfJson);

    if (!file.open(QIODevice::WriteOnly))
    {
        log(QString("Failed to open glTF file for writing: %1").arg(filePath), logCallback);
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    log(QString("Successfully post-processed: %1").arg(filePath), logCallback);
    return true;
}

bool GltfPostProcessor::postProcessGlbFile(const QString& filePath,
    std::function<void(const QString&)> logCallback)
{
    // Read GLB file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        log(QString("Failed to open GLB file for reading: %1").arg(filePath), logCallback);
        return false;
    }

    QByteArray glbData = file.readAll();
    file.close();

    if (glbData.size() < 12)
    {
        log("GLB file too small", logCallback);
        return false;
    }

    // Parse GLB header
    const char* data = glbData.constData();
    quint32 magic = *reinterpret_cast<const quint32*>(data);
    quint32 version = *reinterpret_cast<const quint32*>(data + 4);
    quint32 length = *reinterpret_cast<const quint32*>(data + 8);

    if (magic != 0x46546C67) // "glTF"
    {
        log("Invalid GLB magic number", logCallback);
        return false;
    }

    if (version != 2)
    {
        log("Only glTF 2.0 GLB files are supported", logCallback);
        return false;
    }

    // Read JSON chunk
    int offset = 12;
    if (offset + 8 > glbData.size())
    {
        log("GLB file truncated (no JSON chunk)", logCallback);
        return false;
    }

    quint32 jsonChunkLength = *reinterpret_cast<const quint32*>(data + offset);
    quint32 jsonChunkType = *reinterpret_cast<const quint32*>(data + offset + 4);

    if (jsonChunkType != 0x4E4F534A) // "JSON"
    {
        log("Invalid JSON chunk type", logCallback);
        return false;
    }

    offset += 8;
    if (offset + jsonChunkLength > static_cast<quint32>(glbData.size()))
    {
        log("GLB file truncated (JSON chunk too large)", logCallback);
        return false;
    }

    QByteArray jsonData = glbData.mid(offset, jsonChunkLength);
    offset += jsonChunkLength;

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        log(QString("Failed to parse GLB JSON: %1").arg(parseError.errorString()), logCallback);
        return false;
    }

    if (!doc.isObject())
    {
        log("GLB JSON root is not an object", logCallback);
        return false;
    }

    // Post-process JSON
    QJsonObject gltfJson = doc.object();
    bool modified = postProcessGltfJson(gltfJson, logCallback);

    if (!modified)
    {
        log(QString("No modifications needed for: %1").arg(filePath), logCallback);
        return true;
    }

    // Reconstruct GLB with modified JSON
    doc.setObject(gltfJson);
    QByteArray newJsonData = doc.toJson(QJsonDocument::Compact);

    // Pad JSON to 4-byte boundary
    while (newJsonData.size() % 4 != 0)
    {
        newJsonData.append(' ');
    }

    // Build new GLB
    QByteArray newGlbData;
    quint32 newJsonLength = newJsonData.size();
    quint32 newLength = 12 + 8 + newJsonLength;

    // Copy binary chunks (if any)
    QByteArray binaryData;
    if (offset < glbData.size())
    {
        binaryData = glbData.mid(offset);
        newLength += binaryData.size();
    }

    // Write header
    newGlbData.append(reinterpret_cast<const char*>(&magic), 4);
    newGlbData.append(reinterpret_cast<const char*>(&version), 4);
    newGlbData.append(reinterpret_cast<const char*>(&newLength), 4);

    // Write JSON chunk
    quint32 jsonType = 0x4E4F534A; // "JSON"
    newGlbData.append(reinterpret_cast<const char*>(&newJsonLength), 4);
    newGlbData.append(reinterpret_cast<const char*>(&jsonType), 4);
    newGlbData.append(newJsonData);

    // Write binary chunks
    if (!binaryData.isEmpty())
    {
        newGlbData.append(binaryData);
    }

    // Write back to file
    if (!file.open(QIODevice::WriteOnly))
    {
        log(QString("Failed to open GLB file for writing: %1").arg(filePath), logCallback);
        return false;
    }

    file.write(newGlbData);
    file.close();

    log(QString("Successfully post-processed GLB: %1").arg(filePath), logCallback);
    return true;
}

bool GltfPostProcessor::postProcessGltfFileWithMaterials(
    const QString& filePath,
    const std::vector<TriangleMesh*>& meshes,
    const std::vector<GPULight>& lights,
    std::function<void(const QString&)> logCallback,
    const QString& textureSubfolder,
    const QMap<QString, QString>& pathMapping,
    const QMap<QString, int>& embeddedIndexMapping)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        log(QString("Failed to open glTF file: %1").arg(filePath), logCallback);
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        log("Failed to parse glTF JSON", logCallback);
        return false;
    }

    QJsonObject gltfJson = doc.object();

    // Process with material transforms
    postProcessGltfJsonWithMaterials(gltfJson, meshes, lights, logCallback, textureSubfolder, pathMapping, embeddedIndexMapping);

    doc.setObject(gltfJson);

    if (!file.open(QIODevice::WriteOnly))
    {
        log(QString("Failed to write glTF file: %1").arg(filePath), logCallback);
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    log(QString("Successfully post-processed with materials: %1").arg(filePath), logCallback);
    return true;
}

bool GltfPostProcessor::postProcessGlbFileWithMaterials(
    const QString& filePath,
    const std::vector<TriangleMesh*>& meshes,
    const std::vector<GPULight>& lights,
    std::function<void(const QString&)> logCallback,
    const QString& textureSubfolder,
    const QMap<QString, QString>& pathMapping,
    const QMap<QString, int>& embeddedIndexMapping)
{
    // Read GLB file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        log(QString("Failed to open GLB file for reading: %1").arg(filePath), logCallback);
        return false;
    }

    QByteArray glbData = file.readAll();
    file.close();

    if (glbData.size() < 12)
    {
        log("GLB file too small", logCallback);
        return false;
    }

    // Parse GLB header
    const char* data = glbData.constData();
    quint32 magic = *reinterpret_cast<const quint32*>(data);
    quint32 version = *reinterpret_cast<const quint32*>(data + 4);
    quint32 length = *reinterpret_cast<const quint32*>(data + 8);

    if (magic != 0x46546C67) // "glTF"
    {
        log("Invalid GLB magic number", logCallback);
        return false;
    }

    if (version != 2)
    {
        log("Only glTF 2.0 GLB files are supported", logCallback);
        return false;
    }

    // Read JSON chunk
    int offset = 12;
    if (offset + 8 > glbData.size())
    {
        log("GLB file truncated (no JSON chunk)", logCallback);
        return false;
    }

    quint32 jsonChunkLength = *reinterpret_cast<const quint32*>(data + offset);
    quint32 jsonChunkType = *reinterpret_cast<const quint32*>(data + offset + 4);

    if (jsonChunkType != 0x4E4F534A) // "JSON"
    {
        log("Invalid JSON chunk type", logCallback);
        return false;
    }

    offset += 8;
    if (offset + jsonChunkLength > static_cast<quint32>(glbData.size()))
    {
        log("GLB file truncated (JSON chunk too large)", logCallback);
        return false;
    }

    QByteArray jsonData = glbData.mid(offset, jsonChunkLength);
    offset += jsonChunkLength;

    // Parse JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        log(QString("Failed to parse GLB JSON: %1").arg(parseError.errorString()), logCallback);
        return false;
    }

    if (!doc.isObject())
    {
        log("GLB JSON root is not an object", logCallback);
        return false;
    }

    QJsonObject gltfJson = doc.object();

    // Post-process with material transforms
    postProcessGltfJsonWithMaterials(gltfJson, meshes, lights, logCallback, textureSubfolder, pathMapping, embeddedIndexMapping);

    // Reconstruct GLB with modified JSON
    doc.setObject(gltfJson);
    QByteArray newJsonData = doc.toJson(QJsonDocument::Compact);

    // Pad JSON to 4-byte boundary
    while (newJsonData.size() % 4 != 0)
    {
        newJsonData.append(' ');
    }

    // Build new GLB
    QByteArray newGlbData;
    quint32 newJsonLength = newJsonData.size();
    quint32 newLength = 12 + 8 + newJsonLength;

    // Copy binary chunks (if any) - this preserves embedded textures
    QByteArray binaryData;
    if (offset < glbData.size())
    {
        binaryData = glbData.mid(offset);
        newLength += binaryData.size();
    }

    // Write header
    newGlbData.append(reinterpret_cast<const char*>(&magic), 4);
    newGlbData.append(reinterpret_cast<const char*>(&version), 4);
    newGlbData.append(reinterpret_cast<const char*>(&newLength), 4);

    // Write JSON chunk
    quint32 jsonType = 0x4E4F534A; // "JSON"
    newGlbData.append(reinterpret_cast<const char*>(&newJsonLength), 4);
    newGlbData.append(reinterpret_cast<const char*>(&jsonType), 4);
    newGlbData.append(newJsonData);

    // Write binary chunks (includes embedded textures)
    if (!binaryData.isEmpty())
    {
        newGlbData.append(binaryData);
    }

    // Write back to file
    if (!file.open(QIODevice::WriteOnly))
    {
        log(QString("Failed to open GLB file for writing: %1").arg(filePath), logCallback);
        return false;
    }

    file.write(newGlbData);
    file.close();

    log(QString("Successfully post-processed GLB with materials: %1").arg(filePath), logCallback);
    return true;
}

bool GltfPostProcessor::writePunctualLights(
    QJsonObject& gltfJson,
    const std::vector<GPULight>& lights,
    std::function<void(const QString&)> logCallback)
{
    if (lights.empty())
        return false;

    log(QString("Writing %1 punctual light(s)...").arg(lights.size()), logCallback);

    QJsonArray lightsArray;
    QJsonArray nodesArray = gltfJson.value("nodes").toArray();

    for (size_t i = 0; i < lights.size(); ++i)
    {
        const GPULight& light = lights[i];

        // --- Determine type string ---
        QString typeStr;
        LightType lt = static_cast<LightType>(light.type);
        if (lt == LightType::Directional) typeStr = "directional";
        else if (lt == LightType::Point)       typeStr = "point";
        else if (lt == LightType::Spot)        typeStr = "spot";
        else
        {
            log(QString("  Light %1: unknown type %2, skipping").arg(i).arg(light.type), logCallback);
            continue;
        }

        // --- Build light definition ---
        QJsonObject lightDef;
        lightDef["type"] = typeStr;
        lightDef["name"] = QString("light_%1").arg(i);
        lightDef["intensity"] = static_cast<double>(light.intensity);

        // Color (only write if non-white)
        if (std::abs(light.color.x - 1.0f) > 0.001f ||
            std::abs(light.color.y - 1.0f) > 0.001f ||
            std::abs(light.color.z - 1.0f) > 0.001f)
        {
            QJsonArray colorArr;
            colorArr.append(static_cast<double>(light.color.x));
            colorArr.append(static_cast<double>(light.color.y));
            colorArr.append(static_cast<double>(light.color.z));
            lightDef["color"] = colorArr;
        }

        // Range (omit for directional or infinite/zero range)
        if (lt != LightType::Directional && light.range > 0.0f)
            lightDef["range"] = static_cast<double>(light.range);

        // Spot cone angles (stored as cosines in GPULight, convert back to radians)
        if (lt == LightType::Spot)
        {
            float innerAngle = std::acos(glm::clamp(light.innerConeCos, -1.0f, 1.0f));
            float outerAngle = std::acos(glm::clamp(light.outerConeCos, -1.0f, 1.0f));
            QJsonObject spotDef;
            spotDef["innerConeAngle"] = static_cast<double>(innerAngle);
            spotDef["outerConeAngle"] = static_cast<double>(outerAngle);
            lightDef["spot"] = spotDef;
        }

        int lightIndex = lightsArray.size();
        lightsArray.append(lightDef);

        // --- Build a node for this light ---
        // glTF lights live on nodes: position via translation, direction via rotation.
        // Default light direction in glTF is -Z (0,0,-1).
        QJsonObject lightNode;
        lightNode["name"] = QString("light_node_%1").arg(i);

        // Translation
        QJsonArray translation;
        translation.append(static_cast<double>(light.position.x));
        translation.append(static_cast<double>(light.position.y));
        translation.append(static_cast<double>(light.position.z));
        lightNode["translation"] = translation;

        // Rotation: from glTF default (-Z) to actual light direction
        glm::vec3 defaultDir(0.0f, 0.0f, -1.0f);
        glm::vec3 dir = glm::normalize(light.direction);
        float dot = glm::dot(defaultDir, dir);

        glm::quat rot;
        if (dot >= 1.0f - 1e-6f)
        {
            rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // identity - already pointing -Z
        }
        else if (dot <= -1.0f + 1e-6f)
        {
            // Exactly opposite - 180� around any perpendicular axis
            rot = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else
        {
            glm::vec3 axis = glm::normalize(glm::cross(defaultDir, dir));
            float angle = std::acos(dot);
            rot = glm::angleAxis(angle, axis);
        }
        QJsonArray rotation;
        rotation.append(static_cast<double>(rot.x));
        rotation.append(static_cast<double>(rot.y));
        rotation.append(static_cast<double>(rot.z));
        rotation.append(static_cast<double>(rot.w));
        lightNode["rotation"] = rotation;

        // KHR_lights_punctual reference on the node
        QJsonObject nodeExt;
        QJsonObject lightRef;
        lightRef["light"] = lightIndex;
        nodeExt["KHR_lights_punctual"] = lightRef;
        lightNode["extensions"] = nodeExt;

        int nodeIndex = nodesArray.size();
        nodesArray.append(lightNode);

        log(QString("  -> %1 light '%2' at node %3")
            .arg(typeStr).arg(lightDef["name"].toString()).arg(nodeIndex), logCallback);
    }

    if (lightsArray.isEmpty())
        return false;

    // --- Write lights array to top-level extensions ---
    QJsonObject topExtensions = gltfJson.value("extensions").toObject();
    QJsonObject khrLights;
    khrLights["lights"] = lightsArray;
    topExtensions["KHR_lights_punctual"] = khrLights;
    gltfJson["extensions"] = topExtensions;

    // --- Write updated nodes array ---
    gltfJson["nodes"] = nodesArray;

    // --- Add light nodes to the first scene ---
    if (gltfJson.contains("scenes"))
    {
        QJsonArray scenes = gltfJson["scenes"].toArray();
        if (!scenes.isEmpty())
        {
            QJsonObject scene = scenes[0].toObject();
            QJsonArray sceneNodes = scene.value("nodes").toArray();
            int firstLightNode = nodesArray.size() - static_cast<int>(lightsArray.size());
            for (int i = firstLightNode; i < nodesArray.size(); ++i)
                sceneNodes.append(i);
            scene["nodes"] = sceneNodes;
            scenes[0] = scene;
            gltfJson["scenes"] = scenes;
        }
    }

    // --- Add KHR_lights_punctual to extensionsUsed ---
    QJsonArray extensionsUsed = gltfJson.value("extensionsUsed").toArray();
    bool listed = false;
    for (const QJsonValue& v : extensionsUsed)
        if (v.toString() == "KHR_lights_punctual") { listed = true; break; }
    if (!listed)
    {
        extensionsUsed.append("KHR_lights_punctual");
        gltfJson["extensionsUsed"] = extensionsUsed;
    }

    log(QString("  -> KHR_lights_punctual written (%1 lights)").arg(lightsArray.size()), logCallback);
    return true;
}

// Remove TANGENT attributes from all mesh primitives
// This works around an Assimp bug where glTF files with TANGENT attribute
// fail to load correctly in release builds (UVs get corrupted)
bool GltfPostProcessor::removeTangentAttributes(
    QJsonObject& gltfJson,
    std::function<void(const QString&)> logCallback)
{
    bool modified = false;

    if (!gltfJson.contains("meshes"))
        return false;

    QJsonArray meshes = gltfJson["meshes"].toArray();

    for (int i = 0; i < meshes.size(); ++i)
    {
        QJsonObject mesh = meshes[i].toObject();

        if (!mesh.contains("primitives"))
            continue;

        QJsonArray primitives = mesh["primitives"].toArray();

        for (int j = 0; j < primitives.size(); ++j)
        {
            QJsonObject prim = primitives[j].toObject();

            if (!prim.contains("attributes"))
                continue;

            QJsonObject attributes = prim["attributes"].toObject();

            // Remove TANGENT attribute if present
            if (attributes.contains("TANGENT"))
            {
                attributes.remove("TANGENT");
                prim["attributes"] = attributes;
                primitives[j] = prim;
                modified = true;

                log(QString("  -> Removed TANGENT from mesh %1, primitive %2")
                    .arg(i).arg(j), logCallback);
            }
        }

        if (modified)
        {
            mesh["primitives"] = primitives;
            meshes[i] = mesh;
        }
    }

    if (modified)
    {
        gltfJson["meshes"] = meshes;
        log("  -> TANGENT attributes removed from glTF", logCallback);
    }

    return modified;
}

// ============================================================================
// Material Signature Helper Functions
// ============================================================================

QString GltfPostProcessor::MaterialSignature::computeHash() const
{
    QString hash = name;

    // Include complete texture binding information (order matters for hash consistency)
    // Sort by texture type first, then path, to ensure consistent ordering
    std::vector<TextureBinding> sortedBindings = textureBindings;
    std::sort(sortedBindings.begin(), sortedBindings.end(),
        [](const TextureBinding& a, const TextureBinding& b) {
            if (a.textureType != b.textureType)
                return a.textureType < b.textureType;
            if (a.path != b.path)
                return a.path < b.path;
            return a.texCoordIndex < b.texCoordIndex;
        });

    // Hash each texture binding: type + path + texCoord + transforms
    for (const auto& binding : sortedBindings)
    {
        hash += QString("|%1::%2::%3:%4,%5:%6,%7:%8,%9")
            .arg(binding.textureType)                                    // texture type
            .arg(binding.path)                                           // texture path
            .arg(binding.texCoordIndex)                                  // texCoord index
            .arg(binding.rotationRad, 0, 'f', 4)                         // rotation
            .arg(binding.scale.x(), 0, 'f', 4)                           // scale X
            .arg(binding.scale.y(), 0, 'f', 4)                           // scale Y
            .arg(binding.offset.x(), 0, 'f', 4)                          // offset X
            .arg(binding.offset.y(), 0, 'f', 4)                          // offset Y
            .arg(binding.rotationRad > 0.001 ? "R" : "");                // has rotation flag
    }

    return hash;
}

GltfPostProcessor::MaterialSignature GltfPostProcessor::buildSignatureForMesh(
    int meshIdx,
    const TriangleMesh* mesh,
    std::function<void(const QString&)> logCallback)
{
    MaterialSignature sig;
    sig.meshIndex = meshIdx;
    const GLMaterial& glMat = mesh->getMaterial();
    sig.name = glMat.name();
    sig.originalMaterialIndex = mesh->getOriginalMaterialIndex();

    // Lambda to extract complete texture binding data (type + path + texCoord + transforms)
    auto extractTextureBinding = [&](GLMaterial::TextureType type, const QString& typeStr) {
        const GLMaterial::Texture& tex = glMat.texture(type);
        if (!tex.path.empty()) {
            QString texPath = QString::fromStdString(tex.path);
            sig.textureFilePaths.insert(texPath);

            // Store complete texture binding information
            MaterialSignature::TextureBinding binding;
            binding.textureType = typeStr;
            binding.path = texPath;
            binding.texCoordIndex = tex.texCoordIndex;      // CRITICAL: texture coordinate index
            binding.rotationRad = tex.rotation;
            binding.scale = QVector2D(tex.scale.x, tex.scale.y);
            binding.offset = QVector2D(tex.offset.x, tex.offset.y);
            sig.textureBindings.push_back(binding);
        }
    };

    // Collect texture bindings from all material texture slots
    // Each binding is uniquely identified by type + path + texCoord
    extractTextureBinding(GLMaterial::TextureType::Albedo, "Albedo");
    extractTextureBinding(GLMaterial::TextureType::Normal, "Normal");
    extractTextureBinding(GLMaterial::TextureType::Metallic, "Metallic");
    extractTextureBinding(GLMaterial::TextureType::Roughness, "Roughness");
    extractTextureBinding(GLMaterial::TextureType::AmbientOcclusion, "AmbientOcclusion");
    extractTextureBinding(GLMaterial::TextureType::Emissive, "Emissive");

    return sig;
}

int GltfPostProcessor::computeTextureMatchScore(
    const MaterialSignature& sig,
    const QJsonObject& jsonMat)
{
    int score = 0;

    // Check if material has textures defined
    bool hasPBR = jsonMat.contains("pbrMetallicRoughness");
    if (hasPBR)
    {
        QJsonObject pbr = jsonMat["pbrMetallicRoughness"].toObject();
        if (pbr.contains("baseColorTexture"))
            score += 30;
        if (pbr.contains("metallicRoughnessTexture"))
            score += 25;
    }

    if (jsonMat.contains("normalTexture"))
        score += 25;
    if (jsonMat.contains("occlusionTexture"))
        score += 15;

    // Count JSON textures
    int jsonTexCount = 0;
    if (hasPBR)
    {
        QJsonObject pbr = jsonMat["pbrMetallicRoughness"].toObject();
        if (pbr.contains("baseColorTexture")) jsonTexCount++;
        if (pbr.contains("metallicRoughnessTexture")) jsonTexCount++;
    }
    if (jsonMat.contains("normalTexture")) jsonTexCount++;
    if (jsonMat.contains("occlusionTexture")) jsonTexCount++;

    // Bonus if texture count roughly matches
    int sigTexCount = sig.textureFilePaths.size();
    if (sigTexCount > 0 && jsonTexCount > 0)
    {
        if (sigTexCount >= jsonTexCount && sigTexCount <= jsonTexCount + 1)
            score += 20;
    }

    return score;
}

int GltfPostProcessor::findMaterialBySignature(
    const QString& jsonMatName,
    const QJsonObject& jsonMat,
    const std::vector<MaterialSignature>& signatures,
    int matIdx,
    std::function<void(const QString&)> logCallback)
{
    if (signatures.empty())
        return -1;

    // Find all candidates with matching name
    QVector<int> candidates;
    for (int i = 0; i < static_cast<int>(signatures.size()); ++i)
    {
        if (signatures[i].name == jsonMatName)
            candidates.push_back(i);
    }

    if (candidates.empty())
        return -1;

    // Single candidate - easy decision
    if (candidates.size() == 1)
    {
        int idx = candidates[0];
        log(QString("  Material[%1] '%2': Unique name match (signature %3)")
            .arg(matIdx).arg(jsonMatName).arg(idx), logCallback);
        return idx;
    }

    // Multiple candidates - score by texture match
    int bestIdx = candidates[0];
    int bestScore = computeTextureMatchScore(signatures[bestIdx], jsonMat);

    for (int idx : candidates)
    {
        int score = computeTextureMatchScore(signatures[idx], jsonMat);
        if (score > bestScore)
        {
            bestScore = score;
            bestIdx = idx;
        }
    }

    log(QString("  Material[%1] '%2': Selected by fingerprint (signature %3, score=%4)")
        .arg(matIdx).arg(jsonMatName).arg(bestIdx).arg(bestScore), logCallback);
    return bestIdx;
}

int GltfPostProcessor::findMaterialByNameWithDedup(
    const QString& jsonMatName,
    const std::vector<MaterialSignature>& signatures,
    int matIdx,
    std::function<void(const QString&)> logCallback)
{
    int matchCount = 0;
    int firstSigIdx = -1;

    for (int i = 0; i < static_cast<int>(signatures.size()); ++i)
    {
        if (signatures[i].name == jsonMatName)
        {
            matchCount++;
            if (firstSigIdx == -1)
            {
                firstSigIdx = i;
            }
        }
    }

    if (matchCount == 0)
        return -1;

    if (firstSigIdx < 0)
        return -1;

    if (matchCount > 1)
    {
        log(QString("  Material[%1] '%2': WARNING %3 instances found, using first (signature %4)")
            .arg(matIdx).arg(jsonMatName).arg(matchCount).arg(firstSigIdx), logCallback);
    }
    else
    {
        log(QString("  Material[%1] '%2': Name-based match (signature %3)")
            .arg(matIdx).arg(jsonMatName).arg(firstSigIdx), logCallback);
    }

    return firstSigIdx;
}

int GltfPostProcessor::findMaterialByIndexFallback(
    int matIdx,
    const std::vector<MaterialSignature>& signatures,
    std::function<void(const QString&)> logCallback)
{
    for (int i = 0; i < static_cast<int>(signatures.size()); ++i)
    {
        if (signatures[i].originalMaterialIndex == matIdx)
        {
            log(QString("  Material[%1]: Index fallback match (original index, signature %2)")
                .arg(matIdx).arg(i), logCallback);
            return i;
        }
    }
    return -1;
}

// ============================================================================

bool GltfPostProcessor::postProcessGltfJsonWithMaterials(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    const std::vector<GPULight>& lights,
    std::function<void(const QString&)> logCallback,
    const QString& textureSubfolder,
    const QMap<QString, QString>& pathMapping,
    const QMap<QString, int>& embeddedIndexMapping)
{
    log("=== glTF Post-Processor (with material transforms) ===", logCallback);

    // Store for use by findOrCreateTexture throughout this processing pass
    _textureSubfolder = textureSubfolder.isEmpty() ? "textures" : textureSubfolder;
    _pathMapping = pathMapping;
    _embeddedIndexMapping = embeddedIndexMapping;
    _materialToSourceMeshIndex.clear();

    removeTangentAttributes(gltfJson, logCallback);

    // Track which JSON material indices have been patched, and by which source mesh
    // (a material may be shared by multiple meshes - first mesh wins)
    QMap<int, int> patchedMaterials;  // json material index -> source mesh index

    // First, write actual transforms from source materials
    if (gltfJson.contains("materials") && !meshes.empty())
    {
        QJsonArray materials = gltfJson["materials"].toArray();
        QJsonArray images;
        QJsonArray textures;
        QJsonArray samplers;

        if (gltfJson.contains("images"))
            images = gltfJson["images"].toArray();
        if (gltfJson.contains("textures"))
            textures = gltfJson["textures"].toArray();
        if (gltfJson.contains("samplers"))
            samplers = gltfJson["samplers"].toArray();

        // Clear Assimp's samplers - we'll create correct ones from source materials
        samplers = QJsonArray();

        // ===== MATERIAL IDENTITY-BASED MATCHING =====
        // NEW APPROACH: Use originalMaterialIndex as the authoritative identifier
        //
        // Instead of trying to infer which source mesh became which JSON mesh via name queues,
        // we build a direct map from originalMaterialIndex to source mesh properties.
        // This survives Assimp's reordering because it's based on semantic identity, not position.
        //
        // The key insight: during import, each mesh captures its originalMaterialIndex.
        // This is the true material identity. We use it to rebuild the correct material
        // assignments during export, bypassing deduplication ambiguity.

        // Build material identity map: originalMaterialIndex -> source mesh index with best match
        QMap<int, int> origMatIdxToMeshIdx;  // originalMaterialIndex -> preferred source mesh index

        for (int k = 0; k < static_cast<int>(meshes.size()); ++k)
        {
            if (!meshes[k]) continue;

            int origMatIdx = meshes[k]->getOriginalMaterialIndex();

            // For each originalMaterialIndex, keep track of a source mesh that uses it
            // Prefer the first mesh we encounter for each index
            if (origMatIdx >= 0 && !origMatIdxToMeshIdx.contains(origMatIdx))
            {
                origMatIdxToMeshIdx[origMatIdx] = k;
                log(QString("  Material Identity: originalMaterialIndex[%1] -> source mesh[%2] (%3)")
                    .arg(origMatIdx).arg(k).arg(meshes[k]->getName()), logCallback);
            }
        }

        // Build mesh name -> source mesh index map for fast lookup by name
        QMap<QString, QList<int>> meshNameToIndices;
        for (int k = 0; k < static_cast<int>(meshes.size()); ++k)
        {
            if (!meshes[k]) continue;
            QString fullName = meshes[k]->getName();
            if (!fullName.isEmpty())
            {
                meshNameToIndices[fullName].append(k);
            }
        }

        log(QString("  Material Identity Map: %1 unique material indices").arg(origMatIdxToMeshIdx.size()), logCallback);
        log(QString("  Mesh Name Lookup: %1 named mesh groups").arg(meshNameToIndices.size()), logCallback);
        log(QString("  [SOURCE MESHES] meshes.size()=%1").arg(meshes.size()), logCallback);

        // We may need to update the JSON meshes array (to redirect primitive material
        // pointers when we clone a collapsed material slot). Work on a mutable copy.
        QJsonArray jsonMeshes = gltfJson.value("meshes").toArray();
        log(QString("  [JSON MESHES] jsonMeshes.size()=%1").arg(jsonMeshes.size()), logCallback);

        auto resolveSourceMeshIndex = [&](const QString& targetName,
                                          int jsonMeshIndex,
                                          const QString& matchLabel) -> int
        {
            if (meshNameToIndices.contains(targetName))
            {
                const QList<int>& candidates = meshNameToIndices[targetName];
                if (!candidates.isEmpty())
                {
                    int resolved = candidates[0];
                    log(QString("  [%1] json mesh[%2] '%3' -> source mesh[%4] (exact match)")
                        .arg(matchLabel).arg(jsonMeshIndex).arg(targetName).arg(resolved), logCallback);
                    return resolved;
                }
            }

            for (auto it = meshNameToIndices.begin(); it != meshNameToIndices.end(); ++it)
            {
                const QString& sourceName = it.key();
                const QList<int>& candidates = it.value();
                if (!candidates.isEmpty() &&
                    (sourceName.contains(targetName) || targetName.contains(sourceName)))
                {
                    int resolved = candidates[0];
                    log(QString("  [%1] json mesh[%2] '%3' -> source mesh[%4] (partial: '%5')")
                        .arg(matchLabel).arg(jsonMeshIndex).arg(targetName).arg(resolved).arg(sourceName), logCallback);
                    return resolved;
                }
            }

            return -1;
        };

        for (int j = 0; j < jsonMeshes.size(); ++j)
        {
            QJsonObject jMesh = jsonMeshes[j].toObject();
            QString meshName = jMesh["name"].toString();

            QJsonArray primitives = jMesh["primitives"].toArray();
            if (primitives.isEmpty()) continue;

            log(QString("[MESH LOOP] Processing JSON mesh[%1] name='%2' primitives=%3")
                .arg(j).arg(meshName).arg(primitives.size()), logCallback);

            int baseMeshIdx = resolveSourceMeshIndex(meshName, j, "NAME MATCH");
            if (baseMeshIdx < 0)
            {
                log(QString("  WARNING: no source mesh found for json mesh '%1' (index %2)")
                    .arg(meshName).arg(j), logCallback);
                continue;
            }

            const int suffixSeparator = meshName.lastIndexOf('-');
            const QString primitiveNamePrefix = suffixSeparator >= 0 ? meshName.left(suffixSeparator + 1) : meshName;

            // ===== MATERIAL SLOT ASSIGNMENT =====
            // Assign each primitive independently. Assimp can collapse multiple source
            // meshes into one JSON mesh with several primitives, so primitive[0] and
            // primitive[1] may need different source material identities.
            for (int primIdx = 0; primIdx < primitives.size(); ++primIdx)
            {
                QJsonObject prim = primitives[primIdx].toObject();
                int matIdx = prim.value("material").toInt(-1);
                if (matIdx < 0 || matIdx >= materials.size())
                    continue;

                QString targetSourceName = meshName;
                int meshIdx = baseMeshIdx;

                if (primitives.size() > 1 && suffixSeparator >= 0)
                {
                    QString primitiveSpecificName = primitiveNamePrefix + QString::number(primIdx);
                    int primitiveMeshIdx = resolveSourceMeshIndex(
                        primitiveSpecificName, j, QString("PRIMITIVE %1").arg(primIdx));
                    if (primitiveMeshIdx >= 0)
                    {
                        targetSourceName = primitiveSpecificName;
                        meshIdx = primitiveMeshIdx;
                    }
                }

                // For consolidated meshes without suffix-based names (e.g. STEP/FBX files where
                // Assimp groups many source meshes into one JSON mesh with N primitives),
                // use sequential offset from baseMeshIdx. applyMaterialsToScene assigns
                // mMaterialIndex=i per source mesh, so primitive[k] was created from meshes[baseMeshIdx+k].
                if (primitives.size() > 1 && suffixSeparator < 0 && primIdx > 0)
                {
                    int candidateMeshIdx = baseMeshIdx + primIdx;
                    if (candidateMeshIdx < static_cast<int>(meshes.size()))
                    {
                        meshIdx = candidateMeshIdx;
                        log(QString("  [PRIM OFFSET] primitive[%1] -> source mesh[%2] (base=%3 + offset)")
                            .arg(primIdx).arg(meshIdx).arg(baseMeshIdx), logCallback);
                    }
                }

                if (meshIdx < 0 || meshIdx >= static_cast<int>(meshes.size()))
                    continue;

                int origMatIdx = meshes[meshIdx]->getOriginalMaterialIndex();
                log(QString("  [MATCH] json mesh[%1] primitive[%2] name='%3' material[%4] -> source mesh[%5] origMatIdx=%6 matName='%7'")
                    .arg(j).arg(primIdx).arg(targetSourceName).arg(matIdx).arg(meshIdx)
                    .arg(origMatIdx).arg(meshes[meshIdx]->getMaterial().name()), logCallback);

                if (!patchedMaterials.contains(matIdx))
                {
                    patchedMaterials[matIdx] = meshIdx;
                    log(QString("  Assigned: json mesh[%1] primitive[%2] '%3' -> material[%4] (origMatIdx=%5)")
                        .arg(j).arg(primIdx).arg(targetSourceName).arg(matIdx).arg(origMatIdx), logCallback);
                }
                else if (patchedMaterials[matIdx] == meshIdx)
                {
                    log(QString("  Shared: json mesh[%1] primitive[%2] '%3' -> material[%4] (same source mesh)")
                        .arg(j).arg(primIdx).arg(targetSourceName).arg(matIdx), logCallback);
                }
                else
                {
                    int existingMeshIdx = patchedMaterials[matIdx];
                    int clonedIdx = materials.size();
                    materials.append(materials[matIdx]);  // deep copy

                    prim["material"] = clonedIdx;
                    primitives[primIdx] = prim;
                    jMesh["primitives"] = primitives;
                    jsonMeshes[j] = jMesh;

                    patchedMaterials[clonedIdx] = meshIdx;
                    log(QString("  Cloned: json mesh[%1] primitive[%2] '%3' -> material[%4]→[%5] (origMatIdx=%6 vs existing origMatIdx=%7)")
                        .arg(j).arg(primIdx).arg(targetSourceName).arg(matIdx).arg(clonedIdx).arg(origMatIdx)
                        .arg(meshes[existingMeshIdx]->getOriginalMaterialIndex()), logCallback);
                }
            }  // end primitive material slot assignment
        }  // end for jsonMeshes

        // Write back the (possibly updated) meshes array so primitive material
        // redirections are visible to the rest of the pipeline.
        gltfJson["meshes"] = jsonMeshes;
        _materialToSourceMeshIndex = patchedMaterials;

        // Helper: write KHR_texture_transform from a GLMaterial::Texture
        auto writeTransform = [&](QJsonObject& parent, const QString& key, const GLMaterial::Texture& tex) -> bool {
            if (!parent.contains(key))
            {
                log(QString("    writeTransform(%1): key not in parent").arg(key), logCallback);
                return false;
            }

            log(QString("    writeTransform(%1): scale=[%2,%3] offset=[%4,%5] rotation=%6 texCoord=%7")
                .arg(key).arg(tex.scale.x).arg(tex.scale.y)
                .arg(tex.offset.x).arg(tex.offset.y).arg(tex.rotation).arg(tex.texCoordIndex), logCallback);

            bool hasTransform = (tex.scale.x != 1.0f || tex.scale.y != 1.0f ||
                tex.offset.x != 0.0f || tex.offset.y != 0.0f ||
                tex.rotation != 0.0f || tex.texCoordIndex != 0);

            if (!hasTransform)
            {
                log(QString("      -> No transform (all defaults)"), logCallback);
                return false;
            }

            QJsonObject texInfo = parent[key].toObject();
            QJsonObject extensions = texInfo.value("extensions").toObject();
            QJsonArray scale, offset;
            scale.append(static_cast<double>(tex.scale.x));
            scale.append(static_cast<double>(tex.scale.y));
            offset.append(static_cast<double>(tex.offset.x));
            offset.append(static_cast<double>(tex.offset.y));

            QJsonObject transform;
            transform["scale"] = scale;
            transform["offset"] = offset;
            transform["rotation"] = static_cast<double>(tex.rotation);
            if (tex.texCoordIndex != 0)
                transform["texCoord"] = tex.texCoordIndex;

            extensions["KHR_texture_transform"] = transform;
            texInfo["extensions"] = extensions;
            if (tex.texCoordIndex != 0)
                texInfo["texCoord"] = tex.texCoordIndex;

            parent[key] = texInfo;
            log(QString("      -> KHR_texture_transform written"), logCallback);
            return true;
            };

        // Helper: get or create a sampler
        QMap<QString, int> samplerConfigToIndex;
        QVector<QJsonObject> createdSamplers;
        int samplerBaseIndex = samplers.size();  // Track where new samplers will start

        auto getOrCreateSampler = [&](int mag, int min, int wrapS, int wrapT) -> int {
            QString hash = QString("%1_%2_%3_%4").arg(mag).arg(min).arg(wrapS).arg(wrapT);
            if (samplerConfigToIndex.contains(hash))
                return samplerConfigToIndex[hash] + samplerBaseIndex;  // Offset by base

            int idx = createdSamplers.size();
            QJsonObject sampler;
            sampler["magFilter"] = mag;
            sampler["minFilter"] = min;
            sampler["wrapS"] = wrapS;
            sampler["wrapT"] = wrapT;
            createdSamplers.append(sampler);
            int finalIdx = idx + samplerBaseIndex;  // Final index in merged array
            samplerConfigToIndex[hash] = idx;

            log(QString("    Sampler[%1]: mag=%2, min=%3, wrapS=%4, wrapT=%5 (final index: %6)")
                .arg(idx).arg(mag).arg(min).arg(wrapS).arg(wrapT).arg(finalIdx), logCallback);
            return finalIdx;
            };

        // Helper: update texture sampler
        auto updateTextureSampler = [&](const QJsonObject& parent, const QString& key,
            const GLMaterial::Texture& sourceTex) {
                if (!parent.contains(key)) return;
                int texIndex = parent[key].toObject().value("index").toInt(-1);
                if (texIndex < 0 || texIndex >= textures.size()) return;

                int samplerIdx = getOrCreateSampler(
                    sourceTex.magFilter, sourceTex.minFilter,
                    sourceTex.wrapS, sourceTex.wrapT);

                QJsonObject tex = textures[texIndex].toObject();
                tex["sampler"] = samplerIdx;
                textures[texIndex] = tex;

                log(QString("    %1: texture[%2] -> sampler[%3]")
                    .arg(key).arg(texIndex).arg(samplerIdx), logCallback);
            };

        // Build material signatures from all source meshes for robust matching
        // CRITICAL: Keep full signatures indexed by mesh index for primary matching!
        // Do NOT deduplicate before patching, as that breaks the index correspondence.
        log("=== Building Material Signatures ===", logCallback);
        std::vector<MaterialSignature> signatures;  // Full array: signatures[i] = signature for mesh i
        for (int i = 0; i < static_cast<int>(meshes.size()); ++i)
        {
            MaterialSignature sig = buildSignatureForMesh(i, meshes[i], logCallback);
            signatures.push_back(sig);
            log(QString("  Signature[%1]: name='%2' textures=%3 originalIdx=%4")
                .arg(i).arg(sig.name).arg(sig.textureFilePaths.size())
                .arg(sig.originalMaterialIndex), logCallback);
        }

        log(QString("  RESULT: %1 material signatures built (one per mesh)")
            .arg(signatures.size()), logCallback);

        // Now patch each material that was mapped from a source mesh
        log("=== Patching Materials with Signature Matching ===", logCallback);

        // Note: jsonMatToSignatureMap removed (consolidation disabled)

        for (auto it = patchedMaterials.begin(); it != patchedMaterials.end(); ++it)
        {
            int matIdx = it.key();
            int meshIdx = it.value();  // Source mesh index from our direct mapping

            QJsonObject mat = materials[matIdx].toObject();
            QString jsonMatName = mat["name"].toString();

            log(QString("Material[%1] '%2': from source mesh[%3] (origMatIdx=%4):").arg(matIdx).arg(jsonMatName).arg(meshIdx)
                .arg(meshIdx >= 0 && meshIdx < static_cast<int>(meshes.size()) ? meshes[meshIdx]->getOriginalMaterialIndex() : -1), logCallback);

            // NEW STRATEGY: Use the meshIdx from our direct mapping as PRIMARY source
            // This is the authoritative mapping built during mesh-to-material assignment.
            // All signature matching is now secondary/fallback.

            int matchedSigIdx = -1;

            // Primary: Direct mapping - find signature for this exact source mesh
            if (meshIdx >= 0 && meshIdx < static_cast<int>(signatures.size()))
            {
                if (signatures[meshIdx].meshIndex == meshIdx)
                {
                    matchedSigIdx = meshIdx;
                    log(QString("  [DIRECT MATCH] Using source mesh[%1] signature directly").arg(meshIdx), logCallback);
                }
            }

            // Secondary fallback: Try fingerprint matching if direct didn't work
            if (matchedSigIdx < 0)
            {
                matchedSigIdx = findMaterialBySignature(
                    jsonMatName, mat, signatures, matIdx, logCallback);
            }

            // Tertiary fallback: Name matching with dedup
            if (matchedSigIdx < 0)
            {
                matchedSigIdx = findMaterialByNameWithDedup(
                    jsonMatName, signatures, matIdx, logCallback);
            }

            // Quaternary fallback: Index-based matching
            if (matchedSigIdx < 0)
            {
                matchedSigIdx = findMaterialByIndexFallback(matIdx, signatures, logCallback);
            }

            // Ultimate fallback: use the meshIdx we already have
            if (matchedSigIdx < 0)
            {
                if (meshIdx >= 0 && meshIdx < static_cast<int>(meshes.size()))
                {
                    matchedSigIdx = meshIdx;
                    log(QString("  [FINAL FALLBACK] Using mapped mesh[%1] as signature").arg(meshIdx), logCallback);
                }
                else
                {
                    log(QString("  ERROR: No material signature found and meshIdx=%1 invalid!").arg(meshIdx), logCallback);
                    continue;
                }
            }

            if (matchedSigIdx < 0 || matchedSigIdx >= static_cast<int>(signatures.size()))
            {
                log(QString("  ERROR: Invalid signature index %1").arg(matchedSigIdx), logCallback);
                continue;
            }

            const MaterialSignature& matchedSig = signatures[matchedSigIdx];
            log(QString("  -> Using signature '%1'").arg(matchedSig.name), logCallback);

            // Validate we can access the source material
            int sourceMeshIdx = matchedSig.meshIndex;
            if (sourceMeshIdx < 0 || sourceMeshIdx >= static_cast<int>(meshes.size()))
            {
                log(QString("  ERROR: Invalid mesh index %1 in signature").arg(sourceMeshIdx), logCallback);
                continue;
            }

            // Get a fresh reference to the source material - this is safe because meshes vector is persistent
            auto getSourceMaterial = [&]() -> const GLMaterial& { return meshes[sourceMeshIdx]->getMaterial(); };

            // ===== FIX BASE COLOR =====
            // Assimp's glTF exporter doesn't properly export material colors from aiMaterial.
            // We patch the baseColorFactor directly in the JSON with the correct albedoColor from GLMaterial.
            {
                int meshIdx = matchedSig.meshIndex;
                if (meshIdx < 0 || meshIdx >= static_cast<int>(meshes.size()))
                {
                    log(QString("  WARNING: Cannot access mesh for base color, skipping"), logCallback);
                }
                else
                {
                    QVector3D albedo = getSourceMaterial().albedoColor();
                    if (mat.contains("pbrMetallicRoughness"))
                    {
                        QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                        QJsonArray baseColorFactor;
                        baseColorFactor.append(static_cast<double>(albedo.x()));
                        baseColorFactor.append(static_cast<double>(albedo.y()));
                        baseColorFactor.append(static_cast<double>(albedo.z()));
                        baseColorFactor.append(1.0);  // alpha
                        pbr["baseColorFactor"] = baseColorFactor;
                        mat["pbrMetallicRoughness"] = pbr;

                        log(QString("    Patched baseColorFactor: [%1, %2, %3]")
                            .arg(albedo.x(), 0, 'f', 4)
                            .arg(albedo.y(), 0, 'f', 4)
                            .arg(albedo.z(), 0, 'f', 4), logCallback);
                    }
                }
            }

            // ===== FIX TEXTURE INDICES =====
            // Assimp reorders meshes during export, so texture indices in the
            // exported JSON may point to the wrong images. We correct them by
            // finding the image index that matches the source GLMaterial's texture path.

            auto fixTextureIndex = [&](QJsonObject& parent, const QString& key,
                const GLMaterial::Texture& sourceTex) {
                    if (!parent.contains(key)) return;
                    if (sourceTex.path.empty()) return;

                    // Get just the filename from the full source path
                    QString sourcePath = QString::fromStdString(sourceTex.path);
                    QString sourceFilename = QFileInfo(normalisedGlbPath(sourcePath)).fileName();

                    // Find the image index in the JSON images array with a matching filename
                    int correctImageIdx = -1;

                    auto embeddedIt = _embeddedIndexMapping.find(sourcePath);
                    if (embeddedIt != _embeddedIndexMapping.end() &&
                        embeddedIt.value() >= 0 &&
                        embeddedIt.value() < images.size())
                    {
                        correctImageIdx = embeddedIt.value();
                        log(QString("    Using authoritative embedded image index %1 for '%2'")
                            .arg(correctImageIdx).arg(sourcePath), logCallback);
                    }

                    for (int ii = 0; ii < images.size(); ++ii)
                    {
                        if (correctImageIdx >= 0)
                            break;
                        QString imgUri = images[ii].toObject().value("uri").toString();
                        if (QFileInfo(imgUri).fileName().compare(sourceFilename, Qt::CaseInsensitive) == 0 ||
                            QFileInfo(imgUri).completeBaseName().compare(sourceFilename, Qt::CaseInsensitive) == 0)
                        {
                            correctImageIdx = ii;
                            break;
                        }
                    }

                    // GLB fallback: images have no URI, match by "name" field instead.
                    // Resolve original path through pathMapping to get the packaged filename.
                    if (correctImageIdx < 0 && !_pathMapping.isEmpty())
                    {
                        QString packagedRelPath = resolvePackagedPath(_pathMapping, sourcePath);
                        QString packagedName = QFileInfo(packagedRelPath).fileName();
                        if (!packagedName.isEmpty())
                        {
                            for (int ii = 0; ii < images.size(); ++ii)
                            {
                                QString imgName = images[ii].toObject().value("name").toString();
                                if (!imgName.isEmpty() &&
                                    QFileInfo(imgName).fileName().compare(packagedName, Qt::CaseInsensitive) == 0)
                                {
                                    correctImageIdx = ii;
                                    break;
                                }
                            }
                        }
                    }

                    if (correctImageIdx < 0)
                    {
                        // Image not exported by Assimp - create it via findOrCreateTexture
                        int newTexIdx = findOrCreateTexture(gltfJson, sourcePath, logCallback);
                        if (newTexIdx < 0)
                        {
                            log(QString("    WARNING: Could not create texture for '%1'").arg(sourceFilename), logCallback);
                            return;
                        }
                        // Reload images/textures arrays since findOrCreateTexture may have modified gltfJson
                        images = gltfJson.value("images").toArray();
                        textures = gltfJson.value("textures").toArray();
                        QJsonObject texInfo = parent[key].toObject();
                        texInfo["index"] = newTexIdx;
                        parent[key] = texInfo;
                        log(QString("    Created missing image+texture for '%1' -> tex[%2]")
                            .arg(sourceFilename).arg(newTexIdx), logCallback);
                        return;
                    }

                    // Find or create a texture entry pointing to this image
                    // (reuse existing if same source+sampler, otherwise create new)
                    QJsonObject texInfo = parent[key].toObject();
                    int currentTexIdx = texInfo.value("index").toInt(-1);

                    // Check if current texture already points to the right image
                    if (currentTexIdx >= 0 && currentTexIdx < textures.size())
                    {
                        int currentSource = textures[currentTexIdx].toObject().value("source").toInt(-1);
                        if (currentSource == correctImageIdx)
                            return; // Already correct
                    }

                    // Find an existing texture entry pointing to correctImageIdx,
                    // or create a new one
                    int correctTexIdx = -1;
                    for (int ti = 0; ti < textures.size(); ++ti)
                    {
                        if (textures[ti].toObject().value("source").toInt(-1) == correctImageIdx)
                        {
                            correctTexIdx = ti;
                            break;
                        }
                    }

                    if (correctTexIdx < 0)
                    {
                        // Create a new texture entry
                        QJsonObject newTex;
                        newTex["source"] = correctImageIdx;
                        correctTexIdx = textures.size();
                        textures.append(newTex);
                        log(QString("    Created texture[%1] for image[%2] '%3'")
                            .arg(correctTexIdx).arg(correctImageIdx).arg(sourceFilename), logCallback);
                    }

                    texInfo["index"] = correctTexIdx;
                    parent[key] = texInfo;

                    log(QString("    Fixed %1: texture index %2 -> %3 (image '%4')")
                        .arg(key).arg(currentTexIdx).arg(correctTexIdx).arg(sourceFilename), logCallback);
                };

            // Fix texture indices for all slots before writing transforms/samplers
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                fixTextureIndex(pbr, "baseColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::Albedo));

                // SKIP fixTextureIndex for metallicRoughnessTexture if:
                // 1. Metallic and roughness are separate files (packing was done in exporter)
                // 2. Metallic is missing (roughness-only material - don't apply empty metallic texture)
                const auto& metallicTex = getSourceMaterial().texture(GLMaterial::TextureType::Metallic);
                const auto& roughnessTex = getSourceMaterial().texture(GLMaterial::TextureType::Roughness);
                bool hasPackedMR = (!metallicTex.path.empty() && !roughnessTex.path.empty() &&
                                     metallicTex.path != roughnessTex.path);
                bool isRoughnessOnly = (metallicTex.path.empty() && !roughnessTex.path.empty());

                if (!hasPackedMR && !isRoughnessOnly)
                {
                    // Only fix M/R texture if not packed AND metallic exists
                    fixTextureIndex(pbr, "metallicRoughnessTexture", metallicTex);
                }
                // else: skip to avoid corrupting metallicRoughnessTexture (packed M/R or roughness-only)

                mat["pbrMetallicRoughness"] = pbr;
            }
            fixTextureIndex(mat, "normalTexture", getSourceMaterial().texture(GLMaterial::TextureType::Normal));

            // Handle AO for packed ORM: set occlusionTexture to point to the packed ORM texture
            // (packed ORM has AO data in R channel, so occlusionTexture should use it instead of original AO)
            bool handledAsPackedORM = false;
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                if (pbr.contains("metallicRoughnessTexture"))
                {
                    QJsonObject mrObj = pbr["metallicRoughnessTexture"].toObject();
                    int texIdx = mrObj.value("index").toInt(-1);
                    log(QString("    [ORM Detection] M/R texture index: %1, total textures: %2").arg(texIdx).arg(textures.size()), logCallback);
                    if (texIdx >= 0 && texIdx < textures.size())
                    {
                        QJsonObject texObj = textures[texIdx].toObject();
                        int imgIdx = texObj.value("source").toInt(-1);
                        log(QString("    [ORM Detection] M/R image source: %1, total images: %2").arg(imgIdx).arg(images.size()), logCallback);
                        if (imgIdx >= 0 && imgIdx < images.size())
                        {
                            QString imgUri = images[imgIdx].toObject().value("uri").toString();
                            log(QString("    [ORM Detection] M/R image URI: %1").arg(imgUri), logCallback);
                            if (imgUri.contains("_packed_orm"))
                            {
                                // Set occlusionTexture to use the same packed ORM texture as metallicRoughnessTexture
                                QJsonObject occlusionObj;
                                occlusionObj["index"] = texIdx;  // Point to same texture as M/R
                                mat["occlusionTexture"] = occlusionObj;
                                handledAsPackedORM = true;
                                log("    [ORM Detection] MATCHED! Set occlusionTexture to packed ORM texture (same as metallicRoughnessTexture)", logCallback);
                            }
                            else
                            {
                                log(QString("    [ORM Detection] No match - URI does not contain '_packed_orm_'"), logCallback);
                            }
                        }
                        else
                        {
                            log(QString("    [ORM Detection] Invalid image index"), logCallback);
                        }
                    }
                    else
                    {
                        log(QString("    [ORM Detection] Invalid texture index"), logCallback);
                    }
                }
                else
                {
                    log(QString("    [ORM Detection] No metallicRoughnessTexture found"), logCallback);
                }
            }
            else
            {
                log(QString("    [ORM Detection] No pbrMetallicRoughness found"), logCallback);
            }

            if (!handledAsPackedORM)
            {
                log(QString("    [ORM Detection] Not handled as packed ORM - calling fixTextureIndex for original AO"), logCallback);
                fixTextureIndex(mat, "occlusionTexture", getSourceMaterial().texture(GLMaterial::TextureType::AmbientOcclusion));
            }

            fixTextureIndex(mat, "emissiveTexture", getSourceMaterial().texture(GLMaterial::TextureType::Emissive));

            // --- Texture transforms ---
            bool transformsWritten = false;
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                if (writeTransform(pbr, "baseColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::Albedo)))
                    transformsWritten = true;
                if (writeTransform(pbr, "metallicRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::Metallic)))
                    transformsWritten = true;
                if (transformsWritten)
                    mat["pbrMetallicRoughness"] = pbr;
            }
            if (writeTransform(mat, "normalTexture", getSourceMaterial().texture(GLMaterial::TextureType::Normal)))
                transformsWritten = true;
            // Skip occlusion texture transform if we handled it as packed ORM
            // (the packed ORM texture transform comes from metallicRoughnessTexture, not from AO)
            if (!handledAsPackedORM && writeTransform(mat, "occlusionTexture", getSourceMaterial().texture(GLMaterial::TextureType::AmbientOcclusion)))
                transformsWritten = true;
            if (writeTransform(mat, "emissiveTexture", getSourceMaterial().texture(GLMaterial::TextureType::Emissive)))
                transformsWritten = true;

            // --- Samplers ---
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                updateTextureSampler(pbr, "baseColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::Albedo));
                updateTextureSampler(pbr, "metallicRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::Metallic));
            }
            updateTextureSampler(mat, "normalTexture", getSourceMaterial().texture(GLMaterial::TextureType::Normal));
            updateTextureSampler(mat, "occlusionTexture", getSourceMaterial().texture(GLMaterial::TextureType::AmbientOcclusion));
            updateTextureSampler(mat, "emissiveTexture", getSourceMaterial().texture(GLMaterial::TextureType::Emissive));

            // --- Basic material properties ---
            GLMaterial::BlendMode blendMode = getSourceMaterial().blendMode();
            if (blendMode == GLMaterial::BlendMode::Opaque)
                mat["alphaMode"] = "OPAQUE";
            else if (blendMode == GLMaterial::BlendMode::Masked)
            {
                mat["alphaMode"] = "MASK";
                mat["alphaCutoff"] = static_cast<double>(getSourceMaterial().alphaThreshold());
            }
            else if (blendMode == GLMaterial::BlendMode::Alpha)
                mat["alphaMode"] = "BLEND";

            if (getSourceMaterial().twoSided())
                mat["doubleSided"] = true;

            QVector3D emissive = getSourceMaterial().emissive();
            if (emissive.length() > 0.0f)
            {
                QJsonArray arr;
                arr.append(static_cast<double>(emissive.x()));
                arr.append(static_cast<double>(emissive.y()));
                arr.append(static_cast<double>(emissive.z()));
                mat["emissiveFactor"] = arr;
            }

            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();

                bool hasMRTex = pbr.contains("metallicRoughnessTexture");
                float originalMetallicFactor = pbr.contains("metallicFactor") ?
                    static_cast<float>(pbr["metallicFactor"].toDouble()) : getSourceMaterial().metalness();
                float metallicFactor = originalMetallicFactor;

                // IMPORTANT: Force metallicFactor to 1.0 if:
                // 1. There's a metallicRoughnessTexture in JSON (packed ORM)
                // 2. AND the source material has any metallic data (texture OR metalness factor)
                // This handles materials with packed ORM where JSON has metallicFactor=0 but material has metallic data
                const auto& metallicTex = getSourceMaterial().texture(GLMaterial::TextureType::Metallic);
                const auto& roughnessTex = getSourceMaterial().texture(GLMaterial::TextureType::Roughness);

                // Check if material has actual metallic data (not just roughness-only)
                bool hasMetallicTexture = !metallicTex.path.empty();
                bool hasMetallicFactor = getSourceMaterial().metalness() > 0.0f;
                bool hasMetallicData = hasMetallicTexture || hasMetallicFactor;

                log(QString("    [MetallicFactor] hasMRTex=%1 json=%2 materialMetalness=%3 metalTexPath='%4' roughTexPath='%5' hasMetallicData=%6")
                    .arg(hasMRTex).arg(originalMetallicFactor, 0, 'f', 4)
                    .arg(getSourceMaterial().metalness(), 0, 'f', 4)
                    .arg(QString::fromStdString(metallicTex.path))
                    .arg(QString::fromStdString(roughnessTex.path))
                    .arg(hasMetallicData), logCallback);

                // Force to 1.0 if JSON has M/R texture AND material has metallic data
                // This ensures packed ORM materials export correctly even if JSON shows metallicFactor=0
                if (hasMRTex && metallicFactor == 0.0f && hasMetallicData) {
                    metallicFactor = 1.0f;
                    log(QString("      -> Forced metallicFactor from %1 to 1.0 (has M/R texture + metallic data)")
                        .arg(originalMetallicFactor, 0, 'f', 4), logCallback);
                }
                pbr["metallicFactor"] = static_cast<double>(metallicFactor);

                if (pbr.contains("baseColorFactor"))
                {
                    QJsonArray existing = pbr["baseColorFactor"].toArray();
                    if (existing.size() >= 4)
                    {
                        // Always use the material's intended albedoColor for baseColorFactor
                        // This ensures tints and color multipliers are preserved
                        QVector3D albedo = getSourceMaterial().albedoColor();
                        existing[0] = static_cast<double>(albedo.x());
                        existing[1] = static_cast<double>(albedo.y());
                        existing[2] = static_cast<double>(albedo.z());
                        existing[3] = static_cast<double>(getSourceMaterial().opacity());
                        pbr["baseColorFactor"] = existing;
                        log(QString("    Corrected baseColorFactor to [%1, %2, %3, %4]")
                            .arg(albedo.x()).arg(albedo.y()).arg(albedo.z()).arg(getSourceMaterial().opacity()), logCallback);
                    }
                }
                else
                {
                    QVector3D albedo = getSourceMaterial().albedoColor();
                    float opacity = getSourceMaterial().opacity();
                    if ((std::abs(albedo.x() - 1.0f) > 0.001f) ||
                        (std::abs(albedo.y() - 1.0f) > 0.001f) ||
                        (std::abs(albedo.z() - 1.0f) > 0.001f) ||
                        (opacity < 0.999f))
                    {
                        QJsonArray arr;
                        arr.append(static_cast<double>(albedo.x()));
                        arr.append(static_cast<double>(albedo.y()));
                        arr.append(static_cast<double>(albedo.z()));
                        arr.append(static_cast<double>(opacity));
                        pbr["baseColorFactor"] = arr;
                    }
                }

                float roughness = pbr.contains("roughnessFactor") ?
                    static_cast<float>(pbr["roughnessFactor"].toDouble()) : getSourceMaterial().roughness();
                pbr["roughnessFactor"] = static_cast<double>(roughness);

                mat["pbrMetallicRoughness"] = pbr;
            }

            // --- KHR extensions ---
            QJsonObject extensions = mat.value("extensions").toObject();
            bool hasExtensions = false;

            if (getSourceMaterial().transmission() > 0.0f || !getSourceMaterial().transmissionMapPath().isEmpty())
            {
                QJsonObject trans;
                trans["transmissionFactor"] = static_cast<double>(getSourceMaterial().transmission());

                if (!getSourceMaterial().transmissionMapPath().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, getSourceMaterial().transmissionMapPath(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; trans["transmissionTexture"] = t; }
                }

                extensions["KHR_materials_transmission"] = trans;
                hasExtensions = true;
            }

            float ior = getSourceMaterial().ior();
            if (std::abs(ior - 1.5f) > 0.001f)
            {
                QJsonObject iorExt;
                iorExt["ior"] = static_cast<double>(ior);
                extensions["KHR_materials_ior"] = iorExt;
                hasExtensions = true;
            }

            if (getSourceMaterial().clearcoat() > 0.0f || !getSourceMaterial().clearcoatColorMapPath().isEmpty()
                || !getSourceMaterial().clearcoatRoughnessMapPath().isEmpty() || !getSourceMaterial().clearcoatNormalMapPath().isEmpty())
            {
                QJsonObject cc;
                cc["clearcoatFactor"] = static_cast<double>(getSourceMaterial().clearcoat());
                cc["clearcoatRoughnessFactor"] = static_cast<double>(getSourceMaterial().clearcoatRoughness());

                QString clearcoatColorMap = getSourceMaterial().clearcoatColorMapPath();
                if (!clearcoatColorMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, clearcoatColorMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; cc["clearcoatTexture"] = t; }
                }

                QString clearcoatRoughnessMap = getSourceMaterial().clearcoatRoughnessMapPath();
                if (!clearcoatRoughnessMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, clearcoatRoughnessMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; cc["clearcoatRoughnessTexture"] = t; }
                }

                QString clearcoatNormalMap = getSourceMaterial().clearcoatNormalMapPath();
                if (!clearcoatNormalMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, clearcoatNormalMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; cc["clearcoatNormalTexture"] = t; }
                }

                extensions["KHR_materials_clearcoat"] = cc;
                hasExtensions = true;
            }

            QVector3D sheenColor = getSourceMaterial().sheenColor();
            if (sheenColor.length() > 0.0f || !getSourceMaterial().sheenColorMapPath().isEmpty()
                || !getSourceMaterial().sheenRoughnessMapPath().isEmpty())
            {
                QJsonObject sheen;
                QJsonArray arr;
                arr.append(static_cast<double>(sheenColor.x()));
                arr.append(static_cast<double>(sheenColor.y()));
                arr.append(static_cast<double>(sheenColor.z()));
                sheen["sheenColorFactor"] = arr;
                sheen["sheenRoughnessFactor"] = static_cast<double>(getSourceMaterial().sheenRoughness());

                QString sheenColorMap = getSourceMaterial().sheenColorMapPath();
                if (!sheenColorMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, sheenColorMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sheen["sheenColorTexture"] = t; }
                }

                QString sheenRoughnessMap = getSourceMaterial().sheenRoughnessMapPath();
                if (!sheenRoughnessMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, sheenRoughnessMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sheen["sheenRoughnessTexture"] = t; }
                }

                extensions["KHR_materials_sheen"] = sheen;
                hasExtensions = true;
            }

            if (getSourceMaterial().iridescenceFactor() > 0.0f || !getSourceMaterial().iridescenceMap().isEmpty()
                || !getSourceMaterial().iridescenceThicknessMap().isEmpty())
            {
                QJsonObject irid;
                irid["iridescenceFactor"] = static_cast<double>(getSourceMaterial().iridescenceFactor());
                irid["iridescenceIor"] = static_cast<double>(getSourceMaterial().iridescenceIor());
                irid["iridescenceThicknessMinimum"] = static_cast<double>(getSourceMaterial().iridescenceThicknessMin());
                irid["iridescenceThicknessMaximum"] = static_cast<double>(getSourceMaterial().iridescenceThicknessMax());

                QString iridMap = getSourceMaterial().iridescenceMap();
                if (!iridMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, iridMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; irid["iridescenceTexture"] = t; }
                }

                QString iridThicknessMap = getSourceMaterial().iridescenceThicknessMap();
                if (!iridThicknessMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, iridThicknessMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; irid["iridescenceThicknessTexture"] = t; }
                }

                extensions["KHR_materials_iridescence"] = irid;
                hasExtensions = true;
            }

            if (getSourceMaterial().thicknessFactor() > 0.0f)
            {
                QJsonObject vol;
                vol["thicknessFactor"] = static_cast<double>(getSourceMaterial().thicknessFactor());
                vol["attenuationDistance"] = static_cast<double>(getSourceMaterial().attenuationDistance());
                QVector3D ac = getSourceMaterial().attenuationColor();
                QJsonArray arr;
                arr.append(static_cast<double>(ac.x()));
                arr.append(static_cast<double>(ac.y()));
                arr.append(static_cast<double>(ac.z()));
                vol["attenuationColor"] = arr;
                QString thicknessMap = getSourceMaterial().thicknessMap();
                if (!thicknessMap.isEmpty())
                {
                    int texIndex = findOrCreateTexture(gltfJson, thicknessMap);
                    if (texIndex >= 0)
                    {
                        QJsonObject texInfo;
                        texInfo["index"] = texIndex;
                        vol["thicknessTexture"] = texInfo;
                    }
                }
                extensions["KHR_materials_volume"] = vol;
                hasExtensions = true;
            }

            float specularFactor = getSourceMaterial().specularFactor();
            QVector3D specularColor = getSourceMaterial().specularColorFactor();
            QString specularFactorMap = getSourceMaterial().specularFactorMap();
            QString specularColorMap = getSourceMaterial().specularColorMap();

            // Check if this material has non-default specular values
            bool hasNonDefaultSpecular = (std::abs(specularFactor - 1.0f) > 0.001f) ||
                (specularColor - QVector3D(1, 1, 1)).length() > 0.001f ||
                !specularFactorMap.isEmpty() || !specularColorMap.isEmpty();

            // For metallic-roughness materials, we should ALWAYS write KHR_materials_specular
            // to properly define the specular response, even if using default values
            // Only skip if using the old specular-glossiness workflow
            bool shouldWriteSpecular = (hasNonDefaultSpecular || mat.contains("pbrMetallicRoughness")) &&
                                       !getSourceMaterial().getUseSpecularGlossiness();

            log(QString("    [Specular] factor=%1 color=[%2,%3,%4] nonDefault=%5 hasPBR=%6 useSpecGloss=%7 shouldWrite=%8")
                .arg(specularFactor, 0, 'f', 4).arg(specularColor.x(), 0, 'f', 4)
                .arg(specularColor.y(), 0, 'f', 4).arg(specularColor.z(), 0, 'f', 4)
                .arg(hasNonDefaultSpecular).arg(mat.contains("pbrMetallicRoughness"))
                .arg(getSourceMaterial().getUseSpecularGlossiness()).arg(shouldWriteSpecular), logCallback);

            if (shouldWriteSpecular)
            {
                QJsonObject spec;
                spec["specularFactor"] = static_cast<double>(specularFactor);
                QJsonArray arr;
                arr.append(static_cast<double>(specularColor.x()));
                arr.append(static_cast<double>(specularColor.y()));
                arr.append(static_cast<double>(specularColor.z()));
                spec["specularColorFactor"] = arr;
                if (!specularFactorMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, specularFactorMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; spec["specularTexture"] = t; }
                }
                if (!specularColorMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, specularColorMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; spec["specularColorTexture"] = t; }
                }
                extensions["KHR_materials_specular"] = spec;
                hasExtensions = true;
                log("      -> KHR_materials_specular written", logCallback);
            }
            else if (hasNonDefaultSpecular)
            {
                log("      -> SKIPPED KHR_materials_specular (useSpecularGlossiness=true)", logCallback);
            }

            if (getSourceMaterial().anisotropyStrength() > 0.0f || !getSourceMaterial().anisotropyMap().isEmpty())
            {
                QJsonObject aniso;
                aniso["anisotropyStrength"] = static_cast<double>(getSourceMaterial().anisotropyStrength());
                aniso["anisotropyRotation"] = static_cast<double>(getSourceMaterial().anisotropyRotation());

                if (!getSourceMaterial().anisotropyMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, getSourceMaterial().anisotropyMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; aniso["anisotropyTexture"] = t; }
                }

                extensions["KHR_materials_anisotropy"] = aniso;
                hasExtensions = true;
            }

            if (getSourceMaterial().dispersion() > 0.0f)
            {
                QJsonObject disp;
                disp["dispersion"] = static_cast<double>(getSourceMaterial().dispersion());
                extensions["KHR_materials_dispersion"] = disp;
                hasExtensions = true;
            }

            if (std::abs(getSourceMaterial().emissiveStrength() - 1.0f) > 0.001f)
            {
                QJsonObject es;
                es["emissiveStrength"] = static_cast<double>(getSourceMaterial().emissiveStrength());
                extensions["KHR_materials_emissive_strength"] = es;
                hasExtensions = true;
            }

            if (getSourceMaterial().isUnlit())
            {
                extensions["KHR_materials_unlit"] = QJsonObject();
                hasExtensions = true;
            }

            // KHR_materials_volume_scatter
            if (getSourceMaterial().hasVolumeScattering())
            {
                QJsonObject scatter;
                QVector3D msc = getSourceMaterial().multiScatterColor();
                QJsonArray mscArr;
                mscArr.append(static_cast<double>(msc.x()));
                mscArr.append(static_cast<double>(msc.y()));
                mscArr.append(static_cast<double>(msc.z()));
                scatter["multiscatterColor"] = mscArr;
                extensions["KHR_materials_volume_scatter"] = scatter;
                hasExtensions = true;
            }

            // KHR_materials_diffuse_transmission
            if (getSourceMaterial().diffuseTransmissionFactor() > 0.0f
                || !getSourceMaterial().diffuseTransmissionMap().isEmpty()
                || !getSourceMaterial().diffuseTransmissionColorMap().isEmpty())
            {
                QJsonObject dt;
                dt["diffuseTransmissionFactor"] = static_cast<double>(getSourceMaterial().diffuseTransmissionFactor());

                QVector3D dtc = getSourceMaterial().diffuseTransmissionColorFactor();
                QJsonArray dtcArr;
                dtcArr.append(static_cast<double>(dtc.x()));
                dtcArr.append(static_cast<double>(dtc.y()));
                dtcArr.append(static_cast<double>(dtc.z()));
                dt["diffuseTransmissionColorFactor"] = dtcArr;

                if (!getSourceMaterial().diffuseTransmissionMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, getSourceMaterial().diffuseTransmissionMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; dt["diffuseTransmissionTexture"] = t; }
                }
                if (!getSourceMaterial().diffuseTransmissionColorMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, getSourceMaterial().diffuseTransmissionColorMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; dt["diffuseTransmissionColorTexture"] = t; }
                }

                extensions["KHR_materials_diffuse_transmission"] = dt;
                hasExtensions = true;
            }

            // KHR_materials_pbrSpecularGlossiness
            if (getSourceMaterial().getUseSpecularGlossiness())
            {
                QJsonObject sg;

                QVector3D diff = getSourceMaterial().diffuseColor();
                QJsonArray diffArr;
                diffArr.append(static_cast<double>(diff.x()));
                diffArr.append(static_cast<double>(diff.y()));
                diffArr.append(static_cast<double>(diff.z()));
                diffArr.append(static_cast<double>(getSourceMaterial().opacity()));
                sg["diffuseFactor"] = diffArr;

                QVector3D spec = getSourceMaterial().specularColor();
                QJsonArray specArr;
                specArr.append(static_cast<double>(spec.x()));
                specArr.append(static_cast<double>(spec.y()));
                specArr.append(static_cast<double>(spec.z()));
                sg["specularFactor"] = specArr;

                sg["glossinessFactor"] = static_cast<double>(getSourceMaterial().glossinessFactor());

                if (!getSourceMaterial().diffuseMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, getSourceMaterial().diffuseMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sg["diffuseTexture"] = t; }
                }
                if (!getSourceMaterial().specularGlossinessMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, getSourceMaterial().specularGlossinessMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sg["specularGlossinessTexture"] = t; }
                }

                extensions["KHR_materials_pbrSpecularGlossiness"] = sg;
                hasExtensions = true;
            }

            if (hasExtensions)
                mat["extensions"] = extensions;

            // --- Clearcoat texture index fix + transforms + samplers ---
            // Must run AFTER mat["extensions"] = extensions so the cc object
            // built above (with correct texture references) is already in place.
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject cc = exts.value("KHR_materials_clearcoat").toObject();
                if (!cc.isEmpty())
                {
                    fixTextureIndex(cc, "clearcoatTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatColor));
                    fixTextureIndex(cc, "clearcoatRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatRoughness));
                    fixTextureIndex(cc, "clearcoatNormalTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatNormal));

                    writeTransform(cc, "clearcoatTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatColor));
                    writeTransform(cc, "clearcoatRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatRoughness));
                    writeTransform(cc, "clearcoatNormalTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatNormal));

                    updateTextureSampler(cc, "clearcoatTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatColor));
                    updateTextureSampler(cc, "clearcoatRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatRoughness));
                    updateTextureSampler(cc, "clearcoatNormalTexture", getSourceMaterial().texture(GLMaterial::TextureType::ClearcoatNormal));

                    exts["KHR_materials_clearcoat"] = cc;
                    mat["extensions"] = exts;
                }
            }

            // --- Anisotropy texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject aniso = exts.value("KHR_materials_anisotropy").toObject();
                if (!aniso.isEmpty())
                {
                    fixTextureIndex(aniso, "anisotropyTexture", getSourceMaterial().texture(GLMaterial::TextureType::Anisotropy));
                    writeTransform(aniso, "anisotropyTexture", getSourceMaterial().texture(GLMaterial::TextureType::Anisotropy));
                    updateTextureSampler(aniso, "anisotropyTexture", getSourceMaterial().texture(GLMaterial::TextureType::Anisotropy));

                    exts["KHR_materials_anisotropy"] = aniso;
                    mat["extensions"] = exts;
                }
            }

            // --- Sheen texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject sheen = exts.value("KHR_materials_sheen").toObject();
                if (!sheen.isEmpty())
                {
                    fixTextureIndex(sheen, "sheenColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::SheenColor));
                    fixTextureIndex(sheen, "sheenRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::SheenRoughness));

                    writeTransform(sheen, "sheenColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::SheenColor));
                    writeTransform(sheen, "sheenRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::SheenRoughness));

                    updateTextureSampler(sheen, "sheenColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::SheenColor));
                    updateTextureSampler(sheen, "sheenRoughnessTexture", getSourceMaterial().texture(GLMaterial::TextureType::SheenRoughness));

                    exts["KHR_materials_sheen"] = sheen;
                    mat["extensions"] = exts;
                }
            }

            // --- Iridescence texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject irid = exts.value("KHR_materials_iridescence").toObject();
                if (!irid.isEmpty())
                {
                    fixTextureIndex(irid, "iridescenceTexture", getSourceMaterial().texture(GLMaterial::TextureType::Iridescence));
                    fixTextureIndex(irid, "iridescenceThicknessTexture", getSourceMaterial().texture(GLMaterial::TextureType::IridescenceThickness));

                    writeTransform(irid, "iridescenceTexture", getSourceMaterial().texture(GLMaterial::TextureType::Iridescence));
                    writeTransform(irid, "iridescenceThicknessTexture", getSourceMaterial().texture(GLMaterial::TextureType::IridescenceThickness));

                    updateTextureSampler(irid, "iridescenceTexture", getSourceMaterial().texture(GLMaterial::TextureType::Iridescence));
                    updateTextureSampler(irid, "iridescenceThicknessTexture", getSourceMaterial().texture(GLMaterial::TextureType::IridescenceThickness));

                    exts["KHR_materials_iridescence"] = irid;
                    mat["extensions"] = exts;
                }
            }

            // --- Specular texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject spec = exts.value("KHR_materials_specular").toObject();
                if (!spec.isEmpty())
                {
                    fixTextureIndex(spec, "specularTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularFactor));
                    fixTextureIndex(spec, "specularColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularColor));

                    writeTransform(spec, "specularTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularFactor));
                    writeTransform(spec, "specularColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularColor));

                    updateTextureSampler(spec, "specularTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularFactor));
                    updateTextureSampler(spec, "specularColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularColor));

                    exts["KHR_materials_specular"] = spec;
                    mat["extensions"] = exts;
                }
            }

            // --- Volume texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject vol = exts.value("KHR_materials_volume").toObject();
                if (!vol.isEmpty())
                {
                    fixTextureIndex(vol, "thicknessTexture", getSourceMaterial().texture(GLMaterial::TextureType::Thickness));
                    writeTransform(vol, "thicknessTexture", getSourceMaterial().texture(GLMaterial::TextureType::Thickness));
                    updateTextureSampler(vol, "thicknessTexture", getSourceMaterial().texture(GLMaterial::TextureType::Thickness));

                    exts["KHR_materials_volume"] = vol;
                    mat["extensions"] = exts;
                }
            }

            // --- Transmission texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject trans = exts.value("KHR_materials_transmission").toObject();
                if (!trans.isEmpty())
                {
                    fixTextureIndex(trans, "transmissionTexture", getSourceMaterial().texture(GLMaterial::TextureType::Transmission));
                    writeTransform(trans, "transmissionTexture", getSourceMaterial().texture(GLMaterial::TextureType::Transmission));
                    updateTextureSampler(trans, "transmissionTexture", getSourceMaterial().texture(GLMaterial::TextureType::Transmission));

                    exts["KHR_materials_transmission"] = trans;
                    mat["extensions"] = exts;
                }
            }

            // --- Diffuse transmission texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject dt = exts.value("KHR_materials_diffuse_transmission").toObject();
                if (!dt.isEmpty())
                {
                    fixTextureIndex(dt, "diffuseTransmissionTexture", getSourceMaterial().texture(GLMaterial::TextureType::DiffuseTransmission));
                    fixTextureIndex(dt, "diffuseTransmissionColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::DiffuseTransmissionColor));

                    writeTransform(dt, "diffuseTransmissionTexture", getSourceMaterial().texture(GLMaterial::TextureType::DiffuseTransmission));
                    writeTransform(dt, "diffuseTransmissionColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::DiffuseTransmissionColor));

                    updateTextureSampler(dt, "diffuseTransmissionTexture", getSourceMaterial().texture(GLMaterial::TextureType::DiffuseTransmission));
                    updateTextureSampler(dt, "diffuseTransmissionColorTexture", getSourceMaterial().texture(GLMaterial::TextureType::DiffuseTransmissionColor));

                    exts["KHR_materials_diffuse_transmission"] = dt;
                    mat["extensions"] = exts;
                }
            }

            // --- pbrSpecularGlossiness texture index fix + transforms + samplers ---
            {
                QJsonObject exts = mat.value("extensions").toObject();
                QJsonObject sg = exts.value("KHR_materials_pbrSpecularGlossiness").toObject();
                if (!sg.isEmpty())
                {
                    fixTextureIndex(sg, "diffuseTexture", getSourceMaterial().texture(GLMaterial::TextureType::Diffuse));
                    fixTextureIndex(sg, "specularGlossinessTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularGlossiness));

                    writeTransform(sg, "diffuseTexture", getSourceMaterial().texture(GLMaterial::TextureType::Diffuse));
                    writeTransform(sg, "specularGlossinessTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularGlossiness));

                    updateTextureSampler(sg, "diffuseTexture", getSourceMaterial().texture(GLMaterial::TextureType::Diffuse));
                    updateTextureSampler(sg, "specularGlossinessTexture", getSourceMaterial().texture(GLMaterial::TextureType::SpecularGlossiness));

                    exts["KHR_materials_pbrSpecularGlossiness"] = sg;
                    mat["extensions"] = exts;
                }
            }

            // Write correct material name from source GLMaterial
            QString correctName = getSourceMaterial().name();
            if (!correctName.isEmpty())
                mat["name"] = correctName;

            materials[matIdx] = mat;
        }

        // MERGE created samplers with existing Assimp samplers
        // Keep Assimp's samplers at their original indices (0, 1, 2, ...)
        // Append new samplers after them
        log(QString("  Sampler merge: base=%1, appending %2 created sampler(s)")
            .arg(samplerBaseIndex).arg(createdSamplers.size()), logCallback);
        for (const auto& sampler : createdSamplers)
        {
            samplers.append(sampler);
        }

        // CRITICAL: Merge textures - include any added by findOrCreateTexture during material processing
        QJsonArray gltfTextures = gltfJson.value("textures").toArray();
        if (gltfTextures.size() > textures.size())
        {
            // New textures were added by findOrCreateTexture - append them
            for (int i = textures.size(); i < gltfTextures.size(); ++i)
            {
                textures.append(gltfTextures[i]);
                log(QString("  Merged extension texture[%1] from findOrCreateTexture").arg(i), logCallback);
            }
        }

        // CRITICAL: Write textures with sampler assignments to gltfJson first
        gltfJson["textures"] = textures;

        // CRITICAL: Reload textures array to include any created by findOrCreateTexture
        textures = gltfJson.value("textures").toArray();

        // NOTE: Since samplers are now merged (Assimp samplers + created samplers),
        // all sampler references remain valid. Assimp textures point to 0-N,
        // post-processor textures point to samplerBaseIndex+, and both sets are preserved.

        // MATERIAL DEDUPLICATION PASS
        // When the original model has multi-primitive meshes with shared material names
        // (e.g. 4 wheels each having "Rim2", "Tireside" etc.), Assimp creates separate
        // aiMaterial objects per aiMesh, resulting in duplicate material entries in the
        // exported JSON.  Deduplicate them so each unique name appears only once.
        // This prevents MaterialProcessor from hitting NAME AMBIGUOUS when reloading.
        // IMPORTANT: Only merge materials that are truly identical in content - same name
        // is not sufficient (a model may have distinct materials that share a name).
        {
            // Map from material JSON (as string) -> canonical index
            QMap<QString, int> contentToFirstIdx;
            QMap<int, int>     oldToCanonical;
            QJsonArray         dedupedMaterials;

            for (int i = 0; i < materials.size(); ++i)
            {
                QJsonObject matObj = materials[i].toObject();
                QString name = matObj.value("name").toString();

                // Serialize the full material JSON as the dedup key so that
                // two materials are only merged when their content is identical.
                QString contentKey = QJsonDocument(matObj).toJson(QJsonDocument::Compact);

                if (!name.isEmpty() && contentToFirstIdx.contains(contentKey))
                {
                    // Truly identical material: remap to the first occurrence
                    oldToCanonical[i] = contentToFirstIdx[contentKey];
                    log(QString("  Dedup material[%1] '%2' -> canonical[%3]")
                        .arg(i).arg(name).arg(contentToFirstIdx[contentKey]), logCallback);
                }
                else
                {
                    int newIdx = dedupedMaterials.size();
                    oldToCanonical[i] = newIdx;
                    if (!name.isEmpty())
                        contentToFirstIdx[contentKey] = newIdx;
                    dedupedMaterials.append(matObj);
                }
            }

            if (dedupedMaterials.size() < materials.size())
            {
                log(QString("  Material dedup: %1 -> %2 materials")
                    .arg(materials.size()).arg(dedupedMaterials.size()), logCallback);

                QMap<int, int> remappedMaterialToMesh;
                for (auto it = _materialToSourceMeshIndex.begin(); it != _materialToSourceMeshIndex.end(); ++it)
                {
                    if (!oldToCanonical.contains(it.key()))
                        continue;

                    int canonicalIdx = oldToCanonical[it.key()];
                    if (!remappedMaterialToMesh.contains(canonicalIdx))
                        remappedMaterialToMesh[canonicalIdx] = it.value();
                }

                materials = dedupedMaterials;
                _materialToSourceMeshIndex = remappedMaterialToMesh;

                // Update all mesh primitive material references
                QJsonArray meshesArr = gltfJson.value("meshes").toArray();
                for (int mi = 0; mi < meshesArr.size(); ++mi)
                {
                    QJsonObject mesh = meshesArr[mi].toObject();
                    QJsonArray prims = mesh["primitives"].toArray();
                    bool changed = false;
                    for (int pi = 0; pi < prims.size(); ++pi)
                    {
                        QJsonObject prim = prims[pi].toObject();
                        int oldIdx = prim.value("material").toInt(-1);
                        if (oldIdx >= 0 && oldToCanonical.contains(oldIdx))
                        {
                            int newIdx = oldToCanonical[oldIdx];
                            if (newIdx != oldIdx)
                            {
                                prim["material"] = newIdx;
                                prims[pi] = prim;
                                changed = true;
                            }
                        }
                    }
                    if (changed)
                    {
                        mesh["primitives"] = prims;
                        meshesArr[mi] = mesh;
                    }
                }
                gltfJson["meshes"] = meshesArr;
            }
        }

        gltfJson["materials"] = materials;
        gltfJson["samplers"] = samplers;
        textures = gltfJson.value("textures").toArray();
        gltfJson["textures"] = textures;

        // Verify what was actually written
        QJsonArray finalTextures = gltfJson.value("textures").toArray();
        for (int i = 0; i < finalTextures.size(); ++i)
        {
            QJsonObject tex = finalTextures[i].toObject();
            int samplerIdx = tex.value("sampler").toInt(-1);
            log(QString("  Final texture[%1] sampler: %2").arg(i).arg(samplerIdx), logCallback);
        }

        log(QString("  Final: %1 unique samplers created").arg(samplers.size()), logCallback);

        // THIRD-B PASS: Consolidate duplicate JSON materials
        // DISABLED: Material consolidation removed to preserve accuracy.
        // Deduplication (11->4) works correctly; consolidation caused index shifting bugs.
        // Slight redundancy in JSON materials is acceptable trade-off for correctness.
        // log("=== Consolidating Duplicate JSON Materials (DISABLED) ===", logCallback);

        // FOURTH PASS: Deduplicate images
        // Assimp may create duplicate image entries for the same file
        // We need to merge them and update texture references
        if (gltfJson.contains("images"))
        {
            QJsonArray images = gltfJson["images"].toArray();
            QMap<QString, int> uriToIndex;  // Maps URI to first occurrence index
            QMap<int, int> oldToNewIndex;   // Maps old image index to deduplicated index
            QJsonArray deduplicatedImages;

            // Build deduplication map
            for (int i = 0; i < images.size(); ++i)
            {
                QJsonObject img = images[i].toObject();
                QString uri = img.value("uri").toString();

                if (uriToIndex.contains(uri))
                {
                    // Duplicate - map to existing image
                    oldToNewIndex[i] = uriToIndex[uri];
                }
                else
                {
                    // New image - add to deduplicated array
                    int newIndex = deduplicatedImages.size();
                    uriToIndex[uri] = newIndex;
                    oldToNewIndex[i] = newIndex;
                    deduplicatedImages.append(img);
                }
            }

            // Update texture source references
            if (oldToNewIndex.size() != images.size())  // If we found duplicates
            {
                textures = gltfJson.value("textures").toArray();

                for (int i = 0; i < textures.size(); ++i)
                {
                    QJsonObject texture = textures[i].toObject();
                    int oldSource = texture.value("source").toInt(-1);

                    if (oldSource >= 0 && oldToNewIndex.contains(oldSource))
                    {
                        texture["source"] = oldToNewIndex[oldSource];
                        textures[i] = texture;
                    }
                }

                gltfJson["images"] = deduplicatedImages;
                gltfJson["textures"] = textures;

                log(QString("  Deduplicated images: %1 -> %2")
                    .arg(images.size())
                    .arg(deduplicatedImages.size()), logCallback);
            }
        }

        // ===== UPDATE extensionsUsed ARRAY =====
        // Scan all materials to see which extensions are actually used
        QSet<QString> usedExtensions;

        materials = gltfJson.value("materials").toArray();
        for (const QJsonValue& matVal : materials)
        {
            QJsonObject mat = matVal.toObject();
            if (mat.contains("extensions"))
            {
                QJsonObject exts = mat.value("extensions").toObject();
                for (const QString& extName : exts.keys())
                {
                    usedExtensions.insert(extName);
                }
            }
        }

        // Add to extensionsUsed array
        if (!usedExtensions.isEmpty())
        {
            QJsonArray extensionsUsed;
            if (gltfJson.contains("extensionsUsed"))
            {
                extensionsUsed = gltfJson["extensionsUsed"].toArray();
            }

            // Get existing extensions
            QSet<QString> existing;
            for (const QJsonValue& val : extensionsUsed)
            {
                existing.insert(val.toString());
            }

            // Add new ones
            for (const QString& ext : usedExtensions)
            {
                if (!existing.contains(ext))
                {
                    extensionsUsed.append(ext);
                    log(QString("  Added %1 to extensionsUsed").arg(ext), logCallback);
                }
            }

            gltfJson["extensionsUsed"] = extensionsUsed;
        }
    }

    // FIX: Correct normalTexture.scale (Assimp doesn't export this correctly)
    fixNormalTextureScale(gltfJson, meshes, logCallback);
    fixClearcoatNormalTextureScale(gltfJson, meshes, logCallback);

    // CRITICAL FIX: Correct KHR_materials_specular (or remove if default)
    fixSpecularExtension(gltfJson, meshes, logCallback);

    // CRITICAL FIX: Correct metallicFactor values (Assimp may set to 0 incorrectly)
    fixMetallicFactor(gltfJson, meshes, logCallback);

    if (gltfJson.contains("materials"))
    {
        QJsonArray mats = gltfJson["materials"].toArray();
        for (int i = 0; i < mats.size(); ++i)
        {
            QJsonObject mat = mats[i].toObject();
            log(QString("Material %1:").arg(i), logCallback);

            if (mat.contains("extensions") && mat["extensions"].toObject().contains("KHR_materials_specular"))
            {
                QJsonObject spec = mat["extensions"].toObject()["KHR_materials_specular"].toObject();
                if (spec.contains("specularTexture"))
                {
                    log(QString("  specularTexture index: %1").arg(spec["specularTexture"].toObject().value("index").toInt(-999)), logCallback);
                }
                if (spec.contains("specularColorTexture"))
                {
                    log(QString("  specularColorTexture index: %1").arg(spec["specularColorTexture"].toObject().value("index").toInt(-999)), logCallback);
                }
            }
        }
    }

    log("=== FINAL TEXTURE VALIDATION ===", logCallback);
    {
        QJsonArray finalTextures = gltfJson.value("textures").toArray();
        QJsonArray finalSamplers = gltfJson.value("samplers").toArray();
        int maxSamplerIndex = finalSamplers.size() - 1;
        bool anyFixed = false;

        log(QString("  Samplers available: %1 (indices 0-%2)").arg(finalSamplers.size()).arg(maxSamplerIndex), logCallback);
        log(QString("  Checking %1 textures...").arg(finalTextures.size()), logCallback);

        for (int i = 0; i < finalTextures.size(); ++i)
        {
            QJsonObject tex = finalTextures[i].toObject();
            bool needsFix = false;
            int samplerIdx = -1;

            if (tex.contains("sampler"))
            {
                samplerIdx = tex.value("sampler").toInt(-1);

                // Check if sampler index is invalid
                if (samplerIdx < 0 || samplerIdx > maxSamplerIndex)
                {
                    log(QString("  Texture[%1] has INVALID sampler index %2").arg(i).arg(samplerIdx), logCallback);
                    needsFix = true;
                }
                else
                {
                    log(QString("  Texture[%1] sampler: %2 (valid)").arg(i).arg(samplerIdx), logCallback);
                }
            }
            else
            {
                // Texture has NO sampler at all
                log(QString("  Texture[%1] has NO sampler reference").arg(i), logCallback);
                needsFix = true;
            }

            // Fix by assigning sampler 0
            if (needsFix)
            {
                if (finalSamplers.size() > 0)
                {
                    log(QString("    -> Setting sampler to 0"), logCallback);
                    tex["sampler"] = 0;
                    finalTextures[i] = tex;
                    anyFixed = true;
                }
                else
                {
                    log(QString("    -> WARNING: No samplers exist, cannot fix"), logCallback);
                }
            }
        }

        if (anyFixed)
        {
            log("  Writing fixed textures array back to gltfJson", logCallback);
            gltfJson["textures"] = finalTextures;

            // Verify the fix
            log("  Verification:", logCallback);
            QJsonArray verify = gltfJson.value("textures").toArray();
            for (int i = 0; i < verify.size(); ++i)
            {
                QJsonObject tex = verify[i].toObject();
                int samp = tex.value("sampler").toInt(-999);
                log(QString("    Texture[%1] sampler: %2").arg(i).arg(samp), logCallback);
            }
        }
        else
        {
            log("  All texture sampler references are valid", logCallback);
        }
    }


    if (!lights.empty())
        writePunctualLights(gltfJson, lights, logCallback);

    // Then do standard post-processing (fills in missing properties with defaults)
    return postProcessGltfJson(gltfJson, logCallback);
}

QString GltfPostProcessor::normalisedGlbPath(const QString& path)
{
    if (path.startsWith("glb://") && path.contains("::"))
        return "glb://" + path.mid(path.lastIndexOf("::") + 2);
    return path;
}

bool GltfPostProcessor::fixTextureInfoWithTransforms(QJsonObject& parent, const QString& key)
{
    if (!parent.contains(key))
        return false;

    QJsonObject texInfo = parent[key].toObject();
    bool modified = false;

    if (!texInfo.contains("texCoord"))
    {
        texInfo["texCoord"] = 0;
        modified = true;
    }

    // Always ensure KHR_texture_transform exists with complete properties
    QJsonObject extensions;
    if (texInfo.contains("extensions"))
    {
        extensions = texInfo["extensions"].toObject();
    }

    QJsonObject transform;
    bool transformModified = false;

    if (extensions.contains("KHR_texture_transform"))
    {
        transform = extensions["KHR_texture_transform"].toObject();
    }
    else
    {
        // Create new transform with identity values
        transformModified = true;
    }

    // Add missing transform properties with identity defaults
    if (!transform.contains("offset"))
    {
        QJsonArray offset;
        offset.append(0.0);
        offset.append(0.0);
        transform["offset"] = offset;
        transformModified = true;
    }

    if (!transform.contains("rotation"))
    {
        transform["rotation"] = 0.0;
        transformModified = true;
    }

    if (!transform.contains("scale"))
    {
        QJsonArray scale;
        scale.append(1.0);
        scale.append(1.0);
        transform["scale"] = scale;
        transformModified = true;
    }

    // texCoord can also be specified inside the transform extension
    if (!transform.contains("texCoord") && texInfo.contains("texCoord"))
    {
        transform["texCoord"] = texInfo["texCoord"].toInt();
        transformModified = true;
    }

    if (transformModified)
    {
        extensions["KHR_texture_transform"] = transform;
        texInfo["extensions"] = extensions;
        modified = true;
    }

    if (modified)
    {
        parent[key] = texInfo;
    }

    return modified;
}

bool GltfPostProcessor::fixNormalTextureInfo(QJsonObject& parent, const QString& key)
{
    if (!parent.contains(key))
        return false;

    QJsonObject texInfo = parent[key].toObject();
    bool modified = false;

    // Add normal-specific scale property
    if (!texInfo.contains("scale"))
    {
        texInfo["scale"] = 1.0;
        modified = true;
    }

    // Store texInfo back if we modified it
    if (modified)
    {
        parent[key] = texInfo;
    }

    // Now handle common properties and transforms
    if (fixTextureInfoWithTransforms(parent, key))
    {
        modified = true;
    }

    return modified;
}

bool GltfPostProcessor::fixOcclusionTextureInfo(QJsonObject& parent, const QString& key)
{
    if (!parent.contains(key))
        return false;

    QJsonObject texInfo = parent[key].toObject();
    bool modified = false;

    // Add occlusion-specific strength property
    if (!texInfo.contains("strength"))
    {
        texInfo["strength"] = 1.0;
        modified = true;
    }

    // Store texInfo back if we modified it
    if (modified)
    {
        parent[key] = texInfo;
    }

    // Now handle common properties and transforms
    if (fixTextureInfoWithTransforms(parent, key))
    {
        modified = true;
    }

    return modified;
}

bool GltfPostProcessor::fixNormalTextureScale(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    std::function<void(const QString&)> logCallback)
{
    bool modified = false;

    if (!gltfJson.contains("materials") || meshes.empty())
        return false;

    QJsonArray materials = gltfJson["materials"].toArray();

    for (int i = 0; i < materials.size(); ++i)
    {
        const TriangleMesh* sourceMesh = sourceMeshForMaterial(i, meshes);
        if (!sourceMesh) continue;

        const GLMaterial& glMat = sourceMesh->getMaterial();
        QJsonObject mat = materials[i].toObject();

        // Fix normalTexture.scale if present
        if (mat.contains("normalTexture"))
        {
            QJsonObject normalTex = mat["normalTexture"].toObject();
            float normalScale = glMat.normalScale();

            // Write the scale (always write it, overriding Assimp's incorrect value)
            normalTex["scale"] = static_cast<double>(normalScale);
            mat["normalTexture"] = normalTex;
            materials[i] = mat;
            modified = true;

            log(QString("  -> Set normalTexture.scale to %1 for material %2")
                .arg(normalScale).arg(i), logCallback);
        }
    }

    if (modified)
    {
        gltfJson["materials"] = materials;  // CRITICAL: Write materials back!
        log("  -> normalTexture.scale values updated", logCallback);
    }

    return modified;
}

bool GltfPostProcessor::fixClearcoatNormalTextureScale(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    std::function<void(const QString&)> logCallback)
{
    bool modified = false;

    if (!gltfJson.contains("materials") || meshes.empty())
        return false;

    QJsonArray materials = gltfJson["materials"].toArray();

    for (int i = 0; i < materials.size(); ++i)
    {
        const TriangleMesh* sourceMesh = sourceMeshForMaterial(i, meshes);
        if (!sourceMesh) continue;

        const GLMaterial& glMat = sourceMesh->getMaterial();
        QJsonObject mat = materials[i].toObject();
        QJsonObject extensions = mat.value("extensions").toObject();

        if (!extensions.contains("KHR_materials_clearcoat"))
            continue;

        QJsonObject clearcoat = extensions["KHR_materials_clearcoat"].toObject();
        if (!clearcoat.contains("clearcoatNormalTexture"))
            continue;

        QJsonObject clearcoatNormalTex = clearcoat["clearcoatNormalTexture"].toObject();
        const float clearcoatNormalScale = glMat.clearcoatNormalScale();
        clearcoatNormalTex["scale"] = static_cast<double>(clearcoatNormalScale);
        clearcoat["clearcoatNormalTexture"] = clearcoatNormalTex;
        extensions["KHR_materials_clearcoat"] = clearcoat;
        mat["extensions"] = extensions;
        materials[i] = mat;
        modified = true;

        log(QString("  -> Set clearcoatNormalTexture.scale to %1 for material %2")
            .arg(clearcoatNormalScale).arg(i), logCallback);
    }

    if (modified)
    {
        gltfJson["materials"] = materials;
        log("  -> clearcoatNormalTexture.scale values updated", logCallback);
    }

    return modified;
}

bool GltfPostProcessor::fixSpecularExtension(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    std::function<void(const QString&)> logCallback)
{
    bool modified = false;

    if (!gltfJson.contains("materials") || meshes.empty())
        return false;

    QJsonArray materials = gltfJson["materials"].toArray();

    for (int i = 0; i < materials.size(); ++i)
    {
        const TriangleMesh* sourceMesh = sourceMeshForMaterial(i, meshes);
        if (!sourceMesh) continue;

        const GLMaterial& glMat = sourceMesh->getMaterial();
        QJsonObject mat = materials[i].toObject();

        // Check if material has KHR_materials_specular extension
        if (mat.contains("extensions"))
        {
            QJsonObject extensions = mat["extensions"].toObject();

            if (extensions.contains("KHR_materials_specular"))
            {
                // CRITICAL: Get the EXISTING specular object (preserves textures!)
                QJsonObject specular = extensions["KHR_materials_specular"].toObject();

                // Get the CORRECT values from GLMaterial
                float specularFactor = glMat.specularFactor();
                QVector3D specularColor = glMat.specularColorFactor();

                // Check if this extension should even be exported
                bool isDefaultValues = (std::abs(specularFactor - 1.0f) < 0.001f) &&
                    (std::abs(specularColor.x() - 1.0f) < 0.001f) &&
                    (std::abs(specularColor.y() - 1.0f) < 0.001f) &&
                    (std::abs(specularColor.z() - 1.0f) < 0.001f);

                // Check if there are textures
                bool hasTextures = specular.contains("specularTexture") ||
                    specular.contains("specularColorTexture");

                if (isDefaultValues && !hasTextures)
                {
                    // Remove the extension entirely if it's just defaults with no textures
                    extensions.remove("KHR_materials_specular");
                    mat["extensions"] = extensions;

                    // If no extensions left, remove the extensions object
                    if (extensions.isEmpty())
                    {
                        mat.remove("extensions");
                    }

                    materials[i] = mat;
                    modified = true;

                    log(QString("  -> Removed default KHR_materials_specular from material %1").arg(i), logCallback);
                }
                else
                {
                    // UPDATE the factor/color values while PRESERVING texture references
                    specular["specularFactor"] = static_cast<double>(specularFactor);

                    QJsonArray colorArray;
                    colorArray.append(static_cast<double>(specularColor.x()));
                    colorArray.append(static_cast<double>(specularColor.y()));
                    colorArray.append(static_cast<double>(specularColor.z()));
                    specular["specularColorFactor"] = colorArray;

                    // Write back the MODIFIED specular object (textures still intact!)
                    extensions["KHR_materials_specular"] = specular;
                    mat["extensions"] = extensions;
                    materials[i] = mat;
                    modified = true;

                    log(QString("  -> Fixed KHR_materials_specular for material %1: factor=%2, color=[%3, %4, %5]")
                        .arg(i).arg(specularFactor)
                        .arg(specularColor.x()).arg(specularColor.y()).arg(specularColor.z()),
                        logCallback);
                }
            }
        }
    }

    if (modified)
    {
        gltfJson["materials"] = materials;
    }

    return modified;
}

bool GltfPostProcessor::fixMetallicFactor(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    std::function<void(const QString&)> logCallback)
{
    bool modified = false;

    if (!gltfJson.contains("materials") || meshes.empty())
        return false;

    QJsonArray materials = gltfJson["materials"].toArray();

    for (int i = 0; i < materials.size(); ++i)
    {
        const TriangleMesh* sourceMesh = sourceMeshForMaterial(i, meshes);
        if (!sourceMesh) continue;

        const GLMaterial& glMat = sourceMesh->getMaterial();
        QJsonObject mat = materials[i].toObject();

        if (mat.contains("pbrMetallicRoughness"))
        {
            QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
            float correctMetallic = glMat.metalness();  // Get the CORRECT value

            // Write the correct metallicFactor
            pbr["metallicFactor"] = static_cast<double>(correctMetallic);
            mat["pbrMetallicRoughness"] = pbr;
            materials[i] = mat;
            modified = true;

            log(QString("  -> Fixed metallicFactor to %1 for material %2")
                .arg(correctMetallic).arg(i), logCallback);
        }
    }

    if (modified)
    {
        gltfJson["materials"] = materials;
    }

    return modified;
}

bool GltfPostProcessor::fixSpecularTextures(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    std::function<void(const QString&)> logCallback)
{
    log("=== SPECULAR TEXTURE FIX START ===", logCallback);
    bool modified = false;

    if (!gltfJson.contains("materials"))
    {
        log("  -> No materials in gltfJson", logCallback);
        return false;
    }

    if (meshes.empty())
    {
        log("  -> No meshes provided", logCallback);
        return false;
    }

    QJsonArray materials = gltfJson["materials"].toArray();

    for (int i = 0; i < materials.size(); ++i)
    {
        const TriangleMesh* sourceMesh = sourceMeshForMaterial(i, meshes);
        if (!sourceMesh)
        {
            log(QString("  -> No source mesh mapped for material %1, skipping").arg(i), logCallback);
            continue;
        }

        const GLMaterial& glMat = sourceMesh->getMaterial();

        // DEBUG: Check what maps are set
        QString specularFactorMap = glMat.specularFactorMap();
        QString specularColorMap = glMat.specularColorMap();

        log(QString("Material %1:").arg(i), logCallback);
        log(QString("  specularFactorMap: '%1'").arg(specularFactorMap), logCallback);
        log(QString("  specularColorMap: '%2'").arg(specularColorMap), logCallback);

        if (specularFactorMap.isEmpty() && specularColorMap.isEmpty())
        {
            log("  -> No specular maps, skipping", logCallback);
            continue;
        }

        QJsonObject mat = materials[i].toObject();

        // Check if material has KHR_materials_specular extension
        if (!mat.contains("extensions"))
        {
            log("  -> No extensions object, skipping", logCallback);
            continue;
        }

        QJsonObject extensions = mat["extensions"].toObject();

        if (!extensions.contains("KHR_materials_specular"))
        {
            log("  -> No KHR_materials_specular extension, skipping", logCallback);
            continue;
        }

        QJsonObject specular = extensions["KHR_materials_specular"].toObject();
        log("  -> Found KHR_materials_specular extension", logCallback);
        bool specularModified = false;

        // Add specularTexture if material has it
        if (!specularFactorMap.isEmpty())
        {
            log(QString("  -> Processing specularTexture: %1").arg(specularFactorMap), logCallback);

            int texIndex = findOrCreateTexture(gltfJson, specularFactorMap, logCallback);
            log(QString("  -> findOrCreateTexture returned: %1").arg(texIndex), logCallback);

            if (texIndex >= 0)
            {
                QJsonObject texInfo;
                texInfo["index"] = texIndex;
                specular["specularTexture"] = texInfo;
                specularModified = true;

                log(QString("  -> Added specularTexture with index %1").arg(texIndex), logCallback);
            }
            else
            {
                log("  -> Failed to find/create specularTexture", logCallback);
            }
        }

        // Add specularColorTexture if material has it
        if (!specularColorMap.isEmpty())
        {
            log(QString("  -> Processing specularColorTexture: %1").arg(specularColorMap), logCallback);

            int texIndex = findOrCreateTexture(gltfJson, specularColorMap, logCallback);
            log(QString("  -> findOrCreateTexture returned: %1").arg(texIndex), logCallback);

            if (texIndex >= 0)
            {
                QJsonObject texInfo;
                texInfo["index"] = texIndex;
                specular["specularColorTexture"] = texInfo;
                specularModified = true;

                log(QString("  -> Added specularColorTexture with index %1").arg(texIndex), logCallback);
            }
            else
            {
                log("  -> Failed to find/create specularColorTexture", logCallback);
            }
        }

        if (specularModified)
        {
            extensions["KHR_materials_specular"] = specular;
            mat["extensions"] = extensions;
            materials[i] = mat;
            modified = true;

            log(QString("  -> Modified material %1").arg(i), logCallback);
        }
    }

    if (modified)
    {
        gltfJson["materials"] = materials;
        log("  -> Updated materials array in gltfJson", logCallback);
    }

    log("=== SPECULAR TEXTURE FIX END ===", logCallback);
    return modified;
}

// Helper function with extensive logging
int GltfPostProcessor::findOrCreateTexture(
    QJsonObject& gltfJson,
    const QString& imagePath,
    std::function<void(const QString&)> logCallback)
{
    log(QString("findOrCreateTexture called with: '%1'").arg(imagePath), logCallback);

    // CRITICAL: Get fresh copies EVERY time
    QJsonArray images = gltfJson.value("images").toArray();
    QJsonArray textures = gltfJson.value("textures").toArray();

    log(QString("  Current images count: %1").arg(images.size()), logCallback);
    log(QString("  Current textures count: %1").arg(textures.size()), logCallback);

    // Extract just the filename from the path
    QString imageFileName = QFileInfo(normalisedGlbPath(imagePath)).fileName();
    log(QString("  Extracted filename: '%1'").arg(imageFileName), logCallback);

    // Look for EXISTING texture that already references this image
    // FIRST: Find the image index
    int imageIndex = -1;

    auto embeddedIt = _embeddedIndexMapping.find(imagePath);
    if (embeddedIt != _embeddedIndexMapping.end() &&
        embeddedIt.value() >= 0 &&
        embeddedIt.value() < images.size())
    {
        imageIndex = embeddedIt.value();
        log(QString("  -> Using authoritative embedded image index %1").arg(imageIndex), logCallback);
    }

    for (int i = 0; i < images.size(); ++i)
    {
        if (imageIndex >= 0)
            break;
        QJsonObject img = images[i].toObject();
        QString uri = img.value("uri").toString();
        log(QString("  Checking image %1: uri='%2'").arg(i).arg(uri), logCallback);

        if (uri.endsWith(imageFileName) ||
            QFileInfo(uri).completeBaseName().compare(imageFileName, Qt::CaseInsensitive) == 0)
        {
            imageIndex = i;
            log(QString("  -> Found matching image at index %1").arg(i), logCallback);
            break;
        }
    }

    // GLB fallback: match by "name" field (URI is empty for embedded images)
    if (imageIndex < 0 && !_pathMapping.isEmpty())
    {
        QString packagedRelPath = resolvePackagedPath(_pathMapping, imagePath);
        QString packagedName = QFileInfo(packagedRelPath).fileName();
        if (!packagedName.isEmpty())
        {
            for (int i = 0; i < images.size(); ++i)
            {
                QString imgName = images[i].toObject().value("name").toString();
                if (!imgName.isEmpty() &&
                    QFileInfo(imgName).fileName().compare(packagedName, Qt::CaseInsensitive) == 0)
                {
                    imageIndex = i;
                    log(QString("  -> Found embedded image at index %1 by name '%2'")
                        .arg(i).arg(packagedName), logCallback);
                    break;
                }
            }
        }
    }

    // GLB name fallback: match imageFileName against base name of each image's "name" field.
    // Handles cases like imagePath="glb://image_1" -> imageFileName="image_1",
    // which should match an image with name="image_1.png".
    if (imageIndex < 0)
    {
        QString imageBaseName = QFileInfo(imageFileName).completeBaseName();
        if (imageBaseName.isEmpty()) imageBaseName = imageFileName; // no extension case

        for (int i = 0; i < images.size(); ++i)
        {
            QString imgName = images[i].toObject().value("name").toString();
            if (!imgName.isEmpty())
            {
                QString imgBase = QFileInfo(imgName).completeBaseName();
                if (imgBase.compare(imageBaseName, Qt::CaseInsensitive) == 0)
                {
                    imageIndex = i;
                    log(QString("  -> Found embedded image at index %1 by base name '%2'")
                        .arg(i).arg(imageBaseName), logCallback);
                    break;
                }
            }
        }
    }

    // If image not found, create it
    if (imageIndex < 0)
    {
        QString newUri = _textureSubfolder + "/" + imageFileName;
        QJsonObject newImage;
        newImage["uri"] = newUri;
        images.append(newImage);
        imageIndex = images.size() - 1;

        // CRITICAL: Write back immediately
        gltfJson["images"] = images;

        log(QString("  -> Created new image at index %1 with uri='%2'").arg(imageIndex).arg(newUri), logCallback);
    }

    // NOW check if a texture already points to this image
    for (int i = 0; i < textures.size(); ++i)
    {
        QJsonObject tex = textures[i].toObject();
        int source = tex.value("source").toInt(-1);
        if (source == imageIndex)
        {
            log(QString("  -> Found existing texture at index %1 pointing to image %2").arg(i).arg(imageIndex), logCallback);
            return i;  // Reuse existing texture!
        }
    }

    // Create NEW texture entry pointing to this image
    QJsonObject newTexture;
    newTexture["source"] = imageIndex;

    // Add sampler if one exists
    // Check if sampler exists and add it
    if (gltfJson.contains("samplers"))
    {
        QJsonArray samplers = gltfJson["samplers"].toArray();
        log(QString("  findOrCreateTexture: samplers array size = %1").arg(samplers.size()), logCallback);

        if (samplers.size() > 0)
        {
            newTexture["sampler"] = 0; // Use first sampler
            log("  -> Added sampler reference to texture (index 0)", logCallback);
        }
        else
        {
            log("  -> WARNING: No samplers exist, NOT adding sampler reference", logCallback);
        }
    }
    else
    {
        log("  -> WARNING: No samplers array in gltfJson", logCallback);
    }

    textures.append(newTexture);
    int textureIndex = textures.size() - 1;

    // CRITICAL: Write back immediately
    gltfJson["textures"] = textures;

    log(QString("  -> Created new texture at index %1 pointing to image %2").arg(textureIndex).arg(imageIndex), logCallback);

    return textureIndex;
}
