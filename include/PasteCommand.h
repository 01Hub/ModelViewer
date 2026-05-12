#pragma once

#include "ModelViewerCommand.h"
#include "SceneNode.h"

#include <QList>
#include <QSet>
#include <QUuid>

// ---------------------------------------------------------------------------
// PasteCommand
//
// Undoable command for the Paste operation.  The caller (ModelViewer) performs
// the actual cloning and insertion BEFORE constructing this command; the
// command's job is to undo and redo that work.
//
// Two kinds of pasted items are tracked:
//
//   PastedItem::Mesh    — a single mesh UUID inserted into a node's meshUuids.
//   PastedItem::Subtree — a cloned SceneNode subtree attached as a child node.
//
// Ownership of Subtree roots:
//   Inserted  → SceneGraph owns the subtree.
//   Removed   → PasteCommand owns the subtree (stored in this object).
//   On destroy in removed state → PasteCommand frees the subtree.
// ---------------------------------------------------------------------------
class PasteCommand : public ModelViewerCommand
{
public:
    struct PastedItem
    {
        enum Type { Mesh, Subtree };
        Type type = Mesh;

        // --- Mesh fields ---
        QUuid      meshUuid;
        SceneNode* ownerNode   = nullptr;
        int        meshPosition = 0;

        // --- Subtree fields ---
        SceneNode*   subtreeRoot   = nullptr;
        SceneNode*   subtreeParent = nullptr;
        int          childPosition = 0;
        QList<QUuid> subtreeMeshUuids;  // all mesh UUIDs in the subtree
    };

    PasteCommand(ModelViewer*           viewer,
                 GLWidget*              glWidget,
                 const QList<PastedItem>& items,
                 const QSet<QUuid>&     originalSelection,
                 const QString&         text = QObject::tr("Paste"));

    ~PasteCommand() override;

    void undo() override;
    void redo() override;

    int id() const override { return 8; }

    QSet<QUuid> getReferencedUuids() const;

private:
    QList<PastedItem> _items;
    QSet<QUuid>       _originalSelection;
    bool              _firstRedo;
    bool              _inserted;        // true when items are in the scene

    static void freeSubtree(SceneNode* root);
};
