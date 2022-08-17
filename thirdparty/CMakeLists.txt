# Source Code for recast
add_subdirectory(recastnavigation)

# Source Code for tinyxml2
add_subdirectory(tinyxml2)

# Source Code for libbcrypt
# Disable warning about no project version.
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
# Disable warning about the minimum version of cmake used for bcrypt being deprecated in the future
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)

add_subdirectory(libbcrypt)

file(
	GLOB SOURCES_SQLITE3
	LIST_DIRECTORIES false
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	${CMAKE_CURRENT_SOURCE_DIR}/SQLite/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/SQLite/*.c
)

# MariaDB C++ Connector
include(CMakeMariaDBLists.txt)

# Create our third party library objects
add_subdirectory(raknet)
add_library(sqlite3 ${SOURCES_SQLITE3})

if(UNIX)
	# Add warning disable flags and link Unix libraries to sqlite3
	target_link_libraries(sqlite3 pthread dl m)

	#  -Wno-unused-result -Wno-unknown-pragmas -fpermissive
	target_compile_options(sqlite3 PRIVATE "-Wno-return-local-addr" "-Wno-maybe-uninitialized")
endif()

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