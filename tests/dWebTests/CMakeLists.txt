set(DWEBTESTS_SOURCES
	"MiddlewareTests.cpp"
	"RouteIntegrationTests.cpp"
	"ServerStateTests.cpp"
	"AccountActionTests.cpp"
	"RouteUtilsTests.cpp"
	"JWTTests.cpp"
	"RequireAuthMiddlewareTests.cpp"
	"WebTestDependencies.cpp"
	"PlayerActionTests.cpp"
	"SmtpTests.cpp"
	"OAuth2Tests.cpp"
	"EconomyReportTests.cpp"
	"ItemTraceTests.cpp"
	"CronTests.cpp"
	"PermissionsTests.cpp"
	"PermissionGrantsTests.cpp"
	"ApiKeyTests.cpp"
	"SettingsCatalogTests.cpp"
	"BehaviorXmlTests.cpp"
	"CharacterXmlTests.cpp"
	"CharacterXmlCheckTests.cpp"
	"CharacterRestoreTests.cpp"
	"ZonePathsTests.cpp"
	"RawTerrainTests.cpp"
	"LevelObjectsTests.cpp"
	"GameLabelsTests.cpp"
	"VanityJsonTests.cpp"
	"OpenApiTests.cpp"
	"MessageInspectorTests.cpp"
	"CDClientSchemaTests.cpp"
	"ServerOperationsTests.cpp"
	"PublicPagesTests.cpp"
	"ModerationToolsTests.cpp"
	"LocaleTextTests.cpp"
	"ModeratorHelperTests.cpp"
	"ChatHistoryTests.cpp"
	"ClientSysInfoViewTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/ClientSysInfoView.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/ChatHistory.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/ai/ClaudeClient.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/ai/ModeratorPrompt.cpp"
	"WorldViewTests.cpp"
	"NifFileTests.cpp"
	"DeferredReplyTests.cpp"
	"WorkerPoolTests.cpp"
	"OnceCacheTests.cpp"
	"UgcLookupTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/WorkerPool.cpp"
	"BackupFilesTests.cpp"
	"LogBundleTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/LogBundle.cpp"
	"SecurityFixesTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/BackupFiles.cpp"
	"MetricsFormatTests.cpp"
	"TrafficHistoryTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/TrafficHistory.cpp"
	"NetworkViewTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/NetworkView.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/SettingsCatalog.cpp"
	"AlertsAndTwoFactorTests.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth/Totp.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/notify/WebhookFormat.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/email/OAuth2.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/email/SmtpClient.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth/JWTUtils.cpp"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth/RequireAuthMiddleware.cpp"
)

add_executable(dWebTests ${DWEBTESTS_SOURCES})

target_include_directories(dWebTests PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dClient"
	"${PROJECT_SOURCE_DIR}/dWeb"
	"${PROJECT_SOURCE_DIR}/dDashboardServer"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/routes"
	"${PROJECT_SOURCE_DIR}/dChatFilter"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/email"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/notify"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/ai"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables"
	"${PROJECT_SOURCE_DIR}/dNet"
	"${PROJECT_SOURCE_DIR}/thirdparty/nlohmann"
)

target_link_libraries(dWebTests ${COMMON_LIBRARIES} dWeb dUgc sqlite3 OpenSSL::Crypto CURL::libcurl GTest::gtest_main)

gtest_discover_tests(dWebTests)

target_compile_definitions(dWebTests PRIVATE DLU_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# The 3D views' shader lookups (static/js/scenery-core.js) against the server's bits (NifFile.h), when node is there
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
	add_test(NAME SceneryCoreJs COMMAND ${NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scenery-core.test.mjs"
		"${PROJECT_SOURCE_DIR}/dDashboardServer/static/js/scenery-core.js" "${PROJECT_SOURCE_DIR}/dCommon/NifFile.h"
		"${PROJECT_SOURCE_DIR}/dDashboardServer/routes/Scenery.cpp")
	# The sidebar's saved group state (the script at the end of templates/header.jinja2)
	add_test(NAME SidebarStateJs COMMAND ${NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/sidebar-state.test.mjs"
		"${PROJECT_SOURCE_DIR}/dDashboardServer/templates/header.jinja2")
	# Which links the dashboard follows without a reload, and its in-place update rules (static/js/nav.js)
	add_test(NAME NavRulesJs COMMAND ${NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/nav.test.mjs"
		"${PROJECT_SOURCE_DIR}/dDashboardServer/static/js/nav.js")
	# The Network page's diagram model: nodes and links from the traffic summary (static/js/network-graph.js)
	add_test(NAME NetworkGraphJs COMMAND ${NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/network-graph.test.mjs"
		"${PROJECT_SOURCE_DIR}/dDashboardServer/static/js/network-graph.js")
endif()
