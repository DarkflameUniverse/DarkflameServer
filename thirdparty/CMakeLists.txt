# Source Code for recast
add_subdirectory(recastnavigation)

# Turn off tinyxml2 testing
set(tinyxml2_BUILD_TESTING OFF)
# Source Code for tinyxml2
add_subdirectory(tinyxml2)

# Source Code for libbcrypt.  Uses a file glob instead to get around Windows build issues.
file(
	GLOB SOURCES_LIBBCRYPT
	LIST_DIRECTORIES false
	RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	${CMAKE_CURRENT_SOURCE_DIR}/libbcrypt/src/*.c
)

add_library(bcrypt ${SOURCES_LIBBCRYPT})

# Because we are not using the libbcrypt CMakeLists.txt, we need to include these headers for the library to use.
# fortunately they are only needed for building the libbcrypt directory and nothing else, so these are marked private.

if(NOT WIN32)
	target_include_directories(bcrypt PRIVATE "libbcrypt/include/bcrypt")
endif()

# Need to define this on Clang and GNU for 'strdup' support
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_definitions(bcrypt PRIVATE "_POSIX_C_SOURCE=200809L")
endif()

target_include_directories(bcrypt INTERFACE "libbcrypt/include")
target_include_directories(bcrypt PRIVATE "libbcrypt/src")

# Source code for sqlite
add_subdirectory(SQLite)

# Source code for magic_enum
add_subdirectory(magic_enum)

# Create our third party library objects
add_subdirectory(raknet)

# Download Backtrace if configured
if(UNIX AND NOT APPLE)
	include(FetchContent)
	if (${INCLUDE_BACKTRACE} AND ${COMPILE_BACKTRACE})
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

add_subdirectory(MD5)
