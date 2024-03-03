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
	"ScriptedPowerupSpawner.cpp"
	"SpawnPetBaseServer.cpp")

link_libraries(dDatabase dPhysics)

add_library(dScriptsBase OBJECT ${DSCRIPTS_SOURCES})
target_link_libraries(dScriptsBase INTERFACE dGameBase dComponents)
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

add_subdirectory(02_server)
add_subdirectory(ai)
add_subdirectory(client)
add_subdirectory(EquipmentScripts)
add_subdirectory(EquipmentTriggers)
add_subdirectory(zone)

add_library(dScripts STATIC
	$<TARGET_OBJECTS:dScriptsBase>
	$<TARGET_OBJECTS:dScriptsClient>
	$<TARGET_OBJECTS:dScriptsEquipmentScripts>
	$<TARGET_OBJECTS:dScriptsEquipmentTriggers>
	$<TARGET_OBJECTS:dScriptsZone>
	"CppScripts.cpp"
)
target_link_libraries(dScripts PRIVATE dScriptsAI dScriptsServer)
target_include_directories(dScripts PRIVATE
	$<TARGET_PROPERTY:dScriptsBase,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServer,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsAI,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsClient,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsEquipmentScripts,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsEquipmentTriggers,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsZone,INTERFACE_INCLUDE_DIRECTORIES>
)
target_precompile_headers(dScripts REUSE_FROM dScriptsBase)
