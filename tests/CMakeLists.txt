message (STATUS "Testing is enabled.  Fetching gtest...")
enable_testing()
include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(GoogleTest)
message(STATUS "gtest fetched and is now ready.")
# create the testing file and list of tests
set(Tests)

add_subdirectory(dCommonTests)
foreach (file ${DCOMMONTEST_SOURCES})
	set(Tests ${Tests} "dCommonTests/${file}")
endforeach ()

add_subdirectory(dGameTests)
foreach (file ${DGAMETEST_SOURCES})
	set(Tests ${Tests} "dGameTests/${file}")
endforeach ()

# add the executable
add_executable(CommonCxxTests ${Tests})
target_link_libraries(CommonCxxTests ${COMMON_LIBRARIES} GTest::gtest_main dGame dZoneManager dPhysics detour recast tinyxml2 dWorldServer dChatFilter dNavigation)

# Copy test files to testing directory
configure_file(
	${CMAKE_SOURCE_DIR}/tests/TestBitStreams/AMFBitStreamTest.bin ${PROJECT_BINARY_DIR}/tests/AMFBitStreamTest.bin
	COPYONLY
)

configure_file(
	${CMAKE_SOURCE_DIR}/tests/TestBitStreams/AMFBitStreamUnimplementedTest.bin ${PROJECT_BINARY_DIR}/tests/AMFBitStreamUnimplementedTest.bin
	COPYONLY
)

include(GoogleTest)
gtest_discover_tests(CommonCxxTests)
