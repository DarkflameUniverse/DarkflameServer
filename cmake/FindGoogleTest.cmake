include(FetchContent)

message(STATUS "Fetching gtest...")

FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(GoogleTest)

message(STATUS "gtest fetched and is now ready.")
set(GoogleTest_FOUND TRUE)
