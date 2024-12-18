message (STATUS "Testing is enabled.")
enable_testing()

find_package(GoogleTest REQUIRED)
include(GoogleTest)

# Add the subdirectories
add_subdirectory(dCommonTests)
add_subdirectory(dECSTests)
add_subdirectory(dGameTests)
