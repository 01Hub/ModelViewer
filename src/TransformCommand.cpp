#include "TransformCommand.h"
#include "MainWindow.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

TransformCommand::TransformCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const QVector3D& newTranslation,
    const QVector3D& newRotation,
    const QVector3D& newScale,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
{
    // Capture old states for all meshes before transformation
    for (const QUuid& uuid : meshUuids)
    {
        TriangleMesh* mesh = _glWidget->getMeshByUuid(uuid);
        if (mesh)
        {
            TransformState oldState;
            oldState.translation = mesh->getTranslation();
            oldState.rotation = mesh->getRotation();
            oldState.scale = mesh->getScaling();

            _oldStates[uuid] = oldState;

            // Store new state (same for all meshes in this command)
            TransformState newState;
            newState.translation = newTranslation;
            newState.rotation = newRotation;
            newState.scale = newScale;

            _newStates[uuid] = newState;
        }
    }
}

TransformCommand::TransformCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QMap<QUuid, TransformState>& oldStates,
    const QMap<QUuid, TransformState>& newStates,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text),
      _oldStates(oldStates),
      _newStates(newStates)
{
}

void TransformCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    // Check if any meshes can be undone
    int skippedCount = 0;

    // Apply old transformation states
    for (auto it = _oldStates.begin(); it != _oldStates.end(); ++it)
    {
        const QUuid& uuid = it.key();

        // Skip meshes that have been baked
        if (_bakedMeshes.contains(uuid))
        {
            skippedCount++;
            continue;
        }

        // Check if mesh still exists
        int index = _glWidget->getIndexByUuid(uuid);
        if (index < 0)
        {
            // Mesh was permanently deleted, skip
            continue;
        }
    }

    // Apply the old states that weren't skipped
    applyTransformStates(_oldStates);

    // Show status message if some meshes were skipped due to baking
    if (skippedCount > 0)
    {
        QString msg;
        if (skippedCount == 1)
            msg = QObject::tr("1 mesh was baked - transform undo skipped");
        else
            msg = QObject::tr("%1 meshes were baked - transform undo skipped").arg(skippedCount);

        // Show in status bar
		MainWindow::showStatusMessage(msg, 3000);   
    }
}

void TransformCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    // Apply new transformation states
    applyTransformStates(_newStates);
}

void TransformCommand::applyTransformStates(const QMap<QUuid, TransformState>& states)
{
    // Build map of index -> TransformState for GLWidget
    QMap<int, TransformState> indexedStates;

    for (auto it = states.begin(); it != states.end(); ++it)
    {
        const QUuid& uuid = it.key();

        // Skip baked meshes
        if (_bakedMeshes.contains(uuid))
            continue;

        // Get current index
        int index = _glWidget->getIndexByUuid(uuid);
        if (index >= 0)
        {
            indexedStates[index] = it.value();
        }
    }

    // Apply all transformations efficiently (one update at end)
    if (!indexedStates.isEmpty())
    {
        _glWidget->applyTransforms(indexedStates);
        _viewer->syncLightPositionUiToScene();
        _glWidget->update();

        // Update panel to show current transform values
        _viewer->updateTransformationValues();
    }
}

QSet<QUuid> TransformCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = _oldStates.begin(); it != _oldStates.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}

bool TransformCommand::affectsAnyUuid(const QVector<QUuid>& uuids) const
{
    for (const QUuid& uuid : uuids)
    {
        if (_oldStates.contains(uuid))
            return true;
    }

    return false;
}

void TransformCommand::markMeshBaked(const QUuid& uuid) const
{
    _bakedMeshes.insert(uuid);
}
