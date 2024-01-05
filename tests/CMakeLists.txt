message (STATUS "Testing is enabled.")
enable_testing()

find_package(GoogleTest REQUIRED)
include(GoogleTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

add_custom_target(conncpp_tests
	${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Add the subdirectories
add_subdirectory(dCommonTests)
add_subdirectory(dGameTests)
