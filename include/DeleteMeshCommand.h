#pragma once

#include "ModelViewerCommand.h"
#include "SceneNode.h"
#include <QVector>
#include <QMap>
#include <QUuid>
#include <QSet>

class ModelViewer;
class GLWidget;

/**
 * @brief Undoable command for deleting meshes
 *
 * Captures the UUIDs of deleted meshes and their original positions
 * in the model list to allow restoration on undo.
 */
class DeleteMeshCommand : public ModelViewerCommand
{
public:
    DeleteMeshCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QVector<QUuid>& meshUuids,
        const QString& text = QObject::tr("Delete"));

    void undo() override;
    void redo() override;

    int id() const override { return 5; }

    // For cleanup system
    QSet<QUuid> getReferencedUuids() const
    {
        return QSet<QUuid>(_meshUuids.begin(), _meshUuids.end());
    }

private:
    QVector<QUuid>   _meshUuids;
    QMap<QUuid, int> _originalIndices;  // Store original positions in _meshStore

    // SceneGraph removal records — populated during the first redo() call so
    // that undo() can put each UUID back in exactly the right node and position.
    struct SceneRemovalRecord
    {
        SceneNode* node     = nullptr;
        int        position = -1;
    };
    QMap<QUuid, SceneRemovalRecord> _sceneRecords;
};
