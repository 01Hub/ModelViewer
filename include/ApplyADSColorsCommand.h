#pragma once

#include "ModelViewerCommand.h"
#include "ADSMaterialSettingsPanel.h"
#include <QMap>
#include <QSet>
#include <QUuid>
#include <QString>
#include <QVector3D>

/**
 * @brief Undoable command for applying ADS color properties to meshes
 *
 * Handles applying ADS (Ambient-Diffuse-Specular) color properties including:
 * - Ambient color
 * - Diffuse color
 * - Specular color
 * - Emissive color
 * - Opacity value
 * - Shininess value
 *
 * Triggered when user clicks "Apply Colors" button in ADSMaterialSettingsPanel.
 * Stores only color-related properties (~60 bytes per mesh), not textures.
 */
class ApplyADSColorsCommand : public ModelViewerCommand
{
public:
    /**
     * @brief Construct an apply ADS colors command
     * @param viewer The ModelViewer instance
     * @param glWidget The GLWidget instance
     * @param meshUuids The UUIDs of meshes to apply colors to
     * @param ambient Ambient color (RGB)
     * @param diffuse Diffuse color (RGB)
     * @param specular Specular color (RGB)
     * @param emissive Emissive color (RGB)
     * @param opacity Opacity value (0.0-1.0)
     * @param shininess Shininess value (0-128)
     *
     * Constructor captures current color state of all affected meshes
     * before applying new colors, enabling perfect undo restoration.
     */
    ApplyADSColorsCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        ADSMaterialSettingsPanel* adsPanel,
        const QVector<QUuid>& meshUuids,
        const QVector3D& ambient,
        const QVector3D& diffuse,
        const QVector3D& specular,
        const QVector3D& emissive,
        float opacity,
        int shininess);

    void undo() override;
    void redo() override;

    /**
     * @brief Command ID for merging
     * @return Unique ID for ApplyADSColorsCommand (11)
     */
    int id() const override { return 11; }

    /**
     * @brief Get all UUIDs referenced by this command
     * @return Set of mesh UUIDs affected by this color change
     */
    QSet<QUuid> getReferencedUuids() const;

private:
    /**
     * @brief ADS color properties state
     *
     * Stores all color-related properties for a mesh.
     * Size: ~60 bytes (6 values)
     */
    struct ADSColors
    {
        QVector3D ambient;
        QVector3D diffuse;
        QVector3D specular;
        QVector3D emissive;
        float opacity;
        int shininess;
    };

    QMap<QUuid, ADSColors> _oldColors;  // Colors before command
    QMap<QUuid, ADSColors> _newColors;  // Colors after command

    ADSMaterialSettingsPanel* _adsPanel;

    /**
     * @brief Apply colors to meshes
     * @param colors Map of UUID to ADSColors
     *
     * Helper method that applies color properties to their corresponding
     * meshes using mesh setters (setAmbient, setDiffuse, etc.).
     */
    void applyColors(const QMap<QUuid, ADSColors>& colors);

    void updatePanel(const ADSColors& colors);
};

