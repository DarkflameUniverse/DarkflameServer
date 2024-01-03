set(DWORLDSERVER_SOURCES
	"PerformanceManager.cpp"
)

add_library(dWorldServer ${DWORLDSERVER_SOURCES})
target_link_libraries(dWorldServer PRIVATE dGame) # ${COMMON_LIBRARIES}

add_executable(WorldServer "WorldServer.cpp")
add_compile_definitions(WorldServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

target_link_libraries(WorldServer ${COMMON_LIBRARIES} dGame dChatFilter dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dNavigation)