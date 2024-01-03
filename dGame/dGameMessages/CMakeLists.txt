set(DGAME_DGAMEMESSAGES_SOURCES
	"GameMessageHandler.cpp"
	"GameMessages.cpp"
	"PropertyDataMessage.cpp"
	"PropertySelectQueryProperty.cpp")

add_library(dGameMessages STATIC ${DGAME_DGAMEMESSAGES_SOURCES})
target_link_libraries(dGameMessages
	PUBLIC dDatabase
	INTERFACE dGameBase # TradingManager
)
target_include_directories(dGameMessages PUBLIC "."
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # direct MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # direct SlashCommandHandler.h
	"${PROJECT_SOURCE_DIR}/dGame/dPropertyBehaviors" # direct ControlBehaviors.h
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
)
target_precompile_headers(dGameMessages REUSE_FROM dGameBase)
