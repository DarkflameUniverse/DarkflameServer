file(
    GLOB SOURCES_MASTER
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_executable(MasterServer ${SOURCES_MASTER})
target_link_libraries(MasterServer ${SHARED_LIBS})

if(WIN32)
    add_dependencies(MasterServer WorldServer)
    add_dependencies(MasterServer AuthServer)
    add_dependencies(MasterServer ChatServer)
endif()