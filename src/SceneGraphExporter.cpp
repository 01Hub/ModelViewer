#include "SceneGraphExporter.h"

#include "SceneGraph.h"
#include "SceneNode.h"
#include "TriangleMesh.h"
#include "AssImpMesh.h"
#include "GLLights.h"

#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/light.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <QDebug>
#include <cmath>
#include <initializer_list>

namespace
{
    void setFaceIndices(aiFace& face, std::initializer_list<unsigned int> values)
    {
        face.mNumIndices = static_cast<unsigned int>(values.size());
        face.mIndices = new unsigned int[face.mNumIndices];

        unsigned int dst = 0;
        for (unsigned int value : values)
        {
            face.mIndices[dst++] = value;
        }
    }

    unsigned int primitiveModeToAiPrimitiveType(GLenum primitiveMode)
    {
        switch (primitiveMode)
        {
        case GL_POINTS:
            return aiPrimitiveType_POINT;
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
            return aiPrimitiveType_LINE;
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        default:
            return aiPrimitiveType_TRIANGLE;
        }
    }

    bool populateFacesForPrimitive(aiMesh* mesh,
                                   const std::vector<unsigned int>& indices,
                                   GLenum primitiveMode)
    {
        if (!mesh)
            return false;

        switch (primitiveMode)
        {
        case GL_POINTS:
            if (indices.empty())
                return false;

            mesh->mNumFaces = static_cast<unsigned int>(indices.size());
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
            {
                setFaceIndices(mesh->mFaces[i], { indices[i] });
            }
            return true;

        case GL_LINES:
        {
            const unsigned int faceCount = static_cast<unsigned int>(indices.size() / 2);
            if (faceCount == 0)
                return false;

            mesh->mNumFaces = faceCount;
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i < faceCount; ++i)
            {
                setFaceIndices(mesh->mFaces[i], {
                    indices[i * 2],
                    indices[i * 2 + 1]
                });
            }
            return true;
        }

        case GL_LINE_STRIP:
            if (indices.size() < 2)
                return false;

            mesh->mNumFaces = static_cast<unsigned int>(indices.size() - 1);
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
            {
                setFaceIndices(mesh->mFaces[i], { indices[i], indices[i + 1] });
            }
            return true;

        case GL_LINE_LOOP:
            if (indices.size() < 2)
                return false;

            mesh->mNumFaces = static_cast<unsigned int>(indices.size());
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i + 1 < static_cast<unsigned int>(indices.size()); ++i)
            {
                setFaceIndices(mesh->mFaces[i], { indices[i], indices[i + 1] });
            }
            setFaceIndices(mesh->mFaces[mesh->mNumFaces - 1], { indices.back(), indices.front() });
            return true;

