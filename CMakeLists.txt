cmake_minimum_required(VERSION 3.25)
project(Darkflame
	HOMEPAGE_URL "https://github.com/DarkflameUniverse/DarkflameServer"
	LANGUAGES C CXX
)

# check if the path to the source directory contains a space
if("${CMAKE_SOURCE_DIR}" MATCHES " ")
	message(FATAL_ERROR "The server cannot build in the path (" ${CMAKE_SOURCE_DIR} ") because it contains a space. Please move the server to a path without spaces.")
endif()

include(CTest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 	# Export the compile commands for debugging
set(CMAKE_POLICY_DEFAULT_CMP0063 NEW) 	# Set CMAKE visibility policy to NEW on project and subprojects
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON) # Set C and C++ symbol visibility to hide inlined functions
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(FETCHCONTENT_QUIET FALSE) # GLM takes a long time to clone, this will at least show _something_ while its downloading

# Read variables from file
FILE(READ "${CMAKE_SOURCE_DIR}/CMakeVariables.txt" variables)

string(REPLACE "\\\n" "" variables ${variables})
string(REPLACE "\n" ";" variables ${variables})

# Set the cmake variables, formatted as "VARIABLE #" in variables
foreach(variable ${variables})
	# If the string contains a #, skip it
	if(NOT "${variable}" MATCHES "#")
		# Split the variable into name and value
		string(REPLACE "=" ";" variable ${variable})

		# Check that the length of the variable is 2 (name and value)
		list(LENGTH variable length)

		if(${length} EQUAL 2)
			list(GET variable 0 variable_name)
			list(GET variable 1 variable_value)

			# Set the variable
			set(${variable_name} ${variable_value})

			message(STATUS "Variable: ${variable_name} = ${variable_value}")
		endif()
	endif()
endforeach()

# Set the version
set(PROJECT_VERSION "\"${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}\"")

# Echo the version
message(STATUS "Version: ${PROJECT_VERSION}")

# Disable demo, tests and examples for recastNavigation.  Turn these to ON if you want to use them
# This has to be done here to prevent a rare build error due to missing dependencies on the initial generations.
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)

