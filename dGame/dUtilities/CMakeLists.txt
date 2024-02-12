set(DGAME_DUTILITIES_SOURCES
#header
	"BrickDatabase.h"
	"CheatDetection.h"
	"GUID.h"
	"Loot.h"
	"ObjectIDManager.h"
	"Preconditions.h"
	"VanityUtilities.h"

#source
	"BrickDatabase.cpp"
	"CheatDetection.cpp"
	"GUID.cpp"
	"Loot.cpp"
	"Mail.cpp"
	"ObjectIDManager.cpp"
	"Preconditions.cpp"
	"SlashCommandHandler.cpp"
	"VanityUtilities.cpp")

add_library(dUtilities STATIC ${DGAME_DUTILITIES_SOURCES})
target_precompile_headers(dUtilities REUSE_FROM dGameBase)
target_include_directories(dUtilities PUBLIC .)
target_link_libraries(dUtilities
	PUBLIC dDatabase dPhysics
	INTERFACE dZoneManager)