        case GL_TRIANGLES:
        {
            const unsigned int faceCount = static_cast<unsigned int>(indices.size() / 3);
            if (faceCount == 0)
                return false;

            mesh->mNumFaces = faceCount;
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i < faceCount; ++i)
            {
                setFaceIndices(mesh->mFaces[i], {
                    indices[i * 3],
                    indices[i * 3 + 1],
                    indices[i * 3 + 2]
                });
            }
            return true;
        }

        case GL_TRIANGLE_STRIP:
            if (indices.size() < 3)
                return false;

            mesh->mNumFaces = static_cast<unsigned int>(indices.size() - 2);
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
            {
                if (((i + 1) % 2) == 0)
                {
                    setFaceIndices(mesh->mFaces[i], { indices[i + 1], indices[i], indices[i + 2] });
                }
                else
                {
                    setFaceIndices(mesh->mFaces[i], { indices[i], indices[i + 1], indices[i + 2] });
                }
            }
            return true;

        case GL_TRIANGLE_FAN:
            if (indices.size() < 3)
                return false;

            mesh->mNumFaces = static_cast<unsigned int>(indices.size() - 2);
            mesh->mFaces = new aiFace[mesh->mNumFaces];
            setFaceIndices(mesh->mFaces[0], { indices[0], indices[1], indices[2] });
            for (unsigned int i = 1; i < mesh->mNumFaces; ++i)
            {
                setFaceIndices(mesh->mFaces[i], {
                    indices[0],
                    mesh->mFaces[i - 1].mIndices[2],
                    indices[i + 2]
                });
            }
            return true;

        default:
            return populateFacesForPrimitive(mesh, indices, GL_TRIANGLES);
        }
    }

    aiNode* makeIdentityNode(const QString& name)
    {
        aiNode* node = new aiNode();
        node->mName.Set(name.toUtf8().constData());
        node->mTransformation = aiMatrix4x4(); // identity
        return node;
    }

    aiTextureMapMode toAiTextureMapMode(GLenum wrapMode)
    {
        switch (wrapMode)
        {
        case GL_REPEAT:
            return aiTextureMapMode_Wrap;
        case GL_CLAMP_TO_EDGE:
            return aiTextureMapMode_Clamp;
        case GL_MIRRORED_REPEAT:
            return aiTextureMapMode_Mirror;
        default:
            return aiTextureMapMode_Wrap;
        }
    }

    constexpr unsigned int kMaxExportUvChannels = 4;

    struct ExportTextureBinding
    {
        const GLMaterial::Texture* texture = nullptr;
        aiTextureType aiType = aiTextureType_NONE;
        unsigned int slot = 0;
    };

    std::vector<ExportTextureBinding> collectExportBindings(const GLMaterial& material)
    {
        std::vector<ExportTextureBinding> bindings;
        bindings.reserve(16);

        auto add = [&](GLMaterial::TextureType type, aiTextureType aiType, unsigned int slot) {
            const GLMaterial::Texture& tex = material.texture(type);
            bindings.push_back({ &tex, aiType, slot });
            };

        // Core PBR bindings
        add(GLMaterial::TextureType::Albedo, aiTextureType_BASE_COLOR, 0);
        add(GLMaterial::TextureType::Metallic, aiTextureType_METALNESS, 0);
        add(GLMaterial::TextureType::Roughness, aiTextureType_DIFFUSE_ROUGHNESS, 0);
        add(GLMaterial::TextureType::Normal, aiTextureType_NORMALS, 0);
        add(GLMaterial::TextureType::AmbientOcclusion, aiTextureType_LIGHTMAP, 0);
        add(GLMaterial::TextureType::Emissive, aiTextureType_EMISSIVE, 0);
        add(GLMaterial::TextureType::Opacity, aiTextureType_OPACITY, 0);

        // Extensions / advanced PBR
        add(GLMaterial::TextureType::Transmission, aiTextureType_TRANSMISSION, 0);
        add(GLMaterial::TextureType::ClearcoatColor, aiTextureType_CLEARCOAT, 0);
        add(GLMaterial::TextureType::ClearcoatRoughness, aiTextureType_CLEARCOAT, 1);
        add(GLMaterial::TextureType::ClearcoatNormal, aiTextureType_CLEARCOAT, 2);
        add(GLMaterial::TextureType::SheenColor, aiTextureType_SHEEN, 0);
        add(GLMaterial::TextureType::SheenRoughness, aiTextureType_SHEEN, 1);

        return bindings;
    }

    unsigned int clampUvChannelIndex(int texCoordIndex)
    {
        if (texCoordIndex < 0)
            return 0;
        if (texCoordIndex >= static_cast<int>(kMaxExportUvChannels))
            return 0;
        return static_cast<unsigned int>(texCoordIndex);
    }

    unsigned int maxReferencedUvChannel(const GLMaterial& material)
    {
        const auto bindings = collectExportBindings(material);

        unsigned int maxChannel = 0;
        bool anyTexturedBinding = false;

        for (const auto& binding : bindings)
        {
            if (!binding.texture || binding.texture->path.empty())
                continue;

            anyTexturedBinding = true;
            maxChannel = std::max(maxChannel, clampUvChannelIndex(binding.texture->texCoordIndex));
        }

        // Always keep UV0 available if any texturing exists at all.
        return anyTexturedBinding ? maxChannel : 0;
    }
    
    void addTextureToMaterial(
        aiMaterial* aiMat,
        const GLMaterial::Texture& tex,
        aiTextureType aiType,
        unsigned int slot)
    {
        if (!aiMat || tex.path.empty())
            return;

        aiString path(tex.path.c_str());
        aiMat->AddProperty(&path, AI_MATKEY_TEXTURE(aiType, slot));

        int uvIndex = static_cast<int>(clampUvChannelIndex(tex.texCoordIndex));
        aiMat->AddProperty(&uvIndex, 1, AI_MATKEY_UVWSRC(aiType, slot));

        aiTextureMapMode mapModeU = toAiTextureMapMode(tex.wrapS);
        aiTextureMapMode mapModeV = toAiTextureMapMode(tex.wrapT);
        aiMat->AddProperty(&mapModeU, 1, AI_MATKEY_MAPPINGMODE_U(aiType, slot));
        aiMat->AddProperty(&mapModeV, 1, AI_MATKEY_MAPPINGMODE_V(aiType, slot));

        aiUVTransform uvTransform;
        uvTransform.mTranslation = aiVector2D(tex.offset.x, tex.offset.y);
        uvTransform.mScaling = aiVector2D(tex.scale.x, tex.scale.y);
        uvTransform.mRotation = tex.rotation;
        aiMat->AddProperty(&uvTransform, 1, AI_MATKEY_UVTRANSFORM(aiType, slot));
    }

    aiMaterial* makeDefaultMaterial()
    {
        aiMaterial* mat = new aiMaterial();

        aiString matName("DefaultMaterial");
        mat->AddProperty(&matName, AI_MATKEY_NAME);

        // Optional simple neutral diffuse
        aiColor3D diffuse(0.8f, 0.8f, 0.8f);
        mat->AddProperty(&diffuse, 1, AI_MATKEY_COLOR_DIFFUSE);

        aiColor3D ambient(0.2f, 0.2f, 0.2f);
        mat->AddProperty(&ambient, 1, AI_MATKEY_COLOR_AMBIENT);

        aiColor3D specular(0.2f, 0.2f, 0.2f);
        mat->AddProperty(&specular, 1, AI_MATKEY_COLOR_SPECULAR);

        return mat;
    }

    QString exportedNodeName(const SceneNode* node)
    {
        if (!node)
            return QString();

        // Only synthetic file-wrapper nodes should lose the file extension.
        if (node->isSynthetic)
        {
            // Prefer sourceFile if available (more reliable than display name).
            if (!node->sourceFile.trimmed().isEmpty())
            {
                const QFileInfo fi(node->sourceFile);
                const QString stem = fi.completeBaseName().trimmed();
                if (!stem.isEmpty())
                    return stem;
            }

            // Fallback to the display name.
            const QFileInfo fi(node->name);
            const QString stem = fi.completeBaseName().trimmed();
            if (!stem.isEmpty())
                return stem;
        }

        // Default: preserve original node name exactly.
        return node->name;
    }

    QString textureKeyPart(const GLMaterial::Texture& tex, const QString& label)
    {
        if (tex.path.empty())
            return label + "=<none>";

        return QString("%1=%2|uv=%3|scale=%4,%5|offset=%6,%7|rot=%8")
            .arg(label)
            .arg(QString::fromStdString(tex.path))
            .arg(tex.texCoordIndex)
            .arg(tex.scale.x, 0, 'f', 6)
            .arg(tex.scale.y, 0, 'f', 6)
            .arg(tex.offset.x, 0, 'f', 6)
            .arg(tex.offset.y, 0, 'f', 6)
            .arg(tex.rotation, 0, 'f', 6);
    }

    QString buildMaterialReuseKey(const GLMaterial& material)
    {
        QStringList parts;
        parts << QString("name=%1").arg(material.name().trimmed());
        parts << QString("albedo=%1,%2,%3")
            .arg(material.albedoColor().x(), 0, 'f', 6)
            .arg(material.albedoColor().y(), 0, 'f', 6)
            .arg(material.albedoColor().z(), 0, 'f', 6);
        parts << QString("metal=%1").arg(material.metalness(), 0, 'f', 6);
        parts << QString("rough=%1").arg(material.roughness(), 0, 'f', 6);
        parts << QString("opacity=%1").arg(material.opacity(), 0, 'f', 6);
        parts << QString("blend=%1").arg(static_cast<int>(material.blendMode()));
        parts << QString("twoSided=%1").arg(material.twoSided() ? 1 : 0);
        parts << QString("alphaCutoff=%1").arg(material.alphaThreshold(), 0, 'f', 6);
        parts << QString("ior=%1").arg(material.ior(), 0, 'f', 6);
        parts << QString("transmission=%1").arg(material.transmission(), 0, 'f', 6);
        parts << QString("thickness=%1").arg(material.thicknessFactor(), 0, 'f', 6);
        parts << QString("clearcoat=%1").arg(material.clearcoat(), 0, 'f', 6);
        parts << QString("clearcoatRough=%1").arg(material.clearcoatRoughness(), 0, 'f', 6);
        parts << QString("sheenRough=%1").arg(material.sheenRoughness(), 0, 'f', 6);

        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Albedo), "albedoTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Metallic), "metalTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Roughness), "roughTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Normal), "normalTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::AmbientOcclusion), "aoTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Emissive), "emissiveTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Opacity), "opacityTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::Transmission), "transmissionTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::ClearcoatColor), "clearcoatTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::ClearcoatRoughness), "clearcoatRoughTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::ClearcoatNormal), "clearcoatNormalTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::SheenColor), "sheenColorTex");
        parts << textureKeyPart(material.texture(GLMaterial::TextureType::SheenRoughness), "sheenRoughTex");

        return parts.join("||");
    }
}

