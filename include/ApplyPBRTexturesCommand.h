#pragma once

#include "ModelViewerCommand.h"
#include "GLMaterial.h"
#include <QMap>
#include <QSet>
#include <QUuid>
#include <QString>

/**
 * @brief Undoable command for applying textures to meshes
 *
 * Handles applying texture maps to one or more meshes. Typically triggered
 * when user clicks "Apply" button in TextureMappingPanel (PBR textures) or
 * ADSMaterialSettingsPanel (ADS textures).
 *
 * Stores the complete material state for each affected mesh to ensure proper
 * undo/redo of all texture-related changes including texture paths, IDs,
 * transforms, and sampler settings.
 *
 * The command applies textures using GLWidget::setTexturesToObjects() which
 * handles texture loading, GPU upload, and material resolution automatically.
 */
class ApplyPBRTexturesCommand : public ModelViewerCommand
{
public:
    /**
     * @brief Construct an apply textures command
     * @param viewer The ModelViewer instance
     * @param glWidget The GLWidget instance
     * @param meshUuids The UUIDs of meshes to apply textures to
     * @param newMaterial The material containing textures to apply
     * @param text Description for undo stack (default: "Apply Textures")
     *
     * The constructor captures the current material state of all affected meshes
     * before any changes are made. This allows perfect restoration on undo.
     *
     * The newMaterial parameter should contain the texture paths and settings
     * to be applied. The command will handle texture loading and GPU upload
     * via GLWidget::setTexturesToObjects().
     */
    ApplyPBRTexturesCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QVector<QUuid>& meshUuids,
        const GLMaterial& newMaterial,
        const QString& text = QObject::tr("Apply Textures"));

    void undo() override;
    void redo() override;

    /**
     * @brief Command ID for merging
     * @return Unique ID for ApplyPBRTexturesCommand (10)
     */
    int id() const override { return 10; }

    /**
     * @brief Get all UUIDs referenced by this command
     * @return Set of mesh UUIDs affected by this texture change
     *
     * Used by cleanup system to determine if command references deleted meshes
     */
    QSet<QUuid> getReferencedUuids() const;

private:
    QMap<QUuid, GLMaterial> _oldMaterials;  // Materials before command
    QMap<QUuid, GLMaterial> _newMaterials;  // Materials after command

    /**
     * @brief Apply textures to meshes
     * @param materials Map of UUID to GLMaterial
     *
     * Helper method that applies textures from materials to their corresponding
     * meshes. Uses GLWidget::setTexturesToObjects() to handle texture loading,
     * resolution, and application. Updates view after all textures applied.
     */
    void applyTextures(const QMap<QUuid, GLMaterial>& materials);
};

