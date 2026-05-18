#pragma once

#include <QHash>
#include <QMap>
#include <QQuaternion>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <assimp/matrix4x4.h>

enum class GltfAnimationTargetPath
{
    Translation,
    Rotation,
    Scale,
    Pointer
};

enum class GltfAnimationPointerProperty
{
    None,
    Offset,
    Scale,
    Rotation
};

enum class GltfAnimationTextureTarget
{
    Unknown,
    Albedo,
    Metallic,
    Roughness,
    MetallicRoughness,
    Normal,
    Occlusion,
    Emissive,
    Transmission,
    Thickness,
    IOR,
    SheenColor,
    SheenRoughness,
    Clearcoat,
    ClearcoatRoughness,
    ClearcoatNormal,
    Iridescence,
    IridescenceThickness,
    SpecularFactor,
    SpecularColor,
    Anisotropy,
    DiffuseTransmission,
    DiffuseTransmissionColor,
    Diffuse,
    SpecularGlossiness
};

struct GltfAnimationVec3Key
{
    double    timeSeconds = 0.0;
    QVector3D value;
};

struct GltfAnimationQuatKey
{
    double      timeSeconds = 0.0;
    QQuaternion value;
};

struct GltfAnimationVec2Key
{
    double    timeSeconds = 0.0;
    QVector2D value;
};

struct GltfAnimationFloatKey
{
    double timeSeconds = 0.0;
    float  value = 0.0f;
};

struct GltfAnimationChannel
{
    QString                 targetNodeName;
    GltfAnimationTargetPath targetPath = GltfAnimationTargetPath::Translation;
    QString                 targetPointer;
    int                     targetMaterialIndex = -1;
    GltfAnimationTextureTarget textureTarget = GltfAnimationTextureTarget::Unknown;
    GltfAnimationPointerProperty pointerProperty = GltfAnimationPointerProperty::None;
    QVector<GltfAnimationVec3Key> vec3Keys;
    QVector<GltfAnimationQuatKey> quatKeys;
    QVector<GltfAnimationVec2Key> vec2Keys;
    QVector<GltfAnimationFloatKey> floatKeys;
};

struct GltfAnimationClip
{
    QString                     name;
    double                      durationSeconds = 0.0;
    bool                        hasNodeTransforms = false;
    bool                        hasSkinning = false;
    bool                        hasPointerAnimations = false;
    QVector<GltfAnimationChannel> channels;
};

struct GltfSkinJoint
{
    QString     nodeName;
    aiMatrix4x4 inverseBindMatrix;
};

struct GltfAnimationData
{
    QString                  sourceFile;
    QVector<GltfAnimationClip> clips;
    bool                     hasNodeAnimations = false;
    bool                     hasSkinning = false;
    bool                     hasPointerAnimations = false;
    aiMatrix4x4              rootInverseTransform;

    bool isEmpty() const { return clips.isEmpty(); }
};
