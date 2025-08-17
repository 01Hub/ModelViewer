#include "MaterialLibraryWidget.h"
#include <QTreeWidgetItem>

MaterialLibraryWidget::MaterialLibraryWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::SingleSelection);

    // --- build map of keys to factory methods ---
    materialMap = {
        // Metals
        {"METAL_STEEL",     GLMaterial::METAL_STEEL},
        {"METAL_ALUMINUM",  GLMaterial::METAL_ALUMINUM},
        {"METAL_COPPER",    GLMaterial::METAL_COPPER},
        {"METAL_GOLD",      GLMaterial::METAL_GOLD},
        {"METAL_SILVER",    GLMaterial::METAL_SILVER},
        {"METAL_BRASS",     GLMaterial::METAL_BRASS},
        {"METAL_BRONZE",    GLMaterial::METAL_BRONZE},
        {"METAL_CHROME",    GLMaterial::METAL_CHROME},

        // Stones & gems
        {"STONE_MARBLE",    GLMaterial::STONE_MARBLE},
        {"STONE_GRANITE",   GLMaterial::STONE_GRANITE},
        {"STONE_SLATE",     GLMaterial::STONE_SLATE},
        {"STONE_OBSIDIAN",  GLMaterial::STONE_OBSIDIAN},
        {"STONE_RUBY",      GLMaterial::STONE_RUBY},
        {"STONE_EMERALD",   GLMaterial::STONE_EMERALD},
        {"STONE_TURQUOISE", GLMaterial::STONE_TURQUOISE},
        {"STONE_PEARL",     GLMaterial::STONE_PEARL},

        // Plastics
        {"RED_PLASTIC",     GLMaterial::RED_PLASTIC},
        {"GREEN_PLASTIC",   GLMaterial::GREEN_PLASTIC},
        {"BLACK_PLASTIC",   GLMaterial::BLACK_PLASTIC},

        // Rubbers
        {"RED_RUBBER",      GLMaterial::RED_RUBBER},
        {"GREEN_RUBBER",    GLMaterial::GREEN_RUBBER},
        {"BLACK_RUBBER",    GLMaterial::BLACK_RUBBER},

        // Specials
        {"GLASS",           GLMaterial::GLASS},
        {"WATER",           GLMaterial::WATER},
        {"DIAMOND",         GLMaterial::DIAMOND},
        {"WOOD",            GLMaterial::WOOD}
    };

    populateMaterials();

    connect(this, &QTreeWidget::itemClicked,
        this, &MaterialLibraryWidget::onItemClicked);
}

void MaterialLibraryWidget::populateMaterials()
{
    // --- Metals ---
    QTreeWidgetItem* metals = new QTreeWidgetItem(this, QStringList() << "Metals");
    (new QTreeWidgetItem(metals, QStringList() << "Steel"))->setData(0, Qt::UserRole, "METAL_STEEL");
    (new QTreeWidgetItem(metals, QStringList() << "Aluminum"))->setData(0, Qt::UserRole, "METAL_ALUMINUM");
    (new QTreeWidgetItem(metals, QStringList() << "Copper"))->setData(0, Qt::UserRole, "METAL_COPPER");
    (new QTreeWidgetItem(metals, QStringList() << "Gold"))->setData(0, Qt::UserRole, "METAL_GOLD");
    (new QTreeWidgetItem(metals, QStringList() << "Silver"))->setData(0, Qt::UserRole, "METAL_SILVER");
    (new QTreeWidgetItem(metals, QStringList() << "Brass"))->setData(0, Qt::UserRole, "METAL_BRASS");
    (new QTreeWidgetItem(metals, QStringList() << "Bronze"))->setData(0, Qt::UserRole, "METAL_BRONZE");
    (new QTreeWidgetItem(metals, QStringList() << "Chrome"))->setData(0, Qt::UserRole, "METAL_CHROME");

    // --- Stones ---
    QTreeWidgetItem* stones = new QTreeWidgetItem(this, QStringList() << "Stones & Gems");
    (new QTreeWidgetItem(stones, QStringList() << "Marble"))->setData(0, Qt::UserRole, "STONE_MARBLE");
    (new QTreeWidgetItem(stones, QStringList() << "Granite"))->setData(0, Qt::UserRole, "STONE_GRANITE");
    (new QTreeWidgetItem(stones, QStringList() << "Slate"))->setData(0, Qt::UserRole, "STONE_SLATE");
    (new QTreeWidgetItem(stones, QStringList() << "Obsidian"))->setData(0, Qt::UserRole, "STONE_OBSIDIAN");
    (new QTreeWidgetItem(stones, QStringList() << "Ruby"))->setData(0, Qt::UserRole, "STONE_RUBY");
    (new QTreeWidgetItem(stones, QStringList() << "Emerald"))->setData(0, Qt::UserRole, "STONE_EMERALD");
    (new QTreeWidgetItem(stones, QStringList() << "Turquoise"))->setData(0, Qt::UserRole, "STONE_TURQUOISE");
    (new QTreeWidgetItem(stones, QStringList() << "Pearl"))->setData(0, Qt::UserRole, "STONE_PEARL");

    // --- Plastics ---
    QTreeWidgetItem* plastics = new QTreeWidgetItem(this, QStringList() << "Plastics");
    (new QTreeWidgetItem(plastics, QStringList() << "Red Plastic"))->setData(0, Qt::UserRole, "RED_PLASTIC");
    (new QTreeWidgetItem(plastics, QStringList() << "Green Plastic"))->setData(0, Qt::UserRole, "GREEN_PLASTIC");
    (new QTreeWidgetItem(plastics, QStringList() << "Black Plastic"))->setData(0, Qt::UserRole, "BLACK_PLASTIC");

    // --- Rubbers ---
    QTreeWidgetItem* rubbers = new QTreeWidgetItem(this, QStringList() << "Rubbers");
    (new QTreeWidgetItem(rubbers, QStringList() << "Red Rubber"))->setData(0, Qt::UserRole, "RED_RUBBER");
    (new QTreeWidgetItem(rubbers, QStringList() << "Green Rubber"))->setData(0, Qt::UserRole, "GREEN_RUBBER");
    (new QTreeWidgetItem(rubbers, QStringList() << "Black Rubber"))->setData(0, Qt::UserRole, "BLACK_RUBBER");

    // --- Special ---
    QTreeWidgetItem* special = new QTreeWidgetItem(this, QStringList() << "Special");
    (new QTreeWidgetItem(special, QStringList() << "Glass"))->setData(0, Qt::UserRole, "GLASS");
    (new QTreeWidgetItem(special, QStringList() << "Water"))->setData(0, Qt::UserRole, "WATER");
    (new QTreeWidgetItem(special, QStringList() << "Diamond"))->setData(0, Qt::UserRole, "DIAMOND");
    (new QTreeWidgetItem(special, QStringList() << "Wood"))->setData(0, Qt::UserRole, "WOOD");

    expandAll();
}

void MaterialLibraryWidget::onItemClicked(QTreeWidgetItem *item, int column)
{
    if (!item->childCount())
    {
        QString key = item->data(0, Qt::UserRole).toString();
        if (materialMap.contains(key))
        {
            emit materialSelected(materialMap[key]());
        }
        else
        {
            emit materialSelected(GLMaterial::DEFAULT_MAT());
        }
    }
}
