set(DCOMMONTEST_SOURCES
	"AMFDeserializeTests.cpp"
	"Amf3Tests.cpp"
	"CastUnderlyingTypeTests.cpp"
	"HeaderSkipTest.cpp"
	"TestCDFeatureGatingTable.cpp"
	"TestLDFFormat.cpp"
	"TestNiPoint3.cpp"
	"TestEncoding.cpp"
	"TestLUString.cpp"
	"TestLUWString.cpp"
	"dCommonDependencies.cpp"
)

add_subdirectory(dEnumsTests)
list(APPEND DCOMMONTEST_SOURCES ${DENUMS_TESTS})

# Set our executable
add_executable(dCommonTests ${DCOMMONTEST_SOURCES})
add_dependencies(dCommonTests conncpp_tests)

if(APPLE)
add_custom_command(TARGET dCommonTests POST_BUILD
	COMMAND otool ARGS -l dCommonTests
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
add_custom_command(TARGET dCommonTests POST_BUILD
	COMMAND otool ARGS -L dCommonTests
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
add_custom_command(TARGET dCommonTests POST_BUILD
	COMMAND ls
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
add_custom_command(TARGET dCommonTests POST_BUILD
	COMMAND otool ARGS -D libmariadbcpp.dylib
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

# Link needed libraries
target_link_libraries(dCommonTests ${COMMON_LIBRARIES} GTest::gtest_main)

# Copy test files to testing directory
add_subdirectory(TestBitStreams)
file(COPY ${TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Discover the tests
gtest_discover_tests(dCommonTests)
