#include "SceneTreeWidget.h"
#include "SceneGraph.h"
#include "SceneNode.h"
#include "GLWidget.h"
#include "TriangleMesh.h"

#include <QApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QProxyStyle>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <limits>

// ---------------------------------------------------------------------------
// Icon helpers
// ---------------------------------------------------------------------------

static QPixmap makeGreyscale(const QPixmap& src)
{
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y)
    {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
        {
            const int g = qGray(line[x]);
            line[x] = qRgba(g, g, g, qAlpha(line[x]));
        }
    }
    return QPixmap::fromImage(img);
}

// Lazy-loaded once (after QApplication exists).
// To swap icon sets, change the resource paths here.
struct TreeIcons
{
    QPixmap fileNormal,     fileGrey;
    QPixmap assemblyNormal, assemblyGrey;
    QPixmap meshNormal,     meshGrey;

    TreeIcons()
    {
        QPixmap f(":/icons/res/document-root.png");
        QPixmap a(":/icons/res/assembly.png");
        QPixmap m(":/icons/res/mesh.png");
        fileNormal     = f; fileGrey     = makeGreyscale(f);
        assemblyNormal = a; assemblyGrey = makeGreyscale(a);
        meshNormal     = m; meshGrey     = makeGreyscale(m);
    }
};

static const TreeIcons& treeIcons()
{
    static TreeIcons inst;
    return inst;
}

// ---------------------------------------------------------------------------
// PlusMinusStyle
//
// Replaces the platform arrow branch indicator with a classic +/- box.
// ---------------------------------------------------------------------------
class PlusMinusStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    void drawPrimitive(PrimitiveElement    pe,
                       const QStyleOption* opt,
                       QPainter*           p,
                       const QWidget*      w) const override
    {
        if (pe == PE_IndicatorBranch && (opt->state & State_Children))
        {
            const int sz = 11;
            QRect box = QRect(opt->rect.center() - QPoint(sz / 2, sz / 2),
                              QSize(sz, sz));
            p->save();
            p->setPen(QPen(opt->palette.text().color(), 1));
            p->setBrush(opt->palette.base());
            p->drawRect(box);
            // Horizontal bar
            p->drawLine(box.left() + 2,  box.center().y(),
                        box.right() - 2, box.center().y());
            // Vertical bar only when closed (+)
            if (!(opt->state & State_Open))
                p->drawLine(box.center().x(), box.top() + 2,
                            box.center().x(), box.bottom() - 2);
            p->restore();
            return;
        }
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
};

// ---------------------------------------------------------------------------
// SceneTreeWidget
// ---------------------------------------------------------------------------

SceneTreeWidget::SceneTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    // By default QTreeWidget stretches the last section to fill the viewport,
    // which overrides ResizeToContents and prevents the horizontal scrollbar
    // from ever appearing.  Disable stretch first, then let the column size
    // itself to its content so overflow triggers the scrollbar.
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setEditTriggers(QAbstractItemView::DoubleClicked
                  | QAbstractItemView::EditKeyPressed);
    setAnimated(true);
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setStyle(new PlusMinusStyle(style()));              

    // --- item changed (check-state or text) ----------------------------------
    connect(this, &QTreeWidget::itemChanged,
            this, &SceneTreeWidget::onItemChanged);

    // --- selection changed ---------------------------------------------------
    connect(this, &QTreeWidget::itemSelectionChanged,
            this, &SceneTreeWidget::onItemSelectionChanged);

    // --- rename detection via delegate close-editor --------------------------
    connect(itemDelegate(), &QAbstractItemDelegate::commitData,
            this, [this](QWidget*) {
                _inRename = true;   // next itemChanged is from the delegate
            });

    connect(itemDelegate(), &QAbstractItemDelegate::closeEditor,
            this, [this](QWidget* editor, QAbstractItemDelegate::EndEditHint) {
                _inRename = false;

                QTreeWidgetItem* cur = currentItem();
                if (!cur || !cur->data(0, IsLeafRole).toBool()) return;

                auto* le = qobject_cast<QLineEdit*>(editor);
                if (!le) return;

                const QString newName = le->text().trimmed();
                if (newName.isEmpty()) return;

                const QUuid uuid = cur->data(0, MeshUuidRole).value<QUuid>();
                emit meshRenamed(uuid, newName);
            });
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

