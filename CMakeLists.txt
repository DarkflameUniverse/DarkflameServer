cmake_minimum_required(VERSION 3.12)
project(Darkflame)
include(CTest)

# Read variables from file
FILE(READ "${CMAKE_SOURCE_DIR}/CMakeVariables.txt" variables)

string(REPLACE "\\\n" "" variables ${variables})
string(REPLACE "\n" ";" variables ${variables})

# Set the cmake variables, formatted as "VARIABLE #" in variables
foreach(variable ${variables})
    # If the string contains a #, skip it
    if("${variable}" MATCHES "#")
        continue()
    endif()

    # Split the variable into name and value
    string(REPLACE "=" ";" variable ${variable})

    # Check that the length of the variable is 2 (name and value)
    list(LENGTH variable length)
    if(NOT ${length} EQUAL 2)
        continue()
    endif()

    list(GET variable 0 variable_name)
    list(GET variable 1 variable_value)

    # Set the variable
    set(${variable_name} ${variable_value})

    # Add compiler definition
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${variable_name}=${variable_value}")

    message(STATUS "Variable: ${variable_name} = ${variable_value}")
endforeach()

# On windows it's better to build this from source, as there's no way FindZLIB is gonna find it
if(NOT WIN32)
find_package(ZLIB REQUIRED)
endif()

# Fetch External (Non-Submodule) Libraries
if(WIN32)
include(FetchContent)

FetchContent_Declare(
    mysql
    URL https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-8.0.27-winx64.zip
    URL_HASH MD5=e3c53f6e4d0a72fde2713f7597bf9468
)

FetchContent_Declare(
    zlib
    URL http://github.com/madler/zlib/archive/refs/tags/v1.2.11.zip
    URL_HASH MD5=9d6a627693163bbbf3f26403a3a0b0b1
)

FetchContent_MakeAvailable(zlib)
FetchContent_MakeAvailable(mysql)

set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
set_target_properties(zlib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIRS}") # Why?
add_library(ZLIB::ZLIB ALIAS zlib) # You're welcome

endif(WIN32)
if(UNIX)
if(APPLE)
else()
include(FetchContent)

FetchContent_Declare(
    mysql
    URL https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-8.0.27-linux-glibc2.12-x86-64bit.tar.gz
    URL_HASH MD5=12f086b76c11022cc7139b41a36cdf9e
)

FetchContent_MakeAvailable(mysql)

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
endif(__include_backtrace__)

endif()
endif(UNIX)

# Set the version
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPROJECT_VERSION=${PROJECT_VERSION}")

# Echo the version
message(STATUS "Version: ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif(WIN32)

# Our output dir
set(CMAKE_BINARY_DIR ${PROJECT_BINARY_DIR})

# Create a /res directory
make_directory(${CMAKE_BINARY_DIR}/res)

# Create a /locale directory
make_directory(${CMAKE_BINARY_DIR}/locale)

# Create a /logs directory
make_directory(${CMAKE_BINARY_DIR}/logs)

# Copy ini files on first build
if (NOT EXISTS ${PROJECT_BINARY_DIR}/authconfig.ini)
    configure_file(
        ${CMAKE_SOURCE_DIR}/resources/authconfig.ini ${PROJECT_BINARY_DIR}/authconfig.ini
        COPYONLY
    )
endif()
if (NOT EXISTS ${PROJECT_BINARY_DIR}/chatconfig.ini)
    configure_file(
        ${CMAKE_SOURCE_DIR}/resources/chatconfig.ini ${PROJECT_BINARY_DIR}/chatconfig.ini
        COPYONLY
    )
endif()
if (NOT EXISTS ${PROJECT_BINARY_DIR}/worldconfig.ini)
    configure_file(
        ${CMAKE_SOURCE_DIR}/resources/worldconfig.ini ${PROJECT_BINARY_DIR}/worldconfig.ini
        COPYONLY
    )
endif()
if (NOT EXISTS ${PROJECT_BINARY_DIR}/masterconfig.ini)
    configure_file(
        ${CMAKE_SOURCE_DIR}/resources/masterconfig.ini ${PROJECT_BINARY_DIR}/masterconfig.ini
        COPYONLY
    )
endif()

# Copy files to output
configure_file("${CMAKE_SOURCE_DIR}/vanity/CREDITS.md" "${CMAKE_BINARY_DIR}/vanity/CREDITS.md" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/vanity/INFO.md" "${CMAKE_BINARY_DIR}/vanity/INFO.md" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/vanity/TESTAMENT.md" "${CMAKE_BINARY_DIR}/vanity/TESTAMENT.md" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/vanity/NPC.xml" "${CMAKE_BINARY_DIR}/vanity/NPC.xml" COPYONLY)

# 3rdparty includes
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/raknet/Source/)
if(UNIX)
    if(APPLE)
        include_directories(/usr/local/include/)
        include_directories(/usr/local/mysql-connector-c++/include/jdbc/)
        include_directories(/usr/local/mysql-connector-c++/include/jdbc/cppconn/)
    else()
        include_directories(${mysql_SOURCE_DIR}/include/jdbc/)
        include_directories(${mysql_SOURCE_DIR}/include/jdbc/cppconn/)
    endif(APPLE)
endif(UNIX)
if(WIN32)
include_directories(${mysql_SOURCE_DIR}/include/jdbc)
include_directories(${mysql_SOURCE_DIR}/include/jdbc/cppconn)
endif(WIN32)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/tinyxml2/)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Detour/Include)
include_directories(${ZLIB_INCLUDE_DIRS})

