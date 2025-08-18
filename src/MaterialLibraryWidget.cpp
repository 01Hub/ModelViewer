#include "MaterialLibraryWidget.h"
#include <QTreeWidgetItem>
#include <QFontMetrics>


MaterialLibraryWidget::MaterialLibraryWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setMouseTracking(true);

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

        // Brushed metals
        {"BRUSHED_ALUMINUM", GLMaterial::BRUSHED_ALUMINUM},
		{"BRUSHED_STEEL",    GLMaterial::BRUSHED_STEEL},

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

        // Sheen 
        {"FABRIC",          GLMaterial::FABRIC},
        {"VELVET_RED",      GLMaterial::VELVET_RED},
        {"SATIN_FABRIC",    GLMaterial::SATIN_FABRIC},
        {"MICROFIBER_CLOTH",GLMaterial::MICROFIBER_CLOTH},

        // Leather materials
        {"LEATHER_BLACK",   GLMaterial::LEATHER_BLACK},
        {"LEATHER_BROWN",   GLMaterial::LEATHER_BROWN},
        {"LEATHER_RED",     GLMaterial::LEATHER_RED},
        {"LEATHER_WHITE",   GLMaterial::LEATHER_WHITE},
        {"LEATHER_OXBLOOD", GLMaterial::LEATHER_OXBLOOD},
        {"LEATHER_TAN",     GLMaterial::LEATHER_TAN},		

		// Clearcoat materials
        {"CAR_PAINT_RED",   GLMaterial::CAR_PAINT_RED},
        {"CAR_PAINT_METALLIC_BLUE", GLMaterial::CAR_PAINT_METALLIC_BLUE},
        {"CAR_PAINT_WHITE", GLMaterial::CAR_PAINT_WHITE},
        {"CAR_PAINT_METALLIC_GREEN", GLMaterial::CAR_PAINT_METALLIC_GREEN},
        {"CAR_PAINT_PEARL", GLMaterial::CAR_PAINT_PEARL},
		{"MATTE_GREY",      GLMaterial::MATTE_GREY},
        {"PIANO_BLACK",     GLMaterial::PIANO_BLACK},
        
        // Transmission materials
        {"GLASS",           GLMaterial::GLASS},
        {"FROSTED_GLASS",   GLMaterial::FROSTED_GLASS},
        {"COLORED_GLASS_GREEN", GLMaterial::COLORED_GLASS_GREEN},
        {"CRYSTAL_QUARTZ",  GLMaterial::CRYSTAL_QUARTZ},

        // Emissive materials
        {"NEON_BLUE",       GLMaterial::NEON_BLUE},
        {"NEON_GREEN",      GLMaterial::NEON_GREEN},
        {"NEON_RED",        GLMaterial::NEON_RED},
        {"NEON_YELLOW",     GLMaterial::NEON_YELLOW},
        {"LED_RED",         GLMaterial::LED_RED},
        {"LED_GREEN",       GLMaterial::LED_GREEN},
        {"LED_BLUE",        GLMaterial::LED_BLUE},
		{"LED_YELLOW",      GLMaterial::LED_YELLOW},
        {"LED_WHITE",       GLMaterial::LED_WHITE},

        // Complex materials
        {"IRIDESCENT_SOAP_BUBBLE", GLMaterial::IRIDESCENT_SOAP_BUBBLE},
        {"CARBON_FIBER",    GLMaterial::CARBON_FIBER},
		{"WET_ASPHALT",     GLMaterial::WET_ASPHALT},

        // Wood materials
        {"WOOD",            GLMaterial::WOOD},
        {"WOOD_BAMBOO",     GLMaterial::WOOD_BAMBOO},
        {"WOOD_CEDAR",      GLMaterial::WOOD_CEDAR},
		{"WOOD_REDWOOD",    GLMaterial::WOOD_REDWOOD},
        {"WOOD_OAK",        GLMaterial::WOOD_OAK},
        {"WOOD_PINE",       GLMaterial::WOOD_PINE},
        {"WOOD_BIRCH",      GLMaterial::WOOD_BIRCH},
        {"WOOD_WALNUT",     GLMaterial::WOOD_WALNUT},
        {"WOOD_CHERRY",     GLMaterial::WOOD_CHERRY},
        {"WOOD_TEAK",       GLMaterial::WOOD_TEAK},
		{"WOOD_MAPLE",      GLMaterial::WOOD_MAPLE},

        // Concrete materials
        {"CONCRETE",        GLMaterial::CONCRETE},
        {"CONCRETE_LIGHT",  GLMaterial::CONCRETE_LIGHT},
        {"CONCRETE_DARK",   GLMaterial::CONCRETE_DARK},
		{"CONCRETE_POLISHED", GLMaterial::CONCRETE_POLISHED },

        // Specials        
        {"WATER",           GLMaterial::WATER},
        {"DIAMOND",         GLMaterial::DIAMOND},
        {"CERAMIC",         GLMaterial::CERAMIC},
        {"SKIN",            GLMaterial::SKIN},
        {"PAPER",           GLMaterial::PAPER},        
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
	// connect when user hovers over an item
    connect(this, &QTreeWidget::itemEntered,
        this, &MaterialLibraryWidget::handleItemEntered);    
}

