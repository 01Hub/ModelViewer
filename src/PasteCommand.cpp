#include "PasteCommand.h"
#include "ModelViewer.h"
#include "GLWidget.h"
#include "SceneGraph.h"

PasteCommand::PasteCommand(ModelViewer*             viewer,
                           GLWidget*                glWidget,
                           const QList<PastedItem>& items,
                           const QSet<QUuid>&       originalSelection,
                           const QString&           text)
    : ModelViewerCommand(viewer, glWidget, text)
    , _items(items)
    , _originalSelection(originalSelection)
    , _firstRedo(true)
    , _inserted(true)
{
}

PasteCommand::~PasteCommand()
{
    // If the command is destroyed while undone, items are not in the scene:
    // free detached subtree nodes and permanently remove meshes from the bin.
    if (!_inserted)
    {
        for (const PastedItem& item : _items)
        {
            if (item.type == PastedItem::Subtree)
            {
                if (_glWidget)
                {
                    for (const QUuid& uuid : item.subtreeMeshUuids)
                        _glWidget->permanentlyDeleteFromBin(uuid);
                }
                freeSubtree(item.subtreeRoot);
            }
            else // Mesh
            {
                if (_glWidget)
                    _glWidget->permanentlyDeleteFromBin(item.meshUuid);
            }
        }
    }
}

void PasteCommand::undo()
{
    if (!_viewer || !_glWidget)
        return;

    SceneGraph* sg = _viewer->sceneGraph();

    // Process items in reverse insertion order to keep positions valid.
    for (int i = _items.size() - 1; i >= 0; --i)
    {
        const PastedItem& item = _items[i];

        if (item.type == PastedItem::Mesh)
        {
            int idx = _glWidget->getIndexByUuid(item.meshUuid);
            if (idx >= 0)
                _glWidget->moveToRecycleBin(item.meshUuid, idx);

            int pos = 0;
            sg->removeMeshUuid(item.meshUuid, pos);
        }
        else // Subtree
        {
            // Move all meshes to recycle bin before detaching the subtree
            // so GLWidget is clean before SceneGraph emits structureChanged.
            for (const QUuid& uuid : item.subtreeMeshUuids)
            {
                int idx = _glWidget->getIndexByUuid(uuid);
                if (idx >= 0)
                    _glWidget->moveToRecycleBin(uuid, idx);
            }

            int pos = 0;
            sg->removeChildNode(item.subtreeParent, item.subtreeRoot, pos);
        }
    }

    _inserted = false;

    _glWidget->updateView();
    _viewer->updateDisplayList();
    _viewer->setSelectionWithoutUndo(_originalSelection);
}

void PasteCommand::redo()
{
    if (!_viewer || !_glWidget)
        return;

    if (_firstRedo)
    {
        // Items are already in the scene from the initial paste — just select.
        _firstRedo = false;
        QSet<QUuid> pastedSet;
        for (const PastedItem& item : _items)
        {
            if (item.type == PastedItem::Mesh)
                pastedSet.insert(item.meshUuid);
            else
                pastedSet += QSet<QUuid>(item.subtreeMeshUuids.begin(),
                                         item.subtreeMeshUuids.end());
        }
        _viewer->setSelectionWithoutUndo(pastedSet);
        return;
    }

    SceneGraph* sg = _viewer->sceneGraph();

    for (const PastedItem& item : _items)
    {
        if (item.type == PastedItem::Mesh)
        {
            _glWidget->restoreFromRecycleBin(item.meshUuid);
            sg->restoreMeshUuid(item.ownerNode, item.meshUuid, item.meshPosition);
        }
        else // Subtree
        {
            for (const QUuid& uuid : item.subtreeMeshUuids)
                _glWidget->restoreFromRecycleBin(uuid);

            sg->insertChildNode(item.subtreeParent, item.subtreeRoot,
                                item.childPosition);
        }
    }

    _inserted = true;

    _glWidget->updateView();
    _viewer->updateDisplayList();

    QSet<QUuid> pastedSet;
    for (const PastedItem& item : _items)
    {
        if (item.type == PastedItem::Mesh)
            pastedSet.insert(item.meshUuid);
        else
            pastedSet += QSet<QUuid>(item.subtreeMeshUuids.begin(),
                                     item.subtreeMeshUuids.end());
    }
    _viewer->setSelectionWithoutUndo(pastedSet);
}

QSet<QUuid> PasteCommand::getReferencedUuids() const
{
    QSet<QUuid> result;
    for (const PastedItem& item : _items)
    {
        if (item.type == PastedItem::Mesh)
            result.insert(item.meshUuid);
        else
            result += QSet<QUuid>(item.subtreeMeshUuids.begin(),
                                   item.subtreeMeshUuids.end());
    }
    return result;
}

void PasteCommand::freeSubtree(SceneNode* root)
{
    if (!root)
        return;
    for (SceneNode* child : root->children)
        freeSubtree(child);
    delete root;
}
