#include "GLTFMetadataExtractor.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <iostream>
#include <cmath>

GLTFMetadataExtractor::GLTFMetadataExtractor()
{
}

bool GLTFMetadataExtractor::parseGLTFFile(const std::string& filePath)
{
    clear();

    QFile file(QString::fromStdString(filePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "GLTFMetadataExtractor: Failed to open glTF file:" << QString::fromStdString(filePath);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        qWarning() << "GLTFMetadataExtractor: Invalid JSON in glTF file";
        return false;
    }

    QJsonObject root = doc.object();

    // Check glTF version
    if (root.contains("asset"))
    {
        QJsonObject asset = root["asset"].toObject();
        QString version = asset["version"].toString();
        qDebug() << "GLTFMetadataExtractor: Parsing glTF version" << version;
    }

    // Parse all materials and their texture references
    parseMaterials(root);

    qDebug() << "GLTFMetadataExtractor: Successfully parsed glTF metadata"
        << "(" << _textureMetadata.size() << "textures)";

    return true;
}

TextureMetadata GLTFMetadataExtractor::getTextureMetadata(int textureIndex) const
{
    auto it = _textureMetadata.find(textureIndex);
    if (it != _textureMetadata.end())
    {
        return it->second;
    }
    // Return default metadata if not found
    return TextureMetadata();
}

std::map<std::string, TextureMetadata> GLTFMetadataExtractor::getMaterialTextureMetadata(int materialIndex) const
{
    std::map<std::string, TextureMetadata> result;

    auto it = _materialTextures.find(materialIndex);
    if (it != _materialTextures.end())
    {
        for (const auto& pair : it->second)
        {
            result[pair.first] = pair.second.second;
        }
    }

    return result;
}

bool GLTFMetadataExtractor::hasTextureMetadata(int textureIndex) const
{
    return _textureMetadata.find(textureIndex) != _textureMetadata.end();
}

void GLTFMetadataExtractor::clear()
{
    _textureMetadata.clear();
    _materialTextures.clear();
}

TextureMetadata GLTFMetadataExtractor::parseTextureInfo(const QJsonObject& textureInfoObj)
{
    TextureMetadata metadata;

    // Extract texture index
    if (textureInfoObj.contains("index"))
    {
        // Texture index is stored separately, not extracted here
    }

    // Extract texCoord index (defaults to 0 if not specified)
    metadata.texCoordIndex = extractTexCoordIndex(textureInfoObj);

    // Check for KHR_texture_transform extension
    if (textureInfoObj.contains("extensions"))
    {
        QJsonObject extensions = textureInfoObj["extensions"].toObject();

        if (extensions.contains("KHR_texture_transform"))
        {
            QJsonObject transform = extensions["KHR_texture_transform"].toObject();
            parseTextureTransform(transform, metadata);
        }
    }

    return metadata;
}

void GLTFMetadataExtractor::parseTextureTransform(const QJsonObject& transformObj, TextureMetadata& metadata)
{
    metadata.hasTransform = true;

    // Parse offset [u, v]
    if (transformObj.contains("offset"))
    {
        QJsonArray offsetArray = transformObj["offset"].toArray();
        if (offsetArray.size() >= 2)
        {
            metadata.offset.x = static_cast<float>(offsetArray[0].toDouble());
            metadata.offset.y = static_cast<float>(offsetArray[1].toDouble());
        }
    }

    // Parse scale [u, v]
    if (transformObj.contains("scale"))
    {
        QJsonArray scaleArray = transformObj["scale"].toArray();
        if (scaleArray.size() >= 2)
        {
            metadata.scale.x = static_cast<float>(scaleArray[0].toDouble());
            metadata.scale.y = static_cast<float>(scaleArray[1].toDouble());
        }
    }

    // Parse rotation (in radians)
    if (transformObj.contains("rotation"))
    {
        metadata.rotation = static_cast<float>(transformObj["rotation"].toDouble());
    }

    // Extract texCoord index from transform (overrides material-level texCoord)
    if (transformObj.contains("texCoord"))
    {
        metadata.texCoordIndex = transformObj["texCoord"].toInt(metadata.texCoordIndex);
    }

    qDebug() << "GLTFMetadataExtractor: Parsed KHR_texture_transform"
        << "offset:" << metadata.offset.x << metadata.offset.y
        << "scale:" << metadata.scale.x << metadata.scale.y
        << "rotation:" << metadata.rotation
        << "texCoord:" << metadata.texCoordIndex;
}

int GLTFMetadataExtractor::extractTexCoordIndex(const QJsonObject& textureInfoObj)
{
    if (textureInfoObj.contains("texCoord"))
    {
        return textureInfoObj["texCoord"].toInt(0);
    }
    return 0; // Default to TEXCOORD_0
}

void GLTFMetadataExtractor::parseMaterials(const QJsonObject& root)
{
    if (!root.contains("materials"))
    {
        return; // No materials in this glTF file
    }

    QJsonArray materialsArray = root["materials"].toArray();

    for (int matIdx = 0; matIdx < materialsArray.size(); ++matIdx)
    {
        QJsonObject material = materialsArray[matIdx].toObject();
        std::map<std::string, std::pair<int, TextureMetadata>> materialTextures;

        // === Parse pbrMetallicRoughness ===
        if (material.contains("pbrMetallicRoughness"))
        {
            QJsonObject pbr = material["pbrMetallicRoughness"].toObject();

            // Base Color / Albedo
            if (pbr.contains("baseColorTexture"))
            {
                QJsonObject baseColorTexInfo = pbr["baseColorTexture"].toObject();
                int texIndex = baseColorTexInfo["index"].toInt(-1);
                if (texIndex >= 0)
                {
                    TextureMetadata metadata = parseTextureInfo(baseColorTexInfo);
                    materialTextures["baseColor"] = std::make_pair(texIndex, metadata);
                    _textureMetadata[texIndex] = metadata;
                    qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "baseColor texture" << texIndex
                        << "texCoord:" << metadata.texCoordIndex;
                }
            }

            // Metallic Roughness (combined or separate)
            if (pbr.contains("metallicRoughnessTexture"))
            {
                QJsonObject metalRoughTexInfo = pbr["metallicRoughnessTexture"].toObject();
                int texIndex = metalRoughTexInfo["index"].toInt(-1);
                if (texIndex >= 0)
                {
                    TextureMetadata metadata = parseTextureInfo(metalRoughTexInfo);
                    materialTextures["metallicRoughness"] = std::make_pair(texIndex, metadata);
                    _textureMetadata[texIndex] = metadata;
                    qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "metallicRoughness texture" << texIndex
                        << "texCoord:" << metadata.texCoordIndex;
                }
            }
        }

        // === Normal Map ===
        if (material.contains("normalTexture"))
        {
            QJsonObject normalTexInfo = material["normalTexture"].toObject();
            int texIndex = normalTexInfo["index"].toInt(-1);
            if (texIndex >= 0)
            {
                TextureMetadata metadata = parseTextureInfo(normalTexInfo);
                materialTextures["normal"] = std::make_pair(texIndex, metadata);
                _textureMetadata[texIndex] = metadata;
                qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "normal texture" << texIndex
                    << "texCoord:" << metadata.texCoordIndex;
            }
        }

        // === Occlusion / AO Map ===
        if (material.contains("occlusionTexture"))
        {
            QJsonObject aoTexInfo = material["occlusionTexture"].toObject();
            int texIndex = aoTexInfo["index"].toInt(-1);
            if (texIndex >= 0)
            {
                TextureMetadata metadata = parseTextureInfo(aoTexInfo);
                materialTextures["occlusion"] = std::make_pair(texIndex, metadata);
                _textureMetadata[texIndex] = metadata;
                qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "occlusion texture" << texIndex
                    << "texCoord:" << metadata.texCoordIndex;
            }
        }

        // === Emissive Map ===
        if (material.contains("emissiveTexture"))
        {
            QJsonObject emissiveTexInfo = material["emissiveTexture"].toObject();
            int texIndex = emissiveTexInfo["index"].toInt(-1);
            if (texIndex >= 0)
            {
                TextureMetadata metadata = parseTextureInfo(emissiveTexInfo);
                materialTextures["emissive"] = std::make_pair(texIndex, metadata);
                _textureMetadata[texIndex] = metadata;
                qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "emissive texture" << texIndex
                    << "texCoord:" << metadata.texCoordIndex;
            }
        }

        // === KHR_materials_clearcoat ===
        if (material.contains("extensions"))
        {
            QJsonObject extensions = material["extensions"].toObject();

            if (extensions.contains("KHR_materials_clearcoat"))
            {
                QJsonObject clearcoat = extensions["KHR_materials_clearcoat"].toObject();

                if (clearcoat.contains("clearcoatTexture"))
                {
                    QJsonObject texInfo = clearcoat["clearcoatTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["clearcoat"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "clearcoat texture" << texIndex;
                    }
                }

                if (clearcoat.contains("clearcoatRoughnessTexture"))
                {
                    QJsonObject texInfo = clearcoat["clearcoatRoughnessTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["clearcoatRoughness"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "clearcoatRoughness texture" << texIndex;
                    }
                }

                if (clearcoat.contains("clearcoatNormalTexture"))
                {
                    QJsonObject texInfo = clearcoat["clearcoatNormalTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["clearcoatNormal"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "clearcoatNormal texture" << texIndex;
                    }
                }
            }

            // === KHR_materials_sheen ===
            if (extensions.contains("KHR_materials_sheen"))
            {
                QJsonObject sheen = extensions["KHR_materials_sheen"].toObject();

                if (sheen.contains("sheenColorTexture"))
                {
                    QJsonObject texInfo = sheen["sheenColorTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["sheenColor"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "sheenColor texture" << texIndex;
                    }
                }

                if (sheen.contains("sheenRoughnessTexture"))
                {
                    QJsonObject texInfo = sheen["sheenRoughnessTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["sheenRoughness"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "sheenRoughness texture" << texIndex;
                    }
                }
            }

            // === KHR_materials_transmission ===
            if (extensions.contains("KHR_materials_transmission"))
            {
                QJsonObject transmission = extensions["KHR_materials_transmission"].toObject();

                if (transmission.contains("transmissionTexture"))
                {
                    QJsonObject texInfo = transmission["transmissionTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["transmission"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "transmission texture" << texIndex;
                    }
                }
            }

            // === KHR_materials_volume ===
            if (extensions.contains("KHR_materials_volume"))
            {
                QJsonObject volume = extensions["KHR_materials_volume"].toObject();

                if (volume.contains("thicknessTexture"))
                {
                    QJsonObject texInfo = volume["thicknessTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["thickness"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "thickness texture" << texIndex;
                    }
                }
            }

            // === KHR_materials_specular ===
            if (extensions.contains("KHR_materials_specular"))
            {
                QJsonObject specular = extensions["KHR_materials_specular"].toObject();

                if (specular.contains("specularTexture"))
                {
                    QJsonObject texInfo = specular["specularTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["specularFactor"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "specularFactor texture" << texIndex;
                    }
                }

                if (specular.contains("specularColorTexture"))
                {
                    QJsonObject texInfo = specular["specularColorTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["specularColor"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "specularColor texture" << texIndex;
                    }
                }
            }

            // === KHR_materials_iridescence ===
            if (extensions.contains("KHR_materials_iridescence"))
            {
                QJsonObject iridescence = extensions["KHR_materials_iridescence"].toObject();

                if (iridescence.contains("iridescenceTexture"))
                {
                    QJsonObject texInfo = iridescence["iridescenceTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["iridescence"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "iridescence texture" << texIndex;
                    }
                }

                if (iridescence.contains("iridescenceThicknessTexture"))
                {
                    QJsonObject texInfo = iridescence["iridescenceThicknessTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["iridescenceThickness"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "iridescenceThickness texture" << texIndex;
                    }
                }
            }

            // === KHR_materials_anisotropy ===
            if (extensions.contains("KHR_materials_anisotropy"))
            {
                QJsonObject anisotropy = extensions["KHR_materials_anisotropy"].toObject();

                if (anisotropy.contains("anisotropyTexture"))
                {
                    QJsonObject texInfo = anisotropy["anisotropyTexture"].toObject();
                    int texIndex = texInfo["index"].toInt(-1);
                    if (texIndex >= 0)
                    {
                        TextureMetadata metadata = parseTextureInfo(texInfo);
                        materialTextures["anisotropy"] = std::make_pair(texIndex, metadata);
                        _textureMetadata[texIndex] = metadata;
                        qDebug() << "GLTFMetadataExtractor: Material" << matIdx << "anisotropy texture" << texIndex;
                    }
                }
            }
        }

        // Store all textures for this material
        _materialTextures[matIdx] = materialTextures;
    }
}
