set(DGAME_SOURCES 
#header
	"Character.h"
	"Entity.h"
	"EntityManager.h"
	"LeaderboardManager.h"
	"PlayerManager.h"
	"TeamManager.h"
	"TradingManager.h"
	"User.h"
	"UserManager.h"

#source
	"Character.cpp"
	"Entity.cpp"
	"EntityManager.cpp"
	"LeaderboardManager.cpp"
	"PlayerManager.cpp"
	"TeamManager.cpp"
	"TradingManager.cpp"
	"User.cpp"
	"UserManager.cpp")

add_library(dGameBase ${DGAME_SOURCES})
target_precompile_headers(dGameBase PRIVATE ${HEADERS_DGAME})
target_include_directories(dGameBase PUBLIC .)
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
target_include_directories(dGame INTERFACE
.
./dBehaviors
./dComponents
./dEntity
./dGameMessages
./dInventory
./dMission
./dPropertyBehaviors
./dUtilities
)

target_link_libraries(dGame INTERFACE
	dGameBase dBehaviors dComponents dEntity dGameMessages dInventory dMission dPropertyBehaviors dUtilities dScripts
)
