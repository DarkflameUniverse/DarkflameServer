# Source Code for raknet
file(
GLOB SOURCES_RAKNET
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${CMAKE_CURRENT_SOURCE_DIR}/raknet/Source/*.cpp
)

# Source Code for recast
file(
GLOB SOURCES_RECAST
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${CMAKE_CURRENT_SOURCE_DIR}/recastnavigation/Recast/Source/*.cpp
)

# Source Code for detour
file(
GLOB SOURCES_DETOUR
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${CMAKE_CURRENT_SOURCE_DIR}/recastnavigation/Detour/Source/*.cpp
)

# Source Code for tinyxml2
file(
GLOB SOURCES_TINYXML2
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2/tinyxml2.cpp
)

# Source Code for libbcrypt
file(
GLOB SOURCES_LIBBCRYPT
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${CMAKE_CURRENT_SOURCE_DIR}/libbcrypt/*.c
${CMAKE_CURRENT_SOURCE_DIR}/libbcrypt/src/*.c
)

file(
GLOB SOURCES_SQLITE3
LIST_DIRECTORIES false
RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
${CMAKE_CURRENT_SOURCE_DIR}/SQLite/*.cpp
${CMAKE_CURRENT_SOURCE_DIR}/SQLite/*.c
)

# 3rdparty static libraries:
#add_library(zlib ${SOURCES_ZLIB})
add_library(raknet ${SOURCES_RAKNET})
add_library(tinyxml2 ${SOURCES_TINYXML2})
add_library(detour ${SOURCES_DETOUR})
add_library(recast ${SOURCES_RECAST})
add_library(libbcrypt ${SOURCES_LIBBCRYPT})
add_library(sqlite3 ${SOURCES_SQLITE3})

if(UNIX)
target_link_libraries(sqlite3 pthread dl m)

#  -Wno-unused-result -Wno-unknown-pragmas -fpermissive
target_compile_options(sqlite3 PRIVATE "-Wno-return-local-addr" "-Wno-maybe-uninitialized")
target_compile_options(raknet PRIVATE "-Wno-write-strings" "-Wformat-overflow=0" "-Wformat=0")
target_compile_options(libbcrypt PRIVATE "-Wno-implicit-function-declaration" "-Wno-int-conversion")
endif(UNIX)