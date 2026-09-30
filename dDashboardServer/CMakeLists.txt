set(DDASHBOARDSERVER_SOURCES
	"DashboardServer.cpp"
)

add_subdirectory(routes)
add_subdirectory(auth)

add_executable(DashboardServer ${DDASHBOARDSERVER_SOURCES})

target_include_directories(DashboardServer PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dClient"
	"${PROJECT_SOURCE_DIR}/dCommon/dEnums"
	"${PROJECT_SOURCE_DIR}/dDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/CDClientTables"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/MySQL"
	"${PROJECT_SOURCE_DIR}/dNet"
	"${PROJECT_SOURCE_DIR}/dWeb"
	"${PROJECT_SOURCE_DIR}/dServer"
	"${PROJECT_SOURCE_DIR}/thirdparty"
	"${PROJECT_SOURCE_DIR}/thirdparty/nlohmann"
	"${PROJECT_SOURCE_DIR}/dDashboardServer"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/email"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/notify"
)

target_compile_definitions(DashboardServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
# The game's libraries: the capture viewer reads game messages and replica packets with the game's own structs
target_link_libraries(DashboardServer ${COMMON_LIBRARIES} dWeb dServer bcrypt OpenSSL::Crypto CURL::libcurl DashboardRoutes DashboardAuth
	dScripts dGameBase dComponents dUtilities dGameMessages dInventory dGame dChatFilter dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dNavigation)


# Copy static files and templates to the build directory on every build, so editing only a template or
# script (with no C++ change) still takes effect
add_custom_target(DashboardAssets ALL
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/dDashboardServer/static
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/static ${CMAKE_BINARY_DIR}/dDashboardServer/static
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/dDashboardServer/templates
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/templates ${CMAKE_BINARY_DIR}/dDashboardServer/templates
	COMMENT "Copying DashboardServer static files and templates"
)
add_dependencies(DashboardServer DashboardAssets)
