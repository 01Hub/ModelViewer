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
	// CRITICAL FIX: Ensure correct GL context before any GPU operations
	// This prevents GL context corruption when material is applied from detached dialog
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
            // Resolve texture paths to GPU texture IDs
            // This handles loading textures from disk and applying sampler settings
            GLMaterial resolved = GLWidget::resolveMaterialTextures(_glWidget, mat);

            // Apply textures to mesh using setTextureMaps (mirrors GLWidget::setTexturesToObjects)
            // This properly binds texture GPU IDs and updates texture samplers
            mesh->setTextureMaps(resolved);

			// ADS Cascading: Also bind textures for ADS rendering mode

			// Try using albedo as diffuse since we may not have set diffuse directly
			if (!mat.albedoMapPath().isEmpty())
			{
				_glWidget->setADSDiffuseTexMap({index}, mat.albedoMapPath());
			}
			else if (mat.hasDiffuseMap())
			{
				QString diffusePath = mat.diffuseMap();
				if (!diffusePath.isEmpty())
				{
					_glWidget->setADSDiffuseTexMap({index}, diffusePath);
				}
			}
			else
			{
				// Disable diffuse texture when material has no diffuse/albedo map
				// This prevents stale texture bindings from previous materials from being used in ADS mode
				_glWidget->enableADSDiffuseTexMap({index}, false);
			}

			// If emissive map is available, bind it for ADS emissive rendering
			if (mat.hasEmissiveMap())
			{
				QString emissivePath = mat.emissiveMapPath();
				if (!emissivePath.isEmpty())
				{
					_glWidget->setADSEmissiveTexMap({index}, emissivePath);
				}
			}
			else
			{
				_glWidget->enableADSEmissiveTexMap({index}, false);
			}

			// Map: Normal (PBR) → Normal (ADS)
			if (mat.hasNormalMap())
			{
				QString normalPath = mat.normalMapPath();
				if (!normalPath.isEmpty())
				{
					_glWidget->setADSNormalTexMap({index}, normalPath);
				}
			}
			else
			{
				_glWidget->enableADSNormalTexMap({index}, false);
			}

			// Map: Metallic (PBR) → Specular (ADS)
			if (mat.hasMetallicMap())
			{
				QString metallicPath = mat.metallicMapPath();
				if (!metallicPath.isEmpty())
				{
					_glWidget->setADSSpecularTexMap({index}, metallicPath);
				}
			}
			else
			{
				_glWidget->enableADSSpecularTexMap({index}, false);
			}

			// Map: Height (PBR) → Height (ADS)
			if (mat.hasHeightMap())
			{
				QString heightPath = mat.heightMapPath();
				if (!heightPath.isEmpty())
				{
					_glWidget->setADSHeightTexMap({index}, heightPath);
				}
			}
			else
			{
				_glWidget->enableADSHeightTexMap({index}, false);
			}

			// Map: Opacity (PBR) → Opacity (ADS)
			if (mat.hasOpacityMap())
			{
				QString opacityPath = mat.opacityMapPath();
				if (!opacityPath.isEmpty())
				{
					_glWidget->setADSOpacityTexMap({index}, opacityPath);
				}
			}
			else
			{
				_glWidget->enableADSOpacityTexMap({index}, false);
			}

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

QSet<QUuid> ApplyMaterialCommand::getReferencedUuids() const
{
    QSet<QUuid> uuids;

    for (auto it = _oldMaterials.begin(); it != _oldMaterials.end(); ++it)
    {
        uuids.insert(it.key());
    }

    return uuids;
}
