set(DGAME_DGAMEMESSAGES_SOURCES
#header
	"DoClientProjectileImpact.h"
	"EchoStartSkill.h"
	"EchoSyncSkill.h"
	"GameMessageHandler.h"
	"GameMessages.h"
	"PropertyDataMessage.h"
	"PropertySelectQueryProperty.h"
	"RequestServerProjectileImpact.h"
	"StartSkill.h"
	"SyncSkill.h"

#source
	"GameMessageHandler.cpp"
	"GameMessages.cpp"
	"PropertyDataMessage.cpp"
	"PropertySelectQueryProperty.cpp")

add_library(dGameMessages STATIC ${DGAME_DGAMEMESSAGES_SOURCES})
target_include_directories(dGameMessages PUBLIC .)
target_link_libraries(dGameMessages PUBLIC dDatabase dUtilities)
target_precompile_headers(dGameMessages REUSE_FROM dGameBase)
