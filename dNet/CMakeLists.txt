set(DNET_SOURCES "AuthPackets.cpp"
	"ChatPackets.cpp"
	"ClientPackets.cpp"
	"dServer.cpp"
	"MasterPackets.cpp"
	"PacketUtils.cpp"
	"WorldPackets.cpp"
	"ZoneInstanceManager.cpp")

add_library(dNet STATIC ${DNET_SOURCES})
target_include_directories(dNet PRIVATE
	${PROJECT_SOURCE_DIR}/dGame/dComponents
	${PROJECT_SOURCE_DIR}/dScripts # transitive through components
)
target_link_libraries(dNet
	PUBLIC dCommon dDatabase
	INTERFACE dZoneManager)
