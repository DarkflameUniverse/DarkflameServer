set(DMASTERSERVER_SOURCES
	"InstanceManager.cpp"
	"ObjectIDManager.cpp"
	"Start.cpp"
)

add_library(dMasterServer ${DMASTERSERVER_SOURCES})
add_executable(MasterServer "MasterServer.cpp")
add_compile_definitions(MasterServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

target_link_libraries(dMasterServer ${COMMON_LIBRARIES})
target_link_libraries(MasterServer ${COMMON_LIBRARIES} dMasterServer dServer)
target_include_directories(dMasterServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)

if(WIN32)
	add_dependencies(MasterServer WorldServer AuthServer ChatServer)
endif()
