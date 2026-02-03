#pragma once

#include "ModelViewerCommand.h"
#include <QVector3D>
#include <QMap>
#include <QSet>
#include <QUuid>

/**
 * @brief State of a mesh's transformation
 */
struct TransformState
{
    QVector3D translation;
    QVector3D rotation;
    QVector3D scale;

    TransformState()
        : translation(0, 0, 0)
        , rotation(0, 0, 0)
        , scale(1, 1, 1)
    {
    }

    TransformState(const QVector3D& trans, const QVector3D& rot, const QVector3D& scl)
        : translation(trans)
        , rotation(rot)
        , scale(scl)
    {
    }
};

/**
 * @brief Undoable command for mesh transformations
 *
 * Handles transformation of one or more meshes (translation, rotation, scale).
 * Supports partial undo when some meshes have been baked.
 *
 * Each mesh can have different transformation values, so states are stored
 * per-mesh using UUID as key.
 */
class TransformCommand : public ModelViewerCommand
{
public:
    /**
     * @brief Construct a transform command
     * @param viewer The ModelViewer instance
     * @param glWidget The GLWidget instance
     * @param meshUuids The UUIDs of meshes to transform
     * @param newTranslation New translation value for all meshes
     * @param newRotation New rotation value for all meshes (degrees)
     * @param newScale New scale value for all meshes
     * @param text Description (default: "Transform")
     */
    TransformCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QVector<QUuid>& meshUuids,
        const QVector3D& newTranslation,
        const QVector3D& newRotation,
        const QVector3D& newScale,
        const QString& text = QObject::tr("Transform"));

    void undo() override;
    void redo() override;

    /**
     * @brief Command ID for merging
     * @return Unique ID for TransformCommand (8)
     */
    int id() const override { return 8; }

    /**
     * @brief Get all UUIDs referenced by this command
     * @return Set of mesh UUIDs that are transformed
     *
     * Used by cleanup system to determine if command references deleted meshes
     */
    QSet<QUuid> getReferencedUuids() const;

    /**
     * @brief Check if this command affects any of the given UUIDs
     * @param uuids UUIDs to check
     * @return true if any UUID is affected by this command
     *
     * Used when baking to find commands that should be marked obsolete
     */
    bool affectsAnyUuid(const QVector<QUuid>& uuids) const;

    /**
     * @brief Mark a mesh as baked
     * @param uuid UUID of the baked mesh
     *
     * When a mesh is baked, transformations to it can no longer be undone.
     * This marks the mesh so undo/redo will skip it.
     */
    void markMeshBaked(const QUuid& uuid) const;

private:
    QMap<QUuid, TransformState> _oldStates;  // Transform states before command
    QMap<QUuid, TransformState> _newStates;  // Transform states after command
    mutable QSet<QUuid> _bakedMeshes;                // Meshes that have been baked

    /**
     * @brief Apply transformation states to meshes
     * @param states Map of UUID to TransformState
     */
    void applyTransformStates(const QMap<QUuid, TransformState>& states);
};
