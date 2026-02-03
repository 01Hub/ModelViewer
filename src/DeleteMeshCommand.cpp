#include "DeleteMeshCommand.h"
#include "GLWidget.h"
#include "ModelViewer.h"

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
    // Restore meshes from recycle bin
    for (const QUuid& uuid : _meshUuids)
    {
        if (_glWidget->isInRecycleBin(uuid))
        {
            _glWidget->restoreFromRecycleBin(uuid);
        }
        else
        {
            qWarning() << "DeleteMeshCommand::undo - Mesh not in bin:" << uuid;
            // Mesh was permanently deleted - can't restore
        }
    }

    _glWidget->updateView();
    _viewer->updateDisplayList();
}

void DeleteMeshCommand::redo()
{
    // Move meshes to recycle bin
    for (const QUuid& uuid : _meshUuids)
    {
        int originalIndex = _originalIndices.value(uuid, -1);
        _glWidget->moveToRecycleBin(uuid, originalIndex);
    }

    _glWidget->updateView();
    _viewer->updateDisplayList();
}
