set(DGAMETEST_SOURCES
	"GameDependencies.cpp"
	"GuildManagerTests.cpp"
	"LUBitStreamTests.cpp"
	"MailIdPinTests.cpp"
	"EconomyLedgerTests.cpp"
	"PlayerReportsLimitTests.cpp"
	"SlashCommandPermissionTests.cpp"
	"StaleSaveGuardTests.cpp"
	"ContrabandTests.cpp"
	"KnockbackTests.cpp"
	"CollisionFilterTests.cpp"
	"MovementBlockerTests.cpp"
	"ItemSetPassiveAbilityTests.cpp"
	"PowerUpStatisticTests.cpp"
	"PlayerStatisticTests.cpp"
	"BrickByBrickTests.cpp"
	"PropertyBuildersTests.cpp"
	"ModularBuildTableTests.cpp"
	"ModularBuildIdMigrationTests.cpp"
	"UgcManifestTests.cpp"
	"SceneGhostingTests.cpp"
	"HolidayEventsTests.cpp"
	"SetCurrencySourceTests.cpp"
	"LootDropPositionTests.cpp"
	"LootActivityCoinsTests.cpp"
	"PreconditionItemCostTests.cpp"
	"SpiderQueenTests.cpp"
	"TacArcTests.cpp"
	"CDFdbTests.cpp"
	"CDClientReloadTests.cpp"
	"CDFdbBenchmark.cpp"
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
target_include_directories(dGameTests PRIVATE "." "${PROJECT_SOURCE_DIR}/dChatServer" "${PROJECT_SOURCE_DIR}/dScripts")
# Tests against the live CDClient look for it in the source tree's build folder
target_compile_definitions(dGameTests PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

if(APPLE)
	add_custom_target(dGameTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dGameTests dGameTestsLink)
endif()

# Local capture fixtures (tests/fixtures-local, never committed) are found from the source folder
target_compile_definitions(dGameTests PRIVATE DLU_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(dGameTests ${COMMON_LIBRARIES} sqlite3 GTest::gtest_main
	dGame dScripts dPhysics Detour Recast tinyxml2 dWorldServer dZoneManager dChatFilter dChatServer dNavigation bcrypt MD5)

# Discover the tests
gtest_discover_tests(dGameTests)
