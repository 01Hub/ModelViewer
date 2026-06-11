#include "MaterialVariantsPanel.h"
#include "SceneGraph.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Icon helpers
// ---------------------------------------------------------------------------

static QIcon makeCircleIcon(bool filled, const QColor& color)
{
    constexpr int S = 16;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.5));
    if (filled)
    {
        p.setBrush(color);
    }
    else
    {
        p.setBrush(Qt::NoBrush);
    }
    p.drawEllipse(2, 2, S - 4, S - 4);
    p.end();

    return QIcon(pm);
}

// ---------------------------------------------------------------------------
// MaterialVariantsPanel
// ---------------------------------------------------------------------------

MaterialVariantsPanel::MaterialVariantsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _tree = new QTreeWidget(this);
    _tree->setHeaderHidden(true);
    _tree->setColumnCount(1);
    _tree->setRootIsDecorated(true);
    _tree->setIndentation(16);
    _tree->setAlternatingRowColors(true);
    _tree->setSelectionMode(QAbstractItemView::NoSelection);
    _tree->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(_tree);

    connect(_tree, &QTreeWidget::itemClicked,
            this,  &MaterialVariantsPanel::onItemClicked);
}

void MaterialVariantsPanel::setSceneGraph(SceneGraph* sg)
{
    _sceneGraph = sg;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MaterialVariantsPanel::refresh()
{
    _tree->clear();

    if (!_sceneGraph)
        return;

    const QStringList files = _sceneGraph->filesWithVariants();
    for (const QString& sourceFile : files)
    {
        const GltfVariantData vd  = _sceneGraph->variantDataForFile(sourceFile);
        const int             active = _sceneGraph->activeVariantForFile(sourceFile);

        // --- Single-file: one tree item per file ---
        const QString displayName = QFileInfo(sourceFile).fileName();
        QTreeWidgetItem* fileItem = makeFileItem(sourceFile, displayName);
        _tree->addTopLevelItem(fileItem);

        // "Default" entry always first (variantIndex -1)
        fileItem->addChild(makeVariantItem(tr("Default"), -1, active == -1));

        for (int i = 0; i < vd.variantNames.size(); ++i)
            fileItem->addChild(makeVariantItem(vd.variantNames[i], i, active == i));

        fileItem->setExpanded(true);
    }
}

void MaterialVariantsPanel::setDetachedOverlayMode(bool enabled)
{
    if (_overlayMode == enabled)
        return;

    if (enabled)
    {
        _savedStyleSheet        = _tree->styleSheet();
        _savedPalette           = _tree->palette();
        _savedViewportPalette   = _tree->viewport()->palette();
        _savedAutoFill          = _tree->autoFillBackground();
        _savedViewportAutoFill  = _tree->viewport()->autoFillBackground();

        QPalette p = _savedPalette;
        QColor base      = p.color(QPalette::Base);
        QColor alternate = p.color(QPalette::AlternateBase);
        base.setAlpha(0);
        alternate.setAlpha(0);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternate);

        _tree->setPalette(p);
        _tree->viewport()->setPalette(p);
        _tree->setAutoFillBackground(false);
        _tree->viewport()->setAutoFillBackground(false);
        _tree->setAttribute(Qt::WA_NoSystemBackground, true);
        _tree->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        _tree->viewport()->setAttribute(Qt::WA_StyledBackground, false);
        setProperty("detachedOverlayMode", true);
        _tree->setProperty("detachedOverlayMode", true);
        _tree->viewport()->setProperty("detachedOverlayMode", true);
        _tree->setStyleSheet(QString());

        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
    }
    else
    {
        _tree->setStyleSheet(_savedStyleSheet);
        _tree->setPalette(_savedPalette);
        _tree->viewport()->setPalette(_savedViewportPalette);
        _tree->setAutoFillBackground(_savedAutoFill);
        _tree->viewport()->setAutoFillBackground(_savedViewportAutoFill);
        _tree->setAttribute(Qt::WA_NoSystemBackground, false);
        _tree->viewport()->setAttribute(Qt::WA_NoSystemBackground, false);
        _tree->viewport()->setAttribute(Qt::WA_StyledBackground, false);
        setProperty("detachedOverlayMode", false);
        _tree->setProperty("detachedOverlayMode", false);
        _tree->viewport()->setProperty("detachedOverlayMode", false);

        setAttribute(Qt::WA_NoSystemBackground, false);
        setAutoFillBackground(true);
    }

    _overlayMode = enabled;
    refreshDetachedOverlayTheme();
    _tree->viewport()->update();
    _tree->update();
    update();
}

