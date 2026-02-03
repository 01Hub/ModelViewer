#include "SetMaterialCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

SetMaterialCommand::SetMaterialCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const GLMaterial& newMaterial,
    const QString& materialName,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _materialName(materialName)
{
    // Capture old materials before applying new
    for (const QUuid& uuid : meshUuids)
    {
        TriangleMesh* mesh = _glWidget->getMeshByUuid(uuid);
        if (mesh)
        {
            // Store complete material state for undo
            _oldMaterials[uuid] = mesh->getMaterial();

            // Store new material state for redo
            _newMaterials[uuid] = newMaterial;
        }
    }

    // Update command text with material name if provided
    if (!_materialName.isEmpty())
    {
        setText(QString("Set Material: %1").arg(_materialName));
    }
}

void SetMaterialCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    applyMaterials(_oldMaterials);
}

void SetMaterialCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    applyMaterials(_newMaterials);
}

void SetMaterialCommand::applyMaterials(const QMap<QUuid, GLMaterial>& materials)
{
    for (auto it = materials.begin(); it != materials.end(); ++it)
    {
        const QUuid& uuid = it.key();
        const GLMaterial& mat = it.value();

        // Get current index for this UUID
        int index = _glWidget->getIndexByUuid(uuid);
        if (index < 0)
        {
            // Mesh was permanently deleted, skip
            continue;
        }

        TriangleMesh* mesh = _glWidget->getMeshByIndex(index);
        if (mesh)
        {
            // Apply material to mesh
            mesh->setMaterial(mat);

            // Handle transmission flag if material has transmission
            if (mat.hasTransmission())
            {
                _glWidget->setTransmissionEnabled(true);
            }
        }
    }

    // Update view after all materials applied
    _glWidget->updateView();
    _glWidget->update();

    // Update material panel to reflect current state if needed
    // This ensures the UI stays in sync with mesh state
    // Note: updateTransformationValues() is for transform panel,
    // we might need updateMaterialPanel() for material panel
    // but that depends on ModelViewer's implementation
}

QSet<QUuid> SetMaterialCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = _oldMaterials.begin(); it != _oldMaterials.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
