#pragma once

#include <QHash>
#include <QSet>
#include <QTreeWidget>
#include <QUuid>
#include <vector>

class SceneGraph;
class SceneNode;
class GLWidget;

// ---------------------------------------------------------------------------
// SceneTreeWidget
//
// A QTreeWidget that mirrors the SceneGraph hierarchy.  Replaces the old
// flat ModelObjectList (QListWidget).
//
// Node types shown (icons from Qt resources):
//   document-root.png  Synthetic file-level node (one per imported file)
//   assembly.png       Assembly node (has child SceneNodes)
//   mesh.png           Mesh leaf (one per TriangleMesh UUID in a SceneNode)
//
// Assembly / file items carry tri-state check boxes that control all leaves
// beneath them.  Clicking an assembly item selects all mesh leaves below it.
// Empty assembly nodes (all children deleted) are hidden from the tree but
// kept alive in SceneGraph so undo can restore them without reconstruction.
//
// Thread safety: all public methods must be called from the UI thread.
// ---------------------------------------------------------------------------
class SceneTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    // Data roles stored on QTreeWidgetItem instances
    enum ItemDataRole
    {
        MeshUuidRole    = Qt::UserRole,       // QUuid  — valid only on mesh leaf items
        IsLeafRole      = Qt::UserRole + 1,   // bool   — true if this is a mesh leaf
        PureNameRole    = Qt::UserRole + 2,   // QString— plain mesh name for the editor
        IsSyntheticRole = Qt::UserRole + 3,   // bool   — true on file-level (synthetic) nodes
    };

    explicit SceneTreeWidget(QWidget* parent = nullptr);

    void setSceneGraph(SceneGraph* sg);
    void setGLWidget(GLWidget* gl);

    // -----------------------------------------------------------------------
    // Selection
    // -----------------------------------------------------------------------

    /** Returns UUIDs of all currently selected mesh-leaf items. */
    QList<QUuid> selectedMeshUuids() const;

    /** True if at least one mesh leaf is selected. */
    bool hasMeshSelection() const;

    /** Total number of mesh leaf items currently in the tree. */
    int meshCount() const;

    /**
     * Set selection from a set of mesh-store indices (converted via GLWidget).
     * Does NOT emit selectionUpdated().
     */
    void setSelectionByIndices(const QSet<int>& indices);

    /**
     * Set selection from a set of mesh UUIDs.
     * Does NOT emit selectionUpdated().
     */
    void setSelectionByUuids(const QSet<QUuid>& uuids);

    /** Returns mesh-store indices for all selected mesh leaves. */
    std::vector<int> getSelectedIndices() const;

    /** UUID of the current (last-clicked) item, or null if not a mesh leaf. */
    QUuid currentMeshUuid() const;

    /** Clear the selection without emitting selectionUpdated(). */
    void clearMeshSelection();

    // -----------------------------------------------------------------------
    // Visibility (check state)
    // -----------------------------------------------------------------------

    /**
     * Update check states from the set of currently-visible mesh UUIDs.
     * Does NOT emit meshVisibilityChanged().
     */
    void setVisibilityByUuids(const QSet<QUuid>& visibleUuids);

    /** Returns UUIDs of all checked (visible) mesh leaves. */
    QSet<QUuid> getVisibleUuids() const;

    /** Returns mesh-store indices of all checked (visible) mesh leaves. */
    std::vector<int> getVisibleIndices() const;

    /** Count of checked (visible) mesh leaves. */
    int visibleMeshCount() const;

    // -----------------------------------------------------------------------
    // Filter / search
    // -----------------------------------------------------------------------

    /**
     * Filter the tree by item text (assembly + mesh names, with fuzzy fallback).
     * Matching branches stay visible; if an assembly matches, its descendant
     * mesh leaves become the effective selection. An empty filter restores all
     * items.
     * Emits selectionUpdated() after updating the selection.
     */
    void filterItems(const QString& filter);

    // -----------------------------------------------------------------------
    // Tree expand / collapse helpers (used by context menu)
    // -----------------------------------------------------------------------

    /** Expand item one level: shows direct children; grandchildren untouched. */
    // (use the inherited QTreeWidget::expandItem() directly for this)

    /**
     * Expand item one level strictly: shows direct children but ensures
     * those children are collapsed so their subtrees stay hidden.
     * (Plain expandItem() shows previously-expanded grandchildren too.)
     */
    void expandOneLevel(QTreeWidgetItem* item);

    /** Expand item AND all of its descendants recursively. */
    void expandSubtree(QTreeWidgetItem* item);

    /**
     * Collapse one level: collapse each DIRECT CHILD of item so their
     * contents are hidden, but item itself stays expanded (children remain
     * visible and can be individually re-opened).  Deeper levels untouched.
     */
    void collapseOneLevel(QTreeWidgetItem* item);

    /**
     * Collapse all below: deeply collapse every descendant subtree so that
     * re-opening any child shows it in a fully-collapsed state.  Item itself
     * stays expanded so its direct children remain visible.
     */
    void collapseAllBelow(QTreeWidgetItem* item);

    // -----------------------------------------------------------------------
    // Misc
    // -----------------------------------------------------------------------

    /** Map a local widget position to global screen coordinates. */
    QPoint mapMenuToGlobal(const QPoint& localPos) const;

