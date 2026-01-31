#include "ApplyTexturesCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

ApplyTexturesCommand::ApplyTexturesCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const GLMaterial& newMaterial,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
{
    // Capture old materials before applying new textures
    for (const QUuid& uuid : meshUuids)
    {
        TriangleMesh* mesh = m_glWidget->getMeshByUuid(uuid);
        if (mesh)
        {
            // Store complete material state for undo
            m_oldMaterials[uuid] = mesh->getMaterial();

            // Store new material state for redo
            m_newMaterials[uuid] = newMaterial;
        }
    }
}

void ApplyTexturesCommand::undo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyTextures(m_oldMaterials);
}

void ApplyTexturesCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyTextures(m_newMaterials);
}

void ApplyTexturesCommand::applyTextures(const QMap<QUuid, GLMaterial>& materials)
{
    // Apply textures one mesh at a time using GLWidget's texture application
    for (auto it = materials.begin(); it != materials.end(); ++it)
    {
        const QUuid& uuid = it.key();
        const GLMaterial& mat = it.value();

        // Get current index for this UUID
        int index = m_glWidget->getIndexByUuid(uuid);
        if (index < 0)
        {
            // Mesh was permanently deleted, skip
            continue;
        }

        // Apply textures to this single mesh
        // setTexturesToObjects handles texture loading and GPU upload
        std::vector<int> singleMesh = { index };
        m_glWidget->setTexturesToObjects(singleMesh, mat);
    }

    // Update view after all textures applied
    m_glWidget->updateView();
    m_glWidget->update();
}

QSet<QUuid> ApplyTexturesCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = m_oldMaterials.begin(); it != m_oldMaterials.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
