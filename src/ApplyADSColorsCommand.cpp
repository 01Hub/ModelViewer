#include "ApplyADSColorsCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

ApplyADSColorsCommand::ApplyADSColorsCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const QVector3D& ambient,
    const QVector3D& diffuse,
    const QVector3D& specular,
    const QVector3D& emissive,
    float opacity,
    int shininess)
    : ModelViewerCommand(viewer, glWidget, QObject::tr("Apply ADS Colors"))
{
    // Capture old colors before applying new ones
    for (const QUuid& uuid : meshUuids)
    {
        TriangleMesh* mesh = m_glWidget->getMeshByUuid(uuid);
        if (mesh)
        {
            GLMaterial mat = mesh->getMaterial();

            // Store old colors
            ADSColors oldColors;
            oldColors.ambient = mat.ambient();
            oldColors.diffuse = mat.diffuse();
            oldColors.specular = mat.specular();
            oldColors.emissive = mat.emissive();
            oldColors.opacity = mat.opacity();
            oldColors.shininess = mat.shininess();

            m_oldColors[uuid] = oldColors;

            // Store new colors
            ADSColors newColors;
            newColors.ambient = ambient;
            newColors.diffuse = diffuse;
            newColors.specular = specular;
            newColors.emissive = emissive;
            newColors.opacity = opacity;
            newColors.shininess = shininess;

            m_newColors[uuid] = newColors;
        }
    }
}

void ApplyADSColorsCommand::undo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyColors(m_oldColors);
}

void ApplyADSColorsCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyColors(m_newColors);
}

void ApplyADSColorsCommand::applyColors(const QMap<QUuid, ADSColors>& colors)
{
    for (auto it = colors.begin(); it != colors.end(); ++it)
    {
        const QUuid& uuid = it.key();
        const ADSColors& adsColors = it.value();

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
            // Get material and update colors
            GLMaterial mat = mesh->getMaterial();
            mat.setAmbient(adsColors.ambient);
            mat.setDiffuse(adsColors.diffuse);
            mat.setSpecular(adsColors.specular);
            mat.setEmissive(adsColors.emissive);

            // Set material back to mesh
            mesh->setMaterial(mat);

            // Set opacity and shininess directly on mesh
            mesh->setOpacity(adsColors.opacity);
            mesh->setShininess(adsColors.shininess);
        }
    }

    // Update view after all colors applied
    m_glWidget->updateView();
    m_glWidget->update();
}

QSet<QUuid> ApplyADSColorsCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = m_oldColors.begin(); it != m_oldColors.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
