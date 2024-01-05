set(DCHATSERVER_SOURCES
	"ChatIgnoreList.cpp"
	"ChatPacketHandler.cpp"
	"PlayerContainer.cpp"
)

add_executable(ChatServer "ChatServer.cpp")
add_library(dChatServer ${DCHATSERVER_SOURCES})
target_include_directories(dChatServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)
add_compile_definitions(ChatServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")

target_link_libraries(dChatServer ${COMMON_LIBRARIES} dChatFilter)
target_link_libraries(ChatServer ${COMMON_LIBRARIES} dChatFilter dChatServer dServer)

