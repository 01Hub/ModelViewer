#include "DuplicateCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

DuplicateCommand::DuplicateCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& duplicatedUuids,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , m_duplicatedUuids(duplicatedUuids)
    , m_firstRedo(true)
{
    // Capture the selection state BEFORE auto-selecting duplicates
    // This allows us to restore the original selection on undo
    m_originalSelection = m_viewer->getSelectedUuids();
}

void DuplicateCommand::undo()
{
    if (!m_viewer || !m_glWidget)
        return;

    // Move duplicated meshes to recycle bin
    for (const QUuid& uuid : m_duplicatedUuids)
    {
        int index = m_glWidget->getIndexByUuid(uuid);
        if (index >= 0)
        {
            m_glWidget->moveToRecycleBin(uuid, index);
        }
    }

    // Update view and UI
    m_glWidget->updateView();
    m_viewer->updateDisplayList();

    // Restore the original selection (before duplication)
    m_viewer->setSelectionWithoutUndo(m_originalSelection);
}

void DuplicateCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    if (m_firstRedo)
    {
        // First redo is called automatically by QUndoStack::push()
        // But duplication has already happened, so just select the duplicates
        m_firstRedo = false;

        // Auto-select the duplicates (without creating SelectionCommand)
        QSet<QUuid> duplicateSet(m_duplicatedUuids.begin(), m_duplicatedUuids.end());
        m_viewer->setSelectionWithoutUndo(duplicateSet);
        return;
    }

    // Subsequent redos: restore duplicates from recycle bin
    for (const QUuid& uuid : m_duplicatedUuids)
    {
        m_glWidget->restoreFromRecycleBin(uuid);
    }

    // Update view and UI
    m_glWidget->updateView();
    m_viewer->updateDisplayList();

    // Auto-select the restored duplicates
    QSet<QUuid> duplicateSet(m_duplicatedUuids.begin(), m_duplicatedUuids.end());
    m_viewer->setSelectionWithoutUndo(duplicateSet);
}

QSet<int> DuplicateCommand::convertUuidsToIndices(const QSet<QUuid>& uuids)
{
    QSet<int> indices;

    for (const QUuid& uuid : uuids)
    {
        int index = m_glWidget->getIndexByUuid(uuid);
        if (index >= 0)
        {
            indices.insert(index);
        }
    }

    return indices;
}
