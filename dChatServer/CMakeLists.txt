file(
    GLOB SOURCES_CHAT
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    *.cpp
)

add_executable(ChatServer ${SOURCES_CHAT})
target_link_libraries(ChatServer dChatFilter)
target_link_libraries(ChatServer ${SHARED_LIBS})