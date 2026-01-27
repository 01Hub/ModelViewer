#include "ModelViewerCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"

ModelViewerCommand::ModelViewerCommand(ModelViewer* viewer,
    GLWidget* glWidget,
    const QString& text)
    : QUndoCommand(text)
    , m_viewer(viewer)
    , m_glWidget(glWidget)
{
}