void SceneTreeWidget::setSceneGraph(SceneGraph* sg)
{
    if (_sceneGraph)
        disconnect(_sceneGraph, &SceneGraph::structureChanged,
                   this,        &SceneTreeWidget::rebuild);

    _sceneGraph = sg;

    if (_sceneGraph)
        connect(_sceneGraph, &SceneGraph::structureChanged,
                this,        &SceneTreeWidget::rebuild);
}

void SceneTreeWidget::setGLWidget(GLWidget* gl)
{
    _glWidget = gl;
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

QList<QUuid> SceneTreeWidget::selectedMeshUuids() const
{
    QList<QUuid> result;
    for (QTreeWidgetItem* item : selectedItems())
    {
        if (item->data(0, IsLeafRole).toBool())
            result << item->data(0, MeshUuidRole).value<QUuid>();
    }
    return result;
}

bool SceneTreeWidget::hasMeshSelection() const
{
    for (QTreeWidgetItem* item : selectedItems())
    {
        if (item->data(0, IsLeafRole).toBool())
            return true;
    }
    return false;
}

int SceneTreeWidget::meshCount() const
{
    return _uuidToLeaf.size();
}

void SceneTreeWidget::setSelectionByIndices(const QSet<int>& indices)
{
    if (!_glWidget) return;
    QSet<QUuid> uuids;
    for (int idx : indices)
    {
        QUuid u = _glWidget->getUuidByIndex(idx);
        if (!u.isNull()) uuids.insert(u);
    }
    setSelectionByUuids(uuids);
}

void SceneTreeWidget::setSelectionByUuids(const QSet<QUuid>& uuids)
{
    _updatingTree = true;
    blockSignals(true);
    clearSelection();
    for (const QUuid& uuid : uuids)
    {
        auto it = _uuidToLeaf.find(uuid);
        if (it != _uuidToLeaf.end())
            it.value()->setSelected(true);
    }
    blockSignals(false);
    _updatingTree = false;
}

std::vector<int> SceneTreeWidget::getSelectedIndices() const
{
    std::vector<int> ids;
    if (!_glWidget) return ids;
    for (const QUuid& uuid : selectedMeshUuids())
    {
        int idx = _glWidget->getIndexByUuid(uuid);
        if (idx >= 0) ids.push_back(idx);
    }
    return ids;
}

QUuid SceneTreeWidget::currentMeshUuid() const
{
    QTreeWidgetItem* cur = currentItem();
    if (!cur || !cur->data(0, IsLeafRole).toBool())
        return QUuid{};
    return cur->data(0, MeshUuidRole).value<QUuid>();
}

void SceneTreeWidget::clearMeshSelection()
{
    _updatingTree = true;
    blockSignals(true);
    clearSelection();
    blockSignals(false);
    _updatingTree = false;
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

void SceneTreeWidget::setVisibilityByUuids(const QSet<QUuid>& visibleUuids)
{
    _updatingTree = true;
    blockSignals(true);

    for (auto it = _uuidToLeaf.begin(); it != _uuidToLeaf.end(); ++it)
    {
        Qt::CheckState cs = visibleUuids.contains(it.key())
                            ? Qt::Checked : Qt::Unchecked;
        it.value()->setCheckState(0, cs);
        updateItemIcon(it.value());
    }

    // Update tristate and icons on ALL assembly nodes (bottom-up),
    // not just top-level items, so intermediate nodes are also correct
    refreshAllAssemblyStates();

    blockSignals(false);
    _updatingTree = false;
}

QSet<QUuid> SceneTreeWidget::getVisibleUuids() const
{
    QSet<QUuid> result;
    for (auto it = _uuidToLeaf.constBegin(); it != _uuidToLeaf.constEnd(); ++it)
    {
        if (it.value()->checkState(0) == Qt::Checked)
            result.insert(it.key());
    }
    return result;
}

std::vector<int> SceneTreeWidget::getVisibleIndices() const
{
    std::vector<int> ids;
    if (!_glWidget) return ids;
    for (auto it = _uuidToLeaf.constBegin(); it != _uuidToLeaf.constEnd(); ++it)
    {
        if (it.value()->checkState(0) == Qt::Checked)
        {
            int idx = _glWidget->getIndexByUuid(it.key());
            if (idx >= 0) ids.push_back(idx);
        }
    }
    // setDisplayList uses std::set_difference which requires a sorted range
    std::sort(ids.begin(), ids.end());
    return ids;
}

int SceneTreeWidget::visibleMeshCount() const
{
    int count = 0;
    for (auto it = _uuidToLeaf.constBegin(); it != _uuidToLeaf.constEnd(); ++it)
        if (it.value()->checkState(0) == Qt::Checked) ++count;
    return count;
}

// ---------------------------------------------------------------------------
// Filter
// ---------------------------------------------------------------------------

void SceneTreeWidget::filterItems(const QString& filter)
{
    _updatingTree = true;
    blockSignals(true);
    clearSelection();

    if (filter.isEmpty())
    {
        for (int i = 0; i < topLevelItemCount(); ++i)
            showSubtree(topLevelItem(i));
        blockSignals(false);
        _updatingTree = false;
        return;
    }

    const QString lowerFilter = filter.toLower();
    bool anySubstringMatch = false;
    for (int i = 0; i < topLevelItemCount(); ++i)
        applySubstringFilter(topLevelItem(i), lowerFilter, false, anySubstringMatch);

    if (!anySubstringMatch)
    {
        QTreeWidgetItem* bestItem = nullptr;
        int bestScore = std::numeric_limits<int>::max();

        for (int i = 0; i < topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* candidate =
                findBestFuzzyMatch(topLevelItem(i), lowerFilter, bestScore);
            if (candidate)
                bestItem = candidate;
        }

        if (bestItem && bestScore <= 3)
        {
            for (QTreeWidgetItem* p = bestItem->parent(); p; p = p->parent())
            {
                p->setHidden(false);
                p->setExpanded(true);
            }
            if (bestItem->data(0, IsLeafRole).toBool())
            {
                bestItem->setHidden(false);
            }
            else
            {
                showSubtree(bestItem);
            }
            selectSearchMatch(bestItem);
        }
    }

    blockSignals(false);
    _updatingTree = false;
    emit selectionUpdated();
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

QPoint SceneTreeWidget::mapMenuToGlobal(const QPoint& localPos) const
{
    return mapToGlobal(localPos);
}

// ---------------------------------------------------------------------------
// rebuild
// ---------------------------------------------------------------------------

void SceneTreeWidget::rebuild()
{
    if (!_sceneGraph || !_glWidget) return;

    _updatingTree = true;
    blockSignals(true);

    // Preserve current state before wipe
    QSet<QUuid> savedVisible;
    QSet<QUuid> savedSelected;
    QSet<QUuid> oldUuids;          // every UUID known before the rebuild
    const bool hadItems = !_uuidToLeaf.isEmpty();

    for (auto it = _uuidToLeaf.constBegin(); it != _uuidToLeaf.constEnd(); ++it)
    {
        oldUuids.insert(it.key());
        if (it.value()->checkState(0) == Qt::Checked)
            savedVisible.insert(it.key());
        if (it.value()->isSelected())
            savedSelected.insert(it.key());
    }

    // Full wipe
    clear();
    _uuidToLeaf.clear();

    // Rebuild from SceneGraph
    SceneNode* root = _sceneGraph->root();
    for (SceneNode* fileNode : root->children)
    {
        // Skip file nodes that have no meshes left anywhere beneath them
        if (!nodeHasMeshes(fileNode)) continue;

        QTreeWidgetItem* fileItem = makeAssemblyItem(fileNode);
        addTopLevelItem(fileItem);

        // Direct mesh UUIDs on the file node itself (unusual but possible)
        for (const QUuid& uuid : fileNode->meshUuids)
            fileItem->addChild(makeMeshLeaf(uuid));

        // Child assembly nodes (each skips empty subtrees internally)
        for (SceneNode* child : fileNode->children)
            buildSubtree(fileItem, child);

        fileItem->setExpanded(true);
    }

    // ---- Restore visibility ------------------------------------------------
    if (!hadItems)
    {
        // Fresh load — read from GLWidget's display list
        const std::vector<int> displayedIds = _glWidget->getDisplayedObjectsIds();
        for (int idx : displayedIds)
        {
            QUuid uuid = _glWidget->getUuidByIndex(idx);
            if (!uuid.isNull()) savedVisible.insert(uuid);
        }
        // If GLWidget has no display list yet, make everything visible
        if (savedVisible.isEmpty())
        {
            for (auto it = _uuidToLeaf.constBegin(); it != _uuidToLeaf.constEnd(); ++it)
                savedVisible.insert(it.key());
        }
    }

    for (auto it = _uuidToLeaf.begin(); it != _uuidToLeaf.end(); ++it)
    {
        // UUIDs not seen before this rebuild are newly imported — show them
        const bool isNew = !oldUuids.contains(it.key());
        const bool visible = isNew || savedVisible.contains(it.key());
        it.value()->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
        updateItemIcon(it.value());
    }
    refreshAllAssemblyStates();

    // ---- Restore selection --------------------------------------------------
    for (const QUuid& uuid : savedSelected)
    {
        auto it = _uuidToLeaf.find(uuid);
        if (it != _uuidToLeaf.end())
            it.value()->setSelected(true);
    }

    blockSignals(false);
    _updatingTree = false;
}

// ---------------------------------------------------------------------------
// Key press
// ---------------------------------------------------------------------------

void SceneTreeWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space)
    {
        // Toggle visibility of all selected mesh leaves
        QList<QTreeWidgetItem*> sel = selectedItems();
        if (!sel.isEmpty())
        {
            _updatingTree = true;
            blockSignals(true);
            for (QTreeWidgetItem* item : sel)
            {
                if (!item->data(0, IsLeafRole).toBool()) continue;
                Qt::CheckState newState =
                    (item->checkState(0) == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
                item->setCheckState(0, newState);
                updateItemIcon(item);
                refreshCheckUpward(item->parent());
            }
            blockSignals(false);
            _updatingTree = false;
            emit meshVisibilityChanged();
        }
    }
    else
    {
        QTreeWidget::keyPressEvent(event);
    }
}

// ---------------------------------------------------------------------------
// Mouse press — toggle-deselect on re-click
// ---------------------------------------------------------------------------

void SceneTreeWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton &&
        !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
    {
        QTreeWidgetItem* clicked = itemAt(event->pos());
        if (clicked)
        {
            const bool isLeaf = clicked->data(0, IsLeafRole).toBool();

            if (isLeaf)
            {
                // Leaf: re-clicking the sole selected item deselects it.
                // Snapshot state that only changes on checkbox / branch clicks;
                // if unchanged after base handles the event it was a label click.
                if (clicked->isSelected() && selectedItems().count() == 1)
                {
                    const Qt::CheckState csBefore       = clicked->checkState(0);
                    const bool           expandedBefore = clicked->isExpanded();
                    QTreeWidget::mousePressEvent(event);
                    if (clicked->checkState(0) == csBefore &&
                        clicked->isExpanded()  == expandedBefore)
                        clearSelection();
                    return;
                }
            }
            else
            {
                // Assembly: if ALL its leaves are currently selected,
                // re-clicking the assembly deselects everything (toggle off).
                QList<QTreeWidgetItem*> leaves;
                collectLeaves(clicked, leaves);
                if (!leaves.isEmpty())
                {
                    bool allSelected = true;
                    for (QTreeWidgetItem* leaf : leaves)
                        if (!leaf->isSelected()) { allSelected = false; break; }

                    if (allSelected)
                    {
                        clearSelection();
                        event->accept();
                        return;
                    }
                }
            }
        }
    }
    QTreeWidget::mousePressEvent(event);
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void SceneTreeWidget::onItemChanged(QTreeWidgetItem* item, int /*column*/)
{
    if (_updatingTree || _inRename || !item) return;

    const bool isLeaf = item->data(0, IsLeafRole).toBool();

    if (isLeaf)
    {
        // Leaf check-state changed → update icon, refresh parent tristate.
        // _updatingTree must be set BEFORE updateItemIcon so that the setIcon()
        // call inside it does not re-fire onItemChanged (infinite loop).
        _updatingTree = true;
        blockSignals(true);
        updateItemIcon(item);
        refreshCheckUpward(item->parent());
        blockSignals(false);
        _updatingTree = false;
        emit meshVisibilityChanged();
    }
    else
    {
        // Assembly / file node check-state → propagate to all descendant leaves,
        // then refresh all ancestors' tristate states.
        Qt::CheckState cs = item->checkState(0);
        if (cs == Qt::PartiallyChecked) return;  // partial is set programmatically only

        _updatingTree = true;
        blockSignals(true);
        propagateCheckDown(item, cs);
        updateItemIcon(item);               // assembly's own icon after propagation
        refreshCheckUpward(item->parent()); // update ancestors
        blockSignals(false);
        _updatingTree = false;
        emit meshVisibilityChanged();
    }
}

void SceneTreeWidget::onItemSelectionChanged()
{
    if (_updatingTree) return;

    // If any assembly (non-leaf) items are selected, expand the selection to
    // cover all mesh leaves beneath them, then deselect the assembly items.
    bool hadAssembly = false;
    QSet<QUuid> toSelect;

    for (QTreeWidgetItem* item : selectedItems())
    {
        if (item->data(0, IsLeafRole).toBool())
        {
            toSelect.insert(item->data(0, MeshUuidRole).value<QUuid>());
        }
        else
        {
            hadAssembly = true;
            QList<QTreeWidgetItem*> leaves;
            collectLeaves(item, leaves);
            for (QTreeWidgetItem* leaf : leaves)
                toSelect.insert(leaf->data(0, MeshUuidRole).value<QUuid>());
        }
    }

    if (hadAssembly)
    {
        // Replace with a pure leaf selection (suppressed — no recursive signal)
        _updatingTree = true;
        blockSignals(true);
        clearSelection();
        for (const QUuid& uuid : std::as_const(toSelect))
        {
            auto it = _uuidToLeaf.find(uuid);
            if (it != _uuidToLeaf.end())
                it.value()->setSelected(true);
        }
        blockSignals(false);
        _updatingTree = false;
    }

    emit selectionUpdated();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

bool SceneTreeWidget::nodeHasMeshes(const SceneNode* node) const
{
    if (!node->meshUuids.isEmpty()) return true;
    for (const SceneNode* child : node->children)
        if (nodeHasMeshes(child)) return true;
    return false;
}

void SceneTreeWidget::buildSubtree(QTreeWidgetItem* parentItem,
                                   const SceneNode* node)
{
    // Skip nodes whose entire subtree has been emptied (all meshes deleted).
    // The SceneGraph keeps these nodes alive for undo support; we simply
    // omit them from the view.  They reappear when structureChanged() fires
    // after an undo that restores a mesh into them.
    if (!nodeHasMeshes(node)) return;

    // -----------------------------------------------------------------------
    // Leaf optimisation: if this node has NO child SceneNodes it is a pure
    // mesh-holder — creating a 📦 wrapper around its mesh leaves adds no
    // structural information and produces the "every mesh has a placeholder
    // parent" clutter.  Add the mesh leaves directly to the parent instead.
    // -----------------------------------------------------------------------
    if (node->children.isEmpty())
    {
        for (const QUuid& uuid : node->meshUuids)
            parentItem->addChild(makeMeshLeaf(uuid));
        return;
    }

    // Node has child nodes → create an assembly item to group them.
    QTreeWidgetItem* item = makeAssemblyItem(node);
    parentItem->addChild(item);

    // Direct mesh leaves that sit on this assembly node itself
    for (const QUuid& uuid : node->meshUuids)
        item->addChild(makeMeshLeaf(uuid));

    // Recurse into child nodes (each skips empty subtrees internally)
    for (SceneNode* child : node->children)
        buildSubtree(item, child);

    item->setExpanded(true);
}

QTreeWidgetItem* SceneTreeWidget::makeMeshLeaf(const QUuid& uuid)
{
    QString name;
    if (_glWidget)
    {
        TriangleMesh* mesh = _glWidget->getMeshByUuid(uuid);
        if (mesh) name = mesh->getName();
    }
    if (name.isEmpty())
        name = uuid.toString(QUuid::WithoutBraces).left(8);

    auto* item = new QTreeWidgetItem();
    item->setText(0, name);
    item->setIcon(0, treeIcons().meshNormal);
    item->setFlags(item->flags()
                 | Qt::ItemIsUserCheckable
                 | Qt::ItemIsEditable
                 | Qt::ItemIsSelectable);
    item->setCheckState(0, Qt::Checked);
    item->setData(0, MeshUuidRole, uuid);
    item->setData(0, IsLeafRole,   true);
    item->setData(0, PureNameRole, name);

    _uuidToLeaf[uuid] = item;
    return item;
}

QTreeWidgetItem* SceneTreeWidget::makeAssemblyItem(const SceneNode* node)
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, node->name);
    item->setIcon(0, node->isSynthetic ? treeIcons().fileNormal
                                       : treeIcons().assemblyNormal);
    item->setFlags((item->flags()
                  | Qt::ItemIsUserCheckable
                  | Qt::ItemIsSelectable)
                  & ~Qt::ItemIsEditable);
    item->setCheckState(0, Qt::Checked);
    item->setData(0, IsLeafRole,      false);
    item->setData(0, IsSyntheticRole, node->isSynthetic);
    return item;
}

void SceneTreeWidget::propagateCheckDown(QTreeWidgetItem* item,
                                         Qt::CheckState   state)
{
    for (int i = 0; i < item->childCount(); ++i)
    {
        QTreeWidgetItem* child = item->child(i);
        child->setCheckState(0, state);
        updateItemIcon(child);
        if (child->childCount() > 0)
            propagateCheckDown(child, state);
    }
}

void SceneTreeWidget::refreshCheckUpward(QTreeWidgetItem* item)
{
    if (!item) return;

    int checked = 0, unchecked = 0;
    QList<QTreeWidgetItem*> leaves;
    collectLeaves(item, leaves);
    for (QTreeWidgetItem* leaf : leaves)
    {
        if (leaf->checkState(0) == Qt::Checked) ++checked;
        else                                    ++unchecked;
    }

    Qt::CheckState cs;
    if      (checked   == 0) cs = Qt::Unchecked;
    else if (unchecked == 0) cs = Qt::Checked;
    else                     cs = Qt::PartiallyChecked;

    item->setCheckState(0, cs);
    updateItemIcon(item);
    refreshCheckUpward(item->parent());
}

// Refreshes every assembly node's check-state and icon bottom-up.
// Use this after bulk leaf-state changes where refreshCheckUpward(topLevel)
// only reaches the root, skipping intermediate assembly nodes.
void SceneTreeWidget::refreshAllAssemblyStates()
{
    for (int i = 0; i < topLevelItemCount(); ++i)
        refreshAssemblyBottomUp(topLevelItem(i));
}

void SceneTreeWidget::refreshAssemblyBottomUp(QTreeWidgetItem* item)
{
    if (!item || item->data(0, IsLeafRole).toBool()) return;
    for (int i = 0; i < item->childCount(); ++i)
        refreshAssemblyBottomUp(item->child(i));

    int checked = 0, unchecked = 0;
    QList<QTreeWidgetItem*> leaves;
    collectLeaves(item, leaves);
    for (QTreeWidgetItem* leaf : leaves)
    {
        if (leaf->checkState(0) == Qt::Checked) ++checked;
        else                                    ++unchecked;
    }
    Qt::CheckState cs;
    if      (checked   == 0) cs = Qt::Unchecked;
    else if (unchecked == 0) cs = Qt::Checked;
    else                     cs = Qt::PartiallyChecked;
    item->setCheckState(0, cs);
    updateItemIcon(item);
}

void SceneTreeWidget::collectLeaves(QTreeWidgetItem*         root,
                                    QList<QTreeWidgetItem*>& out) const
{
    if (!root) return;
    if (root->data(0, IsLeafRole).toBool())
    {
        out.append(root);
        return;
    }
    for (int i = 0; i < root->childCount(); ++i)
        collectLeaves(root->child(i), out);
}

void SceneTreeWidget::collectAllLeaves(QList<QTreeWidgetItem*>& out) const
{
    for (int i = 0; i < topLevelItemCount(); ++i)
        collectLeaves(topLevelItem(i), out);
}

void SceneTreeWidget::updateMeshName(const QUuid& uuid, const QString& name)
{
    auto it = _uuidToLeaf.find(uuid);
    if (it == _uuidToLeaf.end()) return;

    QTreeWidgetItem* item = it.value();

    // Block signals so the text change doesn't fire itemChanged and
    // trigger the rename or visibility machinery.
    _updatingTree = true;
    blockSignals(true);

    item->setText(0, name);
    item->setData(0, PureNameRole, name);

    blockSignals(false);
    _updatingTree = false;
}

int SceneTreeWidget::levenshteinDistance(const QString& s1,
                                          const QString& s2) const
{
    const int len1 = s1.size(), len2 = s2.size();
    QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));

    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;

    for (int i = 1; i <= len1; ++i)
        for (int j = 1; j <= len2; ++j)
        {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({d[i-1][j] + 1,
                                d[i][j-1] + 1,
                                d[i-1][j-1] + cost});
        }

    return d[len1][len2];
}

