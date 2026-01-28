#include "DeleteMeshCommand.h"
#include "GLWidget.h"
#include "ModelViewer.h"

DeleteMeshCommand::DeleteMeshCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , m_meshUuids(meshUuids)
{
    // Store original indices for each mesh
    for (const QUuid& uuid : meshUuids)
    {
        int index = m_glWidget->getIndexByUuid(uuid);
        if (index >= 0)
            m_originalIndices[uuid] = index;
    }
}

void DeleteMeshCommand::undo()
{
    // Restore meshes from recycle bin
    for (const QUuid& uuid : m_meshUuids)
    {
        if (m_glWidget->isInRecycleBin(uuid))
        {
            m_glWidget->restoreFromRecycleBin(uuid);
        }
        else
        {
            qWarning() << "DeleteMeshCommand::undo - Mesh not in bin:" << uuid;
            // Mesh was permanently deleted - can't restore
        }
    }

    m_glWidget->updateView();
    m_viewer->updateDisplayList();
}

void DeleteMeshCommand::redo()
{
    // Move meshes to recycle bin
    for (const QUuid& uuid : m_meshUuids)
    {
        int originalIndex = m_originalIndices.value(uuid, -1);
        m_glWidget->moveToRecycleBin(uuid, originalIndex);
    }

    m_glWidget->updateView();
    m_viewer->updateDisplayList();
}
