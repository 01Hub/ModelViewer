#include "CutCommand.h"
#include "ModelViewer.h"

CutCommand::CutCommand(ModelViewer*                 viewer,
                       GLWidget*                    glWidget,
                       const QList<ClipboardEntry>& entries,
                       const QSet<QUuid>&           cutMeshUuids,
                       const QSet<QUuid>&           cutNodeUuids,
                       const QString&               text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _entries(entries)
    , _cutMeshUuids(cutMeshUuids)
    , _cutNodeUuids(cutNodeUuids)
    , _firstRedo(true)
{
}

void CutCommand::undo()
{
    if (!_viewer) return;
    _viewer->clearCutMarks();
}

void CutCommand::redo()
{
    if (!_viewer) return;
    if (_firstRedo) { _firstRedo = false; return; }
    _viewer->reapplyCutMarks(_entries, _cutMeshUuids, _cutNodeUuids);
}
