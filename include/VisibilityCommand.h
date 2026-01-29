#pragma once

#include "ModelViewerCommand.h"
#include <QSet>
#include <QUuid>

/**
 * @brief Undoable command for mesh visibility changes
 *
 * Handles all visibility operations:
 * - Hide selected meshes
 * - Show selected meshes
 * - Show only selected meshes
 * - Show all meshes
 * - Hide all meshes
 *
 * Stores the old and new sets of visible mesh UUIDs and can restore either state.
 */
class VisibilityCommand : public ModelViewerCommand
{
public:
    /**
     * @brief Construct a visibility command
     * @param viewer The ModelViewer instance
     * @param glWidget The GLWidget instance
     * @param newVisibleUuids The new set of visible mesh UUIDs
     * @param text Description (e.g., "Hide", "Show", "Show Only")
     */
    VisibilityCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QSet<QUuid>& newVisibleUuids,
        const QString& text = QObject::tr("Visibility"));

    void undo() override;
    void redo() override;

    /**
     * @brief Command ID for merging
     * @return Unique ID for VisibilityCommand (6)
     */
    int id() const override { return 6; }

    /**
     * @brief Get all UUIDs referenced by this command
     * @return Set of UUIDs that are referenced (both old and new visibility)
     *
     * Used by cleanup system to determine if command references deleted meshes
     */
    QSet<QUuid> getReferencedUuids() const
    {
        return m_oldVisibleUuids | m_newVisibleUuids;
    }

private:
    QSet<QUuid> m_oldVisibleUuids;  // Visible meshes before operation
    QSet<QUuid> m_newVisibleUuids;  // Visible meshes after operation

    /**
     * @brief Apply a visibility state
     * @param visibleUuids The set of mesh UUIDs that should be visible
     */
    void applyVisibility(const QSet<QUuid>& visibleUuids);
};
