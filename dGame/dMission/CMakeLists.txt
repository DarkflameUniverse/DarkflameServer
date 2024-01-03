set(DGAME_DMISSION_SOURCES
	"Mission.cpp"
	"MissionPrerequisites.cpp"
	"MissionTask.cpp")

add_library(dMission STATIC ${DGAME_DMISSION_SOURCES})
target_link_libraries(dMission PUBLIC dDatabase)
target_include_directories(dMission PUBLIC "."
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dGame/dComponents"
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via CharacterComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # via CharacterComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # via LevelProgressionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # via InventoryComponent.h
)
target_precompile_headers(dMission REUSE_FROM dGameBase)
