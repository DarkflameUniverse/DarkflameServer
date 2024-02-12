set(DNET_SOURCES
#header
	"AuthPackets.h"
	"BitStreamUtils.h"
	"ChatPackets.h"
	"ClientPackets.h"
	"dNetCommon.h"
	"dServer.h"
	"MasterPackets.h"
	"PacketUtils.h"
	"WorldPackets.h"
	"ZoneInstanceManager.h"

#source
	"AuthPackets.cpp"
	"ChatPackets.cpp"
	"ClientPackets.cpp"
	"dServer.cpp"
	"MasterPackets.cpp"
	"PacketUtils.cpp"
	"WorldPackets.cpp"
	"ZoneInstanceManager.cpp")

add_library(dNet STATIC ${DNET_SOURCES})
target_include_directories(dNet PUBLIC .)
target_link_libraries(dNet PUBLIC dCommon)
