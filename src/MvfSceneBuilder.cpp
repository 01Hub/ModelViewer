#include "MvfSceneBuilder.h"

#include "AssImpMesh.h"
#include "GLMaterial.h"
#include "SceneGraph.h"
#include "SceneNode.h"
#include "TextureLocationManager.h"
#include "TriangleMesh.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector2D>

#include <cstring>
#include <limits>

namespace
{
constexpr quint32 ComponentTypeFloat = 5126;
constexpr quint32 ComponentTypeUnsignedInt = 5125;
constexpr quint32 BufferTargetArray = 34962;
constexpr quint32 BufferTargetElementArray = 34963;

QJsonArray vec3ToJson(const QVector3D& v)
{
    return QJsonArray{v.x(), v.y(), v.z()};
}

QJsonArray matrixToJson(const aiMatrix4x4& m)
{
    return QJsonArray{
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4
    };
}

QString shadingModelToString(GLMaterial::ShadingModel model)
{
    switch (model)
    {
    case GLMaterial::ShadingModel::Unlit: return QStringLiteral("Unlit");
    case GLMaterial::ShadingModel::BlinnPhong: return QStringLiteral("BlinnPhong");
    case GLMaterial::ShadingModel::PBR: return QStringLiteral("PBR");
    case GLMaterial::ShadingModel::Toon: return QStringLiteral("Toon");
    }
    return QStringLiteral("Unknown");
}

QString blendModeToString(GLMaterial::BlendMode mode)
{
    switch (mode)
    {
    case GLMaterial::BlendMode::Opaque: return QStringLiteral("Opaque");
    case GLMaterial::BlendMode::Masked: return QStringLiteral("Masked");
    case GLMaterial::BlendMode::Alpha: return QStringLiteral("Alpha");
    case GLMaterial::BlendMode::Additive: return QStringLiteral("Additive");
    case GLMaterial::BlendMode::Multiply: return QStringLiteral("Multiply");
    }
    return QStringLiteral("Unknown");
}

QString textureTypeKey(GLMaterial::TextureType type)
{
    switch (type)
    {
    case GLMaterial::TextureType::Albedo: return QStringLiteral("baseColorTexture");
    case GLMaterial::TextureType::Normal: return QStringLiteral("normalTexture");
    case GLMaterial::TextureType::AmbientOcclusion: return QStringLiteral("occlusionTexture");
    case GLMaterial::TextureType::Emissive: return QStringLiteral("emissiveTexture");
    case GLMaterial::TextureType::Metallic: return QStringLiteral("metallicTexture");
    case GLMaterial::TextureType::Roughness: return QStringLiteral("roughnessTexture");
    case GLMaterial::TextureType::Transmission: return QStringLiteral("transmissionTexture");
    case GLMaterial::TextureType::IOR: return QStringLiteral("iorTexture");
    case GLMaterial::TextureType::SheenColor: return QStringLiteral("sheenColorTexture");
    case GLMaterial::TextureType::SheenRoughness: return QStringLiteral("sheenRoughnessTexture");
    case GLMaterial::TextureType::ClearcoatColor: return QStringLiteral("clearcoatTexture");
    case GLMaterial::TextureType::ClearcoatRoughness: return QStringLiteral("clearcoatRoughnessTexture");
    case GLMaterial::TextureType::ClearcoatNormal: return QStringLiteral("clearcoatNormalTexture");
    case GLMaterial::TextureType::Iridescence: return QStringLiteral("iridescenceTexture");
    case GLMaterial::TextureType::IridescenceThickness: return QStringLiteral("iridescenceThicknessTexture");
    case GLMaterial::TextureType::SpecularFactor: return QStringLiteral("specularTexture");
    case GLMaterial::TextureType::SpecularColor: return QStringLiteral("specularColorTexture");
    case GLMaterial::TextureType::Anisotropy: return QStringLiteral("anisotropyTexture");
    case GLMaterial::TextureType::DiffuseTransmission: return QStringLiteral("diffuseTransmissionTexture");
    case GLMaterial::TextureType::DiffuseTransmissionColor: return QStringLiteral("diffuseTransmissionColorTexture");
    case GLMaterial::TextureType::Thickness: return QStringLiteral("thicknessTexture");
    case GLMaterial::TextureType::Diffuse: return QStringLiteral("diffuseTexture");
    case GLMaterial::TextureType::SpecularGlossiness: return QStringLiteral("specularGlossinessTexture");
    case GLMaterial::TextureType::Opacity: return QStringLiteral("opacityTexture");
    case GLMaterial::TextureType::Height: return QStringLiteral("heightTexture");
    case GLMaterial::TextureType::Count:
    default: return QString();
    }
}

QJsonObject buildTextureInfoObject(int textureIndex, const GLMaterial::Texture& texture)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("index"), textureIndex);
    if (texture.texCoordIndex != 0)
        obj.insert(QStringLiteral("texCoord"), texture.texCoordIndex);

    QJsonObject transform;
    if (texture.scale.x != 1.0f || texture.scale.y != 1.0f)
        transform.insert(QStringLiteral("scale"), QJsonArray{texture.scale.x, texture.scale.y});
    if (texture.offset.x != 0.0f || texture.offset.y != 0.0f)
        transform.insert(QStringLiteral("offset"), QJsonArray{texture.offset.x, texture.offset.y});
    if (texture.rotation != 0.0f)
        transform.insert(QStringLiteral("rotation"), texture.rotation);
    if (!transform.isEmpty())
    {
        QJsonObject extensions;
        extensions.insert(QStringLiteral("KHR_texture_transform"), transform);
        obj.insert(QStringLiteral("extensions"), extensions);
    }
    return obj;
}