aiScene* SceneGraphExporter::buildExportScene(
    const SceneGraph* sceneGraph,
    const MeshResolver& resolveMesh)
{
    if (!sceneGraph)
        return nullptr;

    SceneNode* graphRoot = sceneGraph->root();
    if (!graphRoot)
        return nullptr;

    aiScene* scene = new aiScene();

    std::vector<aiMesh*> builtMeshes;
    builtMeshes.reserve(64);

    std::vector<aiMaterial*> builtMaterials;
    builtMaterials.reserve(64);


    // Export root (invisible SceneGraph root becomes a real aiNode container).
    aiNode* exportRoot = makeIdentityNode(QStringLiteral("SceneRoot"));
    scene->mRootNode = exportRoot;

    // Rebuild top-level children from SceneGraph::_root->children
    exportRoot->mNumChildren = static_cast<unsigned int>(graphRoot->children.size());
    if (exportRoot->mNumChildren > 0)
    {
        QMap<QString, unsigned int> materialKeyToIndex;
        exportRoot->mChildren = new aiNode * [exportRoot->mNumChildren];
        for (unsigned int i = 0; i < exportRoot->mNumChildren; ++i)
        {
            SceneNode* srcChild = graphRoot->children.at(static_cast<int>(i));
            aiNode* dstChild = buildNodeRecursive(srcChild, resolveMesh, builtMeshes, builtMaterials, materialKeyToIndex, aiMatrix4x4());
            exportRoot->mChildren[i] = dstChild;
            if (dstChild)
                dstChild->mParent = exportRoot;
        }
    }

    // Transfer built meshes into aiScene::mMeshes
    scene->mNumMeshes = static_cast<unsigned int>(builtMeshes.size());
    if (scene->mNumMeshes > 0)
    {
        scene->mMeshes = new aiMesh * [scene->mNumMeshes];
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
        {
            scene->mMeshes[i] = builtMeshes[i];
        }
    }

    // Transfer built materials into aiScene::mMaterials.
    if (builtMaterials.empty())
    {
        scene->mNumMaterials = 1;
        scene->mMaterials = new aiMaterial * [1];
        scene->mMaterials[0] = makeDefaultMaterial();
    }
    else
    {
        scene->mNumMaterials = static_cast<unsigned int>(builtMaterials.size());
        scene->mMaterials = new aiMaterial * [scene->mNumMaterials];
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
        {
            scene->mMaterials[i] = builtMaterials[i];
        }
    }

    // Transfer lights from SceneGraph into aiScene::mLights (KHR_lights_punctual support)
    const auto& lights = sceneGraph->lights();
    if (!lights.empty())
    {
        scene->mNumLights = static_cast<unsigned int>(lights.size());
        scene->mLights = new aiLight*[scene->mNumLights];

        for (unsigned int i = 0; i < scene->mNumLights; ++i)
        {
            const GPULight& gpuLight = lights[i];
            aiLight* aiLight = new ::aiLight();

            aiLight->mName.Set((std::string("Light_") + std::to_string(i)).c_str());
            aiLight->mPosition = aiVector3D(gpuLight.position.x, gpuLight.position.y, gpuLight.position.z);
            aiLight->mDirection = aiVector3D(gpuLight.direction.x, gpuLight.direction.y, gpuLight.direction.z);
            aiLight->mColorDiffuse = aiColor3D(gpuLight.color.x, gpuLight.color.y, gpuLight.color.z);
            aiLight->mColorSpecular = aiColor3D(gpuLight.color.x, gpuLight.color.y, gpuLight.color.z);

            // Set light type
            switch (static_cast<LightType>(gpuLight.type))
            {
            case LightType::Directional:
                aiLight->mType = aiLightSource_DIRECTIONAL;
                break;
            case LightType::Point:
                aiLight->mType = aiLightSource_POINT;
                aiLight->mAttenuationConstant = 1.0f;
                aiLight->mAttenuationLinear = 0.0f;
                aiLight->mAttenuationQuadratic = 0.0f;
                if (gpuLight.range > 0.0f)
                {
                    // Set range as a custom property (KHR extension)
                    // This will be handled by GltfPostProcessor when writing KHR_lights_punctual
                }
                break;
            case LightType::Spot:
                aiLight->mType = aiLightSource_SPOT;
                aiLight->mAttenuationConstant = 1.0f;
                aiLight->mAttenuationLinear = 0.0f;
                aiLight->mAttenuationQuadratic = 0.0f;
                aiLight->mAngleOuterCone = std::acos(gpuLight.outerConeCos);
                aiLight->mAngleInnerCone = std::acos(gpuLight.innerConeCos);
                break;
            }

            scene->mLights[i] = aiLight;
        }

        qDebug() << "[EXPORT-LIGHTS] Exported" << scene->mNumLights << "punctual lights";
    }

    return scene;
}

