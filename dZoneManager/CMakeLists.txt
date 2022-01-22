# Source Code for dZoneManager
file(
    GLOB SOURCES_DZM
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_library(dZoneManager ${SOURCES_DZM})