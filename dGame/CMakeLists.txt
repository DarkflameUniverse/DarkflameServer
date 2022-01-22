file(
    GLOB SOURCES_DGAME
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
    dBehaviors/*.cpp
    dComponents/*.cpp
    dGameMessages/*.cpp
    dInventory/*.cpp
    dMission/*.cpp
    dEntity/*.cpp
    dUtilities/*.cpp
    ../dScripts/*.cpp
)

add_library(dGame ${SOURCES_DGAME})