aiNode* SceneGraphExporter::buildNodeRecursive(
    const SceneNode* srcNode,
    const MeshResolver& resolveMesh,
    std::vector<aiMesh*>& outMeshes,
    std::vector<aiMaterial*>& outMaterials,
    QMap<QString, unsigned int>& materialKeyToIndex,
    const aiMatrix4x4& parentWorldTransform
)
{
    if (!srcNode)
        return nullptr;

    // Accumulated world transform for this node (same accumulation as processNode at import).
    // Used to un-bake baked-in vertex world-space positions back to local space.
    aiMatrix4x4 worldTransform = parentWorldTransform * srcNode->localTransform;

    // Assimp's glTF2 exporter uses aiMatrix4x4::IsIdentity() (epsilon ~0.01) to decide
    // whether to write a node's transform. Models like MetalRoughSpheresNoTextures encode
    // sphere positions as tiny per-node translations (0.001–0.006 units) that fall below
    // this threshold, so Assimp silently drops them. When the exporter drops the transform
    // but the un-baking code has already moved vertices to local space, all spheres stack
    // at the origin on reimport.
    //
    // Fix: for leaf mesh nodes (meshes, no children) whose localTransform would be dropped
    // by Assimp (localTransform.IsIdentity() == true with Assimp's epsilon), encode the
    // node's local offset into the vertex positions instead, and export the node as identity.
    // On reimport the parent chain re-applies correctly and positions are preserved.
    //
    // When the localTransform is large enough for Assimp to write (e.g. negative scales,
    // significant rotations/translations), the existing worldTransform un-baking is used
    // unchanged so that normal flipping and other transform-dependent effects are correct.
    const bool isLeafMeshNode = !srcNode->meshUuids.isEmpty() && srcNode->children.isEmpty();
    const bool useParentSpaceEncoding = isLeafMeshNode && srcNode->localTransform.IsIdentity();
    const aiMatrix4x4& unbakeTransform = useParentSpaceEncoding ? parentWorldTransform : worldTransform;

    aiNode* dstNode = makeIdentityNode(exportedNodeName(srcNode));
    dstNode->mTransformation = useParentSpaceEncoding ? aiMatrix4x4() : srcNode->localTransform;

    // Build meshes owned directly by this node.
    //
    // Important for STEP/XCAF-derived scenes:
    // exporting several source meshes directly on one aiNode encourages Assimp's
    // glTF writer to collapse them into a single JSON mesh with multiple
    // primitives. That later forces GltfPostProcessor to recover primitive ->
    // source-mesh identity heuristically, which is where color/material
    // remapping starts to drift for CAD imports.
    //
    // To preserve one source mesh -> one explicit export identity, we wrap each
    // mesh in its own identity child node whenever a source node owns multiple
    // meshes. The parent node still carries the original local transform, so the
    // world-space result is unchanged while the exported structure becomes much
    // closer to a typical glTF scene graph.
    const bool splitMeshesIntoChildNodes = srcNode->meshUuids.size() > 1;

    std::vector<unsigned int> nodeMeshIndices;
    nodeMeshIndices.reserve(splitMeshesIntoChildNodes ? 0 : static_cast<size_t>(srcNode->meshUuids.size()));
    std::vector<aiNode*> meshChildNodes;
    meshChildNodes.reserve(splitMeshesIntoChildNodes ? static_cast<size_t>(srcNode->meshUuids.size()) : 0);

    for (const QUuid& meshUuid : srcNode->meshUuids)
    {
        TriangleMesh* triMesh = resolveMesh ? resolveMesh(meshUuid) : nullptr;
        if (!triMesh)
            continue;

        const GLMaterial glMaterial = triMesh->getMaterial();
        unsigned int materialIndex = 0;

        // Use originalMaterialIndex as authoritative mapping to preserve material structure from import.
        // All meshes that referenced the same original material should use the same material in export.
        int originalMatIndex = triMesh->getOriginalMaterialIndex();

        qDebug() << "[EXPORT] Processing mesh:" << triMesh->getName()
                 << "originalMaterialIndex=" << originalMatIndex
                 << "materialName=" << glMaterial.name()
                 << "albedo=" << glMaterial.albedoColor().x() << glMaterial.albedoColor().y() << glMaterial.albedoColor().z();

        if (originalMatIndex >= 0)
        {
            // Mesh came from Assimp import - use original material index as key
            QString originalMatKey = QString("originalMat=%1").arg(originalMatIndex);
            auto matIt = materialKeyToIndex.find(originalMatKey);

            if (matIt != materialKeyToIndex.end())
            {
                // Already created a material for this original index - reuse it
                materialIndex = matIt.value();
                qDebug() << "  [EXPORT-REUSE] Reusing material" << materialIndex
                         << "for originalMatIndex" << originalMatIndex;
            }
            else
            {
                // First time seeing this original material - create it
                aiMaterial* builtMaterial = buildMaterialFromTriangleMesh(triMesh);
                if (!builtMaterial)
                    builtMaterial = makeDefaultMaterial();

                materialIndex = static_cast<unsigned int>(outMaterials.size());
                outMaterials.push_back(builtMaterial);
                materialKeyToIndex.insert(originalMatKey, materialIndex);

                qDebug() << "  [EXPORT-CREATE] Created material" << materialIndex
                         << "for originalMatIndex" << originalMatIndex
                         << "from mesh" << triMesh->getName();
            }
        }
        else
        {
            // Fallback for non-import sources: use content-based deduplication
            const QString materialKey = buildMaterialReuseKey(glMaterial);
            auto matIt = materialKeyToIndex.find(materialKey);

            if (matIt != materialKeyToIndex.end())
            {
                // Reuse already-created equivalent material
                materialIndex = matIt.value();
            }
            else
            {
                aiMaterial* builtMaterial = buildMaterialFromTriangleMesh(triMesh);
                if (!builtMaterial)
                    builtMaterial = makeDefaultMaterial();

                materialIndex = static_cast<unsigned int>(outMaterials.size());
                outMaterials.push_back(builtMaterial);
                materialKeyToIndex.insert(materialKey, materialIndex);
            }
        }

        aiMesh* builtMesh = buildMeshFromTriangleMesh(triMesh, materialIndex);
        if (!builtMesh)
        {
            // IMPORTANT:
            // If mesh build fails, do not delete a reused material.
            // Also do not roll back a newly-created material here because other meshes
            // may still legitimately reuse it during this export pass.
            continue;
        }

        // Un-bake the transform from the mesh vertices/normals.
        // processMesh bakes the full world transform into vertices at import time:
        //   positions:              M * P_local
        //   normals/tangents/bitangents: M^{-T} * N_local, then flipped if det(M) < 0
        // When useParentSpaceEncoding is true (leaf node, localTransform too small for Assimp
        // to write), unbakeTransform == parentWorldTransform so the node's own local offset
        // stays encoded in the vertex data and the exported node has identity transform.
        // Otherwise unbakeTransform == worldTransform (full un-bake back to local space).
        if (!unbakeTransform.IsIdentity())
        {
            aiMatrix4x4 invUnbake = unbakeTransform;
            invUnbake.Inverse();

            // M^T: inverse of the forward normal transform M^{-T}.
            aiMatrix3x3 normalUnbake = aiMatrix3x3(unbakeTransform);
            normalUnbake.Transpose();

            // det(M^T) == det(M): negative means the world transform has a negative scale.
            const bool hasNegativeScale = normalUnbake.Determinant() < 0.0f;

            for (unsigned int vi = 0; vi < builtMesh->mNumVertices; ++vi)
            {
                builtMesh->mVertices[vi] = invUnbake * builtMesh->mVertices[vi];

                if (builtMesh->mNormals)
                {
                    aiVector3D n = normalUnbake * builtMesh->mNormals[vi];
                    if (hasNegativeScale) n = -n;
                    builtMesh->mNormals[vi] = n.NormalizeSafe();
                }

                if (builtMesh->mTangents)
                {
                    aiVector3D t = normalUnbake * builtMesh->mTangents[vi];
                    if (hasNegativeScale) t = -t;
                    builtMesh->mTangents[vi] = t.NormalizeSafe();
                }

                if (builtMesh->mBitangents)
                {
                    aiVector3D b = normalUnbake * builtMesh->mBitangents[vi];
                    if (hasNegativeScale) b = -b;
                    builtMesh->mBitangents[vi] = b.NormalizeSafe();
                }
            }
        }

        qDebug() << "  [EXPORT-ASSIGN] Mesh:" << triMesh->getName()
                 << "→ materialIndex=" << materialIndex
                 << "(meshIndex in export=" << outMeshes.size() << ")";

        const unsigned int exportMeshIndex = static_cast<unsigned int>(outMeshes.size());
        outMeshes.push_back(builtMesh);

        if (splitMeshesIntoChildNodes)
        {
            aiNode* meshNode = makeIdentityNode(triMesh->getName());
            meshNode->mNumMeshes = 1;
            meshNode->mMeshes = new unsigned int[1];
            meshNode->mMeshes[0] = exportMeshIndex;
            meshChildNodes.push_back(meshNode);
        }
        else
        {
            nodeMeshIndices.push_back(exportMeshIndex);
        }
    }

    qDebug() << "[EXPORT-NODE-SUMMARY] Node:" << srcNode->name
             << "has" << srcNode->meshUuids.size() << "meshes"
             << "materialKeyToIndex.size()=" << materialKeyToIndex.size()
             << "outMaterials.size()=" << outMaterials.size();

    dstNode->mNumMeshes = static_cast<unsigned int>(nodeMeshIndices.size());
    if (dstNode->mNumMeshes > 0)
    {
        dstNode->mMeshes = new unsigned int[dstNode->mNumMeshes];
        for (unsigned int i = 0; i < dstNode->mNumMeshes; ++i)
        {
            dstNode->mMeshes[i] = nodeMeshIndices[i];
        }
    }

    // Recurse children.
    dstNode->mNumChildren = static_cast<unsigned int>(meshChildNodes.size() + srcNode->children.size());
    if (dstNode->mNumChildren > 0)
    {
        dstNode->mChildren = new aiNode * [dstNode->mNumChildren];
        unsigned int childIndex = 0;

        for (aiNode* meshChild : meshChildNodes)
        {
            dstNode->mChildren[childIndex++] = meshChild;
            if (meshChild)
                meshChild->mParent = dstNode;
        }

        for (int i = 0; i < srcNode->children.size(); ++i)
        {
            SceneNode* srcChild = srcNode->children.at(i);
            aiNode* dstChild = buildNodeRecursive(srcChild, resolveMesh, outMeshes, outMaterials, materialKeyToIndex, worldTransform);
            dstNode->mChildren[childIndex++] = dstChild;
            if (dstChild)
                dstChild->mParent = dstNode;
        }
    }

    return dstNode;
}