void alignBuffer(QByteArray& buffer, int alignment = 4)
{
    while (buffer.size() % alignment != 0)
        buffer.append(char(0));
}

template <typename T>
int appendBinary(QByteArray& buffer, const T* data, int count)
{
    if (count <= 0)
        return buffer.size();

    alignBuffer(buffer);
    const int offset = buffer.size();
    buffer.append(reinterpret_cast<const char*>(data), static_cast<int>(sizeof(T) * count));
    return offset;
}

QJsonObject makeBufferView(int bufferIndex,
                           quint64 byteOffset,
                           quint64 byteLength,
                           quint32 target,
                           const QString& name = {})
{
    QJsonObject obj;
    obj.insert(QStringLiteral("buffer"), bufferIndex);
    obj.insert(QStringLiteral("byteOffset"), static_cast<qint64>(byteOffset));
    obj.insert(QStringLiteral("byteLength"), static_cast<qint64>(byteLength));
    if (target != 0)
        obj.insert(QStringLiteral("target"), static_cast<int>(target));
    if (!name.isEmpty())
        obj.insert(QStringLiteral("name"), name);
    return obj;
}

QJsonObject makeAccessor(int bufferView,
                         quint64 byteOffset,
                         quint32 componentType,
                         quint64 count,
                         const QString& type,
                         const QString& name = {},
                         const QJsonArray& min = {},
                         const QJsonArray& max = {})
{
    QJsonObject obj;
    obj.insert(QStringLiteral("bufferView"), bufferView);
    obj.insert(QStringLiteral("byteOffset"), static_cast<qint64>(byteOffset));
    obj.insert(QStringLiteral("componentType"), static_cast<int>(componentType));
    obj.insert(QStringLiteral("count"), static_cast<qint64>(count));
    obj.insert(QStringLiteral("type"), type);
    if (!name.isEmpty())
        obj.insert(QStringLiteral("name"), name);
    if (!min.isEmpty())
        obj.insert(QStringLiteral("min"), min);
    if (!max.isEmpty())
        obj.insert(QStringLiteral("max"), max);
    return obj;
}
}

