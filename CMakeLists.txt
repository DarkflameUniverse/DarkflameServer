cmake_minimum_required(VERSION 3.14)
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
endif()

if(UNIX AND NOT APPLE)
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
    endif()

    link_directories(${backtrace_SOURCE_DIR}/.libs/)
    include_directories(${backtrace_SOURCE_DIR})
endif()

# Set the version
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPROJECT_VERSION=${PROJECT_VERSION}")

# Echo the version
message(STATUS "Version: ${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 17)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Our output dir
set(CMAKE_BINARY_DIR ${PROJECT_BINARY_DIR})

# Create /res, /locale and /logs directories
make_directory(${CMAKE_BINARY_DIR}/res)
make_directory(${CMAKE_BINARY_DIR}/locale)
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
    endif()
endif()

if(WIN32)
    include_directories(${mysql_SOURCE_DIR}/include/jdbc)
    include_directories(${mysql_SOURCE_DIR}/include/jdbc/cppconn)
endif()
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/tinyxml2/)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Recast/Include)
include_directories(${PROJECT_SOURCE_DIR}/thirdparty/recastnavigation/Detour/Include)
include_directories(${ZLIB_INCLUDE_DIRS})

# Bcrypt
if (NOT WIN32)
    include_directories(${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt)
    include_directories(${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/include/bcrypt)
else()
    include_directories(${PROJECT_SOURCE_DIR}/thirdparty/libbcrypt/include)
endif()

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
endif()

# Lib folders:
link_directories(${PROJECT_BINARY_DIR})
if(UNIX)
    if(APPLE)
        link_directories(/usr/local/mysql-connector-c++/lib64/)
    else()
        link_directories(${mysql_SOURCE_DIR}/lib64/)

        # Link to libmysqlcppconn on Linux
        set(MYSQL_LIB mysqlcppconn)
    endif()
endif()

if(WIN32)
    link_directories(${mysql_SOURCE_DIR}/lib64/vs14)
endif()

# Third-Party libraries
add_subdirectory(thirdparty)

# add_subdirectory for libraries
add_subdirectory(dCommon)
add_subdirectory(dChatFilter)
add_subdirectory(dDatabase)
add_subdirectory(dNet)
add_subdirectory(dGame)
add_subdirectory(dZoneManager)
add_subdirectory(dPhysics)

# Setup shared libraries between binaries

set(SHARED_LIBS dCommon dNet dDatabase raknet ${MYSQL_LIB})

if(UNIX)
    set(SHARED_LIBS ${SHARED_LIBS} pthread dl)

    if(NOT APPLE AND __include_backtrace__)
        set(SHARED_LIBS ${SHARED_LIBS} backtrace)
    endif()
endif()

# Our executables:
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(dAuthServer)
add_subdirectory(dChatServer)
add_subdirectory(dWorldServer)
add_subdirectory(dMasterServer)

# Compiler flags:
# Disabled deprecated warnings as the MySQL includes have deprecated code in them. 
# Disabled misleading indentation as DL_LinkedList from RakNet has a weird indent.
# Disabled no-register
# Disabled unknown pragmas because Linux doesn't understand Windows pragmas.
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
elseif(WIN32)
    # Skip warning for invalid conversion from size_t to uint32_t for all targets below for now
    add_compile_options("/wd4267")
endif()

# Finally, add the tests
add_subdirectory(tests)

