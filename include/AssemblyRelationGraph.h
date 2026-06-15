#pragma once

#include <QJsonObject>
#include <QString>
#include <QSet>
#include <QUuid>

class GLWidget;
class SceneGraph;

namespace AssemblyRelationGraph
{

struct AutoPlacementHints
{
    QSet<QString> strongPairKeys;
};

QString makeMeshPairKey(const QUuid& a, const QUuid& b);

AutoPlacementHints buildAutoPlacementHints(const QSet<QUuid>& assemblyUuids,
                                           const GLWidget* glWidget,
                                           const SceneGraph* sceneGraph);

QJsonObject buildInspectionJson(const QSet<QUuid>& assemblyUuids,
                                const GLWidget* glWidget,
                                const SceneGraph* sceneGraph);

}
