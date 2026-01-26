set(DDASHBOARDSERVER_SOURCES
	"DashboardServer.cpp"
	"MasterPacketHandler.cpp"
	"routes/APIRoutes.cpp"
	"routes/StaticRoutes.cpp"
	"routes/DashboardRoutes.cpp"
	"routes/WSRoutes.cpp"
	"routes/AuthRoutes.cpp"
	"auth/JWTUtils.cpp"
	"auth/DashboardAuthService.cpp"
	"auth/AuthMiddleware.cpp"
	"auth/RequireAuthMiddleware.cpp"
)

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
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes"
)

target_link_libraries(DashboardServer ${COMMON_LIBRARIES} dWeb dServer bcrypt OpenSSL::Crypto)


# Copy static files and templates to build directory (always copy)
add_custom_command(TARGET DashboardServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove_directory
	${CMAKE_BINARY_DIR}/dDashboardServer/static
	COMMENT "Removing old static files"
)

add_custom_command(TARGET DashboardServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/static
	${CMAKE_BINARY_DIR}/dDashboardServer/static
	COMMENT "Copying DashboardServer static files"
)

add_custom_command(TARGET DashboardServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E remove_directory
	${CMAKE_BINARY_DIR}/dDashboardServer/templates
	COMMENT "Removing old templates"
)

add_custom_command(TARGET DashboardServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_CURRENT_SOURCE_DIR}/templates
	${CMAKE_BINARY_DIR}/dDashboardServer/templates
	COMMENT "Copying DashboardServer templates"
)
