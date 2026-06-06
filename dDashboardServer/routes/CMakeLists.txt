set(DASHBOARDROUTES_SOURCES
	"APIRoutes.cpp"
	"StaticRoutes.cpp"
	"DashboardRoutes.cpp"
	"WSRoutes.cpp"
	"AuthRoutes.cpp"
)

add_library(DashboardRoutes STATIC ${DASHBOARDROUTES_SOURCES})

target_include_directories(DashboardRoutes PRIVATE
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

target_link_libraries(DashboardRoutes PRIVATE ${COMMON_LIBRARIES} dWeb dServer)
