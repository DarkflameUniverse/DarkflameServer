# The capture tool (docs/CaptureReplay.md): bundles, live capture import, and replays against a sandbox stack
add_executable(CaptureTool "CaptureTool.cpp" "FakeClient.cpp" "LiveImport.cpp" "Replayer.cpp" "Sandbox.cpp")
target_include_directories(CaptureTool PRIVATE ${PROJECT_SOURCE_DIR}/dServer)
target_compile_definitions(CaptureTool PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
# The game's libraries: game messages are read and compared with the game's own message structs
target_link_libraries(CaptureTool ${COMMON_LIBRARIES} bcrypt dServer
	dScripts dGameBase dComponents dUtilities dGameMessages dInventory dGame dChatFilter dZoneManager dPhysics Detour Recast tinyxml2 dWorldServer dNavigation)
