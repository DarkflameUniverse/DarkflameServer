set(DGAME_DCINEMA_SOURCES "Recorder.cpp"
    "Prefab.cpp"
    "Scene.cpp"
    "Play.cpp")

add_library(dCinema STATIC ${DGAME_DCINEMA_SOURCES})
target_precompile_headers(dCinema REUSE_FROM dGameBase)
target_link_libraries(dCinema 
    PUBLIC dPhysics dDatabase
    INTERFACE dUtilities dCommon dBehaviors dChatFilter dMission dInventory dComponents
)

target_include_directories(dCinema PUBLIC "."
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # direct MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # direct SlashCommandHandler.h
	"${PROJECT_SOURCE_DIR}/dGame/dPropertyBehaviors" # direct ControlBehaviors.h
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages"
	"${PROJECT_SOURCE_DIR}/dZoneManager" # via GameMessages.cpp, GameMessageHandler.cpp
)