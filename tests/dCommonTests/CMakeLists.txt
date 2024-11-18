set(DCOMMONTEST_SOURCES
	"AMFDeserializeTests.cpp"
	"Amf3Tests.cpp"
	"ToUnderlyingTests.cpp"
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

# Apple needs some special linkage for the mariadb connector for tests.
if(APPLE)
add_custom_command(TARGET dCommonTests POST_BUILD
	COMMAND otool ARGS -l dCommonTests
	COMMAND otool ARGS -L dCommonTests
	COMMAND ls
	COMMAND otool ARGS -D libmariadbcpp.dylib
	COMMAND install_name_tool ARGS -change libmariadbcpp.dylib @rpath/libmariadbcpp.dylib dCommonTests
	COMMAND otool ARGS -L dCommonTests
	WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

# Link needed libraries
target_link_libraries(dCommonTests ${COMMON_LIBRARIES} GTest::gtest_main)

# Copy test files to testing directory
add_subdirectory(TestBitStreams)
file(COPY ${TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Discover the tests
gtest_discover_tests(dCommonTests)
