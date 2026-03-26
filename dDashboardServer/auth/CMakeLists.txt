set(DASHBOARDAUTH_SOURCES
	"JWTUtils.cpp"
	"DashboardAuthService.cpp"
	"AuthMiddleware.cpp"
	"AuthTokenHandler.cpp"
	"RequireAuthMiddleware.cpp"
)

add_library(DashboardAuth STATIC ${DASHBOARDAUTH_SOURCES})

target_include_directories(DashboardAuth PRIVATE
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
)

target_link_libraries(DashboardAuth PRIVATE ${COMMON_LIBRARIES} dWeb dServer bcrypt OpenSSL::Crypto)
