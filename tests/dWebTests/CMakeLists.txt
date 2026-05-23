set(DWEBTESTS_SOURCES
	"MiddlewareTests.cpp"
	"RouteIntegrationTests.cpp"
)

add_executable(dWebTests ${DWEBTESTS_SOURCES})

target_include_directories(dWebTests PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dClient"
	"${PROJECT_SOURCE_DIR}/dWeb"
	"${PROJECT_SOURCE_DIR}/dDashboardServer"
	"${PROJECT_SOURCE_DIR}/dDashboardServer/auth"
	"${PROJECT_SOURCE_DIR}/thirdparty/nlohmann"
)

target_link_libraries(dWebTests ${COMMON_LIBRARIES} dWeb GTest::gtest_main)

gtest_discover_tests(dWebTests)
