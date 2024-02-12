set(DGAME_DMISSION_SOURCES
#header
	"Mission.h"
	"MissionPrerequisites.h"
	"MissionTask.h"

#source
	"Mission.cpp"
	"MissionPrerequisites.cpp"
	"MissionTask.cpp")

add_library(dMission STATIC ${DGAME_DMISSION_SOURCES})
target_link_libraries(dMission PUBLIC dDatabase dGame)
target_precompile_headers(dMission REUSE_FROM dGameBase)
