set(DGAMETEST_SOURCES
	"GameDependencies.cpp"
)

add_subdirectory(dComponentsTests)
list(APPEND DGAMETEST_SOURCES ${DCOMPONENTS_TESTS})

add_subdirectory(dGameMessagesTests)
list(APPEND DGAMETEST_SOURCES ${DGAMEMESSAGES_TESTS})

file(COPY ${GAMEMESSAGE_TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${COMPONENT_TEST_DATA} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Add the executable.  Remember to add all tests above this!
add_executable(dGameTests ${DGAMETEST_SOURCES})

if(APPLE)
	add_custom_target(dGameTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dGameTests dGameTestsLink)
endif()

target_link_libraries(dGameTests ${COMMON_LIBRARIES} GTest::gtest_main
	dGame dScripts dPhysics Detour Recast tinyxml2 dWorldServer dZoneManager dChatFilter dNavigation)

# Discover the tests
gtest_discover_tests(dGameTests)