QString SceneTreeWidget::itemSearchText(QTreeWidgetItem* item) const
{
    if (!item) return {};

    if (item->data(0, IsLeafRole).toBool())
        return item->data(0, PureNameRole).toString().toLower();

    return item->text(0).toLower();
}

QStringList SceneTreeWidget::searchTerms(const QString& text) const
{
    QString normalized = text;

    // Split common separators and camelCase boundaries into searchable terms.
    normalized.replace(QRegularExpression("([a-z0-9])([A-Z])"), "\\1 \\2");
    normalized = normalized.toLower();
    normalized.replace(QRegularExpression("[^a-z0-9]+"), " ");

    return normalized.split(' ', Qt::SkipEmptyParts);
}

int SceneTreeWidget::textMatchRank(const QString& text, const QString& lowerFilter) const
{
    if (text.isEmpty() || lowerFilter.isEmpty())
        return 0;

    if (text == lowerFilter)
        return 4;

    const QStringList terms = searchTerms(text);
    for (const QString& term : terms)
    {
        if (term == lowerFilter)
            return 4;
    }

    for (const QString& term : terms)
    {
        if (term.startsWith(lowerFilter))
            return 3;
    }

    if (text.startsWith(lowerFilter))
        return 3;

    for (const QString& term : terms)
    {
        if (term.contains(lowerFilter))
            return 2;
    }

    if (text.contains(lowerFilter))
        return 1;

    return 0;
}

