#include "SelectionCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

SelectionCommand::SelectionCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QSet<int>& newSelection,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , m_newSelection(newSelection)
{
    // Capture the current selection state before the change
    std::vector<int> currentIDs = m_viewer->getSelectedIDs();
    m_oldSelection = QSet<int>(currentIDs.begin(), currentIDs.end());
}

void SelectionCommand::undo()
{
    applySelection(m_oldSelection);
}

void SelectionCommand::redo()
{
    applySelection(m_newSelection);
}

void SelectionCommand::applySelection(const QSet<int>& selection)
{
    if (!m_viewer || !m_glWidget)
        return;

    // Use the non-undo version to prevent recursion
    m_viewer->setSelectionWithoutUndo(selection);
}

bool SelectionCommand::mergeWith(const QUndoCommand* other)
{
    // Merging is currently disabled - each selection is a separate undo step
    // This gives users fine-grained control over undo/redo

    // To enable merging of consecutive selections, uncomment:
    /*
    if (other->id() != id())
        return false;

    const SelectionCommand* otherCmd = static_cast<const SelectionCommand*>(other);

    // Update the new selection to the latest one
    // This combines multiple rapid selections into one undo step
    m_newSelection = otherCmd->m_newSelection;

    return true;
    */

    return false;
}
