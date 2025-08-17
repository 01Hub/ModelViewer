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
        {"METAL_TITANIUM",  GLMaterial::METAL_TITANIUM},
        {"METAL_PLATINUM",  GLMaterial::METAL_PLATINUM},
        {"METAL_MAGNESIUM", GLMaterial::METAL_MAGNESIUM},
        {"METAL_ZINC",      GLMaterial::METAL_ZINC},
        {"METAL_NICKEL",    GLMaterial::METAL_NICKEL},
        {"METAL_IRON_RAW",  GLMaterial::METAL_IRON_RAW},
        {"METAL_COBALT",    GLMaterial::METAL_COBALT},
        {"METAL_PEWTER",    GLMaterial::METAL_PEWTER},
		{"METAL_TUNGSTEN",  GLMaterial::METAL_TUNGSTEN},

        // Stones & gems
        {"STONE_MARBLE",    GLMaterial::STONE_MARBLE},
        {"STONE_GRANITE",   GLMaterial::STONE_GRANITE},
        {"STONE_SLATE",     GLMaterial::STONE_SLATE},
        {"STONE_OBSIDIAN",  GLMaterial::STONE_OBSIDIAN},
        {"STONE_RUBY",      GLMaterial::STONE_RUBY},
        {"STONE_EMERALD",   GLMaterial::STONE_EMERALD},
        {"STONE_TURQUOISE", GLMaterial::STONE_TURQUOISE},
        {"STONE_PEARL",     GLMaterial::STONE_PEARL},
        {"STONE_JADE",      GLMaterial::STONE_JADE},
        {"STONE_LIMESTONE", GLMaterial::STONE_LIMESTONE},
        {"STONE_SANDSTONE", GLMaterial::STONE_SANDSTONE},
        {"STONE_BASALT",    GLMaterial::STONE_BASALT},
        {"STONE_OBSIDIAN",  GLMaterial::STONE_OBSIDIAN},
        {"STONE_TRAVERTINE",GLMaterial::STONE_TRAVERTINE},
        {"STONE_QUARTZITE", GLMaterial::STONE_QUARTZITE},
		{"STONE_SOAPSTONE", GLMaterial::STONE_SOAPSTONE},

        // Plastics
        {"RED_PLASTIC",     GLMaterial::RED_PLASTIC},
        {"GREEN_PLASTIC",   GLMaterial::GREEN_PLASTIC},
		{"BLUE_PLASTIC",    GLMaterial::BLUE_PLASTIC},
        {"YELLOW_PLASTIC",  GLMaterial::YELLOW_PLASTIC},
		{"CYAN_PLASTIC",    GLMaterial::CYAN_PLASTIC},
		{"MAGENTA_PLASTIC", GLMaterial::MAGENTA_PLASTIC},
        {"BLACK_PLASTIC",   GLMaterial::BLACK_PLASTIC},
		{"WHITE_PLASTIC",   GLMaterial::WHITE_PLASTIC},

        // Rubbers
        {"RED_RUBBER",      GLMaterial::RED_RUBBER},
        {"GREEN_RUBBER",    GLMaterial::GREEN_RUBBER},
		{"BLUE_RUBBER",     GLMaterial::BLUE_RUBBER},
        {"YELLOW_RUBBER",   GLMaterial::YELLOW_RUBBER},
		{"CYAN_RUBBER",     GLMaterial::CYAN_RUBBER},
		{"MAGENTA_RUBBER",  GLMaterial::MAGENTA_RUBBER},
        {"BLACK_RUBBER",    GLMaterial::BLACK_RUBBER},
		{"WHITE_RUBBER",    GLMaterial::WHITE_RUBBER},

        // Specials
        {"GLASS",           GLMaterial::GLASS},
        {"WATER",           GLMaterial::WATER},
        {"DIAMOND",         GLMaterial::DIAMOND},
        {"CERAMIC",         GLMaterial::CERAMIC},
        {"FABRIC",          GLMaterial::FABRIC},
        {"SKIN",            GLMaterial::SKIN},
        {"PAPER",           GLMaterial::PAPER},
        {"WOOD",            GLMaterial::WOOD}
    };

    populateMaterials();

    connect(this, &QTreeWidget::itemClicked,
        this, &MaterialLibraryWidget::onItemClicked);
	// connect when user selects a material using arrow keys
    connect(this, &QTreeWidget::itemSelectionChanged,
        this, [this]() {
            if (selectedItems().isEmpty()) return;
            QTreeWidgetItem* item = selectedItems().first();
            if (item) {
                QString key = item->data(0, Qt::UserRole).toString();
                if (materialMap.contains(key)) {
                    emit materialSelected(materialMap[key]());
                }
            }
		});
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
	(new QTreeWidgetItem(metals, QStringList() << "Titanium"))->setData(0, Qt::UserRole, "METAL_TITANIUM");
	(new QTreeWidgetItem(metals, QStringList() << "Platinum"))->setData(0, Qt::UserRole, "METAL_PLATINUM");
	(new QTreeWidgetItem(metals, QStringList() << "Magnesium"))->setData(0, Qt::UserRole, "METAL_MAGNESIUM");
	(new QTreeWidgetItem(metals, QStringList() << "Zinc"))->setData(0, Qt::UserRole, "METAL_ZINC");
	(new QTreeWidgetItem(metals, QStringList() << "Nickel"))->setData(0, Qt::UserRole, "METAL_NICKEL");
	(new QTreeWidgetItem(metals, QStringList() << "Iron (Raw)"))->setData(0, Qt::UserRole, "METAL_IRON_RAW");
	(new QTreeWidgetItem(metals, QStringList() << "Cobalt"))->setData(0, Qt::UserRole, "METAL_COBALT");
	(new QTreeWidgetItem(metals, QStringList() << "Pewter"))->setData(0, Qt::UserRole, "METAL_PEWTER");
	(new QTreeWidgetItem(metals, QStringList() << "Tungsten"))->setData(0, Qt::UserRole, "METAL_TUNGSTEN");


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
	(new QTreeWidgetItem(stones, QStringList() << "Jade"))->setData(0, Qt::UserRole, "STONE_JADE");
	(new QTreeWidgetItem(stones, QStringList() << "Limestone"))->setData(0, Qt::UserRole, "STONE_LIMESTONE");
	(new QTreeWidgetItem(stones, QStringList() << "Sandstone"))->setData(0, Qt::UserRole, "STONE_SANDSTONE");
	(new QTreeWidgetItem(stones, QStringList() << "Basalt"))->setData(0, Qt::UserRole, "STONE_BASALT");
	(new QTreeWidgetItem(stones, QStringList() << "Obsidian"))->setData(0, Qt::UserRole, "STONE_OBSIDIAN");
	(new QTreeWidgetItem(stones, QStringList() << "Travertine"))->setData(0, Qt::UserRole, "STONE_TRAVERTINE");
	(new QTreeWidgetItem(stones, QStringList() << "Quartzite"))->setData(0, Qt::UserRole, "STONE_QUARTZITE");
	(new QTreeWidgetItem(stones, QStringList() << "Soapstone"))->setData(0, Qt::UserRole, "STONE_SOAPSTONE");

    // --- Plastics ---
    QTreeWidgetItem* plastics = new QTreeWidgetItem(this, QStringList() << "Plastics");
    (new QTreeWidgetItem(plastics, QStringList() << "Red Plastic"))->setData(0, Qt::UserRole, "RED_PLASTIC");
    (new QTreeWidgetItem(plastics, QStringList() << "Green Plastic"))->setData(0, Qt::UserRole, "GREEN_PLASTIC");
	(new QTreeWidgetItem(plastics, QStringList() << "Blue Plastic"))->setData(0, Qt::UserRole, "BLUE_PLASTIC");
    (new QTreeWidgetItem(plastics, QStringList() << "Yellow Plastic"))->setData(0, Qt::UserRole, "YELLOW_PLASTIC");
    (new QTreeWidgetItem(plastics, QStringList() << "Cyan Plastic"))->setData(0, Qt::UserRole, "CYAN_PLASTIC");
	(new QTreeWidgetItem(plastics, QStringList() << "Magenta Plastic"))->setData(0, Qt::UserRole, "MAGENTA_PLASTIC");
    (new QTreeWidgetItem(plastics, QStringList() << "Black Plastic"))->setData(0, Qt::UserRole, "BLACK_PLASTIC");	
    (new QTreeWidgetItem(plastics, QStringList() << "White Plastic"))->setData(0, Qt::UserRole, "WHITE_PLASTIC");

    // --- Rubbers ---
    QTreeWidgetItem* rubbers = new QTreeWidgetItem(this, QStringList() << "Rubbers");
    (new QTreeWidgetItem(rubbers, QStringList() << "Red Rubber"))->setData(0, Qt::UserRole, "RED_RUBBER");
    (new QTreeWidgetItem(rubbers, QStringList() << "Green Rubber"))->setData(0, Qt::UserRole, "GREEN_RUBBER");
	(new QTreeWidgetItem(rubbers, QStringList() << "Blue Rubber"))->setData(0, Qt::UserRole, "BLUE_RUBBER");
	(new QTreeWidgetItem(rubbers, QStringList() << "Yellow Rubber"))->setData(0, Qt::UserRole, "YELLOW_RUBBER");
	(new QTreeWidgetItem(rubbers, QStringList() << "Cyan Rubber"))->setData(0, Qt::UserRole, "CYAN_RUBBER");
	(new QTreeWidgetItem(rubbers, QStringList() << "Magenta Rubber"))->setData(0, Qt::UserRole, "MAGENTA_RUBBER");
    (new QTreeWidgetItem(rubbers, QStringList() << "Black Rubber"))->setData(0, Qt::UserRole, "BLACK_RUBBER");
    (new QTreeWidgetItem(rubbers, QStringList() << "White Rubber"))->setData(0, Qt::UserRole, "WHITE_RUBBER");

    // --- Special ---
    QTreeWidgetItem* special = new QTreeWidgetItem(this, QStringList() << "Special");
    (new QTreeWidgetItem(special, QStringList() << "Glass"))->setData(0, Qt::UserRole, "GLASS");
    (new QTreeWidgetItem(special, QStringList() << "Water"))->setData(0, Qt::UserRole, "WATER");
    (new QTreeWidgetItem(special, QStringList() << "Diamond"))->setData(0, Qt::UserRole, "DIAMOND");
    (new QTreeWidgetItem(special, QStringList() << "Wood"))->setData(0, Qt::UserRole, "WOOD");
	(new QTreeWidgetItem(special, QStringList() << "Ceramic"))->setData(0, Qt::UserRole, "CERAMIC");
	(new QTreeWidgetItem(special, QStringList() << "Fabric"))->setData(0, Qt::UserRole, "FABRIC");
	(new QTreeWidgetItem(special, QStringList() << "Skin"))->setData(0, Qt::UserRole, "SKIN");
	(new QTreeWidgetItem(special, QStringList() << "Paper"))->setData(0, Qt::UserRole, "PAPER");
	
    // --- Expand all items ---
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
