cmake_minimum_required(VERSION 3.18)
project(Darkflame)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wuninitialized -fPIC")
	add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0 _GLIBCXX_USE_CXX17_ABI=0)

	if(NOT APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -lstdc++fs")
	endif()

	if(${DYNAMIC} AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
	endif()

	if(${GGDB})
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
	endif()

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -O2 -fPIC")
elseif(MSVC)
	# Skip warning for invalid conversion from size_t to uint32_t for all targets below for now
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	add_compile_options ("/wd4267" "/utf-8")
elseif(WIN32)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Our output dir
set(CMAKE_BINARY_DIR ${PROJECT_BINARY_DIR})

# TODO make this not have to override the build type directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Create a /resServer directory
make_directory(${CMAKE_BINARY_DIR}/resServer)

# Create a /logs directory
make_directory(${CMAKE_BINARY_DIR}/logs)

# Copy resource files on first build
set(RESOURCE_FILES "sharedconfig.ini" "authconfig.ini" "chatconfig.ini" "worldconfig.ini" "masterconfig.ini" "blacklist.dcf")
message(STATUS "Checking resource file integrity")

include(Utils)
UpdateConfigOption(${PROJECT_BINARY_DIR}/authconfig.ini "port" "auth_server_port")
UpdateConfigOption(${PROJECT_BINARY_DIR}/chatconfig.ini "port" "chat_server_port")
UpdateConfigOption(${PROJECT_BINARY_DIR}/masterconfig.ini "port" "master_server_port")

foreach(resource_file ${RESOURCE_FILES})
	set(file_size 0)

	if(EXISTS ${PROJECT_BINARY_DIR}/${resource_file})
		file(SIZE ${PROJECT_BINARY_DIR}/${resource_file} file_size)
	endif()

	if(${file_size} EQUAL 0)
		configure_file(
			${CMAKE_SOURCE_DIR}/resources/${resource_file} ${PROJECT_BINARY_DIR}/${resource_file}
			COPYONLY
		)
		message(STATUS "Moved " ${resource_file} " to project binary directory")
	elseif(resource_file MATCHES ".ini")
		message(STATUS "Checking " ${resource_file} " for missing config options")
		file(READ ${PROJECT_BINARY_DIR}/${resource_file} current_file_contents)
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
					message(STATUS "Adding missing config option " ${variable_name} " to " ${resource_file})
					set(line_to_add ${line_to_add} ${line})

					foreach(line_to_append ${line_to_add})
						file(APPEND ${PROJECT_BINARY_DIR}/${resource_file} "\n" ${line_to_append})
					endforeach()

					file(APPEND ${PROJECT_BINARY_DIR}/${resource_file} "\n")
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
set(VANITY_FILES "CREDITS.md" "INFO.md" "TESTAMENT.md" "NPC.xml")

foreach(file ${VANITY_FILES})
	configure_file("${CMAKE_SOURCE_DIR}/vanity/${file}" "${CMAKE_BINARY_DIR}/vanity/${file}" COPYONLY)
endforeach()

# Move our migrations for MasterServer to run
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/migrations/dlu/)
file(GLOB SQL_FILES ${CMAKE_SOURCE_DIR}/migrations/dlu/*.sql)

foreach(file ${SQL_FILES})
	get_filename_component(file ${file} NAME)
	configure_file(${CMAKE_SOURCE_DIR}/migrations/dlu/${file} ${PROJECT_BINARY_DIR}/migrations/dlu/${file})
endforeach()

file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/migrations/cdserver/)
file(GLOB SQL_FILES ${CMAKE_SOURCE_DIR}/migrations/cdserver/*.sql)

foreach(file ${SQL_FILES})
	get_filename_component(file ${file} NAME)
	configure_file(${CMAKE_SOURCE_DIR}/migrations/cdserver/${file} ${PROJECT_BINARY_DIR}/migrations/cdserver/${file})
endforeach()

# Create our list of include directories
set(INCLUDED_DIRECTORIES
	"dCommon"
	"dCommon/dClient"
	"dCommon/dEnums"

	"dChatFilter"

	"dGame"
	"dGame/dBehaviors"
	"dGame/dComponents"
	"dGame/dGameMessages"
	"dGame/dInventory"
	"dGame/dMission"
	"dGame/dEntity"
	"dGame/dPropertyBehaviors"
	"dGame/dPropertyBehaviors/ControlBehaviorMessages"
	"dGame/dUtilities"

	"dPhysics"

	"dNavigation"
	"dNavigation/dTerrain"

	"dZoneManager"

	"dDatabase"
	"dDatabase/CDClientDatabase"
	"dDatabase/CDClientDatabase/CDClientTables"
	"dDatabase/GameDatabase"
	"dDatabase/GameDatabase/ITables"
	"dDatabase/GameDatabase/MySQL"
	"dDatabase/GameDatabase/MySQL/Tables"

	"dNet"

	"thirdparty/magic_enum/include/magic_enum"
	"thirdparty/raknet/Source"
	"thirdparty/tinyxml2"
	"thirdparty/recastnavigation"
	"thirdparty/SQLite"
	"thirdparty/cpplinq"
	"thirdparty/cpp-httplib"

	"tests"
	"tests/dCommonTests"
	"tests/dGameTests"
	"tests/dGameTests/dComponentsTests"
)

# Add system specfic includes for Apple, Windows and Other Unix OS' (including Linux)
if(APPLE)
	include_directories("/usr/local/include/")
endif()

# Actually include the directories from our list
foreach(dir ${INCLUDED_DIRECTORIES})
	include_directories(${PROJECT_SOURCE_DIR}/${dir})
endforeach()

if(NOT WIN32)
	include_directories("${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/include/bcrypt")
endif()

include_directories("${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/include")

# Add linking directories:
link_directories(${PROJECT_BINARY_DIR})

# Load all of our third party directories
add_subdirectory(thirdparty)
if (UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
endif()
# Glob together all headers that need to be precompiled
file(
	GLOB HEADERS_DDATABASE
	LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/*.h
	${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/CDClientTables/*.h
	${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables/*.h
	${PROJECT_SOURCE_DIR}/thirdparty/SQLite/*.h
)

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

# Create a list of common libraries shared between all binaries
set(COMMON_LIBRARIES "dCommon" "dDatabase" "dNet" "raknet" "mariadbConnCpp" "magic_enum")

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

# Need to specify to use the CXX compiler language here or else we get errors including <string>.
target_precompile_headers(
	dDatabase PRIVATE
	"$<$<COMPILE_LANGUAGE:CXX>:${HEADERS_DDATABASE}>"
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