void SceneTreeWidget::showSubtree(QTreeWidgetItem* item)
{
    if (!item) return;

    item->setHidden(false);
    item->setExpanded(true);
    for (int i = 0; i < item->childCount(); ++i)
        showSubtree(item->child(i));
}

void SceneTreeWidget::selectSearchMatch(QTreeWidgetItem* item)
{
    if (!item) return;

    if (item->data(0, IsLeafRole).toBool())
    {
        item->setSelected(true);
        return;
    }

    QList<QTreeWidgetItem*> leaves;
    collectLeaves(item, leaves);
    for (QTreeWidgetItem* leaf : leaves)
        leaf->setSelected(true);
}

bool SceneTreeWidget::applySubstringFilter(QTreeWidgetItem* item,
                                           const QString& lowerFilter,
                                           bool ancestorMatched,
                                           bool& anyMatch)
{
    if (!item) return false;

    const bool ownMatch = textMatchRank(itemSearchText(item), lowerFilter) > 0;
    bool descendantMatch = false;

    for (int i = 0; i < item->childCount(); ++i)
    {
        if (applySubstringFilter(item->child(i),
                                 lowerFilter,
                                 ancestorMatched || ownMatch,
                                 anyMatch))
        {
            descendantMatch = true;
        }
    }

    const bool visible = ancestorMatched || ownMatch || descendantMatch;
    item->setHidden(!visible);

    if (!visible)
        return false;

    if (item->childCount() > 0 && (ownMatch || descendantMatch))
        item->setExpanded(true);

    if (ownMatch)
    {
        anyMatch = true;
        if (!item->data(0, IsLeafRole).toBool())
            showSubtree(item);
        selectSearchMatch(item);
    }

    return ownMatch || descendantMatch;
}

