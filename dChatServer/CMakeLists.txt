set(DCHATSERVER_SOURCES
	"ChatPacketHandler.cpp"
	"PlayerContainer.cpp"
)

add_executable(ChatServer "ChatServer.cpp")
add_library(dChatServer ${DCHATSERVER_SOURCES})

target_link_libraries(dChatServer ${COMMON_LIBRARIES} dChatFilter)
target_link_libraries(ChatServer ${COMMON_LIBRARIES} dChatFilter dChatServer)
