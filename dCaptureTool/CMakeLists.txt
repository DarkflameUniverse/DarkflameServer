# The capture tool (docs/CaptureReplay.md): bundles, live capture import, and replays against a sandbox stack
add_executable(CaptureTool "CaptureTool.cpp" "FakeClient.cpp" "LiveImport.cpp" "Replayer.cpp" "Sandbox.cpp")
target_include_directories(CaptureTool PRIVATE ${PROJECT_SOURCE_DIR}/dServer)
target_compile_definitions(CaptureTool PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
target_link_libraries(CaptureTool ${COMMON_LIBRARIES} bcrypt dServer)
