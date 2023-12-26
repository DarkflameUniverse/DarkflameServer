set(DWORLDSERVER_SOURCES
	"ObjectIDManager.cpp"
	"PerformanceManager.cpp"
)

add_library(dWorldServer ${DWORLDSERVER_SOURCES})
add_executable(WorldServer "WorldServer.cpp")

target_link_libraries(dWorldServer ${COMMON_LIBRARIES})
target_link_libraries(WorldServer ${COMMON_LIBRARIES} dChatFilter dGame dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dNavigation)

# If using gcc, warn on old-style C casts
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(${DWORLDSERVER_SOURCES} PROPERTIES COMPILE_FLAGS "-Wold-style-cast")
endif()
