message (STATUS "Testing is enabled.")
enable_testing()

find_package(GoogleTest REQUIRED)
include(GoogleTest)

unset(CMAKE_RUNTIME_OUTPUT_DIRECTORY) # for now

# Add the subdirectories
add_subdirectory(dCommonTests)
add_subdirectory(dGameTests)
