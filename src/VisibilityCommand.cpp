#include "VisibilityCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

VisibilityCommand::VisibilityCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QSet<QUuid>& newVisibleUuids,
    const QString& text)
    : ModelViewerCommand(viewer, glWidget, text)
    , m_newVisibleUuids(newVisibleUuids)
{
    // Capture the current visibility state before the change
    m_oldVisibleUuids = m_viewer->getVisibleUuids();
}

void VisibilityCommand::undo()
{
    applyVisibility(m_oldVisibleUuids);
}

void VisibilityCommand::redo()
{
    applyVisibility(m_newVisibleUuids);
}

void VisibilityCommand::applyVisibility(const QSet<QUuid>& visibleUuids)
{
    if (!m_viewer || !m_glWidget)
        return;

    // Use the non-undo version to prevent recursion
    m_viewer->setVisibilityWithoutUndo(visibleUuids);
}