QTreeWidgetItem* SceneTreeWidget::findBestFuzzyMatch(QTreeWidgetItem* item,
                                                     const QString& lowerFilter,
                                                     int& bestScore) const
{
    if (!item) return nullptr;

    QTreeWidgetItem* bestItem = nullptr;
    const QString text = itemSearchText(item);
    if (!text.isEmpty())
    {
        int score = levenshteinDistance(lowerFilter, text);
        for (const QString& term : searchTerms(text))
            score = std::min(score, levenshteinDistance(lowerFilter, term));
        if (score < bestScore)
        {
            bestScore = score;
            bestItem = item;
        }
    }

    for (int i = 0; i < item->childCount(); ++i)
    {
        QTreeWidgetItem* childBest =
            findBestFuzzyMatch(item->child(i), lowerFilter, bestScore);
        if (childBest)
            bestItem = childBest;
    }

    return bestItem;
}

void SceneTreeWidget::expandOneLevel(QTreeWidgetItem* item)
{
    if (!item) return;
    item->setExpanded(true);
    // Collapse every direct child so their subtrees stay hidden.
    // Without this, children that were previously expanded would show their
    // grandchildren too, making "Expand" indistinguishable from "Expand All".
    for (int i = 0; i < item->childCount(); ++i)
        item->child(i)->setExpanded(false);
}

