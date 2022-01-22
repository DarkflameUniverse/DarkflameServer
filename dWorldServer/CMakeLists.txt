file(
    GLOB SOURCES_WORLD
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_executable(WorldServer ${SOURCES_WORLD})

target_link_libraries(WorldServer dChatFilter)
target_link_libraries(WorldServer dGame)
target_link_libraries(WorldServer dZoneManager)
target_link_libraries(WorldServer dPhysics)
target_link_libraries(WorldServer detour)
target_link_libraries(WorldServer recast)
target_link_libraries(WorldServer tinyxml2)
target_link_libraries(WorldServer ${SHARED_LIBS})