# Bcrypt
if (NOT WIN32)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/include/bcrypt)
else ()
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/include)
endif ()

# Our includes
include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/dChatFilter/)
include_directories(${PROJECT_SOURCE_DIR}/dCommon/)
include_directories(${PROJECT_SOURCE_DIR}/dGame/)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dBehaviors)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dComponents)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dGameMessages)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dInventory)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dMission)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dEntity)
include_directories(${PROJECT_SOURCE_DIR}/dGame/dUtilities)
include_directories(${PROJECT_SOURCE_DIR}/dPhysics/)
include_directories(${PROJECT_SOURCE_DIR}/dZoneManager/)
include_directories(${PROJECT_SOURCE_DIR}/dDatabase/)
include_directories(${PROJECT_SOURCE_DIR}/dDatabase/Tables/)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/SQLite/)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/cpplinq/)
include_directories(${PROJECT_SOURCE_DIR}/dNet/)
include_directories(${PROJECT_SOURCE_DIR}/dScripts/)

# Default to linking to libmysql
set(MYSQL_LIB mysql)
if(WIN32)
set(MYSQL_LIB mysqlcppconn)
endif(WIN32)

# Lib folders:
link_directories(${PROJECT_BINARY_DIR})
if(UNIX)
    if(APPLE)
        link_directories(/usr/local/mysql-connector-c++/lib64/)
    else()
        link_directories(${mysql_SOURCE_DIR}/lib64/)

        # Link to libmysqlcppconn on Linux
        set(MYSQL_LIB mysqlcppconn)
    endif(APPLE)
endif(UNIX)
if(WIN32)
link_directories(${mysql_SOURCE_DIR}/lib64/vs14)
endif(WIN32)

