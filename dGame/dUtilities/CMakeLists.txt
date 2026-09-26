set(DGAME_DUTILITIES_SOURCES "BrickDatabase.cpp"
	"CheatDetection.cpp"
	"DashboardActions.cpp"
	"EconomyLedger.cpp"
	"DashboardNotify.cpp"
	"GUID.cpp"
	"LiveEvents.cpp"
	"Loot.cpp"
	"Mail.cpp"
	"MessageInspector.cpp"
	"ObjectIDManager.cpp"
	"PlayerReports.cpp"
	"Preconditions.cpp"
	"SlashCommandHandler.cpp"
	"VanityUtilities.cpp"
	"WorldMigration.cpp")

add_subdirectory(SlashCommands)

foreach(file ${DGAME_DUTILITIES_SLASHCOMMANDS})
	set(DGAME_DUTILITIES_SOURCES ${DGAME_DUTILITIES_SOURCES} "SlashCommands/${file}")
endforeach()

add_library(dUtilities OBJECT ${DGAME_DUTILITIES_SOURCES})
target_precompile_headers(dUtilities REUSE_FROM dGameBase)
target_include_directories(dUtilities PUBLIC "." "SlashCommands"
	PRIVATE
	"${PROJECT_SOURCE_DIR}/dGame/dComponents"
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # transitive via PossessableComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages"
	"${PROJECT_SOURCE_DIR}/dGame/dBehaviors" # transitive via InventoryComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dMission" # transitive via MissionComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # transitive via dZoneManager/Spawner.h
	"${PROJECT_SOURCE_DIR}/dZoneManager" # Mail.cpp
	"${PROJECT_SOURCE_DIR}/dChatFilter" # DashboardActions.cpp (messages sent into chat)
)
target_link_libraries(dUtilities
	PUBLIC dDatabase dPhysics
	INTERFACE dZoneManager)
