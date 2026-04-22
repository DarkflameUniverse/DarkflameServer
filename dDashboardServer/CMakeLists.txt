add_subdirectory(blueprints)

set(DDASHBOARDSERVER_SOURCES
	"DashboardWeb.cpp"
	# Explicitly include blueprint sources to ensure they are compiled into the library
	"blueprints/AuthBlueprint.cpp"
	"blueprints/ApiBlueprint.cpp"
	"blueprints/PageBlueprint.cpp"
	"blueprints/PlayKeysBlueprint.cpp"
	"blueprints/CharactersBlueprint.cpp"
	"blueprints/MailBlueprint.cpp"
	"blueprints/BugReportsBlueprint.cpp"
	"blueprints/ModerationBlueprint.cpp"
)

# Create dDashboardServer library
add_library(dDashboardServer ${DDASHBOARDSERVER_SOURCES})
target_include_directories(dDashboardServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)
find_package(CURL)
if (CURL_FOUND)
	target_link_libraries(dDashboardServer ${COMMON_LIBRARIES} dServer Crow::Crow bcrypt CURL::libcurl)
else()
	message(WARNING "libcurl not found; building dDashboardServer without CURL::libcurl. Some features may be disabled.")
	target_link_libraries(dDashboardServer ${COMMON_LIBRARIES} dServer Crow::Crow bcrypt)
endif()

add_executable(DashboardServer "DashboardServer.cpp")
if (CURL_FOUND)
	target_link_libraries(DashboardServer ${COMMON_LIBRARIES} dServer Crow::Crow bcrypt CURL::libcurl dDashboardServer)
else()
	target_link_libraries(DashboardServer ${COMMON_LIBRARIES} dServer Crow::Crow bcrypt dDashboardServer)
endif()
target_include_directories(DashboardServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)
add_compile_definitions(DashboardServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

# Define Windows version for ASIO/Crow compatibility (Windows 10)
if(WIN32)
    target_compile_definitions(DashboardServer PRIVATE _WIN32_WINNT=0x0A00)
    target_compile_definitions(dDashboardServer PRIVATE _WIN32_WINNT=0x0A00)
endif()

# Copy static files and templates to build directory
add_custom_command(TARGET DashboardServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/static
		$<TARGET_FILE_DIR:DashboardServer>/static
	COMMENT "Copying static files to build directory"
)

add_custom_command(TARGET DashboardServer POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/templates
		$<TARGET_FILE_DIR:DashboardServer>/templates
	COMMENT "Copying templates to build directory"
)
