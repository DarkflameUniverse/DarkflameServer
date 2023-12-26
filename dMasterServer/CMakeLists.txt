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

# If using gcc, warn on old-style C casts
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(${DMASTERSERVER_SOURCES} PROPERTIES COMPILE_FLAGS "-Wold-style-cast")
endif()