# Compiler flags:
# Disabled deprecated warnings as the MySQL includes have deprecated code in them.
# Disabled misleading indentation as DL_LinkedList from RakNet has a weird indent.
# Disabled no-register
# Disabled unknown pragmas because Linux doesn't understand Windows pragmas.
if(UNIX)
	add_link_options("-Wl,-rpath,$ORIGIN/")
	add_compile_options("-fPIC")
	add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0 _GLIBCXX_USE_CXX17_ABI=0)

	# For all except Clang and Apple Clang
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		add_compile_options("-static-libgcc" "-lstdc++fs")
	endif()

	if(${DYNAMIC} AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		add_link_options("-export-dynamic")
	endif()

	if(${GGDB})
		add_compile_options("-ggdb")
	endif()
elseif(MSVC)
	# Skip warning for invalid conversion from size_t to uint32_t for all targets below for now
	# Also disable non-portable MSVC volatile behavior
	add_compile_options("/wd4267" "/utf-8" "/volatile:iso" "/Zc:inline")
elseif(WIN32)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Our output dir
#set(CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON) # unfortunately, forces all libraries to be built in series, which will slow down the build process

# TODO make this not have to override the build type directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

find_package(MariaDB)

# Create a /resServer directory
make_directory(${CMAKE_BINARY_DIR}/resServer)

# Create a /logs directory
make_directory(${CMAKE_BINARY_DIR}/logs)

# Get DLU config directory
if(DEFINED ENV{DLU_CONFIG_DIR})
	set(DLU_CONFIG_DIR $ENV{DLU_CONFIG_DIR})
else()
	set(DLU_CONFIG_DIR ${PROJECT_BINARY_DIR})
endif()
message(STATUS "Variable: DLU_CONFIG_DIR = ${DLU_CONFIG_DIR}")

# Copy resource files on first build
set(RESOURCE_FILES "sharedconfig.ini" "authconfig.ini" "chatconfig.ini" "worldconfig.ini" "masterconfig.ini" "blocklist.dcf")
message(STATUS "Checking resource file integrity")

include(Utils)
UpdateConfigOption(${DLU_CONFIG_DIR}/authconfig.ini "port" "auth_server_port")
UpdateConfigOption(${DLU_CONFIG_DIR}/chatconfig.ini "port" "chat_server_port")
UpdateConfigOption(${DLU_CONFIG_DIR}/masterconfig.ini "port" "master_server_port")

foreach(resource_file ${RESOURCE_FILES})
	set(file_size 0)

	if(EXISTS ${DLU_CONFIG_DIR}/${resource_file})
		file(SIZE ${DLU_CONFIG_DIR}/${resource_file} file_size)
	endif()

	if(${file_size} EQUAL 0)
		configure_file(
			${CMAKE_SOURCE_DIR}/resources/${resource_file} ${DLU_CONFIG_DIR}/${resource_file}
			COPYONLY
		)
		message(STATUS "Moved " ${resource_file} " to DLU config directory")
	elseif(resource_file MATCHES ".ini")
		message(STATUS "Checking " ${resource_file} " for missing config options")
		file(READ ${DLU_CONFIG_DIR}/${resource_file} current_file_contents)
		string(REPLACE "\\\n" "" current_file_contents ${current_file_contents})
		string(REPLACE "\n" ";" current_file_contents ${current_file_contents})
		set(parsed_current_file_contents "")

		# Remove comment lines so they do not interfere with the variable parsing
		foreach(line ${current_file_contents})
			string(FIND ${line} "#" is_comment)

			if(NOT ${is_comment} EQUAL 0)
				string(APPEND parsed_current_file_contents ${line})
			endif()
		endforeach()

		file(READ ${CMAKE_SOURCE_DIR}/resources/${resource_file} depot_file_contents)
		string(REPLACE "\\\n" "" depot_file_contents ${depot_file_contents})
		string(REPLACE "\n" ";" depot_file_contents ${depot_file_contents})
		set(line_to_add "")

		foreach(line ${depot_file_contents})
			string(FIND ${line} "#" is_comment)

			if(NOT ${is_comment} EQUAL 0)
				string(REPLACE "=" ";" line_split ${line})
				list(GET line_split 0 variable_name)

				if(NOT ${parsed_current_file_contents} MATCHES ${variable_name})
					# For backwards compatibility with older setup versions, dont add this option.
					if(NOT ${variable_name} MATCHES "database_type")
						message(STATUS "Adding missing config option " ${variable_name} " to " ${resource_file})
						set(line_to_add ${line_to_add} ${line})

						foreach(line_to_append ${line_to_add})
							file(APPEND ${DLU_CONFIG_DIR}/${resource_file} "\n" ${line_to_append})
						endforeach()

						file(APPEND ${DLU_CONFIG_DIR}/${resource_file} "\n")
					endif()
				endif()
				set(line_to_add "")
			else()
				set(line_to_add ${line_to_add} ${line})
			endif()
		endforeach()
	endif()
endforeach()

message(STATUS "Resource file integrity check complete")

# if navmeshes directory does not exist, create it
if(NOT EXISTS ${PROJECT_BINARY_DIR}/navmeshes)
	file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/navmeshes)
endif()

# Copy navmesh data on first build and extract it
configure_file(${CMAKE_SOURCE_DIR}/resources/navmeshes.zip ${PROJECT_BINARY_DIR}/navmeshes.zip COPYONLY)

file(ARCHIVE_EXTRACT INPUT ${PROJECT_BINARY_DIR}/navmeshes.zip DESTINATION ${PROJECT_BINARY_DIR}/navmeshes)
file(REMOVE ${PROJECT_BINARY_DIR}/navmeshes.zip)

# Copy vanity files on first build
set(VANITY_FILES "CREDITS.md" "INFO.md" "TESTAMENT.md" "root.xml" "dev-tribute.xml" "atm.xml" "demo.xml")

