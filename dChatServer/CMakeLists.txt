set(DCHATSERVER_SOURCES
	"ChatIgnoreList.cpp"
	"ChatPacketHandler.cpp"
	"PlayerContainer.cpp"
)

add_executable(ChatServer "ChatServer.cpp")
target_include_directories(ChatServer PRIVATE "${PROJECT_SOURCE_DIR}/dChatFilter")
add_compile_definitions(ChatServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

add_library(dChatServer ${DCHATSERVER_SOURCES})
target_include_directories(dChatServer PRIVATE "${PROJECT_SOURCE_DIR}/dServer")

target_link_libraries(dChatServer ${COMMON_LIBRARIES} dChatFilter)
target_link_libraries(ChatServer ${COMMON_LIBRARIES} dChatFilter dChatServer dServer)

