set(DMASTERSERVER_SOURCES
	"InstanceManager.cpp"
	"ObjectIDManager.cpp"
)

add_library(dMasterServer ${DMASTERSERVER_SOURCES})
add_executable(MasterServer "MasterServer.cpp")

target_link_libraries(dMasterServer ${COMMON_LIBRARIES})
target_link_libraries(MasterServer ${COMMON_LIBRARIES} dMasterServer)

if(WIN32)
	add_dependencies(MasterServer WorldServer AuthServer ChatServer)
endif()
