#pragma once

#include <QHash>
#include <QMap>
#include <QQuaternion>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVector3D>
#include <assimp/matrix4x4.h>

enum class GltfAnimationTargetPath
{
    Translation,
    Rotation,
    Scale
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

struct GltfAnimationChannel
{
    QString                 targetNodeName;
    GltfAnimationTargetPath targetPath = GltfAnimationTargetPath::Translation;
    QVector<GltfAnimationVec3Key> vec3Keys;
    QVector<GltfAnimationQuatKey> quatKeys;
};

struct GltfAnimationClip
{
    QString                     name;
    double                      durationSeconds = 0.0;
    bool                        hasNodeTransforms = false;
    bool                        hasSkinning = false;
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
    aiMatrix4x4              rootInverseTransform;

    bool isEmpty() const { return clips.isEmpty(); }
};
