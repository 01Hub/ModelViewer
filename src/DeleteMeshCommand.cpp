#include "DeleteMeshCommand.h"
#include "GLWidget.h"
#include "ModelViewer.h"
#include "SceneGraph.h"
#include "TriangleMesh.h"

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

    // Re-register punctual light data for any files that had all their meshes
    // deleted.  validateLightData() cleared this data during redo(); we put it
    // back now that the meshes are live again.
    if (sg)
    {
        for (auto it = _savedLightData.cbegin(); it != _savedLightData.cend(); ++it)
            sg->setLightData(it.key(), it.value());
    }

    _glWidget->updateView();
    _viewer->updateDisplayList();
}

void DeleteMeshCommand::redo()
{
    SceneGraph* sg = _viewer->sceneGraph();

    // Snapshot punctual-light data for any file that is about to lose ALL its
    // meshes.  This must happen before removeMeshUuid() because structureChanged
    // → validateLightData() → clearLightData() fires synchronously inside that
    // call and would destroy the data we need to save.
    _savedLightData.clear();
    if (sg)
    {
        const QSet<QUuid> deletingSet(_meshUuids.begin(), _meshUuids.end());

        // Collect unique source files for the meshes being deleted.
        QSet<QString> candidateFiles;
        for (const QUuid& uuid : _meshUuids)
        {
            if (TriangleMesh* m = _glWidget->getMeshByUuid(uuid))
                candidateFiles.insert(m->getSourceFile());
        }

        for (const QString& file : candidateFiles)
        {
            const GltfLightData& ld = sg->lightDataForFile(file);
            if (ld.isEmpty())
                continue;

            // Only snapshot if every mesh belonging to this file is in the
            // deletion set — i.e., the file will have no remaining live mesh.
            bool allDeleted = true;
            const std::vector<TriangleMesh*>& store = _glWidget->getMeshStore();
            for (const TriangleMesh* mesh : store)
            {
                if (mesh && mesh->getSourceFile() == file &&
                    !deletingSet.contains(mesh->uuid()))
                {
                    allDeleted = false;
                    break;
                }
            }
            if (allDeleted)
                _savedLightData[file] = ld;
        }
    }

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
