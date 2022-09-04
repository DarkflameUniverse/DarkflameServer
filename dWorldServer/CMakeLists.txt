if (BUILD_VISUAL_DEBUGGER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_VISUAL_DEBUGGER=1")
endif()

set(DWORLDSERVER_SOURCES "ObjectIDManager.cpp"
		"PerformanceManager.cpp"
		"WorldServer.cpp")

add_executable(WorldServer ${DWORLDSERVER_SOURCES})

set(DWORLDSERVER_LIBRARIES dChatFilter dGame dZoneManager Detour Recast dPhysics tinyxml2 dNavigation)

if (BUILD_VISUAL_DEBUGGER)
	set(DWORLDSERVER_LIBRARIES ${DWORLDSERVER_LIBRARIES} dVisualDebugger raylib)
endif()

target_link_libraries(WorldServer ${COMMON_LIBRARIES} ${DWORLDSERVER_LIBRARIES})
