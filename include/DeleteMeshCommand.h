#pragma once

#include "GltfLightData.h"
#include "ModelViewerCommand.h"
#include "SceneNode.h"
#include <QHash>
#include <QVector>
#include <QMap>
#include <QUuid>
#include <QSet>
#include <QString>

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

    // Snapshot of GltfLightData for files that will lose ALL their meshes in
    // this deletion.  Populated at the start of redo() (before any removal so
    // validateLightData hasn't cleared it yet) and restored in undo().
    QHash<QString, GltfLightData> _savedLightData;
};
