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
	${PROJECT_SOURCE_DIR}/dGame # UserManager.h
	${PROJECT_SOURCE_DIR}/dGame/dComponents
	${PROJECT_SOURCE_DIR}/dGame/dGameMessages # GameMessages.h
	${PROJECT_SOURCE_DIR}/dGame/dInventory # via PossessableComponent.h
	${PROJECT_SOURCE_DIR}/dGame/dUtilities # via Item.h
	${PROJECT_SOURCE_DIR}/dScripts # transitive through components
)
target_link_libraries(dNet
    PUBLIC dCommon dDatabase
	PRIVATE dZoneManager)
