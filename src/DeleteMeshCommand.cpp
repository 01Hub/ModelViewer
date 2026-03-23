#include "DeleteMeshCommand.h"
#include "GLWidget.h"
#include "ModelViewer.h"
#include "SceneGraph.h"

DeleteMeshCommand::DeleteMeshCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _meshUuids(meshUuids)
{
    // Store original indices for each mesh
    for (const QUuid& uuid : meshUuids)
    {
        int index = _glWidget->getIndexByUuid(uuid);
        if (index >= 0)
            _originalIndices[uuid] = index;
    }
}

void DeleteMeshCommand::undo()
{
    SceneGraph* sg = _viewer->sceneGraph();

    for (const QUuid& uuid : _meshUuids)
    {
        // Restore mesh in the renderer's store
        if (_glWidget->isInRecycleBin(uuid))
        {
            _glWidget->restoreFromRecycleBin(uuid);
        }
        else
        {
            qWarning() << "DeleteMeshCommand::undo - Mesh not in bin:" << uuid;
        }

        // Restore UUID in the SceneGraph at its original node and position
        if (sg)
        {
            const SceneRemovalRecord& rec = _sceneRecords.value(uuid);
            sg->restoreMeshUuid(rec.node, uuid, rec.position);
        }
    }

    _glWidget->updateView();
    _viewer->updateDisplayList();
}

void DeleteMeshCommand::redo()
{
    SceneGraph* sg = _viewer->sceneGraph();

    for (const QUuid& uuid : _meshUuids)
    {
        // Remove UUID from SceneGraph and record where it was for undo
        if (sg)
        {
            SceneRemovalRecord rec;
            rec.node = sg->removeMeshUuid(uuid, rec.position);
            _sceneRecords[uuid] = rec;
        }

        // Move mesh to recycle bin in the renderer's store
        int originalIndex = _originalIndices.value(uuid, -1);
        _glWidget->moveToRecycleBin(uuid, originalIndex);
    }

    _glWidget->updateView();
    _viewer->updateDisplayList();
}