foreach(file ${VANITY_FILES})
	configure_file("${CMAKE_SOURCE_DIR}/vanity/${file}" "${CMAKE_BINARY_DIR}/vanity/${file}" COPYONLY)
endforeach()

# Move our migrations for MasterServer to run
file(REMOVE_RECURSE ${PROJECT_BINARY_DIR}/migrations)
file(COPY ${CMAKE_SOURCE_DIR}/migrations DESTINATION ${CMAKE_BINARY_DIR})

# Add system specfic includes for Apple, Windows and Other Unix OS' (including Linux)
if (APPLE)
	include_directories("/usr/local/include/")
endif()

# Load all of our third party directories
add_subdirectory(thirdparty SYSTEM)

# Create our list of include directories
include_directories(
	"dPhysics"

	"dNavigation"

	"dNet"

	"dWeb"

	"tests"
	"tests/dCommonTests"
	"tests/dGameTests"
	"tests/dGameTests/dComponentsTests"

	SYSTEM
	"thirdparty/magic_enum/include/magic_enum"
	"thirdparty/raknet/Source"
	"thirdparty/tinyxml2"
	"thirdparty/recastnavigation"
	"thirdparty/SQLite"
	"thirdparty/cpplinq"
	"thirdparty/MD5"
	"thirdparty/nlohmann"
	"thirdparty/mongoose"
)

# Add system specfic includes for Apple, Windows and Other Unix OS' (including Linux)
# TODO: Should probably not do this.
if(APPLE)
	include_directories("/usr/local/include/")
endif()

# Set warning flags
if(MSVC)
	# add_compile_options("/W4")
	# Want to enable warnings eventually, but WAY too much noise right now
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	add_compile_options("-Wuninitialized" "-Wold-style-cast")
else()
	message(WARNING "Unknown compiler: '${CMAKE_CXX_COMPILER_ID}' - No warning flags enabled.")
endif()

# Add linking directories:
file(
	GLOB HEADERS_DZONEMANAGER
	LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/dZoneManager/*.h
)

file(
	GLOB HEADERS_DCOMMON
	LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/dCommon/*.h
)

file(
	GLOB HEADERS_DGAME
	LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/dGame/Entity.h
	${PROJECT_SOURCE_DIR}/dGame/dGameMessages/GameMessages.h
	${PROJECT_SOURCE_DIR}/dGame/EntityManager.h
	${PROJECT_SOURCE_DIR}/dScripts/CppScripts.h
)

# Add our library subdirectories for creation of the library object
add_subdirectory(dCommon)
add_subdirectory(dDatabase)
add_subdirectory(dChatFilter)
add_subdirectory(dNet)
add_subdirectory(dScripts) # Add for dGame to use
add_subdirectory(dGame)
add_subdirectory(dZoneManager)
add_subdirectory(dNavigation)
add_subdirectory(dPhysics)
add_subdirectory(dServer)
add_subdirectory(dWeb)

# Create a list of common libraries shared between all binaries
set(COMMON_LIBRARIES glm::glm "dCommon" "dDatabase" "dNet" "raknet" "magic_enum")

# Add platform specific common libraries
if(UNIX)
	set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "dl" "pthread")

	if(NOT APPLE AND ${INCLUDE_BACKTRACE})
		set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "backtrace")
	endif()
endif()

# Include all of our binary directories
add_subdirectory(dWorldServer)
add_subdirectory(dAuthServer)
add_subdirectory(dChatServer)
add_subdirectory(dMasterServer) # Add MasterServer last so it can rely on the other binaries

target_precompile_headers(
	dZoneManager PRIVATE
	${HEADERS_DZONEMANAGER}
)

target_precompile_headers(
	dCommon PRIVATE
	${HEADERS_DCOMMON}
)

target_precompile_headers(
	tinyxml2 PRIVATE
	"$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_SOURCE_DIR}/thirdparty/tinyxml2/tinyxml2.h>"
)

if(${ENABLE_TESTING})
	add_subdirectory(tests)
endif()
