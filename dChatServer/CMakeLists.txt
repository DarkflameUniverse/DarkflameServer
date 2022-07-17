set(DCHATSERVER_SOURCES "ChatPacketHandler.cpp"
	"ChatServer.cpp"
	"PlayerContainer.cpp")
	
add_executable(ChatServer ${DCHATSERVER_SOURCES})
target_link_libraries(ChatServer ${COMMON_LIBRARIES} dChatFilter)
