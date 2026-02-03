#include "ApplyPBRTexturesCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

ApplyPBRTexturesCommand::ApplyPBRTexturesCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const GLMaterial& newMaterial,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
{
    // Capture old materials before applying new textures
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
}

void ApplyPBRTexturesCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    applyTextures(_oldMaterials);
}

void ApplyPBRTexturesCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    applyTextures(_newMaterials);
}

void ApplyPBRTexturesCommand::applyTextures(const QMap<QUuid, GLMaterial>& materials)
{
    // Apply textures one mesh at a time using GLWidget's texture application
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

        // Apply textures to this single mesh
        // setTexturesToObjects handles texture loading and GPU upload
        std::vector<int> singleMesh = { index };
        _glWidget->setTexturesToObjects(singleMesh, mat);
    }

    // Update view after all textures applied
    _glWidget->updateView();
    _glWidget->update();
}

QSet<QUuid> ApplyPBRTexturesCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = _oldMaterials.begin(); it != _oldMaterials.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
