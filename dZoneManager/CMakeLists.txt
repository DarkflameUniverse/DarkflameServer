set(DZONEMANAGER_SOURCES
#header
	"dZMCommon.h"
	"dZoneManager.h"
	"Level.h"
	"LUTriggers.h"
	"Spawner.h"
	"WorldConfig.h"
	"Zone.h"
	
#source
	"dZoneManager.cpp"
	"Level.cpp"
	"Spawner.cpp"
	"Zone.cpp")

add_library(dZoneManager STATIC ${DZONEMANAGER_SOURCES})
target_include_directories(dZoneManager PUBLIC .)
target_link_libraries(dZoneManager
	PUBLIC dPhysics
	INTERFACE dWorldServer)
