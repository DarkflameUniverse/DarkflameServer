message (STATUS "Testing is enabled.")
enable_testing()

find_package(GoogleTest REQUIRED)
include(GoogleTest)

message(STATUS "CMAKE_CURRENT_BINARY_DIR = ${CMAKE_CURRENT_BINARY_DIR}")

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

#if(APPLE)
#	set(CMAKE_INSTALL_RPATH_USE_LINK_PATH True)
#	set(CMAKE_BUILD_WITH_INSTALL_RPATH True)
#	set(CMAKE_INSTALL_RPATH "@executable_path")
#endif()

#message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY = ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_custom_target(conncpp_tests
	${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR}) #${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Add the subdirectories
add_subdirectory(dCommonTests)
add_subdirectory(dGameTests)