void MaterialVariantsPanel::refreshDetachedOverlayTheme()
{
    if (!_overlayMode || !_tree)
        return;

    const bool lightText = property("overlayViewerLightText").toBool();
    const QColor textColor = lightText ? QColor(255, 255, 255) : QColor(0, 0, 0);
    _detachedOverlayFillColor = lightText ? QColor(255, 255, 255, 65) : QColor(0, 0, 0, 45);

    QPalette treePalette = _tree->palette();
    treePalette.setColor(QPalette::Text, textColor);
    treePalette.setColor(QPalette::WindowText, textColor);
    treePalette.setColor(QPalette::ButtonText, textColor);
    treePalette.setColor(QPalette::HighlightedText, textColor);
    _tree->setPalette(treePalette);

    QPalette viewportPalette = _tree->viewport()->palette();
    viewportPalette.setColor(QPalette::Text, textColor);
    viewportPalette.setColor(QPalette::WindowText, textColor);
    viewportPalette.setColor(QPalette::ButtonText, textColor);
    viewportPalette.setColor(QPalette::HighlightedText, textColor);
    _tree->viewport()->setPalette(viewportPalette);

    if (_sceneGraph)
        refresh();
}

void MaterialVariantsPanel::paintEvent(QPaintEvent* event)
{
    if (_overlayMode)
    {
        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(event->rect(), _detachedOverlayFillColor);
    }

    QWidget::paintEvent(event);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MaterialVariantsPanel::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item)
        return;

    const bool isFileItem = item->data(0, IsFileItemRole).toBool();
    if (isFileItem)
        return;  // clicking the file label does nothing

    // SourceFileRole is stored on the file-level parent, not on variant items.
    QTreeWidgetItem* parentItem = item->parent();
    const QString sourceFile = parentItem
        ? parentItem->data(0, SourceFileRole).toString()
        : QString();
    const int variantIndex = item->data(0, VariantIndexRole).toInt();

    if (sourceFile.isEmpty())
        return;

    // Update radio icons for this file's children
    markActiveVariant(sourceFile, variantIndex);

    emit variantActivated(sourceFile, variantIndex);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QTreeWidgetItem* MaterialVariantsPanel::makeFileItem(const QString& sourceFile,
                                                      const QString& displayName) const
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, displayName);
    item->setToolTip(0, sourceFile);
    item->setData(0, SourceFileRole,  sourceFile);
    item->setData(0, IsFileItemRole,  true);
    item->setData(0, VariantIndexRole, QVariant());
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    QFont f = item->font(0);
    f.setBold(true);
    item->setFont(0, f);
    return item;
}

QTreeWidgetItem* MaterialVariantsPanel::makeVariantItem(const QString& label,
                                                         int variantIndex,
                                                         bool active) const
{
    auto* item = new QTreeWidgetItem();
    item->setText(0, label);
    item->setData(0, VariantIndexRole, variantIndex);
    item->setData(0, IsFileItemRole,   false);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    item->setIcon(0, active ? activeIcon() : inactiveIcon());
    return item;
}

void MaterialVariantsPanel::markActiveVariant(const QString& sourceFile, int variantIndex)
{
    for (int ti = 0; ti < _tree->topLevelItemCount(); ++ti)
    {
        QTreeWidgetItem* fileItem = _tree->topLevelItem(ti);
        if (fileItem->data(0, SourceFileRole).toString() != sourceFile)
            continue;

        for (int ci = 0; ci < fileItem->childCount(); ++ci)
        {
            QTreeWidgetItem* child = fileItem->child(ci);
            const int idx = child->data(0, VariantIndexRole).toInt();
            child->setIcon(0, (idx == variantIndex) ? activeIcon() : inactiveIcon());
        }
        break;
    }

    if (_sceneGraph)
        _sceneGraph->setActiveVariant(sourceFile, variantIndex);
}

QIcon MaterialVariantsPanel::activeIcon() const
{
    const QColor c = _tree ? _tree->palette().color(QPalette::Text)
                           : palette().color(QPalette::Text);
    return makeCircleIcon(true, c);
}

QIcon MaterialVariantsPanel::inactiveIcon() const
{
    QColor c = _tree ? _tree->palette().color(QPalette::Text)
                     : palette().color(QPalette::Text);
    c.setAlpha(160);
    return makeCircleIcon(false, c);
}
