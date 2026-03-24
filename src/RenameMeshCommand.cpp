#include "RenameMeshCommand.h"
#include "GLWidget.h"
#include "SceneTreeWidget.h"
#include "TriangleMesh.h"

// ---------------------------------------------------------------------------

RenameMeshCommand::RenameMeshCommand(ModelViewer*     viewer,
                                     GLWidget*        glWidget,
                                     SceneTreeWidget* treeWidget,
                                     const QUuid&     uuid,
                                     const QString&   oldName,
                                     const QString&   newName,
                                     const QString&   text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _treeWidget(treeWidget)
    , _uuid(uuid)
    , _oldName(oldName)
    , _newName(newName)
{
}

void RenameMeshCommand::redo()
{
    applyName(_newName);
}

void RenameMeshCommand::undo()
{
    applyName(_oldName);
}

void RenameMeshCommand::applyName(const QString& name)
{
    TriangleMesh* mesh = _glWidget->getMeshByUuid(_uuid);
    if (!mesh) return;

    mesh->setName(name);

    if (_treeWidget)
        _treeWidget->updateMeshName(_uuid, name);
}
