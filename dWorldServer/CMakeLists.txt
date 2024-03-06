set(DWORLDSERVER_SOURCES
	"PerformanceManager.cpp"
)

add_library(dWorldServer OBJECT ${DWORLDSERVER_SOURCES})
target_link_libraries(dWorldServer PUBLIC dGameBase dCommon)

add_executable(WorldServer "WorldServer.cpp")
target_include_directories(WorldServer PRIVATE "${PROJECT_SOURCE_DIR}/dChatFilter")
add_compile_definitions(WorldServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

target_include_directories(WorldServer PRIVATE
	"${PROJECT_SOURCE_DIR}/dServer" # BinaryPathFinder.h
)

target_link_libraries(WorldServer ${COMMON_LIBRARIES}
	dScripts
	dGameBase
	dComponents
	dUtilities
	dGameMessages
	dInventory
	dGame dChatFilter dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dNavigation dServer)
