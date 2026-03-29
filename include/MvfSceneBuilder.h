#pragma once

#include "MvfDocument.h"

#include <QByteArray>
#include <QList>
#include <QSet>
#include <QUuid>

class SceneGraph;
class TriangleMesh;

namespace Mvf
{
struct MVFPackage
{
    Document document;
    QByteArray geometryChunk;
    QByteArray imageChunk;
};

MVFPackage buildMVFPackage(const SceneGraph& sceneGraph,
                                   const std::vector<TriangleMesh*>& meshStore,
                                   const QSet<QUuid>& visibleMeshUuids,
                                   const QSet<QUuid>& selectedMeshUuids);
}
