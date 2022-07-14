set(DMASTERSERVER_SOURCES "InstanceManager.cpp"
		"MasterServer.cpp"
		"ObjectIDManager.cpp")
	
add_executable(MasterServer ${DMASTERSERVER_SOURCES})
target_link_libraries(MasterServer ${COMMON_LIBRARIES})

if(WIN32)
	add_dependencies(MasterServer WorldServer AuthServer ChatServer)
endif()
