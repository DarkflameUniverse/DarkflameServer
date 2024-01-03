set(DSCRIPTS_SOURCES
	"ActivityManager.cpp"
	"BaseConsoleTeleportServer.cpp"
	"BasePropertyServer.cpp"
	"BaseRandomServer.cpp"
	"BaseSurvivalServer.cpp"
	"BaseWavesGenericEnemy.cpp"
	"BaseWavesServer.cpp"
	"ChooseYourDestinationNsToNt.cpp"
	"Darkitect.cpp"
	"InvalidScript.cpp"
	"NPCAddRemoveItem.cpp"
	"NtFactionSpyServer.cpp"
	"ScriptComponent.cpp"
	"ScriptedPowerupSpawner.cpp"
	"SpawnPetBaseServer.cpp")

link_libraries(dDatabase dPhysics)

add_library(dScriptsBase STATIC ${DSCRIPTS_SOURCES})
target_include_directories(dScriptsBase PUBLIC .)
target_link_libraries(dScriptsBase INTERFACE dGameBase)
target_precompile_headers(dScriptsBase PRIVATE ${HEADERS_DGAME})

include_directories(
	"${PROJECT_SOURCE_DIR}/dScripts"
	"${PROJECT_SOURCE_DIR}/dGame"
	"${PROJECT_SOURCE_DIR}/dGame/dComponents" # e.g. ScriptedActivityComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # e.g. direct ActivityManager
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # e.g. direct ActivityManager
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager.h
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # viaInventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dZoneManager"
)
link_libraries(dScriptsBase)
# dComponents

add_subdirectory(02_server)
add_subdirectory(ai)
add_subdirectory(client)
add_subdirectory(EquipmentScripts)
add_subdirectory(EquipmentTriggers)
add_subdirectory(zone)

add_library(dScripts STATIC "CppScripts.cpp")
target_precompile_headers(dScripts REUSE_FROM dScriptsBase)
target_include_directories(dScripts PUBLIC ".")
target_link_libraries(dScripts
	dScriptsBase
	dScriptsServer
	dScriptsAI
	dScriptsClient
	dScriptsEquipmentScripts
	dScriptsEquipmentTriggers
	dScriptsZone)
