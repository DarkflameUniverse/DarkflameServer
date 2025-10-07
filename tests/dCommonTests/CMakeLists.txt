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


# Copy test files to the build directory where the executable runs from
# Use a custom command to ensure files are copied when the target is built
add_custom_command(TARGET dCommonTests POST_BUILD
    # Copy AMF test files
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/AMFBitStreamTest.bin"
        "$<TARGET_FILE_DIR:dCommonTests>/AMFBitStreamTest.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/AMFBitStreamUnimplementedTest.bin"
        "$<TARGET_FILE_DIR:dCommonTests>/AMFBitStreamUnimplementedTest.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/test.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/test.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/invalid_transform.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/invalid_transform.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/empty_transform.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/empty_transform.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/too_few_values.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/too_few_values.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/non_numeric_transform.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/non_numeric_transform.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/mixed_invalid_transform.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/mixed_invalid_transform.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/no_bricks.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/no_bricks.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/mixed_valid_invalid.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/mixed_valid_invalid.lxfml"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LxfmlTestFiles/deeply_nested.lxfml"
        "$<TARGET_FILE_DIR:dCommonTests>/deeply_nested.lxfml"
    COMMENT "Copying test files to executable directory"
)

# Discover the tests
gtest_discover_tests(dCommonTests)