namespace Mvf
{
MVFPackage buildMVFPackage(const SceneGraph& sceneGraph,
                                   const std::vector<TriangleMesh*>& meshStore,
                                   const QSet<QUuid>& visibleMeshUuids,
                                   const QSet<QUuid>& selectedMeshUuids)
{
    MVFPackage package;
    Document& document = package.document;

    QJsonObject geomBuffer;
    geomBuffer.insert(QStringLiteral("name"), QStringLiteral("geometry"));
    geomBuffer.insert(QStringLiteral("chunk"), QStringLiteral("GEOM"));
    geomBuffer.insert(QStringLiteral("byteLength"), 0);
    document.buffers.append(geomBuffer);

    QJsonObject imageBuffer;
    imageBuffer.insert(QStringLiteral("name"), QStringLiteral("images"));
    imageBuffer.insert(QStringLiteral("chunk"), QStringLiteral("IMGS"));
    imageBuffer.insert(QStringLiteral("byteLength"), 0);
    document.buffers.append(imageBuffer);

    QHash<QUuid, int> meshIndexByUuid;
    QHash<QUuid, int> materialIndexByUuid;
    QHash<QString, int> imageIndexByPath;
    QHash<QString, int> samplerIndexBySignature;
    QHash<QString, int> textureIndexBySignature;

    for (TriangleMesh* mesh : meshStore)
    {
        if (!mesh)
            continue;

        const QUuid meshUuid = mesh->uuid();
        meshIndexByUuid.insert(meshUuid, document.meshes.size());

        QJsonObject meshObj;
        meshObj.insert(QStringLiteral("id"), meshUuid.toString(QUuid::WithoutBraces));
        meshObj.insert(QStringLiteral("name"), mesh->getName());

        QJsonObject primitive;
        primitive.insert(QStringLiteral("mode"), static_cast<int>(mesh->getPrimitiveMode()));

        QJsonObject primitiveExtras;
        primitiveExtras.insert(QStringLiteral("sceneIndex"), mesh->getSceneIndex());
        primitiveExtras.insert(QStringLiteral("meshUuid"), meshUuid.toString(QUuid::WithoutBraces));
        primitiveExtras.insert(QStringLiteral("hasGeometryChunk"), true);

        if (const auto* assImpMesh = dynamic_cast<const AssImpMesh*>(mesh))
        {
            const std::vector<Vertex> vertices = assImpMesh->vertices();
            const std::vector<unsigned int> indices = assImpMesh->indices();

            primitiveExtras.insert(QStringLiteral("vertexCount"), static_cast<int>(vertices.size()));
            primitiveExtras.insert(QStringLiteral("indexCount"), static_cast<int>(indices.size()));

            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> tangents;
            std::vector<float> colors;
            std::vector<float> uv0;
            std::vector<float> uv1;
            std::vector<float> uv2;
            std::vector<float> uv3;
            positions.reserve(vertices.size() * 3);
            normals.reserve(vertices.size() * 3);
            tangents.reserve(vertices.size() * 3);
            colors.reserve(vertices.size() * 4);
            uv0.reserve(vertices.size() * 2);
            uv1.reserve(vertices.size() * 2);
            uv2.reserve(vertices.size() * 2);
            uv3.reserve(vertices.size() * 2);

            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float minZ = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();
            float maxZ = std::numeric_limits<float>::lowest();

            bool hasColor = false;
            bool hasUv1 = false;
            bool hasUv2 = false;
            bool hasUv3 = false;

            for (const Vertex& v : vertices)
            {
                positions.push_back(v.Position.x);
                positions.push_back(v.Position.y);
                positions.push_back(v.Position.z);

                normals.push_back(v.Normal.x);
                normals.push_back(v.Normal.y);
                normals.push_back(v.Normal.z);

                tangents.push_back(v.Tangent.x);
                tangents.push_back(v.Tangent.y);
                tangents.push_back(v.Tangent.z);

                colors.push_back(v.Color.r);
                colors.push_back(v.Color.g);
                colors.push_back(v.Color.b);
                colors.push_back(v.Color.a);

                uv0.push_back(v.TexCoords[0].x);
                uv0.push_back(v.TexCoords[0].y);
                uv1.push_back(v.TexCoords[1].x);
                uv1.push_back(v.TexCoords[1].y);
                uv2.push_back(v.TexCoords[2].x);
                uv2.push_back(v.TexCoords[2].y);
                uv3.push_back(v.TexCoords[3].x);
                uv3.push_back(v.TexCoords[3].y);

                minX = std::min(minX, v.Position.x);
                minY = std::min(minY, v.Position.y);
                minZ = std::min(minZ, v.Position.z);
                maxX = std::max(maxX, v.Position.x);
                maxY = std::max(maxY, v.Position.y);
                maxZ = std::max(maxZ, v.Position.z);

                hasColor = hasColor || v.Color != glm::vec4(1.0f);
                hasUv1 = hasUv1 || v.TexCoords[1] != glm::vec2(0.0f);
                hasUv2 = hasUv2 || v.TexCoords[2] != glm::vec2(0.0f);
                hasUv3 = hasUv3 || v.TexCoords[3] != glm::vec2(0.0f);
            }

            QJsonObject attributes;

            const int posOffset = appendBinary(package.geometryChunk, positions.data(), static_cast<int>(positions.size()));
            const int posView = document.bufferViews.size();
            document.bufferViews.append(makeBufferView(0, posOffset, positions.size() * sizeof(float), BufferTargetArray,
                                                       QStringLiteral("%1_POSITION").arg(mesh->getName())));
            const int posAccessor = document.accessors.size();
            document.accessors.append(makeAccessor(posView, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC3"),
                                                   QStringLiteral("%1_POSITION").arg(mesh->getName()),
                                                   QJsonArray{minX, minY, minZ},
                                                   QJsonArray{maxX, maxY, maxZ}));
            attributes.insert(QStringLiteral("POSITION"), posAccessor);

            const int normalOffset = appendBinary(package.geometryChunk, normals.data(), static_cast<int>(normals.size()));
            const int normalView = document.bufferViews.size();
            document.bufferViews.append(makeBufferView(0, normalOffset, normals.size() * sizeof(float), BufferTargetArray,
                                                       QStringLiteral("%1_NORMAL").arg(mesh->getName())));
            const int normalAccessor = document.accessors.size();
            document.accessors.append(makeAccessor(normalView, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC3"),
                                                   QStringLiteral("%1_NORMAL").arg(mesh->getName())));
            attributes.insert(QStringLiteral("NORMAL"), normalAccessor);

            const int tangentOffset = appendBinary(package.geometryChunk, tangents.data(), static_cast<int>(tangents.size()));
            const int tangentView = document.bufferViews.size();
            document.bufferViews.append(makeBufferView(0, tangentOffset, tangents.size() * sizeof(float), BufferTargetArray,
                                                       QStringLiteral("%1_TANGENT").arg(mesh->getName())));
            const int tangentAccessor = document.accessors.size();
            document.accessors.append(makeAccessor(tangentView, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC3"),
                                                   QStringLiteral("%1_TANGENT").arg(mesh->getName())));
            attributes.insert(QStringLiteral("TANGENT"), tangentAccessor);

            const int uv0Offset = appendBinary(package.geometryChunk, uv0.data(), static_cast<int>(uv0.size()));
            const int uv0View = document.bufferViews.size();
            document.bufferViews.append(makeBufferView(0, uv0Offset, uv0.size() * sizeof(float), BufferTargetArray,
                                                       QStringLiteral("%1_TEXCOORD_0").arg(mesh->getName())));
            const int uv0Accessor = document.accessors.size();
            document.accessors.append(makeAccessor(uv0View, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC2"),
                                                   QStringLiteral("%1_TEXCOORD_0").arg(mesh->getName())));
            attributes.insert(QStringLiteral("TEXCOORD_0"), uv0Accessor);

            if (hasUv1)
            {
                const int uv1Offset = appendBinary(package.geometryChunk, uv1.data(), static_cast<int>(uv1.size()));
                const int uv1View = document.bufferViews.size();
                document.bufferViews.append(makeBufferView(0, uv1Offset, uv1.size() * sizeof(float), BufferTargetArray,
                                                           QStringLiteral("%1_TEXCOORD_1").arg(mesh->getName())));
                const int uv1Accessor = document.accessors.size();
                document.accessors.append(makeAccessor(uv1View, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC2"),
                                                       QStringLiteral("%1_TEXCOORD_1").arg(mesh->getName())));
                attributes.insert(QStringLiteral("TEXCOORD_1"), uv1Accessor);
            }

            if (hasUv2)
            {
                const int uv2Offset = appendBinary(package.geometryChunk, uv2.data(), static_cast<int>(uv2.size()));
                const int uv2View = document.bufferViews.size();
                document.bufferViews.append(makeBufferView(0, uv2Offset, uv2.size() * sizeof(float), BufferTargetArray,
                                                           QStringLiteral("%1_TEXCOORD_2").arg(mesh->getName())));
                const int uv2Accessor = document.accessors.size();
                document.accessors.append(makeAccessor(uv2View, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC2"),
                                                       QStringLiteral("%1_TEXCOORD_2").arg(mesh->getName())));
                attributes.insert(QStringLiteral("TEXCOORD_2"), uv2Accessor);
            }

            if (hasUv3)
            {
                const int uv3Offset = appendBinary(package.geometryChunk, uv3.data(), static_cast<int>(uv3.size()));
                const int uv3View = document.bufferViews.size();
                document.bufferViews.append(makeBufferView(0, uv3Offset, uv3.size() * sizeof(float), BufferTargetArray,
                                                           QStringLiteral("%1_TEXCOORD_3").arg(mesh->getName())));
                const int uv3Accessor = document.accessors.size();
                document.accessors.append(makeAccessor(uv3View, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC2"),
                                                       QStringLiteral("%1_TEXCOORD_3").arg(mesh->getName())));
                attributes.insert(QStringLiteral("TEXCOORD_3"), uv3Accessor);
            }

            if (hasColor)
            {
                const int colorOffset = appendBinary(package.geometryChunk, colors.data(), static_cast<int>(colors.size()));
                const int colorView = document.bufferViews.size();
                document.bufferViews.append(makeBufferView(0, colorOffset, colors.size() * sizeof(float), BufferTargetArray,
                                                           QStringLiteral("%1_COLOR_0").arg(mesh->getName())));
                const int colorAccessor = document.accessors.size();
                document.accessors.append(makeAccessor(colorView, 0, ComponentTypeFloat, vertices.size(), QStringLiteral("VEC4"),
                                                       QStringLiteral("%1_COLOR_0").arg(mesh->getName())));
                attributes.insert(QStringLiteral("COLOR_0"), colorAccessor);
            }

            primitive.insert(QStringLiteral("attributes"), attributes);

            const int indexOffset = appendBinary(package.geometryChunk, indices.data(), static_cast<int>(indices.size()));
            const int indexView = document.bufferViews.size();
            document.bufferViews.append(makeBufferView(0, indexOffset, indices.size() * sizeof(unsigned int), BufferTargetElementArray,
                                                       QStringLiteral("%1_INDICES").arg(mesh->getName())));
            const int indexAccessor = document.accessors.size();
            document.accessors.append(makeAccessor(indexView, 0, ComponentTypeUnsignedInt, indices.size(), QStringLiteral("SCALAR"),
                                                   QStringLiteral("%1_INDICES").arg(mesh->getName())));
            primitive.insert(QStringLiteral("indices"), indexAccessor);
        }

        const GLMaterial material = mesh->getMaterial();
        QJsonObject materialObj;
        materialObj.insert(QStringLiteral("id"), meshUuid.toString(QUuid::WithoutBraces));
        materialObj.insert(QStringLiteral("name"), material.name());
        materialObj.insert(QStringLiteral("shadingModel"), shadingModelToString(material.shadingModel()));
        materialObj.insert(QStringLiteral("blendMode"), blendModeToString(material.blendMode()));
        materialObj.insert(QStringLiteral("doubleSided"), material.twoSided());
        materialObj.insert(QStringLiteral("alphaCutoff"), material.alphaThreshold());
        materialObj.insert(QStringLiteral("opacity"), material.opacity());

        QJsonObject pbr;
        pbr.insert(QStringLiteral("baseColorFactor"),
                   QJsonArray{material.albedoColor().x(), material.albedoColor().y(),
                              material.albedoColor().z(), material.opacity()});
        pbr.insert(QStringLiteral("metallicFactor"), material.metalness());
        pbr.insert(QStringLiteral("roughnessFactor"), material.roughness());
        materialObj.insert(QStringLiteral("pbr"), pbr);

        QJsonObject extensions;
        QJsonObject ads;
        ads.insert(QStringLiteral("ambient"), vec3ToJson(material.ambient()));
        ads.insert(QStringLiteral("diffuse"), vec3ToJson(material.diffuse()));
        ads.insert(QStringLiteral("specular"), vec3ToJson(material.specular()));
        ads.insert(QStringLiteral("emissive"), vec3ToJson(material.emissive()));
        ads.insert(QStringLiteral("shininess"), material.shininess());
        extensions.insert(QStringLiteral("MVF_material_ads"), ads);

        QJsonObject mvfPbr;
        mvfPbr.insert(QStringLiteral("ior"), material.ior());
        mvfPbr.insert(QStringLiteral("transmission"), material.transmission());
        mvfPbr.insert(QStringLiteral("clearcoat"), material.clearcoat());
        mvfPbr.insert(QStringLiteral("clearcoatRoughness"), material.clearcoatRoughness());
        mvfPbr.insert(QStringLiteral("sheenColor"), vec3ToJson(material.sheenColor()));
        mvfPbr.insert(QStringLiteral("sheenRoughness"), material.sheenRoughness());

        for (int textureTypeIndex = 0;
             textureTypeIndex < static_cast<int>(GLMaterial::TextureType::Count);
             ++textureTypeIndex)
        {
            const auto type = static_cast<GLMaterial::TextureType>(textureTypeIndex);
            const GLMaterial::Texture& texture = material.texture(type);
            if (texture.path.empty())
                continue;

            const QString imagePath = QString::fromStdString(texture.path);
            int imageIndex = imageIndexByPath.value(imagePath, -1);
            if (imageIndex < 0)
            {
                imageIndex = document.images.size();
                imageIndexByPath.insert(imagePath, imageIndex);

                QJsonObject imageObj;
                imageObj.insert(QStringLiteral("name"), QFileInfo(imagePath).fileName());
                imageObj.insert(QStringLiteral("originalUri"), imagePath);
                imageObj.insert(QStringLiteral("bufferView"), -1);
                imageObj.insert(QStringLiteral("mimeType"), QString());
                imageObj.insert(QStringLiteral("byteLength"), 0);
                document.images.append(imageObj);
            }

            const QString samplerSignature = QStringLiteral("%1|%2|%3|%4")
                .arg(static_cast<int>(texture.magFilter))
                .arg(static_cast<int>(texture.minFilter))
                .arg(static_cast<int>(texture.wrapS))
                .arg(static_cast<int>(texture.wrapT));

            int samplerIndex = samplerIndexBySignature.value(samplerSignature, -1);
            if (samplerIndex < 0)
            {
                samplerIndex = document.samplers.size();
                samplerIndexBySignature.insert(samplerSignature, samplerIndex);

                QJsonObject samplerObj;
                samplerObj.insert(QStringLiteral("magFilter"), static_cast<int>(texture.magFilter));
                samplerObj.insert(QStringLiteral("minFilter"), static_cast<int>(texture.minFilter));
                samplerObj.insert(QStringLiteral("wrapS"), static_cast<int>(texture.wrapS));
                samplerObj.insert(QStringLiteral("wrapT"), static_cast<int>(texture.wrapT));
                document.samplers.append(samplerObj);
            }

            const QString textureSignature = QStringLiteral("%1|%2")
                .arg(imageIndex)
                .arg(samplerIndex);

            int textureIndex = textureIndexBySignature.value(textureSignature, -1);
            if (textureIndex < 0)
            {
                textureIndex = document.textures.size();
                textureIndexBySignature.insert(textureSignature, textureIndex);

                QJsonObject textureObj;
                textureObj.insert(QStringLiteral("image"), imageIndex);
                textureObj.insert(QStringLiteral("sampler"), samplerIndex);
                document.textures.append(textureObj);
            }

            const QString key = textureTypeKey(type);
            if (!key.isEmpty())
                mvfPbr.insert(key, buildTextureInfoObject(textureIndex, texture));
        }

        extensions.insert(QStringLiteral("MVF_material_pbr"), mvfPbr);
        materialObj.insert(QStringLiteral("extensions"), extensions);
        materialObj.insert(QStringLiteral("extras"), QJsonObject{
            {QStringLiteral("meshUuid"), meshUuid.toString(QUuid::WithoutBraces)}
        });
        document.materials.append(materialObj);
        const int materialIndex = document.materials.size() - 1;
        materialIndexByUuid.insert(meshUuid, materialIndex);
        primitive.insert(QStringLiteral("material"), materialIndex);
        primitive.insert(QStringLiteral("extras"), primitiveExtras);

        QJsonArray primitives;
        primitives.append(primitive);
        meshObj.insert(QStringLiteral("primitives"), primitives);
        document.meshes.append(meshObj);
    }

    QHash<const SceneNode*, int> nodeIndexByPtr;
    std::function<int(const SceneNode*)> appendNode = [&](const SceneNode* node) -> int {
        QJsonObject nodeObj;
        nodeObj.insert(QStringLiteral("id"), node->nodeUuid.toString(QUuid::WithoutBraces));
        nodeObj.insert(QStringLiteral("name"), node->name);
        nodeObj.insert(QStringLiteral("matrix"), matrixToJson(node->localTransform));

        if (!node->meshUuids.isEmpty())
        {
            QJsonArray meshBindings;
            for (const QUuid& meshUuid : node->meshUuids)
            {
                QJsonObject bindingObj;
                bindingObj.insert(QStringLiteral("uuid"), meshUuid.toString(QUuid::WithoutBraces));
                bindingObj.insert(QStringLiteral("visible"), visibleMeshUuids.contains(meshUuid));
                bindingObj.insert(QStringLiteral("mesh"), meshIndexByUuid.value(meshUuid, -1));
                bindingObj.insert(QStringLiteral("materialOverride"), materialIndexByUuid.value(meshUuid, -1));
                meshBindings.append(bindingObj);
            }
            nodeObj.insert(QStringLiteral("meshBindings"), meshBindings);
        }

        document.nodes.append(nodeObj);
        const int nodeIndex = document.nodes.size() - 1;
        nodeIndexByPtr.insert(node, nodeIndex);

        QJsonArray children;
        for (const SceneNode* child : node->children)
            children.append(appendNode(child));

        QJsonObject updatedNode = document.nodes.at(nodeIndex).toObject();
        if (!children.isEmpty())
            updatedNode.insert(QStringLiteral("children"), children);
        document.nodes.replace(nodeIndex, updatedNode);
        return nodeIndex;
    };

    QJsonArray sceneRoots;
    for (const SceneNode* child : sceneGraph.root()->children)
        sceneRoots.append(appendNode(child));

    QJsonObject sceneObj;
    sceneObj.insert(QStringLiteral("name"), QStringLiteral("Default Scene"));
    sceneObj.insert(QStringLiteral("nodes"), sceneRoots);
    document.scenes.append(sceneObj);
    document.scene = 0;

    QJsonArray visibleArray;
    for (const QUuid& uuid : visibleMeshUuids)
        visibleArray.append(uuid.toString(QUuid::WithoutBraces));

    QJsonArray selectedArray;
    for (const QUuid& uuid : selectedMeshUuids)
        selectedArray.append(uuid.toString(QUuid::WithoutBraces));

    document.mvfSession.insert(QStringLiteral("visibleMeshUuids"), visibleArray);
    document.mvfSession.insert(QStringLiteral("selectedMeshUuids"), selectedArray);
    document.mvfSession.insert(QStringLiteral("geometryChunkPresent"), !package.geometryChunk.isEmpty());
    document.extensionsUsed.append(QStringLiteral("MVF_material_ads"));
    document.extensionsUsed.append(QStringLiteral("MVF_material_pbr"));

    QJsonObject updatedGeomBuffer = document.buffers.at(0).toObject();
    updatedGeomBuffer.insert(QStringLiteral("byteLength"), package.geometryChunk.size());
    document.buffers.replace(0, updatedGeomBuffer);

    // --- Image embedding ---
    // Try to read each image's source file and append it to the IMGS chunk.
    // glb:// URIs are resolved via TextureLocationManager (uses its cache).
    // KTX2 files are skipped (complex GPU-compressed format; path-only fallback).
    {
        TextureLocationManager resolver;

        for (int imgIdx = 0; imgIdx < document.images.size(); ++imgIdx)
        {
            QJsonObject imageObj = document.images[imgIdx].toObject();
            const QString origUri = imageObj[QStringLiteral("originalUri")].toString();
            if (origUri.isEmpty())
                continue;

            // Resolve the path
            QString resolvedPath = origUri;
            if (origUri.startsWith(QStringLiteral("glb://")))
            {
                const TextureMetadata meta = resolver.resolveTexture(origUri);
                if (meta.resolvedPath.isEmpty())
                    continue;
                resolvedPath = meta.resolvedPath;
            }

            const QString ext = QFileInfo(resolvedPath).suffix().toLower();
            if (ext == QLatin1String("ktx2"))
                continue;   // skip GPU-compressed textures

            QFile f(resolvedPath);
            if (!f.exists() || !f.open(QIODevice::ReadOnly))
                continue;
            const QByteArray fileData = f.readAll();
            if (fileData.isEmpty())
                continue;

            QString mimeType;
            if      (ext == QLatin1String("png"))  mimeType = QStringLiteral("image/png");
            else if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg"))
                                                   mimeType = QStringLiteral("image/jpeg");
            else if (ext == QLatin1String("webp")) mimeType = QStringLiteral("image/webp");
            else if (ext == QLatin1String("bmp"))  mimeType = QStringLiteral("image/bmp");
            else                                   mimeType = QStringLiteral("application/octet-stream");

            alignBuffer(package.imageChunk);
            const quint64 imgByteOffset = static_cast<quint64>(package.imageChunk.size());
            package.imageChunk.append(fileData);

            // Create a bufferView in the IMGS buffer (buffer index 1)
            QJsonObject imgBv;
            imgBv.insert(QStringLiteral("buffer"),     1);
            imgBv.insert(QStringLiteral("byteOffset"), static_cast<qint64>(imgByteOffset));
            imgBv.insert(QStringLiteral("byteLength"), static_cast<qint64>(fileData.size()));
            const int imgBvIndex = document.bufferViews.size();
            document.bufferViews.append(imgBv);

            imageObj.insert(QStringLiteral("bufferView"), imgBvIndex);
            imageObj.insert(QStringLiteral("byteLength"), static_cast<qint64>(fileData.size()));
            imageObj.insert(QStringLiteral("mimeType"),   mimeType);
            document.images.replace(imgIdx, imageObj);
        }

        if (!package.imageChunk.isEmpty())
        {
            QJsonObject updatedImgBuffer = document.buffers.at(1).toObject();
            updatedImgBuffer.insert(QStringLiteral("byteLength"),
                                    static_cast<qint64>(package.imageChunk.size()));
            document.buffers.replace(1, updatedImgBuffer);
        }
    }

    document.mvfSession.insert(QStringLiteral("imageChunkPresent"),
                                !package.imageChunk.isEmpty());

    return package;
}
}
