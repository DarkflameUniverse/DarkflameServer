set(DGAME_DUTILITIES_SOURCES "BrickDatabase.cpp"
	"CheatDetection.cpp"
	"GUID.cpp"
	"Loot.cpp"
	"Mail.cpp"
	"Preconditions.cpp"
	"SlashCommandHandler.cpp"
	"VanityUtilities.cpp")

add_library(dUtilities STATIC ${DGAME_DUTILITIES_SOURCES})
target_precompile_headers(dUtilities REUSE_FROM dGameBase)
target_link_libraries(dUtilities
	PUBLIC dDatabase dPhysics
	INTERFACE dZoneManager)
