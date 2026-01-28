#pragma once

#include "ModelViewerCommand.h"
#include <QVector>
#include <QMap>
#include <QUuid>
#include <QSet>

class ModelViewer;
class GLWidget;

/**
 * @brief Undoable command for deleting meshes
 *
 * Captures the UUIDs of deleted meshes and their original positions
 * in the model list to allow restoration on undo.
 */
class DeleteMeshCommand : public ModelViewerCommand
{
public:
    DeleteMeshCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QVector<QUuid>& meshUuids,
        const QString& text = QObject::tr("Delete"));

    void undo() override;
    void redo() override;

    int id() const override { return 5; }

    // For cleanup system
    QSet<QUuid> getReferencedUuids() const
    {
        return QSet<QUuid>(m_meshUuids.begin(), m_meshUuids.end());
    }

private:
    QVector<QUuid> m_meshUuids;
    QMap<QUuid, int> m_originalIndices;  // Store original positions
};
