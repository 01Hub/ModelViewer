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
	// DEBUG: Log what paths are received in constructor
	qDebug() << "=== ApplyMaterialCommand::constructor START ===";
	qDebug() << "Constructor received - Albedo path:" << newMaterial.albedoMapPath();
	qDebug() << "Constructor received - Normal path:" << newMaterial.normalMapPath();
	qDebug() << "Constructor received - Metallic path:" << newMaterial.metallicMapPath();
	qDebug() << "Constructor received - Roughness path:" << newMaterial.roughnessMapPath();

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

	// DEBUG: Log what paths are actually stored after copy
	qDebug() << "After copy to _newMaterials:";
	if (!_newMaterials.isEmpty())
	{
		auto it = _newMaterials.begin();
		qDebug() << "Stored - Albedo path:" << it->albedoMapPath();
		qDebug() << "Stored - Normal path:" << it->normalMapPath();
		qDebug() << "Stored - Metallic path:" << it->metallicMapPath();
		qDebug() << "Stored - Roughness path:" << it->roughnessMapPath();
	}
	qDebug() << "=== ApplyMaterialCommand::constructor END ===";

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
	qDebug() << "=== ApplyMaterialCommand::applyMaterials START ===";
	qDebug() << "Number of materials to apply:" << materials.size();

    for (auto it = materials.begin(); it != materials.end(); ++it)
    {
        const QUuid& uuid = it.key();
        const GLMaterial& mat = it.value();

		// DEBUG: Log what paths are in the material received by applyMaterials
		qDebug() << "applyMaterials received for UUID" << uuid.toString().left(8);
		qDebug() << "  Albedo path:" << mat.albedoMapPath();
		qDebug() << "  Normal path:" << mat.normalMapPath();
		qDebug() << "  Metallic path:" << mat.metallicMapPath();
		qDebug() << "  Roughness path:" << mat.roughnessMapPath();

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

			// DEBUG: Log what the resolved material contains
			qDebug() << "After resolveMaterialTextures:";
			qDebug() << "  resolved.albedoMapPath():" << resolved.albedoMapPath();
			qDebug() << "  resolved.normalMapPath():" << resolved.normalMapPath();
			qDebug() << "  resolved.metallicMapPath():" << resolved.metallicMapPath();
			qDebug() << "  resolved.roughnessMapPath():" << resolved.roughnessMapPath();
			qDebug() << "Calling mesh->setTextureMaps() with resolved material";

            // Apply textures to mesh using setTextureMaps (mirrors GLWidget::setTexturesToObjects)
            // This properly binds texture GPU IDs and updates texture samplers
            mesh->setTextureMaps(resolved);

			qDebug() << "mesh->setTextureMaps() completed";

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
