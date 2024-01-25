add_executable(AuthServer "AuthServer.cpp")

target_link_libraries(AuthServer ${COMMON_LIBRARIES} dServer)

target_include_directories(AuthServer PRIVATE ${PROJECT_SOURCE_DIR}/dServer)

add_compile_definitions(AuthServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
