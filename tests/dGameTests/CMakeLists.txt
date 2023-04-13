set(DGAMETEST_SOURCES
	"GameDependencies.cpp"
	"LeaderboardTests.cpp"
)

add_subdirectory(dComponentsTests)
list(APPEND DGAMETEST_SOURCES ${DCOMPONENTS_TESTS})

add_subdirectory(dGameMessagesTests)
list(APPEND DGAMETEST_SOURCES ${DGAMEMESSAGES_TESTS})

file(COPY ${GAMEMESSAGE_TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Add the executable.  Remember to add all tests above this!
add_executable(dGameTests ${DGAMETEST_SOURCES})

target_link_libraries(dGameTests ${COMMON_LIBRARIES} GTest::gtest_main dGame dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dChatFilter dNavigation)

# Discover the tests
gtest_discover_tests(dGameTests)
