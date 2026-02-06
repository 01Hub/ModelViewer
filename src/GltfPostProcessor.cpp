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

        // Track which textures we've processed to write samplers
        QMap<int, const GLMaterial::Texture*> textureToSourceMap;

        // FIRST PASS: Track ALL textures from source materials for sampler updates
        for (int i = 0; i < materials.size() && i < static_cast<int>(meshes.size()); ++i)
        {
            if (!meshes[i]) continue;

            const GLMaterial& glMat = meshes[i]->getMaterial();
            QJsonObject mat = materials[i].toObject();

            // Helper to track texture index from material
            auto trackTexture = [&](const QJsonObject& parent, const QString& key, const GLMaterial::Texture& tex) {
                if (!parent.contains(key)) return;
                QJsonObject texInfo = parent[key].toObject();
                int texIndex = texInfo.value("index").toInt(-1);
                if (texIndex >= 0)
                {
                    textureToSourceMap[texIndex] = &tex;
                }
                };

            // Track PBR textures
            if (mat.contains("pbrMetallicRoughness"))
            {
                QJsonObject pbr = mat["pbrMetallicRoughness"].toObject();
                trackTexture(pbr, "baseColorTexture", glMat.texture(GLMaterial::TextureType::Albedo));
                trackTexture(pbr, "metallicRoughnessTexture", glMat.texture(GLMaterial::TextureType::Metallic));
            }

            // Track other textures
            trackTexture(mat, "normalTexture", glMat.texture(GLMaterial::TextureType::Normal));
            trackTexture(mat, "occlusionTexture", glMat.texture(GLMaterial::TextureType::AmbientOcclusion));
            trackTexture(mat, "emissiveTexture", glMat.texture(GLMaterial::TextureType::Emissive));
        }

        // SECOND PASS: Write transforms for textures that need them
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

        // THIRD PASS: Create samplers from source materials
        // Assimp doesn't handle samplers properly, so we create them ourselves

        // Clear any samplers Assimp may have created (they're unreliable)
        samplers = QJsonArray();

        // Track unique sampler configurations to deduplicate
        struct SamplerConfig
        {
            int magFilter;
            int minFilter;
            int wrapS;
            int wrapT;

            bool operator==(const SamplerConfig& other) const
            {
                return magFilter == other.magFilter &&
                    minFilter == other.minFilter &&
                    wrapS == other.wrapS &&
                    wrapT == other.wrapT;
            }
        };

        QMap<QString, int> samplerConfigToIndex;  // hash -> sampler index

        // Process each tracked texture and create/assign samplers
        for (auto it = textureToSourceMap.constBegin(); it != textureToSourceMap.constEnd(); ++it)
        {
            int texIndex = it.key();
            const GLMaterial::Texture* sourceTex = it.value();

            if (texIndex >= textures.size()) continue;

            QJsonObject texture = textures[texIndex].toObject();

            // Create sampler config hash
            QString configHash = QString("%1_%2_%3_%4")
                .arg(sourceTex->magFilter)
                .arg(sourceTex->minFilter)
                .arg(sourceTex->wrapS)
                .arg(sourceTex->wrapT);

            int samplerIndex;

            // Check if we already have this sampler configuration
            if (samplerConfigToIndex.contains(configHash))
            {
                // Reuse existing sampler
                samplerIndex = samplerConfigToIndex[configHash];
            }
            else
            {
                // Create new sampler
                samplerIndex = samplers.size();
                samplerConfigToIndex[configHash] = samplerIndex;

                QJsonObject sampler;
                sampler["magFilter"] = static_cast<int>(sourceTex->magFilter);
                sampler["minFilter"] = static_cast<int>(sourceTex->minFilter);
                sampler["wrapS"] = static_cast<int>(sourceTex->wrapS);
                sampler["wrapT"] = static_cast<int>(sourceTex->wrapT);

                samplers.append(sampler);
            }

            // Update texture to reference the sampler
            texture["sampler"] = samplerIndex;
            textures[texIndex] = texture;
        }

        gltfJson["materials"] = materials;

        if (samplers.size() > 0)
        {
            gltfJson["samplers"] = samplers;
            gltfJson["textures"] = textures;
            log(QString("  Created %1 sampler(s) from source materials").arg(samplers.size()), logCallback);
        }

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