# Source Code
file(
GLOB SOURCES
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dWorldServer/*.cpp
)

# Source Code for AuthServer
file(
GLOB SOURCES_AUTH
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dAuthServer/*.cpp
)

# Source Code for MasterServer
file(
GLOB SOURCES_MASTER
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dMasterServer/*.cpp
)

# Source Code for ChatServer
file(
GLOB SOURCES_CHAT
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dChatServer/*.cpp
)

# Source Code for raknet
file(
GLOB SOURCES_RAKNET
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/thirdparty/raknet/Source/*.cpp
)

# Source Code for recast
file(
GLOB SOURCES_RECAST
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Source/*.cpp
)

# Source Code for detour
file(
GLOB SOURCES_DETOUR
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Detour/Source/*.cpp
)

# Source Code for tinyxml2
file(
GLOB SOURCES_TINYXML2
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/thirdparty/tinyxml2/tinyxml2.cpp
)

# Source Code for libbcrypt
file(
GLOB SOURCES_LIBBCRYPT
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/*.c
${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/src/*.c
)

# Source Code for dCommon
file(
GLOB SOURCES_DCOMMON
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dCommon/*.cpp
)

# Source Code for dChatFilter
file(
GLOB SOURCES_DCHATFILTER
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dChatFilter/*.cpp
)

# Source Code for dDatabase
file(
GLOB SOURCES_DDATABASE
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dDatabase/*.cpp
${PROJECT_SOURCE_DIR}/dDatabase/Tables/*.cpp
${PROJECT_SOURCE_DIR}/thirdparty/SQLite/*.cpp
${PROJECT_SOURCE_DIR}/thirdparty/SQLite/*.c
)

# Source Code for dNet
file(
GLOB SOURCES_DNET
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dNet/*.cpp
)

# Source Code for dGame
file(
GLOB SOURCES_DGAME
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dGame/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dBehaviors/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dComponents/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dGameMessages/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dInventory/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dMission/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dEntity/*.cpp
${PROJECT_SOURCE_DIR}/dGame/dUtilities/*.cpp
${PROJECT_SOURCE_DIR}/dScripts/*.cpp
)

# Source Code for dZoneManager
file(
GLOB SOURCES_DZM
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dZoneManager/*.cpp
)

# Source Code for dPhysics
file(
GLOB SOURCES_DPHYSICS
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${PROJECT_SOURCE_DIR}/dPhysics/*.cpp
)

if(MSVC)
    # Skip warning for invalid conversion from size_t to uint32_t for all targets below for now
    add_compile_options("/wd4267")
endif(MSVC)

# 3rdparty static libraries:
#add_library(zlib ${SOURCES_ZLIB})
add_library(raknet ${SOURCES_RAKNET})
add_library(tinyxml2 ${SOURCES_TINYXML2})
add_library(detour ${SOURCES_DETOUR})
add_library(recast ${SOURCES_RECAST})
add_library(libbcrypt ${SOURCES_LIBBCRYPT})

# Our static libraries:
add_library(dCommon ${SOURCES_DCOMMON})
add_library(dChatFilter ${SOURCES_DCHATFILTER})
add_library(dDatabase ${SOURCES_DDATABASE})
add_library(dNet ${SOURCES_DNET})
add_library(dGame ${SOURCES_DGAME})
add_library(dZoneManager ${SOURCES_DZM})
add_library(dPhysics ${SOURCES_DPHYSICS})
target_link_libraries(dNet dCommon) #Needed because otherwise linker errors occur.
target_link_libraries(dCommon ZLIB::ZLIB)
target_link_libraries(dCommon libbcrypt)

if(WIN32)
target_link_libraries(raknet ws2_32)
endif(WIN32)

# Our executables:
add_executable(WorldServer ${SOURCES})
add_executable(AuthServer ${SOURCES_AUTH})
add_executable(MasterServer ${SOURCES_MASTER})
add_executable(ChatServer ${SOURCES_CHAT})

# Target libraries to link to:
target_link_libraries(WorldServer dCommon)
target_link_libraries(WorldServer dChatFilter)
target_link_libraries(WorldServer dDatabase)
target_link_libraries(WorldServer dNet)
target_link_libraries(WorldServer dGame)
target_link_libraries(WorldServer dZoneManager)
target_link_libraries(WorldServer dPhysics)
target_link_libraries(WorldServer detour)
target_link_libraries(WorldServer recast)
target_link_libraries(WorldServer raknet)
target_link_libraries(WorldServer ${MYSQL_LIB})
if(UNIX)
target_link_libraries(WorldServer pthread)
target_link_libraries(WorldServer dl)

if(NOT APPLE AND __include_backtrace__)
target_link_libraries(WorldServer backtrace)
target_link_libraries(MasterServer backtrace)
target_link_libraries(AuthServer backtrace)
target_link_libraries(ChatServer backtrace)
endif()

endif(UNIX)
target_link_libraries(WorldServer tinyxml2)

# Target libraries for Auth:
target_link_libraries(AuthServer dCommon)
target_link_libraries(AuthServer dDatabase)
target_link_libraries(AuthServer dNet)
target_link_libraries(AuthServer raknet)
target_link_libraries(AuthServer ${MYSQL_LIB})
if(UNIX)
target_link_libraries(AuthServer pthread)
target_link_libraries(AuthServer dl)
endif(UNIX)

# Target libraries for Master:
target_link_libraries(MasterServer dCommon)
target_link_libraries(MasterServer dDatabase)
target_link_libraries(MasterServer dNet)
target_link_libraries(MasterServer raknet)
target_link_libraries(MasterServer ${MYSQL_LIB})
if(UNIX)
target_link_libraries(MasterServer pthread)
target_link_libraries(MasterServer dl)
endif(UNIX)

# Target libraries for Chat:
target_link_libraries(ChatServer dCommon)
target_link_libraries(ChatServer dChatFilter)
target_link_libraries(ChatServer dDatabase)
target_link_libraries(ChatServer dNet)
target_link_libraries(ChatServer raknet)
target_link_libraries(ChatServer ${MYSQL_LIB})
if(UNIX)
target_link_libraries(ChatServer pthread)
target_link_libraries(ChatServer dl)
endif(UNIX)

# Compiler flags:
# Disabled deprecated warnings as the MySQL includes have deprecated code in them. 
# Disabled misleading indentation as DL_LinkedList from RakNet has a weird indent.
# Disabled no-register
# Disabled unknown pragmas because Linux doesn't understand Windows pragmas.
if(UNIX)
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -O2 -Wuninitialized -Wno-unused-result -Wno-unknown-pragmas -fpermissive -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -fPIC")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17 -O2 -Wuninitialized -Wno-unused-result -Wno-unknown-pragmas -fpermissive -D_GLIBCXX_USE_CXX11_ABI=0 -D_GLIBCXX_USE_CXX17_ABI=0 -static-libgcc -fPIC")
    endif()
    if (__dynamic)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
    endif()
    if (__ggdb)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
    endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -O2 -fPIC")
endif(UNIX)

if(WIN32)
add_dependencies(MasterServer WorldServer)
add_dependencies(MasterServer AuthServer)
add_dependencies(MasterServer ChatServer)
endif()

# Finally, add the tests
add_subdirectory(tests)

