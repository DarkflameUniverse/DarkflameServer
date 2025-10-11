set(DDASHBOARDSERVER_SOURCES
	"DashboardWeb.cpp"
)

add_executable(DashboardServer "DashboardServer.cpp" "DashboardWeb.cpp")
target_link_libraries(DashboardServer ${COMMON_LIBRARIES} dServer dWeb)
target_include_directories(DashboardServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer ${PROJECT_SOURCE_DIR}/dWeb)
add_compile_definitions(DashboardServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
