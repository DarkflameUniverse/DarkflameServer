set(DWORLDSERVER_SOURCES
	"PerformanceManager.cpp"
)

add_library(dWorldServer ${DWORLDSERVER_SOURCES})
add_executable(WorldServer "WorldServer.cpp")
add_compile_definitions(WorldServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

target_link_libraries(dWorldServer ${COMMON_LIBRARIES})
target_link_libraries(WorldServer ${COMMON_LIBRARIES} dChatFilter dGame dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dNavigation dServer)
target_include_directories(WorldServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)