public slots:
    /**
     * Fully rebuild the tree from the current SceneGraph state.
     * Automatically called when SceneGraph::structureChanged() fires.
     * Preserves existing visibility and selection state where possible.
     * Assembly nodes whose entire subtree has been deleted are hidden.
     */
    void rebuild();

    /**
     * Update the display name of a single mesh leaf without triggering a
     * full rebuild.  Used by RenameMeshCommand redo/undo to apply or
     * revert a rename efficiently.
     */
    void updateMeshName(const QUuid& uuid, const QString& name);

signals:
    /** Emitted when the user changes the selection (not programmatic). */
    void selectionUpdated();

    /** Emitted when any leaf check-state (visibility) changes. */
    void meshVisibilityChanged();

    /** Emitted when a mesh leaf is successfully renamed by the user. */
    void meshRenamed(const QUuid& uuid, const QString& newName);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemSelectionChanged();

private:
    // Recursive tree builder — skips nodes with no mesh descendants
    void buildSubtree(QTreeWidgetItem* parentItem, const SceneNode* node);

    // Returns true if node has at least one mesh UUID anywhere in its subtree
    bool nodeHasMeshes(const SceneNode* node) const;

    // Item factories
    QTreeWidgetItem* makeMeshLeaf(const QUuid& uuid);
    QTreeWidgetItem* makeAssemblyItem(const SceneNode* node);

    // Tri-state helpers
    void propagateCheckDown(QTreeWidgetItem* item, Qt::CheckState state);
    void refreshCheckUpward(QTreeWidgetItem* item);

    // Enumerate all leaf items (DFS)
    void collectLeaves(QTreeWidgetItem* subtree,
                       QList<QTreeWidgetItem*>& out) const;
    void collectAllLeaves(QList<QTreeWidgetItem*>& out) const;

    // Internal helper: collapse item and all its descendants
    void collapseSubtreeHelper(QTreeWidgetItem* item);

    // Bulk assembly tristate + icon refresh (used after show/hide all)
    void refreshAllAssemblyStates();
    void refreshAssemblyBottomUp(QTreeWidgetItem* item);

    // Sync the item's icon to its current check-state
    void updateItemIcon(QTreeWidgetItem* item);

    // Levenshtein distance for fuzzy name matching
    int levenshteinDistance(const QString& s1, const QString& s2) const;

    // Search / filter helpers
    QString itemSearchText(QTreeWidgetItem* item) const;
    QString itemPathSearchText(QTreeWidgetItem* item) const;
    QStringList searchTerms(const QString& text) const;
    int textMatchRank(QTreeWidgetItem* item, const QString& lowerFilter) const;
    int fuzzyThreshold(const QString& lowerFilter) const;
    void showSubtree(QTreeWidgetItem* item);
    void selectSearchMatch(QTreeWidgetItem* item);
    void selectMatchesAtRank(QTreeWidgetItem* item,
                             const QString& lowerFilter,
                             int bestRank);
    bool applySubstringFilter(QTreeWidgetItem* item,
                              const QString& lowerFilter,
                              bool ancestorMatched,
                              bool& anyMatch,
                              int& bestRank);
    QTreeWidgetItem* findBestFuzzyMatch(QTreeWidgetItem* item,
                                        const QString& lowerFilter,
                                        int& bestScore) const;

    SceneGraph* _sceneGraph = nullptr;
    GLWidget*   _glWidget   = nullptr;

    // O(1) lookup from mesh UUID to its leaf QTreeWidgetItem
    QHash<QUuid, QTreeWidgetItem*> _uuidToLeaf;

    bool _updatingTree = false; // suppress re-entrant signal handling
    bool _inRename     = false; // suppress itemChanged during delegate setModelData
};
