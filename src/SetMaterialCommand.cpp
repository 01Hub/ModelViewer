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
    , m_materialName(materialName)
{
    // Capture old materials before applying new
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

    // Update command text with material name if provided
    if (!m_materialName.isEmpty())
    {
        setText(QString("Set Material: %1").arg(m_materialName));
    }
}

void SetMaterialCommand::undo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyMaterials(m_oldMaterials);
}

void SetMaterialCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyMaterials(m_newMaterials);
}

void SetMaterialCommand::applyMaterials(const QMap<QUuid, GLMaterial>& materials)
{
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

        TriangleMesh* mesh = m_glWidget->getMeshByIndex(index);
        if (mesh)
        {
            // Apply material to mesh
            mesh->setMaterial(mat);

            // Handle transmission flag if material has transmission
            if (mat.hasTransmission())
            {
                m_glWidget->setTransmissionEnabled(true);
            }
        }
    }

    // Update view after all materials applied
    m_glWidget->updateView();
    m_glWidget->update();

    // Update material panel to reflect current state if needed
    // This ensures the UI stays in sync with mesh state
    // Note: updateTransformationValues() is for transform panel,
    // we might need updateMaterialPanel() for material panel
    // but that depends on ModelViewer's implementation
}

QSet<QUuid> SetMaterialCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = m_oldMaterials.begin(); it != m_oldMaterials.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
