set(DMASTERSERVER_SOURCES
#header
	"InstanceManager.h"
	"PersistentIDManager.h"
	"Start.h"

#source
	"InstanceManager.cpp"
	"PersistentIDManager.cpp"
	"Start.cpp"
)

add_library(dMasterServer ${DMASTERSERVER_SOURCES})
target_link_libraries(dMasterServer ${COMMON_LIBRARIES} dZoneManager)
target_include_directories(dMasterServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)

add_executable(MasterServer "MasterServer.cpp")
add_compile_definitions(MasterServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
target_link_libraries(MasterServer ${COMMON_LIBRARIES} dMasterServer dServer)

if(WIN32)
	add_dependencies(MasterServer WorldServer AuthServer ChatServer)
endif()