void MaterialLibraryWidget::handleItemEntered(QTreeWidgetItem* item, int column)
{

    QRect rect = visualItemRect(item);
    QFontMetrics metrics(font());
    QString fullText = item->text(column);
    QString elidedText = metrics.elidedText(fullText, Qt::ElideRight, rect.width());

    if (elidedText != fullText)
    {
        item->setToolTip(column, fullText);
    }
    else
    {
        item->setToolTip(column, QString()); // Clear tooltip if not needed
    }
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

	// --- Brushed Metals ---
	QTreeWidgetItem* brushedMetals = new QTreeWidgetItem(this, QStringList() << "Brushed Metals");
	(new QTreeWidgetItem(brushedMetals, QStringList() << "Brushed Aluminum"))->setData(0, Qt::UserRole, "BRUSHED_ALUMINUM");
	(new QTreeWidgetItem(brushedMetals, QStringList() << "Brushed Steel"))->setData(0, Qt::UserRole, "BRUSHED_STEEL");

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

	// --- Wood materials ---
	QTreeWidgetItem* wood = new QTreeWidgetItem(this, QStringList() << "Wood Materials");
    (new QTreeWidgetItem(wood, QStringList() << "Wood"))->setData(0, Qt::UserRole, "WOOD");
	(new QTreeWidgetItem(wood, QStringList() << "Bamboo"))->setData(0, Qt::UserRole, "WOOD_BAMBOO");
	(new QTreeWidgetItem(wood, QStringList() << "Cedar"))->setData(0, Qt::UserRole, "WOOD_CEDAR");
	(new QTreeWidgetItem(wood, QStringList() << "Redwood"))->setData(0, Qt::UserRole, "WOOD_REDWOOD");
	(new QTreeWidgetItem(wood, QStringList() << "Oak"))->setData(0, Qt::UserRole, "WOOD_OAK");
	(new QTreeWidgetItem(wood, QStringList() << "Pine"))->setData(0, Qt::UserRole, "WOOD_PINE");
	(new QTreeWidgetItem(wood, QStringList() << "Birch"))->setData(0, Qt::UserRole, "WOOD_BIRCH");
	(new QTreeWidgetItem(wood, QStringList() << "Walnut"))->setData(0, Qt::UserRole, "WOOD_WALNUT");
	(new QTreeWidgetItem(wood, QStringList() << "Cherry"))->setData(0, Qt::UserRole, "WOOD_CHERRY");
	(new QTreeWidgetItem(wood, QStringList() << "Teak"))->setData(0, Qt::UserRole, "WOOD_TEAK");
    (new QTreeWidgetItem(wood, QStringList() << "Maple"))->setData(0, Qt::UserRole, "WOOD_MAPLE");

	// --- Concrete materials ---
	QTreeWidgetItem* concrete = new QTreeWidgetItem(this, QStringList() << "Concrete Materials");
	(new QTreeWidgetItem(concrete, QStringList() << "Concrete"))->setData(0, Qt::UserRole, "CONCRETE");
	(new QTreeWidgetItem(concrete, QStringList() << "Concrete Light"))->setData(0, Qt::UserRole, "CONCRETE_LIGHT");
	(new QTreeWidgetItem(concrete, QStringList() << "Concrete Dark"))->setData(0, Qt::UserRole, "CONCRETE_DARK");
	(new QTreeWidgetItem(concrete, QStringList() << "Concrete Polished"))->setData(0, Qt::UserRole, "CONCRETE_POLISHED");
    
    // --- Advanced PBR Materials ---
	// --- Sheen materials ---
    QTreeWidgetItem* sheen = new QTreeWidgetItem(this, QStringList() << "Sheen Materials");
    (new QTreeWidgetItem(sheen, QStringList() << "Fabric"))->setData(0, Qt::UserRole, "FABRIC");
    (new QTreeWidgetItem(sheen, QStringList() << "Velvet Red"))->setData(0, Qt::UserRole, "VELVET_RED");
    (new QTreeWidgetItem(sheen, QStringList() << "Satin Fabric"))->setData(0, Qt::UserRole, "SATIN_FABRIC");
    (new QTreeWidgetItem(sheen, QStringList() << "Microfiber Cloth"))->setData(0, Qt::UserRole, "MICROFIBER_CLOTH");

	// --- Leather materials ---
	QTreeWidgetItem* leather = new QTreeWidgetItem(this, QStringList() << "Leather Materials");
	(new QTreeWidgetItem(leather, QStringList() << "Leather Black"))->setData(0, Qt::UserRole, "LEATHER_BLACK");
	(new QTreeWidgetItem(leather, QStringList() << "Leather Brown"))->setData(0, Qt::UserRole, "LEATHER_BROWN");
	(new QTreeWidgetItem(leather, QStringList() << "Leather Red"))->setData(0, Qt::UserRole, "LEATHER_RED");
	(new QTreeWidgetItem(leather, QStringList() << "Leather White"))->setData(0, Qt::UserRole, "LEATHER_WHITE");
	(new QTreeWidgetItem(leather, QStringList() << "Leather Oxblood"))->setData(0, Qt::UserRole, "LEATHER_OXBLOOD");
	(new QTreeWidgetItem(leather, QStringList() << "Leather Tan"))->setData(0, Qt::UserRole, "LEATHER_TAN");

	// --- Clearcoat materials ---
	QTreeWidgetItem* clearcoat = new QTreeWidgetItem(this, QStringList() << "Clearcoat Materials");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Car Paint Red"))->setData(0, Qt::UserRole, "CAR_PAINT_RED");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Car Paint Metallic Blue"))->setData(0, Qt::UserRole, "CAR_PAINT_METALLIC_BLUE");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Car Paint White"))->setData(0, Qt::UserRole, "CAR_PAINT_WHITE");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Car Paint Metallic Green"))->setData(0, Qt::UserRole, "CAR_PAINT_METALLIC_GREEN");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Car Paint Pearl"))->setData(0, Qt::UserRole, "CAR_PAINT_PEARL");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Matte Grey"))->setData(0, Qt::UserRole, "MATTE_GREY");
	(new QTreeWidgetItem(clearcoat, QStringList() << "Piano Black"))->setData(0, Qt::UserRole, "PIANO_BLACK");
	
	// --- Transmission materials ---
	QTreeWidgetItem* transmission = new QTreeWidgetItem(this, QStringList() << "Transmission Materials");
	(new QTreeWidgetItem(transmission, QStringList() << "Glass"))->setData(0, Qt::UserRole, "GLASS");
	(new QTreeWidgetItem(transmission, QStringList() << "Frosted Glass"))->setData(0, Qt::UserRole, "FROSTED_GLASS");
	(new QTreeWidgetItem(transmission, QStringList() << "Colored Glass (Green)"))->setData(0, Qt::UserRole, "COLORED_GLASS_GREEN");
	(new QTreeWidgetItem(transmission, QStringList() << "Crystal Quartz"))->setData(0, Qt::UserRole, "CRYSTAL_QUARTZ");

	// --- Emissive materials ---
	QTreeWidgetItem* emissive = new QTreeWidgetItem(this, QStringList() << "Emissive Materials");
	(new QTreeWidgetItem(emissive, QStringList() << "Neon Blue"))->setData(0, Qt::UserRole, "NEON_BLUE");
	(new QTreeWidgetItem(emissive, QStringList() << "Neon Green"))->setData(0, Qt::UserRole, "NEON_GREEN");
	(new QTreeWidgetItem(emissive, QStringList() << "Neon Red"))->setData(0, Qt::UserRole, "NEON_RED");
	(new QTreeWidgetItem(emissive, QStringList() << "Neon Yellow"))->setData(0, Qt::UserRole, "NEON_YELLOW");
	(new QTreeWidgetItem(emissive, QStringList() << "LED Red"))->setData(0, Qt::UserRole, "LED_RED");
	(new QTreeWidgetItem(emissive, QStringList() << "LED Green"))->setData(0, Qt::UserRole, "LED_GREEN");
	(new QTreeWidgetItem(emissive, QStringList() << "LED Blue"))->setData(0, Qt::UserRole, "LED_BLUE");
	(new QTreeWidgetItem(emissive, QStringList() << "LED Yellow"))->setData(0, Qt::UserRole, "LED_YELLOW");
	(new QTreeWidgetItem(emissive, QStringList() << "LED White"))->setData(0, Qt::UserRole, "LED_WHITE");

	// --- Complex materials ---
	QTreeWidgetItem* complex = new QTreeWidgetItem(this, QStringList() << "Complex Materials");
	(new QTreeWidgetItem(complex, QStringList() << "Iridescent Soap Bubble"))->setData(0, Qt::UserRole, "IRIDESCENT_SOAP_BUBBLE");
	(new QTreeWidgetItem(complex, QStringList() << "Carbon Fiber"))->setData(0, Qt::UserRole, "CARBON_FIBER");
	(new QTreeWidgetItem(complex, QStringList() << "Wet Asphalt"))->setData(0, Qt::UserRole, "WET_ASPHALT");

    // --- Special ---
    QTreeWidgetItem* special = new QTreeWidgetItem(this, QStringList() << "Special");    
    (new QTreeWidgetItem(special, QStringList() << "Water"))->setData(0, Qt::UserRole, "WATER");
    (new QTreeWidgetItem(special, QStringList() << "Diamond"))->setData(0, Qt::UserRole, "DIAMOND");    
    (new QTreeWidgetItem(special, QStringList() << "Ceramic"))->setData(0, Qt::UserRole, "CERAMIC");    
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
