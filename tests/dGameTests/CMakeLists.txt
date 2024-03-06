set(DGAMETEST_SOURCES
	"GameDependencies.cpp"
)

add_subdirectory(dComponentsTests)
list(APPEND DGAMETEST_SOURCES ${DCOMPONENTS_TESTS})

add_subdirectory(dGameMessagesTests)
list(APPEND DGAMETEST_SOURCES ${DGAMEMESSAGES_TESTS})

file(COPY ${GAMEMESSAGE_TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Add the executable.  Remember to add all tests above this!
add_executable(dGameTests ${DGAMETEST_SOURCES})
add_dependencies(dGameTests conncpp_tests)

# Apple needs some special linkage for the mariadb connector for tests.
if(APPLE)
add_custom_command(TARGET dGameTests POST_BUILD
	COMMAND install_name_tool ARGS -change libmariadbcpp.dylib @rpath/libmariadbcpp.dylib dGameTests
	COMMAND otool ARGS -L dGameTests
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

target_link_libraries(dGameTests ${COMMON_LIBRARIES} GTest::gtest_main
	dGame dScripts dPhysics Detour Recast tinyxml2 dWorldServer dZoneManager dChatFilter dNavigation)

# Discover the tests
gtest_discover_tests(dGameTests)
