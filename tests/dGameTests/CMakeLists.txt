set(DGAMETEST_SOURCES
	"GameDependencies.cpp"
	"LUBitStreamTests.cpp"
	"MailIdPinTests.cpp"
	"EconomyLedgerTests.cpp"
	"PlayerReportsLimitTests.cpp"
	"SlashCommandPermissionTests.cpp"
	"StaleSaveGuardTests.cpp"
	"ContrabandTests.cpp"
	"KnockbackTests.cpp"
	"CollisionFilterTests.cpp"
)

add_subdirectory(dComponentsTests)
list(APPEND DGAMETEST_SOURCES ${DCOMPONENTS_TESTS})

add_subdirectory(dGameMessagesTests)
list(APPEND DGAMETEST_SOURCES ${DGAMEMESSAGES_TESTS})

add_subdirectory(dNetTests)
list(APPEND DGAMETEST_SOURCES ${DNET_TESTS})

file(COPY ${GAMEMESSAGE_TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${COMPONENT_TEST_DATA} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Add the executable.  Remember to add all tests above this!
add_executable(dGameTests ${DGAMETEST_SOURCES})
target_include_directories(dGameTests PRIVATE "." "${PROJECT_SOURCE_DIR}/dChatServer")

if(APPLE)
	add_custom_target(dGameTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dGameTests dGameTestsLink)
endif()

target_link_libraries(dGameTests ${COMMON_LIBRARIES} GTest::gtest_main
	dGame dScripts dPhysics Detour Recast tinyxml2 dWorldServer dZoneManager dChatFilter dNavigation bcrypt MD5)

# Discover the tests
gtest_discover_tests(dGameTests)
