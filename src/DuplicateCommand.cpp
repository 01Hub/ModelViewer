#include "DuplicateCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

DuplicateCommand::DuplicateCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& duplicatedUuids,
    const QSet<QUuid>& originalSelection,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , m_duplicatedUuids(duplicatedUuids)
    , m_originalSelection(originalSelection)  // Use passed-in value
    , m_firstRedo(true)
{
    // Original selection is passed as parameter (captured before updateDisplayList)
    // No need to capture it here
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

    // Update view and UI FIRST (this rebuilds the list)
    m_glWidget->updateView();
    m_viewer->updateDisplayList();

    // THEN restore the original selection (after list is stable)
    m_viewer->setSelectionWithoutUndo(m_originalSelection);
}

void DuplicateCommand::redo()
{
    if (!m_viewer || !m_glWidget)
        return;

    if (m_firstRedo)
    {
        // First redo is called automatically by QUndoStack::push()
        // Duplication has already happened, list has already been updated
        // Just select the duplicates
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

    // Update view and UI FIRST (this rebuilds the list)
    m_glWidget->updateView();
    m_viewer->updateDisplayList();

    // THEN auto-select the restored duplicates (after list is stable)
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
