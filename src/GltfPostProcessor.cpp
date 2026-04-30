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

        // ===== MESH-NAME-BASED MATERIAL PATCHING =====
        // Assimp reorders meshes and deduplicates identical geometry during export.
        // The ONLY reliable mapping is:
        //   json_meshes[j]["name"]  ->  TriangleMesh* source (by getName())
        //   json_meshes[j]["primitives"][0]["material"]  ->  which JSON material to patch
        //
        // We iterate JSON meshes, find the source TriangleMesh by name,
        // then patch the corresponding JSON material using that mesh's GLMaterial.

        // Build name -> ordered queue of source mesh indices.
        //
        // Application mesh names are "ModelName (MeshName)" but exported JSON mesh
        // names are just "MeshName". We queue by both the full name and the bare
        // name so either form matches.
        //
        // Using a queue (ordered list + cursor) rather than a single-value map
        // correctly handles the case where multiple source meshes share the same
        // name but carry different materials (e.g. 25 prisms each with a distinct
        // IOR/dispersion value). Each JSON mesh dequeues one source mesh in order,
        // so the Nth "Prism" JSON mesh gets the Nth "Prism" source mesh.
        QMap<QString, QList<int>> meshNameToQueue;   // bare name -> ordered source indices
        QMap<QString, int>        meshNameCursor;    // bare name -> next unconsumed index
        QMap<QString, QString>    lastMatForKey;     // last queued material name per key

        for (int k = 0; k < static_cast<int>(meshes.size()); ++k)
        {
            if (!meshes[k]) continue;
            QString fullName = meshes[k]->getName();
            if (fullName.isEmpty()) continue;

            // Extract bare name inside "ModelName (BareName)"
            QString bareName = fullName;
            int parenOpen = fullName.lastIndexOf('(');
            int parenClose = fullName.lastIndexOf(')');
            if (parenOpen >= 0 && parenClose > parenOpen)
                bareName = fullName.mid(parenOpen + 1, parenClose - parenOpen - 1).trimmed();

            // Instance deduplication: glTF instancing causes ModelViewer to create
            // one TriangleMesh per node referencing a mesh, so N nodes sharing the
            // same mesh+material produce N consecutive identical source entries.
            // The exported JSON collapses those back to one mesh entry. We skip
            // any source mesh whose material name matches the last queued entry
            // for the same key, so the queue holds one entry per distinct material
            // variant rather than one per node instance.
            QString matName = meshes[k]->getMaterial().name();

            // Queue by bare name (covers JSON mesh name lookups)
            if (!bareName.isEmpty())
            {
                if (!meshNameToQueue.contains(bareName) ||
                    lastMatForKey.value(bareName) != matName)
                {
                    meshNameToQueue[bareName].append(k);
                    lastMatForKey[bareName] = matName;
                }
            }

            // Also queue by full name if it differs (fallback for exact-name match)
            if (fullName != bareName)
            {
                if (!meshNameToQueue.contains(fullName) ||
                    lastMatForKey.value(fullName) != matName)
                {
                    meshNameToQueue[fullName].append(k);
                    lastMatForKey[fullName] = matName;
                }
            }
        }
        log(QString("  Built mesh-name queues: %1 name(s)").arg(meshNameToQueue.size()), logCallback);

        // We may need to update the JSON meshes array (to redirect primitive material
        // pointers when we clone a collapsed material slot). Work on a mutable copy.
        QJsonArray jsonMeshes = gltfJson.value("meshes").toArray();
        for (int j = 0; j < jsonMeshes.size(); ++j)
        {
            QJsonObject jMesh = jsonMeshes[j].toObject();
            QString meshName = jMesh["name"].toString();

            QJsonArray primitives = jMesh["primitives"].toArray();
            if (primitives.isEmpty()) continue;
            int matIdx = primitives[0].toObject().value("material").toInt(-1);
            if (matIdx < 0 || matIdx >= materials.size()) continue;

            // Dequeue the next unconsumed source mesh for this name.
            // Prefer exact full-name match first; fall back to bare name.
            auto dequeue = [&](const QString& key) -> int {
                if (!meshNameToQueue.contains(key)) return -1;
                int& cursor = meshNameCursor[key];
                const QList<int>& q = meshNameToQueue[key];
                if (cursor >= q.size()) return -1;
                return q[cursor++];
                };

            int meshIdx = dequeue(meshName);
            if (meshIdx < 0)
            {
                // Try bare name (JSON mesh names don't include the "ModelName (" prefix)
                // already handled since we queue under bareName too, but try
                // the full application name form just in case.
                for (auto it = meshNameToQueue.begin(); it != meshNameToQueue.end(); ++it)
                {
                    QString key = it.key();
                    if (key.endsWith("(" + meshName + ")") ||
                        key.endsWith(" " + meshName + ")"))
                    {
                        meshIdx = dequeue(key);
                        if (meshIdx >= 0) break;
                    }
                }
            }

            if (meshIdx < 0)
            {
                log(QString("  WARNING: no source mesh found for json mesh '%1'").arg(meshName), logCallback);
                continue;
            }

            if (!patchedMaterials.contains(matIdx))
            {
                // Material slot is free - claim it directly.
                patchedMaterials[matIdx] = meshIdx;
                log(QString("  Mapped: json mesh[%1] '%2' -> material[%3] via source mesh[%4]")
                    .arg(j).arg(meshName).arg(matIdx).arg(meshIdx), logCallback);
            }
            else if (patchedMaterials[matIdx] == meshIdx)
            {
                // Same source mesh already owns this slot (two primitives in one mesh
                // sharing a material) - nothing to do.
                log(QString("  Shared: json mesh[%1] '%2' -> material[%3] (same source mesh[%4])")
                    .arg(j).arg(meshName).arg(matIdx).arg(meshIdx), logCallback);
            }
            else
            {
                // Collision: this material slot is already owned by a DIFFERENT source
                // mesh. Assimp collapsed two distinct source materials into one JSON
                // slot. Clone the slot, redirect this mesh primitive to the clone,
                // and claim the clone for our source mesh.
                //
                // This restores the correct 1-source-mesh : 1-material-slot mapping
                // that the post-processor patching phase requires.
                int clonedIdx = materials.size();
                materials.append(materials[matIdx]);  // deep copy (QJsonValue is CoW)

                // Redirect this JSON mesh's primitive to the cloned slot
                QJsonObject prim0 = primitives[0].toObject();
                prim0["material"] = clonedIdx;
                primitives[0] = prim0;
                jMesh["primitives"] = primitives;
                jsonMeshes[j] = jMesh;

                patchedMaterials[clonedIdx] = meshIdx;
                log(QString("  Cloned: json mesh[%1] '%2' -> material[%3] cloned to [%4] for source mesh[%5]")
                    .arg(j).arg(meshName).arg(matIdx).arg(clonedIdx).arg(meshIdx), logCallback);
            }
        }

        // Write back the (possibly updated) meshes array so primitive material
        // redirections are visible to the rest of the pipeline.
        gltfJson["meshes"] = jsonMeshes;

        // Helper: write KHR_texture_transform from a GLMaterial::Texture
        auto writeTransform = [&](QJsonObject& parent, const QString& key, const GLMaterial::Texture& tex) -> bool {
            if (!parent.contains(key)) return false;

            QJsonObject texInfo = parent[key].toObject();

            bool hasTransform = (tex.scale.x != 1.0f || tex.scale.y != 1.0f ||
                tex.offset.x != 0.0f || tex.offset.y != 0.0f ||
                tex.rotation != 0.0f || tex.texCoordIndex != 0);

            if (!hasTransform) return false;

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
            return true;
            };

        // Helper: get or create a sampler
        QMap<QString, int> samplerConfigToIndex;
        QVector<QJsonObject> createdSamplers;

        auto getOrCreateSampler = [&](int mag, int min, int wrapS, int wrapT) -> int {
            QString hash = QString("%1_%2_%3_%4").arg(mag).arg(min).arg(wrapS).arg(wrapT);
            if (samplerConfigToIndex.contains(hash))
                return samplerConfigToIndex[hash];

            int idx = createdSamplers.size();
            QJsonObject sampler;
            sampler["magFilter"] = mag;
            sampler["minFilter"] = min;
            sampler["wrapS"] = wrapS;
            sampler["wrapT"] = wrapT;
            createdSamplers.append(sampler);
            samplerConfigToIndex[hash] = idx;

            log(QString("    Sampler[%1]: mag=%2, min=%3, wrapS=%4, wrapT=%5")
                .arg(idx).arg(mag).arg(min).arg(wrapS).arg(wrapT), logCallback);
            return idx;
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

        // Now patch each material that was mapped from a source mesh
        for (auto it = patchedMaterials.begin(); it != patchedMaterials.end(); ++it)
        {
            int matIdx = it.key();
            int meshIdx = it.value();

            const GLMaterial& glMat = meshes[meshIdx]->getMaterial();
            QJsonObject mat = materials[matIdx].toObject();

            log(QString("  Patching material[%1] using source mesh[%2] '%3'")
                .arg(matIdx).arg(meshIdx).arg(glMat.name()), logCallback);

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
                fixTextureIndex(pbr, "baseColorTexture", glMat.texture(GLMaterial::TextureType::Albedo));

                // SKIP fixTextureIndex for metallicRoughnessTexture if:
                // 1. Metallic and roughness are separate files (packing was done in exporter)
                // 2. Metallic is missing (roughness-only material - don't apply empty metallic texture)
                const auto& metallicTex = glMat.texture(GLMaterial::TextureType::Metallic);
                const auto& roughnessTex = glMat.texture(GLMaterial::TextureType::Roughness);
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
            fixTextureIndex(mat, "normalTexture", glMat.texture(GLMaterial::TextureType::Normal));

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
                fixTextureIndex(mat, "occlusionTexture", glMat.texture(GLMaterial::TextureType::AmbientOcclusion));
            }

            fixTextureIndex(mat, "emissiveTexture", glMat.texture(GLMaterial::TextureType::Emissive));

            // --- Texture transforms ---
            bool transformsWritten = false;
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                if (writeTransform(pbr, "baseColorTexture", glMat.texture(GLMaterial::TextureType::Albedo)))
                    transformsWritten = true;
                if (writeTransform(pbr, "metallicRoughnessTexture", glMat.texture(GLMaterial::TextureType::Metallic)))
                    transformsWritten = true;
                if (transformsWritten)
                    mat["pbrMetallicRoughness"] = pbr;
            }
            if (writeTransform(mat, "normalTexture", glMat.texture(GLMaterial::TextureType::Normal)))
                transformsWritten = true;
            if (writeTransform(mat, "occlusionTexture", glMat.texture(GLMaterial::TextureType::AmbientOcclusion)))
                transformsWritten = true;
            if (writeTransform(mat, "emissiveTexture", glMat.texture(GLMaterial::TextureType::Emissive)))
                transformsWritten = true;

            // --- Samplers ---
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                updateTextureSampler(pbr, "baseColorTexture", glMat.texture(GLMaterial::TextureType::Albedo));
                updateTextureSampler(pbr, "metallicRoughnessTexture", glMat.texture(GLMaterial::TextureType::Metallic));
            }
            updateTextureSampler(mat, "normalTexture", glMat.texture(GLMaterial::TextureType::Normal));
            updateTextureSampler(mat, "occlusionTexture", glMat.texture(GLMaterial::TextureType::AmbientOcclusion));
            updateTextureSampler(mat, "emissiveTexture", glMat.texture(GLMaterial::TextureType::Emissive));

            // --- Basic material properties ---
            GLMaterial::BlendMode blendMode = glMat.blendMode();
            if (blendMode == GLMaterial::BlendMode::Opaque)
                mat["alphaMode"] = "OPAQUE";
            else if (blendMode == GLMaterial::BlendMode::Masked)
            {
                mat["alphaMode"] = "MASK";
                mat["alphaCutoff"] = static_cast<double>(glMat.alphaThreshold());
            }
            else if (blendMode == GLMaterial::BlendMode::Alpha)
                mat["alphaMode"] = "BLEND";

            if (glMat.twoSided())
                mat["doubleSided"] = true;

            QVector3D emissive = glMat.emissive();
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
                float metallicFactor = pbr.contains("metallicFactor") ?
                    static_cast<float>(pbr["metallicFactor"].toDouble()) : glMat.metalness();

                // IMPORTANT: Only force metallicFactor to 1.0 if the material actually has metallic data
                // For roughness-only materials (no metallic texture/factor), keep metallicFactor at 0
                const auto& metallicTex = glMat.texture(GLMaterial::TextureType::Metallic);
                bool hasMtallicData = !metallicTex.path.empty() || glMat.metalness() > 0.0f;

                if (hasMRTex && metallicFactor == 0.0f && hasMtallicData)
                    metallicFactor = 1.0f;
                pbr["metallicFactor"] = static_cast<double>(metallicFactor);

                if (pbr.contains("baseColorFactor"))
                {
                    QJsonArray existing = pbr["baseColorFactor"].toArray();
                    if (existing.size() >= 4)
                    {
                        // Always use the material's intended albedoColor for baseColorFactor
                        // This ensures tints and color multipliers are preserved
                        QVector3D albedo = glMat.albedoColor();
                        existing[0] = static_cast<double>(albedo.x());
                        existing[1] = static_cast<double>(albedo.y());
                        existing[2] = static_cast<double>(albedo.z());
                        existing[3] = static_cast<double>(glMat.opacity());
                        pbr["baseColorFactor"] = existing;
                        log(QString("    Corrected baseColorFactor to [%1, %2, %3, %4]")
                            .arg(albedo.x()).arg(albedo.y()).arg(albedo.z()).arg(glMat.opacity()), logCallback);
                    }
                }
                else
                {
                    QVector3D albedo = glMat.albedoColor();
                    float opacity = glMat.opacity();
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
                    static_cast<float>(pbr["roughnessFactor"].toDouble()) : glMat.roughness();
                pbr["roughnessFactor"] = static_cast<double>(roughness);

                mat["pbrMetallicRoughness"] = pbr;
            }

            // --- KHR extensions ---
            QJsonObject extensions = mat.value("extensions").toObject();
            bool hasExtensions = false;

            if (glMat.transmission() > 0.0f || !glMat.transmissionMapPath().isEmpty())
            {
                QJsonObject trans;
                trans["transmissionFactor"] = static_cast<double>(glMat.transmission());

                if (!glMat.transmissionMapPath().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, glMat.transmissionMapPath(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; trans["transmissionTexture"] = t; }
                }

                extensions["KHR_materials_transmission"] = trans;
                hasExtensions = true;
            }

            float ior = glMat.ior();
            if (std::abs(ior - 1.5f) > 0.001f)
            {
                QJsonObject iorExt;
                iorExt["ior"] = static_cast<double>(ior);
                extensions["KHR_materials_ior"] = iorExt;
                hasExtensions = true;
            }

            if (glMat.clearcoat() > 0.0f || !glMat.clearcoatColorMapPath().isEmpty()
                || !glMat.clearcoatRoughnessMapPath().isEmpty() || !glMat.clearcoatNormalMapPath().isEmpty())
            {
                QJsonObject cc;
                cc["clearcoatFactor"] = static_cast<double>(glMat.clearcoat());
                cc["clearcoatRoughnessFactor"] = static_cast<double>(glMat.clearcoatRoughness());

                QString clearcoatColorMap = glMat.clearcoatColorMapPath();
                if (!clearcoatColorMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, clearcoatColorMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; cc["clearcoatTexture"] = t; }
                }

                QString clearcoatRoughnessMap = glMat.clearcoatRoughnessMapPath();
                if (!clearcoatRoughnessMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, clearcoatRoughnessMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; cc["clearcoatRoughnessTexture"] = t; }
                }

                QString clearcoatNormalMap = glMat.clearcoatNormalMapPath();
                if (!clearcoatNormalMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, clearcoatNormalMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; cc["clearcoatNormalTexture"] = t; }
                }

                extensions["KHR_materials_clearcoat"] = cc;
                hasExtensions = true;
            }

            QVector3D sheenColor = glMat.sheenColor();
            if (sheenColor.length() > 0.0f || !glMat.sheenColorMapPath().isEmpty()
                || !glMat.sheenRoughnessMapPath().isEmpty())
            {
                QJsonObject sheen;
                QJsonArray arr;
                arr.append(static_cast<double>(sheenColor.x()));
                arr.append(static_cast<double>(sheenColor.y()));
                arr.append(static_cast<double>(sheenColor.z()));
                sheen["sheenColorFactor"] = arr;
                sheen["sheenRoughnessFactor"] = static_cast<double>(glMat.sheenRoughness());

                QString sheenColorMap = glMat.sheenColorMapPath();
                if (!sheenColorMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, sheenColorMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sheen["sheenColorTexture"] = t; }
                }

                QString sheenRoughnessMap = glMat.sheenRoughnessMapPath();
                if (!sheenRoughnessMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, sheenRoughnessMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sheen["sheenRoughnessTexture"] = t; }
                }

                extensions["KHR_materials_sheen"] = sheen;
                hasExtensions = true;
            }

            if (glMat.iridescenceFactor() > 0.0f || !glMat.iridescenceMap().isEmpty()
                || !glMat.iridescenceThicknessMap().isEmpty())
            {
                QJsonObject irid;
                irid["iridescenceFactor"] = static_cast<double>(glMat.iridescenceFactor());
                irid["iridescenceIor"] = static_cast<double>(glMat.iridescenceIor());
                irid["iridescenceThicknessMinimum"] = static_cast<double>(glMat.iridescenceThicknessMin());
                irid["iridescenceThicknessMaximum"] = static_cast<double>(glMat.iridescenceThicknessMax());

                QString iridMap = glMat.iridescenceMap();
                if (!iridMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, iridMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; irid["iridescenceTexture"] = t; }
                }

                QString iridThicknessMap = glMat.iridescenceThicknessMap();
                if (!iridThicknessMap.isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, iridThicknessMap, logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; irid["iridescenceThicknessTexture"] = t; }
                }

                extensions["KHR_materials_iridescence"] = irid;
                hasExtensions = true;
            }

            if (glMat.thicknessFactor() > 0.0f)
            {
                QJsonObject vol;
                vol["thicknessFactor"] = static_cast<double>(glMat.thicknessFactor());
                vol["attenuationDistance"] = static_cast<double>(glMat.attenuationDistance());
                QVector3D ac = glMat.attenuationColor();
                QJsonArray arr;
                arr.append(static_cast<double>(ac.x()));
                arr.append(static_cast<double>(ac.y()));
                arr.append(static_cast<double>(ac.z()));
                vol["attenuationColor"] = arr;
                QString thicknessMap = glMat.thicknessMap();
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

            float specularFactor = glMat.specularFactor();
            QVector3D specularColor = glMat.specularColorFactor();
            QString specularFactorMap = glMat.specularFactorMap();
            QString specularColorMap = glMat.specularColorMap();
            bool hasSpecular = (std::abs(specularFactor - 1.0f) > 0.001f) ||
                (specularColor - QVector3D(1, 1, 1)).length() > 0.001f ||
                !specularFactorMap.isEmpty() || !specularColorMap.isEmpty();
            if (hasSpecular && !glMat.getUseSpecularGlossiness())
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
            }

            if (glMat.anisotropyStrength() > 0.0f || !glMat.anisotropyMap().isEmpty())
            {
                QJsonObject aniso;
                aniso["anisotropyStrength"] = static_cast<double>(glMat.anisotropyStrength());
                aniso["anisotropyRotation"] = static_cast<double>(glMat.anisotropyRotation());

                if (!glMat.anisotropyMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, glMat.anisotropyMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; aniso["anisotropyTexture"] = t; }
                }

                extensions["KHR_materials_anisotropy"] = aniso;
                hasExtensions = true;
            }

            if (glMat.dispersion() > 0.0f)
            {
                QJsonObject disp;
                disp["dispersion"] = static_cast<double>(glMat.dispersion());
                extensions["KHR_materials_dispersion"] = disp;
                hasExtensions = true;
            }

            if (std::abs(glMat.emissiveStrength() - 1.0f) > 0.001f)
            {
                QJsonObject es;
                es["emissiveStrength"] = static_cast<double>(glMat.emissiveStrength());
                extensions["KHR_materials_emissive_strength"] = es;
                hasExtensions = true;
            }

            if (glMat.isUnlit())
            {
                extensions["KHR_materials_unlit"] = QJsonObject();
                hasExtensions = true;
            }

            // KHR_materials_volume_scatter
            if (glMat.hasVolumeScattering())
            {
                QJsonObject scatter;
                QVector3D msc = glMat.multiScatterColor();
                QJsonArray mscArr;
                mscArr.append(static_cast<double>(msc.x()));
                mscArr.append(static_cast<double>(msc.y()));
                mscArr.append(static_cast<double>(msc.z()));
                scatter["multiscatterColor"] = mscArr;
                extensions["KHR_materials_volume_scatter"] = scatter;
                hasExtensions = true;
            }

            // KHR_materials_diffuse_transmission
            if (glMat.diffuseTransmissionFactor() > 0.0f
                || !glMat.diffuseTransmissionMap().isEmpty()
                || !glMat.diffuseTransmissionColorMap().isEmpty())
            {
                QJsonObject dt;
                dt["diffuseTransmissionFactor"] = static_cast<double>(glMat.diffuseTransmissionFactor());

                QVector3D dtc = glMat.diffuseTransmissionColorFactor();
                QJsonArray dtcArr;
                dtcArr.append(static_cast<double>(dtc.x()));
                dtcArr.append(static_cast<double>(dtc.y()));
                dtcArr.append(static_cast<double>(dtc.z()));
                dt["diffuseTransmissionColorFactor"] = dtcArr;

                if (!glMat.diffuseTransmissionMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, glMat.diffuseTransmissionMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; dt["diffuseTransmissionTexture"] = t; }
                }
                if (!glMat.diffuseTransmissionColorMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, glMat.diffuseTransmissionColorMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; dt["diffuseTransmissionColorTexture"] = t; }
                }

                extensions["KHR_materials_diffuse_transmission"] = dt;
                hasExtensions = true;
            }

            // KHR_materials_pbrSpecularGlossiness
            if (glMat.getUseSpecularGlossiness())
            {
                QJsonObject sg;

                QVector3D diff = glMat.diffuseColor();
                QJsonArray diffArr;
                diffArr.append(static_cast<double>(diff.x()));
                diffArr.append(static_cast<double>(diff.y()));
                diffArr.append(static_cast<double>(diff.z()));
                diffArr.append(static_cast<double>(glMat.opacity()));
                sg["diffuseFactor"] = diffArr;

                QVector3D spec = glMat.specularColor();
                QJsonArray specArr;
                specArr.append(static_cast<double>(spec.x()));
                specArr.append(static_cast<double>(spec.y()));
                specArr.append(static_cast<double>(spec.z()));
                sg["specularFactor"] = specArr;

                sg["glossinessFactor"] = static_cast<double>(glMat.glossinessFactor());

                if (!glMat.diffuseMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, glMat.diffuseMap(), logCallback);
                    if (ti >= 0) { QJsonObject t; t["index"] = ti; sg["diffuseTexture"] = t; }
                }
                if (!glMat.specularGlossinessMap().isEmpty())
                {
                    int ti = findOrCreateTexture(gltfJson, glMat.specularGlossinessMap(), logCallback);
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
                    fixTextureIndex(cc, "clearcoatTexture", glMat.texture(GLMaterial::TextureType::ClearcoatColor));
                    fixTextureIndex(cc, "clearcoatRoughnessTexture", glMat.texture(GLMaterial::TextureType::ClearcoatRoughness));
                    fixTextureIndex(cc, "clearcoatNormalTexture", glMat.texture(GLMaterial::TextureType::ClearcoatNormal));

                    writeTransform(cc, "clearcoatTexture", glMat.texture(GLMaterial::TextureType::ClearcoatColor));
                    writeTransform(cc, "clearcoatRoughnessTexture", glMat.texture(GLMaterial::TextureType::ClearcoatRoughness));
                    writeTransform(cc, "clearcoatNormalTexture", glMat.texture(GLMaterial::TextureType::ClearcoatNormal));

                    updateTextureSampler(cc, "clearcoatTexture", glMat.texture(GLMaterial::TextureType::ClearcoatColor));
                    updateTextureSampler(cc, "clearcoatRoughnessTexture", glMat.texture(GLMaterial::TextureType::ClearcoatRoughness));
                    updateTextureSampler(cc, "clearcoatNormalTexture", glMat.texture(GLMaterial::TextureType::ClearcoatNormal));

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
                    fixTextureIndex(aniso, "anisotropyTexture", glMat.texture(GLMaterial::TextureType::Anisotropy));
                    writeTransform(aniso, "anisotropyTexture", glMat.texture(GLMaterial::TextureType::Anisotropy));
                    updateTextureSampler(aniso, "anisotropyTexture", glMat.texture(GLMaterial::TextureType::Anisotropy));

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
                    fixTextureIndex(sheen, "sheenColorTexture", glMat.texture(GLMaterial::TextureType::SheenColor));
                    fixTextureIndex(sheen, "sheenRoughnessTexture", glMat.texture(GLMaterial::TextureType::SheenRoughness));

                    writeTransform(sheen, "sheenColorTexture", glMat.texture(GLMaterial::TextureType::SheenColor));
                    writeTransform(sheen, "sheenRoughnessTexture", glMat.texture(GLMaterial::TextureType::SheenRoughness));

                    updateTextureSampler(sheen, "sheenColorTexture", glMat.texture(GLMaterial::TextureType::SheenColor));
                    updateTextureSampler(sheen, "sheenRoughnessTexture", glMat.texture(GLMaterial::TextureType::SheenRoughness));

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
                    fixTextureIndex(irid, "iridescenceTexture", glMat.texture(GLMaterial::TextureType::Iridescence));
                    fixTextureIndex(irid, "iridescenceThicknessTexture", glMat.texture(GLMaterial::TextureType::IridescenceThickness));

                    writeTransform(irid, "iridescenceTexture", glMat.texture(GLMaterial::TextureType::Iridescence));
                    writeTransform(irid, "iridescenceThicknessTexture", glMat.texture(GLMaterial::TextureType::IridescenceThickness));

                    updateTextureSampler(irid, "iridescenceTexture", glMat.texture(GLMaterial::TextureType::Iridescence));
                    updateTextureSampler(irid, "iridescenceThicknessTexture", glMat.texture(GLMaterial::TextureType::IridescenceThickness));

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
                    fixTextureIndex(spec, "specularTexture", glMat.texture(GLMaterial::TextureType::SpecularFactor));
                    fixTextureIndex(spec, "specularColorTexture", glMat.texture(GLMaterial::TextureType::SpecularColor));

                    writeTransform(spec, "specularTexture", glMat.texture(GLMaterial::TextureType::SpecularFactor));
                    writeTransform(spec, "specularColorTexture", glMat.texture(GLMaterial::TextureType::SpecularColor));

                    updateTextureSampler(spec, "specularTexture", glMat.texture(GLMaterial::TextureType::SpecularFactor));
                    updateTextureSampler(spec, "specularColorTexture", glMat.texture(GLMaterial::TextureType::SpecularColor));

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
                    fixTextureIndex(vol, "thicknessTexture", glMat.texture(GLMaterial::TextureType::Thickness));
                    writeTransform(vol, "thicknessTexture", glMat.texture(GLMaterial::TextureType::Thickness));
                    updateTextureSampler(vol, "thicknessTexture", glMat.texture(GLMaterial::TextureType::Thickness));

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
                    fixTextureIndex(trans, "transmissionTexture", glMat.texture(GLMaterial::TextureType::Transmission));
                    writeTransform(trans, "transmissionTexture", glMat.texture(GLMaterial::TextureType::Transmission));
                    updateTextureSampler(trans, "transmissionTexture", glMat.texture(GLMaterial::TextureType::Transmission));

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
                    fixTextureIndex(dt, "diffuseTransmissionTexture", glMat.texture(GLMaterial::TextureType::DiffuseTransmission));
                    fixTextureIndex(dt, "diffuseTransmissionColorTexture", glMat.texture(GLMaterial::TextureType::DiffuseTransmissionColor));

                    writeTransform(dt, "diffuseTransmissionTexture", glMat.texture(GLMaterial::TextureType::DiffuseTransmission));
                    writeTransform(dt, "diffuseTransmissionColorTexture", glMat.texture(GLMaterial::TextureType::DiffuseTransmissionColor));

                    updateTextureSampler(dt, "diffuseTransmissionTexture", glMat.texture(GLMaterial::TextureType::DiffuseTransmission));
                    updateTextureSampler(dt, "diffuseTransmissionColorTexture", glMat.texture(GLMaterial::TextureType::DiffuseTransmissionColor));

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
                    fixTextureIndex(sg, "diffuseTexture", glMat.texture(GLMaterial::TextureType::Diffuse));
                    fixTextureIndex(sg, "specularGlossinessTexture", glMat.texture(GLMaterial::TextureType::SpecularGlossiness));

                    writeTransform(sg, "diffuseTexture", glMat.texture(GLMaterial::TextureType::Diffuse));
                    writeTransform(sg, "specularGlossinessTexture", glMat.texture(GLMaterial::TextureType::SpecularGlossiness));

                    updateTextureSampler(sg, "diffuseTexture", glMat.texture(GLMaterial::TextureType::Diffuse));
                    updateTextureSampler(sg, "specularGlossinessTexture", glMat.texture(GLMaterial::TextureType::SpecularGlossiness));

                    exts["KHR_materials_pbrSpecularGlossiness"] = sg;
                    mat["extensions"] = exts;
                }
            }

            // Write correct material name from source GLMaterial
            QString correctName = glMat.name();
            if (!correctName.isEmpty())
                mat["name"] = correctName;

            materials[matIdx] = mat;
        }

        // Replace samplers array with our created samplers
        samplers = QJsonArray();
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

        // CRITICAL FIX: Remove invalid sampler references from textures
        // If Assimp created textures with sampler indices that no longer exist,
        // we need to remove those references to prevent "out of bounds" errors
        for (int i = 0; i < textures.size(); ++i)
        {
            QJsonObject tex = textures[i].toObject();

            int source = tex.value("source").toInt(-1);
            int samplerIdx = tex.value("sampler").toInt(-1);

            log(QString("  Texture[%1]: source=%2, sampler=%3").arg(i).arg(source).arg(samplerIdx), logCallback);

            if (tex.contains("sampler"))
            {
                if (samplerIdx >= samplers.size())
                {
                    log(QString("    -> INVALID sampler index %1 (max is %2), REMOVING")
                        .arg(samplerIdx).arg(samplers.size() - 1), logCallback);
                    tex.remove("sampler");
                    textures[i] = tex;
                }
                else
                {
                    log(QString("    -> Valid sampler index"), logCallback);
                }
            }
            else
            {
                log(QString("    -> No sampler reference"), logCallback);
            }
        }

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

                materials = dedupedMaterials;

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

    for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
    {
        if (!meshes[i]) continue;

        const GLMaterial& glMat = meshes[i]->getMaterial();
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

    for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
    {
        if (!meshes[i]) continue;

        const GLMaterial& glMat = meshes[i]->getMaterial();
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

    for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
    {
        if (!meshes[i]) continue;

        const GLMaterial& glMat = meshes[i]->getMaterial();
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

    for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
    {
        if (!meshes[i]) continue;

        const GLMaterial& glMat = meshes[i]->getMaterial();
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

    for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
    {
        if (!meshes[i])
        {
            log(QString("  -> Mesh %1 is null, skipping").arg(i), logCallback);
            continue;
        }

        const GLMaterial& glMat = meshes[i]->getMaterial();

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
