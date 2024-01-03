set(DGAME_DINVENTORY_SOURCES
	"EquippedItem.cpp"
	"Inventory.cpp"
	"Item.cpp"
	"ItemSet.cpp"
	"ItemSetPassiveAbility.cpp")

add_library(dInventory STATIC ${DGAME_DINVENTORY_SOURCES})
target_include_directories(dInventory PUBLIC "."
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # Item.h uses Preconditions.h
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # direct
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # direct InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dZoneManager" # via Item.cpp
)
target_link_libraries(dInventory INTERFACE dNet)
target_precompile_headers(dInventory REUSE_FROM dGameBase)
