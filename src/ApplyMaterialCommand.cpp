#include "ApplyMaterialCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

ApplyMaterialCommand::ApplyMaterialCommand(ModelViewer* viewer,
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

void ApplyMaterialCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    applyMaterials(_oldMaterials);
}

void ApplyMaterialCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    applyMaterials(_newMaterials);
}

void ApplyMaterialCommand::applyMaterials(const QMap<QUuid, GLMaterial>& materials)
{
    // Ensure the correct GL context before any GPU-backed material updates.
    if (!_glWidget)
    {
        qWarning() << "ApplyMaterialCommand::applyMaterials - GLWidget is null!";
        return;
    }

    _glWidget->makeCurrent();

    for (auto it = materials.begin(); it != materials.end(); ++it)
    {
        const QUuid& uuid = it.key();
        const GLMaterial& mat = it.value();

        int index = _glWidget->getIndexByUuid(uuid);
        if (index < 0)
        {
            // Mesh was permanently deleted, skip.
            continue;
        }

        TriangleMesh* mesh = _glWidget->getMeshByIndex(index);
        if (!mesh)
            continue;

        // Resolve texture paths to GPU texture IDs and sampler state, then
        // make the resolved GLMaterial the authoritative mesh state.
        GLMaterial resolved = GLWidget::resolveMaterialTextures(_glWidget, mat);
        resolved.setIsGLTFMaterial(true);

        mesh->setMaterial(resolved);
        mesh->setTextureMaps(resolved);
        mesh->invertOpacityADSMap(resolved.isOpacityMapInverted());
        mesh->invertOpacityPBRMap(resolved.isOpacityMapInverted());

        if (mat.hasTransmission())
            _glWidget->setTransmissionEnabled(true);
    }

    _glWidget->updateView();
    _glWidget->update();
}

QSet<QUuid> ApplyMaterialCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = _oldMaterials.begin(); it != _oldMaterials.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