aiMesh* SceneGraphExporter::buildMeshFromTriangleMesh(const TriangleMesh* mesh, unsigned int materialIndex)
{
    if (!mesh)
        return nullptr;

    // We only export AssImp-backed runtime meshes in this first pass,
    // because those expose CPU-side vertices()/indices().
    const AssImpMesh* assimpMesh = dynamic_cast<const AssImpMesh*>(mesh);
    if (!assimpMesh)
        return nullptr;

    const std::vector<Vertex> verts = assimpMesh->vertices();
    const std::vector<unsigned int> indices = assimpMesh->indices();

    if (verts.empty() || indices.empty())
        return nullptr;

    aiMesh* out = new aiMesh();

    out->mName.Set(mesh->getName().toUtf8().constData());
    out->mPrimitiveTypes = primitiveModeToAiPrimitiveType(mesh->getPrimitiveMode());
    out->mMaterialIndex = materialIndex;

    // --- Vertices ---
    out->mNumVertices = static_cast<unsigned int>(verts.size());
    out->mVertices = new aiVector3D[out->mNumVertices];
    out->mNormals = new aiVector3D[out->mNumVertices];

    for (unsigned int i = 0; i < out->mNumVertices; ++i)
    {
        const Vertex& v = verts[i];

        out->mVertices[i] = aiVector3D(
            v.Position.x,
            v.Position.y,
            v.Position.z);

        out->mNormals[i] = aiVector3D(
            v.Normal.x,
            v.Normal.y,
            v.Normal.z);
    }

    // --- UV channels (material-driven, up to Vertex-supported maximum = 4) ---
    // IMPORTANT:
    // Material bindings already preserve per-texture texCoordIndex through AI_MATKEY_UVWSRC.
    // So the mesh must export every UV channel that is actually referenced by the material,
    // capped to the 4 channels physically stored in Vertex::TexCoords[4].
    const GLMaterial exportMaterial = mesh->getMaterial();
    const unsigned int uvChannelCount = std::min(
        maxReferencedUvChannel(exportMaterial) + 1,
        kMaxExportUvChannels);

    for (unsigned int channel = 0; channel < uvChannelCount; ++channel)
    {
        out->mTextureCoords[channel] = new aiVector3D[out->mNumVertices];
        out->mNumUVComponents[channel] = 2;

        for (unsigned int i = 0; i < out->mNumVertices; ++i)
        {
            const glm::vec2& uv = verts[i].TexCoords[channel];
            out->mTextureCoords[channel][i] = aiVector3D(uv.x, uv.y, 0.0f);
        }
    }

    // --- Vertex Colors ---
    out->mColors[0] = new aiColor4D[out->mNumVertices];
    for (unsigned int i = 0; i < out->mNumVertices; ++i)
    {
        const Vertex& v = verts[i];
        out->mColors[0][i] = aiColor4D(v.Color.r, v.Color.g, v.Color.b, v.Color.a);
    }

    if (!populateFacesForPrimitive(out, indices, mesh->getPrimitiveMode()))
    {
        delete out;
        return nullptr;
    }

    return out;
}

