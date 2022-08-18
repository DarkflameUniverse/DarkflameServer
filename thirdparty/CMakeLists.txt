# Source Code for recast
add_subdirectory(recastnavigation)

# Turn off tinyxml2 testing
# set(tinyxml2_BUILD_TESTING OFF)
# Source Code for tinyxml2
add_subdirectory(tinyxml2)

# Source Code for libbcrypt
# Disable warning about no project version.
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
# Disable warning about the minimum version of cmake used for bcrypt being deprecated in the future
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

add_subdirectory(libbcrypt)

# Source code for sqlite
add_subdirectory(SQLite)

# MariaDB C++ Connector
include(CMakeMariaDBLists.txt)

# Create our third party library objects
add_subdirectory(raknet)

# Download Backtrace if configured
if(UNIX AND NOT APPLE)
	include(FetchContent)
	if (__include_backtrace__ AND __compile_backtrace__)
		FetchContent_Declare(
			backtrace
			GIT_REPOSITORY https://github.com/ianlancetaylor/libbacktrace.git
		)

		FetchContent_MakeAvailable(backtrace)

		if (NOT EXISTS ${backtrace_SOURCE_DIR}/.libs)
			set(backtrace_make_cmd "${backtrace_SOURCE_DIR}/configure --prefix=\"/usr\" --enable-shared --with-system-libunwind")

			execute_process(
				COMMAND bash -c "cd ${backtrace_SOURCE_DIR} && ${backtrace_make_cmd} && make && cd ${CMAKE_SOURCE_DIR}"
			)
		endif()

		link_directories(${backtrace_SOURCE_DIR}/.libs/)
		include_directories(${backtrace_SOURCE_DIR})
	endif()
endif()
