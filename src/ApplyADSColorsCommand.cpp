#include "ApplyADSColorsCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

ApplyADSColorsCommand::ApplyADSColorsCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    ADSMaterialSettingsPanel* adsPanel,
    const QVector<QUuid>& meshUuids,
    const QVector3D& ambient,
    const QVector3D& diffuse,
    const QVector3D& specular,
    const QVector3D& emissive,
    float opacity,
    int shininess)
	: ModelViewerCommand(viewer, glWidget, QObject::tr("Apply ADS Colors")), _adsPanel(adsPanel)
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

    // Just update panel's material and refresh sliders
    if (_adsPanel && !m_oldColors.isEmpty())
    {
        const ADSColors& colors = m_oldColors.first();

        // Get panel's material and update it
        GLMaterial* mat = _adsPanel->getMaterial();
        if (mat)
        {
            mat->setOpacity(colors.opacity);
            mat->setShininess(colors.shininess);
            // Colors too if you want
            mat->setAmbient(colors.ambient);
            mat->setDiffuse(colors.diffuse);
            mat->setSpecular(colors.specular);
            mat->setEmissive(colors.emissive);
        }

		// Update color buttons
		_adsPanel->updateMaterialButtonStyles();
        // Update sliders to reflect material
        _adsPanel->updateMaterialPropertySliders();
    }
}

void ApplyADSColorsCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyColors(m_newColors);

    if (_adsPanel && !m_newColors.isEmpty())
    {
        const ADSColors& colors = m_newColors.first();

        GLMaterial* mat = _adsPanel->getMaterial();
        if (mat)
        {
            mat->setOpacity(colors.opacity);
            mat->setShininess(colors.shininess);
            mat->setAmbient(colors.ambient);
            mat->setDiffuse(colors.diffuse);
            mat->setSpecular(colors.specular);
            mat->setEmissive(colors.emissive);
        }

        // Update color buttons
        _adsPanel->updateMaterialButtonStyles();
		// Update sliders to reflect material
        _adsPanel->updateMaterialPropertySliders();
    }
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
			mat.setShininess(adsColors.shininess);
			mat.setOpacity(adsColors.opacity);

            // Set material back to mesh
            mesh->setMaterial(mat);          
        }
    }

    // Update view after all colors applied
    m_glWidget->updateView();
    m_glWidget->update();
}

void ApplyADSColorsCommand::updatePanel(const ADSColors& colors)
{
    if (!_adsPanel)
        return;

    // Update panel's internal material
    GLMaterial* material = _adsPanel->getMaterial();  // Or however you access it
    if (material)
    {
        material->setAmbient(colors.ambient);
        material->setDiffuse(colors.diffuse);
        material->setSpecular(colors.specular);
        material->setEmissive(colors.emissive);
        material->setOpacity(colors.opacity);
        material->setShininess(colors.shininess);
    }

    // Update all sliders to reflect these values
    _adsPanel->updateMaterialPropertySliders();
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
