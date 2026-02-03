#pragma once

#include "ModelViewerCommand.h"
#include <QVector>
#include <QSet>
#include <QUuid>

/**
 * @brief Undoable command for mesh duplication
 *
 * Handles duplication of one or more meshes. When undone, duplicates are moved
 * to the recycle bin (similar to deletion). When redone, duplicates are restored
 * from the recycle bin.
 *
 * Also manages selection state:
 * - After duplication, duplicates are auto-selected (without creating SelectionCommand)
 * - On undo, original selection (before duplication) is restored
 * - On redo, duplicates are auto-selected again
 */
class DuplicateCommand : public ModelViewerCommand
{
public:
    /**
     * @brief Construct a duplicate command
     * @param viewer The ModelViewer instance
     * @param glWidget The GLWidget instance
     * @param duplicatedUuids The UUIDs of the newly created duplicate meshes
     * @param originalSelection The UUIDs that were selected before duplication
     * @param text Description (default: "Duplicate")
     *
     * Note: This constructor is called AFTER duplication has already occurred.
     * It receives the original selection as a parameter (captured before duplication).
     */
    DuplicateCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QVector<QUuid>& duplicatedUuids,
        const QSet<QUuid>& originalSelection,
        const QString& text = QObject::tr("Duplicate"));

    void undo() override;
    void redo() override;

    /**
     * @brief Command ID for merging
     * @return Unique ID for DuplicateCommand (7)
     */
    int id() const override { return 7; }

    /**
     * @brief Get all UUIDs referenced by this command
     * @return Set of duplicate mesh UUIDs
     *
     * Used by cleanup system to determine if command references deleted meshes
     */
    QSet<QUuid> getReferencedUuids() const
    {
        return QSet<QUuid>(_duplicatedUuids.begin(), _duplicatedUuids.end());
    }

private:
    QVector<QUuid> _duplicatedUuids;    // UUIDs of the duplicated meshes
    QSet<QUuid> _originalSelection;     // Selection before duplication (passed in)
    bool _firstRedo;                    // Flag to skip first redo call

    /**
     * @brief Convert UUIDs to current indices
     * @param uuids Set of mesh UUIDs
     * @return Set of current indices for these meshes
     */
    QSet<int> convertUuidsToIndices(const QSet<QUuid>& uuids);
};
