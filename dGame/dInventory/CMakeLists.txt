set(DGAME_DINVENTORY_SOURCES
	"EquippedItem.cpp"
	"Inventory.cpp"
	"Item.cpp"
	"ItemSet.cpp"
	"ItemSetPassiveAbility.cpp")

add_library(dInventory OBJECT ${DGAME_DINVENTORY_SOURCES})
target_include_directories(dInventory PUBLIC "."
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # Item.h uses Preconditions.h
	"${PROJECT_SOURCE_DIR}/dCommon/eEnums" # Item.h uses dCommonVars.h
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dEnums"
	"${PROJECT_SOURCE_DIR}/dCommon/dClient" # Item.cpp uses AssetManager

	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/CDClientTables"

	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # direct
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # direct InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dZoneManager" # via Item.cpp
)
target_precompile_headers(dInventory REUSE_FROM dGameBase)

# INTERFACE link w/o dependency
#set_property(TARGET dInventory APPEND PROPERTY INTERFACE_LINK_LIBRARIES
#	dNet dDatabaseCDClient
#)
