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
        int index = _glWidget->getIndexByUuid(uuid);
        if (index < 0) continue;

        TriangleMesh* mesh = _glWidget->getMeshByIndex(index);
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

        _oldTextures[uuid] = oldTex;

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

        _newTextures[uuid] = newTex;
    }
}

void ApplyADSTexturesCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    applyTextures(_oldTextures);
}

void ApplyADSTexturesCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    applyTextures(_newTextures);
}

void ApplyADSTexturesCommand::applyTextures(const QMap<QUuid, ADSTextures>& textures)
{
    // Convert UUIDs to indices
    std::vector<int> ids;
    for (auto it = textures.begin(); it != textures.end(); ++it)
    {
        int index = _glWidget->getIndexByUuid(it.key());
        if (index >= 0)
            ids.push_back(index);
    }

    if (ids.empty())
        return;

    // Get the texture state (all meshes get same textures)
    const ADSTextures& tex = textures.first();

    // Use the ORIGINAL ADS-specific methods that WORK!
    _glWidget->enableADSDiffuseTexMap(ids, tex.hasDiffuse);
    if (tex.hasDiffuse)
    {
        _glWidget->setADSDiffuseTexMap(ids, tex.diffusePath);
    }

    _glWidget->enableADSSpecularTexMap(ids, tex.hasSpecular);
    if (tex.hasSpecular)
    {
        _glWidget->setADSSpecularTexMap(ids, tex.specularPath);
    }

    _glWidget->enableADSNormalTexMap(ids, tex.hasNormal);
    if (tex.hasNormal)
    {
        _glWidget->setADSNormalTexMap(ids, tex.normalPath);
    }

    _glWidget->enableADSEmissiveTexMap(ids, tex.hasEmissive);
    if (tex.hasEmissive)
    {
        _glWidget->setADSEmissiveTexMap(ids, tex.emissivePath);
    }

    _glWidget->enableADSHeightTexMap(ids, tex.hasHeight);
    if (tex.hasHeight)
    {
        _glWidget->setADSHeightTexMap(ids, tex.heightPath);
    }

    _glWidget->enableADSOpacityTexMap(ids, tex.hasOpacity);
    if (tex.hasOpacity)
    {
        _glWidget->setADSOpacityTexMap(ids, tex.opacityPath);
    }

    // Update view
    _glWidget->updateView();
    _glWidget->update();
}

QSet<QUuid> ApplyADSTexturesCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;
    for (auto it = _oldTextures.begin(); it != _oldTextures.end(); ++it)
    {
        uuids.insert(it.key());
    }
    return uuids;
}
