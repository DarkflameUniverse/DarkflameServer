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
	"LxfmlTests.cpp"
)

add_subdirectory(dEnumsTests)
list(APPEND DCOMMONTEST_SOURCES ${DENUMS_TESTS})

# Set our executable
add_executable(dCommonTests ${DCOMMONTEST_SOURCES})

# Needs to be in binary dir for ctest 
if(APPLE)
	add_custom_target(dCommonTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dCommonTests dCommonTestsLink)
endif()

# Link needed libraries
target_link_libraries(dCommonTests ${COMMON_LIBRARIES} GTest::gtest_main)

# Copy test files to testing directory
add_subdirectory(TestBitStreams)
file(COPY ${TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(LxfmlTestFiles)
file(COPY ${LXFMLTESTFILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Discover the tests
gtest_discover_tests(dCommonTests)