void SceneTreeWidget::expandSubtree(QTreeWidgetItem* item)
{
    if (!item) return;
    expandItem(item);
    for (int i = 0; i < item->childCount(); ++i)
        expandSubtree(item->child(i));
}

void SceneTreeWidget::collapseOneLevel(QTreeWidgetItem* item)
{
    // Collapse each direct child so its contents are hidden, but leave
    // 'item' itself expanded — the children stay visible and can be
    // individually re-opened.  Deeper levels are untouched.
    if (!item) return;
    for (int i = 0; i < item->childCount(); ++i)
        collapseItem(item->child(i));
    // item stays expanded
}

void SceneTreeWidget::collapseAllBelow(QTreeWidgetItem* item)
{
    // Deep-collapse every descendant so re-opening any child starts fully
    // collapsed.  'item' itself stays expanded so its children remain visible.
    if (!item) return;
    for (int i = 0; i < item->childCount(); ++i)
        collapseSubtreeHelper(item->child(i));
    // item stays expanded
}

// Internal recursive helper: collapse 'item' and all its descendants.
void SceneTreeWidget::collapseSubtreeHelper(QTreeWidgetItem* item)
{
    if (!item) return;
    for (int i = 0; i < item->childCount(); ++i)
        collapseSubtreeHelper(item->child(i));
    // Use setExpanded(false) directly — collapseItem() can silently no-op
    // on items that Qt's view hasn't yet materialised (first invocation).
    item->setExpanded(false);
}

void SceneTreeWidget::updateItemIcon(QTreeWidgetItem* item)
{
    if (!item) return;
    const Qt::CheckState cs = item->checkState(0);
    if (item->data(0, IsLeafRole).toBool())
    {
        item->setIcon(0, cs == Qt::Checked
                      ? treeIcons().meshNormal
                      : treeIcons().meshGrey);
    }
    else
    {
        const bool synthetic = item->data(0, IsSyntheticRole).toBool();
        const bool allHidden = (cs == Qt::Unchecked);
        if (synthetic)
            item->setIcon(0, allHidden ? treeIcons().fileGrey     : treeIcons().fileNormal);
        else
            item->setIcon(0, allHidden ? treeIcons().assemblyGrey : treeIcons().assemblyNormal);
    }
}
