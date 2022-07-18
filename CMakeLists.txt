cmake_minimum_required(VERSION 3.14)
project(Darkflame)
include(CTest)

set (CMAKE_CXX_STANDARD 17)

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

			# Add compiler definition
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${variable_name}=${variable_value}")

			message(STATUS "Variable: ${variable_name} = ${variable_value}")
		endif()
	endif()
endforeach()

# Set the version
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

# Echo the version
message(STATUS "Version: ${PROJECT_VERSION}")

# Compiler flags:
# Disabled deprecated warnings as the MySQL includes have deprecated code in them. 
# Disabled misleading indentation as DL_LinkedList from RakNet has a weird indent.
# Disabled no-register
# Disabled unknown pragmas because Linux doesn't understand Windows pragmas.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPROJECT_VERSION=${PROJECT_VERSION}")
if(UNIX)
	if(APPLE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -O2 -Wuninitialized -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -fPIC")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -O2 -Wuninitialized -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -static-libgcc -fPIC")
	endif()
	if (__dynamic)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
	endif()
	if (__ggdb)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
	endif()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -O2 -fPIC")
elseif(MSVC)
	# Skip warning for invalid conversion from size_t to uint32_t for all targets below for now
	add_compile_options("/wd4267")
elseif(WIN32)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Our output dir
set(CMAKE_BINARY_DIR ${PROJECT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Create a /res directory
make_directory(${CMAKE_BINARY_DIR}/res)

# Create a /locale directory
make_directory(${CMAKE_BINARY_DIR}/locale)

# Create a /logs directory
make_directory(${CMAKE_BINARY_DIR}/logs)

# Copy ini files on first build
set(INI_FILES "authconfig.ini" "chatconfig.ini" "worldconfig.ini" "masterconfig.ini")
foreach(ini ${INI_FILES})
	if (NOT EXISTS ${PROJECT_BINARY_DIR}/${ini})
		configure_file(
			${CMAKE_SOURCE_DIR}/resources/${ini} ${PROJECT_BINARY_DIR}/${ini}
			COPYONLY
		)
	endif()
endforeach()

# Copy vanity files on first build
set(VANITY_FILES "CREDITS.md" "INFO.md" "TESTAMENT.md" "NPC.xml")
foreach(file ${VANITY_FILES})
	configure_file("${CMAKE_SOURCE_DIR}/vanity/${file}" "${CMAKE_BINARY_DIR}/vanity/${file}" COPYONLY)
endforeach()

# Move our migrations for MasterServer to run
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/migrations/)
file(GLOB SQL_FILES ${CMAKE_SOURCE_DIR}/migrations/dlu/*.sql)
foreach(file ${SQL_FILES})
	get_filename_component(file ${file} NAME)
	if (NOT EXISTS ${PROJECT_BINARY_DIR}/migrations/${file})
		configure_file(
			${CMAKE_SOURCE_DIR}/migrations/dlu/${file} ${PROJECT_BINARY_DIR}/migrations/${file}
			COPYONLY
		)
	endif()
endforeach()

# Create our list of include directories
set(INCLUDED_DIRECTORIES
	"dCommon"
	"dChatFilter"
	"dGame" 
	"dGame/dBehaviors" 
	"dGame/dComponents" 
	"dGame/dGameMessages" 
	"dGame/dInventory" 
	"dGame/dMission" 
	"dGame/dEntity" 
	"dGame/dUtilities" 
	"dPhysics" 
	"dZoneManager" 
	"dDatabase" 
	"dDatabase/Tables"
	"dNet"
	"dScripts"

	"thirdparty/raknet/Source"
	"thirdparty/tinyxml2"
	"thirdparty/recastnavigation/Recast/Include"
	"thirdparty/recastnavigation/Detour/Include"
	"thirdparty/SQLite"
	"thirdparty/cpplinq"
	)

# Add system specfic includes for Apple, Windows and Other Unix OS' (including Linux)
if (APPLE)
	include_directories("/usr/local/include/")
endif()

if (WIN32)
	set(INCLUDED_DIRECTORIES ${INCLUDED_DIRECTORIES} "thirdparty/libbcrypt/include")
elseif (UNIX)
	set(INCLUDED_DIRECTORIES ${INCLUDED_DIRECTORIES} "thirdparty/libbcrypt")
	set(INCLUDED_DIRECTORIES ${INCLUDED_DIRECTORIES} "thirdparty/libbcrypt/include/bcrypt")
endif()

include_directories(${ZLIB_INCLUDE_DIRS})
# Add binary directory as an include directory
include_directories(${PROJECT_BINARY_DIR})

# Actually include the directories from our list
foreach (dir ${INCLUDED_DIRECTORIES})
	include_directories(${PROJECT_SOURCE_DIR}/${dir})
endforeach()

# Add linking directories:
link_directories(${PROJECT_BINARY_DIR})

# Load all of our third party directories
add_subdirectory(thirdparty)

# Glob together all headers that need to be precompiled
file(
	GLOB HEADERS_DDATABASE
	LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/dDatabase/*.h
	${PROJECT_SOURCE_DIR}/dDatabase/Tables/*.h
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
add_subdirectory(dPhysics)

# Create a list of common libraries shared between all binaries
set(COMMON_LIBRARIES "dCommon" "dDatabase" "dNet" "raknet" "mariadbConnCpp")

# Add platform specific common libraries
if (UNIX)
	set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "dl" "pthread")

	if (NOT APPLE AND __include_backtrace__)
		set(COMMON_LIBRARIES ${COMMON_LIBRARIES} "backtrace")
	endif()
endif()

add_subdirectory(tests)

# Include all of our binary directories
add_subdirectory(dWorldServer)
add_subdirectory(dAuthServer)
add_subdirectory(dChatServer)
add_subdirectory(dMasterServer) # Add MasterServer last so it can rely on the other binaries

# Add our precompiled headers
target_precompile_headers(
	dGame PRIVATE 
	${HEADERS_DGAME}
)

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