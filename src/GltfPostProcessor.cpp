#include "GltfPostProcessor.h"
#include "GLMaterial.h"
#include "TriangleMesh.h"
#include <QDebug>
#include <QJsonParseError>
#include <QMap>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

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
    std::function<void(const QString&)> logCallback)
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
    postProcessGltfJsonWithMaterials(gltfJson, meshes, lights, logCallback);

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

    QJsonObject gltfJson = doc.object();

    // Post-process with material transforms
    postProcessGltfJsonWithMaterials(gltfJson, meshes, lights, logCallback);

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
        glm::quat rot = glm::rotation(defaultDir, dir);
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
    std::function<void(const QString&)> logCallback)
{
    log("=== glTF Post-Processor (with material transforms) ===", logCallback);

    removeTangentAttributes(gltfJson, logCallback);

    // First, write actual transforms from source materials
    if (gltfJson.contains("materials") && !meshes.empty())
    {
        QJsonArray materials = gltfJson["materials"].toArray();
        QJsonArray textures;
        QJsonArray samplers;

        if (gltfJson.contains("textures"))
            textures = gltfJson["textures"].toArray();
        if (gltfJson.contains("samplers"))
            samplers = gltfJson["samplers"].toArray();

        // Clear Assimp's samplers - we'll create correct ones from source materials
        samplers = QJsonArray();

        // PASS 1: Write transforms for textures that need them
        for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
        {
            if (!meshes[i]) continue;

            const GLMaterial& glMat = meshes[i]->getMaterial();
            QJsonObject mat = materials[i].toObject();

            // Helper to write transform from GLMaterial::Texture
            auto writeTransform = [&](QJsonObject& parent, const QString& key, const GLMaterial::Texture& tex) -> bool {
                if (!parent.contains(key)) return false;

                QJsonObject texInfo = parent[key].toObject();

                // Check if transform is non-identity
                bool hasTransform = (tex.scale.x != 1.0f || tex.scale.y != 1.0f ||
                    tex.offset.x != 0.0f || tex.offset.y != 0.0f ||
                    tex.rotation != 0.0f || tex.texCoordIndex != 0);

                if (!hasTransform) return false;

                QJsonObject extensions = texInfo.value("extensions").toObject();
                QJsonObject transform;

                QJsonArray scale, offset;
                scale.append(static_cast<double>(tex.scale.x));
                scale.append(static_cast<double>(tex.scale.y));
                offset.append(static_cast<double>(tex.offset.x));
                offset.append(static_cast<double>(tex.offset.y));

                transform["scale"] = scale;
                transform["offset"] = offset;
                transform["rotation"] = static_cast<double>(tex.rotation);

                if (tex.texCoordIndex != 0)
                    transform["texCoord"] = tex.texCoordIndex;

                extensions["KHR_texture_transform"] = transform;
                texInfo["extensions"] = extensions;
                parent[key] = texInfo;

                return true;
                };

            bool modified = false;

            // Write transforms for PBR textures
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                if (writeTransform(pbr, "baseColorTexture", glMat.texture(GLMaterial::TextureType::Albedo)))
                    modified = true;
                if (writeTransform(pbr, "metallicRoughnessTexture", glMat.texture(GLMaterial::TextureType::Metallic)))
                    modified = true;
                if (modified)
                    mat["pbrMetallicRoughness"] = pbr;
            }

            // Write transforms for other textures
            if (writeTransform(mat, "normalTexture", glMat.texture(GLMaterial::TextureType::Normal)))
                modified = true;
            if (writeTransform(mat, "occlusionTexture", glMat.texture(GLMaterial::TextureType::AmbientOcclusion)))
                modified = true;
            if (writeTransform(mat, "emissiveTexture", glMat.texture(GLMaterial::TextureType::Emissive)))
                modified = true;

            if (modified)
            {
                materials[i] = mat;
                log(QString("  Wrote transforms for material %1").arg(i), logCallback);
            }
        }

        // CORRECT APPROACH: Map materials[i] -> meshes[i], update texture samplers
        // Build deduplicated samplers based on actual source material properties

        log("  Building samplers from source materials...", logCallback);

        // Track unique sampler configurations
        QMap<QString, int> samplerConfigToIndex;  // hash -> sampler index
        QVector<QJsonObject> createdSamplers;

        // Helper to get or create a sampler
        auto getOrCreateSampler = [&](int mag, int min, int wrapS, int wrapT) -> int {
            QString hash = QString("%1_%2_%3_%4").arg(mag).arg(min).arg(wrapS).arg(wrapT);

            if (samplerConfigToIndex.contains(hash))
            {
                return samplerConfigToIndex[hash];
            }

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

        // Helper to update texture sampler from source material texture
        auto updateTextureSampler = [&](const QJsonObject& parent, const QString& key,
            const GLMaterial::Texture& sourceTex) {
                if (!parent.contains(key)) return;

                int texIndex = parent[key].toObject().value("index").toInt(-1);
                if (texIndex < 0 || texIndex >= textures.size()) return;

                // Get or create sampler with source material's properties
                int samplerIdx = getOrCreateSampler(
                    sourceTex.magFilter,
                    sourceTex.minFilter,
                    sourceTex.wrapS,
                    sourceTex.wrapT
                );

                // Update texture to reference this sampler
                QJsonObject tex = textures[texIndex].toObject();
                tex["sampler"] = samplerIdx;
                textures[texIndex] = tex;

                log(QString("    Material texture %1: texture[%2] -> sampler[%3]")
                    .arg(key).arg(texIndex).arg(samplerIdx), logCallback);
            };

        // Process each material
        for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
        {
            if (!meshes[i]) continue;

            const GLMaterial& glMat = meshes[i]->getMaterial();
            QJsonObject mat = materials[i].toObject();

            log(QString("  Material[%1]:").arg(i), logCallback);

            // Process PBR textures
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                updateTextureSampler(pbr, "baseColorTexture", glMat.texture(GLMaterial::TextureType::Albedo));
                updateTextureSampler(pbr, "metallicRoughnessTexture", glMat.texture(GLMaterial::TextureType::Metallic));
            }

            // Process other textures
            updateTextureSampler(mat, "normalTexture", glMat.texture(GLMaterial::TextureType::Normal));
            updateTextureSampler(mat, "occlusionTexture", glMat.texture(GLMaterial::TextureType::AmbientOcclusion));
            updateTextureSampler(mat, "emissiveTexture", glMat.texture(GLMaterial::TextureType::Emissive));

            // ===== WRITE BASIC MATERIAL PROPERTIES =====
            // Assimp may not handle these correctly, so we write them directly

            // alphaMode and alphaCutoff
            GLMaterial::BlendMode blendMode = glMat.blendMode();            
            if (blendMode == GLMaterial::BlendMode::Opaque)
            {
                mat["alphaMode"] = "OPAQUE";
            }
            else if (blendMode == GLMaterial::BlendMode::Masked)
            {
                mat["alphaMode"] = "MASK";
                mat["alphaCutoff"] = static_cast<double>(glMat.alphaThreshold());
            }
            else if (blendMode == GLMaterial::BlendMode::Alpha)
            {
                mat["alphaMode"] = "BLEND";
            }

            // doubleSided
            bool twoSided = glMat.twoSided();
            if (twoSided)
            {
                mat["doubleSided"] = true;
            }

            // emissiveFactor
            QVector3D emissive = glMat.emissive();
            if (emissive.length() > 0.0f)
            {
                QJsonArray emissiveArray;
                emissiveArray.append(static_cast<double>(emissive.x()));
                emissiveArray.append(static_cast<double>(emissive.y()));
                emissiveArray.append(static_cast<double>(emissive.z()));
                mat["emissiveFactor"] = emissiveArray;
            }

            // pbrMetallicRoughness factors
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();

                bool hasMetallicRoughnessTexture = pbr.contains("metallicRoughnessTexture");

                // Handle metallicFactor
                float metallicFactor = pbr.contains("metallicFactor") ?
                    static_cast<float>(pbr["metallicFactor"].toDouble()) :
                    glMat.metalness();

                // CRITICAL: If texture exists but metallicFactor is 0, override to 1.0
                // This handles cases where Assimp incorrectly exports metallicFactor: 0
                if (hasMetallicRoughnessTexture && metallicFactor == 0.0f)
                {
                    metallicFactor = 1.0f;  // glTF default when texture exists
                }

                pbr["metallicFactor"] = static_cast<double>(metallicFactor);

                // Handle baseColorFactor (including alpha for transparency)
                // Preserve existing baseColorFactor if present
                if (pbr.contains("baseColorFactor"))
                {
                    // Already has it - just make sure alpha is correct
                    QJsonArray existing = pbr["baseColorFactor"].toArray();
                    if (existing.size() >= 4)
                    {
                        // Update alpha channel
                        existing[3] = static_cast<double>(glMat.opacity());
                        pbr["baseColorFactor"] = existing;
                    }
                }
                else
                {
                    // Only add if non-default
                    QVector3D albedo = glMat.albedoColor();
                    float opacity = glMat.opacity();

                    bool isNonDefault = (std::abs(albedo.x() - 1.0f) > 0.001f) ||
                        (std::abs(albedo.y() - 1.0f) > 0.001f) ||
                        (std::abs(albedo.z() - 1.0f) > 0.001f) ||
                        (opacity < 0.999f);

                    if (isNonDefault)
                    {
                        QJsonArray baseColorArray;
                        baseColorArray.append(static_cast<double>(albedo.x()));
                        baseColorArray.append(static_cast<double>(albedo.y()));
                        baseColorArray.append(static_cast<double>(albedo.z()));
                        baseColorArray.append(static_cast<double>(opacity));
                        pbr["baseColorFactor"] = baseColorArray;
                    }
                }

                // Handle roughnessFactor  
                float roughnessFactor = pbr.contains("roughnessFactor") ?
                    static_cast<float>(pbr["roughnessFactor"].toDouble()) :
                    glMat.roughness();

                // If texture exists and roughness is very low, it might be wrong
                // But DON'T override - roughness can legitimately be 0 with a texture
                // (The texture modulates the base roughness value)

                pbr["roughnessFactor"] = static_cast<double>(roughnessFactor);

                mat["pbrMetallicRoughness"] = pbr;
            }

            // ===== WRITE KHR EXTENSIONS FROM SOURCE MATERIAL =====
            // Assimp doesn't handle these, so we write them directly to JSON

            QJsonObject extensions = mat.value("extensions").toObject();
            bool hasExtensions = false;

            // KHR_materials_transmission
            if (glMat.transmission() > 0.0f)
            {
                QJsonObject trans;
                trans["transmissionFactor"] = static_cast<double>(glMat.transmission());

                // Transmission texture
                const auto& transTex = glMat.texture(GLMaterial::TextureType::Transmission);
                if (!transTex.path.empty())
                {
                    // Find texture index for this texture
                    // TODO: Implement texture lookup and add transmissionTexture
                }

                extensions["KHR_materials_transmission"] = trans;
                hasExtensions = true;
            }

            // KHR_materials_ior
            float ior = glMat.ior();
            if (std::abs(ior - 1.5f) > 0.001f)
            {
                QJsonObject iorExt;
                iorExt["ior"] = static_cast<double>(ior);
                extensions["KHR_materials_ior"] = iorExt;
                hasExtensions = true;
            }

            // KHR_materials_clearcoat
            if (glMat.clearcoat() > 0.0f)
            {
                QJsonObject cc;
                cc["clearcoatFactor"] = static_cast<double>(glMat.clearcoat());
                cc["clearcoatRoughnessFactor"] = static_cast<double>(glMat.clearcoatRoughness());

                // TODO: Add clearcoat textures

                extensions["KHR_materials_clearcoat"] = cc;
                hasExtensions = true;
            }

            // KHR_materials_sheen
            QVector3D sheenColor = glMat.sheenColor();
            if (sheenColor.length() > 0.0f)
            {
                QJsonObject sheen;
                QJsonArray sheenColorArray;
                sheenColorArray.append(static_cast<double>(sheenColor.x()));
                sheenColorArray.append(static_cast<double>(sheenColor.y()));
                sheenColorArray.append(static_cast<double>(sheenColor.z()));
                sheen["sheenColorFactor"] = sheenColorArray;
                sheen["sheenRoughnessFactor"] = static_cast<double>(glMat.sheenRoughness());

                extensions["KHR_materials_sheen"] = sheen;
                hasExtensions = true;
            }

            // KHR_materials_iridescence
            if (glMat.iridescenceFactor() > 0.0f)
            {
                QJsonObject irid;
                irid["iridescenceFactor"] = static_cast<double>(glMat.iridescenceFactor());
                irid["iridescenceIor"] = static_cast<double>(glMat.iridescenceIor());
                irid["iridescenceThicknessMinimum"] = static_cast<double>(glMat.iridescenceThicknessMin());
                irid["iridescenceThicknessMaximum"] = static_cast<double>(glMat.iridescenceThicknessMax());

                extensions["KHR_materials_iridescence"] = irid;
                hasExtensions = true;
            }

            // KHR_materials_volume
            if (glMat.thicknessFactor() > 0.0f)
            {
                QJsonObject vol;
                vol["thicknessFactor"] = static_cast<double>(glMat.thicknessFactor());
                vol["attenuationDistance"] = static_cast<double>(glMat.attenuationDistance());

                QVector3D attenColor = glMat.attenuationColor();
                QJsonArray attenArray;
                attenArray.append(static_cast<double>(attenColor.x()));
                attenArray.append(static_cast<double>(attenColor.y()));
                attenArray.append(static_cast<double>(attenColor.z()));
                vol["attenuationColor"] = attenArray;

                // Add thicknessTexture if present
                QString thicknessMap = glMat.thicknessMap();
                if (!thicknessMap.isEmpty())
                {
                    int texIndex = findOrCreateTexture(gltfJson, thicknessMap);
                    if (texIndex >= 0)
                    {
                        QJsonObject texInfo;
                        texInfo["index"] = texIndex;
                        vol["thicknessTexture"] = texInfo;
                        log(QString("    -> Added thicknessTexture (index %1)").arg(texIndex), logCallback);
                    }
                }

                extensions["KHR_materials_volume"] = vol;
                hasExtensions = true;
            }

            // KHR_materials_specular
            float specularFactor = glMat.specularFactor();
            QVector3D specularColorFactor = glMat.specularColorFactor();
            QString specularFactorMap = glMat.specularFactorMap();
            QString specularColorMap = glMat.specularColorMap();

            bool hasSpecularExtension = (std::abs(specularFactor - 1.0f) > 0.001f) ||
                (specularColorFactor - QVector3D(1, 1, 1)).length() > 0.001f ||
                !specularFactorMap.isEmpty() ||
                !specularColorMap.isEmpty();

            if (hasSpecularExtension)
            {
                QJsonObject spec;
                spec["specularFactor"] = static_cast<double>(specularFactor);

                QJsonArray colorArray;
                colorArray.append(static_cast<double>(specularColorFactor.x()));
                colorArray.append(static_cast<double>(specularColorFactor.y()));
                colorArray.append(static_cast<double>(specularColorFactor.z()));
                spec["specularColorFactor"] = colorArray;

                // Add specularTexture if present
                if (!specularFactorMap.isEmpty())
                {                    
                    int texIndex = findOrCreateTexture(gltfJson, specularFactorMap, logCallback);

                    if (texIndex >= 0)
                    {
                        QJsonObject texInfo;
                        texInfo["index"] = texIndex;
                        spec["specularTexture"] = texInfo;
                        log(QString("    -> Set spec[\"specularTexture\"] to index %1").arg(texIndex), logCallback);
                    }
                    else
                    {
                        log("    -> texIndex was negative, NOT adding specularTexture", logCallback);
                    }
                }

                // Add specularColorTexture if present
                if (!specularColorMap.isEmpty())
                {                    
                    int texIndex = findOrCreateTexture(gltfJson, specularColorMap, logCallback);

                    if (texIndex >= 0)
                    {
                        QJsonObject texInfo;
                        texInfo["index"] = texIndex;
                        spec["specularColorTexture"] = texInfo;
                        log(QString("    -> Set spec[\"specularColorTexture\"] to index %1").arg(texIndex), logCallback);
                    }
                    else
                    {
                        log("    -> texIndex was negative, NOT adding specularColorTexture", logCallback);
                    }
                }

                if (spec.contains("specularTexture"))
                {
                    log(QString("  specularTexture: %1").arg(spec["specularTexture"].toObject().value("index").toInt(-999)), logCallback);
                }
                if (spec.contains("specularColorTexture"))
                {
                    log(QString("  specularColorTexture: %1").arg(spec["specularColorTexture"].toObject().value("index").toInt(-999)), logCallback);
                }

                extensions["KHR_materials_specular"] = spec;
                hasExtensions = true;
            }

            // KHR_materials_anisotropy
            if (glMat.anisotropyStrength() > 0.0f)
            {
                QJsonObject aniso;
                aniso["anisotropyStrength"] = static_cast<double>(glMat.anisotropyStrength());
                aniso["anisotropyRotation"] = static_cast<double>(glMat.anisotropyRotation());

                extensions["KHR_materials_anisotropy"] = aniso;
                hasExtensions = true;
            }

            // KHR_materials_dispersion
            if (glMat.dispersion() > 0.0f)
            {
                QJsonObject disp;
                disp["dispersion"] = static_cast<double>(glMat.dispersion());
                extensions["KHR_materials_dispersion"] = disp;
                hasExtensions = true;
            }

            // KHR_materials_emissive_strength
            if (std::abs(glMat.emissiveStrength() - 1.0f) > 0.001f)
            {
                QJsonObject emissiveStr;
                emissiveStr["emissiveStrength"] = static_cast<double>(glMat.emissiveStrength());
                extensions["KHR_materials_emissive_strength"] = emissiveStr;
                hasExtensions = true;
            }

            // KHR_materials_unlit
            if (glMat.isUnlit())
            {
                extensions["KHR_materials_unlit"] = QJsonObject();  // Empty object
                hasExtensions = true;
            }

            // Write extensions back to material
            if (hasExtensions)
            {
                mat["extensions"] = extensions;
            }

            // Always write material back (we may have modified pbrMetallicRoughness, alphaMode, etc.)
            materials[i] = mat;
        }
                
        // Replace samplers array with our created samplers
        samplers = QJsonArray();
        for (const auto& sampler : createdSamplers)
        {
            samplers.append(sampler);
        }
                
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
    QString imageFileName = QFileInfo(imagePath).fileName();
    log(QString("  Extracted filename: '%1'").arg(imageFileName), logCallback);

    // Look for EXISTING texture that already references this image
    // FIRST: Find the image index
    int imageIndex = -1;
    for (int i = 0; i < images.size(); ++i)
    {
        QJsonObject img = images[i].toObject();
        QString uri = img.value("uri").toString();
        log(QString("  Checking image %1: uri='%2'").arg(i).arg(uri), logCallback);

        if (uri.endsWith(imageFileName))
        {
            imageIndex = i;
            log(QString("  -> Found matching image at index %1").arg(i), logCallback);
            break;
        }
    }

    // If image not found, create it
    if (imageIndex < 0)
    {
        QString newUri = "textures/" + imageFileName;
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
