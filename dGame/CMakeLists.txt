set(DGAME_SOURCES "Character.cpp"
		"Entity.cpp"
		"EntityManager.cpp"
		"LeaderboardManager.cpp"
		"Player.cpp"
		"TeamManager.cpp"
		"TradingManager.cpp"
		"User.cpp"
		"UserManager.cpp")

include_directories(
	${PROJECT_SOURCE_DIR}/dScripts
	${PROJECT_SOURCE_DIR}/dGame
)

add_library(dGameBase ${DGAME_SOURCES})
target_precompile_headers(dGameBase PRIVATE ${HEADERS_DGAME})
target_include_directories(dGameBase PUBLIC "." "dEntity"
	PRIVATE "dComponents" "dGameMessages" "dBehaviors" "dMission" "dUtilities" "dInventory"
	"${PROJECT_SOURCE_DIR}/dZoneManager"	
)
target_link_libraries(dGameBase
	PUBLIC dDatabase dPhysics
	INTERFACE dComponents dEntity)

add_subdirectory(dBehaviors)
add_subdirectory(dComponents)
add_subdirectory(dEntity)
add_subdirectory(dGameMessages)
add_subdirectory(dInventory)
add_subdirectory(dMission)
add_subdirectory(dPropertyBehaviors)
add_subdirectory(dUtilities)

add_library(dGame INTERFACE)
target_link_libraries(dGame INTERFACE
	dGameBase dBehaviors dComponents dEntity dGameMessages dInventory dMission dPropertyBehaviors dUtilities dScripts
)
