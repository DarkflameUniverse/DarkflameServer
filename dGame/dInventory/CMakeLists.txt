set(DGAME_DINVENTORY_SOURCES
	"EquippedItem.cpp"
	"Inventory.cpp"
	"Item.cpp"
	"ItemSet.cpp"
	"ItemSetPassiveAbility.cpp")

add_library(dInventory STATIC ${DGAME_DINVENTORY_SOURCES})
target_precompile_headers(dInventory REUSE_FROM dGameBase)
