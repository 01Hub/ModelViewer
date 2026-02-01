#include "ApplyADSTexturesCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

ApplyADSTexturesCommand::ApplyADSTexturesCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& meshUuids,
    const QString& diffusePath,
    const QString& specularPath,
    const QString& normalPath,
    const QString& emissivePath,
    const QString& heightPath,
    const QString& opacityPath,
    bool opacityInverted)
    : ModelViewerCommand(viewer, glWidget, QObject::tr("Apply ADS Textures"))
{
    // Capture old texture states
    for (const QUuid& uuid : meshUuids)
    {
        int index = m_glWidget->getIndexByUuid(uuid);
        if (index < 0) continue;

        TriangleMesh* mesh = m_glWidget->getMeshByIndex(index);
        if (!mesh) continue;

        // Store old texture state
        ADSTextures oldTex;
        // TODO: Need to get old paths from mesh
        // For now, assume empty (textures weren't set before)
        oldTex.diffusePath = "";
        oldTex.specularPath = "";
        oldTex.normalPath = "";
        oldTex.emissivePath = "";
        oldTex.heightPath = "";
        oldTex.opacityPath = "";
        oldTex.opacityInverted = false;
        oldTex.hasDiffuse = false;
        oldTex.hasSpecular = false;
        oldTex.hasNormal = false;
        oldTex.hasEmissive = false;
        oldTex.hasHeight = false;
        oldTex.hasOpacity = false;

        m_oldTextures[uuid] = oldTex;

        // Store new texture state
        ADSTextures newTex;
        newTex.diffusePath = diffusePath;
        newTex.specularPath = specularPath;
        newTex.normalPath = normalPath;
        newTex.emissivePath = emissivePath;
        newTex.heightPath = heightPath;
        newTex.opacityPath = opacityPath;
        newTex.opacityInverted = opacityInverted;
        newTex.hasDiffuse = !diffusePath.isEmpty();
        newTex.hasSpecular = !specularPath.isEmpty();
        newTex.hasNormal = !normalPath.isEmpty();
        newTex.hasEmissive = !emissivePath.isEmpty();
        newTex.hasHeight = !heightPath.isEmpty();
        newTex.hasOpacity = !opacityPath.isEmpty();

        m_newTextures[uuid] = newTex;
    }
}

void ApplyADSTexturesCommand::undo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyTextures(m_oldTextures);
}

void ApplyADSTexturesCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    applyTextures(m_newTextures);
}

void ApplyADSTexturesCommand::applyTextures(const QMap<QUuid, ADSTextures>& textures)
{
    // Convert UUIDs to indices
    std::vector<int> ids;
    for (auto it = textures.begin(); it != textures.end(); ++it)
    {
        int index = m_glWidget->getIndexByUuid(it.key());
        if (index >= 0)
            ids.push_back(index);
    }

    if (ids.empty())
        return;

    // Get the texture state (all meshes get same textures)
    const ADSTextures& tex = textures.first();

    // Use the ORIGINAL ADS-specific methods that WORK!
    m_glWidget->enableADSDiffuseTexMap(ids, tex.hasDiffuse);
    if (tex.hasDiffuse)
    {
        m_glWidget->setADSDiffuseTexMap(ids, tex.diffusePath);
    }

    m_glWidget->enableADSSpecularTexMap(ids, tex.hasSpecular);
    if (tex.hasSpecular)
    {
        m_glWidget->setADSSpecularTexMap(ids, tex.specularPath);
    }

    m_glWidget->enableADSNormalTexMap(ids, tex.hasNormal);
    if (tex.hasNormal)
    {
        m_glWidget->setADSNormalTexMap(ids, tex.normalPath);
    }

    m_glWidget->enableADSEmissiveTexMap(ids, tex.hasEmissive);
    if (tex.hasEmissive)
    {
        m_glWidget->setADSEmissiveTexMap(ids, tex.emissivePath);
    }

    m_glWidget->enableADSHeightTexMap(ids, tex.hasHeight);
    if (tex.hasHeight)
    {
        m_glWidget->setADSHeightTexMap(ids, tex.heightPath);
    }

    m_glWidget->enableADSOpacityTexMap(ids, tex.hasOpacity);
    if (tex.hasOpacity)
    {
        m_glWidget->setADSOpacityTexMap(ids, tex.opacityPath);
    }

    // Update view
    m_glWidget->updateView();
    m_glWidget->update();
}

QSet<QUuid> ApplyADSTexturesCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;
    for (auto it = m_oldTextures.begin(); it != m_oldTextures.end(); ++it)
    {
        uuids.insert(it.key());
    }
    return uuids;
}
