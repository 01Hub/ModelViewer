#include "GltfPostProcessor.h"
#include "GLMaterial.h"
#include "TriangleMesh.h"
#include <QDebug>
#include <QJsonParseError>
#include <QMap>

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
    postProcessGltfJsonWithMaterials(gltfJson, meshes, logCallback);

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
    postProcessGltfJsonWithMaterials(gltfJson, meshes, logCallback);

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

bool GltfPostProcessor::postProcessGltfJsonWithMaterials(
    QJsonObject& gltfJson,
    const std::vector<TriangleMesh*>& meshes,
    std::function<void(const QString&)> logCallback)
{
    log("=== glTF Post-Processor (with material transforms) ===", logCallback);

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

                extensions["KHR_materials_volume"] = vol;
                hasExtensions = true;
            }

            // KHR_materials_specular
            float specularFactor = glMat.specularFactor();
            QVector3D specularColorFactor = glMat.specularColorFactor();
            if (std::abs(specularFactor - 1.0f) > 0.001f ||
                (specularColorFactor - QVector3D(1, 1, 1)).length() > 0.001f)
            {
                QJsonObject spec;
                spec["specularFactor"] = static_cast<double>(specularFactor);

                QJsonArray colorArray;
                colorArray.append(static_cast<double>(specularColorFactor.x()));
                colorArray.append(static_cast<double>(specularColorFactor.y()));
                colorArray.append(static_cast<double>(specularColorFactor.z()));
                spec["specularColorFactor"] = colorArray;

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
                materials[i] = mat;
            }
        }

        // Replace samplers array with our created samplers
        samplers = QJsonArray();
        for (const auto& sampler : createdSamplers)
        {
            samplers.append(sampler);
        }

        // CRITICAL FIX: Remove invalid sampler references from textures
        // If Assimp created textures with sampler indices that no longer exist,
        // we need to remove those references to prevent "out of bounds" errors
        for (int i = 0; i < textures.size(); ++i)
        {
            QJsonObject tex = textures[i].toObject();
            if (tex.contains("sampler"))
            {
                int samplerIdx = tex.value("sampler").toInt(-1);
                if (samplerIdx >= samplers.size())
                {
                    tex.remove("sampler");
                    textures[i] = tex;
                    log(QString("    Removed invalid sampler[%1] from texture[%2]")
                        .arg(samplerIdx).arg(i), logCallback);
                }
            }
        }

        gltfJson["materials"] = materials;
        gltfJson["samplers"] = samplers;
        gltfJson["textures"] = textures;

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

    // Then do standard post-processing (fills in missing properties with defaults)
    return postProcessGltfJson(gltfJson, logCallback);
}bool GltfPostProcessor::fixTextureInfoWithTransforms(QJsonObject& parent, const QString& key)
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