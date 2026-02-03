#include "DuplicateCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

DuplicateCommand::DuplicateCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QVector<QUuid>& duplicatedUuids,
    const QSet<QUuid>& originalSelection,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _duplicatedUuids(duplicatedUuids)
    , _originalSelection(originalSelection)  // Use passed-in value
    , _firstRedo(true)
{
    // Original selection is passed as parameter (captured before updateDisplayList)
    // No need to capture it here
}

void DuplicateCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    // Move duplicated meshes to recycle bin
    for (const QUuid& uuid : _duplicatedUuids)
    {
        int index = _glWidget->getIndexByUuid(uuid);
        if (index >= 0)
        {
            _glWidget->moveToRecycleBin(uuid, index);
        }
    }

    // Update view and UI FIRST (this rebuilds the list)
    _glWidget->updateView();
    _viewer->updateDisplayList();

    // THEN restore the original selection (after list is stable)
    _viewer->setSelectionWithoutUndo(_originalSelection);
}

void DuplicateCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    if (_firstRedo)
    {
        // First redo is called automatically by QUndoStack::push()
        // Duplication has already happened, list has already been updated
        // Just select the duplicates
        _firstRedo = false;

        // Auto-select the duplicates (without creating SelectionCommand)
        QSet<QUuid> duplicateSet(_duplicatedUuids.begin(), _duplicatedUuids.end());
        _viewer->setSelectionWithoutUndo(duplicateSet);
        return;
    }

    // Subsequent redos: restore duplicates from recycle bin
    for (const QUuid& uuid : _duplicatedUuids)
    {
        _glWidget->restoreFromRecycleBin(uuid);
    }

    // Update view and UI FIRST (this rebuilds the list)
    _glWidget->updateView();
    _viewer->updateDisplayList();

    // THEN auto-select the restored duplicates (after list is stable)
    QSet<QUuid> duplicateSet(_duplicatedUuids.begin(), _duplicatedUuids.end());
    _viewer->setSelectionWithoutUndo(duplicateSet);
}

QSet<int> DuplicateCommand::convertUuidsToIndices(const QSet<QUuid>& uuids)
{
    QSet<int> indices;

    for (const QUuid& uuid : uuids)
    {
        int index = _glWidget->getIndexByUuid(uuid);
        if (index >= 0)
        {
            indices.insert(index);
        }
    }

    return indices;
}
