#pragma once
class SceneMesh;


#include "MvfDocument.h"

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QVector>
#include <QUuid>

class SceneGraph;
class RenderableMesh;
struct GltfCameraData;

namespace Mvf
{
struct MVFPackage
{
    Document document;
    QByteArray geometryChunk;
    QByteArray imageChunk;
};

MVFPackage buildMVFPackage(const SceneGraph& sceneGraph,
                                   const std::vector<SceneMesh*>& meshStore,
                                   const QSet<QUuid>& visibleMeshUuids,
                                   const QSet<QUuid>& selectedMeshUuids,
                                   const QVector<GltfCameraData>& cameraDataByFile = {});
}
