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
include(GoogleTest)

message(STATUS "gtest fetched and is now ready.")

# Add the subdirectories
add_subdirectory(dCommonTests)
add_subdirectory(dGameTests)
