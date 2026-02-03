#include "VisibilityCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

VisibilityCommand::VisibilityCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QSet<QUuid>& newVisibleUuids,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _newVisibleUuids(newVisibleUuids)
{
    // Capture the current visibility state before the change
    _oldVisibleUuids = _viewer->getVisibleUuids();
}

void VisibilityCommand::undo()
{
    applyVisibility(_oldVisibleUuids);
}

void VisibilityCommand::redo()
{
    applyVisibility(_newVisibleUuids);
}

void VisibilityCommand::applyVisibility(const QSet<QUuid>& visibleUuids)
{
    if (!_viewer || !_glWidget)
        return;

    // Use the non-undo version to prevent recursion
    _viewer->setVisibilityWithoutUndo(visibleUuids);
}