aiMaterial* SceneGraphExporter::buildMaterialFromTriangleMesh(const TriangleMesh* mesh)
{
    if (!mesh)
        return nullptr;

    const GLMaterial material = mesh->getMaterial();

    aiMaterial* aiMat = new aiMaterial();

    const QString materialName =
        material.name().trimmed().isEmpty()
        ? mesh->getName()
        : material.name().trimmed();

    aiString matName(materialName.toUtf8().constData());
    aiMat->AddProperty(&matName, AI_MATKEY_NAME);

    // Core PBR / legacy-compatible scalar properties.
    {
        aiColor3D albedo(
            static_cast<float>(material.albedoColor().x()),
            static_cast<float>(material.albedoColor().y()),
            static_cast<float>(material.albedoColor().z()));
        aiMat->AddProperty(&albedo, 1, AI_MATKEY_BASE_COLOR);
        aiMat->AddProperty(&albedo, 1, AI_MATKEY_COLOR_DIFFUSE);
    }

    {
        float metallic = material.metalness();
        aiMat->AddProperty(&metallic, 1, AI_MATKEY_METALLIC_FACTOR);
    }

    {
        float roughness = material.roughness();
        aiMat->AddProperty(&roughness, 1, AI_MATKEY_ROUGHNESS_FACTOR);
    }

    {
        float opacity = material.opacity();
        aiMat->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);
    }

    {
        float ior = material.ior();
        aiMat->AddProperty(&ior, 1, AI_MATKEY_REFRACTI);
    }

    {
        aiColor3D emissive(
            static_cast<float>(material.emissive().x()),
            static_cast<float>(material.emissive().y()),
            static_cast<float>(material.emissive().z()));
        aiMat->AddProperty(&emissive, 1, AI_MATKEY_COLOR_EMISSIVE);

        float emissiveStrength = material.emissiveStrength();
        aiMat->AddProperty(&emissiveStrength, 1, AI_MATKEY_EMISSIVE_INTENSITY);
    }

    if (material.transmission() > 0.0f)
    {
        float transmission = material.transmission();
        aiMat->AddProperty(&transmission, 1, AI_MATKEY_TRANSMISSION_FACTOR);
    }

    if (material.clearcoat() > 0.0f)
    {
        float clearcoat = material.clearcoat();
        aiMat->AddProperty(&clearcoat, 1, AI_MATKEY_CLEARCOAT_FACTOR);

        float clearcoatRoughness = material.clearcoatRoughness();
        aiMat->AddProperty(&clearcoatRoughness, 1, AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR);
    }

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

    // Basic rendering hints.
    {
        int twoSided = material.twoSided() ? 1 : 0;
        aiMat->AddProperty(&twoSided, 1, "$mat.gltf.doubleSided", 0, 0);
    }

    {
        aiString alphaModeStr;
        switch (material.blendMode())
        {
        case GLMaterial::BlendMode::Masked:
        {
            alphaModeStr.Set("MASK");
            float alphaCutoff = material.alphaThreshold();
            aiMat->AddProperty(&alphaCutoff, 1, "$mat.gltf.alphaCutoff", 0, 0);
            break;
        }
        case GLMaterial::BlendMode::Alpha:
            alphaModeStr.Set("BLEND");
            break;
        case GLMaterial::BlendMode::Opaque:
        default:
            alphaModeStr.Set("OPAQUE");
            break;
        }
        aiMat->AddProperty(&alphaModeStr, "$mat.gltf.alphaMode", 0, 0);
    }

    // Texture bindings.
    const auto bindings = collectExportBindings(material);
    for (const auto& binding : bindings)
    {
        if (!binding.texture)
            continue;

        addTextureToMaterial(aiMat, *binding.texture, binding.aiType, binding.slot);
    }

    return aiMat;
}
